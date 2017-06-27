from __future__ import print_function
import zmq
import threading
import numpy as np
import struct
import time
from datetime import datetime
import socket
import sys
from collections import deque

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
SOCKET_ACTIVITY_TIMEOUT_MILLIS = 30000

EVENT_HISTORY_BUFFER_SIZE = 30

EVENT_HISTORY_SENT_HEARTBEAT = 1
EVENT_HISTORY_RECEIVED_HEARTBEAT = 2
EVENT_HISTORY_SENT_CONTAINER_METADATA = 3
EVENT_HISTORY_RECEIVED_CONTAINER_METADATA = 4
EVENT_HISTORY_SENT_CONTAINER_CONTENT = 5
EVENT_HISTORY_RECEIVED_CONTAINER_CONTENT = 6

MAXIMUM_UTF_8_CHAR_LENGTH_BYTES = 4
BYTES_PER_INT = 4


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


class EventHistory:
    def __init__(self, size):
        self.history_buffer = deque(maxlen=size)

    def insert(self, msg_type):
        curr_time_millis = time.time() * 1000
        self.history_buffer.append((curr_time_millis, msg_type))

    def get_events(self):
        return self.history_buffer


class PredictionError(Exception):
    def __init__(self, value):
        self.value = value

    def __str__(self):
        return repr(self.value)


class Server(threading.Thread):
    def __init__(self, context, clipper_ip, clipper_port):
        threading.Thread.__init__(self)
        self.context = context
        self.clipper_ip = clipper_ip
        self.clipper_port = clipper_port
        self.event_history = EventHistory(EVENT_HISTORY_BUFFER_SIZE)

    def handle_prediction_request(self, prediction_request):
        """
        Returns
        -------
        PredictionResponse
            A prediction response containing an output
            for each input included in the specified
            predict response
        """
        predict_fn = self.get_prediction_function()
        total_length = 0
        outputs = predict_fn(prediction_request.inputs)
        # Type check the outputs:
        if not type(outputs) == list:
            raise PredictionError("Model did not return a list")
        if len(outputs) != len(prediction_request.inputs):
            raise PredictionError(
                "Expected model to return %d outputs, found %d outputs" %
                (len(prediction_request.inputs), len(outputs)))
        if not type(outputs[0]) == str:
            raise PredictionError("Model must return a list of strs. Found %s"
                                  % type(outputs[0]))
        for o in outputs:
            total_length += len(o)
        response = PredictionResponse(prediction_request.msg_id,
                                      len(prediction_request.inputs),
                                      total_length)
        for output in outputs:
            response.add_output(output)

        return response

    def handle_feedback_request(self, feedback_request):
        """
        Returns
        -------
        FeedbackResponse
            A feedback response corresponding
            to the specified feedback request
        """
        response = FeedbackResponse(feedback_request.msg_id, "ACK")
        return response

    def get_prediction_function(self):
        if self.model_input_type == INPUT_TYPE_INTS:
            return self.model.predict_ints
        elif self.model_input_type == INPUT_TYPE_FLOATS:
            return self.model.predict_floats
        elif self.model_input_type == INPUT_TYPE_DOUBLES:
            return self.model.predict_doubles
        elif self.model_input_type == INPUT_TYPE_BYTES:
            return self.model.predict_bytes
        elif self.model_input_type == INPUT_TYPE_STRINGS:
            return self.model.predict_strings
        else:
            print(
                "Attempted to get predict function for invalid model input type!"
            )
            raise

    def get_event_history(self):
        return self.event_history.get_events()

    def run(self):
        print("Serving predictions for {0} input type.".format(
            input_type_to_string(self.model_input_type)))
        connected = False
        clipper_address = "tcp://{0}:{1}".format(self.clipper_ip,
                                                 self.clipper_port)
        poller = zmq.Poller()
        sys.stdout.flush()
        sys.stderr.flush()
        while True:
            socket = self.context.socket(zmq.DEALER)
            poller.register(socket, zmq.POLLIN)
            socket.connect(clipper_address)
            self.send_heartbeat(socket)
            while True:
                receivable_sockets = dict(
                    poller.poll(SOCKET_POLLING_TIMEOUT_MILLIS))
                if socket not in receivable_sockets or receivable_sockets[socket] != zmq.POLLIN:
                    # Failed to receive a message before the specified polling timeout
                    if connected:
                        curr_time = datetime.now()
                        time_delta = curr_time - last_activity_time_millis
                        time_delta_millis = (time_delta.seconds * 1000) + (
                            time_delta.microseconds / 1000)
                        if time_delta_millis >= SOCKET_ACTIVITY_TIMEOUT_MILLIS:
                            # Terminate the session
                            print("Connection timed out, reconnecting...")
                            sys.stdout.flush()
                            sys.stderr.flush()
                            connected = False
                            poller.unregister(socket)
                            socket.close()
                            break
                        else:
                            self.send_heartbeat(socket)
                    continue

                # Received a message before the polling timeout
                if not connected:
                    connected = True
                last_activity_time_millis = datetime.now()

                t1 = datetime.now()
                # Receive delimiter between routing identity and content
                socket.recv()
                msg_type_bytes = socket.recv()
                msg_type = struct.unpack("<I", msg_type_bytes)[0]
                if msg_type == MESSAGE_TYPE_HEARTBEAT:
                    self.event_history.insert(EVENT_HISTORY_RECEIVED_HEARTBEAT)
                    print("Received heartbeat!")
                    heartbeat_type_bytes = socket.recv()
                    heartbeat_type = struct.unpack("<I",
                                                   heartbeat_type_bytes)[0]
                    if heartbeat_type == HEARTBEAT_TYPE_REQUEST_CONTAINER_METADATA:
                        self.send_container_metadata(socket)
                    continue
                elif msg_type == MESSAGE_TYPE_NEW_CONTAINER:
                    self.event_history.insert(
                        EVENT_HISTORY_RECEIVED_CONTAINER_METADATA)
                    print(
                        "Received erroneous new container message from Clipper!"
                    )
                    continue
                elif msg_type == MESSAGE_TYPE_CONTAINER_CONTENT:
                    self.event_history.insert(
                        EVENT_HISTORY_RECEIVED_CONTAINER_CONTENT)
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

                        prediction_request = PredictionRequest(
                            msg_id_bytes, inputs)
                        response = self.handle_prediction_request(
                            prediction_request)

                        t4 = datetime.now()

                        response.send(socket, self.event_history)

                        print("recv: %f us, parse: %f us, handle: %f us" %
                              ((t2 - t1).microseconds, (t3 - t2).microseconds,
                               (t4 - t3).microseconds))
                        sys.stdout.flush()
                        sys.stderr.flush()

                    else:
                        feedback_request = FeedbackRequest(msg_id_bytes, [])
                        response = self.handle_feedback_request(received_msg)
                        response.send(socket, self.event_history)
                        print("recv: %f us" % ((t2 - t1).microseconds))

                sys.stdout.flush()
                sys.stderr.flush()

    def send_container_metadata(self, socket):
        socket.send("", zmq.SNDMORE)
        socket.send(struct.pack("<I", MESSAGE_TYPE_NEW_CONTAINER), zmq.SNDMORE)
        socket.send_string(self.model_name, zmq.SNDMORE)
        socket.send_string(str(self.model_version), zmq.SNDMORE)
        socket.send_string(str(self.model_input_type))
        self.event_history.insert(EVENT_HISTORY_SENT_CONTAINER_METADATA)
        print("Sent container metadata!")

    def send_heartbeat(self, socket):
        socket.send("", zmq.SNDMORE)
        socket.send(struct.pack("<I", MESSAGE_TYPE_HEARTBEAT))
        self.event_history.insert(EVENT_HISTORY_SENT_HEARTBEAT)
        print("Sent heartbeat!")


class PredictionRequest:
    """
    Parameters
    ----------
    msg_id : bytes
        The raw message id associated with the RPC 
        prediction request message
    inputs : 
        One of [[byte]], [[int]], [[float]], [[double]], [string]
    """

    def __init__(self, msg_id, inputs):
        self.msg_id = msg_id
        self.inputs = inputs

    def __str__(self):
        return self.inputs


class PredictionResponse():
    output_buffer = bytearray(1024)

    def __init__(self, msg_id, num_outputs, total_string_length):
        """
        Parameters
        ----------
        msg_id : bytes
            The message id associated with the PredictRequest
            for which this is a response
        num_outputs : int
            The number of outputs to be included in the prediction response
        max_outputs_size_bytes:
            The total length of the string content
        """
        self.msg_id = msg_id
        self.num_outputs = num_outputs
        self.expand_buffer_if_necessary(
            total_string_length * MAXIMUM_UTF_8_CHAR_LENGTH_BYTES)
        self.memview = memoryview(self.output_buffer)
        struct.pack_into("<I", self.output_buffer, 0, num_outputs)
        self.string_content_end_position = BYTES_PER_INT + (
            BYTES_PER_INT * num_outputs)
        self.current_output_sizes_position = BYTES_PER_INT

    def add_output(self, output):
        """
        Parameters
        ----------
        output : string
        """
        output = unicode(output, "utf-8").encode("utf-8")
        output_len = len(output)
        struct.pack_into("<I", self.output_buffer,
                         self.current_output_sizes_position, output_len)
        self.current_output_sizes_position += BYTES_PER_INT
        self.memview[self.string_content_end_position:
                     self.string_content_end_position + output_len] = output
        self.string_content_end_position += output_len

    def send(self, socket, event_history):
        socket.send("", flags=zmq.SNDMORE)
        socket.send(
            struct.pack("<I", MESSAGE_TYPE_CONTAINER_CONTENT),
            flags=zmq.SNDMORE)
        socket.send(self.msg_id, flags=zmq.SNDMORE)
        socket.send(self.output_buffer[0:self.string_content_end_position])
        event_history.insert(EVENT_HISTORY_SENT_CONTAINER_CONTENT)

    def expand_buffer_if_necessary(self, size):
        if len(self.output_buffer) < size:
            self.output_buffer = bytearray(size * 2)


class FeedbackRequest():
    def __init__(self, msg_id, content):
        self.msg_id = msg_id
        self.content = content

    def __str__(self):
        return self.content


class FeedbackResponse():
    def __init__(self, msg_id, content):
        self.msg_id = msg_id
        self.content = content

    def send(self, socket):
        socket.send("", flags=zmq.SNDMORE)
        socket.send(
            struct.pack("<I", MESSAGE_TYPE_CONTAINER_CONTENT),
            flags=zmq.SNDMORE)
        socket.send(self.msg_id, flags=zmq.SNDMORE)
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


class RPCService:
    def __init__(self):
        pass

    def get_event_history(self):
        if self.server:
            return self.server.get_event_history()
        else:
            print("Cannot retrieve message history for inactive RPC service!")
            raise

    def start(self, model, host, port, model_name, model_version, input_type):
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
        self.server = Server(context, ip, port)
        model_input_type = string_to_input_type(input_type)
        self.server.model_name = model_name
        self.server.model_version = model_version
        self.server.model_input_type = model_input_type
        self.server.model = model
        self.server.run()
