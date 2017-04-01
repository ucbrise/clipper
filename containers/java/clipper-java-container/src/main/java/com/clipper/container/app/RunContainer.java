package com.clipper.container.app;

import com.clipper.container.app.data.*;

import java.net.UnknownHostException;

public class RunContainer {

    private static final int EXPECTED_NUM_ARGUMENTS = 5;

    public static void main(String[] args) {
        if(args.length < EXPECTED_NUM_ARGUMENTS) {
            throw new IllegalArgumentException(
                    "Arguments must be specified in order as: <model_name>, <model_version>, "
                    + " <clipper_address>, <clipper_port>, <input_type>");
        }
        String modelName = args[0];
        int modelVersion = Integer.valueOf(args[1]);
        String clipperAddress = args[2];
        int clipperPort = Integer.valueOf(args[3]);
        DataType inputType = DataType.fromCode(Integer.valueOf(args[4]));

        Model model;
        if(inputType == DataType.Strings) {
            model = new NoOpStringModel(modelName, modelVersion);
        } else {
            model = new NoOpModel(modelName, modelVersion, inputType);
        }
        runContainer(model, getParserForInputType(inputType), clipperAddress, clipperPort);
    }

    private static DataVectorParser getParserForInputType(DataType inputType) {
        switch(inputType) {
            case Bytes:
                return new ByteVector.Parser();
            case Ints:
                return new IntVector.Parser();
            case Floats:
                return new FloatVector.Parser();
            case Doubles:
                return new DoubleVector.Parser();
            case Strings:
                return new SerializableString.Parser();
            default:
                return new ByteVector.Parser();
        }
    }

    private static <I extends DataVector<?>> void runContainer(
            Model<I> model, DataVectorParser<?, I> parser, String clipperAddress, int clipperPort) {
        System.out.println("Starting...");
        ModelContainer<I> modelContainer = new ModelContainer(parser);
        try {
            modelContainer.start(model, clipperAddress, clipperPort);
        } catch (UnknownHostException e) {
            e.printStackTrace();
        }
        while (true);
    }

}
