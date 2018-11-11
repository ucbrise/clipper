from collections import namedtuple
from functools import partial
from distutils.version import LooseVersion
import click
global_registry = []

class Action(object):
    def __init__(self, name, command):
        self.name = name
        self.command = command
        self.dependencies = []

        global global_registry
        global_registry.append(self)

    def _sanitize_command(self):
        return self.command.replace("\n", "\n\t")

    def __str__(self):
        return f"""
{self.name}: {" ".join(self.dependencies)}
\t{self._sanitize_command()}
        """

    def __lt__(self, action):
        self.dependencies.append(action.name)

    def __eq__(self, value):
        return self.name == value.name and self.command == value.command

def print_make_all():
    for action in global_registry:
        print(action)

    print(
        f"""
all: {' '.join([action.name for action in global_registry])}
"""
    )


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
            -f dockerfiles/{dockerfile} {build_ctx['clipper_root']} > {image}.build.log"

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

ctx = {}

@click.command()
@click.option("--sha-tag", "-s", required=True, help="SHA1 tag of the Clipper codebase to tag the image")
@click.option("--namespace", "-n", default="clipper", help="Docker namespace to tag push this images to")
@click.option("--clipper-root", "-r", default="../", help="File directory of Clipper root")
@click.option("--version-tag", "-v", default="develop", help="Clipper versiont tag, in case we need to push it")
@click.option("--config", "-c", required=True, help="The configuration python file, like build_clipper.py")
@click.option("--push/--no-push", default=True, help="Override the option to push or not push")
def generate_make_file(sha_tag, namespace, clipper_root, version_tag, config, push):
    global ctx
    ctx.update({
        "sha_tag": sha_tag,
        "namespace": namespace,
        "clipper_root": clipper_root,
        "version_tag": version_tag,
        "push": push
    })

    # prevent ppl to make directly
    Action("placeholder", 'echo "Do not run make directly"')

    exec(open(config).read())
    print_make_all()

generate_make_file()
