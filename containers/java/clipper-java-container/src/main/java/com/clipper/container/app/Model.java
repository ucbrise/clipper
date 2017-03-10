import data.DataVector;
import data.DataType;

abstract class Model<I extends DataVector, O extends DataVector> {

    private String name;
    private int version;
    DataType inputType;
    DataType outputType;

    Model(String name, int version, DataType inputType, DataType outputType) {
        this.name = name;
        this.version = version;
        this.inputType = inputType;
        this.outputType = outputType;
    }

    public String getName() {
        return name;
    }

    public int getVersion() {
        return version;
    }

    public DataType getInputType() {
        return inputType;
    }

    public DataType getOutputType() {
        return outputType;
    }

    public abstract O predict(I inputVector);

}