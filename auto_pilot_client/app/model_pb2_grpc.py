# Generated by the gRPC Python protocol compiler plugin. DO NOT EDIT!
import grpc

import model_pb2 as model__pb2


class PredictServiceStub(object):
  """service, encode a plain text 
  """

  def __init__(self, channel):
    """Constructor.

    Args:
      channel: A grpc.Channel.
    """
    self.Predict = channel.unary_unary(
        '/modeltest.PredictService/Predict',
        request_serializer=model__pb2.input.SerializeToString,
        response_deserializer=model__pb2.output.FromString,
        )
    self.SetProxy = channel.unary_unary(
        '/modeltest.PredictService/SetProxy',
        request_serializer=model__pb2.proxyinfo.SerializeToString,
        response_deserializer=model__pb2.response.FromString,
        )
    self.Ping = channel.unary_unary(
        '/modeltest.PredictService/Ping',
        request_serializer=model__pb2.hi.SerializeToString,
        response_deserializer=model__pb2.response.FromString,
        )


class PredictServiceServicer(object):
  """service, encode a plain text 
  """

  def Predict(self, request, context):
    """request a service of encode
    """
    context.set_code(grpc.StatusCode.UNIMPLEMENTED)
    context.set_details('Method not implemented!')
    raise NotImplementedError('Method not implemented!')

  def SetProxy(self, request, context):
    # missing associated documentation comment in .proto file
    pass
    context.set_code(grpc.StatusCode.UNIMPLEMENTED)
    context.set_details('Method not implemented!')
    raise NotImplementedError('Method not implemented!')

  def Ping(self, request, context):
    # missing associated documentation comment in .proto file
    pass
    context.set_code(grpc.StatusCode.UNIMPLEMENTED)
    context.set_details('Method not implemented!')
    raise NotImplementedError('Method not implemented!')


def add_PredictServiceServicer_to_server(servicer, server):
  rpc_method_handlers = {
      'Predict': grpc.unary_unary_rpc_method_handler(
          servicer.Predict,
          request_deserializer=model__pb2.input.FromString,
          response_serializer=model__pb2.output.SerializeToString,
      ),
      'SetProxy': grpc.unary_unary_rpc_method_handler(
          servicer.SetProxy,
          request_deserializer=model__pb2.proxyinfo.FromString,
          response_serializer=model__pb2.response.SerializeToString,
      ),
      'Ping': grpc.unary_unary_rpc_method_handler(
          servicer.Ping,
          request_deserializer=model__pb2.hi.FromString,
          response_serializer=model__pb2.response.SerializeToString,
      ),
  }
  generic_handler = grpc.method_handlers_generic_handler(
      'modeltest.PredictService', rpc_method_handlers)
  server.add_generic_rpc_handlers((generic_handler,))
