from __future__ import absolute_import, print_function
import os
import sys
import requests
import json
import numpy as np
import time
import logging
import xgboost as xgb
import pickle

cur_dir = os.path.dirname(os.path.abspath(__file__))

sys.path.insert(0, os.path.abspath('%s/util_direct_import/' % cur_dir))
from util_package import mock_module_in_package as mmip

from test_utils import (create_docker_connection, BenchmarkException, headers,
                        log_clipper_state)
cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath("%s/../clipper_admin" % cur_dir))
from clipper_admin.deployers.xgboost import deploy_xgboost_model, create_endpoint

from clipper_admin.deployers.deployer_utils import save_python_function

logging.basicConfig(
    format='%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
    datefmt='%y-%m-%d:%H:%M:%S',
    level=logging.INFO)

logger = logging.getLogger(__name__)

app_name = "xgboost-test"
model_name = "xgboost-model"


def predict(model, xs):
    return [str(model.predict(xgb.DMatrix(xs)))]

def deploy_and_test_model(clipper_conn,
                          model,
                          version,
                          link_model=False,
                          predict_fn=predict):
    serialization_dir = save_python_function('xgboost_model', predict_fn)
    xgboost_model_save_loc = os.path.join(serialization_dir,
        "xgboost_model_data.pickle.dat")
    try:
        pickle.dump(model, open(xgboost_model_save_loc, "wb"))
    except Exception as e:
        logger.warn("Error saving xgboost model: %s" % e)
        raise e
    base_image = None
    clipper_conn.build_and_deploy_model('xgboost_model', version, "integers",
                        serialization_dir, base_image, pkgs_to_install=['xgboost'])

    time.sleep(5)

    if link_model:
        clipper_conn.link_model_to_app(app_name, model_name)
        time.sleep(5)

    test_model(clipper_conn, app_name, version)


def test_model(clipper_conn, app, version):
    time.sleep(25)
    num_preds = 25
    num_defaults = 0
    addr = clipper_conn.get_query_addr()
    for i in range(num_preds):
        response = requests.post(
            "http://%s/%s/predict" % (addr, app),
            headers=headers,
            data=json.dumps({
                'input': get_test_point()
            }))
        result = response.json()
        if response.status_code == requests.codes.ok and result["default"]:
            num_defaults += 1
        elif response.status_code != requests.codes.ok:
            print(result)
            raise BenchmarkException(response.text)

    if num_defaults > 0:
        print("Error: %d/%d predictions were default" % (num_defaults,
                                                         num_preds))
    if num_defaults > num_preds / 2:
        raise BenchmarkException("Error querying APP %s, MODEL %s:%d" %
                                 (app, model_name, version))

def get_test_point():
    return [np.random.randint(255) for _ in range(784)]

if __name__ == "__main__":
    pos_label = 3
    try:
        clipper_conn = create_docker_connection(
            cleanup=True, start_clipper=True)

        train_path = os.path.join(cur_dir, "data/agaricus.txt.train")
        test_path = os.path.join(cur_dir, "data/agaricus.txt.test")
        try:
            clipper_conn.register_application(app_name, "integers",
                                              "default_pred", 100000)
            time.sleep(1)

            addr = clipper_conn.get_query_addr()
            response = requests.post(
                "http://%s/%s/predict" % (addr, app_name),
                headers=headers,
                data=json.dumps({
                    'input': get_test_point()
                }))
            result = response.json()
            if response.status_code != requests.codes.ok:
                print("Error: %s" % response.text)
                raise BenchmarkException("Error creating app %s" % app_name)

            version = 1
            dtrain = xgb.DMatrix(get_test_point(), label=[0])
            param = {'max_depth': 2, 'eta': 1, 'silent': 1, 'objective': 'binary:logistic'}
            watchlist = [(dtrain, 'train')]
            num_round = 2
            bst = xgb.train(param, dtrain, num_round, watchlist)
            deploy_and_test_model(
                clipper_conn,
                bst,
                version,
                link_model=True,
                predict_fn=predict)
        except BenchmarkException as e:
            log_clipper_state(clipper_conn)
            logger.exception("BenchmarkException")
            clipper_conn = create_docker_connection(
                cleanup=True, start_clipper=False)
            sys.exit(1)
        else:
            clipper_conn = create_docker_connection(
                cleanup=True, start_clipper=False)
    except Exception as e:
        logger.exception("Exception")
        clipper_conn = create_docker_connection(
            cleanup=True, start_clipper=False)
        sys.exit(1)
