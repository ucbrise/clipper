# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: model.proto

import sys
_b=sys.version_info[0]<3 and (lambda x:x) or (lambda x:x.encode('latin1'))
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from google.protobuf import reflection as _reflection
from google.protobuf import symbol_database as _symbol_database
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()




DESCRIPTOR = _descriptor.FileDescriptor(
  name='model.proto',
  package='modeltest',
  syntax='proto3',
  serialized_options=None,
  serialized_pb=_b('\n\x0bmodel.proto\x12\tmodeltest\"\x11\n\x02hi\x12\x0b\n\x03msg\x18\x01 \x01(\t\"/\n\x05input\x12\x11\n\tinputType\x18\x01 \x01(\t\x12\x13\n\x0binputStream\x18\x02 \x01(\t\"1\n\tproxyinfo\x12\x11\n\tproxyName\x18\x01 \x01(\t\x12\x11\n\tproxyPort\x18\x02 \x01(\t\"\x1a\n\x08response\x12\x0e\n\x06status\x18\x01 \x01(\t2\xab\x01\n\x0ePredictService\x12\x32\n\x07Predict\x12\x10.modeltest.input\x1a\x13.modeltest.response\"\x00\x12\x37\n\x08SetProxy\x12\x14.modeltest.proxyinfo\x1a\x13.modeltest.response\"\x00\x12,\n\x04Ping\x12\r.modeltest.hi\x1a\x13.modeltest.response\"\x00\x62\x06proto3')
)




_HI = _descriptor.Descriptor(
  name='hi',
  full_name='modeltest.hi',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='msg', full_name='modeltest.hi.msg', index=0,
      number=1, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=26,
  serialized_end=43,
)


_INPUT = _descriptor.Descriptor(
  name='input',
  full_name='modeltest.input',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='inputType', full_name='modeltest.input.inputType', index=0,
      number=1, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='inputStream', full_name='modeltest.input.inputStream', index=1,
      number=2, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=45,
  serialized_end=92,
)


_PROXYINFO = _descriptor.Descriptor(
  name='proxyinfo',
  full_name='modeltest.proxyinfo',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='proxyName', full_name='modeltest.proxyinfo.proxyName', index=0,
      number=1, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
    _descriptor.FieldDescriptor(
      name='proxyPort', full_name='modeltest.proxyinfo.proxyPort', index=1,
      number=2, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=94,
  serialized_end=143,
)


_RESPONSE = _descriptor.Descriptor(
  name='response',
  full_name='modeltest.response',
  filename=None,
  file=DESCRIPTOR,
  containing_type=None,
  fields=[
    _descriptor.FieldDescriptor(
      name='status', full_name='modeltest.response.status', index=0,
      number=1, type=9, cpp_type=9, label=1,
      has_default_value=False, default_value=_b("").decode('utf-8'),
      message_type=None, enum_type=None, containing_type=None,
      is_extension=False, extension_scope=None,
      serialized_options=None, file=DESCRIPTOR),
  ],
  extensions=[
  ],
  nested_types=[],
  enum_types=[
  ],
  serialized_options=None,
  is_extendable=False,
  syntax='proto3',
  extension_ranges=[],
  oneofs=[
  ],
  serialized_start=145,
  serialized_end=171,
)

DESCRIPTOR.message_types_by_name['hi'] = _HI
DESCRIPTOR.message_types_by_name['input'] = _INPUT
DESCRIPTOR.message_types_by_name['proxyinfo'] = _PROXYINFO
DESCRIPTOR.message_types_by_name['response'] = _RESPONSE
_sym_db.RegisterFileDescriptor(DESCRIPTOR)

hi = _reflection.GeneratedProtocolMessageType('hi', (_message.Message,), dict(
  DESCRIPTOR = _HI,
  __module__ = 'model_pb2'
  # @@protoc_insertion_point(class_scope:modeltest.hi)
  ))
_sym_db.RegisterMessage(hi)

input = _reflection.GeneratedProtocolMessageType('input', (_message.Message,), dict(
  DESCRIPTOR = _INPUT,
  __module__ = 'model_pb2'
  # @@protoc_insertion_point(class_scope:modeltest.input)
  ))
_sym_db.RegisterMessage(input)

proxyinfo = _reflection.GeneratedProtocolMessageType('proxyinfo', (_message.Message,), dict(
  DESCRIPTOR = _PROXYINFO,
  __module__ = 'model_pb2'
  # @@protoc_insertion_point(class_scope:modeltest.proxyinfo)
  ))
_sym_db.RegisterMessage(proxyinfo)

response = _reflection.GeneratedProtocolMessageType('response', (_message.Message,), dict(
  DESCRIPTOR = _RESPONSE,
  __module__ = 'model_pb2'
  # @@protoc_insertion_point(class_scope:modeltest.response)
  ))
_sym_db.RegisterMessage(response)



_PREDICTSERVICE = _descriptor.ServiceDescriptor(
  name='PredictService',
  full_name='modeltest.PredictService',
  file=DESCRIPTOR,
  index=0,
  serialized_options=None,
  serialized_start=174,
  serialized_end=345,
  methods=[
  _descriptor.MethodDescriptor(
    name='Predict',
    full_name='modeltest.PredictService.Predict',
    index=0,
    containing_service=None,
    input_type=_INPUT,
    output_type=_RESPONSE,
    serialized_options=None,
  ),
  _descriptor.MethodDescriptor(
    name='SetProxy',
    full_name='modeltest.PredictService.SetProxy',
    index=1,
    containing_service=None,
    input_type=_PROXYINFO,
    output_type=_RESPONSE,
    serialized_options=None,
  ),
  _descriptor.MethodDescriptor(
    name='Ping',
    full_name='modeltest.PredictService.Ping',
    index=2,
    containing_service=None,
    input_type=_HI,
    output_type=_RESPONSE,
    serialized_options=None,
  ),
])
_sym_db.RegisterServiceDescriptor(_PREDICTSERVICE)

DESCRIPTOR.services_by_name['PredictService'] = _PREDICTSERVICE

# @@protoc_insertion_point(module_scope)
