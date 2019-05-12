# This container RPC test operates as follows:
# 1. Containers maintain a timestamped log of recent RPC messages (stored via ringbuffer).
# 2. The test script launches a Java container and a Python container, each with models having access to the RPC message log
# 3. The test script executes the rpctest target, starting Clipper's rpc_container_test
# 4. Clipper waits for the containers to connect. It then requests all recent log messages from Clipper based on the system time
# 5. Clipper verifies that the container's log messages are consistent with correct RPC protocol, logging any errors if they occur.
# 6. Clipper logs a success message if the containers' protocols are valid, else it logs an overall failure.
# 7. The test script repeats steps 3-6.
# 8. The test scripts exits with code 0 if both iterations of rpc_container_test were successful. Otherwise, it exits with code 1.
# 9. rpc_container_test will fail automatically if containers are not validated within a configurable timeout (test_container_rpc.sh specifies a timeout of 20 seconds).

#!/usr/bin/env bash

set -e
set -u
set -o pipefail

if [ $# -eq 0 ]
then
    echo "The redis port must be supplied as an argument!"
    exit
fi

success=false

PORT_RANGE_START=10000
PORT_RANGE_END=20000
RPC_SERVICE_PORT=`perl -e "print int(rand($PORT_RANGE_END-$PORT_RANGE_START)) + $PORT_RANGE_START"`

function clean_up {
    # Perform program exit housekeeping
    # echo Background jobs: $(jobs -l)
    # echo
    # echo Killing jobs
    echo Exiting RPC test...
    if [ "$success" = false ] ; then
      echo 'Error in RPC test'
    fi
    kill $(jobs -p) &> /dev/null
    echo
    sleep 2
    # echo Remaining background jobs: $(jobs -l)
    exit
}


trap clean_up SIGHUP SIGINT SIGTERM EXIT
unset CDPATH
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd $DIR

# Start python rpc test container
echo "Starting python RPC test container... (port:$RPC_SERVICE_PORT)"
python ../python/rpc_test_container.py --rpc_service_port $RPC_SERVICE_PORT &

# Deprecate JVM containers
# cd ../jvm
# mvn clean package -DskipTests &> /dev/null
# # Start java rpc test container
# echo "Starting java RPC test container..."
# # && mvn -Dtest=RPCProtocolTest test &> /dev/null &
# java -Djava.library.path=$JZMQ_HOME \
#    -cp rpc-test/target/rpc-test-0.1.jar \
#    ai.clipper.rpctest.RPCProtocolTest &

cd $DIR/../../
./configure && cd debug/src

# Start cpp rpc test container
cd container
make container_rpc_test
container_uptime_seconds=180
echo "Starting cpp RPC test container... (port:$RPC_SERVICE_PORT)"
./container_rpc_test -t $container_uptime_seconds -p $RPC_SERVICE_PORT &

sleep 10s

cd $DIR/../../debug/src/benchmarks
make rpctest
REDIS_PORT=$1
echo "Executing RPC test (first iteration)... (redis port:$REDIS_PORT, rpc_service_port:$RPC_SERVICE_PORT)"
./rpctest --num_containers=2 --timeout_seconds=30 --redis_port $REDIS_PORT --rpc_service_port $RPC_SERVICE_PORT
redis-cli -p $REDIS_PORT "flushall"
echo "Sleeping for 5 seconds..."
sleep 5s
echo "Executing RPC test (second iteration)... (redis port:$REDIS_PORT, rpc_service_port:$RPC_SERVICE_PORT)"
./rpctest --num_containers=2 --timeout_seconds=30 --redis_port $REDIS_PORT --rpc_service_port $RPC_SERVICE_PORT
redis-cli -p $REDIS_PORT "flushall"
echo "TEST PASSED!"
success=true
exit 0
