from __future__ import print_function, with_statement, absolute_import

import logging
from .cloudpickle import CloudPickler
import six
import os
import sys
if sys.version < '3':
    import subprocess32 as subprocess
    PY3 = False
else:
    import subprocess
    PY3 = True

CONTAINER_CONDA_PLATFORM = 'linux-64'


def save_python_function(name, func):
    relative_base_serializations_dir = "python_func_serializations"
    predict_fname = "func.pkl"
    environment_fname = "environment.yml"
    conda_dep_fname = "conda_dependencies.txt"
    pip_dep_fname = "pip_dependencies.txt"

    # Serialize function
    s = six.StringIO()
    c = CloudPickler(s, 2)
    c.dump(func)
    serialized_prediction_function = s.getvalue()

    # Set up serialization directory
    serialization_dir = os.path.join(
        '/tmp', relative_base_serializations_dir, name)
    if not os.path.exists(serialization_dir):
        os.makedirs(serialization_dir)

    # Export Anaconda environment
    environment_file_abs_path = os.path.join(serialization_dir,
                                             environment_fname)

    conda_env_exported = export_conda_env(environment_file_abs_path)

    if conda_env_exported:
        logging.info("Anaconda environment found. Verifying packages.")

        # Confirm that packages installed through conda are solvable
        # Write out conda and pip dependency files to be supplied to container
        if not (check_and_write_dependencies(
                environment_file_abs_path, serialization_dir,
                conda_dep_fname, pip_dep_fname)):
            return False

        logging.info("Supplied environment details")
    else:
        logging.info(
            "Warning: Anaconda environment was either not found or exporting the environment "
            "failed. Your function will still be serialized and deployed, but may fail due to "
            "missing dependencies. In this case, please re-run inside an Anaconda environment. "
            "See http://clipper.ai/documentation/python_model_deployment/ for more information."
        )

    # Write out function serialization
    func_file_path = os.path.join(serialization_dir, predict_fname)
    with open(func_file_path, "w") as serialized_function_file:
        serialized_function_file.write(serialized_prediction_function)
    logging.info("Serialized and supplied predict function")
    return serialization_dir


def export_conda_env(environment_file_abs_path):
    """Returns true if attempt to export the current conda environment is successful

    Parameters
    ----------
    environment_file_abs_path : str
        The desired absolute path for the exported conda environment file
    """

    process = subprocess.Popen(
        "PIP_FORMAT=legacy conda env export >> {environment_file_abs_path}".
        format(environment_file_abs_path=environment_file_abs_path),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        shell=True)
    process.wait()
    return process.returncode == 0


def check_and_write_dependencies(environment_path, directory,
                                 conda_dep_fname, pip_dep_fname):
    """Returns true if the provided conda environment is compatible with the container os.

    If packages listed in specified conda environment file have conflicting dependencies,
    this function will warn the user and return False.

    If there are no conflicting package dependencies, existence of the packages in the
    container conda channel is tested. The user is warned about any missing packages.
    All existing conda packages are written out to `conda_dep_fname` and pip packages
    to `pip_dep_fname` in the given `directory`. This function then returns True.

    Parameters
    ----------
    environment_path : str
        The path to the input conda environment file
    directory : str
        The path to the diretory containing the environment file
    conda_dep_fname : str
        The name of the output conda dependency file
    pip_dep_fname : str
        The name of the output pip dependency file

    Returns
    -------
    bool
        Returns True if the packages specified in `environment_fname` are compatible with conda
        on the container os. Otherwise returns False.
    """
    if "CONDA_PREFIX" not in os.environ:
        logging.info("No Anaconda environment found")
        return False

    root_prefix = os.environ["CONDA_PREFIX"].split("envs")[0]
    py_path = os.path.join(root_prefix, "bin", "python")
    cur_dir = os.path.dirname(os.path.abspath(__file__))
    process = subprocess.Popen(
        ("{py_path} {cur_dir}/check_and_write_deps.py {environment_path} {directory} {platform} "
         "{conda_dep_fname} {pip_dep_fname}").
        format(
            py_path=py_path,
            cur_dir=cur_dir,
            environment_path=environment_path,
            directory=directory,
            platform=CONTAINER_CONDA_PLATFORM,
            conda_dep_fname=conda_dep_fname,
            pip_dep_fname=pip_dep_fname),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        shell=True)
    out, err = process.communicate()
    logging.info(out)
    logging.info(err)
    return process.returncode == 0
