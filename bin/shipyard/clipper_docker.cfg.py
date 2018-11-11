from shipyard import create_and_push_with_ctx, print_make_all, ctx
from itertools import product

######################
# Lib Base Build DAG #
######################
lib_base = create_and_push_with_ctx(
    ctx, "lib_base", "ClipperLibBaseDockerfile", push_version=False
)

query_frontend = create_and_push_with_ctx(
    ctx, "query_frontend", "QueryFrontendDockerfile", push_version=True
)
management_frontend = create_and_push_with_ctx(
    ctx, "management_frontend", "ManagementFrontendDockerfile", push_version=True
)

dev = create_and_push_with_ctx(ctx, "dev", "ClipperDevDockerfile ", push_version=True)
py35_dev = create_and_push_with_ctx(
    ctx, "py35-dev", "ClipperPy35DevDockerfile ", push_version=True
)

unittests = create_and_push_with_ctx(
    ctx, "unittests", "ClipperTestsDockerfile ", push_version=False
)
py35tests = create_and_push_with_ctx(
    ctx, "py35tests", "ClipperPy35TestsDockerfile ", push_version=False
)

lib_base > query_frontend
lib_base > management_frontend
lib_base > dev
lib_base > py35_dev
dev > unittests
py35_dev > py35tests

######################
# Misc Container DAG #
######################

# Deprecate Spark Container!
# create_and_push_with_ctx(
#     ctx, "spark-scala-container", "SparkScalaContainerDockerfile", push_version=True
# )

create_and_push_with_ctx(
    ctx, "r-container-base", "RContainerDockerfile", push_version=True
)
create_and_push_with_ctx(
    ctx, "frontend-exporter", "FrontendExporterDockerfile", push_version=True
)

##################
# RPC Containers #
##################
py_rpc = create_and_push_with_ctx(ctx, "py-rpc", "Py2RPCDockerfile", rpc_version="py", push_version=True)
py35_rpc = create_and_push_with_ctx(
    ctx, "py35-rpc", "Py35RPCDockerfile",  rpc_version="py35", push_version=True
)
py36_rpc = create_and_push_with_ctx(
    ctx, "py36-rpc", "Py36RPCDockerfile",  rpc_version="py36", push_version=True
)

# Will be used for model containers building
rpc_containers = {
    'py': py_rpc,
    'py35': py35_rpc,
    'py36': py36_rpc
}


py_rpc > create_and_push_with_ctx(
    ctx, "sum-container", "SumDockerfile ", push_version=False
)
py_rpc > create_and_push_with_ctx(
    ctx, "noop-container", "NoopDockerfile", push_version=True
)

####################
# Model Containers #
####################
models = [
    ("mxnet{version}", "MXNetContainer"),
    ("pytorch{version}", "PyTorchContainer"),
    ("tf{version}", "TensorFlow"),
    ("pyspark{version}", "PySparkContainer"),
    ("python{version}-closure", "PyClosureContainer")
]
py_version = [("", "py"), ("35", "py35"), ("36", "py36")]

for (model_name, docker_file), (py_version_name, rpc_version) in product(
    models, py_version
):
    container = create_and_push_with_ctx(
        ctx,
        name=f"{model_name.format(version=py_version_name)}-container",
        dockerfile=f"{docker_file}Dockerfile",
        rpc_version=rpc_version,
        push_version=True,
    )
    # link dependency
    rpc_containers[rpc_version] > container





