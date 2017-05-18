package ai.clipper.rpctest;

import ai.clipper.container.data.DoubleVector;
import ai.clipper.rpc.RPC;

import java.net.UnknownHostException;

public class RPCProtocolTest {
  public static void main(String[] args) {
    RPC<DoubleVector> rpc = new RPC<>(new DoubleVector.Parser());
    RPCTestModel testModel = new RPCTestModel(rpc);
    String clipperAddress = "localhost";
    int clipperPort = 7000;
    try {
      rpc.start(testModel, "rpctest_java", 1, clipperAddress, clipperPort);
    } catch (UnknownHostException e) {
      e.printStackTrace();
    }
  }
}
