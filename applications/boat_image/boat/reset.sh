#!/bin/sh
node_id=-1
if [ $# -ne 1 ] ; then
    echo "ERROR: [reset.sh] Incorrect number of arguments. Usage: sh reset.sh NODE_ID"
    exit 1
else
    node_id=$1
fi

echo "INFO: [reset.sh] Cleaning up Clipper $node_id"

# Clean up containers
containers=`docker network inspect clipper_network_$node_id --format '{{range .Containers}}{{println .Name}}{{end}}'`
docker stop $containers >&- 2>&-
docker rm $containers >&- 2>&-

# Clean up volumns
docker volume prune --force >&- 2>&-

# Clean up networks
docker network rm clipper_network_$node_id >&- 2>&-

# i=0
# while [ $i -lt $num_nodes ] ; do
#     containers=`docker network inspect clipper_network_$i --format '{{range .Containers}}{{println .Name}}{{end}}'`
#     docker stop $containers >&- 2>&-
#     docker rm $containers >&- 2>&-
#     docker network rm clipper_network_$i >&- 2>&-
#     i=$(($i+1))
# done

images=`docker images --format '{{.Repository}}\t{{.ID}}' | grep '<none>' | cut -f 2`
docker image rm $images >&- 2>&-

echo "INFO: [reset.sh] All Clipper $node_id containers, volumes, networks and outdated images are cleared away."
exit 0