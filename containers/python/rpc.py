from __future__ import print_function
import zmq
import sys
import threading
import numpy as np
import array
import struct
import rpc_pb2
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
		self.socket.send(msg.content);

	def handle_message(self, msg):
		# msg.set_content("Acknowledged!")
		# preds = np.arange(len(msg.content), dtype='float32')

		# DO PROTO PARSING HERE, TIME IT!
		before = datetime.now()
		request = rpc_pb2.Request()
		request.ParseFromString(msg.content[0])
		after = datetime.now()
		print("proto parsing: %f" % ((after - before).microseconds))

		if request.data_type != 1:
			print("Invalid data type! Currently only supporting doubles!")
			raise

		# parse raw bytes into arrays of doubles
		# TODO: this parsing is really slow
		inputs = [np.array(array.array('d', bytes(data_item.data))) for data_item in request.request_data]

		preds = self.model.predict_floats(inputs)
  		assert preds.dtype == np.dtype("float32")
		msg.set_content(preds.tobytes())
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
