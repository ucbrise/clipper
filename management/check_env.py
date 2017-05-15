from conda.api import get_index
from conda.base.context import context
from conda.exceptions import UnsatisfiableError, NoPackagesFoundError
import conda.resolve
import conda_env.specs as specs
import sys


def check_for_conflicts_and_existence(env_fname, directory, platform, conda_dep_fname, pip_dep_fname):
    """Returns true if the specified conda env is compatible with the provided platform.

    If packages listed in specified conda environment file have conflicting dependencies,
    this function will warn the user and return False. If packages don't exist in the
    container's conda channel, this function will warn the user and remove those packages.

    Parameters
    ----------
    environment_fname : str
        The file name of the exported conda environment file
    directory : str
        The path to the diretory containing the environment file
    platform : str
        The name of the platform to check compatibility against

    Returns
    -------
    bool
        Returns True if the (possibly modified) environment file is compatible with conda
        on the specified platform. Otherwise returns False.
    """

    index = get_index(platform=platform)
    r = conda.resolve.Resolve(index)
    spec = specs.detect(filename=env_fname, directory=directory)
    env = spec.environment
    dependency_details = env.dependencies.items()
    missing_packages = None
    conda_deps = []
    pip_deps = []
    conda_deps_with_channel = []
    for distribution, dependencies in dependency_details:
        if distribution == 'conda':
            for dependency in dependencies:
                if "::" in dependency:
                    conda_deps_with_channel.append(dependency)
                else:
                    conda_deps.append(dependency)
        elif distribution == 'pip':
            pip_deps = dependencies

    for p in conda_deps_with_channel:
        p_no_channel = p.split("::")[1]
        conda_deps.append(p_no_channel)
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

    if missing_packages:
        print(
            "The following packages in your conda environment aren't available in the linux-64 conda channel the container will use:"
        )
        print(", ".join(str(package) for package in missing_packages))
        print(
            "We will skip their installation when deploying your function. If your function uses these packages, the container will experience a runtime error when queried."
        )

    with open(conda_dep_fname, 'wb') as f:
        for item in conda_deps:
            f.write("%s\n" % '='.join(item.split()))

    with open(pip_dep_fname, 'wb') as f:
        for item in pip_deps:
            f.write("%s\n" % item)

    return True


if __name__ == '__main__':
    if len(sys.argv) != 6:
        print("Usage: ./check_env.py <env_fname> <directory> <platform> <conda_dep_fname> <pip_dep_fname>")
        sys.exit(1)
    env_fname = sys.argv[1]
    directory = sys.argv[2]
    platform = sys.argv[3]
    conda_dep_fname = sys.argv[4]
    pip_dep_fname = sys.argv[5]
    if check_for_conflicts_and_existence(env_fname, directory, platform, conda_dep_fname, pip_dep_fname):
        sys.exit(0)
    sys.exit(1)
