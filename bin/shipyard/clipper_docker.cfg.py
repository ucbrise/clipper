from shipyard import ctx, Action
from itertools import product
from functools import partial
from distutils.version import LooseVersion


def create_image_with_context(build_ctx, image, dockerfile, rpc_version=None):
    if rpc_version is None:
        rpc_version = ""
    else:
        rpc_version = f"--build-arg RPC_VERSION={rpc_version}"

    namespace = build_ctx["namespace"]
    sha_tag = build_ctx["sha_tag"]

    docker_build_str = f"time docker build --build-arg CODE_VERSION={sha_tag} \
            --build-arg REGISTRY={namespace} {rpc_version} \
            -t {namespace}/{image}:{sha_tag} \
            -f dockerfiles/{dockerfile} {build_ctx['clipper_root']} "

    # setup build log redirect
    fluent_bit_exe = ' '.join([
        "docker",
        "run",
        "-it",
        "--rm",
        "fluent/fluent-bit:0.14",
        "/fluent-bit/bin/fluent-bit",
        "-i",
        "stdin",
        "-o",
        "kafka",
        "-p",
        f"brokers={build_ctx['kafka_address']}",
        "-p",
        f"topic=clipper_{build_ctx['sha_tag']}"
    ])
    jq_pipe_transofmer = "jq -R '{log: .} + {container_name: " + f'"{image}"' + "}'"

    docker_build_str += ' | ' + jq_pipe_transofmer + " | " + fluent_bit_exe

    return Action(image, docker_build_str)


def push_image_with_context(build_ctx, image, push_sha=True, push_version=False):
    namespace = build_ctx["namespace"]
    sha_tag = build_ctx["sha_tag"]
    version_tag = build_ctx["version_tag"]

    image_name_sha = f"{namespace}/{image}:{sha_tag}"
    image_name_version = f"{namespace}/{image}:{version_tag}"

    docker_tag = f"docker tag {image_name_sha} {image_name_version}"
    docker_push_sha = f"docker push {image_name_sha}"
    docker_push_version = f"docker push {image_name_version}"

    commands = [docker_tag]
    if push_sha and ctx["push"]:
        commands.append(docker_push_sha)
    if push_version and ctx["push"]:
        commands.append(docker_push_version)

        version = LooseVersion(version_tag).version
        if len(version) >= 3:
            minor_version = ".".join(version[:2])
            image_name_minor_version = f"{namespace}/{image}:{minor_version}"

            tag_minor_ver = f"docker tag {image_name_sha} {image_name_minor_version}"
            push_minor_ver = f"docker push {image_name_minor_version}"
            commands.extend([tag_minor_ver, push_minor_ver])

    return Action(f"publish_{image}", "\n".join(commands))


def create_and_push_with_ctx(
    ctx, name, dockerfile, push_version=False, rpc_version=None
):
    create_image = partial(create_image_with_context, ctx)
    push_image = partial(push_image_with_context, ctx)

    created = create_image(name, dockerfile, rpc_version)
    pushed = push_image(name, push_sha=True, push_version=push_version)

    created > pushed

    return created


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
py_rpc = create_and_push_with_ctx(
    ctx, "py-rpc", "Py2RPCDockerfile", rpc_version="py", push_version=True
)
py35_rpc = create_and_push_with_ctx(
    ctx, "py35-rpc", "Py35RPCDockerfile", rpc_version="py35", push_version=True
)
py36_rpc = create_and_push_with_ctx(
    ctx, "py36-rpc", "Py36RPCDockerfile", rpc_version="py36", push_version=True
)

# Will be used for model containers building
rpc_containers = {"py": py_rpc, "py35": py35_rpc, "py36": py36_rpc}


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
    ("python{version}-closure", "PyClosureContainer"),
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
