import rpc
import os
import sys
import numpy as np

class RPCTestContainer(rpc.ModelContainerBase):
	def __init__(self, rpc_service):
		self.rpc_service = rpc_service
	
	def predict_doubles(self, inputs):
		event_history = self.rpc_service.get_event_history()
		print(event_history)
		return np.array(event_history, dtype='float32')

if __name__ == "__main__":
	ip = "127.0.0.1"
	port = 7000
	model_name = "rpctest_py"
	input_type = "doubles"
	model_version = 1

	rpc_service = rpc.RPCService()
	model = RPCTestContainer(rpc_service)
	rpc_service.start(model, ip, port, model_name, model_version, input_type)




