"""Package a native CLI, its metadata files and licenses; exclude game assets."""

import argparse
import hashlib
import json
from pathlib import Path
import platform
import plistlib
import shutil
import subprocess
import tempfile
import zipfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--game", choices=("1", "2"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[1]
    name = f"igi{args.game}conv"
    binary = args.binary.resolve()
    report = json.loads(args.report.read_text())
    if report["failed"] or report["game"] != int(args.game):
        raise SystemExit("A passing format report for this game is required")
    if report["binary_sha256"] != hashlib.sha256(binary.read_bytes()).hexdigest():
        raise SystemExit("The report was produced by a different binary")

    version = subprocess.check_output([str(binary), "--version"], text=True).strip()
    with tempfile.TemporaryDirectory(prefix="igi-package-") as temporary:
        folder = Path(temporary) / name
        folder.mkdir()
        shutil.copy2(binary, folder / binary.name)
        if platform.system() == "Darwin":
            contents = folder / f"{name}.app" / "Contents"
            executable = contents / "MacOS" / name
            executable.parent.mkdir(parents=True)
            shutil.copy2(binary, executable)
            executable.chmod(0o755)
            with (contents / "Info.plist").open("wb") as stream:
                plistlib.dump(
                    {
                        "CFBundleIdentifier": f"com.heaven-hm.{name}",
                        "CFBundleName": name,
                        "CFBundleExecutable": name,
                        "CFBundlePackageType": "APPL",
                        "CFBundleShortVersionString": "1.11.0"
                        if args.game == "1"
                        else "0.2.0",
                        "LSUIElement": True,
                    },
                    stream,
                )
        metadata = ("IGIModels.json", "IGIModelsAllLevel.json", "IGIAutoComplete.txt")
        for filename in metadata:
            original = source / "assets" / filename
            if original.exists():
                shutil.copy2(original, folder / filename)
                if platform.system() == "Darwin":
                    shutil.copy2(original, contents / "MacOS" / filename)
        for filename in ("LICENSE", "LICENSE-PRIOR", "LICENSE_SCOPE.md", "README.md"):
            if (source / filename).exists():
                shutil.copy2(source / filename, folder / filename)
        documentation = folder / "docs"
        documentation.mkdir()
        shutil.copy2(source / "docs/verification.md", documentation / "verification.md")
        evidence = folder / "verification/evidence"
        evidence.mkdir(parents=True)
        shutil.copy2(args.report, evidence / "platform-formats.json")
        licenses = folder / "third-party-licenses"
        licenses.mkdir()
        for filename, relative in (
            ("nlohmann-json-MIT.txt", "third_party/nlohmann/LICENSE.MIT"),
            ("stb-LICENSE.txt", "third_party/tinygltf/LICENSE"),
            ("glm-LICENSE.txt", "third_party/glm-1.0.1/copying.txt"),
        ):
            if (source / relative).exists():
                shutil.copy2(source / relative, licenses / filename)
        stb_header = (source / "third_party/tinygltf/stb_image.h").read_text()
        start = stb_header.rfind("ALTERNATIVE A - MIT License")
        end = stb_header.find("ALTERNATIVE B - Public Domain", start)
        if start < 0 or end < 0:
            raise SystemExit("Vendored stb license could not be located")
        (licenses / "stb-MIT.txt").write_text(stb_header[start:end])
        (folder / "CLI.txt").write_text(
            f"{version}\nNative {platform.system()} {platform.machine()} CLI.\n"
            f"Run ./{binary.name} --help in a terminal.\n"
            f"On macOS: ./{name}.app/Contents/MacOS/{name} --help\n"
            "This app bundle has no GUI. No game files or Windows DLLs are included.\n"
        )
        args.output.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(args.output, "w", zipfile.ZIP_DEFLATED) as archive:
            for path in sorted(folder.rglob("*")):
                if path.is_file():
                    archive.write(path, path.relative_to(folder.parent))
    digest = hashlib.sha256(args.output.read_bytes()).hexdigest()
    args.output.with_suffix(".zip.sha256").write_text(f"{digest}  {args.output.name}\n")
    print(args.output, digest)


if __name__ == "__main__":
    main()
