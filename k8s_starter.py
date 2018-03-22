from clipper_admin import ClipperConnection, KubernetesContainerManager
from clipper_admin.deployers import python as python_deployer
import requests, json, numpy as np

clipper_conn = ClipperConnection(
    KubernetesContainerManager("https://api.chester-dev.clipper-k8s-dev.com"))
try:

    clipper_conn.stop_all()
    clipper_conn.stop_all_model_containers()
    clipper_conn.start_clipper()
    clipper_conn.register_application(
        name="hello-world",
        input_type="doubles",
        default_output="-1.0",
        slo_micros=100000)
    clipper_conn.get_all_apps()
    addr = clipper_conn.get_query_addr()

    def feature_sum(xs):
        return [str(sum(x)) for x in xs]

    python_deployer.deploy_python_closure(
        clipper_conn,
        name="sum-model",
        version=1,
        input_type="doubles",
        func=feature_sum,
        registry="chesterleung",
        num_replicas=2)
    clipper_conn.link_model_to_app(
        app_name="hello-world", model_name="sum-model")
    headers = {"Content-type": "application/json"}
    print("Predictions:")
    print(requests.post(
        "http://%s/hello-world/predict" % addr,
        headers=headers,
        data=json.dumps({
            "input": list(np.random.random(10))
        })).json())

except Exception as e:
    print(e)
