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
    "keras": "python /clipper/integration-tests/deploy_keras_models.py",
    "multi_tenancy": "python /clipper/integration-tests/multi_tenancy_test.py",
    # "rclipper": "/clipper/integration-tests/r_integration_test/rclipper_test.sh",
    "docker_metric": "python /clipper/integration-tests/clipper_metric_docker.py",
    "fluentd": "python /clipper/integration-tests/clipper_fluentd_logging_docker.py"
}

NUM_RETRIES = 2
NUM_RETRIES_BASH = "{1.." + str(NUM_RETRIES) + "}"


def generate_test_command(python_version, test_to_run):
    assert python_version in ["2.7", "3.6", "3.7"]

    if python_version == "2.7":
        image = "unittests"
    elif python_version == "3.6":
        image = "py36tests"
    else:
        image = "py37tests"

    # CLIPPER_TESTING_DOCKERHUB_PASSWORD should be already in the environment
    command = f"""
    python ./bin/retry_with_timeout.py --retry 1 --timeout 40m -- \
    docker run --rm --network=host -v /var/run/docker.sock:/var/run/docker.sock -v /tmp:/tmp \
        -e CLIPPER_REGISTRY={ctx['namespace']} \
        -e CLIPPER_TESTING_DOCKERHUB_PASSWORD=$CLIPPER_TESTING_DOCKERHUB_PASSWORD \
        {ctx['namespace']}/{image}:{ctx['sha_tag']} \
        \"{test_to_run}\" 
    """.strip(
        "\n"
    )

    return command


# Create make targets for both
for name, test_to_run in UNITTESTS.items():
    CIPrettyLogAction(
        name=f"unittest_py2_{name}",
        command=generate_test_command("2.7", test_to_run),
        tags="unittest_py2",
    )

    CIPrettyLogAction(
        name=f"unittest_py36_{name}",
        command=generate_test_command("3.6", test_to_run),
        tags="unittest_py36",
    )

    CIPrettyLogAction(
        name=f"unittest_py37_{name}",
        command=generate_test_command("3.6", test_to_run),
        tags="unittest_py37",
    )

for name, test_to_run in DOCKER_INTEGRATION_TESTS.items():
    CIPrettyLogAction(
        name=f"integration_py2_{name}",
        command=generate_test_command("2.7", test_to_run),
        tags="integration_py2",
    )

    CIPrettyLogAction(
        name=f"integration_py36_{name}",
        command=generate_test_command("3.6", test_to_run),
        tags="integration_py36",
    )

    CIPrettyLogAction(
        name=f"integration_py37_{name}",
        command=generate_test_command("3.7", test_to_run),
        tags="integration_py37",
    )

# Specify specific dependencies
# TODO(simon): these tests should have hierachies.
