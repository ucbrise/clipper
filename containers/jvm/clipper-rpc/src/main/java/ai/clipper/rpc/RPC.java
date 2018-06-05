package ai.clipper.rpc;

import java.io.IOException;
import java.io.File;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.LongBuffer;
import java.util.*;

import ai.clipper.container.data.*;
import ai.clipper.container.ClipperModel;
import ai.clipper.rpc.logging.*;
import org.zeromq.ZMQ;

public class RPC<I extends DataVector<?>> {
  private static String CONNECTION_ADDRESS = "tcp://%s:%s";
  private static final long RPC_VERSION = 3;
  private static final long SOCKET_POLLING_TIMEOUT_MILLIS = 5000;
  private static final long SOCKET_ACTIVITY_TIMEOUT_MILLIS = 30000;
  private static final int EVENT_HISTORY_BUFFER_SIZE = 30;

  private final DataVectorParser<?, I> inputVectorParser;
  private final RPCEventHistory eventHistory;

  private Thread servingThread;
  private ByteBuffer outputHeaderBuffer;
  private ByteBuffer outputContentBuffer;
  private int outputHeaderBufferSize;
  private int outputContentBufferSize;

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

    try {
      File file = new File("/model_is_ready.check");
      file.createNewFile();
    } catch (IOException e) {
      e.printStackTrace();
    }

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

  private void validateRpcVersion(long receivedVersion) throws Exception {
    if (receivedVersion != RPC_VERSION) {
      System.out.println(String.format(
          "ERROR: Received a message with RPC version: %d that does not match container version: %d",
          receivedVersion, RPC_VERSION));
    }
  }

  private void serveModel(ClipperModel<I> model, String modelName, int modelVersion,
      final ZMQ.Context context, String ip, int port)
      throws NoSuchFieldException, IllegalArgumentException {
    int inputHeaderBufferSize = 0;
    int inputContentBufferSize = 0;
    ByteBuffer inputHeaderBuffer = null;
    ByteBuffer inputContentBuffer = null;
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
        byte[] rpcVersionMessage = socket.recv();
        byte[] typeMessage = socket.recv();
        List<Long> parsedVersionMessage = DataUtils.getUnsignedIntsFromBytes(rpcVersionMessage);
        long rpcVersion = parsedVersionMessage.get(0);
        try {
          validateRpcVersion(rpcVersion);
        } catch (Exception e) {
          e.printStackTrace();
          System.exit(1);
        }
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
                inputHeaderBuffer.order(ByteOrder.LITTLE_ENDIAN);
              }
              inputHeaderBuffer.rewind();
              inputHeaderBuffer.limit(inputHeaderBufferSize);
              int inputHeaderBytesRead =
                  socket.recvZeroCopy(inputHeaderBuffer, inputHeaderSize, -1);
              inputHeaderBuffer.rewind();
              inputHeaderBuffer.limit(inputHeaderBytesRead);

              LongBuffer inputHeader =
                  inputHeaderBuffer.slice().order(ByteOrder.LITTLE_ENDIAN).asLongBuffer();

              DataType inputType = DataType.fromCode((int) inputHeader.get(0));
              long numInputs = inputHeader.get(1);

              int inputContentSizeBytes = 0;
              for (int i = 0; i < numInputs; ++i) {
                inputContentSizeBytes += inputHeader.get(i + 2);
              }

              if (inputContentBufferSize < inputContentSizeBytes) {
                inputContentBufferSize = inputContentSizeBytes * 2;
                inputContentBuffer = ByteBuffer.allocateDirect(inputContentBufferSize);
                inputContentBuffer.order(ByteOrder.LITTLE_ENDIAN);
              }
              inputContentBuffer.rewind();

              ArrayList<I> inputs = new ArrayList<>();
              int bufferPosition = 0;
              for (int i = 0; i < numInputs; ++i) {
                inputContentBuffer.position(bufferPosition);
                int inputSizeBytes = (int) inputHeader.get(i + 2);
                ByteBuffer inputBuffer = sliceOrdered(inputContentBuffer);
                socket.recvZeroCopy(inputBuffer, inputSizeBytes, -1);
                inputBuffer.position(0);
                inputBuffer.limit(inputSizeBytes);
                I input = inputVectorParser.constructDataVector(inputBuffer, inputSizeBytes);
                inputs.add(input);
                bufferPosition += inputSizeBytes;
              }

              PerformanceTimer.logElapsed("Recv and Parse");

              try {
                handlePredictRequest(msgId, inputs, model, socket);
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

  private ByteBuffer sliceOrdered(ByteBuffer buffer) {
    return buffer.slice().order(ByteOrder.LITTLE_ENDIAN);
  }

  private void validateRequestInputType(ClipperModel<I> model, DataType inputType)
      throws IllegalArgumentException {
    if (model.getInputType() != inputType) {
      throw new IllegalArgumentException(
          String.format("RPC message has input of incorrect type \"%s\". Expected type: \"%s\"",
              inputType.toString(), model.getInputType().toString()));
    }
  }

  private void handlePredictRequest(long msgId, ArrayList<I> inputs, ClipperModel<I> model,
      ZMQ.Socket socket) throws IOException {
    List<SerializableString> predictions = model.predict(inputs);

    if (predictions.size() != inputs.size()) {
      String message =
          String.format("Attempting to send %d outputs for a request containg %d inputs!",
              predictions.size(), inputs.size());
      throw new IllegalStateException(message);
    }

    long numOutputs = predictions.size();
    int outputHeaderSizeBytes = (int) (numOutputs + 1) * Long.BYTES;
    int outputContentMaxSizeBytes = 0;
    for (SerializableString p : predictions) {
      // Add the maximum size of the string
      // with utf-8 encoding to the maximum buffer size.
      // The actual output length will be determined
      // when the string predictions are serialized
      outputContentMaxSizeBytes += p.maxSizeBytes();
    }

    if (outputHeaderBuffer == null || outputHeaderBufferSize < outputHeaderSizeBytes) {
      outputHeaderBufferSize = outputHeaderSizeBytes * 2;
      outputHeaderBuffer = ByteBuffer.allocateDirect(outputHeaderBufferSize);
      outputHeaderBuffer.order(ByteOrder.LITTLE_ENDIAN);
    }
    outputHeaderBuffer.rewind();
    outputHeaderBuffer.limit(outputHeaderSizeBytes);

    if (outputContentBuffer == null || outputContentBufferSize < outputContentMaxSizeBytes) {
      outputContentBufferSize = outputContentMaxSizeBytes * 2;
      outputContentBuffer = ByteBuffer.allocateDirect(outputContentBufferSize);
      outputContentBuffer.order(ByteOrder.LITTLE_ENDIAN);
    }
    outputContentBuffer.rewind();

    LongBuffer longHeader = outputHeaderBuffer.asLongBuffer();
    longHeader.put(numOutputs);

    // Create a list of buffers for individual outputs.
    // These buffers will all be backed by `outputContentBuffer`
    ByteBuffer[] outputBuffers = new ByteBuffer[(int) numOutputs];
    int[] outputLengths = new int[(int) numOutputs];

    int outputContentBufferPosition = 0;
    for (int i = 0; i < predictions.size(); i++) {
      outputContentBuffer.position(outputContentBufferPosition);
      SerializableString prediction = predictions.get(i);
      long outputLength = prediction.encodeUTF8ToBuffer(outputContentBuffer);
      outputContentBuffer.position(outputContentBufferPosition);
      ByteBuffer outputBuffer = outputContentBuffer.slice();
      outputBuffer.order(ByteOrder.LITTLE_ENDIAN);
      outputBuffer.limit((int) outputLength);
      outputBuffers[i] = outputBuffer;
      outputLengths[i] = (int) outputLength;
      longHeader.put(outputLength);
      outputContentBufferPosition += outputLength;
    }
    longHeader.rewind();

    socket.send("", ZMQ.SNDMORE);
    socket.send(
        DataUtils.getBytesFromInts(ContainerMessageType.ContainerContent.getCode()), ZMQ.SNDMORE);
    ByteBuffer msgIdBuf = ByteBuffer.allocate(Integer.BYTES);
    msgIdBuf.order(ByteOrder.LITTLE_ENDIAN);
    msgIdBuf.putInt((int) msgId);
    byte[] msgIdByteArr = msgIdBuf.array();
    ByteBuffer headerSizeBuf = ByteBuffer.allocate(Long.BYTES);
    headerSizeBuf.order(ByteOrder.LITTLE_ENDIAN);
    headerSizeBuf.putLong(outputHeaderSizeBytes);
    byte[] headerSizeByteArr = headerSizeBuf.array();
    socket.send(msgIdByteArr, ZMQ.SNDMORE);
    socket.send(headerSizeByteArr, ZMQ.SNDMORE);
    socket.sendZeroCopy(outputHeaderBuffer, outputHeaderSizeBytes, ZMQ.SNDMORE);
    int lastOutputMsgNum = predictions.size() - 1;
    for (int i = 0; i < predictions.size(); i++) {
      if (i < lastOutputMsgNum) {
        socket.sendZeroCopy(outputBuffers[i], outputLengths[i], ZMQ.SNDMORE);
      } else {
        socket.sendZeroCopy(outputBuffers[i], outputLengths[i], 0);
      }
    }
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
    socket.send(String.valueOf(model.getInputType().getCode()), ZMQ.SNDMORE);
    socket.send(DataUtils.getBytesFromLongs(RPC_VERSION), 0);
    eventHistory.insert(RPCEventType.SentContainerMetadata);
    System.out.println("Sent container metadata!");
  }
}
