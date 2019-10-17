from __future__ import absolute_import, division, print_function
from ..container_manager import (
    create_model_container_label, ContainerManager, CLIPPER_DOCKER_LABEL,
    CLIPPER_MODEL_CONTAINER_LABEL, CLIPPER_INTERNAL_RPC_PORT,
    CLIPPER_INTERNAL_MANAGEMENT_PORT, CLIPPER_INTERNAL_QUERY_PORT,
    CLIPPER_INTERNAL_METRIC_PORT, CLIPPER_NAME_LABEL, ClusterAdapter)
from ..exceptions import ClipperException

from contextlib import contextmanager
import nomad

from .redis_deployment import redis_deployment
from .model_deployment import model_deployment, generate_model_job_name, model_job_prefix, model_check_name
from .mgmt_deployment import mgmt_deployment
from .query_frontend_deployment import query_frontend_deployment

from .utils import nomad_job_prefix, query_frontend_job_prefix, query_frontend_service_check, query_frontend_rpc_check
from .utils import mgmt_job_prefix, mgmt_check
from .utils import redis_job_prefix, redis_check

from dns.resolver import NXDOMAIN

import logging
import json
import yaml
import os
import time
import jinja2
from jinja2.exceptions import TemplateNotFound

logger = logging.getLogger(__name__)
cur_dir = os.path.dirname(os.path.abspath(__file__))


@contextmanager
def _pass_conflicts():
    try:
        yield
    except ApiException as e:
        body = json.loads(e.body)
        if body['reason'] == 'AlreadyExists':
            logger.info("{} already exists, skipping!".format(body['details']))
            pass
        else:
            raise e


class NomadContainerManager(ContainerManager):
    def __init__(self,
                 nomad_ip,
                 dns,
                 load_balancer=None,
                 cluster_name="default-cluster",
                 datacenters=["dc1"],
                 redis_ip=None,
                 redis_port=6379,
                 namespace=None,
                 create_namespace_if_not_exists=False
                 ):
        """

        Parameters
        ----------
        nomad_ip: str
            The ip of Nomad
        dns: DNS
            The DNS service that you used with Nomad. Consul is the most popular option.
        load_balancer: str 
            The Load Balancer used with Nomad. If you dont have one or dont want to use it, leave it None
        cluster_name: str
            A unique name for this Clipper cluster. This can be used to run multiple Clipper
            clusters on the same Kubernetes cluster without interfering with each other.
            Kubernetes cluster name must follow Kubernetes label value naming rule, namely:
            Valid label values must be 63 characters or less and must be empty or begin and end with
            an alphanumeric character ([a-z0-9A-Z]) with dashes (-), underscores (_), dots (.),
            and alphanumerics between. See more at:
            https://kubernetes.io/docs/concepts/overview/working-with-objects/labels/#syntax-and-character-set
        datacenter: str, optional
            The name of your Nomad datacenter
        redis_ip : str, optional
            The address of a running Redis cluster. If set to None, Clipper will start
            a Redis deployment for you.
        redis_port : int, optional
            The Redis port. If ``redis_ip`` is set to None, Clipper will start Redis on this port.
            If ``redis_ip`` is provided, Clipper will connect to Redis on this port.
        namespace: str, optional
            The Nomad namespace to use .
            If this argument is provided, all Clipper artifacts and resources will be created in this
            Nomad namespace. If not, the default namespace is used.
        create_namespace_if_not_exists: bool, False
            Create a Nomad namespace if the namespace doesnt already exist. This feature is only in Enterprise Edition.
            If this argument is provided and the Nomad namespace does not exist a new Nomad namespace will
            be created.

        Note
        ----
        Clipper stores all persistent configuration state (such as application and model
        information) in Redis. If you want Clipper to be durable and able to recover from failures,
        we recommend configuring your own persistent and replicated Redis cluster rather than
        letting Clipper launch one for you.
        """

        self.cluster_name = cluster_name

        self.dns = dns
        self.load_balancer = load_balancer
        self.datacenters = datacenters

        self.redis_ip = redis_ip
        self.redis_port = redis_port

        self.query_ip = None
        self.query_port = None

        # connect to nomad cluster
        self.nomad = nomad.Nomad(host=nomad_ip, timeout=5)
        namespaces = []

        if namespace in namespaces:
            pass
        elif namespace is not None and create_namespace_if_not_exists:
            pass

        self.cluster_name = cluster_name 
        self.cluster_identifier = "{cluster}".format(cluster=self.cluster_name)
        self.logger = ClusterAdapter(logger, {
            'cluster_name': self.cluster_identifier
        })

    def start_clipper(self,
                      query_frontend_image,
                      mgmt_frontend_image,
                      frontend_exporter_image,
                      cache_size,
                      qf_http_thread_pool_size,
                      qf_http_timeout_request,
                      qf_http_timeout_content,
                      num_frontend_replicas=1):
        self._start_redis()
        self._start_mgmt(mgmt_frontend_image, num_replicas=1)
        self.num_frontend_replicas = num_frontend_replicas
        self._start_query(query_frontend_image, frontend_exporter_image,
                          cache_size, qf_http_thread_pool_size,
                          qf_http_timeout_request, qf_http_timeout_content,
                          num_frontend_replicas)
        #self._start_prometheus()
        #self.connect()

    def _start_redis(self, sleep_time=5):
        # If an existing Redis service isn't provided, start one
        if self.redis_ip is None:
            job_id = redis_job_prefix(self.cluster_name)
            self.nomad.job.register_job(job_id, redis_deployment(job_id, self.datacenters, self.cluster_name))


            # Wait for max 10 minutes
            wait_count = 0
            redis_ip = None
            redis_port = None
            check_name = redis_check(self.cluster_name)

            while redis_ip is None:
                time.sleep(3)
                wait_count += 3
                if wait_count > 600:
                    raise ClipperException(
                        "Could not create a Nomad deployment: {}".format(job_id))
                try:
                    redis_ip, redis_port = self.dns.resolveSRV(check_name)
                    self.logger.info('Redis is at {}:{}'.format(redis_ip, redis_port))
                except NXDOMAIN as err:
                    self.logger.warning('DNS query failed: {}'.format(err))

            self.redis_ip = redis_ip
            self.redis_port = redis_port

    def _start_mgmt(self, mgmt_image, num_replicas):
        job_id = mgmt_job_prefix(self.cluster_name),
        self.nomad.job.register_job(
            job_id, 
            mgmt_deployment(
                job_id, 
                self.datacenters, 
                self.cluster_name, 
                mgmt_image, 
                self.redis_ip, 
                self.redis_port, 
                num_replicas
            )
        )

        wait_count = 0

        mgmt_ip = None
        mgmt_port = None

        check_name = mgmt_check(self.cluster_name)

        while mgmt_ip is None:
            time.sleep(3)
            wait_count += 3
            if wait_count > 600:
                raise ClipperException(
                    "Could not create a Nomad deployment: {}".format(job_id))
            try:
                mgmt_ip, mgmt_port = self.dns.resolveSRV(check_name)
                self.logger.info('Clipper mgmt is at {}:{}'.format(mgmt_ip, mgmt_port))
            except NXDOMAIN as err:
                self.logger.warning('DNS query failed: {}'.format(err))
        self.mgmt_ip = mgmt_ip 
        self.mgmt_port = mgmt_port 

    def _start_query(self, query_image, frontend_exporter_image, cache_size,
                     qf_http_thread_pool_size, qf_http_timeout_request,
                     qf_http_timeout_content, num_replicas):
        job_id = query_frontend_job_prefix(self.cluster_name)
        self.nomad.job.register_job(
            job_id, 
            query_frontend_deployment(
                job_id,
                self.datacenters, 
                self.cluster_name, 
                query_image,
                self.redis_ip, 
                self.redis_port,
                num_replicas,
                cache_size,
                qf_http_thread_pool_size,
                qf_http_timeout_request,
                qf_http_timeout_content
            )
        )

        # Wait for max 10 minutes
        wait_count = 0
        query_ip = None
        query_port = None

        while query_ip is None:
            time.sleep(3)
            wait_count += 3
            if wait_count > 600:
                raise ClipperException(
                    "Could not create a Nomad deployment: {}".format(job_id))
            try:
                query_ip, query_port = self._resolve_query_ip()
                self.logger.info('Clipper query is at {}:{}'.format(query_ip, query_port))
            except NXDOMAIN as err:
                self.logger.warning('DNS query failed: {}'.format(err))
        self.query_ip = query_ip
        self.query_port = query_port

    def _resolve_query_ip(self):
        return self._resolve_query_frontend_service()

    """
        This function queries the DNS server with a SRV request to get ip and port for the query frontend service (REST API)
    """
    def _resolve_query_frontend_service(self):
        check_name = query_frontend_service_check(self.cluster_name)
        query_ip, query_port = self.dns.resolveSRV(check_name)
        return (query_ip, query_port)

    """
        This function queries the DNS server with a SRV request to get ip and port for the query frontend rpc
    """
    def _resolve_query_frontend_rpc(self):
        check_name = query_frontend_rpc_check(self.cluster_name)
        query_ip, query_port = self.dns.resolveSRV(check_name)
        return (query_ip, query_port)

    def _start_prometheus(self):
        pass

    def connect(self):
       pass 

    def deploy_model(self, name, version, input_type, image, num_replicas=1):
        job_id = generate_model_job_name(self.cluster_name, name, version)

        if self.load_balancer != None:
            query_frontend_ip = self.load_balancer.ip
            query_frontend_port = self.load_balancer.port
        else:
            self.logger.warning('''
                You did not set a load balancer, this is potentially dangerous because ip and ports may change over time 
                and not be updated on the model sides, prefer using a load balancer like Fabio
            ''')
            query_frontend_ip, query_frontend_port = self._resolve_query_frontend_rpc()

        self.nomad.job.register_job(
            job_id, 
            model_deployment(
                job_id, 
                self.datacenters, 
                self.cluster_name, 
                name, 
                version, 
                input_type, 
                image, 
                num_replicas,
                query_frontend_ip,
                query_frontend_port

            )
        )

        # Wait for max 10 minutes
        wait_count = 0
        model_ip = None
        model_port = None
        while model_ip is None:
            time.sleep(3)
            wait_count += 3
            if wait_count > 600:
                raise ClipperException(
                    "Could not create a Nomad deployment: {}".format(job_id))
            try:
                model_ip, model_port = self.dns.resolveSRV(check_name)
                self.logger.info('Clipper model is at {}:{}'.format(model_ip, model_port))
            except NXDOMAIN as err:
                self.logger.warning('DNS query failed: {}'.format(err))

    def get_num_replicas(self, name, version):
        self.logger.warning('get_num_replicasis not supported with Nomad')
        return 0 

    def set_num_replicas(self, name, version, input_type, image, num_replicas):
        self.logger.warning('set_num_replicas is not supported with Nomad')

    def get_logs(self, logging_dir):
        self.logger.warning('get_logs is not supported with Nomad')

    def stop_models(self, models):
        # Stops all deployments of containers running Clipper models with the specified
        # names and versions.
        try:
            for m in models:
                for v in models[m]:
                    job_name = generate_model_job_name(self.cluster_name, m, v)
                    self.nomad.job.deregister_job(job_name)
        except Exception as e:
            self.logger.warning(
                "Exception deleting Nomad deployments: {}".format(e))
            raise e

    def stop_all_model_containers(self):
        print('model job prefix {}', model_job_prefix(self.cluster_name))
        jobs = self.nomad.jobs.get_jobs(prefix=model_job_prefix(self.cluster_name))
        print('jobs: {}', jobs)
        for job in jobs:
            self.logger.warning('nomad job below')
            self.logger.warning(job)
            self.nomad.job.deregister_job(job['Name'])

    def stop_all(self, graceful=True):
        self.logger.info("Stopping all running Clipper resources")
        jobs = self.nomad.jobs.get_jobs(prefix=nomad_job_prefix(self.cluster_name))

        for job in jobs:
            self.logger.warning('nomad job below')
            self.logger.warning(job)
            self.nomad.job.deregister_job(job['Name'])


    def get_admin_addr(self):
        check_name = mgmt_check(self.cluster_name)
        mgmt_ip, mgmt_port = self.dns.resolveSRV(check_name)
        return '{}:{}'.format(mgmt_ip, mgmt_port)

    def get_query_addr(self):
        check_name = query_frontend_service_check(self.cluster_name)
        try:
            query_ip, query_port= self.dns.resolveSRV(check_name)
            self.query_ip = query_ip
            self.query_port = query_port
            return '{}:{}'.format(query_ip, query_port)
        except NXDOMAIN:
            return ''


    def get_metric_addr(self):
        self.logger.warning("get_metric_addr is not supported with Nomad")


def get_model_deployment_name(name, version, query_frontend_id, cluster_name):
    return "{name}-{version}-deployment-at-{query_frontend_id}-at-{cluster_name}".format(
        name=name,
        version=version,
        query_frontend_id=query_frontend_id,
        cluster_name=cluster_name)
