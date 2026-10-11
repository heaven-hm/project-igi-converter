#!/usr/bin/env python3
"""Package the two shared-build executables with Qt and terminal launchers."""
import argparse
import pathlib
import plistlib
import re
import shutil
import subprocess
from package_notices import add_package_notices

parser = argparse.ArgumentParser()
parser.add_argument('--build', type=pathlib.Path, required=True)
parser.add_argument('--output', type=pathlib.Path, required=True)
parser.add_argument('--macdeployqt', required=True)
args = parser.parse_args()

def run(*command):
    subprocess.run(command, check=True)

def relocate_qt(executable: pathlib.Path, bundle: pathlib.Path) -> None:
    """Rewrite Homebrew build paths to the Qt frameworks deployed in the app."""
    linked = subprocess.check_output(['otool', '-L', str(executable)], text=True)
    for line in linked.splitlines()[1:]:
        library = line.strip().split(' (compatibility', 1)[0]
        match = re.search(r'/(Qt[^/]+\.framework)/Versions/([^/]+)/([^/]+)$', library)
        if library.startswith('/opt/homebrew/') and match:
            embedded = '@rpath/' + '/'.join(match.groups())
            run('install_name_tool', '-change', library, embedded, str(executable))
    load_commands = subprocess.check_output(['otool', '-l', str(executable)], text=True)
    for path in re.findall(r'path (/opt/homebrew/[^ ]+) \(offset', load_commands):
        subprocess.run(['install_name_tool', '-delete_rpath', path, str(executable)], check=False)
    run('install_name_tool', '-add_rpath', '@executable_path/../Frameworks', str(executable))

args.output.mkdir(parents=True, exist_ok=True)
for game in (1, 2):
    name = f'igi{game}conv'
    bundle = args.output / f'{name}.app'
    if bundle.exists():
        raise SystemExit(f'Refusing to overwrite existing bundle: {bundle}')
    contents = bundle / 'Contents'
    executable_dir = contents / 'MacOS'
    executable_dir.mkdir(parents=True)
    resources_dir = contents / 'Resources'
    resources_dir.mkdir()
    shutil.copy2(args.build / 'bin' / name, executable_dir / name)
    for asset in ('IGIModels.json', 'IGIModelsAllLevel.json', 'IGIAutoComplete.txt'):
        shutil.copy2(args.build / 'bin' / asset, resources_dir / asset)
    with (contents / 'Info.plist').open('wb') as target:
        plistlib.dump({
            'CFBundleExecutable': name,
            'CFBundleIdentifier': f'com.heaven-hm.{name}',
            'CFBundleName': name,
            'CFBundleDisplayName': f'IGI {game} Converter',
            'CFBundlePackageType': 'APPL',
            'CFBundleVersion': '2026.10.11',
            'NSHighResolutionCapable': True,
        }, target)
    qt_lib = pathlib.Path(args.macdeployqt).resolve().parent.parent / 'lib'
    # Homebrew splits Qt into formulae. The aggregate Qt prefix contains
    # optional plugin dependencies that the executable's rpaths do not list.
    aggregate_lib = pathlib.Path(args.macdeployqt).parent.parent / 'lib'
    # Deploy the widgets/audio/image plugins this application actually uses.
    # The aggregate Homebrew Qt installation also exposes unrelated plugins
    # (PDF, SVG and virtual keyboard) whose modules may not be installed.
    plugin_root = pathlib.Path(args.macdeployqt).parent.parent / 'share/qt/plugins'
    plugins = []
    for relative in ('platforms/libqcocoa.dylib', 'styles/libqmacstyle.dylib',
                     'multimedia/libdarwinmediaplugin.dylib',
                     'imageformats/libqjpeg.dylib', 'imageformats/libqgif.dylib',
                     'imageformats/libqtga.dylib'):
        target = contents / 'PlugIns' / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(plugin_root / relative, target)
        plugins.append(target)
    subprocess.run([args.macdeployqt, str(bundle.resolve()), '-always-overwrite',
                    '-no-plugins', '-no-codesign',
                    f'-libpath={aggregate_lib}', f'-libpath={qt_lib}',
                    *(f'-executable={plugin.resolve()}' for plugin in plugins)], check=True)
    relocate_qt(executable_dir / name, bundle)
    # Clear metadata inherited from Homebrew framework directories before signing.
    subprocess.run(['xattr', '-cr', str(bundle)], check=True)
    for item in (bundle, *bundle.rglob('*')):
        for metadata in ('com.apple.FinderInfo', 'com.apple.ResourceFork'):
            subprocess.run(['xattr', '-d', metadata, str(item)],
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                           check=False)
    subprocess.run(['codesign', '--force', '--deep', '--sign', '-', str(bundle)], check=True)
    subprocess.run(['codesign', '--verify', '--deep', str(bundle)], check=True)
    subprocess.run(['codesign', '--verify', '--deep', '--strict', str(bundle)], check=True)
    launcher = args.output / name
    launcher.write_text('#!/bin/sh\napp_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
                        f'exec "$app_dir/{name}.app/Contents/MacOS/{name}" "$@"\n')
    launcher.chmod(0o755)
    subprocess.run([str(launcher.resolve()), '--help'], check=True)
    qt_version = subprocess.check_output([str(pathlib.Path(args.macdeployqt).parent / 'qmake'), '-query', 'QT_VERSION'], text=True).strip()
    add_package_notices(args.output / (name + '-package-info'), name, 'macos', qt_version)
    print(f'Packaged GUI and CLI: {bundle}')
