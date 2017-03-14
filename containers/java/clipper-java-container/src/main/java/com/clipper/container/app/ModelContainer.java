import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;

import org.zeromq.ZMQ;

import data.DataType;
import data.DataUtils;
import data.DataVector;
import data.FloatVector;
import data.DataVectorParser;

class ModelContainer<I extends DataVector<?>> {
  private static String CONNECTION_ADDRESS = "tcp://%s:%s";

  private DataVectorParser<?, I> inputVectorParser;
  private Thread servingThread;

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
        serveModel(socket, model);
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

  private void serveModel(ZMQ.Socket socket, Model<I> model) {
    while (true) {
      socket.recv();
      PerformanceTimer.startTiming();
      byte[] idMessage = socket.recv();
      List<Long> parsedIdMessage = DataUtils.getUnsignedIntsFromBytes(idMessage);
      if (parsedIdMessage.size() < 1) {
        // TODO: Throw an exception here
        return;
      }
      long msgId = parsedIdMessage.get(0);
      System.out.println("Message ID: " + msgId);

      byte[] requestHeaderMessage = socket.recv();
      List<Integer> requestHeader = DataUtils.getSignedIntsFromBytes(requestHeaderMessage);
      if (requestHeader.size() < 1) {
        // TODO: Throw an exception here
        return;
      }
      RequestType requestType = RequestType.fromCode(requestHeader.get(0));

      if (requestType == RequestType.Predict) {
        // Handle Predict request
        byte[] inputHeaderMessage = socket.recv();
        byte[] rawContent = socket.recv();
        System.out.println("rawContent length: " + rawContent.length);

        PerformanceTimer.logElapsed("Recv");

        List<Integer> inputHeader = DataUtils.getSignedIntsFromBytes(inputHeaderMessage);
        if (inputHeader.size() < 2) {
          // TODO: Throw here
          return;
        }

        DataType inputType = DataType.fromCode(inputHeader.get(0));
        int numInputs = inputHeader.get(1);
        List<Integer> inputSplits = inputHeader.subList(2, inputHeader.size());
        validateRequestInputType(model, inputType);
        // PROCESS SPLITS
        List<I> dataVectors = inputVectorParser.parse(ByteBuffer.wrap(rawContent), inputSplits);

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

  private void validateRequestInputType(Model<I> model, DataType inputType) {
    if (model.inputType != inputType) {
      // TODO: THROW HERE
    }
  }

  private void handlePredictRequest(
      long msgId, List<I> dataVectors, Model<I> model, ZMQ.Socket socket) throws IOException {
    List<FloatVector> predictions = model.predict(dataVectors);
    int outputBufferLen = 0;
    for (FloatVector p : predictions) {
      outputBufferLen += p.getData().length;
    }
    ByteBuffer outputBuffer = ByteBuffer.allocate(outputBufferLen * 4);
    outputBuffer.order(ByteOrder.LITTLE_ENDIAN);
    for (FloatVector preds : predictions) {
      for (float p : preds.getData()) {
        outputBuffer.putFloat(p);
      }
    }
    byte[] responseData = outputBuffer.array();
    socket.send("", ZMQ.SNDMORE);
    ByteBuffer b = ByteBuffer.allocate(8);
    b.order(ByteOrder.LITTLE_ENDIAN);
    b.putLong(msgId);
    b.position(4);
    byte[] msgIdByteArr = b.slice().array();
    socket.send(msgIdByteArr, ZMQ.SNDMORE);
    socket.send(responseData, 0, responseData.length);
  }
}
