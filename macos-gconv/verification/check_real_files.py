"""Check native conversions against the complete supplied IGI 2 fixture corpus.

Missing fixture groups and missing original-DLL evidence are failures.
"""

import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument(
    "--binary", type=Path, default=Path(__file__).resolve().parents[1] / "build/gconv"
)
parser.add_argument(
    "--samples", type=Path, default=Path("/private/tmp/neonix-user-samples")
)
parser.add_argument(
    "--demo", type=Path, default=Path("/private/tmp/neonix-igi2-download/samples")
)
parser.add_argument(
    "--demo-archives",
    type=Path,
    default=Path("/private/tmp/neonix-igi2-download/sample-archives"),
)
parser.add_argument(
    "--level5-images",
    type=Path,
    default=Path("/private/tmp/neonix-level5-check/archive-samples"),
)
parser.add_argument("--downloads", type=Path, default=Path.home() / "Downloads")
parser.add_argument(
    "--qsc", type=Path, default=Path("/private/tmp/neonix-level5-check/all-qsc")
)
parser.add_argument("--level5", type=Path, default=Path.home() / "Downloads/LEVEL5")
parser.add_argument(
    "--oracle-input",
    type=Path,
    default=Path("/private/tmp/gconv-native-test/editor.tga"),
)
parser.add_argument(
    "--oracle-output",
    type=Path,
    default=Path("/private/tmp/gconv-native-test/image/editor.tex"),
)
parser.add_argument("--report", type=Path, required=True)
args = parser.parse_args()
records = []


def ensure(condition, message):
    if not condition:
        raise RuntimeError(message)


def run(*arguments):
    result = subprocess.run(
        [str(args.binary.resolve()), *map(str, arguments)],
        capture_output=True,
        text=True,
        timeout=30,
    )
    ensure(result.returncode == 0, result.stderr or result.stdout)
    ensure(
        "AddressSanitizer" not in result.stderr
        and "runtime error:" not in result.stderr,
        result.stderr,
    )
    return result.stdout


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def collect(root, extensions):
    files = sorted(p for p in root.rglob("*") if p.suffix.lower() in extensions)
    ensure(files, f"Required fixture group is empty or missing: {root}")
    return files


def payloads(root):
    return {p.relative_to(root): p.read_bytes() for p in root.rglob("*") if p.is_file()}


fonts = collect(args.samples / "fnt-files", {".fnt"})
fonts += collect(args.samples / "more-sprites", {".fnt"})
fonts += collect(args.demo / "fonts", {".fnt"})
textures = []
for group in (
    "more-sprites",
    "sprites-res",
    "menusystem-res",
    "menusystem1-res",
    "tex-files",
):
    textures += collect(args.samples / group, {".spr", ".pic", ".tex"})
textures += collect(args.demo / "spr", {".spr"})
textures += collect(args.level5_images, {".tex", ".spr", ".pic"})
textures += collect(args.level5, {".tex", ".spr", ".pic"})
archives = [
    args.downloads / name
    for name in ("sprites.res", "menusystem.res", "menusystem (1).res")
]
archives += collect(args.level5, {".res"})
archives += collect(args.demo_archives, {".res"})
qsc_files = collect(args.qsc, {".qsc"})
original_scripts = collect(args.level5, {".qvm"})
ensure(
    {p.stem for p in original_scripts} == {p.stem for p in qsc_files},
    "QSC fixture set does not cover every original LEVEL5 QVM",
)
for source in archives + [args.oracle_input, args.oracle_output]:
    ensure(source.is_file(), f"Required fixture is missing: {source}")

with tempfile.TemporaryDirectory(prefix="gconv-cpp17-") as temporary:
    output = Path(temporary)
    for index, source in enumerate(fonts):
        for extension in ("png", "tga"):
            image = output / f"{index}.{extension}"
            packed = output / f"{index}-{extension}.fnt"
            run("fnt", "info", source)
            run("fnt", "extract", source, image)
            run("fnt", "pack", source, image, packed)
            ensure(
                source.read_bytes() == packed.read_bytes(),
                f"Font round trip differs: {source}",
            )
            records.append(
                {
                    "font": source.name,
                    "source_sha256": digest(source),
                    "format": extension,
                    "byte_identical": True,
                }
            )
    for index, source in enumerate(textures):
        directory = output / f"texture-{index}"
        run("tex", "extract", source, directory)
        ensure(
            list(directory.glob("*.tga")), f"Texture has no extracted bitmaps: {source}"
        )
        packed = output / f"texture-{index}{source.suffix}"
        run("tex", "pack", source, directory, packed)
        ensure(
            source.read_bytes() == packed.read_bytes(),
            f"Texture round trip differs: {source}",
        )
        records.append(
            {
                "texture": source.name,
                "source_sha256": digest(source),
                "byte_identical": True,
            }
        )
    for index, source in enumerate(archives):
        directory, extracted = output / f"res-{index}", output / f"repacked-{index}"
        packed = output / f"res-{index}.res"
        listing = run("res", "list", source)
        run("res", "extract", source, directory)
        original = payloads(directory)
        ensure(
            len(listing.splitlines()) == len(original),
            f"Resource listing differs: {source}",
        )
        run("res", "pack", directory, packed)
        run("res", "extract", packed, extracted)
        ensure(original == payloads(extracted), f"Resource payloads differ: {source}")
        records.append(
            {
                "resource": source.name,
                "source_sha256": digest(source),
                "files": len(original),
                "payloads_identical": True,
            }
        )
    for index, source in enumerate(qsc_files):
        packed = output / f"script-{index}.qvm"
        run("qsc", "compile", source, packed)
        originals = [p for p in original_scripts if p.stem == source.stem]
        ensure(
            any(packed.read_bytes() == p.read_bytes() for p in originals),
            f"QVM differs from game original: {source}",
        )
        records.append({"qsc": source.name, "original_byte_identical": True})
    native = output / "native.tex"
    run("tex", "make", args.oracle_input, native)
    original = args.oracle_output.read_bytes()
    actual = native.read_bytes()
    ensure(actual[32:] == original[32 : len(actual)], "Original DLL base pixels differ")
    ensure(
        actual[:20] == original[:20] and actual[22:32] == original[22:32],
        "Original DLL TEX header fields differ",
    )
    records.append(
        {
            "original_dll_texture_oracle": True,
            "base_pixels_identical": True,
            "note": "Native make emits base level only; original defaults include mipmaps.",
        }
    )
args.report.parent.mkdir(parents=True, exist_ok=True)
args.report.write_text(json.dumps(records, indent=2) + "\n")
print(
    f"PASS: {len(fonts)} fonts / {len(fonts)*2} round trips, "
    f"{len(textures)} texture round trips, {len(archives)} resource archives, "
    f"{len(qsc_files)} original QVM matches"
)
