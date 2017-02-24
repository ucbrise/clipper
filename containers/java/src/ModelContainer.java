import data.DataType;
import data.DataUtils;
import data.DataVector;
import data.DataVectorParser;
import org.zeromq.ZMQ;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.List;

class ModelContainer<I extends DataVector<?>, O extends DataVector> {

    private static String CONNECTION_ADDRESS = "tcp://%s:%s";

    private DataVectorParser<?, I> inputVectorParser;
    private Thread servingThread;

    ModelContainer(DataVectorParser<?, I> inputVectorParser) {
        this.inputVectorParser = inputVectorParser;
    }

    public void start(final Model<I,O> model, final String host, final int port) throws UnknownHostException {
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
        if(servingThread != null) {
            servingThread.interrupt();
            servingThread = null;
        }
    }

    private void createConnection(ZMQ.Socket socket, Model<I,O> model, String ip, int port) {
        String address = String.format(CONNECTION_ADDRESS, ip, port);
        socket.connect(address);
        socket.send("", ZMQ.SNDMORE);
        socket.send(model.getName(), ZMQ.SNDMORE);
        socket.send(String.valueOf(model.getVersion()), ZMQ.SNDMORE);
        socket.send(String.valueOf(model.getInputType().getCode()));
    }

    private void serveModel(ZMQ.Socket socket, Model<I,O> model) {
        while(true) {
            socket.recv();
            PerformanceTimer.startTiming();
            byte[] idMessage = socket.recv();
            List<Long> parsedIdMessage = DataUtils.getUnsignedIntsFromBytes(idMessage);
            if(parsedIdMessage.size() < 1) {
                // TODO: Throw an exception here
                return;
            }
            long msgId = parsedIdMessage.get(0);

            byte[] requestHeaderMessage = socket.recv();
            List<Integer> requestHeader = DataUtils.getSignedIntsFromBytes(requestHeaderMessage);
            if(requestHeader.size() < 1) {
                // TODO: Throw an exception here
                return;
            }
            RequestType requestType = RequestType.fromCode(requestHeader.get(0));

            if(requestType == RequestType.Predict) {
                // Handle Predict request
                byte[] inputHeaderMessage = socket.recv();
                byte[] rawContent = socket.recv();

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
                List<I> dataVectors = inputVectorParser.parse(rawContent, inputSplits);

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

    private void validateRequestInputType(Model<I,O> model, DataType inputType) {
        if(model.inputType != inputType) {
            //TODO: THROW HERE
        }
    }

    private void handlePredictRequest(long msgId, List<I> dataVectors, Model<I,O> model, ZMQ.Socket socket)
            throws IOException {
        List<O> predictions = new ArrayList<>();
        for(I dataVector : dataVectors) {
            O prediction = model.predict(dataVector);
            predictions.add(prediction);
        }
        ByteArrayOutputStream concatenatedByteStream = new ByteArrayOutputStream();
        for(O prediction : predictions) {
            concatenatedByteStream.write(prediction.toBytes());
        }
        byte[] responseData = concatenatedByteStream.toByteArray();
        socket.send("", ZMQ.SNDMORE);
        socket.send(String.valueOf(msgId), ZMQ.SNDMORE);
        socket.send(responseData, 0, responseData.length);
    }
}
