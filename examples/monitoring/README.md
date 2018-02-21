This example demonstrates Clipper's metric monitoring functionality.

Steps:
1. Run `python2 query.py`
2. Wait for the query.py start outputing predictions.
3. Run `python init_grafana.py` and follow the instruction.

For Kubernetes:

- You can find the metric address via `KubernetesContainerManager.get_metric_addr` function. 