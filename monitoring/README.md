This module is related to Clipper's metric monitoring function. For full design, see [Design Doc.](https://docs.google.com/document/d/10whRxCc97gOJl4j2lY6R-v7cI_ZoAMVcNGPG_9oj6iY/edit?usp=sharing)

## Prometheus Server
We use prometheus as the metric tracking system. Once you spin up a clipper query frontend and a model containers. You can view prometheus UI at: [`http://localhost:9090`](http://localhost:9090). 

Please note that Prometheus UI is for debug purpose only. You can view certain metric and graph the timeseries. But for better visualization, we recommend [Grafana](https://grafana.com/). Grafana has default support for Prometheus Client. 

## Avaliable Metrics
See `metrics_config.yaml` for the metrics detail and description
