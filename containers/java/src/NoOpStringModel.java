import data.DataType;
import data.DoubleVector;
import data.SerializableString;

public class NoOpStringModel extends Model<SerializableString, DoubleVector> {

    NoOpStringModel(String name, int version) {
        super(name, version, DataType.Strings, DataType.Doubles);
    }

    @Override
    public DoubleVector predict(SerializableString inputVector) {
        return new DoubleVector(new double[]{0});
    }
}
