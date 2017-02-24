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
        List<Double> data = new ArrayList<Double>();
        data.add(255.0);
        DoubleVector outputVector = new DoubleVector(data);
        return outputVector;
    }

}
