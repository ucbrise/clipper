package clipper.container.app;

import java.io.IOException;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.IntBuffer;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import clipper.container.app.data.*;
import org.zeromq.ZMQ;

class ModelContainer<I extends DataVector<?>> {
  private static String CONNECTION_ADDRESS = "tcp://%s:%s";

  private DataVectorParser<?, I> inputVectorParser;
  private Thread servingThread;
  private ByteBuffer responseBuffer;
  private int responseBufferSize;

  ModelContainer(DataVectorParser<?, I> inputVectorParser) {
    this.inputVectorParser = inputVectorParser;
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
        ZMQ.Socket socket = context.socket(ZMQ.DEALER);
        createConnection(socket, model, ip, port);
        try {
          serveModel(socket, model);
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

  private void createConnection(ZMQ.Socket socket, Model<I> model, String ip, int port) {
    String address = String.format(CONNECTION_ADDRESS, ip, port);
    socket.connect(address);
    socket.send("", ZMQ.SNDMORE);
    socket.send(model.getName(), ZMQ.SNDMORE);
    socket.send(String.valueOf(model.getVersion()), ZMQ.SNDMORE);
    socket.send(String.valueOf(model.getInputType().getCode()));
  }

  private void serveModel(ZMQ.Socket socket, Model<I> model)
      throws NoSuchFieldException, IllegalArgumentException {
    int inputHeaderBufferSize = 0;
    int inputBufferSize = 0;
    ByteBuffer inputHeaderBuffer = null;
    ByteBuffer inputBuffer = null;
    while (true) {
      socket.recv();
      PerformanceTimer.startTiming();
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
        int inputHeaderBytesRead = socket.recvZeroCopy(inputHeaderBuffer, inputHeaderSize, -1);
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
    List<SerializableString> predictions = new ArrayList<>();
    while (dataVectors.hasNext()) {
      I dataVector = dataVectors.next();
      SerializableString prediction = model.predict(dataVector);
      predictions.add(prediction);
    }
    // At minimum, the output contains an unsigned
    // integer specifying the number of string
    // outputs
    int outputLenBytes = 4;
    int maxBufferSizeBytes = 4;
    for(SerializableString p : predictions) {
      // Add byte length corresponding to an
      // integer containing the string's size
      outputLenBytes += 4;
      maxBufferSizeBytes += 4;
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

    responseBuffer.put(DataUtils.getBytesFromInts(predictions.size()));
    for (SerializableString p : predictions) {
      int bytesWritten = p.toBytes(responseBuffer);
      outputLenBytes += bytesWritten;
    }
    responseBuffer.limit(outputLenBytes);

    socket.send("", ZMQ.SNDMORE);
    ByteBuffer b = ByteBuffer.allocate(8);
    b.order(ByteOrder.LITTLE_ENDIAN);
    b.putLong(msgId);
    b.position(4);
    byte[] msgIdByteArr = b.slice().array();
    socket.send(msgIdByteArr, ZMQ.SNDMORE);
    socket.sendZeroCopy(responseBuffer, responseBuffer.position(), 0);
  }
}
