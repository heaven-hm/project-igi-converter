"""Independent CLI checks using small synthetic files; no game fixtures required."""

import argparse
import json
import os
import struct
import subprocess
import tempfile
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--binary", type=Path, required=True)
parser.add_argument("--report", type=Path, required=True)
args = parser.parse_args()
results = []


def run(*arguments, success=True):
    process = subprocess.run(
        [str(args.binary.resolve()), *map(str, arguments)],
        capture_output=True,
        text=True,
        timeout=20,
    )
    if success and process.returncode != 0:
        raise AssertionError(process.stderr)
    if not success and process.returncode != 1:
        raise AssertionError(
            f"Expected rejection, got {process.returncode}: {process.stdout} {process.stderr}"
        )
    if "AddressSanitizer" in process.stderr or "runtime error:" in process.stderr:
        raise AssertionError(process.stderr)
    return process.stdout


def check(name, operation):
    try:
        operation()
        results.append({"check": name, "passed": True})
    except Exception as error:
        results.append({"check": name, "passed": False, "error": str(error)})


def equal(actual, expected):
    if actual != expected:
        raise AssertionError(f"Values differ: {actual!r} != {expected!r}")


def ilff(kind, chunks):
    output = bytearray(b"ILFF" + struct.pack("<III", 0, 4, 0) + kind)
    for index, (tag, payload) in enumerate(chunks):
        padding = (-len(payload)) % 4
        advance = 16 + len(payload) + padding if index + 1 < len(chunks) else 0
        output += (
            tag
            + struct.pack("<III", len(payload), 4, advance)
            + payload
            + bytes(padding)
        )
    struct.pack_into("<I", output, 4, len(output))
    return bytes(output)


def resource(entries):
    return ilff(
        b"IRES",
        [
            (tag, payload)
            for name, data in entries
            for tag, payload in ((b"NAME", name.encode() + b"\0"), (b"BODY", data))
        ],
    )


def tga(width, height, depth, descriptor, payload, rle=False):
    header = bytearray(18)
    header[2] = 10 if rle else 2
    struct.pack_into("<HH", header, 12, width, height)
    header[16:18] = bytes((depth, descriptor))
    return bytes(header) + payload


def texture(width=2, height=2, payload=bytes(16)):
    header = bytearray(32)
    header[:4] = b"LOOP"
    struct.pack_into("<II", header, 4, 11, 3)
    struct.pack_into("<HHHHH", header, 22, width, height, width, height, 4)
    return bytes(header) + payload


with tempfile.TemporaryDirectory(prefix="gconv-cli-checks-") as temporary:
    root = Path(temporary)

    def file(name, content):
        path = root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
        return path

    check("help", lambda: equal("native C++17 IGI 2 CLI" in run("--help"), True))
    check("version", lambda: equal("igi2conv 0.2.0" in run("--version"), True))
    check("no arguments", lambda: equal(run(), run("--help")))
    for family, operation in [
        ("qsc", "compile"),
        ("res", "list"),
        ("res", "extract"),
        ("res", "pack"),
        ("fnt", "info"),
        ("fnt", "extract"),
        ("fnt", "pack"),
        ("tex", "extract"),
        ("tex", "make"),
        ("tex", "pack"),
    ]:
        check(
            f"{family} {operation}: missing arguments",
            lambda f=family, o=operation: run(f, o, success=False),
        )
    check("unknown command", lambda: run("anm", "make", "x", success=False))
    check(
        "missing source",
        lambda: run("fnt", "info", root / "missing.fnt", success=False),
    )

    for prefix in range(32):
        check(
            f"truncated TEX header {prefix}",
            lambda n=prefix: run(
                "tex",
                "extract",
                file("bad.tex", texture()[:n]),
                root / "bad-out",
                success=False,
            ),
        )
    for name, data in [
        ("zero width", texture(0, 2)),
        ("zero height", texture(2, 0)),
        ("missing base pixels", texture(payload=b"")),
        ("truncated pixels", texture(payload=bytes(15))),
    ]:
        check(
            name,
            lambda d=data: run(
                "tex", "extract", file("bad.tex", d), root / "bad-out", success=False
            ),
        )
    sprite = bytearray(52)
    sprite[:4] = b"LOOP"
    struct.pack_into("<I", sprite, 4, 9)
    struct.pack_into("<III", sprite, 40, 2, 2, 3)
    check(
        "zero sprite bitmap count",
        lambda: run(
            "tex", "extract", file("empty.spr", sprite), root / "bad-out", success=False
        ),
    )

    # Four origin combinations, raw/RLE, and all supported true-color depths.
    for depth in (16, 24, 32):
        stride = depth // 8
        canonical = (
            struct.pack("<4H", 0x801F, 0x83E0, 0xFC00, 0x7FFF)
            if depth == 16
            else bytes(range(1, 4 * stride + 1))
        )
        expected = (
            canonical
            if depth != 24
            else b"".join(canonical[i : i + 3] + b"\xff" for i in range(0, 12, 3))
        )
        for descriptor in (0, 16, 32, 48):
            stored = b"".join(
                canonical[
                    (
                        (y if descriptor & 32 else 1 - y) * 2
                        + (1 - x if descriptor & 16 else x)
                    )
                    * stride : (
                        (y if descriptor & 32 else 1 - y) * 2
                        + (1 - x if descriptor & 16 else x)
                    )
                    * stride
                    + stride
                ]
                for y in range(2)
                for x in range(2)
            )
            for rle in (False, True):

                def orientation(
                    d=depth, desc=descriptor, raw=stored, packed=rle, expected=expected
                ):
                    source = file(
                        "orientation.tga",
                        tga(2, 2, d, desc, (b"\x03" + raw) if packed else raw, packed),
                    )
                    target = root / "orientation.tex"
                    run("tex", "make", source, target)
                    equal(target.read_bytes()[32:], expected)

                check(f"TGA {depth}bpp origin {descriptor} RLE={rle}", orientation)

    def repeat_packet():
        pixel = bytes((12, 34, 56, 78))
        source = file("repeat.tga", tga(2, 2, 32, 32, b"\x83" + pixel, True))
        run("tex", "make", source, root / "repeat.tex")
        equal((root / "repeat.tex").read_bytes()[32:], pixel * 4)

    check("RLE repeat packet", repeat_packet)
    check(
        "RLE packet overrun",
        lambda: run(
            "tex",
            "make",
            file("rle.tga", tga(1, 1, 32, 32, b"\x81" + bytes(4), True)),
            root / "rle.tex",
            success=False,
        ),
    )
    check(
        "invalid PNG",
        lambda: run(
            "tex",
            "make",
            file("bad.png", b"not a png"),
            root / "bad.tex",
            success=False,
        ),
    )
    check(
        "TEX source overwrite",
        lambda: run(
            "tex",
            "make",
            file("overwrite.tga", tga(2, 2, 32, 32, bytes(16))),
            root / "overwrite.tga",
            success=False,
        ),
    )
    check(
        "QSC source overwrite",
        lambda: run(
            "qsc",
            "compile",
            file("overwrite.qsc", b"Foo();"),
            root / "overwrite.qsc",
            success=False,
        ),
    )

    def hardlink_output(family, operation, source_name, content):
        source = file(source_name, content)
        target = root / ("alias-" + source_name)
        os.link(source, target)
        run(family, operation, source, target, success=False)
        equal(source.read_bytes(), content)

    check(
        "TEX hardlink output preserved",
        lambda: hardlink_output(
            "tex", "make", "hardlink.tga", tga(2, 2, 32, 32, bytes(16))
        ),
    )
    check(
        "QSC hardlink output preserved",
        lambda: hardlink_output("qsc", "compile", "hardlink.qsc", b"Foo();"),
    )

    for text in (b"Foo(", b"if(TRUE) {", b'Foo("unterminated);', b"@"):
        check(
            "malformed QSC " + repr(text),
            lambda data=text: run(
                "qsc", "compile", file("bad.qsc", data), root / "bad.qvm", success=False
            ),
        )

    for name in (
        "LOCAL:../escape",
        "LOCAL:/absolute",
        "LOCAL:C:/escape",
        "LOCAL:folder/../../escape",
    ):
        check(
            "resource traversal " + name,
            lambda n=name: run(
                "res",
                "extract",
                file("unsafe.res", resource([(n, b"x")])),
                root / "unsafe-out",
                success=False,
            ),
        )
    check(
        "duplicate resource names",
        lambda: run(
            "res",
            "extract",
            file("duplicate.res", resource([("LOCAL:one", b"a"), ("LOCAL:ONE", b"b")])),
            root / "duplicates",
            success=False,
        ),
    )

    def symlink_extraction():
        destination = root / "symlink-out"
        destination.mkdir()
        outside = root / "outside"
        outside.mkdir()
        (destination / "link").symlink_to(outside, target_is_directory=True)
        run(
            "res",
            "extract",
            file("link.res", resource([("LOCAL:link/file", b"x")])),
            destination,
            success=False,
        )
        equal(list(outside.iterdir()), [])

    check("resource symlink escape", symlink_extraction)

    def existing_resource():
        destination = root / "existing"
        destination.mkdir()
        original = file("existing/file", b"keep")
        run(
            "res",
            "extract",
            file("existing.res", resource([("LOCAL:file", b"new")])),
            destination,
            success=False,
        )
        equal(original.read_bytes(), b"keep")

    check("resource existing output preserved", existing_resource)

    def res_pack():
        directory = root / "pack-input"
        directory.mkdir()
        file("pack-input/sub/a", b"alpha")
        file("pack-input/b", b"beta")
        run("res", "pack", directory, root / "packed.res")
        equal(len(run("res", "list", root / "packed.res").splitlines()), 2)
        run("res", "extract", root / "packed.res", root / "packed-output")
        equal((root / "packed-output/sub/a").read_bytes(), b"alpha")
        equal((root / "packed-output/b").read_bytes(), b"beta")

    check("resource pack/list/extract", res_pack)

    def empty_resource():
        source = file("empty.res", resource([]))
        destination = root / "empty-extracted"
        run("res", "extract", source, destination)
        equal(destination.is_dir(), True)
        run("res", "pack", destination, root / "empty-repacked.res")
        equal(run("res", "list", root / "empty-repacked.res"), "")

    check("empty resource extract and repack", empty_resource)

    # Check real edits rather than only extraction followed by unchanged repacking.
    texh = bytearray(24)
    struct.pack_into("<H", texh, 0, 3)
    struct.pack_into("<5H", texh, 14, 2, 2, 2, 2, 32)
    original_pixels = bytes(range(16))
    edited_pixels = bytes(range(16, 32))
    font_bytes = ilff(
        b"FONT",
        [
            (b"TEXH", texh),
            (b"BODY", original_pixels),
            (b"TEXH", texh),
            (b"BODY", b"keep-second-body"),
        ],
    )

    def font_edit():
        source = file("font.fnt", font_bytes)
        image = file("font-edit.tga", tga(2, 2, 32, 32, edited_pixels))
        equal("2x2 32-bit" in run("fnt", "info", source), True)
        run("fnt", "extract", source, root / "font.tga")
        equal((root / "font.tga").read_bytes()[18:], original_pixels)
        run("fnt", "pack", source, image, root / "edited.fnt")
        equal(
            (root / "edited.fnt").read_bytes(),
            font_bytes.replace(original_pixels, edited_pixels, 1),
        )

    check("font edit preserves second atlas and metadata", font_edit)

    def texture_edit():
        source = file("template.tex", texture())
        directory = root / "texture-edit"
        directory.mkdir()
        file("texture-edit/template.0.tga", tga(2, 2, 32, 32, edited_pixels))
        run("tex", "pack", source, directory, root / "edited.tex")
        equal(
            (root / "edited.tex").read_bytes(), source.read_bytes()[:32] + edited_pixels
        )

    check("texture edits change exactly the base pixels", texture_edit)

args.report.parent.mkdir(parents=True, exist_ok=True)
args.report.write_text(json.dumps(results, indent=2) + "\n")
failed = [r for r in results if not r["passed"]]
print(f"{len(results)-len(failed)}/{len(results)} CLI checks passed")
for result in failed:
    print(result["check"] + ": " + result["error"])
raise SystemExit(bool(failed))
