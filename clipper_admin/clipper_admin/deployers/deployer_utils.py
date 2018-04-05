from __future__ import print_function, with_statement, absolute_import

import logging
from cloudpickle import CloudPickler
from ..clipper_admin import CLIPPER_TEMP_DIR
import six
import os
import tempfile

cur_dir = os.path.dirname(os.path.abspath(__file__))

logger = logging.getLogger(__name__)


def serialize_object(obj):
    s = six.StringIO()
    c = CloudPickler(s, 2)
    c.dump(obj)
    return s.getvalue()


def save_python_function(name, func):
    predict_fname = "func.pkl"

    # Serialize function
    s = six.StringIO()
    c = CloudPickler(s, 2)
    c.dump(func)
    serialized_prediction_function = s.getvalue()

    # Set up serialization directory
    if not os.path.exists(CLIPPER_TEMP_DIR):
        os.makedirs(CLIPPER_TEMP_DIR)
    serialization_dir = tempfile.mkdtemp(dir=CLIPPER_TEMP_DIR)
    logger.info("Saving function to {}".format(serialization_dir))

    # Write out function serialization
    func_file_path = os.path.join(serialization_dir, predict_fname)
    with open(func_file_path, "w") as serialized_function_file:
        serialized_function_file.write(serialized_prediction_function)
    logging.info("Serialized and supplied predict function")
    return serialization_dir
