This module is related to Clipper's metric monitoring function. For full design, see [Design Doc.](https://docs.google.com/document/d/10whRxCc97gOJl4j2lY6R-v7cI_ZoAMVcNGPG_9oj6iY/edit?usp=sharing)

## Prometheus Server
We use prometheus as the metric tracking system. Once you spin up a clipper query frontend and a model containers. 
If you are using `DockerContainerManager`, You can view prometheus UI at: [`http://localhost:9090`](http://localhost:9090). 
If you are using `KubernetesContainerManager`, You can query the metric address by calling `get_metric_addr()`. 

Please note that Prometheus UI is for debug purpose only. You can view certain metric and graph the timeseries. But for better visualization, we recommend [Grafana](https://grafana.com/). Grafana has default support for Prometheus Client. Feel free to checkout `examples/monitoring` for example of displaying Clipper metrics in Grafana.

## Avaliable Metrics
See `metrics_config.yaml` for the metrics detail and description. This configuration file is used in the creation of `rpc.py` container

## Scrape Target Discovery
In Docker, we add scrape targets as the service is setup and an new replica added. The scrape configuration file is saved in `tmp/clipper`. 

In Kubernetes, prometheus can utilize Kubernetes's service discovery mechanism. The scrape target will be acquired automatically. In more detail, any pods with annotation 

```yaml
'prometheus.io/scrape': 'true'
```

will be scraped. 