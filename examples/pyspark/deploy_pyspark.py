from __future__ import absolute_import, print_function

import sys
import os
import numpy as np
from clipper_admin.clipper_manager import Clipper



import findspark
findspark.init()
import pyspark
from pyspark import SparkConf, SparkContext
from pyspark.mllib.classification import LogisticRegressionModel, LogisticRegressionWithSGD
from pyspark.mllib.classification import SVMModel, SVMWithSGD
from pyspark.mllib.tree import RandomForestModel
from pyspark.mllib.regression import LabeledPoint


'''
Run using:
[SparkDir]/spark/bin/spark-submit --driver-memory 2g mnist.py
'''


# from cvm.svm import SVC
# from cvm.kreg import KernelLogisticRegression

# Set parameters here:
NMAX = 10000
GAMMA = 0.02
C = 1.0




def normalize(x):
    return x.astype(np.double) / 255.0


def objective(y, pos_label):
    # prediction objective
    if y == pos_label:
        return 1
    else:
        return 0

def parseData(line, obj, pos_label):
    fields = line.strip().split(',')
    # return LabeledPoint(obj(int(fields[0]), pos_label), [float(v)/255.0 for v in fields[1:]])
    return LabeledPoint(obj(int(fields[0]), pos_label), normalize(np.array(fields[1:])))

def predict(sc, model, xs):
    return [str(model.predict(normalize(x))) for x in xs]

def train_logistic_regression(pos_label):
    conf = SparkConf() \
        .setAppName("crankshaw-pyspark") \
        .set("spark.executor.memory", "2g") \
        .set("master", "local")
    sc = SparkContext(conf=conf, batchSize=10)
    print('Parsing data')
    train_path = "/Users/crankshaw/code/amplab/model-serving/data/mnist_data/train.data"
    test_path = "/Users/crankshaw/code/amplab/model-serving/data/mnist_data/test.data"
    trainRDD = sc.textFile(train_path).map(lambda line: parseData(line, objective, pos_label)).cache()

    with open(test_path, "r") as test_file:
        test_data = [np.array(l.strip().split(",")[1:]) for l in test_file]

    # testRDD = sc.textFile("/crankshaw-local/mnist/data/test.data").map(lambda line: parseData(line, objective)).cache()

    print('Fitting model')

    model = LogisticRegressionWithSGD.train(trainRDD, iterations=100)

    print(predict(sc, model, test_data[:10]))

    # sameModel = LogisticRegressionModel.load(sc, path)
    # sameModel.predict(testRDD)
    # # # sameModel.predict(array([0.0, 1.0]))
    # # # sameModel.predict(SparseVector(2, {0: 1.0}))
    # #
    # # # model = SVC(gamma=GAMMA, C=C, nmax=NMAX)
    # # # model = KernelLogisticRegression(gamma=0.01, C=2.0, nmax=3000)
    # #
    # # # model.train(trainRDD)
    # # print("Time: {:2.2f}".format(time.time() - time_start))
    # #
    # # print 'Predicting outcomes training set'
    # # labelsAndPredsTrain = trainRDD.map(lambda p: (p.label, sameModel.predict(p.features)))
    # # trainErr = labelsAndPredsTrain.filter(lambda (v, p): v != p).count() / float(trainRDD.count())
    # # print("Training Error = " + str(trainErr))
    # # print("Time: {:2.2f}".format(time.time() - time_start))
    # #
    # print 'Predicting outcomes test set'
    # labelsAndPredsTest = testRDD.map(lambda p: (p.label, sameModel.predict(p.features)))
    # testErr = labelsAndPredsTest.filter(lambda (v, p): v != p).count() / float(testRDD.count())
    # print("Test Error = " + str(testErr))
    # print("Time: {:2.2f}".format(time.time() - time_start))

    # clean up
    sc.stop()

def train_svm(pos_label):
    conf = SparkConf() \
        .setAppName("crankshaw-pyspark") \
        .set("spark.executor.memory", "2g") \
        .set("spark.kryoserializer.buffer.mb", "128") \
        .set("master", "local")
    sc = SparkContext(conf=conf, batchSize=10)
    print('Parsing data')
    trainRDD = sc.textFile("/crankshaw-local/mnist/data/train_norm.data").map(lambda line: parseData(line, objective, pos_label)).cache()
    # testRDD = sc.textFile("/crankshaw-local/mnist/data/test.data").map(lambda line: parseData(line, objective)).cache()

    print('Fitting model')

    svm = SVMWithSGD.train(trainRDD)

    path = 'spark_models/svm_predict_%d' % pos_label
    svm.save(sc, path)
    sc.stop()

def train_random_forest(num_trees, depth, pos_label):

    # time_start = time.time()
    # data_path = os.path.expanduser("~/mnist/data/train.data")
    # trainRDD = sc.textFile(data_path).map(lambda line: parseData(line, objective)).cache()
    # testRDD = sc.textFile('/Users/crankshaw/model-serving/data/mnist_data/test-mnist-dense-with-labels.data').map(lambda line: parseData(line, objective)).cache()

    conf = SparkConf() \
        .setAppName("crankshaw-pyspark") \
        .set("spark.executor.memory", "80g") \
        .set("spark.driver.memory", "80g") \
        .set("spark.kryoserializer.buffer.mb", "512") \
        .set("master", "local")
    sc = SparkContext(conf=conf, batchSize=10)
    data_path = os.path.expanduser("/crankshaw-local/mnist/data/train_norm.data")
    print('Parsing data')
    trainRDD = sc.textFile("/crankshaw-local/mnist/data/train_norm.data").map(lambda line: parseData(line, objective, pos_label)).cache()

    print('Fitting model')
    rf = RandomForest.trainClassifier(trainRDD, 2, {}, num_trees, maxDepth=depth)
    rf.save(sc, "/crankshaw-local/clipper/model_wrappers/python/spark_models/%drf_pred_%d_depth_%d" % (num_trees, pos_label, depth))
    sc.stop()


if __name__ == "__main__":
    pos_label = 3
    train_logistic_regression(pos_label)

    # for i in range(10):
    #     print "training model to predict %d" % (i + 1)
    #     train_logistic_regression(i + 1)

    # conf = SparkConf() \
    #     .setAppName("crankshaw-pyspark") \
    #     .set("spark.executor.memory", "100g") \
    #     .set("spark.driver.memory", "100g") \
    #     .set("spark.kryoserializer.buffer.mb", "512") \
    #     .set("master", "local")
    # sc = SparkContext(conf=conf, batchSize=10)
    # data_path = os.path.expanduser("/crankshaw-local/mnist/data/train_norm.data")
    # print 'Parsing data'
    # pos_label = 3
    # trainRDD = sc.textFile("/crankshaw-local/mnist/data/train_norm.data").map(lambda line: parseData(line, objective, pos_label)).cache()
    # pos_label = 3
    # train_random_forest(50, 2, pos_label)
    # print "Trained depth 2 RF"
    # train_random_forest(50, 4, pos_label)
    # print "Trained depth 4 RF"
    # train_random_forest(50, 8, pos_label)
    # print "Trained depth 8 RF"
    # train_random_forest(50, 16, pos_label)
    # print "Trained depth 16 RF"
    # train_random_forest(50, 32, pos_label)
    # print "Trained depth 32 RF"
    # # sc.stop()
    #
    # # train_random_forest(100)
    # # train_random_forest(500)
    # # if (len(sys.argv) != 1):
    # #     print "Usage: [SPARKDIR]/bin/spark-submit --driver-memory 2g " + \
    # #         "mnist.py"
    # #     sys.exit(1)
    #
    # # set up environment
