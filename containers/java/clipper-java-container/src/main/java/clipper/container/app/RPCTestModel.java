package clipper.container.app;

import clipper.container.app.data.DataType;
import clipper.container.app.data.DoubleVector;
import clipper.container.app.data.FloatVector;

import java.nio.FloatBuffer;

public class RPCTestModel extends Model<DoubleVector> {

    private final ModelContainer<DoubleVector> container;

    public RPCTestModel(ModelContainer<DoubleVector> container) {
        super("rpctest_java", 1, DataType.Doubles);
        this.container = container;
    }

    @Override
    public FloatVector predict(DoubleVector inputVector) {
        RPCEvent[] eventHistory = container.getEventHistory();
        float[] eventCodes = new float[eventHistory.length];
        for(int i = 0; i < eventHistory.length; i++) {
            eventCodes[i] = eventHistory[i].getCode();
        }
        FloatBuffer buffer = FloatBuffer.wrap(eventCodes);
        return new FloatVector(buffer);
    }
}
