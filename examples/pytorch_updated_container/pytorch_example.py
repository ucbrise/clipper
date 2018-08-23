from clipper_admin import ClipperConnection, DockerContainerManager
from clipper_admin.deployers import pytorch as pytorch_deployer
import torch
import logging
import requests
import json

logger = logging.getLogger(__name__)


# Define prediction functions
def pyt_pred(model, inputs):
    model.eval()
    inputs = torch.tensor(inputs)
    preds = model(inputs)
    return [str(p) for p in preds]


clipper_conn = ClipperConnection(DockerContainerManager())
clipper_conn.start_clipper()

clipper_conn.register_application(name="pyt.app", input_type="float", default_output="-1.0", slo_micros=100000)
model = torch.load('pyt_model')
pytorch_deployer.deploy_pytorch_model(clipper_conn, name="pyt",
                                      version="1", input_type="float",
                                      func=pyt_pred, pytorch_model=model)
clipper_conn.link_model_to_app(app_name="pyt.app", model_name="pyt")

headers = {"Content-type": "application/json"}
single_input = [0.0,0.1,0.0,0.1]
r = requests.post("http://localhost:1337/pyt.app/predict",
                  headers=headers,
                  data=json.dumps({"input": single_input})).json()
print(r)
clipper_conn.stop_all()