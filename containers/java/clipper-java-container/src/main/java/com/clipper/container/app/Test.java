import data.*;

import java.net.UnknownHostException;

public class Test {

    public static void main(String[] args) {
        NoOpModel model = new NoOpModel("test", 1);
        runContainer(model, new DoubleVector.Parser());
//        NoOpStringModel model = new NoOpStringModel("test", 1);
//        runContainer(model, new SerializableString.Parser());
    }

    public static <I extends DataVector<?>, O extends DataVector> void runContainer(
            Model<I,O> model,
            DataVectorParser<?, I> parser) {
        System.out.println("Starting...");
        ModelContainer<I,O> modelContainer = new ModelContainer(parser);
        try {
            modelContainer.start(model, "127.0.0.1", 8000);
        } catch (UnknownHostException e) {
            e.printStackTrace();
        }
        while(true);
    }

}
