#!/usr/bin/env bash

set -e
set -u
set -o pipefail

success=false

function clean_up {
    # Perform program exit housekeeping
    # echo Background jobs: $(jobs -l)
    # echo
    # echo Killing jobs
    echo Exiting...
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
echo "Starting python RPC test container..."
python ../python/rpc_test_container.py &>/dev/null & 


cd ../java/clipper-java-container/
mvn clean package -DskipTests &> /dev/null
#Start java rpc test container
echo "Starting java RPC test container..."
# && mvn -Dtest=RPCProtocolTest test &> /dev/null &
java -Djava.library.path=/usr/local/lib \
   -cp target/clipper-java-container-1.0-SNAPSHOT.jar \
   clipper.container.app.RPCProtocolTest &> /dev/null &

sleep 10
cd $DIR/../../
./configure && cd debug
cd src/benchmarks
make rpctest
echo "Executing RPC test (first iteration)..."
./rpctest --num_containers=2 --timeout_seconds=20
echo "Sleeping for 5 seconds..."
sleep 5s
echo "Executing RPC test (second iteration)..."
./rpctest --num_containers=2 --timeout_seconds=20
echo "TEST PASSED!"
success=true
exit 0

