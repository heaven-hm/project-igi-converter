#!/usr/bin/env python3
"""Deploy both shared-build Windows GUI/CLI apps, including Qt plugins and DLLs."""
import argparse
from pathlib import Path
import shlex
import shutil
import subprocess
import re
from package_notices import add_package_notices

parser = argparse.ArgumentParser()
parser.add_argument('--build', type=Path, required=True)
parser.add_argument('--qt-bin', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--runtime-dir', type=Path,
                    help='Compiler runtime DLL directory when different from the Qt toolchain')
parser.add_argument('--runner', default='', help='Optional executable runner, e.g. Wine/CrossOver')
args = parser.parse_args()
if args.runtime_dir:
    for library in ('libgcc_s_seh-1.dll', 'libstdc++-6.dll'):
        if not (args.runtime_dir / library).is_file():
            parser.error(f'Missing compiler runtime: {args.runtime_dir / library}')

for game in (1, 2):
    name = f'igi{game}conv'
    target = args.output / name
    target.mkdir(parents=True, exist_ok=False)
    binary = args.build / 'bin' / f'{name}.exe'
    if not binary.exists():
        binary = args.build / 'bin' / 'Release' / f'{name}.exe'
    shutil.copyfile(binary, target / binary.name)
    command = [*shlex.split(args.runner), str((args.qt_bin / 'windeployqt.exe').resolve()),
               '--release', '--no-translations', str((target / binary.name).resolve())]
    subprocess.run(command, check=True)
    for asset in ('IGIModels.json', 'IGIModelsAllLevel.json', 'IGIAutoComplete.txt'):
        shutil.copyfile(binary.parent / asset, target / asset)
    if args.runtime_dir:
        for library in ('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll'):
            source = args.runtime_dir / library
            if source.exists():
                shutil.copyfile(source, target / library)
    config = (args.qt_bin.parent / 'mkspecs/qconfig.pri').read_text()
    version = re.search(r'^QT_VERSION\s*=\s*(\S+)', config, re.MULTILINE).group(1)
    add_package_notices(target, name, 'windows', version)
    print(f'Packaged GUI and CLI: {target}')
