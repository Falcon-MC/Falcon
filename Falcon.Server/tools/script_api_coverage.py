#!/usr/bin/env python3
"""Script API coverage of a Falcon ref against the official typings.

usage: script_api_coverage.py <dir of extracted packages> <falcon repo> [git ref ...]

The directory is the one gen_script_surface.py reads (minecraft-server-2.11.0/package/index.d.ts...).

Counts classes, their members, enums, enum values and functions of each module's newest stable
typings. Interfaces are left out: they are option bags read from plain objects, never bound.
"""
import os
import re
import subprocess
import sys

MODULES = [
    ('server', 'minecraft-server-2.11.0'),
    ('ui', 'minecraft-server-ui-2.3.0'),
    ('common', 'minecraft-common-1.3.0'),
    ('graphics', 'minecraft-server-graphics-1.0.0'),
]


def official(path):
    names = set()
    text = open(path).read()
    kind = owner = None
    for line in text.split('\n'):
        match = re.match(r'export (?:declare )?(class|interface|enum) (\w+)', line)
        if match:
            kind, owner = match.groups()
            if kind != 'interface':
                names.add(owner)
            continue
        function = re.match(r'export (?:declare )?(?:function|const) (\w+)', line)
        if function:
            names.add(function.group(1))
            owner = None
            continue
        if line.startswith('}'):
            owner = None
            continue
        if owner is None or kind == 'interface':
            continue
        if kind == 'enum':
            value = re.match(r"\s+'?(\w+)'?\s*=", line)
            if value:
                names.add(owner + '.' + value.group(1))
            continue
        member = re.match(r"    (?:static |readonly |get |set )*'?(\w+)'?\??\s*[(:=]", line)
        if member:
            names.add(owner + '.' + member.group(1))
    return names


def falcon(repo, ref):
    text = subprocess.run(['git', '-C', repo, 'show', ref + ':Falcon.Server/src/Scripting/Binding/ScriptSurface.inc'],
                          capture_output=True, text=True).stdout
    bound = {}
    for match in re.finditer(r'\{"([a-z-]+):([^"]+)"', text):
        name = match.group(2).split('=')[0].replace('static ', '')
        bound.setdefault(match.group(1), set()).add(name)
    return bound


def main():
    pkgs, repo, refs = sys.argv[1], sys.argv[2], sys.argv[3:] or ['origin/dev']
    for ref in refs:
        bound = falcon(repo, ref)
        total = done = 0
        parts = []
        for short, folder in MODULES:
            path = os.path.join(pkgs, folder, 'package', 'index.d.ts')
            if not os.path.exists(path):
                continue
            names = official(path)
            have = len(names & bound.get(short, set()))
            total += len(names)
            done += have
            parts.append('%s %d/%d (%.0f%%)' % (short, have, len(names), 100.0 * have / len(names)))
        print('%s: %s | total %d/%d (%.0f%%)' % (ref, ', '.join(parts), done, total, 100.0 * done / total))


if __name__ == '__main__':
    main()
