package clipper.container.app;

import clipper.container.app.data.DoubleVector;
import org.junit.Test;

import java.net.UnknownHostException;

public class RPCTest {

    @Test
    public void testRPCProtocolCorrect() {
        ModelContainer<DoubleVector> container = new ModelContainer<DoubleVector>(new DoubleVector.Parser());
        RPCTestModel testModel = new RPCTestModel(container);
        String clipperAddress = "localhost";
        int clipperPort = 7000;
        try {
            container.start(testModel, clipperAddress, clipperPort);
        } catch (UnknownHostException e) {
            e.printStackTrace();
        }
        while(true);
    }

}
