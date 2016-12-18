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
from collections import deque

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

	def parse_data_vector(self, data_vec, dtype):
		o = flatbuffers.number_types.UOffsetTFlags.py_type(data_vec._tab.Offset(4))
		if o != 0:
			length = data_vec._tab.VectorLen(o)
			a = data_vec._tab.Vector(o)
			offset = a + flatbuffers.number_types.UOffsetTFlags.py_type(0)
			print(offset)
			t1 = datetime.now()
			parsed_data = np.frombuffer(data_vec._tab.Bytes, dtype=dtype, count=length, offset=offset)
			t2 = datetime.now()
			return parsed_data, (t2 - t1)
		else:
			print("Failed to deserialize data!")
			raise

	def parse_int_data(self, pred_request):
		o = flatbuffers.number_types.UOffsetTFlags.py_type(pred_request._tab.Offset(4))
		parsed_int_vecs = []
		if o != 0:
			length = pred_request._tab.VectorLen(o)
			x = pred_request._tab.Vector(o)
			for j in range(0, length):
				obj = clipper_fbs.IntVec.IntVec()
				indirection_index = flatbuffers.number_types.UOffsetTFlags.py_type(j) * 4
				obj.Init(pred_request._tab.Bytes, pred_request._tab.Indirect(x + indirection_index))
				parsed_int_vecs.append(self.parse_data_vector(obj, np.int32))
		return np.array(parsed_int_vecs)

	def parse_float_data(self, pred_request):
		o = flatbuffers.number_types.UOffsetTFlags.py_type(pred_request._tab.Offset(6))
		parsed_float_vecs = []
		if o != 0:
			length = pred_request._tab.VectorLen(o)
			x = pred_request._tab.Vector(o)
			for j in range(0, length):
				obj = clipper_fbs.FloatVec.FloatVec()
				indirection_index = flatbuffers.number_types.UOffsetTFlags.py_type(j) * 4
				obj.Init(pred_request._tab.Bytes, pred_request._tab.Indirect(x + indirection_index))
				parsed_float_vecs.append(self.parse_data_vector(obj, np.float32))
		return np.array(parsed_float_vecs)

	def parse_double_data(self, pred_request):
		o = flatbuffers.number_types.UOffsetTFlags.py_type(pred_request._tab.Offset(8))
		parsed_double_vecs = deque()
		total_time = 0
		if o != 0:
			length = pred_request._tab.VectorLen(o)
			x = pred_request._tab.Vector(o)
			for j in range(0, length):
				t1 = datetime.now()
				obj = clipper_fbs.DoubleVec.DoubleVec()
				indirection_index = flatbuffers.number_types.UOffsetTFlags.py_type(j) * 4
				obj.Init(pred_request._tab.Bytes, pred_request._tab.Indirect(x + indirection_index))
				parsed, time = self.parse_data_vector(obj, np.float64)
				parsed_double_vecs.append(parsed)
				t2 = datetime.now()
				total_time += (t2 - t1).microseconds
		print(total_time)
		return np.array(parsed_double_vecs)

	def parse_string_data(self, pred_request):
		o = flatbuffers.number_types.UOffsetTFlags.py_type(pred_request._tab.Offset(10))
		parsed_string_vecs = []
		if o != 0:
			length = pred_request._tab.VectorLen(o)
			x = pred_request._tab.Vector(o)
			for j in range(0, length):
				obj = clipper_fbs.StringVec.StringVec()
				indirection_index = flatbuffers.number_types.UOffsetTFlags.py_type(j) * 4
				obj.Init(pred_request._tab.Bytes, pred_request._tab.Indirect(x + indirection_index))

				# TODO: Parse the individual strings
				parsed_string_vecs.append(np.zeros(obj.DataLength()))
		return parsed_string_vecs

	def parse_byte_data(self, pred_request):
		o = flatbuffers.number_types.UOffsetTFlags.py_type(pred_request._tab.Offset(12))
		parsed_byte_vecs = []
		if o != 0:
			length = pred_request._tab.VectorLen(o)
			x = pred_request._tab.Vector(o)
			for j in range(0, length):
				obj = clipper_fbs.ByteVec.ByteVec()
				indirection_index = flatbuffers.number_types.UOffsetTFlags.py_type(j) * 4
				obj.Init(pred_request._tab.Bytes, pred_request._tab.Indirect(x + indirection_index))
				parsed_byte_vecs.append(self.parse_data_vector(obj, np.uint8))
		return np.array(parsed_byte_vecs)

	def handle_message(self, msg):
		request = FbsRequest.Request.GetRootAsRequest(msg.content[0], 0)
		request_type = request.RequestType()

		# If we have a prediction request, process it
		if request_type == 0:
			prediction_request = request.PredictionRequest()

			t1 = datetime.now()
			parsed_doubles = self.parse_double_data(prediction_request)
			t2 = datetime.now()
			parsed_floats = self.parse_float_data(prediction_request)
			parsed_ints = self.parse_int_data(prediction_request)
			parsed_bytes = self.parse_byte_data(prediction_request)
			parsed_strings = self.parse_string_data(prediction_request)

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
