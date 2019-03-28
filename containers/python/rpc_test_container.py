import rpc
import argparse
import json


class RPCTestContainer(rpc.ModelContainerBase):
    def __init__(self, rpc_service):
        self.rpc_service = rpc_service

    def predict_doubles(self, inputs):
        outputs = []
        for input_item in inputs:
            input_item = inputs[0]
            clipper_time = input_item[0]
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
            outputs.append(json.dumps(recent_events))
        return outputs


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '--rpc_service_port',
        type=int,
        default=7000)
    args = parser.parse_args()

    rpc_service = rpc.RPCService(collect_metrics=False, read_config=False)
    rpc_service.model_name = "rpctest_py"
    rpc_service.model_version = 1
    rpc_service.host = "127.0.0.1"
    rpc_service.port = args.rpc_service_port
    rpc_service.input_type = "doubles"

    model = RPCTestContainer(rpc_service)
    rpc_service.start(model)
