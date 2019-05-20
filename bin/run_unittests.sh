#!/usr/bin/env bash

set -e
set -u
set -o pipefail

function usage {
    cat <<EOF
    usage: run_unittests.sh

    This script is used to run Clipper tests. By default, it will run all possible
    tests

    Options:

    -a, --all                   Run all tests
    -l, --libclipper            Run tests only for libclipper folder.
    -m, --management            Run tests only for management folder.
    -f, --frontend              Run tests only for frontend folder.
    -j, --jvm-container         Run tests only for jvm container folder.
    -c, --cpp-container         Run tests only for the cpp container folder.
    -rc, --r-container          Run tests only for the R container folder.
    -r, --rpc-container         Run tests only for rpc container folder.
    -i, --integration_tests     Run integration tests.
    -h, --help                  Display this message and exit.

$@
EOF
}

function clean_up {
    # Perform program exit housekeeping
    # echo Background jobs: $(jobs -l)
    # echo
    # echo Killing jobs
    echo Exiting unit tests...
    kill $(jobs -p) &> /dev/null
    echo "Cleanup exit code: $?"
    sleep 2
    # echo Remaining background jobs: $(jobs -l)
    exit
}

function randomize_redis_port {
    set +e  # turn of exit on command fail
    REDIS_PORT=$((34256 + RANDOM % 1000))
    lsof -i :$REDIS_PORT &> /dev/null

    if [ $? -eq 0 ]; then # existing port in use found
      while true; do
        REDIS_PORT=$(($REDIS_PORT + RANDOM % 1000))
        lsof -i :$REDIS_PORT &> /dev/null
        if [ $? -eq 1 ]; then  # port not in use
          break
        fi
      done
    fi
    echo "randomized redis port: $REDIS_PORT"
}

trap clean_up SIGHUP SIGINT SIGTERM EXIT

unset CDPATH
# one-liner from http://stackoverflow.com/a/246128
# Determines absolute path of the directory containing
# the script.
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

function set_test_environment {
  if ! type "redis-server" &> /dev/null; then
      echo -e "\nERROR:"
      echo -e "\tUnit tests require Redis. Please install redis-server"
      echo -e "\tand make sure it's on your PATH.\n"
      exit 1
  fi

  randomize_redis_port
  set -e # turn back on exit on command fail

  # start Redis on the test port if it's not already running
  redis-server --port $REDIS_PORT &> /dev/null &
}

function run_jvm_container_tests {
  echo "Running JVM container tests..."
  cd $DIR
  cd ../containers/jvm
  mvn test -q
}

function run_r_container_tests {
  cd $DIR
  cd ../containers/R/tests
  echo "Running R container tests..."
  ./run_tests.sh
}

function run_rpc_container_tests {
  echo "Testing container RPC protocol correctness..."
  cd $DIR
  cd ../containers/test/
  ./test_container_rpc.sh $REDIS_PORT
}

function run_libclipper_tests {
  cd $DIR/../debug
  echo -e "\nRunning libclipper tests\n\n"
  ./src/libclipper/libclippertests --redis_port $REDIS_PORT
}

function run_management_tests {
  cd $DIR/../debug
  echo -e "\nRunning management tests\n\n"
  ./src/management/managementtests --redis_port $REDIS_PORT
}

function run_frontend_tests {
  cd $DIR/../debug
  echo -e "\nRunning frontend tests\n\n"
  ./src/frontends/frontendtests --redis_port $REDIS_PORT
}

# This function is kept for legacy reason.
# In Clipper CI starting 2019, the following suite of tests
# will be ran directly from Jenkins on individual basis. 
function run_integration_tests {
  echo -e "\nDEPRECATED: Running integration tests\n\n"
  cd $DIR

  echo "GREPTHIS Docker State before:"
  docker ps

  python ../integration-tests/clipper_admin_tests.py
  python ../integration-tests/many_apps_many_models.py
  python ../integration-tests/deploy_pyspark_models.py
  python ../integration-tests/deploy_pyspark_pipeline_models.py
  python ../integration-tests/deploy_pyspark_sparkml_models.py
  python ../integration-tests/deploy_tensorflow_models.py
  python ../integration-tests/deploy_mxnet_models.py
  python ../integration-tests/deploy_pytorch_models.py
  python ../integration-tests/multi_tenancy_test.py

  ../integration-tests/r_integration_test/rclipper_test.sh
  python ../integration-tests/clipper_metric_docker.py

  # python ../integration-tests/kubernetes_integration_test.py
  # python ../integration-tests/kubernetes_multi_frontend.py
  # See issue #475
  # python ../integration-tests/deploy_pytorch_to_caffe2_with_onnx.py
  # python ../integration-tests/clipper_metric_kube.py
  # python ../integration-tests/multi_tenancy_test.py --kubernetes
  # python ../integration-tests/kubernetes_namespace.py

  echo "GREPTHIS Docker State After"
  docker ps

  echo "Exit code: $?"
  echo "GREPTHIS Done running unit tests"
}

function run_all_tests {
  run_libclipper_tests
  redis-cli -p $REDIS_PORT "flushall"
  run_frontend_tests
  redis-cli -p $REDIS_PORT "flushall"
  run_management_tests
  redis-cli -p $REDIS_PORT "flushall"
  run_integration_tests
  redis-cli -p $REDIS_PORT "flushall"
  run_jvm_container_tests
  redis-cli -p $REDIS_PORT "flushall"
  run_r_container_tests
  redis-cli -p $REDIS_PORT "flushall"
  run_rpc_container_tests
  redis-cli -p $REDIS_PORT "flushall"
}

if [ "$#" == 0 ]
then
  args="--help"
else
  args=$1
fi

case $args in
    -a | --all )                set_test_environment
                                run_all_tests
                                ;;
    -l | --libclipper )         set_test_environment
                                run_libclipper_tests
                                ;;
    -m | --management )         set_test_environment
                                run_management_tests
                                ;;
    -f | --frontend )           set_test_environment
                                run_frontend_tests
                                ;;
    # R and JVM are currently unmaintained. 
    # -j | --jvm-container )      set_test_environment
    #                             run_jvm_container_tests
    #                             ;;
    # -rc | --r-container )       set_test_environment
    #                             run_r_container_tests
    #                             ;;
    -r | --rpc-container )      set_test_environment
                                run_rpc_container_tests
                                ;;
    # -i is deprecated. 
    # -i | --integration_tests )  set_test_environment
    #                             run_integration_tests
    #                             ;;
    -h | --help )               usage
                                ;;
    * )                         usage
esac
