package com.clipper.container.app.data;

public abstract class DataVector<T> {

    T data;

    DataVector(T data) {
        this.data = data;
    }

    public T getData() {
        return data;
    }

}
