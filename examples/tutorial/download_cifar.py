#!/usr/bin/env python

from __future__ import print_function
import cPickle
import sys
import numpy as np
import os
import tarfile
import urllib


# Taken from https://www.cs.toronto.edu/~kriz/cifar.html
def unpickle(file):
    fo = open(file, 'rb')
    dict = cPickle.load(fo)
    fo.close()
    return dict


def download_cifar(loc):
    # download and extract the data
    if not os.path.exists(loc):
        os.makedirs(loc)
    if not os.path.exists(os.path.join(loc, 'cifar-10-python.tar.gz')):
        print("CIFAR10 dataset not found, downloading...")
        # http://stackoverflow.com/a/19602990
        testfile = urllib.URLopener()
        testfile.retrieve(
            'https://www.cs.toronto.edu/~kriz/cifar-10-python.tar.gz',
            os.path.join(loc, 'cifar-10-python.tar.gz'))
        print("Finished downloading")

    print("Extracting tar file...")
    cifar10_tar = tarfile.open(os.path.join(loc, 'cifar-10-python.tar.gz'))
    cifar10_tar.extractall(loc)
    cifar10_tar.close()

    print("Unpickling data batches...")
    batches_dir = os.path.join(loc, 'cifar-10-batches-py')
    data, labels = [], []
    test_data, test_labels = [], []
    for filename in os.listdir(batches_dir):
        file_path = os.path.join(batches_dir, filename)
        if 'data_batch' in filename:
            print(file_path)
            batch_dict = unpickle(file_path)
            data.append(batch_dict['data'])
            labels.append(batch_dict['labels'])
        elif filename == 'test_batch':
            print(file_path)
            batch_dict = unpickle(file_path)
            test_data = batch_dict['data']
            test_labels = np.array(batch_dict['labels'])

    data = np.vstack(data)
    labels = np.hstack(labels)
    return (data, labels, test_data, test_labels)


def extract_to_csv(loc, data, labels, test_data, test_labels):
    print("Writing out to CSV...")
    # Output to CSV format
    # Format: label,pixel0,pixel1,...
    with open(os.path.join(loc, 'cifar_train.data'), 'w') as outfile:
        for i in range(len(data)):
            example_data = [labels[i]] + data[i].tolist()
            example_data = map(lambda x: str(x), example_data)
            output_line = ','.join(example_data) + '\n'
            outfile.write(output_line)
    with open(os.path.join(loc, 'cifar_test.data'), 'w') as outfile:
        for i in range(len(test_data)):
            example_data = [test_labels[i]] + test_data[i].tolist()
            example_data = map(lambda x: str(x), example_data)
            output_line = ','.join(example_data) + '\n'
            outfile.write(output_line)
    print("Finished!")


if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: ./download_cifar.py download_path")
        sys.exit(1)
    loc = os.path.abspath(os.path.expanduser(sys.argv[1]))
    extract_to_csv(loc, *download_cifar(loc))
