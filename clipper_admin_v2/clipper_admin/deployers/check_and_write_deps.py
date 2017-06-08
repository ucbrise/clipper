from conda.api import get_index
from conda.base.context import context
from conda.exceptions import UnsatisfiableError, NoPackagesFoundError
import conda.resolve
import conda_env.specs as specs
import sys
import os


def _extract_conda_dependencies_without_channel_prefixes(dependencies):
    conda_deps = []
    conda_deps_with_channel = []
    for dependency in dependencies:
        if "::" in dependency:
            conda_deps_with_channel.append(dependency)
        else:
            conda_deps.append(dependency)
    for p in conda_deps_with_channel:
        p_no_channel = p.split("::")[1]
        conda_deps.append(p_no_channel)
    return conda_deps


def _write_out_dependencies(directory, conda_dep_fname, pip_dep_fname,
                            conda_deps, pip_deps):
    conda_dep_abs_path = os.path.join(directory, conda_dep_fname)
    pip_dep_abs_path = os.path.join(directory, pip_dep_fname)

    with open(conda_dep_abs_path, 'w') as f:
        for item in conda_deps:
            f.write("%s\n" % '='.join(item.split()))

    with open(pip_dep_abs_path, 'w') as f:
        for item in pip_deps:
            f.write("%s\n" % item)


def check_solvability_write_deps(env_path, directory, platform,
                                 conda_dep_fname, pip_dep_fname):
    """Returns true if the provided conda environment is compatible with the container os.

    If packages listed in specified conda environment file have conflicting dependencies,
    this function will warn the user and return False.

    If there are no conflicting package dependencies, existence of the packages in channels
    accessible to `platform` is tested. The user is warned about any missing packages.
    All existing conda packages are written out to `conda_dep_fname` and pip packages
    to `pip_dep_fname` in the given `directory`. This function then returns True.

    Parameters
    ----------
    env_path : str
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

    index = get_index(platform=platform)
    r = conda.resolve.Resolve(index)
    spec = specs.detect(filename=env_path)
    env = spec.environment
    dependency_details = env.dependencies.items()
    pip_deps = []
    conda_deps = []

    for distribution, dependencies in dependency_details:
        if distribution == 'conda':
            conda_deps = _extract_conda_dependencies_without_channel_prefixes(
                dependencies)
        elif distribution == 'pip':
            pip_deps = dependencies

    # Check for conflicts and existence of packages
    missing_packages = None
    try:
        try:
            # This call doesn't install anything; it checks the solvability of package dependencies.
            r.install(conda_deps)
        except NoPackagesFoundError as missing_packages_error:
            missing_packages = missing_packages_error.pkgs
            for package in missing_packages:
                conda_deps.remove(package)

            # Check that the dependencies that are not missing are satisfiable
            r.install(conda_deps)
    except UnsatisfiableError as unsat_e:
        print(
            "Your conda dependencies are unsatisfiable (see error text below). Please resolve these issues and call `deploy_predict_func` again."
        )
        print(unsat_e)
        return False

    if missing_packages is not None:
        print(
            "The following packages in your conda environment aren't available in the linux-64 conda channel the container will use:"
        )
        print(", ".join(str(package) for package in missing_packages))
        print(
            "We will skip their installation when deploying your function. If your function uses these packages, the container will experience a runtime error when queried."
        )

    _write_out_dependencies(directory, conda_dep_fname, pip_dep_fname,
                            conda_deps, pip_deps)
    return True


if __name__ == '__main__':
    if len(sys.argv) != 6:
        print(
            "Usage: ./check_env.py <env_path> <directory> <platform> <conda_dep_fname> <pip_dep_fname>"
        )
        sys.exit(1)
    _, env_path, directory, platform, conda_dep_fname, pip_dep_fname = sys.argv
    if check_solvability_write_deps(env_path, directory, platform,
                                    conda_dep_fname, pip_dep_fname):
        sys.exit(0)
    sys.exit(1)
