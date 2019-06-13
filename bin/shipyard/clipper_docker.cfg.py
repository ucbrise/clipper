import shlex
from distutils.version import LooseVersion
from functools import partial
from itertools import product

from shipyard import Action, CIPrettyLogAction, IsolatedAction, ctx


#####################################
# Travis: Wait and pull/push images #
#####################################
def wait_and_pull_cmd(image_name):
    return f"until docker pull {image_name}; do sleep 5; done"


def wait_and_push_cmd(image_name):
    return f"until docker push {image_name}; do sleep 5; done"


def create_image_with_context(build_ctx, image, dockerfile, rpc_version=None):
    if rpc_version is None:
        rpc_version = ""
    else:
        rpc_version = f"--build-arg RPC_VERSION={rpc_version}"

    namespace = build_ctx["namespace"]
    sha_tag = build_ctx["sha_tag"]

    docker_build_str = f"docker build --build-arg CODE_VERSION={sha_tag} \
            --build-arg REGISTRY={namespace} {rpc_version} \
            -t {namespace}/{image}:{sha_tag} \
            -f dockerfiles/{dockerfile} {build_ctx['clipper_root']} "

    return CIPrettyLogAction(image, docker_build_str, tags=["build"])


def push_image_with_context(build_ctx, image, push_sha=True, push_version=False):
    namespace = build_ctx["namespace"]
    sha_tag = build_ctx["sha_tag"]
    version_tag = build_ctx["version_tag"]

    image_name_sha = f"{namespace}/{image}:{sha_tag}"
    image_name_version = f"{namespace}/{image}:{version_tag}"

    docker_tag = f"docker tag {image_name_sha} {image_name_version}"
    docker_push_sha = wait_and_push_cmd(image_name_sha)
    docker_push_version = wait_and_push_cmd(image_name_version)

    commands = [docker_tag]
    if push_sha:
        commands.append(docker_push_sha)
    if push_version and ctx["push"]:
        commands.append(docker_push_version)

        version = LooseVersion(version_tag).version
        if len(version) >= 3:
            minor_version = ".".join(version[:2])
            image_name_minor_version = f"{namespace}/{image}:{minor_version}"

            tag_minor_ver = f"docker tag {image_name_sha} {image_name_minor_version}"
            push_minor_ver = wait_and_push_cmd(image_name_minor_version)
            commands.extend([tag_minor_ver, push_minor_ver])

    return CIPrettyLogAction(f"publish_{image}", "\n".join(commands), tags=["push"])


def create_and_push_with_ctx(
    ctx, name, dockerfile, push_version=False, rpc_version=None
):
    create_image = partial(create_image_with_context, ctx)
    push_image = partial(push_image_with_context, ctx)

    created = create_image(name, dockerfile, rpc_version)
    pushed = push_image(name, push_sha=True, push_version=push_version)

    # Prepull will let docker re-use cached images
    prepull = CIPrettyLogAction(
        f"prepull_{name}", f"docker pull clipper/{name}:develop || true", ["prepull"]
    )
    prepull > created
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
py36_dev = create_and_push_with_ctx(
    ctx, "py36-dev", "ClipperPy36DevDockerfile ", push_version=True
)
# py37_dev = create_and_push_with_ctx(
#     ctx, "py37-dev", "ClipperPy37DevDockerfile ", push_version=True
# )

unittests = create_and_push_with_ctx(
    ctx, "unittests", "ClipperTestsDockerfile ", push_version=False
)
py36tests = create_and_push_with_ctx(
    ctx, "py36tests", "ClipperPy36TestsDockerfile ", push_version=False
)
# py37tests = create_and_push_with_ctx(
#     ctx, "py37tests", "ClipperPy37TestsDockerfile ", push_version=False
# )

lib_base > query_frontend
lib_base > management_frontend
lib_base > dev
lib_base > py36_dev
# lib_base > py37_dev
dev > unittests
py36_dev > py36tests
# py37_dev > py37tests

######################
# Misc Container DAG #
######################

# Deprecate JVM Container!
# create_and_push_with_ctx(
#     ctx, "spark-scala-container", "SparkScalaContainerDockerfile", push_version=True
# )

# create_and_push_with_ctx(
#     ctx, "r-container-base", "RContainerDockerfile", push_version=True
# )
frontend_exporter = create_and_push_with_ctx(
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
py37_rpc = create_and_push_with_ctx(
    ctx, "py37-rpc", "Py37RPCDockerfile", rpc_version="py37", push_version=True
)

# Will be used for model containers building
rpc_containers = {"py": py_rpc, "py35": py35_rpc, "py36": py36_rpc, "py37": py37_rpc}


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
    ("keras{version}", "KerasContainer")
]
py_version = [("", "py"), ("35", "py35"), ("36", "py36"), ("37", "py37")]

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

##############################
# Kubernetes Test Dependency #
##############################
kubernetes_test_target = IsolatedAction("kubernetes_test_containers")
kubernetes_containers = [
    query_frontend.name,
    management_frontend.name,
    frontend_exporter.name,
    "noop-container",
    "python-closure-container",  # travis has py2.7
]

for container in kubernetes_containers:
    Action.get_action(container) > kubernetes_test_target
    Action.get_action(f"publish_{container}") > kubernetes_test_target


for container in kubernetes_containers:
    wait = IsolatedAction(
        f"wait_{container}",
        wait_and_pull_cmd(f'{ctx["namespace"]}/{container}:{ctx["sha_tag"]}'),
        tags="wait_for_kubernetes_test_containers",
    )

    tag = IsolatedAction(
        f"travis_re_tag_{container}",
        f'docker tag {ctx["namespace"]}/{container}:{ctx["sha_tag"]} localhost:5000/{container}:{ctx["sha_tag"]}',
        tags="retag_kubernetes_test_containers",
    )
    wait > tag

    push = IsolatedAction(
        f"travis_re_push_{container}",
        f'docker push localhost:5000/{container}:{ctx["sha_tag"]}',
        tags="repush_kubernetes_test_containers",
    )
    tag > push
