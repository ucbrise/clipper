from jsonschema import validate
from enum import Enum


class Prom_Type(str, Enum):
    """
    Enum is here to prevent mispelled Prometheus data types. 
    """
    g = 'Gauge'
    c = 'Counter'
    h = 'Histogram'
    s = 'Summary'

versions = ['0.3.0']

add_schema = {
    'properties': {
        'description': {
            'type': 'string'
        },
        'name': {
            'type': 'string'
        },
        'type': {
            'enum': list(Prom_Type),
            'type': 'string'
        },
        'buckets': {
            "type": "array",
            "items": {
                "type": "number"
            }
        }
    },
    'type': 'object',
    'required': ['name', 'description', 'type']
}

report_schema = {
    'properties': {
        'data': {
            'type': 'number'
        },
        'name': {
            'type': 'string'
        },
    },
    'type': 'object',
    'required': ['data', 'name']
}

schema = {
    'properties': {
        'endpoint': {
            'enum': ['add', 'report'],
            'type': 'string'
        },
        'version': {
            'type': 'string',
            'enum': versions
        },
        'data': {
            'oneOf': [add_schema, report_schema]
        }
    },
    'required': ['endpoint', 'version', 'data'],
    'type': 'object'
}


def validate_schema(messege_dict):
    inner_schema = {'add': add_schema, 'report': report_schema}

    validate(messege_dict, schema)
    endpoint = messege_dict['endpoint']

    # This line will error if validate fails
    validate(messege_dict['data'], inner_schema[endpoint])

    return True
