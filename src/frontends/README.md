# REST Example
A simple example using the REST API:
```bash
# Run this in a separate window
$ ./rest
# Send JSON request using curl
$ curl -X POST --header "Content-Type:text/json" -d '{"uid": 1, "input": [1, 2, 3, 4]}' 127.0.0.1:1337/predict
```

# RPC Benchmark Example:
To run the rpc service benchmark.

Start the NoopContainer:
```bash
$ CLIPPER_MODEL_NAME="m" CLIPPER_MODEL_VERSION=1 CLIPPER_PORT=8000 CLIPPER_INPUT_TYPE="ints" python noop_container.py
```

Edit the `run_benchmarks()` function in [`rpc_service_bench.cpp`](src/rpc_service_bench.cpp) to use the input type you specified when starting the model container.

Then compile the benchmark and run it:
```bash
$ ./configure --release
$ cd release
$ make rpcbench
$ ./src/frontends/rpcbench
``
