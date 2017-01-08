import numpy as np
import os
import pandas as pd
import tensorflow as tf

BATCH_SIZE = 128
NUM_CLASSES = 10

sess = tf.InteractiveSession()

# Preprocess data
def load_cifar(cifar_location, cifar_filename = "train.data", norm=False):
    cifar_path = cifar_location + "/" + cifar_filename
    print("Source file: %s" % cifar_path)
    df = pd.read_csv(cifar_path, sep=",", header=None)
    data = df.values
    print("Number of image files: %d" % len(data))
    y = data[:,0]
    X = data[:,1:]
    Z = X
    if norm:
        mu = np.mean(X.T,0)
        sigma = np.var(X.T,0)
        Z = (X.T - mu) / np.array([np.sqrt(z) if z > 0 else 1. for z in sigma])
        Z = Z.T
    return (Z, y)

# https://indico.io/blog/tensorflow-data-inputs-part1-placeholders-protobufs-queues/
def data_iterator(X, y):
    while True:
        idx = np.arange(0, len(X))
        np.random.shuffle(idx)
        shuffled_X = X[idx]
        shuffled_y = y[idx]
        for batch_index in range(0, (len(X) / BATCH_SIZE) * BATCH_SIZE,
                                 BATCH_SIZE):
            images_batch = shuffled_X[batch_index:batch_index + BATCH_SIZE]
            images_batch = images_batch.astype("float32")
            labels_batch = shuffled_y[batch_index:batch_index + BATCH_SIZE]
            yield images_batch, labels_batch

# Useful functions
def weight_variable(shape, stddev=0.1):
    initial = tf.truncated_normal(shape, stddev=stddev)
    return tf.Variable(initial)

def bias_variable(shape, init_val=0.1):
    initial = tf.constant(init_val, shape=shape)
    return tf.Variable(initial)

def conv2d(x, W):
    return tf.nn.conv2d(x, W, strides=[1, 1, 1, 1], padding='SAME')

def max_pool_2x2(x):
    return tf.nn.max_pool(x, ksize=[1, 2, 2, 1],
                          strides=[1, 2, 2, 1], padding='SAME')

x = tf.placeholder(tf.float32, [None, 3072], name="x")
x_image = tf.reshape(x, [-1,32,32,3])

# Put in weight decay and initialization params
# Layer 1
W_conv1 = weight_variable([5, 5, 3, 64], stddev=5e-2)
b_conv1 = bias_variable([64], init_val=0.0)

conv1 = tf.nn.relu(conv2d(x_image, W_conv1) + b_conv1)
pool1 = tf.nn.max_pool(conv1, ksize=[1, 3, 3, 1], strides=[1, 2, 2, 1],
                       padding='SAME', name='pool1')
norm1 = tf.nn.lrn(pool1, 4, bias=1.0, alpha=0.001 / 9.0, beta=0.75,
                  name='norm1')

# Layer 2
W_conv2 = weight_variable([5, 5, 64, 64], stddev=5e-2)
b_conv2 = bias_variable([64])

conv2 = tf.nn.relu(conv2d(norm1, W_conv2) + b_conv2)
norm2 = tf.nn.lrn(conv2, 4, bias=1.0, alpha=0.001 / 9.0, beta=0.75,
                  name='norm2')
pool2 = tf.nn.max_pool(norm2, ksize=[1, 3, 3, 1],
                       strides=[1, 2, 2, 1], padding='SAME', name='pool2')
reshape = tf.reshape(pool2, [-1, 8 * 8 * 64])

# Layer 3
W_local3 = weight_variable([8 * 8 * 64, 384], stddev=0.04)
b_local3 = bias_variable([384])
W_local3_weight_decay = tf.mul(tf.nn.l2_loss(W_local3), 0.004)
tf.add_to_collection('losses', W_local3_weight_decay)

local3 = tf.nn.relu(tf.matmul(reshape, W_local3) + b_local3)

# Layer 4
W_local4 = weight_variable([384, 192], stddev=0.04)
b_local4 = bias_variable([192])
W_local4_weight_decay = tf.mul(tf.nn.l2_loss(W_local4), 0.004)
tf.add_to_collection('losses', W_local4_weight_decay)

local4 = tf.nn.relu(tf.matmul(local3, W_local4) + b_local4)

# Layer 5 (Softmax)
W_softmax = weight_variable([192, NUM_CLASSES], stddev=1/192.0)
b_softmax = bias_variable([NUM_CLASSES], init_val=0.0)
softmax_logits = tf.nn.softmax(tf.matmul(local4, W_softmax) + b_softmax, name='softmax_logits')
y_ = tf.placeholder(tf.int64, [None], name="y_")

cross_entropy = tf.nn.sparse_softmax_cross_entropy_with_logits(
    softmax_logits, y_, name='cross_entropy_per_example')
cross_entropy_mean = tf.reduce_mean(cross_entropy, name='cross_entropy')
tf.add_to_collection('losses', cross_entropy_mean)
total_loss = tf.add_n(tf.get_collection('losses'), name='total_loss')
# train_step = tf.train.AdamOptimizer(1e-4).minimize(cross_entropy)
train_step = tf.train.AdamOptimizer(1e-4).minimize(total_loss)
correct_prediction = tf.equal(tf.argmax(softmax_logits,1), y_)
accuracy = tf.reduce_mean(tf.cast(correct_prediction, tf.float32))

# For importing/exporting graphs
def save_full_graph(sess, path):
    with sess.graph.as_default():
        saver = tf.train.Saver()
        saver.save(sess, path, meta_graph_suffix='meta', write_meta_graph=True)

def load_full_graph(path):
    sess = tf.Session('', tf.Graph())
    with sess.graph.as_default():
        saver = tf.train.import_meta_graph(path + '.meta')
        saver.restore(sess, path)
    return sess
export_dir = 'models'
if not os.path.exists(export_dir):
    os.mkdir(export_dir)
saver = tf.train.Saver(sharded=False)

X_train, y_train = load_cifar('data', norm=True)
batch_iter_ = data_iterator(X_train, y_train)
sess.run(tf.initialize_all_variables())
for i in range(50000):
    images_batch, labels_batch = batch_iter_.next()
    if i % 100 == 0:
        train_accuracy = accuracy.eval(
            feed_dict={x:images_batch, y_: labels_batch})
        train_loss = total_loss.eval(
                feed_dict={x:images_batch, y_: labels_batch})
        print("step %d, training accuracy %g, training loss %f" % (i, train_accuracy, train_loss))
    train_step.run(feed_dict={x: images_batch, y_: labels_batch})

# Write out everything
save_full_graph(sess, os.path.join(export_dir, 'cifar10_model_full'))
# Read the contents back in
sess = load_full_graph(os.path.join(export_dir, 'cifar10_model_full'))
for i in range(20):
    images_batch, labels_batch = batch_iter_.next()
    print(sess.run('softmax_logits:0',
        feed_dict={'x:0':images_batch}))
