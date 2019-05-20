from __future__ import print_function
import zmq
import threading
import numpy as np
import struct
import time
from datetime import datetime
import socket
import sys
import os
import yaml
import logging
from collections import deque
if sys.version_info < (3, 0):
    from subprocess32 import Popen, PIPE
else:
    from subprocess import Popen, PIPE
from prometheus_client import start_http_server
from prometheus_client.core import Counter, Gauge, Histogram, Summary
import clipper_admin.metrics as metrics

RPC_VERSION = 3

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
BYTES_PER_LONG = 8

# Initial size of the buffer used for receiving
# request input content
INITIAL_INPUT_CONTENT_BUFFER_SIZE = 1024
# Initial size of the buffers used for sending response
# header data and receiving request header data
INITIAL_HEADER_BUFFER_SIZE = 1024

INPUT_HEADER_DTYPE = np.dtype(np.uint64)

logger = logging.getLogger(__name__)


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
        return np.dtype(np.int8)
    elif input_type == INPUT_TYPE_INTS:
        return np.dtype(np.int32)
    elif input_type == INPUT_TYPE_FLOATS:
        return np.dtype(np.float32)
    elif input_type == INPUT_TYPE_DOUBLES:
        return np.dtype(np.float64)
    elif input_type == INPUT_TYPE_STRINGS:
        return str


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

    def validate_rpc_version(self, received_version):
        if received_version != RPC_VERSION:
            print(
                "ERROR: Received an RPC message with version: {clv} that does not match container version: {mcv}"
                .format(clv=received_version, mcv=RPC_VERSION))

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
        response = PredictionResponse(prediction_request.msg_id)
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

    def run(self, collect_metrics=True):
        print("Serving predictions for {0} input type.".format(
            input_type_to_string(self.model_input_type)))
        connected = False
        clipper_address = "tcp://{0}:{1}".format(self.clipper_ip,
                                                 self.clipper_port)
        poller = zmq.Poller()
        sys.stdout.flush()
        sys.stderr.flush()

        self.input_header_buffer = bytearray(INITIAL_HEADER_BUFFER_SIZE)
        self.input_content_buffer = bytearray(
            INITIAL_INPUT_CONTENT_BUFFER_SIZE)

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
                            connected = False
                            poller.unregister(socket)
                            socket.close()
                            break
                        else:
                            self.send_heartbeat(socket)
                        sys.stdout.flush()
                        sys.stderr.flush()
                    continue

                # Received a message before the polling timeout
                if not connected:
                    connected = True
                last_activity_time_millis = datetime.now()

                t1 = datetime.now()
                # Receive delimiter between routing identity and content
                socket.recv()
                rpc_version_bytes = socket.recv()
                rpc_version = struct.unpack("<I", rpc_version_bytes)[0]
                self.validate_rpc_version(rpc_version)
                msg_type_bytes = socket.recv()
                msg_type = struct.unpack("<I", msg_type_bytes)[0]
                if msg_type == MESSAGE_TYPE_HEARTBEAT:
                    self.event_history.insert(EVENT_HISTORY_RECEIVED_HEARTBEAT)
                    print("Received heartbeat!")
                    sys.stdout.flush()
                    sys.stderr.flush()
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
                        input_header_size_raw = socket.recv()
                        input_header_size_bytes = struct.unpack(
                            "<Q", input_header_size_raw)[0]

                        typed_input_header_size = int(
                            input_header_size_bytes /
                            INPUT_HEADER_DTYPE.itemsize)

                        if len(self.input_header_buffer
                               ) < input_header_size_bytes:
                            self.input_header_buffer = bytearray(
                                input_header_size_bytes * 2)

                        # While this procedure still incurs a copy, it saves a potentially
                        # costly memory allocation by ZMQ. This savings only occurs
                        # if the input header did not have to be resized
                        input_header_view = memoryview(
                            self.input_header_buffer)[:input_header_size_bytes]
                        input_header_content = socket.recv(copy=False).buffer
                        input_header_view[:
                                          input_header_size_bytes] = input_header_content

                        parsed_input_header = np.frombuffer(
                            self.input_header_buffer,
                            dtype=INPUT_HEADER_DTYPE)[:typed_input_header_size]

                        input_type, num_inputs, input_sizes = parsed_input_header[
                            0], parsed_input_header[1], parsed_input_header[2:]

                        input_dtype = input_type_to_dtype(input_type)
                        input_sizes = [
                            int(inp_size) for inp_size in input_sizes
                        ]

                        if input_type == INPUT_TYPE_STRINGS:
                            inputs = self.recv_string_content(
                                socket, num_inputs, input_sizes)
                        else:
                            inputs = self.recv_primitive_content(
                                socket, num_inputs, input_sizes, input_dtype)

                        t2 = datetime.now()

                        if int(input_type) != int(self.model_input_type):
                            print((
                                "Received incorrect input. Expected {expected}, "
                                "received {received}").format(
                                    expected=input_type_to_string(
                                        int(self.model_input_type)),
                                    received=input_type_to_string(
                                        int(input_type))))
                            raise

                        t3 = datetime.now()

                        prediction_request = PredictionRequest(
                            msg_id_bytes, inputs)
                        response = self.handle_prediction_request(
                            prediction_request)

                        t4 = datetime.now()

                        response.send(socket, self.event_history)

                        recv_time = (t2 - t1).total_seconds()
                        parse_time = (t3 - t2).total_seconds()
                        handle_time = (t4 - t3).total_seconds()

                        if collect_metrics:
                            metrics.report_metric('clipper_mc_pred_total', 1)
                            metrics.report_metric('clipper_mc_recv_time_ms',
                                                  recv_time * 1000.0)
                            metrics.report_metric('clipper_mc_parse_time_ms',
                                                  parse_time * 1000.0)
                            metrics.report_metric('clipper_mc_handle_time_ms',
                                                  handle_time * 1000.0)
                            metrics.report_metric(
                                'clipper_mc_end_to_end_latency_ms',
                                (recv_time + parse_time + handle_time) *
                                1000.0)

                        print("recv: %f s, parse: %f s, handle: %f s" %
                              (recv_time, parse_time, handle_time))

                        sys.stdout.flush()
                        sys.stderr.flush()

                    else:
                        feedback_request = FeedbackRequest(msg_id_bytes, [])
                        response = self.handle_feedback_request(received_msg)
                        response.send(socket, self.event_history)
                        print("recv: %f s" % ((t2 - t1).total_seconds()))

                sys.stdout.flush()
                sys.stderr.flush()

    def recv_string_content(self, socket, num_inputs, input_sizes):
        # Create an empty numpy array that will contain
        # input string references
        inputs = np.empty(num_inputs, dtype=object)
        for i in range(num_inputs):
            # Obtain a memoryview of the received message's
            # ZMQ frame buffer
            input_item_buffer = socket.recv(copy=False).buffer
            # Copy the memoryview content into a string object
            input_str = input_item_buffer.tobytes()
            inputs[i] = input_str

        return inputs

    def recv_primitive_content(self, socket, num_inputs, input_sizes,
                               input_dtype):
        def recv_different_lengths():
            # Create an empty numpy array that will contain
            # input array references
            inputs = np.empty(num_inputs, dtype=object)
            for i in range(num_inputs):
                # Receive input data and copy it into a byte
                # buffer that can be parsed into a writeable
                # array
                input_item_buffer = socket.recv(copy=True)
                input_item = np.frombuffer(
                    input_item_buffer, dtype=input_dtype)
                inputs[i] = input_item

            return inputs

        def recv_same_lengths():
            input_type_size_bytes = input_dtype.itemsize
            input_content_size_bytes = sum(input_sizes)
            typed_input_content_size = int(
                input_content_size_bytes / input_type_size_bytes)

            if len(self.input_content_buffer) < input_content_size_bytes:
                self.input_content_buffer = bytearray(
                    input_content_size_bytes * 2)

            input_content_view = memoryview(
                self.input_content_buffer)[:input_content_size_bytes]

            item_start_idx = 0
            for i in range(num_inputs):
                input_size = input_sizes[i]
                # Obtain a memoryview of the received message's
                # ZMQ frame buffer
                input_item_buffer = socket.recv(copy=False).buffer
                # Copy the memoryview content into a pre-allocated content buffer
                input_content_view[item_start_idx:item_start_idx +
                                   input_size] = input_item_buffer
                item_start_idx += input_size

            # Reinterpret the content buffer as a typed numpy array
            inputs = np.frombuffer(
                self.input_content_buffer,
                dtype=input_dtype)[:typed_input_content_size]

            # All inputs are of the same size, so we can use
            # np.reshape to construct an input matrix
            inputs = np.reshape(inputs, (len(input_sizes), -1))

            return inputs

        if len(set(input_sizes)) == 1:
            return recv_same_lengths()
        else:
            return recv_different_lengths()

    def send_container_metadata(self, socket):
        if sys.version_info < (3, 0):
            socket.send("", zmq.SNDMORE)
        else:
            socket.send("".encode('utf-8'), zmq.SNDMORE)
        socket.send(struct.pack("<I", MESSAGE_TYPE_NEW_CONTAINER), zmq.SNDMORE)
        socket.send_string(self.model_name, zmq.SNDMORE)
        socket.send_string(str(self.model_version), zmq.SNDMORE)
        socket.send_string(str(self.model_input_type), zmq.SNDMORE)
        socket.send(struct.pack("<I", RPC_VERSION))
        self.event_history.insert(EVENT_HISTORY_SENT_CONTAINER_METADATA)
        print("Sent container metadata!")
        sys.stdout.flush()
        sys.stderr.flush()

    def send_heartbeat(self, socket):
        if sys.version_info < (3, 0):
            socket.send("", zmq.SNDMORE)
        else:
            socket.send_string("", zmq.SNDMORE)
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


class PredictionResponse:
    header_buffer = bytearray(INITIAL_HEADER_BUFFER_SIZE)

    def __init__(self, msg_id):
        """
        Parameters
        ----------
        msg_id : bytes
            The message id associated with the PredictRequest
            for which this is a response
        """
        self.msg_id = msg_id
        self.outputs = []
        self.num_outputs = 0

    def add_output(self, output):
        """
        Parameters
        ----------
        output : string
        """
        if not isinstance(output, str):
            output = unicode(output, "utf-8").encode("utf-8")
        else:
            output = output.encode('utf-8')
        self.outputs.append(output)
        self.num_outputs += 1

    def send(self, socket, event_history):
        """
        Sends the encapsulated response data via
        the specified socket

        Parameters
        ----------
        socket : zmq.Socket
        event_history : EventHistory
            The RPC event history that should be
            updated as a result of this operation
        """
        assert self.num_outputs > 0
        output_header, header_length_bytes = self._create_output_header()
        if sys.version_info < (3, 0):
            socket.send("", flags=zmq.SNDMORE)
        else:
            socket.send_string("", flags=zmq.SNDMORE)
        socket.send(
            struct.pack("<I", MESSAGE_TYPE_CONTAINER_CONTENT),
            flags=zmq.SNDMORE)
        socket.send(self.msg_id, flags=zmq.SNDMORE)
        socket.send(struct.pack("<Q", header_length_bytes), flags=zmq.SNDMORE)
        socket.send(output_header, flags=zmq.SNDMORE)
        for idx in range(self.num_outputs):
            if idx == self.num_outputs - 1:
                # Don't use the `SNDMORE` flag if
                # this is the last output being sent
                socket.send(self.outputs[idx])
            else:
                socket.send(self.outputs[idx], flags=zmq.SNDMORE)

        event_history.insert(EVENT_HISTORY_SENT_CONTAINER_CONTENT)

    def _expand_buffer_if_necessary(self, size):
        """
        If necessary, expands the reusable output
        header buffer to accomodate content of the
        specified size

        size : int
            The size, in bytes, that the buffer must be
            able to store
        """
        if len(PredictionResponse.header_buffer) < size:
            PredictionResponse.header_buffer = bytearray(size * 2)

    def _create_output_header(self):
        """
        Returns
        ----------
        (bytearray, int)
            A tuple with the output header as the first
            element and the header length as the second
            element
        """
        header_length = BYTES_PER_LONG * (len(self.outputs) + 1)
        self._expand_buffer_if_necessary(header_length)
        header_idx = 0
        struct.pack_into("<Q", PredictionResponse.header_buffer, header_idx,
                         self.num_outputs)
        header_idx += BYTES_PER_LONG
        for output in self.outputs:
            struct.pack_into("<Q", PredictionResponse.header_buffer,
                             header_idx, len(output))
            header_idx += BYTES_PER_LONG

        return PredictionResponse.header_buffer[:header_length], header_length


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
    def __init__(self, collect_metrics=True, read_config=True):
        self.collect_metrics = collect_metrics
        if read_config:
            self._read_config_from_environment()

    def _read_config_from_environment(self):
        try:
            self.model_name = os.environ["CLIPPER_MODEL_NAME"]
        except KeyError:
            print(
                "ERROR: CLIPPER_MODEL_NAME environment variable must be set",
                file=sys.stdout)
            sys.exit(1)
        try:
            self.model_version = os.environ["CLIPPER_MODEL_VERSION"]
        except KeyError:
            print(
                "ERROR: CLIPPER_MODEL_VERSION environment variable must be set",
                file=sys.stdout)
            sys.exit(1)

        self.host = "127.0.0.1"
        if "CLIPPER_IP" in os.environ:
            self.host = os.environ["CLIPPER_IP"]
        else:
            print("Connecting to Clipper on localhost")

        self.port = 7000
        if "CLIPPER_PORT" in os.environ:
            self.port = int(os.environ["CLIPPER_PORT"])
        else:
            print("Connecting to Clipper with default port: {port}".format(
                port=self.port))

        self.input_type = "doubles"
        if "CLIPPER_INPUT_TYPE" in os.environ:
            self.input_type = os.environ["CLIPPER_INPUT_TYPE"]
        else:
            print("Using default input type: doubles")

        self.model_path = os.environ["CLIPPER_MODEL_PATH"]

    def get_model_path(self):
        return self.model_path

    def get_input_type(self):
        return self.input_type

    def get_event_history(self):
        if self.server:
            return self.server.get_event_history()
        else:
            print("Cannot retrieve message history for inactive RPC service!")
            raise

    def start(self, model):
        """
        Args:
            model (object): The loaded model object ready to make predictions.
        """

        try:
            ip = socket.gethostbyname(self.host)
        except socket.error as e:
            print("Error resolving %s: %s" % (self.host, e))
            sys.exit(1)
        context = zmq.Context()
        self.server = Server(context, ip, self.port)
        self.server.model_name = self.model_name
        self.server.model_version = self.model_version
        self.server.model_input_type = string_to_input_type(self.input_type)
        self.server.model = model

        # Create a file named model_is_ready.check to show that model and container
        # are ready
        with open("/model_is_ready.check", "w") as f:
            f.write("READY")
        if self.collect_metrics:
            start_metric_server()
            add_metrics()

        self.server.run(collect_metrics=self.collect_metrics)


def add_metrics():
    config_file_path = 'metrics_config.yaml'

    config_file_path = os.path.join(
        os.path.split(os.path.realpath(__file__))[0], config_file_path)

    with open(config_file_path, 'r') as f:
        config = yaml.load(f, Loader=yaml.FullLoader)
    config = config['Model Container']

    prefix = 'clipper_{}_'.format(config.pop('prefix'))

    for name, spec in config.items():
        metric_type = spec.get('type')
        metric_description = spec.get('description')

        name = prefix + name

        if metric_type == 'Histogram' and 'bucket' in spec.keys():
            buckets = spec['bucket'] + [float("inf")]
            metrics.add_metric(name, metric_type, metric_description, buckets)
        else:  # This case include default histogram buckets + all other
            metrics.add_metric(name, metric_type, metric_description)


def start_metric_server():

    DEBUG = False
    cmd = ['python', '-m', 'clipper_admin.metrics.server']
    if DEBUG:
        cmd.append('DEBUG')

    Popen(cmd)

    # sleep is necessary because Popen returns immediately
    time.sleep(5)
