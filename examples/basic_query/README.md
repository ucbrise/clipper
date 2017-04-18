
# Basic Query Example Requirements

The examples in this directory depend on a few Python packages.
We recommend using [Anaconda](https://www.continuum.io/downloads)
to install Python packages.

+ [`requests`](http://docs.python-requests.org/en/master/)
+ [`numpy`](http://www.numpy.org/)

# Running the example query

1. Start Clipper locally
  + With Docker `cd <clipper-root>/docker && docker-compose up -d query_frontend`
  + Without Docker `<clipper-root>/bin/start_clipper.sh`
2. Run the example: `python example_client.py`
3. Connect a container: `cd <clipper-root>/containers/python && CLIPPER_MODEL_NAME=example_model CLIPPER_MODEL_VERSION=1 CLIPPER_INPUT_TYPE=doubles python noop_container.py`
