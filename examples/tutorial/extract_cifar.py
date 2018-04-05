#!/usr/bin/env python

from __future__ import print_function
import cPickle
import sys
import numpy as np
import os
import tarfile


# Taken from https://www.cs.toronto.edu/~kriz/cifar.html
def unpickle(file):
    fo = open(file, 'rb')
    dict = cPickle.load(fo)
    fo.close()
    return dict


def extract_cifar(loc):
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


def extract_to_csv(loc,
                   data,
                   labels,
                   test_data,
                   test_labels,
                   max_train_datapoints=None,
                   max_test_datapoints=None):
    print("Writing out to CSV...")
    # Output to CSV format
    # Format: label,pixel0,pixel1,...

    # Users can optionally specify a maximum number of training and testing datapoints
    num_train_datapoints = len(data) if max_train_datapoints is None \
        else min(len(data), max_train_datapoints)
    num_test_datapoints = len(test_data) if max_test_datapoints is None \
        else min(len(test_data), max_train_datapoints)

    with open(os.path.join(loc, 'cifar_train.data'), 'w') as outfile:
        for i in range(num_train_datapoints):
            example_data = [labels[i]] + data[i].tolist()
            example_data = map(lambda x: str(x), example_data)
            output_line = ','.join(example_data) + '\n'
            outfile.write(output_line)
    with open(os.path.join(loc, 'cifar_test.data'), 'w') as outfile:
        for i in range(num_test_datapoints):
            example_data = [test_labels[i]] + test_data[i].tolist()
            example_data = map(lambda x: str(x), example_data)
            output_line = ','.join(example_data) + '\n'
            outfile.write(output_line)
    print("Finished!")


if __name__ == '__main__':
    if len(sys.argv) != 4:
        print("Usage: ./download_cifar.py <download_path> \
            <max_train_datapoints> <max_test_datapoints>")
        sys.exit(1)
    loc = os.path.abspath(os.path.expanduser(sys.argv[1]))

    def parse_datapoint_limits(input):
        if input == "None":
            return None
        else:
            limit = int(input)
            if limit <= 0:
                raise ValueError
            return limit

    try:
        max_train_datapoints = parse_datapoint_limits(sys.argv[2])
        max_test_datapoints = parse_datapoint_limits(sys.argv[3])
    except ValueError:
        print(
            "Invalid Input: <max_train_datapoints> and <max_test_datapoints> must be positive integers or None."
        )
        sys.exit(1)
    extract_to_csv(loc, *extract_cifar(loc), \
        max_train_datapoints=max_train_datapoints, max_test_datapoints=max_test_datapoints)
