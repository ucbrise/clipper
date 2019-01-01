import shlex

from shipyard import Action, CIPrettyLogAction, ctx

UNITTESTS = {
    "libclipper": "/clipper/bin/run_unittests.sh --libclipper",
    "management": "/clipper/bin/run_unittests.sh --management",
    "frontend": "/clipper/bin/run_unittests.sh --frontend",
    # JVM and R containers are not maintained.
    # "jvm": "/clipper/bin/run_unittests.sh --jvm-container",
    # "r_container": "/clipper/bin/run_unittests.sh --r-container",
    "rpc_container": "/clipper/bin/run_unittests.sh --rpc-container",
}

DOCKER_INTEGRATION_TESTS = {
    "admin_unit_test": "python /clipper/integration-tests/clipper_admin_tests.py",
    "many_apps_many_models": "python /clipper/integration-tests/many_apps_many_models.py",
    "pyspark": "python /clipper/integration-tests/deploy_pyspark_models.py",
    "pyspark_pipeline": "python /clipper/integration-tests/deploy_pyspark_pipeline_models.py",
    "pysparkml": "python /clipper/integration-tests/deploy_pyspark_sparkml_models.py",
    "tensorflow": "python /clipper/integration-tests/deploy_tensorflow_models.py",
    "mxnet": "python /clipper/integration-tests/deploy_mxnet_models.py",
    "pytorch": "python /clipper/integration-tests/deploy_pytorch_models.py",
    "multi_tenancy": "python /clipper/integration-tests/multi_tenancy_test.py",
    # "rclipper": "/clipper/integration-tests/r_integration_test/rclipper_test.sh",
    "docker_metric": "python /clipper/integration-tests/clipper_metric_docker.py",
}

NUM_RETRIES = 2
NUM_RETRIES_BASH = "{1.." + str(NUM_RETRIES) + "}"


def generate_test_command(python_version, test_to_run):
    assert python_version in [2, 3]

    image = "unittests" if python_version == 2 else "py35tests"

    # CLIPPER_TESTING_DOCKERHUB_PASSWORD should be already in the environment
    # Retry logic comes from
    #   https://unix.stackexchange.com/questions/82598/how-do-i-write-a-retry-logic-in-script-to-keep-retrying-to-run-it-upto-5-times
    command = f"""
    for i in {NUM_RETRIES_BASH}; do  \
    (docker run --rm --network=host -v /var/run/docker.sock:/var/run/docker.sock -v /tmp:/tmp \
        -e CLIPPER_REGISTRY={ctx['namespace']} \
        -e CLIPPER_TESTING_DOCKERHUB_PASSWORD=$CLIPPER_TESTING_DOCKERHUB_PASSWORD \
        {ctx['namespace']}/{image}:{ctx['sha_tag']} \
        \"{test_to_run}\") \
    && break || echo "failed at try $i, retrying";  \
    if [ "$i" -eq "{NUM_RETRIES}" ];  \
        then exit 1; fi; \
    done
    """.strip(
        "\n"
    )

    return command


# Create make targets for both
for name, test_to_run in UNITTESTS.items():
    CIPrettyLogAction(
        name=f"unittest_{name}",
        command=generate_test_command(2, test_to_run),
        tags="unittest",
    )

for name, test_to_run in DOCKER_INTEGRATION_TESTS.items():
    CIPrettyLogAction(
        name=f"integration_py2_{name}",
        command=generate_test_command(2, test_to_run),
        tags="integration",
    )

    CIPrettyLogAction(
        name=f"integration_py3_{name}",
        command=generate_test_command(3, test_to_run),
        tags="integration",
    )

# Specify specific dependencies
# TODO(simon): these tests should have hierachies.
