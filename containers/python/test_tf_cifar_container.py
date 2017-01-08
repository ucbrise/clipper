import numpy as np
import os
import pandas as pd
import rpc
import sys
from tf_cifar_container import TfCifarContainer
from sklearn.metrics import accuracy_score

classes = ['airplane', 'automobile', 'bird', 'cat',
                   'deer', 'dog', 'frog', 'horse', 'ship', 'truck']
positive_class = classes.index('airplane')
negative_class = classes.index('bird')

def load_cifar(cifar_location, cifar_filename = "train.data", norm=False):
    cifar_path = cifar_location + "/" + cifar_filename
    print("Source file: %s" % cifar_path)
    df = pd.read_csv(cifar_path, sep=",", header=None)
    data = df.values
    print("Number of image files: %d" % len(data))
    y = data[:,0]
    X = data[:,1:]
    Z = X
    if norm:
        mu = np.mean(X.T,0)
        sigma = np.var(X.T,0)
        Z = (X.T - mu) / np.array([np.sqrt(z) if z > 0 else 1. for z in sigma])
        Z = Z.T
    return (Z, y)

def filter_data(X, y):
    X_train, y_train = [], []
    for (example, label) in zip(X, y):
        if label == positive_class:
            X_train.append(example)
            y_train.append(1.0)
        elif label == negative_class:
            X_train.append(example)
            y_train.append(-1.0)
    X_train = np.array(X_train)
    y_train = np.array(y_train)
    return X_train, y_train

if __name__ == '__main__':
    model_path = os.environ["CLIPPER_MODEL_PATH"]
    model = TfCifarContainer(model_path)
    X_test, y_test = load_cifar('data', 'test.data')
    X_test, y_test = filter_data(X_test, y_test)
    y_test[np.where(y_test == -1)] = 0
    preds = model.predict_ints(X_test)
    print("Test accuracy: %f" % accuracy_score(y_test, preds))
