import data.DataVector;

import java.util.List;

public class RPCMessage<T extends DataVector> {

    private int id;
    private List<T> dataVectors;

    RPCMessage(int id, List<T> dataVectors) {

    }

}
