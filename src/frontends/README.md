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

__NOTE:__ Redis must be running on port 6379 (the default port) to
run the RPC benchmark.
