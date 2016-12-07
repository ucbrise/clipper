from __future__ import print_function
import zmq
import sys
import threading
import numpy as np
import array
import struct
import rpc_pb2
import flatbuffers
import clipper_fbs.Request as FbsRequest
import clipper_fbs.PredictRequest
import clipper_fbs.ByteVec
import clipper_fbs.DoubleVec
import clipper_fbs.IntVec
import clipper_fbs.StringVec
import clipper_fbs.FloatVec
from datetime import datetime

class Server(threading.Thread):

	def __init__(self, context, clipper_ip, clipper_port):
		threading.Thread.__init__(self)
		self.socket = context.socket(zmq.DEALER)
		self.clipper_ip = clipper_ip
		self.clipper_port = clipper_port

	def respond(self, msg):
		self.socket.send(msg.identity, flags=zmq.SNDMORE)
		self.socket.send("", flags=zmq.SNDMORE)
		self.socket.send(msg.content)

	def parse_data(self, data_vec, dtype):
		o = flatbuffers.number_types.UOffsetTFlags.py_type(data_vec._tab.Offset(4))
		if o != 0:
			length = data_vec._tab.VectorLen(o)
			a = data_vec._tab.Vector(o)
			offset = a + flatbuffers.number_types.UOffsetTFlags.py_type(0)
			parsed_data = np.frombuffer(data_vec._tab.Bytes, dtype=dtype, count=length, offset=offset)
			return parsed_data
		else:
			print("Failed to deserialize data!")
			raise 

	def parse_doubles(self, double_vec):
		return self.parse_data(double_vec, np.float64)

	def parse_floats(self, float_vec):
		return self.parse_data(float_vec, np.float32)

	def parse_ints(self, int_vec):
		return self.parse_data(int_vec, np.int32)

	def parse_bytes(self, byte_vec):
		return self.parse_data(byte_vec, np.uint8)

	def parse_strings(self, string_vec):
		# TODO...
		return np.zeros(string_vec.DataLength())

	def handle_message(self, msg):
		request = FbsRequest.Request.GetRootAsRequest(msg.content[0], 0)
		request_type = request.RequestType()

		# If we have a prediction request, process it
		if request_type == 0:
			prediction_request = request.PredictionRequest()

			for i in range(0, prediction_request.DoubleDataLength()):
				self.parse_doubles(prediction_request.DoubleData(i))

			for i in range(0, prediction_request.FloatDataLength()):
				self.parse_floats(prediction_request.FloatData(i))

			for i in range(0, prediction_request.IntegerDataLength()):
				self.parse_ints(prediction_request.IntegerData(i))

			for i in range(0, prediction_request.ByteDataLength()):
				self.parse_bytes(prediction_request.ByteData(i))

			for i in range(0, prediction_request.StringDataLength()):
				self.parse_strings(prediction_request.StringData(i))


		# # preds = self.model.predict_floats(inputs)
  # # 		assert preds.dtype == np.dtype("float32")
		# msg.set_content(preds.tobytes())
		msg.set_content("ACK")
		return msg

	def run(self):
		self.socket.connect("tcp://{0}:{1}".format(self.clipper_ip, self.clipper_port))
		self.socket.send("", zmq.SNDMORE);
		self.socket.send(self.model_name, zmq.SNDMORE);
		self.socket.send(str(self.model_version));
		print("Serving...")
		while True:
			# Receive delimiter between identity and content
			self.socket.recv()
			t1 = datetime.now()
			msg_id_bytes = self.socket.recv()
			msg_id = struct.unpack("<I", msg_id_bytes)
			print("Got start of message %d " % msg_id)
			# list of byte arrays
			raw_content = self.socket.recv_multipart()
			t2 = datetime.now()
			received_msg = Message(msg_id_bytes, raw_content)
			t3 = datetime.now()
			# print("received %d inputs" % len(raw_content))
			response = self.handle_message(received_msg)
			t4 = datetime.now()
			response.send(self.socket)
			print("recv: %f us, parse: %f us, handle: %f" % ((t2 - t1).microseconds, (t3 - t2).microseconds, (t4-t3).microseconds))

class Message:

	def __init__(self, msg_id, content):
		self.msg_id = msg_id
		self.content = content

	def __str__(self):
		return self.content

	def set_content(self, content):
		self.content = content

	def send(self, socket):
		socket.send("", flags=zmq.SNDMORE)
		socket.send(self.msg_id, flags=zmq.SNDMORE)
		socket.send(self.content)

class ModelContainerBase(object):
    def predict_ints(self, inputs):
        pass

    def predict_floats(self, inputs):
        pass

    def predict_bytes(self, inputs):
        pass

    def predict_strings(self, inputs):
        pass

class NoopContainer(ModelContainerBase):
    def __init__(self):
	pass

    def predict_floats(self, inputs):
	return np.array([np.sum(x) for x in inputs], dtype='float32')
        # return np.ones(len(inputs), dtype='float32')



if __name__ == "__main__":
	
    if len(sys.argv) < 4:
	print("Invalid arguments")
	print("Usage:")
	print("\tpython rpc.py URL PORT MODEL_NAME MODEL_VERSION")
    else:
	context = zmq.Context();
	server = Server(context, sys.argv[1], sys.argv[2])
	model_name = sys.argv[3]
	version = int(sys.argv[4])
	server.model_name = model_name
	server.model_version = version
	server.model = NoopContainer()
	server.run()
