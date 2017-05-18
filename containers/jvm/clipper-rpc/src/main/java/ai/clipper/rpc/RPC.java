package ai.clipper.rpc;

import java.io.IOException;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
import java.util.*;

import ai.clipper.container.data.*;
import ai.clipper.container.ClipperModel;
import ai.clipper.rpc.logging.*;
import org.zeromq.ZMQ;

public class RPC<I extends DataVector<?>> {
  private static String CONNECTION_ADDRESS = "tcp://%s:%s";
  private static final long SOCKET_POLLING_TIMEOUT_MILLIS = 5000;
  private static final long SOCKET_ACTIVITY_TIMEOUT_MILLIS = 30000;
  private static final int EVENT_HISTORY_BUFFER_SIZE = 30;
  private static final int BYTES_PER_INT = 4;

  private final DataVectorParser<?, I> inputVectorParser;
  private final RPCEventHistory eventHistory;

  private Thread servingThread;
  private ByteBuffer responseBuffer;
  private int responseBufferSize;

  public RPC(DataVectorParser<?, I> inputVectorParser) {
    this.inputVectorParser = inputVectorParser;
    this.eventHistory = new RPCEventHistory(EVENT_HISTORY_BUFFER_SIZE);
  }

  /**
   * This method blocks until the RPC connection is ended
   */
  public void start(final ClipperModel<I> model, final String modelName, int modelVersion,
      final String host, final int port) throws UnknownHostException {
    InetAddress address = InetAddress.getByName(host);
    String ip = address.getHostAddress();
    final ZMQ.Context context = ZMQ.context(1);

    servingThread = new Thread(() -> {
      // Initialize ZMQ
      try {
        serveModel(model, modelName, modelVersion, context, ip, port);
      } catch (NoSuchFieldException | IllegalArgumentException e) {
        e.printStackTrace();
      }
    });
    servingThread.start();
    try {
      servingThread.join();
    } catch (InterruptedException e) {
      System.out.println("RPC thread interrupted: " + e.getMessage());
    }
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

  private void serveModel(ClipperModel<I> model, String modelName, int modelVersion,
      final ZMQ.Context context, String ip, int port)
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
              sendContainerMetadata(socket, model, modelName, modelVersion);
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

  private void validateRequestInputType(ClipperModel<I> model, DataType inputType)
      throws IllegalArgumentException {
    if (model.getInputType() != inputType) {
      throw new IllegalArgumentException(
          String.format("RPC message has input of incorrect type \"%s\". Expected type: \"%s\"",
              inputType.toString(), model.getInputType().toString()));
    }
  }

  private void handlePredictRequest(long msgId, Iterator<I> dataVectors, ClipperModel<I> model,
      ZMQ.Socket socket) throws IOException {
    ArrayList<I> inputs = new ArrayList<>();
    dataVectors.forEachRemaining(inputs::add);
    List<SerializableString> predictions = model.predict(inputs);

    // TODO: check length of input and output lists match

    // At minimum, the output contains an unsigned
    // integer specifying the number of string
    // outputs
    int outputLenBytes = BYTES_PER_INT;
    int maxBufferSizeBytes = BYTES_PER_INT;
    for (SerializableString p : predictions) {
      // Add byte length corresponding to an
      // integer containing the string's size
      outputLenBytes += BYTES_PER_INT;
      maxBufferSizeBytes += BYTES_PER_INT;
      // Add the maximum size of the string
      // with utf-8 encoding to the maximum buffer size.
      // The actual output length will be determined
      // when the string predictions are serialized
      maxBufferSizeBytes += p.maxSizeBytes();
    }

    if (responseBuffer == null || responseBufferSize < maxBufferSizeBytes) {
      responseBufferSize = maxBufferSizeBytes * 2;
      responseBuffer = ByteBuffer.allocateDirect(responseBufferSize);
      responseBuffer.order(ByteOrder.LITTLE_ENDIAN);
    }
    responseBuffer.rewind();

    int numOutputs = predictions.size();
    // Write the number of outputs to the buffer
    responseBuffer.putInt(numOutputs);
    int baseStringLengthsPosition = responseBuffer.position();
    // We will begin writing data after the segment allocated
    // for storing string lengths. Advance past this segment
    // for now
    responseBuffer.position(baseStringLengthsPosition + (BYTES_PER_INT * numOutputs));
    for (int i = 0; i < predictions.size(); ++i) {
      SerializableString prediction = predictions.get(i);
      // Serialize the prediction and write it to the output buffer
      int serializedSize = prediction.encodeUTF8ToBuffer(responseBuffer);
      outputLenBytes += serializedSize;
      int currPosition = responseBuffer.position();
      // Navigate to the buffer segment allocated for storing
      // the length of the serialized string and write the length
      // in this location
      responseBuffer.position(baseStringLengthsPosition + (BYTES_PER_INT * i));
      responseBuffer.putInt(serializedSize);
      // Return to the position in the buffer where the next string
      // should be written
      responseBuffer.position(currPosition);
    }
    responseBuffer.limit(outputLenBytes);

    socket.send("", ZMQ.SNDMORE);
    socket.send(
        DataUtils.getBytesFromInts(ContainerMessageType.ContainerContent.getCode()), ZMQ.SNDMORE);
    ByteBuffer b = ByteBuffer.allocate(2 * BYTES_PER_INT);
    b.order(ByteOrder.LITTLE_ENDIAN);
    b.putLong(msgId);
    b.position(BYTES_PER_INT);
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

  private void sendContainerMetadata(
      ZMQ.Socket socket, ClipperModel<I> model, String modelName, int modelVersion) {
    socket.send("", ZMQ.SNDMORE);
    socket.send(
        DataUtils.getBytesFromInts(ContainerMessageType.NewContainer.getCode()), ZMQ.SNDMORE);
    socket.send(modelName, ZMQ.SNDMORE);
    socket.send(String.valueOf(modelVersion), ZMQ.SNDMORE);
    socket.send(String.valueOf(model.getInputType().getCode()));
    eventHistory.insert(RPCEventType.SentContainerMetadata);
    System.out.println("Sent container metadata!");
  }
}
