trap "trap - SIGTERM && kill -- -$$" SIGINT SIGTERM EXIT
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Start python rpc test container
echo "Starting python RPC test container..."
python ../python/rpc_test_container.py &>/dev/null & 

#Start java rpc test container
echo "Starting java RPC test container..."
cd ../java/clipper-java-container/ && mvn -Dtest=RPCProtocolTest test &>/dev/null &

sleep 10s
cd DIR
cd ../../
./configure && cd debug
cd src/benchmarks
make rpctest
echo "Executing RPC test (first iteration)..."
./rpctest --num_containers=2 --timeout_seconds=20
EXITCODE_FIRST=$?
echo "Sleeping for 5 seconds..."
sleep 5s
echo "Executing RPC test (second iteration)..."
./rpctest --num_containers=2 --timeout_seconds=20
EXITCODE_SECOND=$?
if [ $EXITCODE_FIRST == 0 ] && [ $EXITCODE_SECOND == 0 ]
then
   echo "TEST PASSED!"
   exit 0
else
   echo "TEST FAILED!"
   exit 1
fi

