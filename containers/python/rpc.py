from __future__ import print_function
import zmq
import threading
import numpy as np
import struct
from datetime import datetime
import socket
import sys

INPUT_TYPE_BYTES = 0
INPUT_TYPE_INTS = 1
INPUT_TYPE_FLOATS = 2
INPUT_TYPE_DOUBLES = 3
INPUT_TYPE_STRINGS = 4

REQUEST_TYPE_PREDICT = 0
REQUEST_TYPE_FEEDBACK = 1

MESSAGE_TYPE_NEW_CONTAINER = 0
MESSAGE_TYPE_CONTAINER_CONTENT = 1
MESSAGE_TYPE_HEARTBEAT = 2

HEARTBEAT_TYPE_KEEPALIVE = 0
HEARTBEAT_TYPE_REQUEST_CONTAINER_METADATA = 1

SOCKET_POLLING_TIMEOUT_MILLIS = 5000
SOCKET_ACTIVITY_TIMEOUT_MILLIS = 60000


def string_to_input_type(input_str):
    input_str = input_str.strip().lower()
    byte_strs = ["b", "bytes", "byte"]
    int_strs = ["i", "ints", "int", "integer", "integers"]
    float_strs = ["f", "floats", "float"]
    double_strs = ["d", "doubles", "double"]
    string_strs = ["s", "strings", "string", "strs", "str"]

    if any(input_str == s for s in byte_strs):
        return INPUT_TYPE_BYTES
    elif any(input_str == s for s in int_strs):
        return INPUT_TYPE_INTS
    elif any(input_str == s for s in float_strs):
        return INPUT_TYPE_FLOATS
    elif any(input_str == s for s in double_strs):
        return INPUT_TYPE_DOUBLES
    elif any(input_str == s for s in string_strs):
        return INPUT_TYPE_STRINGS
    else:
        return -1


def input_type_to_dtype(input_type):
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


def input_type_to_string(input_type):
    if input_type == INPUT_TYPE_BYTES:
        return "bytes"
    elif input_type == INPUT_TYPE_INTS:
        return "ints"
    elif input_type == INPUT_TYPE_FLOATS:
        return "floats"
    elif input_type == INPUT_TYPE_DOUBLES:
        return "doubles"
    elif input_type == INPUT_TYPE_STRINGS:
        return "string"


class Server(threading.Thread):
    def __init__(self, context, clipper_ip, clipper_port):
        threading.Thread.__init__(self)
        self.context = context
        self.clipper_ip = clipper_ip
        self.clipper_port = clipper_port

    def handle_feedback_request(self, msg):
        msg.set_content("ACK")
        return msg

    def handle_predict_request(self, msg):
        if self.model_input_type == INPUT_TYPE_INTS:
            preds = self.model.predict_ints(msg.content)
        elif self.model_input_type == INPUT_TYPE_FLOATS:
            preds = self.model.predict_floats(msg.content)
        elif self.model_input_type == INPUT_TYPE_DOUBLES:
            preds = self.model.predict_doubles(msg.content)
        elif self.model_input_type == INPUT_TYPE_BYTES:
            preds = self.model.predict_bytes(msg.content)
        elif self.model_input_type == INPUT_TYPE_STRINGS:
            preds = self.model.predict_strings(msg.content)
        assert preds.dtype == np.dtype("float32")
        msg.set_content(preds.tobytes())
        return msg

    def run(self):
        print("Serving predictions for {0} input type.".format(
            input_type_to_string(self.model_input_type)))
        connected = False
        clipper_address = "tcp://{0}:{1}".format(self.clipper_ip,
                                                 self.clipper_port)
        poller = zmq.Poller()
        while True:
            socket = self.context.socket(zmq.DEALER)
            poller.register(socket, zmq.POLLIN)
            socket.connect(clipper_address)
            self.send_heartbeat(socket)
            while True:
                receivable_sockets = dict(
                    poller.poll(SOCKET_POLLING_TIMEOUT_MILLIS))
                if socket not in receivable_sockets or receivable_sockets[socket] != zmq.POLLIN:
                    if connected:
                        curr_time = datetime.now()
                        time_delta = curr_time - last_activity_time_millis
                        time_delta_millis = (time_delta.seconds * 1000) + (
                            time_delta.microseconds / 1000)
                        if time_delta_millis >= SOCKET_ACTIVITY_TIMEOUT_MILLIS:
                            print("Connection timed out, reconnecting...")
                            connected = False
                            poller.unregister(socket)
                            socket.close()
                            break
                        else:
                            self.send_heartbeat(socket)
                    continue

                if not connected:
                    connected = True
                last_activity_time_millis = datetime.now()

                t1 = datetime.now()
                # Receive delimiter between routing identity and content
                socket.recv()
                msg_type_bytes = socket.recv()
                msg_type = struct.unpack("<I", msg_type_bytes)[0]
                if msg_type == MESSAGE_TYPE_HEARTBEAT:
                    print("Received heartbeat!")
                    heartbeat_type_bytes = socket.recv()
                    heartbeat_type = struct.unpack("<I",
                                                   heartbeat_type_bytes)[0]
                    if heartbeat_type == HEARTBEAT_TYPE_REQUEST_CONTAINER_METADATA:
                        self.send_container_metadata(socket)
                    continue
                elif msg_type == MESSAGE_TYPE_NEW_CONTAINER:
                    # This is an error, log it
                    continue
                elif msg_type == MESSAGE_TYPE_CONTAINER_CONTENT:
                    msg_id_bytes = socket.recv()
                    msg_id = int(struct.unpack("<I", msg_id_bytes)[0])

                    print("Got start of message %d " % msg_id)
                    # list of byte arrays
                    request_header = socket.recv()
                    request_type = struct.unpack("<I", request_header)[0]

                    if request_type == REQUEST_TYPE_PREDICT:
                        input_header_size = socket.recv()
                        input_header = socket.recv()
                        raw_content_size = socket.recv()
                        raw_content = socket.recv()

                        t2 = datetime.now()

                        parsed_input_header = np.frombuffer(
                            input_header, dtype=np.int32)
                        input_type, input_size, splits = parsed_input_header[
                            0], parsed_input_header[1], parsed_input_header[2:]

                        if int(input_type) != int(self.model_input_type):
                            print((
                                "Received incorrect input. Expected {expected}, "
                                "received {received}").format(
                                    expected=input_type_to_string(
                                        int(self.model_input_type)),
                                    received=input_type_to_string(
                                        int(input_type))))
                            raise

                        if input_type == INPUT_TYPE_STRINGS:
                            # If we're processing string inputs, we delimit them using
                            # the null terminator included in their serialized representation,
                            # ignoring the extraneous final null terminator by
                            # using a -1 slice
                            inputs = np.array(
                                raw_content.split('\0')[:-1],
                                dtype=input_type_to_dtype(input_type))
                        else:
                            inputs = np.array(
                                np.split(
                                    np.frombuffer(
                                        raw_content,
                                        dtype=input_type_to_dtype(input_type)),
                                    splits))

                        t3 = datetime.now()

                        received_msg = Message(msg_id_bytes, inputs)
                        response = self.handle_predict_request(received_msg)

                        t4 = datetime.now()

                        response.send(socket)

                        print("recv: %f us, parse: %f us, handle: %f us" %
                              ((t2 - t1).microseconds, (t3 - t2).microseconds,
                               (t4 - t3).microseconds))

                else:
                    received_msg = Message(msg_id_bytes, [])
                    response = self.handle_feedback_request(received_msg)
                    response.send(socket)
                    print("recv: %f us" % ((t2 - t1).microseconds))

        sys.stdout.flush()
        sys.stderr.flush()

    def send_container_metadata(self, socket):
        socket.send("", zmq.SNDMORE)
        socket.send(struct.pack("<I", MESSAGE_TYPE_NEW_CONTAINER), zmq.SNDMORE)
        socket.send_string(self.model_name, zmq.SNDMORE)
        socket.send_string(str(self.model_version), zmq.SNDMORE)
        socket.send_string(str(self.model_input_type))
        print("Sent container metadata!")

    def send_heartbeat(self, socket):
        socket.send("", zmq.SNDMORE)
        socket.send(struct.pack("<I", MESSAGE_TYPE_HEARTBEAT))
        print("Sent heartbeat!")


class Message:
    def __init__(self, msg_id, content):
        self.msg_id = msg_id
        self.content = content

    def __str__(self):
        return self.content

    def set_content(self, content):
        self.content = content

    def send(self, socket):
        socket.send("", zmq.SNDMORE)
        socket.send(
            struct.pack("<I", MESSAGE_TYPE_CONTAINER_CONTENT), zmq.SNDMORE)
        socket.send(self.msg_id, zmq.SNDMORE)
        socket.send(self.content)


class ModelContainerBase(object):
    def predict_ints(self, inputs):
        pass

    def predict_floats(self, inputs):
        pass

    def predict_doubles(self, inputs):
        pass

    def predict_bytes(self, inputs):
        pass

    def predict_strings(self, inputs):
        pass


def start(model, host, port, model_name, model_version, input_type):
    """
    Args:
        model (object): The loaded model object ready to make predictions.
        host (str): The Clipper RPC hostname or IP address.
        port (int): The Clipper RPC port.
        model_name (str): The name of the model.
        model_version (int): The version of the model
        input_type (str): One of ints, doubles, floats, bytes, strings.
    """

    try:
        ip = socket.gethostbyname(host)
    except socket.error as e:
        print("Error resolving %s: %s" % (host, e))
        sys.exit(1)
    context = zmq.Context()
    server = Server(context, ip, port)
    model_input_type = string_to_input_type(input_type)
    server.model_name = model_name
    server.model_version = model_version
    server.model_input_type = model_input_type
    server.model = model
    server.run()
