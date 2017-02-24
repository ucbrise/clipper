import data.DataUtils;
import data.DoubleVector;

import java.net.UnknownHostException;
import java.util.List;

public class Test {

    public static void main(String[] args) {
        //testDataParsing();
        runContainer();
    }

    public static void runContainer() {
        System.out.println("Starting...");
        NoOpModel noOpModel = new NoOpModel("test", 1);
        ModelContainer<DoubleVector, DoubleVector> modelContainer =
                new ModelContainer<>(new DoubleVector.Parser());
        try {
            modelContainer.start(noOpModel, "127.0.0.1", 8000);
        } catch (UnknownHostException e) {
            e.printStackTrace();
        }
        while(true);
    }

    public static void testDataParsing() {
        byte[] bytes = new byte[4];
        bytes[0] = 0x03;
        bytes[1] = 0;
        bytes[2] = 0;
        bytes[3] = 0;

        List<Integer> ints = DataUtils.getSignedIntsFromBytes(bytes);
        System.out.println(ints.get(0));
    }


}
