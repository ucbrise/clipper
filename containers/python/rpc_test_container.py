import rpc
import os
import sys
import numpy as np


class RPCTestContainer(rpc.ModelContainerBase):
    def __init__(self, rpc_service):
        self.rpc_service = rpc_service

    def predict_doubles(self, inputs):
        clipper_time = inputs[0][0]
        event_history = self.rpc_service.get_event_history()
        recent_events = []
        for i in range(0, len(event_history)):
            curr_event = event_history[i]
            if curr_event[0] >= clipper_time:
                if i > 0 and len(recent_events) == 0:
                    # Capture the heartbeat message
                    # sent before Clipper came online
                    recent_events.append(event_history[i - 1][1])
                recent_events.append(event_history[i][1])
        print(recent_events)
        return np.array(recent_events, dtype='float32')


if __name__ == "__main__":
    ip = "127.0.0.1"
    port = 7000
    model_name = "rpctest_py"
    input_type = "doubles"
    model_version = 1

    rpc_service = rpc.RPCService()
    model = RPCTestContainer(rpc_service)
    rpc_service.start(model, ip, port, model_name, model_version, input_type)
