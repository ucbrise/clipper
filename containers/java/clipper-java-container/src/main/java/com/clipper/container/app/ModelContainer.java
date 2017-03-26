package com.clipper.container.app;

import java.io.IOException;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import com.clipper.container.app.data.*;
import org.zeromq.ZMQ;

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

  private void serveModel(ZMQ.Socket socket, Model<I> model) throws NoSuchFieldException, IllegalArgumentException {
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
        // Handle Predict request
        byte[] inputHeaderMessage = socket.recv();
        byte[] rawContent = socket.recv();

        PerformanceTimer.logElapsed("Recv");

        List<Integer> inputHeader = DataUtils.getSignedIntsFromBytes(inputHeaderMessage);
        if (inputHeader.size() < 2) {
          throw new NoSuchFieldException("RPC message input header is missing or is of insufficient size");
        }

        DataType inputType = DataType.fromCode(inputHeader.get(0));
        int numInputs = inputHeader.get(1);
        List<Integer> inputSplits = inputHeader.subList(2, inputHeader.size());
        validateRequestInputType(model, inputType);
        // PROCESS SPLITS
        Iterator<I> dataVectors = inputVectorParser.parseDataVectors(ByteBuffer.wrap(rawContent), inputSplits);

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

  private void validateRequestInputType(Model<I> model, DataType inputType) throws IllegalArgumentException {
    if (model.inputType != inputType) {
      throw new IllegalArgumentException(
              String.format(
                      "RPC message has input of incorrect type \"{}\". Expected type: \"{}\"",
                      inputType.toString(),
                      model.inputType.toString()));
    }
  }

  private void handlePredictRequest(
      long msgId, Iterator<I> dataVectors, Model<I> model, ZMQ.Socket socket) throws IOException {
    List<FloatVector> predictions = new ArrayList<FloatVector>();
    while(dataVectors.hasNext()) {
      I dataVector = dataVectors.next();
      FloatVector prediction = model.predict(dataVector);
      predictions.add(prediction);
    }
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
