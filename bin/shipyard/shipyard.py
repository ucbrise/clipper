from collections import namedtuple
from functools import partial
from collections import defaultdict
from distutils.version import LooseVersion
import click

global_registry = {}


class Action(object):
    def __init__(self, name, command="", tags=None):
        self.name = name
        self.command = command
        self.tags = tags

        if not self.tags:
            self.tags = ["all"]
        elif isinstance(self.tags, str):
            self.tags = ["all", self.tags]
        else:
            self.tags = ["all"] + list(self.tags)

        self.dependencies = []

        global global_registry
        global_registry[name] = self

        self.post_processing_hooks = [self._sanitize_command]

    @classmethod
    def get_action(cls, name):
        return global_registry[name]

    @classmethod
    def get_all_action(cls):
        return global_registry.values()

    def add_tag(self, tag):
        self.tags.append(tag)

    def _sanitize_command(self):
        self.command = self.command.replace("\n", "\n\t")

    def __str__(self):
        [hook() for hook in reversed(self.post_processing_hooks)]

        return f"""
{self.name}: {" ".join(self.dependencies)}
\t{self.command}
        """

    def __lt__(self, action):
        self.dependencies.append(action.name)

    def __eq__(self, value):
        return self.name == value.name and self.command == value.command


def print_make_all():
    tag_to_action_name = defaultdict(list)
    for action in global_registry.values():
        print(action)
        for t in action.tags:
            tag_to_action_name[t].append(action.name)
    for tag, actions in tag_to_action_name.items():
        print(
            f"""
{tag}: {' '.join(actions)}
"""
        )


ctx = {}


@click.command()
@click.option(
    "--sha-tag",
    "-s",
    required=True,
    help="SHA1 tag of the Clipper codebase to tag the image",
)
@click.option(
    "--namespace",
    "-n",
    default="clipper",
    help="Docker namespace to tag push this images to",
)
@click.option(
    "--clipper-root", "-r", default="../", help="File directory of Clipper root"
)
@click.option(
    "--version-tag",
    "-v",
    default="develop",
    help="Clipper versiont tag, in case we need to push it",
)
@click.option(
    "--config",
    "-c",
    required=True,
    help="The configuration python file, like build_clipper.py",
)
@click.option(
    "--push/--no-push",
    default=True,
    help="Override the option to push or not push version",
)
@click.option(
    "--kafka-address", "-a", required=True, help="Kafka address to send the log to"
)
def generate_make_file(
    sha_tag, namespace, clipper_root, version_tag, config, push, kafka_address
):
    global ctx
    ctx.update(
        {
            "sha_tag": sha_tag,
            "namespace": namespace,
            "clipper_root": clipper_root,
            "version_tag": version_tag,
            "push": push,
            "kafka_address": kafka_address,
        }
    )

    # prevent ppl to make directly
    Action("placeholder", 'echo "Do not run make without any target!"')

    exec(open(config).read(), globals())

    print_make_all()


generate_make_file()
