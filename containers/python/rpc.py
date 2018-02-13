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
from collections import deque
from multiprocessing import Pipe, Process
from prometheus_client import start_http_server
from prometheus_client.core import Counter, Gauge, Histogram, Summary

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

    def validate_rpc_version(self, received_version):
        if received_version != RPC_VERSION:
            raise Exception(
                "Received an RPC message with version: {clv} that does not match container version: {mcv}"
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

    def run(self, metric_conn):
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
                        input_header_size = socket.recv()
                        input_header = socket.recv()

                        t2 = datetime.now()

                        parsed_input_header = np.frombuffer(
                            input_header, dtype=np.uint64)

                        input_type, num_inputs, input_sizes = parsed_input_header[
                            0], parsed_input_header[1], parsed_input_header[2:]

                        inputs = []
                        for _ in range(num_inputs):
                            input_item = socket.recv()
                            if input_type == INPUT_TYPE_STRINGS:
                                input_item = str(input_item)
                            else:
                                input_item = np.frombuffer(
                                    input_item,
                                    dtype=input_type_to_dtype(input_type))
                            inputs.append(input_item)

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

                        model_container_metric = {}
                        model_container_metric['pred_total'] = 1
                        model_container_metric[
                            'recv_time_ms'] = recv_time / 1000.0
                        model_container_metric[
                            'parse_time_ms'] = parse_time / 1000.0
                        model_container_metric[
                            'handle_time_ms'] = handle_time / 1000.0
                        model_container_metric['end_to_end_latency_ms'] = (
                            recv_time + parse_time + handle_time) / 1000.0
                        metric_conn.send(model_container_metric)

                        print("recv: %f us, parse: %f us, handle: %f us" %
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

    def send_container_metadata(self, socket):
        socket.send("", zmq.SNDMORE)
        socket.send(struct.pack("<I", MESSAGE_TYPE_NEW_CONTAINER), zmq.SNDMORE)
        socket.send_string(self.model_name, zmq.SNDMORE)
        socket.send_string(str(self.model_version), zmq.SNDMORE)
        socket.send_string(str(self.model_input_type))
        self.event_history.insert(EVENT_HISTORY_SENT_CONTAINER_METADATA)
        print("Sent container metadata!")
        sys.stdout.flush()
        sys.stderr.flush()

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


class PredictionResponse:
    header_buffer = bytearray(1024)

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
        output = unicode(output, "utf-8").encode("utf-8")
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
        socket.send("", flags=zmq.SNDMORE)
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

        child_conn, parent_conn = Pipe(duplex=False)
        metrics_proc = Process(target=run_metric, args=(child_conn, ))
        metrics_proc.start()
        self.server.run(parent_conn)


class MetricCollector:
    """
    Note this is no longer a Prometheus Collector.
    Instead, this is simply a class to encapsulate metric recording.
    """

    def __init__(self, pipe_child_conn):
        self.pipe_conn = pipe_child_conn
        self.metrics = {}
        self.name_to_type = {}

        self._load_config()

    def _load_config(self):
        config_file_path = 'metrics_config.yaml'

        # Make sure we are inside /container, where the config file lives.
        cwd = os.path.split(os.getcwd())[1]
        if cwd != 'container':
            config_file_path = os.path.join(os.getcwd(), 'container',
                                            config_file_path)

        with open(config_file_path, 'r') as f:
            config = yaml.load(f)
        config = config['Model Container']

        prefix = 'clipper_{}_'.format(config.pop('prefix'))

        for name, spec in config.items():
            metric_type = spec.get('type')
            metric_description = spec.get('description')

            if not metric_type and not metric_description:
                raise Exception(
                    "{}: Metric Type and Metric Description are Required in Config File.".
                    format(name))

            if metric_type == 'Counter':
                self.metrics[name] = Counter(prefix + name, metric_description)
            elif metric_type == 'Gauge':
                self.metrics[name] = Gauge(prefix + name, metric_description)
            elif metric_type == 'Histogram':
                if 'bucket' in spec.keys():
                    buckets = spec['bucket'] + [float("inf")]
                    self.metrics[name] = Histogram(
                        prefix + name, metric_description, buckets=buckets)
                else:
                    self.metrics[name] = Histogram(prefix + name,
                                                   metric_description)
            elif metric_type == 'Summary':
                self.metrics[name] = Summary(prefix + name, metric_description)
            else:
                raise Exception(
                    "Unknown Metric Type: {}. See config file.".format(
                        metric_type))

            self.name_to_type[name] = metric_type

    def collect(self):
        while True:
            latest_metric_dict = self.pipe_conn.recv(
            )  # This call is blocking.
            for name, value in latest_metric_dict.items():
                metric = self.metrics[name]
                if self.name_to_type[name] == 'Counter':
                    metric.inc(value)
                elif self.name_to_type[name] == 'Gauge':
                    metric.set(value)
                elif self.name_to_type[name] == 'Histogram' or self.name_to_type[name] == 'Summary':
                    metric.observe(value)
                else:
                    raise Exception(
                        "Unknown Metric Type for {}. See config file.".format(
                            name))


def run_metric(child_conn):
    """
    This function takes a child_conn at the end of the pipe and
    receive object to update prometheus metric.

    It is recommended to be ran in a separate process.
    """
    collector = MetricCollector(child_conn)
    start_http_server(1390)
    collector.collect()
