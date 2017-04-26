from conda.api import get_index
from conda.base.context import context
from conda.exceptions import UnsatisfiableError, NoPackagesFoundError
import conda.resolve
import conda_env.specs as specs
import sys


def check_for_conflicts_and_existence(env_fname, directory, platform):
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
    try:
        missing_packages = None
        for distribution, dependencies in dependency_details:
            if distribution == 'conda':
                try:
                    # This call doesn't install anything; it checks the solvability of package dependencies.
                    r.install(dependencies)
                except NoPackagesFoundError as missing_packages_error:
                    missing_packages = missing_packages_error.pkgs
                    for package in missing_packages:
                        dependencies.remove(package)

                    # Check that the dependencies that are not missing are satisfiable
                    r.install(dependencies)
                if missing_packages:
                    print(
                        "The following packages in your conda environment aren't available in the linux-64 conda channel the container will use:"
                    )
                    print(", ".join(
                        str(package) for package in missing_packages))
                    print(
                        "We will skip their installation when deploying your function. If your function uses these packages, the container will experience a runtime error when queried."
                    )

                    missing_packages_raw = [
                        '='.join(package.split())
                        for package in missing_packages
                    ]
                    for missing_package_raw in missing_packages_raw:
                        env.dependencies.raw.remove(missing_package_raw)
                    print(
                        "Removed unavailable packages from supplied environment specifications"
                    )
                    env.dependencies.parse()
                    env.save()
        return True
    except UnsatisfiableError as unsat_e:
        print(
            "Your conda dependencies are unsatisfiable (see error text below). Please resolve these issues and call `deploy_predict_func` again."
        )
        print(unsat_e)
        return False


if __name__ == '__main__':
    if len(sys.argv) != 4:
        print("Usage: ./check_env.py <env_fname> <directory> <platform>")
        sys.exit(1)
    env_fname = sys.argv[1]
    directory = sys.argv[2]
    platform = sys.argv[3]
    if check_for_conflicts_and_existence(env_fname, directory, platform):
        sys.exit(0)
    sys.exit(1)
