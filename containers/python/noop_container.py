from __future__ import print_function
import rpc
import os
import sys
import numpy as np


class NoopContainer(rpc.ModelContainerBase):
    def __init__(self, prediction="1.0"):
        self.prediction = prediction

    def _predict(self, inputs):
        return [self.prediction] * len(inputs)

    def predict_ints(self, inputs):
        return self._predict(inputs)

    def predict_floats(self, inputs):
        return self._predict(inputs)

    def predict_doubles(self, inputs):
        return self._predict(inputs)

    def predict_bytes(self, inputs):
        return self._predict(inputs)

    def predict_strings(self, inputs):
        return self._predict(inputs)


if __name__ == "__main__":
    print("Starting No-Op container")
    rpc_service = rpc.RPCService()
    model = NoopContainer()
    sys.stdout.flush()
    sys.stderr.flush()
    rpc_service.start(model)
