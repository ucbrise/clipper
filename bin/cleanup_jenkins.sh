# This script will delete all testing containers and images
# available on this machine. 
set -x

CLIPPER_REGISTRY=clippertesting

# best effort, ignore all errors
try_cleanup() {
    (docker ps --all | grep "$@" | awk '{ print $1 }' | xargs docker kill) || true
}

KEYWORDS=(
  ${CLIPPER_REGISTRY}
  clipper
  redis
  model
  prometheus
  fluentd
  metric
)

echo "Docker Status Before"
docker ps

for i in "${KEYWORDS[@]}"
do
  try_cleanup "$i"
done

echo "Docker Status After"
docker ps
