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
./rpctest --num_containers=2 --timeout_seconds=20
EXITCODE=$?
if [ $EXITCODE == 0 ]
then
   echo "TEST PASSED!"
else
   echo "TEST FAILED!"
fi
exit $EXITCODE

