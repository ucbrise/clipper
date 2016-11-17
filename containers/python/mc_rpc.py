from __future__ import print_function
import zmq
import time
import sys
import threading

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
		# Do work
		print('Received Message')
		# serialize floats
		# TODO: 
		# msg.set_content("Acknowledged!")
		preds = np.arange(len(msg.content), dtype='float32')
                assert preds.dtype == np.dtype("float32")
		msg.set_content(preds.tobytes())
		return msg

	def run(self):
		self.socket.connect("tcp://{0}:{1}".format(self.clipper_ip, self.clipper_port))
		self.socket.send("", zmq.SNDMORE);
		self.socket.send(self.model_name, zmq.SNDMORE);
		self.socket.send(str(self.model_version), zmq.SNDMORE);
		print("Serving...")
		while True:
			# Receive delimiter between identity and content
			self.socket.recv()
			msg_id = self.socket.recv()
			# list of bytes
			raw_content = self.socket.recv_multipart()
			# parse raw bytes into arrays of doubles
			inputs = [np.array(array.array('d', bytes(data))) for data in raw_content]
			print("received %d inputs" % len(content))
			print(inputs)
			received_msg = Message(msg_id, inputs)
			response = self.handle_message(received_msg)
			response.send(self.socket)

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

class ModelWrapperBase(object):
    def predict_ints(self, inputs):
        pass

    def predict_floats(self, inputs):
        pass

    def predict_bytes(self, inputs):
        pass

    def predict_strings(self, inputs):
        pass


if __name__ == "__main__":
	model_name = "m"
	version = 1
	context = zmq.Context();
	server = Server(context, sys.argv[1], sys.argv[2])
	server.model_name = model_name
	server.model_version = version
	server.run()
