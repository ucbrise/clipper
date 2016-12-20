from __future__ import print_function
import zmq
import sys
import threading
import numpy as np
import array
import struct
from datetime import datetime

INPUT_TYPE_BYTES = 0
INPUT_TYPE_INTS = 1
INPUT_TYPE_FLOATS = 2
INPUT_TYPE_DOUBLES = 3
INPUT_TYPE_STRINGS = 4

REQUEST_TYPE_PREDICT = 0
REQUEST_TYPE_FEEDBACK = 1

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

	def handle_feedback_request(self, msg):
		msg.set_content("ACK")
		return msg


	def handle_predict_request(self, msg):
		# msg.set_content("Acknowledged!")
		#preds = np.arange(len(msg.content), dtype='float32')
		preds = self.model.predict_floats(msg.content)
                assert preds.dtype == np.dtype("float32")
		msg.set_content(preds.tobytes())
		return msg

	def get_parse_type(self, input_type):
		if input_type == INPUT_TYPE_BYTES:
			return np.int8
		elif input_type == INPUT_TYPE_INTS:
			return np.int32
		elif input_type == INPUT_TYPE_FLOATS:
			return np.float32
		elif input_type == INPUT_TYPE_DOUBLES:
			return np.float64
		elif input_type == INPUT_TYPE_STRINGS:
			return np.str_ 

	def run(self):
		self.socket.connect("tcp://{0}:{1}".format(self.clipper_ip, self.clipper_port))
		self.socket.send("", zmq.SNDMORE);
		self.socket.send(self.model_name, zmq.SNDMORE);
		self.socket.send(str(self.model_version), zmq.SNDMORE);
		self.socket.send(str(self.model_input_type))
		print(self.model_input_type)
		print("Serving...")
		while True:
			# Receive delimiter between identity and content
			self.socket.recv()
			t1 = datetime.now()
			msg_id_bytes = self.socket.recv()
			msg_id = struct.unpack("<I", msg_id_bytes)
			print("Got start of message %d " % msg_id)
			# list of byte arrays
			request_header = self.socket.recv()
			request_type = struct.unpack("<I", request_header)[0]

			if request_type == REQUEST_TYPE_PREDICT:
				input_header = self.socket.recv()
				raw_content = self.socket.recv()

				t2 = datetime.now()

				parsed_input_header = np.frombuffer(input_header, dtype=np.int32)
				input_type, splits = parsed_input_header[0], parsed_input_header[1:]

				if int(input_type) != int(self.model_input_type):
					print('Received an input of incorrect type for this container!')
					raise

				if input_type == INPUT_TYPE_STRINGS:
					# If we're processing string inputs, we delimit them using
					# the null terminator included in their serialized representation,
					# ignoring the extraneous final null terminator by using a -1 slice
					inputs = np.split(np.array(raw_content.split('\0')[:-1], dtype=self.get_parse_type(input_type)), splits)
				else:
					inputs = np.split(np.frombuffer(raw_content, dtype=self.get_parse_type(input_type)), splits)

				t3 = datetime.now()

				received_msg = Message(msg_id_bytes, inputs)
				response = self.handle_predict_request(received_msg)

				t4 = datetime.now()

				response.send(self.socket)

				print("recv: %f us, parse: %f us, handle: %f us" % ((t2 - t1).microseconds, (t3 - t2).microseconds, (t4-t3).microseconds))

			else:
				received_msg = Message(msg_id_bytes, [])
				response = self.handle_feedback_request(received_msg)
				response.send(self.socket)

				print("recv: %f us" % ((t2 - t1).microseconds))
		

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
	
    if len(sys.argv) < 6:
	print("Invalid arguments")
	print("Usage:")
	print("\tpython rpc.py URL PORT MODEL_NAME MODEL_VERSION MODEL_INPUT_TYPE")
    else:
	context = zmq.Context();
	server = Server(context, sys.argv[1], sys.argv[2])
	model_name = sys.argv[3]
	version = int(sys.argv[4])
	model_input_type = sys.argv[5]
	server.model_name = model_name
	server.model_version = version
	server.model_input_type = model_input_type
	server.model = NoopContainer()
	server.run()
