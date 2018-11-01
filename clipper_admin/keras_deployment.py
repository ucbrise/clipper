from clipper_admin import ClipperConnection, DockerContainerManager
from clipper_admin.deployers import keras as keras_deployer
import keras

clipper_conn = ClipperConnection(DockerContainerManager())
clipper_conn.connect()  # if cluster exists

inpt = keras.layers.Input(shape=(1,))
out = keras.layers.multiply([inpt, inpt])
model = keras.models.Model(inputs=inpt, outputs=out)
#pred = model.predict([1, 2, 3, 5])
#print(pred)


# docker build -f dockerfiles/KerasDockerfile -t keras-container .
def predict(model, inputs):
    return [model.predict(x) for x in inputs]

try:
    clipper_conn.delete_application(name="keras-pow")
except:
    pass
clipper_conn.register_application(name="keras-pow", input_type="ints", default_output="-1.0", slo_micros=1000000)

try:
    clipper_conn.stop_models('pow')
except:
    pass
keras_deployer.deploy_keras_model(clipper_conn=clipper_conn, name="pow", version="1", input_type="ints",
                                  func=predict,
                                  model_path_or_object=model,
                                  base_image='keras-container')

clipper_conn.link_model_to_app(app_name="keras-pow", model_name="pow")
