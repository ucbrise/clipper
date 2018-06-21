from __future__ import absolute_import, print_function
import os
import sys
import requests
import json
import numpy as np
import time
import logging

cur_dir = os.path.dirname(os.path.abspath(__file__))

import torch
import torch.utils.data as data
from torch import nn, optim
from torch.autograd import Variable
import torch.nn.functional as F

from test_utils import (create_docker_connection, BenchmarkException, headers,
                        log_clipper_state)
cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath("%s/../clipper_admin" % cur_dir))

from clipper_admin.deployers.onnx import deploy_pytorch_model, create_pytorch_endpoint

logging.basicConfig(
    format='%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
    datefmt='%y-%m-%d:%H:%M:%S',
    level=logging.INFO)

logger = logging.getLogger(__name__)

app_name = "caffe2-test"
model_name = "caffe2-model"


def normalize(x):
    return x.astype(np.double) / 255.0


def objective(y, pos_label):
    # prediction objective
    if y == pos_label:
        return 1
    else:
        return 0


def parsedata(train_path, pos_label):
    trainData = np.genfromtxt(train_path, delimiter=',', dtype=int)
    records = trainData[:, 1:]
    labels = trainData[:, :1]
    transformedlabels = [objective(ele, pos_label) for ele in labels]
    return (records, transformedlabels)


def predict(model, inputs):
    preds = model.run(np.array(inputs).astype(np.float32))
    return [str(p) for p in preds[0]]


def deploy_and_test_model(clipper_conn,
                          model,
                          inputs,
                          version,
                          link_model=False,
                          predict_fn=predict):
    deploy_pytorch_model(
        clipper_conn,
        model_name,
        version,
        "integers",
        inputs,
        predict_fn,
        model,
        onnx_backend="caffe2")

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
            logger.error(result)
            raise BenchmarkException(response.text)

    if num_defaults > 0:
        logger.error("Error: %d/%d predictions were default" % (num_defaults,
                                                                num_preds))
    if num_defaults > num_preds / 2:
        raise BenchmarkException("Error querying APP %s, MODEL %s:%d" %
                                 (app, model_name, version))


# Define a simple NN model
class BasicNN(nn.Module):
    def __init__(self):
        super(BasicNN, self).__init__()
        self.net = nn.Linear(28 * 28, 2)

    def forward(self, x):
        if isinstance(x, np.ndarray):
            x = torch.from_numpy(x)
        x = x.float()
        if isinstance(x, type(torch.randn(1))):
            x = Variable(x)
        x = x.view(1, 1, 28, 28)
        x = x / 255.0
        batch_size = x.size(0)
        x = x.view(batch_size, -1)
        output = self.net(x.float())
        return F.softmax(output)


def train(model):
    model.train()
    optimizer = optim.SGD(model.parameters(), lr=0.001)
    for epoch in range(10):
        for i, d in enumerate(train_loader, 1):
            image, j = d
            optimizer.zero_grad()
            output = model(image)
            loss = F.cross_entropy(output,
                                   Variable(
                                       torch.LongTensor([train_y[i - 1]])))
            loss.backward()
            optimizer.step()
    return model


def get_test_point():
    return [np.random.randint(255) for _ in range(784)]


# Define a dataloader to read data
class TrainingDataset(data.Dataset):
    def __init__(self, data, label):
        self.imgs = data
        self.classes = label

    def __getitem__(self, index):
        img = self.imgs[index]
        label = self.classes[index]
        img = torch.Tensor(img)
        return img, torch.Tensor(label)


if __name__ == "__main__":
    pos_label = 3

    import random
    cluster_name = "onnx-{}".format(random.randint(0, 5000))
    try:
        clipper_conn = create_docker_connection(
            cleanup=False, start_clipper=True, new_name=cluster_name)

        train_path = os.path.join(cur_dir, "data/train.data")
        train_x, train_y = parsedata(train_path, pos_label)
        train_x = normalize(train_x)
        train_loader = TrainingDataset(train_x, train_y)

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
                logger.error("Error: %s" % response.text)
                raise BenchmarkException("Error creating app %s" % app_name)

            version = 1

            model = BasicNN()
            nn_model = train(model)

            inputs = Variable(torch.randn(len(get_test_point())))
            deploy_and_test_model(
                clipper_conn, nn_model, inputs, version, link_model=True)

            app_and_model_name = "easy-register-app-model"
            create_pytorch_endpoint(
                clipper_conn,
                app_and_model_name,
                "integers",
                inputs,
                predict,
                nn_model,
                onnx_backend="caffe2")
            test_model(clipper_conn, app_and_model_name, 1)

        except BenchmarkException:
            sys.exit(1)
            log_clipper_state(clipper_conn)
            logger.exception("BenchmarkException")
            clipper_conn = create_docker_connection(
                cleanup=True, start_clipper=False, cleanup_name=cluster_name)
            sys.exit(1)
        else:
            clipper_conn = create_docker_connection(
                cleanup=True, start_clipper=False, cleanup_name=cluster_name)
    except Exception:
        logger.exception("Exception")
        clipper_conn = create_docker_connection(
            cleanup=True, start_clipper=False, cleanup_name=cluster_name)
        sys.exit(1)
