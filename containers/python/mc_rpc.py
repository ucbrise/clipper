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
		print 'Received Message'
		msg.set_content("Acknowledged!")
		return msg

	def run(self):
		self.socket.connect("tcp://{0}:{1}".format(self.clipper_ip, self.clipper_port))
		self.socket.send("", zmq.SNDMORE);
		self.socket.send(bytes([-1]), zmq.SNDMORE);
		self.socket.send("CONNECT");
		print("Serving...")
		while True:
			# Receive delimiter between identity and content
			self.socket.recv()
			msg_id = self.socket.recv()
			content = self.socket.recv()
			received_msg = Message(msg_id, content)
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


if __name__ == "__main__":
	context = zmq.Context();
	server = Server(context, sys.argv[1], sys.argv[2])
	server.run()
