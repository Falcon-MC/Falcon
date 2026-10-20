#!/usr/bin/env python3
"""Check the BDS layout inventory; optionally audit the original include-only relocation."""

import argparse
import hashlib
import json
import re
from pathlib import Path


def verify(root, verify_content):
    manifest_path = root / 'Falcon.Server/structure/bds-1.26.10.json'
    manifest = json.loads(manifest_path.read_text())
    records = manifest['files']
    headers = {
        row['path'].removeprefix('Falcon.Server/include/'):
            row['old'].removeprefix('Falcon.Server/include/')
        for row in records if row['old'].startswith('Falcon.Server/include/')
    }
    include = re.compile(r'(#\s*include\s*[<"])([^>"\n]+)([>"])')
    errors = []
    paths = set()
    directory_entries = {}
    for row in records:
        relative = row['path']
        if relative.casefold() in paths:
            errors.append('Duplicate destination: ' + relative)
        paths.add(relative.casefold())
        path = root / relative
        if not path.is_file():
            errors.append('Missing file: ' + relative)
            continue
        parent = root
        for segment in Path(relative).parts:
            names = directory_entries.setdefault(parent, None)
            if names is None:
                names = {entry.name for entry in parent.iterdir()}
                directory_entries[parent] = names
            if segment not in names:
                errors.append('Incorrect path case: ' + relative)
                break
            parent = parent / segment
        if row['old'].casefold() != relative.casefold() and (root / row['old']).is_file():
            errors.append('Old location still exists: ' + row['old'])
        if verify_content:
            restored = include.sub(lambda m: m[1] + headers.get(m[2], m[2]) + m[3], path.read_text())
            digest = hashlib.sha256(restored.encode()).hexdigest()
            if digest != row['sha256']:
                errors.append('Content changed beyond include paths: ' + relative)
    inventoried = {row['path'] for row in records}
    for tree in ['include', 'src']:
        for path in (root / 'Falcon.Server' / tree).rglob('*'):
            if path.is_file() and path.suffix in {'.h', '.cpp', '.hpp', '.inl'}:
                relative = path.relative_to(root).as_posix()
                if relative not in inventoried:
                    errors.append('File missing from inventory: ' + relative)
    # A legacy owned include must not silently resolve to a different dependency header.
    legacy = {row['old'].removeprefix('Falcon.Server/include/')
              for row in records if row['old'].startswith('Falcon.Server/include/')
              and row['old'] != row['path']}
    for tree in ['include', 'src', 'tests', 'cmake']:
        for path in (root / 'Falcon.Server' / tree).rglob('*'):
            if not path.is_file():
                continue
            try:
                contents = path.read_text()
            except UnicodeDecodeError:
                continue
            for match in include.finditer(contents):
                if match[2] in legacy:
                    errors.append('Legacy include in ' + str(path.relative_to(root)) + ': ' + match[2])
    if errors:
        raise RuntimeError('\n'.join(errors))
    print('Verified ' + str(len(records)) + ' files'
          + ('; original contents preserved except include paths' if verify_content else ''))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--verify-content', action='store_true',
                        help='Compare against the pre-relocation snapshot; use only to audit this refactor')
    args = parser.parse_args()
    verify(Path(__file__).resolve().parents[2], args.verify_content)


if __name__ == '__main__':
    main()
