package data;

import java.util.List;

public abstract class DataVector<T> {

    List<T> data;

    DataVector(List<T> data) {
        this.data = data;
    }

    public abstract byte[] toBytes();

    protected List<T> getData() {
        return data;
    }

}
