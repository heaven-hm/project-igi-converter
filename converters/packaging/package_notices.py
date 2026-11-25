"""Copy project/dependency notices and write launch instructions for GUI packages."""
from pathlib import Path
import shutil


def add_package_notices(destination: Path, name: str, platform: str, qt_version: str) -> None:
    source = Path(__file__).resolve().parent.parent
    destination.mkdir(parents=True, exist_ok=True)
    notices = destination / 'licenses'
    notices.mkdir(exist_ok=True)
    for filename in ('LICENSE', 'LICENSE-PRIOR', 'LICENSE_SCOPE.md'):
        path = source.parent / filename
        if path.exists():
            shutil.copyfile(path, notices / filename)
    for relative, name_in_package in (
        ('third_party/nlohmann/LICENSE.MIT', 'nlohmann-json-MIT.txt'),
        ('third_party/glm-1.0.1/copying.txt', 'GLM-license.txt'),
        ('third_party/minimp3/LICENSE', 'minimp3-CC0.txt'),
    ):
        shutil.copyfile(source / relative, notices / name_in_package)
    for path in (source / 'packaging/licenses').glob('*.txt'):
        shutil.copyfile(path, notices / path.name)
    notices.joinpath('THIRD-PARTY-NOTICES.txt').write_text(
        f'These {name} GUI/CLI packages dynamically link Qt {qt_version}.\n'
        'Qt licenses apply separately from the converter license.\n'
        'Qt source: https://download.qt.io/archive/qt/\n'
        'Qt source repositories and third-party notices: https://code.qt.io/cgit/qt/\n'
        'Windows Qt Multimedia includes FFmpeg 7 libraries under their own licenses.\n'
        'FFmpeg source and notices: https://ffmpeg.org/releases/\n'
        'minimp3 (CC0): https://github.com/lieff/minimp3\n'
        'GLM: https://github.com/g-truc/glm\n'
        'nlohmann JSON: https://github.com/nlohmann/json\n'
        'stb image and image-write retain their license notices in the source headers:\n'
        'https://github.com/nothings/stb\n'
        'macOS deployed system dependencies retain their individual upstream licenses;\n'
        'source and formula metadata: https://github.com/Homebrew/homebrew-core\n'
        'No proprietary game assets or original GConv game DLLs are included.\n', encoding='utf-8')
    launch = (f'Open {name}.app for the GUI. In Terminal, use ./{name} --help.\n'
              if platform == 'macos' else
              f'Open {name}.exe for the GUI. In a terminal, use {name}.exe --help.\n')
    destination.joinpath('START-HERE.txt').write_text(
        launch + 'Keep the complete extracted folder; Qt frameworks/DLLs and plugins are required.\n'
        'Use --open PATH to select an asset in the GUI.\n'
        'Both GUI and CLI are in the same executable. IGI1Conv and IGI2Conv share the converters source tree.\n'
        'Rebuild: cmake -S converters -B build -DIGICONV_BUILD_GUI=ON; cmake --build build\n'
        'The source supports replacing dynamically linked Qt libraries; rebuilding against a compatible\n'
        'Qt installation is recommended. Modified macOS bundles require a new local signature:\n'
        'codesign --force --deep --sign - igiNconv.app\n'
        'Windows testing uses CrossOver on macOS. Native Windows-host validation is not claimed.\n'
        'See licenses/ for applicable licenses and upstream sources.\n', encoding='utf-8')
