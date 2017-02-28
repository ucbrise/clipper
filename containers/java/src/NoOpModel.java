import data.DataType;
import data.DoubleVector;

import java.util.ArrayList;
import java.util.List;

public class NoOpModel extends Model<DoubleVector, DoubleVector> {

    NoOpModel(String name, int version) {
        super(name, version, DataType.Doubles, DataType.Doubles);
    }

    @Override
    public DoubleVector predict(DoubleVector inputVector) {
        DoubleVector outputVector = new DoubleVector(new double[]{255});
        return outputVector;
    }

}
