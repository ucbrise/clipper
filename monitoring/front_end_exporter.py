import requests
from flatten_json import flatten
import itertools
import time
from prometheus_client import start_http_server
from prometheus_client.core import GaugeMetricFamily, REGISTRY
import argparse
import logging

parser = argparse.ArgumentParser(
    description='Spin up a node exporter for query_frontend.')
parser.add_argument(
    '--query_frontend_name',
    metavar='str',
    type=str,
    required=True,
    help='The name of docker container in clipper_network')
args = parser.parse_args()

logging.basicConfig(
    format='%(asctime)s %(levelname)-8s [%(filename)s:%(lineno)d] %(message)s',
    datefmt='%y-%m-%d:%H:%M:%S',
    level=logging.INFO)
logger = logging.getLogger(__name__)

query_frontend_id = args.query_frontend_name
ADDRESS = 'http://{}/metrics'.format(query_frontend_id)
logger.info("Scraping {}".format(ADDRESS))


def load_metric():
    try:
        res = requests.get(ADDRESS)
        return res.json()
    except Exception as e:
        logger.warning("Scrape Failed! Error: {}\n".format(e))
        return dict()


def multi_dict_unpacking(lst):
    """
    Receive a list of dictionaries, join them into one big dictionary
    """
    result = {}
    for d in lst:
        for key, val in d.items():
            result[key] = val
    return result


def parse_metric(metrics):
    if len(metrics) == 0:
        # Return empty dictionary if it's empty
        return metrics

    wo_type = list(itertools.chain.from_iterable(metrics.values()))
    wo_type_flattened = list(itertools.chain([flatten(d) for d in wo_type]))
    wo_type_joined = multi_dict_unpacking(wo_type_flattened)
    return wo_type_joined


class ClipperCollector(object):
    def __init__(self):
        pass

    def collect(self):
        metrics = parse_metric(load_metric())

        for name, val in metrics.items():
            try:
                if '.' or 'e' in val:
                    val = float(val)
                else:
                    val = int(val)
                name = name.replace(':', '_').replace('-', '_')
                yield GaugeMetricFamily(name, 'help', value=val)
            except ValueError:
                pass


if __name__ == '__main__':
    REGISTRY.register(ClipperCollector())
    start_http_server(1390)
    while True:
        time.sleep(1)
