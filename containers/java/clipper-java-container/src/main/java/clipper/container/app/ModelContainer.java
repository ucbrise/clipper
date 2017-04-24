package clipper.container.app;

import java.io.IOException;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
import java.util.*;

import clipper.container.app.data.*;
import clipper.container.app.logging.RPCEvent;
import clipper.container.app.logging.RPCEventHistory;
import clipper.container.app.logging.RPCEventType;
import org.zeromq.ZMQ;

class ModelContainer<I extends DataVector<?>> {
  private static final String CONNECTION_ADDRESS = "tcp://%s:%s";
  private static final long SOCKET_POLLING_TIMEOUT_MILLIS = 5000;
  private static final long SOCKET_ACTIVITY_TIMEOUT_MILLIS = 30000;
  private static final int EVENT_HISTORY_BUFFER_SIZE = 30;

  private final DataVectorParser<?, I> inputVectorParser;
  private final RPCEventHistory eventHistory;

  private Thread servingThread;
  private ByteBuffer responseBuffer;
  private int responseBufferSize;

  ModelContainer(DataVectorParser<?, I> inputVectorParser) {
    this.inputVectorParser = inputVectorParser;
    this.eventHistory = new RPCEventHistory(EVENT_HISTORY_BUFFER_SIZE);
  }

  public void start(final Model<I> model, final String host, final int port)
      throws UnknownHostException {
    InetAddress address = InetAddress.getByName(host);
    String ip = address.getHostAddress();
    final ZMQ.Context context = ZMQ.context(1);

    servingThread = new Thread(new Runnable() {
      @Override
      public void run() {
        // Initialize ZMQ
        try {
          serveModel(model, context, ip, port);
        } catch (NoSuchFieldException | IllegalArgumentException e) {
          e.printStackTrace();
        }
      }
    });
    servingThread.start();
  }

  public void stop() {
    if (servingThread != null) {
      servingThread.interrupt();
      servingThread = null;
    }
  }

  public RPCEvent[] getEventHistory() {
    return eventHistory.getEvents();
  }

  private void serveModel(Model<I> model, final ZMQ.Context context, String ip, int port)
      throws NoSuchFieldException, IllegalArgumentException {
    int inputHeaderBufferSize = 0;
    int inputBufferSize = 0;
    ByteBuffer inputHeaderBuffer = null;
    ByteBuffer inputBuffer = null;
    boolean connected = false;
    long lastActivityTimeMillis = 0;
    String clipperAddress = String.format(CONNECTION_ADDRESS, ip, port);
    ZMQ.Poller poller = new ZMQ.Poller(1);
    while (true) {
      ZMQ.Socket socket = context.socket(ZMQ.DEALER);
      poller.register(socket, ZMQ.Poller.POLLIN);
      socket.connect(clipperAddress);
      sendHeartbeat(socket);
      while (true) {
        poller.poll(SOCKET_POLLING_TIMEOUT_MILLIS);
        if (!poller.pollin(0)) {
          // Failed to receive a message prior to the polling timeout
          if (connected) {
            if (System.currentTimeMillis() - lastActivityTimeMillis
                >= SOCKET_ACTIVITY_TIMEOUT_MILLIS) {
              // Terminate the session
              System.out.println("Connection timed out. Reconnecting...");
              connected = false;
              poller.unregister(socket);
              socket.close();
              break;
            } else {
              sendHeartbeat(socket);
            }
          }
          continue;
        }

        // Received a message prior to the polling timeout
        if (!connected) {
          connected = true;
        }
        lastActivityTimeMillis = System.currentTimeMillis();
        PerformanceTimer.startTiming();
        // Receive delimiter between routing identity and content
        socket.recv();
        byte[] typeMessage = socket.recv();
        List<Long> parsedTypeMessage = DataUtils.getUnsignedIntsFromBytes(typeMessage);
        ContainerMessageType messageType =
            ContainerMessageType.fromCode(parsedTypeMessage.get(0).intValue());
        switch (messageType) {
          case Heartbeat:
            System.out.println("Received heartbeat!");
            eventHistory.insert(RPCEventType.ReceivedHeartbeat);
            byte[] heartbeatTypeMessage = socket.recv();
            List<Long> parsedHeartbeatTypeMessage =
                DataUtils.getUnsignedIntsFromBytes(heartbeatTypeMessage);
            HeartbeatType heartbeatType =
                HeartbeatType.fromCode(parsedHeartbeatTypeMessage.get(0).intValue());
            if (heartbeatType == HeartbeatType.RequestContainerMetadata) {
              sendContainerMetadata(socket, model);
            }
            break;
          case ContainerContent:
            eventHistory.insert(RPCEventType.ReceivedContainerContent);
            byte[] idMessage = socket.recv();
            List<Long> parsedIdMessage = DataUtils.getUnsignedIntsFromBytes(idMessage);
            if (parsedIdMessage.size() < 1) {
              throw new NoSuchFieldException("Field \"message_id\" is missing from RPC message");
            }
            long msgId = parsedIdMessage.get(0);
            System.out.println("Message ID: " + msgId);

            byte[] requestHeaderMessage = socket.recv();
            List<Integer> requestHeader = DataUtils.getSignedIntsFromBytes(requestHeaderMessage);
            if (requestHeader.size() < 1) {
              throw new NoSuchFieldException("Request header is missing from RPC message");
            }
            RequestType requestType = RequestType.fromCode(requestHeader.get(0));

            if (requestType == RequestType.Predict) {
              byte[] inputHeaderSizeMessage = socket.recv();
              List<Long> parsedInputHeaderSizeMessage =
                  DataUtils.getLongsFromBytes(inputHeaderSizeMessage);
              if (parsedInputHeaderSizeMessage.size() < 1) {
                throw new NoSuchFieldException(
                    "Input header size is missing from RPC predict request message");
              }
              int inputHeaderSize = (int) ((long) parsedInputHeaderSizeMessage.get(0));
              if (inputHeaderBuffer == null || inputHeaderBufferSize < inputHeaderSize) {
                inputHeaderBufferSize = inputHeaderSize * 2;
                inputHeaderBuffer = ByteBuffer.allocateDirect(inputHeaderBufferSize);
              }
              inputHeaderBuffer.rewind();
              inputHeaderBuffer.limit(inputHeaderBufferSize);
              int inputHeaderBytesRead =
                  socket.recvZeroCopy(inputHeaderBuffer, inputHeaderSize, -1);
              inputHeaderBuffer.rewind();
              inputHeaderBuffer.limit(inputHeaderBytesRead);

              IntBuffer inputHeader =
                  inputHeaderBuffer.slice().order(ByteOrder.LITTLE_ENDIAN).asIntBuffer();

              byte[] inputContentSizeMessage = socket.recv();
              List<Long> parsedInputContentSizeMessage =
                  DataUtils.getLongsFromBytes(inputContentSizeMessage);
              if (parsedInputContentSizeMessage.size() < 1) {
                throw new NoSuchFieldException(
                    "Input content size is missing from RPC predict request message");
              }

              int inputContentSize = (int) ((long) parsedInputContentSizeMessage.get(0));
              if (inputBufferSize < inputContentSize) {
                inputBufferSize = inputContentSize * 2;
                inputBuffer = ByteBuffer.allocateDirect(inputBufferSize);
              }
              inputBuffer.rewind();
              inputBuffer.limit(inputBufferSize);
              int inputBytesRead = socket.recvZeroCopy(inputBuffer, inputContentSize, -1);
              inputBuffer.rewind();
              inputBuffer.limit(inputBytesRead);

              PerformanceTimer.logElapsed("Recv");

              if (inputHeader.remaining() < 2) {
                throw new NoSuchFieldException(
                    "RPC message input header is missing or is of insufficient size");
              }

              DataType inputType = DataType.fromCode(inputHeader.get());
              int numInputs = inputHeader.get();
              validateRequestInputType(model, inputType);
              Iterator<I> dataVectors =
                  inputVectorParser.parseDataVectors(inputBuffer.slice(), inputHeader.slice());

              PerformanceTimer.logElapsed("Parse");

              try {
                handlePredictRequest(msgId, dataVectors, model, socket);
              } catch (IOException e) {
                e.printStackTrace();
              }

              PerformanceTimer.logElapsed("Handle");
              System.out.println(PerformanceTimer.getLog());
            } else {
              // Handle Feedback request
            }
            break;
          case NewContainer:
            // We should never receive a new container message from Clipper
            eventHistory.insert(RPCEventType.ReceivedContainerMetadata);
            System.out.println("Received erroneous new container message from Clipper!");
            break;
          default:
            break;
        }
      }
    }
  }

  private void validateRequestInputType(Model<I> model, DataType inputType)
      throws IllegalArgumentException {
    if (model.inputType != inputType) {
      throw new IllegalArgumentException(
          String.format("RPC message has input of incorrect type \"%s\". Expected type: \"%s\"",
              inputType.toString(), model.inputType.toString()));
    }
  }

  private void handlePredictRequest(
      long msgId, Iterator<I> dataVectors, Model<I> model, ZMQ.Socket socket) throws IOException {
    List<FloatVector> predictions = new ArrayList<>();
    while (dataVectors.hasNext()) {
      I dataVector = dataVectors.next();
      FloatVector prediction = model.predict(dataVector);
      predictions.add(prediction);
    }
    int outputLenBytes = 0;
    for (FloatVector p : predictions) {
      outputLenBytes += p.getData().remaining();
    }
    outputLenBytes = outputLenBytes * 4;
    if (responseBuffer == null || responseBufferSize < outputLenBytes) {
      responseBufferSize = outputLenBytes * 2;
      responseBuffer = ByteBuffer.allocateDirect(responseBufferSize);
      responseBuffer.order(ByteOrder.LITTLE_ENDIAN);
    }
    responseBuffer.rewind();
    responseBuffer.limit(outputLenBytes);
    for (FloatVector preds : predictions) {
      while (preds.getData().hasRemaining()) {
        responseBuffer.putFloat(preds.getData().get());
      }
    }
    socket.send("", ZMQ.SNDMORE);
    socket.send(DataUtils.getBytesFromInts(ContainerMessageType.ContainerContent.getCode()), ZMQ.SNDMORE);
    ByteBuffer b = ByteBuffer.allocate(8);
    b.order(ByteOrder.LITTLE_ENDIAN);
    b.putLong(msgId);
    b.position(4);
    byte[] msgIdByteArr = b.slice().array();
    socket.send(msgIdByteArr, ZMQ.SNDMORE);
    socket.sendZeroCopy(responseBuffer, responseBuffer.position(), 0);
    eventHistory.insert(RPCEventType.SentContainerContent);
  }

  private void sendHeartbeat(ZMQ.Socket socket) {
    socket.send("", ZMQ.SNDMORE);
    socket.send(DataUtils.getBytesFromInts(ContainerMessageType.Heartbeat.getCode()), 0);
    eventHistory.insert(RPCEventType.SentHeartbeat);
    System.out.println("Sent heartbeat!");
  }

  private void sendContainerMetadata(ZMQ.Socket socket, Model<I> model) {
    socket.send("", ZMQ.SNDMORE);
    socket.send(
        DataUtils.getBytesFromInts(ContainerMessageType.NewContainer.getCode()), ZMQ.SNDMORE);
    socket.send(model.getName(), ZMQ.SNDMORE);
    socket.send(String.valueOf(model.getVersion()), ZMQ.SNDMORE);
    socket.send(String.valueOf(model.getInputType().getCode()));
    eventHistory.insert(RPCEventType.SentContainerMetadata);
    System.out.println("Sent container metadata!");
  }
}
