import json
import os
import requests
from datetime import datetime
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt

DEMO_UID = 0

classes = ['airplane', 'automobile', 'bird', 'cat',
           'deer', 'dog', 'frog', 'horse', 'ship', 'truck']
positive_class = classes.index('airplane')
negative_class = classes.index('bird')


def recover_pixels(x):
    return np.transpose(x.reshape(3, 32, 32), (1, 2, 0))


def show_example_images(images, labels, num_rows):
    imgs_per_row = 6
    num_images = imgs_per_row * num_rows
    idxs = np.random.randint(0, len(labels), num_images)
    f, axes = plt.subplots(nrows=num_rows,
                           ncols=imgs_per_row,
                           figsize=(1.5*imgs_per_row, 1.5*num_rows))

    f.tight_layout()
    for i, idx in enumerate(idxs):
        image = recover_pixels(images[idx])
        label = labels[idx]
        cur_ax = axes[i / imgs_per_row][i % imgs_per_row]
        cur_ax.imshow(image.astype(np.ubyte), interpolation="nearest")
        cur_ax.axis('off')
        if label == -1:
            title = classes[negative_class]
        else:
            title = classes[positive_class]
        cur_ax.set_title(title)


def load_cifar(cifar_location, cifar_filename="cifar_train.data", norm=True):
    cifar_path = os.path.join(cifar_location, cifar_filename)
    # print("Source file: %s" % cifar_path)
    df = pd.read_csv(cifar_path, sep=",", header=None)
    data = df.values
    print("Number of image files: %d" % len(data))
    y = data[:, 0]
    X = data[:, 1:]
    Z = X
    if norm:
        mu = np.mean(X.T, 0)
        sigma = np.var(X.T, 0)
        Z = (X.T - mu) / np.array([np.sqrt(z) if z > 0 else 1. for z in sigma])
        Z = Z.T
    return Z, y


def filter_data(X, y):
    X_train, y_train = [], []
    for (example, label) in zip(X, y):
        if label == positive_class:
            X_train.append(example)
            y_train.append(1.0)
        elif label == negative_class:
            X_train.append(example)
            y_train.append(-1.0)
    X_train = np.array(X_train)
    y_train = np.array(y_train)
    return X_train, y_train


def cifar_update(host, app, uid, x, y, print_result=False):
    url = "http://%s:1337/%s/update" % (host, app)
    req_json = json.dumps({
        'uid': uid,
        'input': list(x),
        'label': float(y),
        # These updates aren't coming from predictions made by a particular
        # model, so we can ignore the model name and version fields.
        'model_name': 'NA',
        'model_version': 1
        })
    headers = {'Content-type': 'application/json'}
    start = datetime.now()
    r = requests.post(url, headers=headers, data=req_json)
    end = datetime.now()
    latency = (end - start).total_seconds() * 1000.0
    if print_result:
        print("'%s', %f ms" % (r.text, latency))


def parse_pred(p):
    splits = p.split(",")
    qid = int(splits[0].strip().split(":")[1])
    pred = float(splits[1].strip().split(":")[1])
    return qid, pred


def cifar_prediction(host, app, uid, x):
    url = "http://%s:1337/%s/predict" % (host, app)
    req_json = json.dumps({'uid': uid, 'input': list(x)})
    headers = {'Content-type': 'application/json'}
    start = datetime.now()
    r = requests.post(url, headers=headers, data=req_json)
    end = datetime.now()
    latency = (end - start).total_seconds() * 1000.0
    qid, pred = parse_pred(r.text)
    # TODO TODO TODO: remove check for 0 once selection policy is updated
    if pred == 0.0:
        # print("Warning: Clipper predicted 0.0")
        pred = -1.0
    assert pred == 1.0 or pred == -1.0
    return (pred, latency)


def run_iteration(host, app, uid, test_x, test_y):
    correct = 0
    false_pos = 0
    false_neg = 0
    latencies = []
    true_pos = 0
    true_neg = 0
    total = 100
    for i in range(total):
        example_num = np.random.randint(0, len(test_y))
        correct_y = float(test_y[example_num])
        pred_y, latency = cifar_prediction(host, app, uid, test_x[example_num])
        if correct_y == pred_y:
            if correct_y == -1:
                true_neg += 1
            elif correct_y == 1:
                true_pos += 1
            correct += 1
        elif correct_y == -1 and pred_y == 1:
            false_pos += 1
        elif correct_y == 1 and pred_y == -1:
            false_neg += 1
        else:
            print "predicted: {p}, correct: {c}".format(p=pred_y, c=correct_y)
        latencies.append(latency)
    total = float(total)
    return (float(correct)/total,
            float(false_pos)/total,
            float(false_neg)/total,
            float(true_pos)/total,
            float(true_neg)/total,
            np.mean(latencies))


def run_serving_workload(host, app, test_x, test_y):
    fig, (ax_acc) = plt.subplots(1, 1, sharex=True)
    ax_acc.set_ylabel("application accuracy")
    ax_acc.set_xlabel("iterations")
    # DON'T SHOW LATENCY
    ax_acc.set_ylim(0, 1.0)
    ax_acc.set_title("Superman Detection Accuracy Over Time")
    xs = []
    accs = []
    lats = []
    j = 0
    uid = DEMO_UID
    while True:
        correct, fp, fn, tp, tn, mean_lat, = run_iteration(host, app, uid,
                                                           test_x, test_y)
        xs.append(j)
        accs.append(correct)
        lats.append(mean_lat)
        j += 1
        ax_acc.set_xlim(0, j + 1)

        ax_acc.plot(xs, accs, 'b')
        # fig.set_size_inches(6, 3)
        fig.tight_layout()
        fig.canvas.draw()
        # print(("Accuracy: {cor}, false positives: {fp}, "
        #       "false negatives: {fn}, true positives: {tp}, "
        #        "true negatives: {tn}").format(
        #     cor=correct, fp=fp, fn=fn, tp=tp, tn=tn))
        # print("Mean latency: {lat} ms".format(lat=mean_lat))


def run_serving_workload_show_latency(host, app, test_x, test_y):
    fig, (ax_acc, ax_lat) = plt.subplots(2, 1, sharex=True)
    ax_acc.set_ylabel("accuracy")
    ax_lat.set_xlabel("time")
    # DON'T SHOW LATENCY
    ax_lat.set_ylabel("latency")
    ax_acc.set_ylim(0, 1.0)
    xs = []
    accs = []
    lats = []
    j = 0
    uid = DEMO_UID
    while True:
        correct, fp, fn, tp, tn, mean_lat, = run_iteration(host, app, uid,
                                                           test_x, test_y)
        xs.append(j)
        accs.append(correct)
        lats.append(mean_lat)
        j += 1
        ax_acc.set_xlim(0, j + 1)
        ax_lat.set_xlim(0, j + 1)

        ax_acc.plot(xs, accs, 'b')
        ax_lat.plot(xs, lats, 'r')
        ax_lat.set_ylim(0, 300)
        fig.canvas.draw()
        print(("Accuracy: {cor}, false positives: {fp}, "
              "false negatives: {fn}, true positives: {tp}, "
               "true negatives: {tn}").format(
            cor=correct, fp=fp, fn=fn, tp=tp, tn=tn))
        print("Mean latency: {lat} ms".format(lat=mean_lat))


def enable_feedback(host, app, test_x, test_y, num_updates):
    uid = DEMO_UID
    for i in range(num_updates):
        example_num = np.random.randint(0, len(test_y))
        cifar_update(host, app, uid, test_x[example_num],
                     float(test_y[example_num]))
