package ai.clipper.examples.serve;

import ai.clipper.examples.container.NoOpModel;
import ai.clipper.container.data.*;
import ai.clipper.container.ClipperModel;
import ai.clipper.examples.container.NoOpStringModel;
import ai.clipper.rpc.RPC;

import java.net.UnknownHostException;

public class RunContainer {
  private static final int EXPECTED_NUM_ARGUMENTS = 5;

  public static void main(String[] args) {
    if (args.length < EXPECTED_NUM_ARGUMENTS) {
      throw new IllegalArgumentException(
          "Arguments must be specified in order as: <model_name>, <model_version>, "
          + " <clipper_address>, <clipper_port>, <input_type>");
    }
    String modelName = args[0];
    int modelVersion = Integer.valueOf(args[1]);
    String clipperAddress = args[2];
    int clipperPort = Integer.valueOf(args[3]);
    DataType inputType = DataType.fromCode(Integer.valueOf(args[4]));

    ClipperModel model;
    if (inputType == DataType.Strings) {
      model = new NoOpStringModel();
    } else {
      model = new NoOpModel(inputType);
    }
    runContainer(model, getParserForInputType(inputType), clipperAddress, clipperPort, modelName,
        modelVersion);
  }

  private static DataVectorParser getParserForInputType(DataType inputType) {
    switch (inputType) {
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

  private static <I extends DataVector<?>> void runContainer(ClipperModel<I> model,
      DataVectorParser<?, I> parser, String clipperAddress, int clipperPort, String modelName,
      int modelVersion) {
    System.out.println("Starting...");
    RPC<I> rpcClient = new RPC(parser);
    try {
      rpcClient.start(model, modelName, modelVersion, clipperAddress, clipperPort);
    } catch (UnknownHostException e) {
      e.printStackTrace();
    }
  }
}
