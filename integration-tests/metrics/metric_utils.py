import os
import yaml
import logging
import requests

def get_metrics_config():
    cur_dir = os.path.dirname(os.path.abspath(__file__))
    config_path = os.path.join(
        os.path.abspath("%s/../../monitoring" % cur_dir), 'metrics_config.yaml')
    with open(config_path, 'r') as f:
        conf = yaml.load(f)
    return conf


def get_matched_query(query):
    logger = logging.getLogger(__name__)
    logger.info("Querying: {}".format(query))
    res = requests.get(query).json()
    logger.info(res)
    return res


def parse_res_and_assert_node(res, node_num):
    assert res['status'] == 'success'
    assert len(res['data']) == node_num
