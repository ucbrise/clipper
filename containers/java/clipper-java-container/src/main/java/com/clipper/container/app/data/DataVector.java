package data;

public abstract class DataVector<T> {

    T data;

    DataVector(T data) {
        this.data = data;
    }

    public abstract byte[] toBytes();

    public T getData() {
        return data;
    }

}
