import click
import shlex
import sys
import os
import re
import subprocess


def _replace_env_vars(script):
    return re.sub('\\$[A-Za-z0-9_]+',
                  lambda k: "\"{}\"".format(os.environ[k.group(0)[1:]]),
                  script)


def _inject_env_vars(env_vars):
    for var_string in env_vars:
        click.echo("--Injecting "+var_string+'--')
        key, val = var_string.split('=', 1)
        os.environ[key] = val


def _parse_cmds(lines):
    """
    Parse the following shell script:

    ```
    # proc1
    python2 some_script.py
    # proc2
    python3 some_script.py
    ```
    
    into python dictionary:
    ```
        {
            'proc1': ['python2', 'some_script.py'],
            'proc2': ['python3', 'some_script.py']
        }
    ```
    """
    cmds = {}
    proc_name = cmd = None

    for line in lines:
        if line.startswith('#'):
            proc_name = line[1:].strip()
        else:
            cmd = shlex.split(line)

        if proc_name and cmd:
            cmds[proc_name] = cmd
            proc_name = cmd = None

    return cmds


def _process_exit_codes(procs):
    click.echo("")

    exit_codes = []
    for name, p in procs.items():
        click.echo("{name} exited with status {code}".format(
            name=name, code=p.returncode))
        exit_codes.append(p.returncode)

    if sum(exit_codes) != 0:
        click.echo("Not all process exited with 0.")
        sys.exit(1)
    else:
        sys.exit(0)


@click.command('parallel')
@click.argument('shell_script', type=click.File('r'))
@click.option('-e', '--env-vars', multiple=True)
def run_parallel(shell_script, env_vars):
    _inject_env_vars(env_vars)

    # Read in files
    raw = shell_script.read()
    raw = _replace_env_vars(raw)
    no_multiline = raw.replace('\\\n', ' ')
    lines = [l.strip() for l in no_multiline.split('\n') if l != '']
    cmds = _parse_cmds(lines)

    # Printout cmds
    click.echo(
        "Running following {k} commands in parallel\n".format(k=len(cmds)))
    for name, cmd in cmds.items():
        click.echo("Process Name: {name}".format(name=name))
        click.echo("\t" + " ".join(cmd))
        click.echo("")

    procs = {
        name: subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            universal_newlines=True)
        for name, cmd in cmds.items()
    }
    while True:

        for name, cmd in procs.items():
            try:
                line = next(cmd.stdout)
                if line == '\n':
                    continue
                click.echo("[{name}]  {line}".format(
                    name=name, line=line.strip()))
            except: # end of iterator
                pass

        running = {
            name: proc
            for name, proc in procs.items() if proc.poll() is None
        }
        all_done = len(running) == 0
        if all_done:
            _process_exit_codes(procs)


if __name__ == '__main__':
    run_parallel()
