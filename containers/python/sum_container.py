from __future__ import print_function
import rpc
import os
import sys
import numpy as np


class SumContainer(rpc.ModelContainerBase):
    def __init__(self):
        pass

    def predict_ints(self, inputs):
        return [str(sum(item)) for item in inputs]

    def predict_floats(self, inputs):
        return [str(sum(item)) for item in inputs]

    def predict_doubles(self, inputs):
        return [str(sum(item)) for item in inputs]

    def predict_bytes(self, inputs):
        return [str(sum(item)) for item in inputs]

    def predict_strings(self, inputs):
        return [str(len(item)) for item in inputs]


if __name__ == "__main__":
    rpc_service = rpc.RPCService()
    model = SumContainer()
    rpc_service.start(model)
