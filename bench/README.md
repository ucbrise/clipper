# Running the performance benchmark

## Required Files
### CIFAR10 Python Dataset for SKLearn Model
This benchmark serves an SKLearn model that depends on the CIFAR10 Python dataset for training. In order to train the model with this dataset, it must be converted to CSV format. The [CIFAR10 python download utility](https://github.com/ucbrise/clipper/blob/develop/examples/tutorial/download_cifar.py) can be used to obtain the required CSV-formatted dataset.

```sh
../examples/tutorial/download_cifar.py /path/to/save/dataset
```

### CIFAR10 Binary Dataset for Query Execution
The C++ benchmark works by sending CIFAR10 query vectors to the container serving the trained SKLearn model. To achieve this, the **binary dataset** is required. It can also be obtained from [https://www.cs.toronto.edu/~kriz/cifar.html](https://www.cs.toronto.edu/~kriz/cifar.html).

## Optional Configuration Files
The following benchmark attributes can be loaded via a JSON configuration file:
- **cifar_data_path**: The path to a **specific binary data file** within the CIFAR10 binary dataset with a name of the form `data_batch_<n>.bin`. (`data_batch_1.bin`, for example)
- **num_threads**: The number of threads of execution
- **num_batches**: The number of batches of requests to be sent by each thread
- **batch_size**: The number of requests to be sent in each batch
- **batch_delay_millis**: The per-thread delay between batches, in milliseconds

To configure these attributes, create a JSON file with the following format and specify its path when the benchmark is executed (see **Steps of Execution** below).

```
{
   "cifar_data_path":"<cifar_data_path>",
   "num_threads":"<num_threads>",
   "num_batches":"<num_batches>",
   "batch_size":"<batch_size>",
   "batch_delay_millis":"<batch_delay_millis>"
}
```

If a configuration file is not specified, the benchmark will prompt you for the values of these attributes at runtime.

## Steps of Execution
**These steps are given relative to the current directory.**

1. Execute the following:
  ```sh
  ./setup_bench.sh <path_to_cifar_python_dataset>
  ```
where `<path_to_cifar_python_dataset>` is the path to the **directory** containing a parsed CIFAR10 CSV data file with name `cifar_train.data`.

2. Execute the following:
  ```sh
  ../configure --release && cd ../release
  make end_to_end_bench
  ```
  
3. If you created a JSON configuration file above, execute the following:
  ```sh
  ./src/benchmarks/end_to_end_bench -f "<path_to_config_.json>"
  ```
  
  Otherwise, execute
  ```sh
  ./src/benchmarks/end_to_end_bench
  ```
  and specify the values of the attributes enumerated in the **Optional Configuration Files** section above. 
