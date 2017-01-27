import cPickle
import numpy as np
import os
import tarfile
import urllib

# Taken from https://www.cs.toronto.edu/~kriz/cifar.html
def unpickle(file):
    import cPickle
    fo = open(file, 'rb')
    dict = cPickle.load(fo)
    fo.close()
    return dict

# download and extract the data
if not os.path.exists('data'):
    os.mkdir('data')
if not os.path.exists('data/cifar-10-python.tar.gz'):
    print("CIFAR10 dataset not found, downloading...")
    # http://stackoverflow.com/a/19602990
    testfile = urllib.URLopener()
    testfile.retrieve('https://www.cs.toronto.edu/~kriz/cifar-10-python.tar.gz',
                      'data/cifar-10-python.tar.gz')
    print("Finished!")

print("Extracting tar file...")
cifar10_tar = tarfile.open('data/cifar-10-python.tar.gz')
cifar10_tar.extractall('data')
cifar10_tar.close()

print("Unpickling data batches...")
batches_dir = 'data/cifar-10-batches-py'
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

print("Writing out to CSV...")
# Output to CSV format
# Based off load_digits used in previous scripts
# Format: label,pixel0,pixel1,...
with open('data/cifar_train.data', 'w') as outfile:
    for i in range(len(data)):
        example_data = [labels[i]] + data[i].tolist()
        example_data = map(lambda x: str(x), example_data)
        output_line = ','.join(example_data) + '\n'
        outfile.write(output_line)
with open('data/cifar_test.data', 'w') as outfile:
    for i in range(len(test_data)):
        example_data = [test_labels[i]] + test_data[i].tolist()
        example_data = map(lambda x: str(x), example_data)
        output_line = ','.join(example_data) + '\n'
        outfile.write(output_line)
print("Finished!")
