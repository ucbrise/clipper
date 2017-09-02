from __future__ import print_function, with_statement, absolute_import

import logging
from .cloudpickle import CloudPickler
from .module_dependency import ModuleDependencyAnalyzer
from ..clipper_admin import CLIPPER_TEMP_DIR
import six
import os
import sys
import shutil
import tempfile
cur_dir = os.path.dirname(os.path.abspath(__file__))
if sys.version < '3':
    import subprocess32 as subprocess
    PY3 = False
else:
    import subprocess
    PY3 = True

CONTAINER_CONDA_PLATFORM = 'linux-64'

logger = logging.getLogger(__name__)


def save_python_function(name, func):
    predict_fname = "func.pkl"
    environment_fname = "environment.yml"
    conda_dep_fname = "conda_dependencies.txt"
    pip_dep_fname = "pip_dependencies.txt"
    local_modules_folder_name = "modules"

    # Serialize function
    s = six.StringIO()
    c = CloudPickler(s, 2)
    c.dump(func)
    serialized_prediction_function = s.getvalue()

    # Set up serialization directory
    if not os.path.exists(CLIPPER_TEMP_DIR):
        os.makedirs(CLIPPER_TEMP_DIR)
    serialization_dir = tempfile.mkdtemp(dir=CLIPPER_TEMP_DIR)
    logger.info("Saving function to {}".format(serialization_dir))

    # Export Anaconda environment
    environment_file_abs_path = os.path.join(serialization_dir,
                                             environment_fname)

    conda_env_exported = export_conda_env(environment_file_abs_path)

    if conda_env_exported:
        logger.info("Anaconda environment found. Verifying packages.")

        # Confirm that packages installed through conda are solvable
        # Write out conda and pip dependency files to be supplied to container
        if not (check_and_write_dependencies(
                environment_file_abs_path, serialization_dir, conda_dep_fname,
                pip_dep_fname)):
            return False

        logger.info("Supplied environment details")
    else:
        logger.info(
            "Warning: Anaconda environment was either not found or exporting the environment "
            "failed. Your function will still be serialized and deployed, but may fail due to "
            "missing dependencies. In this case, please re-run inside an Anaconda environment. "
            "See http://clipper.ai/documentation/python_model_deployment/ for more information."
        )

    # Export modules used by predict_function not captured in anaconda or pip
    export_local_modules(c.modules, serialization_dir, conda_dep_fname,
                         pip_dep_fname, local_modules_folder_name)
    logger.info("Supplied local modules")

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


def check_and_write_dependencies(environment_path, directory, conda_dep_fname,
                                 pip_dep_fname):
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
         "{conda_dep_fname} {pip_dep_fname}").format(
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


def get_already_exported_modules(serialization_dir, conda_deps_fname,
                                 pip_deps_fname):
    """
    Returns a list of names of modules that will be installed on the container.
    This list includes the names of exported conda and pip modules as well as
    those of preinstalled modules.

    Returns
    -------
    list
        List of names of modules whose installations are already accounted for
    """
    conda_deps_abs_path = os.path.join(serialization_dir, conda_deps_fname)
    pip_deps_abs_path = os.path.join(serialization_dir, pip_deps_fname)
    python_container_conda_deps_fname = os.path.abspath(
        os.path.join(cur_dir, "..", "python_container_conda_deps.txt"))

    def get_module_name(line):
        return line.strip().split('=')[1]

    ignore_list = []

    if (os.path.isfile(conda_deps_abs_path)):
        with open(conda_deps_abs_path, 'r') as f:
            for line in f:
                ignore_list.append(get_module_name(line))

    if (os.path.isfile(pip_deps_fname)):
        with open(pip_deps_abs_path, 'r') as f:
            for line in f:
                ignore_list.append(get_module_name(line))

    with open(python_container_conda_deps_fname, 'r') as f:
        for line in f:
            ignore_list.append(line.strip())

    return ignore_list


def export_local_modules(func_modules, serialization_dir, conda_deps_fname,
                         pip_deps_fname, local_modules_folder_name):
    """
    Supplies modules in `func_modules` not already accounted for by Anaconda or pip to container.
    Copies those modules to {`serialization_dir`}/{`local_module_deps_fname`}.

    Parameters
    ----------
    func_modules : str
        The modules to consider exporting
    serialization_dir : str
        The path to the diretory containing the conda, pip, and output local modules dependency
        information
    conda_deps_fname : str
        The name of the conda dependency file
    pip_deps_fname : str
        The name of the pip dependency file
    local_modules_folder_name : str
        The name of the folder to copy modules to
    """
    ignore_list = get_already_exported_modules(
        serialization_dir, conda_deps_fname, pip_deps_fname)
    paths_to_copy = set()

    mda = ModuleDependencyAnalyzer()
    for module_name in ignore_list:
        mda.ignore(module_name)
    for module in func_modules:
        module_name = module.__name__
        if module_name not in ignore_list and 'clipper_admin' not in module_name:
            mda.add(module_name)

            if '.' in module_name:
                module_name_split = module_name.split()
                try:
                    file_location = module.__file__
                    package_nested_layer = len(module_name.split('.')) - 1
                    folder_indices = [
                        i for i, x in enumerate(file_location) if x == "/"
                    ]
                    cutoff_index = folder_indices[-package_nested_layer]
                    package_path = file_location[:cutoff_index]
                    paths_to_copy.add(package_path)
                except Exception as e:
                    logger.warning(
                        "Could not identify the location of the root package {package} for module {module}. Will not supply it to the container.".
                        format(
                            package=module_name_split[0], module=module_name))
        else:
            mda.ignore(module_name)
    module_paths = mda.get_and_clear_paths()

    def path_already_captured(path):
        for existing_path in paths_to_copy:
            if path.startswith(existing_path):
                return True
        return False

    for module_path in module_paths:
        if not path_already_captured(module_path):
            paths_to_copy.add(module_path)

    modules_dir = os.path.join(serialization_dir, local_modules_folder_name)
    if not os.path.exists(modules_dir):
        os.mkdir(modules_dir)
    for path in paths_to_copy:
        module_fname = os.path.basename(path)
        dst = os.path.join(modules_dir, module_fname)
        try:
            if os.path.isfile(path):
                shutil.copyfile(path, dst)
            elif os.path.isdir(path):
                shutil.copytree(path, dst)
        except Exception as e:
            logger.warning(
                "Encountered an error copying {path}. Will not supply it to container.".
                format(path=path))
