from __future__ import absolute_import, print_function
import os
import sys
import requests
import json
import numpy as np
import time
import logging

cur_dir = os.path.dirname(os.path.abspath(__file__))

sys.path.insert(0, os.path.abspath('%s/util_direct_import/' % cur_dir))

from util_package import mock_module_in_package as mmip
import mock_module as mm

import torch
from torch.utils.data import DataLoader
import torch.utils.data as data
from PIL import Image
from torch import nn, optim
from torch.autograd import Variable
from torchvision import transforms

from test_utils import (create_docker_connection, BenchmarkException, headers,
                        log_clipper_state)
cur_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.abspath("%s/../clipper_admin" % cur_dir))

from clipper_admin.deployers.pytorch import deploy_pytorch_model, create_endpoint

logging.basicConfig(
    format='%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
    datefmt='%y-%m-%d:%H:%M:%S',
    level=logging.INFO)

logger = logging.getLogger(__name__)

app_name = "pytorch-test"
model_name = "pytorch-model"


def normalize(x):
    return x.astype(np.double) / 255.0


def objective(y, pos_label):
    # prediction objective
    if y == pos_label:
        return 1
    else:
        return 0


def parseData(train_path, pos_label):
    trainData = np.genfromtxt(train_path, delimiter=',', dtype=int)
    records = trainData[:, 1:]
    labels = trainData[:, :1]
    transformedlabels = [objective(ele, pos_label) for ele in labels]
    return (records, transformedlabels)


def predict(model, xs):
    preds = model(xs)
    return [str(p) for p in preds]

def deploy_and_test_model(clipper_conn,
                          model,
                          version,
                          link_model=False,
                          predict_fn=predict):
    deploy_pytorch_model(clipper_conn, model_name, version, "integers",
                         predict_fn, model)

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

# Define Logistic Regression model
class Logstic_Regression(nn.Module):
    def __init__(self, in_dim, n_class):
        super(Logstic_Regression, self).__init__()
        self.logstic = nn.Linear(in_dim, n_class)

    def forward(self, x):
        out = self.logstic(x)
        return out


def train_logistic_regression(model,train_loader):
    model = model
    use_gpu = torch.cuda.is_available()
    if use_gpu:
        model = model.cuda()
    criterion = nn.CrossEntropyLoss()
    optimizer = optim.SGD(model.parameters(), lr=0.001)
    for epoch in range(1000):
        print('*' * 10)
        print('epoch {}'.format(epoch + 1))
        since = time.time()
        running_loss = 0.0
        for i, data in enumerate(train_loader, 1):
            img, label = data
            if use_gpu:
                img = Variable(img).cuda()
                label = Variable(label).cuda()
            else:
                img = Variable(img)
                label = Variable(label)

            #Forward propagation
            out = model(img)
            loss = criterion(out, label)
            running_loss += loss.data[0] * label.size(0)
            _, pred = torch.max(out, 1)

            #Back propagation
            optimizer.zero_grad()
            loss.backward()
            optimizer.step()

        model.eval()

    return model


def get_test_point():
    return [np.random.randint(255) for _ in range(784)]

#Define a dataloader to read our data
class myDatafolder(data.Dataset):
    def __init__(self, data, label):
        self.imgs = data
        self.classes = label

    def __getitem__(self, index):
        img = self.imgs[index]
        label = self.classes[index]
        img = torch.Tensor(img)
        return img, torch.Tensor(label)

    def __len__(self):
        return len(self.imgs)

    def getName(self):
        return self.classes


if __name__ == "__main__":
    pos_label = 3
    try:
        clipper_conn = create_docker_connection(
            cleanup=True, start_clipper=True)

        train_path = os.path.join(cur_dir, "data/train.data")
        (x, y) = parseData(train_path, pos_label)
        train_loader = myDatafolder(x,y)

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

            model = Logstic_Regression(28*28,2)
            lr_model = train_logistic_regression(model,train_loader)

            deploy_and_test_model(
                clipper_conn,
                lr_model,
                version,
                link_model=True)

            app_and_model_name = "easy-register-app-model"
            create_endpoint(clipper_conn, app_and_model_name, "integers",
                            predict, lr_model)
            test_model(clipper_conn, app_and_model_name, 1)

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
