package clipper.container.app;

import java.net.UnknownHostException;

import clipper.container.app.data.*;
import org.junit.Test;

public class ContainerTest {
  @Test
  public void TestContainer() {
    NoOpModel model = new NoOpModel("test", 1, DataType.Doubles);
    runContainer(model, new DoubleVector.Parser());
    //    NoOpStringModel model = new NoOpStringModel("test", 1);
    //    runContainer(model, new SerializableString.Parser());
  }

  private <I extends DataVector<?>> void runContainer(
      Model<I> model, DataVectorParser<?, I> parser) {
    System.out.println("Starting...");
    ModelContainer<I> modelContainer = new ModelContainer(parser);
    try {
      modelContainer.start(model, "127.0.0.1", 7000);
    } catch (UnknownHostException e) {
      e.printStackTrace();
    }
    while (true)
      ;
  }
}
