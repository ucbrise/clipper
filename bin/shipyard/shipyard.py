import re
from collections import defaultdict, namedtuple
from distutils.version import LooseVersion
from functools import partial

import click

# Global action registry, this will be used to generate the final Makefile
global_registry = {}


class Action(object):
    """Action object represent a single Makefile target

    Two actions can be related by setting up dependencies:
    ```
        a1 > a2
    ```
    will translate to makefile, meaning a2 depends on a1
    ```
        a2: a1
    ```

    Action can also be used to register group(s) it belongs to:
    ```
        a3 = Action('a3', ...,  tags=['group1'])
        a4 = Action('a4', ..., tags='group1')
    ```
    will translate to makefile:
    ```
    group1: a3 a4
        # empty target
    ```

    All actions have a default target 'all' except IsolatedAction. 
    """

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

        # This hook will be used to process the command before it
        # gets printed out to Makfile.
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


class IsolatedAction(Action):
    """Action that does not belong to group 'all'"""

    def __init__(self, name, command="", tags=None):
        super().__init__(name, command, tags)
        self.post_processing_hooks.append(self._not_included_in_all)

    def _not_included_in_all(self):
        self.tags.remove("all")


class CIPrettyLogAction(Action):
    """Action which the command output will be tagged by the name and colored.
    In addition, a header and footer will be added so the final output will look
    like this:
    ```
    ===== start: target_name ======
    [target_name] color_log_line
    ===== finished: target_name =====
    ```
    """

    def __init__(self, name, command="", tags=None):
        super().__init__(name, command, tags)
        self.post_processing_hooks.append(self._colorize_output)

    def _colorize_output(self):
        whitespace = re.compile("^[\s]*$")
        header = "=" * 5 + f" start: {self.name} " + "=" * 5
        footer = "=" * 5 + f" finished: {self.name} " + "=" * 5
        self.command = "\n".join(
            [f"\t@echo {header}\n"]
            + [
                f"\t({line}) 2>&1 | python3 ./bin/colorize_output.py --tag {self.name}\n"
                for line in self.command.split("\n")
                if not whitespace.match(line)
            ]
            + [f"\t@echo {footer}\n"]
        )


def print_make_all():
    """Global function to printout the makefile"""

    # register all the tags or groups.
    # for example, group 'all'
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
def generate_make_file(sha_tag, namespace, clipper_root, version_tag, config, push):
    global ctx
    ctx.update(
        {
            "sha_tag": sha_tag,
            "namespace": namespace,
            "clipper_root": clipper_root,
            "version_tag": version_tag,
            "push": push,
        }
    )

    # prevent ppl to make directly
    IsolatedAction("placeholder", 'echo "Do not run make without any target!"')

    exec(open(config).read(), globals())

    print("SHELL=/bin/bash -o pipefail")
    print_make_all()


generate_make_file()
