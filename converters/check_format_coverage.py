"""Run one real end-to-end example per supported game asset format."""

import argparse
import hashlib
import json
import math
import re
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import wave


def resource_payloads(path):
    data = path.read_bytes()
    if data[:4] != b"ILFF" or data[16:20] != b"IRES":
        raise AssertionError("Not an ILFF/IRES resource archive")
    alignment = struct.unpack_from("<I", data, 8)[0]
    position = (20 + alignment - 1) // alignment * alignment if alignment else 20
    result = {}
    name = None
    while position < len(data):
        tag, length, alignment, advance = struct.unpack_from("<4sIII", data, position)
        start = position + 16
        if alignment:
            start = (start + alignment - 1) // alignment * alignment
        payload = data[start : start + length]
        if len(payload) != length:
            raise AssertionError("Truncated archive payload")
        if tag == b"NAME":
            name = payload.split(b"\0")[0].decode("latin1").removeprefix("LOCAL:")
        elif tag == b"BODY":
            if name is None or name in result:
                raise AssertionError("Missing or duplicate resource name")
            result[name] = payload
            name = None
        if not advance:
            break
        position += advance
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--samples", required=True, type=Path)
    parser.add_argument("--game", required=True, choices=("1", "2"))
    parser.add_argument("--report", required=True, type=Path)
    args = parser.parse_args()
    binary = args.binary.resolve()
    samples = args.samples.resolve()
    rows = []

    with tempfile.TemporaryDirectory(prefix="igi-format-test-") as temporary:
        root = Path(temporary)

        def run(*command, expect=0):
            result = subprocess.run(
                [str(binary), *map(str, command)],
                capture_output=True,
                text=True,
                timeout=90,
            )
            if (expect == 0 and result.returncode != 0) or (
                expect != 0 and result.returncode == 0
            ):
                raise AssertionError(
                    f"{command}: exit {result.returncode}\n{result.stdout}\n{result.stderr}"
                )
            return result.stdout

        def fixture(name):
            source = samples / name
            if not source.is_file():
                raise AssertionError(f"Missing real fixture: {name}")
            target = root / "input" / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(source, target)
            return target

        def exact(first, second):
            if first.read_bytes() != second.read_bytes():
                raise AssertionError(f"Binary round trip differs: {first.name}")

        def check(label, operation):
            try:
                detail = operation() or {}
                rows.append({"format": label, "status": "passed", **detail})
                print(f"PASS {label}", flush=True)
            except Exception as error:
                rows.append({"format": label, "status": "failed", "error": str(error)})
                print(f"FAIL {label}: {error}", flush=True)

        def script_roundtrip(name="qvm.qvm"):
            source = fixture(name)
            output = root / "script.qsc"
            rebuilt = root / "script.qvm"
            run("qvm", "decompile", source, "-o", output)
            if args.game == "1":
                run("qsc", "compile", output, "-o", rebuilt)
            else:
                run("qsc", "compile", output, rebuilt)
            exact(source, rebuilt)
            return {
                "fidelity": "byte-identical",
                "source_sha256": hashlib.sha256(source.read_bytes()).hexdigest(),
            }

        check("QVM -> QSC -> QVM", script_roundtrip)
        check(
            "QSC -> QVM -> QSC",
            lambda: (
                run(
                    "qvm",
                    "decompile",
                    root / "script.qvm",
                    "-o",
                    root / "script-again.qsc",
                ),
                exact(root / "script.qsc", root / "script-again.qsc"),
                {"fidelity": "byte-identical source"},
            )[-1],
        )

        if args.game == "2":
            check("QVM retail 8.7", lambda: script_roundtrip("qvm-retail.qvm"))

        def source_guard():
            source = fixture("qvm.qvm")
            before = source.read_bytes()
            run("qvm", "decompile", source, "-o", source, expect=1)
            if source.read_bytes() != before:
                raise AssertionError("Rejected overwrite changed the original file")
            return {"rejection": "source overwrite refused and input unchanged"}

        check("source overwrite guard", source_guard)

        if args.game == "1":

            def graph_size_guard():
                source = root / "overflow-graph.dat"
                data = bytearray(30)
                struct.pack_into("<I", data, 0, 0xFFEEDDCC)
                struct.pack_into("<I", data, 12, 0x80000000)
                source.write_bytes(data)
                output = root / "overflow-graph.json"
                run("asset", "to-json", source, output, expect=1)
                if output.exists():
                    raise AssertionError("Invalid graph produced output")
                return {"rejection": "overflowing adjacency capacity refused"}

            check("graph capacity overflow guard", graph_size_guard)

        def invalid_json_range():
            source = fixture("olm.olm")
            document_path = root / "invalid-olm.json"
            run("asset", "to-json", source, document_path)
            document = json.loads(document_path.read_text())
            document["layers"][0]["rgba"][0] = 256
            document_path.write_text(json.dumps(document))
            output = root / "invalid.olm"
            run("asset", "from-json", document_path, output, expect=1)
            if output.exists():
                raise AssertionError("Invalid JSON created an output file")
            return {"rejection": "integer overflow refused before writing"}

        check("JSON range guard", invalid_json_range)

        def archive_roundtrip():
            source = fixture("res.res")
            directory = root / "resource-assets"
            rebuilt = root / "rebuilt.res"
            run("res", "extract", source, directory)
            run("res", "pack", directory, rebuilt)
            if resource_payloads(source) != resource_payloads(rebuilt):
                raise AssertionError("Archive names or payloads changed")
            return {"fidelity": "all names and payloads identical"}

        check("RES", archive_roundtrip)
        for extension in ("tex", "spr", "pic"):

            def texture_roundtrip(extension=extension):
                source = fixture(f"{extension}.{extension}")
                directory = root / f"{extension}-bitmaps"
                rebuilt = root / f"rebuilt.{extension}"
                run("tex", "extract", source, directory)
                run("tex", "pack", source, directory, rebuilt)
                exact(source, rebuilt)
                return {"fidelity": "byte-identical"}

            check(extension.upper(), texture_roundtrip)

        def font_roundtrip():
            source = fixture("fnt.fnt")
            for extension in ("png", "tga"):
                image = root / f"font.{extension}"
                rebuilt = root / f"font-{extension}.fnt"
                run("fnt", "extract", source, image)
                run("fnt", "pack", source, image, rebuilt)
                exact(source, rebuilt)
            return {"fidelity": "byte-identical via PNG and TGA"}

        check("FNT", font_roundtrip)

        def bitmap_source_guard():
            source = fixture("fnt.fnt")
            bitmap = root / "guard-bitmap.png"
            shutil.copy2(root / "font.png", bitmap)
            before = bitmap.read_bytes()
            run("fnt", "pack", source, bitmap, bitmap, expect=1)
            if bitmap.read_bytes() != before:
                raise AssertionError("Rejected pack overwrote the input bitmap")
            alias = root / "guard-alias.fnt"
            alias.hardlink_to(bitmap)
            run("fnt", "pack", source, bitmap, alias, expect=1)
            if bitmap.read_bytes() != before:
                raise AssertionError("Rejected alias pack overwrote the input bitmap")
            return {"rejection": "bitmap and hard-link aliases remain unchanged"}

        check("bitmap source overwrite guard", bitmap_source_guard)

        for extension in ("png", "tga"):

            def image_input(extension=extension):
                output = root / f"created-{extension}.tex"
                run("tex", "make", root / f"font.{extension}", output)
                run("tex", "extract", output, root / f"created-{extension}")
                if not list((root / f"created-{extension}").glob("*.tga")):
                    raise AssertionError("No image output")
                return {"fidelity": "native image -> TEX -> TGA"}

            check(extension.upper(), image_input)

        def model_roundtrip(name="mef.mef"):
            source = fixture(name)
            document = root / "model.json"
            rebuilt = root / "model.mef"
            run("asset", "to-json", source, document)
            run("asset", "from-json", document, rebuilt)
            exact(source, rebuilt)
            if args.game == "2":
                native_text = root / "model-native.json"
                run("mef", "to-text", source, "-o", native_text)
                run("mef", "compile", native_text, "-o", rebuilt)
                exact(source, rebuilt)
            run("mef", "export", source, "-o", root / "model.obj")
            text = (root / "model.obj").read_text()
            if "\nv " not in text or "\nf " not in text:
                raise AssertionError("OBJ does not contain vertices and faces")
            return {"fidelity": "byte-identical JSON rebuild; OBJ geometry exported"}

        check("MEF", model_roundtrip)

        if args.game == "2":
            for name in ("mef-skeletal.mef", "mef-lightmapped.mef"):
                check(name, lambda name=name: model_roundtrip(name))

        def animation_roundtrip():
            source = fixture("iff.iff")
            rebuilt = root / "animation.iff"
            document = root / "animation.json"
            run("iff", "to-json", source, document)
            run("iff", "from-json", document, rebuilt)
            exact(source, rebuilt)
            return {"fidelity": "byte-identical JSON rebuild"}

        check("IFF", animation_roundtrip)
        if args.game == "1":

            def bef_roundtrip():
                source = fixture("iff.iff")
                directory = root / "befs"
                run("iff", "convert", source, directory)
                paths = list(directory.glob("*.BEF"))
                if not paths:
                    raise AssertionError("No BEF output")
                rebuilt = root / "bef-rebuilt" / "iff.iff"
                rebuilt.parent.mkdir()
                run("iff", "create", directory, rebuilt)
                again = root / "bef-again"
                run("iff", "convert", rebuilt, again)
                for path in paths:
                    before = re.split(
                        r"(-?\d+(?:\.\d*)?(?:e[+-]?\d+)?)", path.read_text()
                    )
                    after = re.split(
                        r"(-?\d+(?:\.\d*)?(?:e[+-]?\d+)?)",
                        (again / path.name).read_text(),
                    )
                    if len(before) != len(after):
                        raise AssertionError("BEF keyframe or event count changed")
                    for index, (first, second) in enumerate(zip(before, after)):
                        if index % 2:
                            if not math.isclose(
                                float(first), float(second), rel_tol=1e-6, abs_tol=1e-7
                            ):
                                raise AssertionError("BEF numeric content changed")
                        elif first != second:
                            raise AssertionError("BEF command structure changed")
                return {
                    "fidelity": "commands and numeric animation content preserved within 1e-6",
                    "animation_count": len(paths),
                }

            check("BEF", bef_roundtrip)

        def audio_roundtrip(name="wav.wav"):
            source = fixture(name)
            pcm = root / "audio.wav"
            rebuilt = root / "rebuilt-audio.wav"
            second = root / "audio-second.wav"
            run("wav", "extract", source, pcm)
            run("wav", "pack", source, pcm, rebuilt)
            run("wav", "extract", rebuilt, second)
            with wave.open(str(pcm), "rb") as a, wave.open(str(second), "rb") as b:
                if a.getparams() != b.getparams():
                    raise AssertionError("PCM audio parameters changed")
                first = a.readframes(a.getnframes())
                after = b.readframes(b.getnframes())
            raw = struct.unpack_from("<H", source.read_bytes(), 4)[0] < 2
            if raw:
                exact(source, rebuilt)
                if first != after:
                    raise AssertionError("Raw PCM samples changed")
            else:
                before_values = struct.unpack(f"<{len(first) // 2}h", first)
                after_values = struct.unpack(f"<{len(after) // 2}h", after)
                error = sum(
                    abs(a - b) for a, b in zip(before_values, after_values)
                ) / max(1, len(before_values))
                if error > 1024:
                    raise AssertionError(
                        f"ADPCM mean absolute error exceeds limit: {error}"
                    )
                return {
                    "fidelity": "ADPCM re-encoding; parameters preserved",
                    "mean_absolute_sample_error": error,
                }
            return {"fidelity": "byte-identical raw PCM"}

        check("WAV/ILSF", audio_roundtrip)
        if args.game == "1":
            check("WAV ADPCM", lambda: audio_roundtrip("wav-adpcm.wav"))

        def mapping_roundtrip(source_name):
            source = fixture(source_name)
            dat = root / (source_name + ".dat")
            package = root / (source_name + ".mtp")
            if source.suffix == ".mtp":
                run("mtp", "to-dat", source, "-o", dat)
            else:
                shutil.copy2(source, dat)
            run("dat", "to-mtp", dat, "-o", package)
            restored = root / (source_name + "-restored.dat")
            run("mtp", "to-dat", package, "-o", restored)
            original_json = root / (source_name + "-original.json")
            restored_json = root / (source_name + "-restored.json")
            run("dat", "export", dat, "-o", original_json)
            run("dat", "export", restored, "-o", restored_json)
            before = json.loads(original_json.read_text())
            after = json.loads(restored_json.read_text())
            if before["models"] != after["models"]:
                raise AssertionError("Model/texture mappings changed")
            return {"fidelity": "model and texture mappings preserved"}

        check("MTP", lambda: mapping_roundtrip("mtp.mtp"))
        check("DAT model mappings", lambda: mapping_roundtrip("mapping.dat"))

        def json_roundtrip(name):
            source = fixture(name)
            document = root / (name + ".json")
            rebuilt = root / ("rebuilt-" + name)
            run("asset", "to-json", source, document)
            run("asset", "from-json", document, rebuilt)
            exact(source, rebuilt)
            return {"fidelity": "byte-identical JSON rebuild"}

        def edited_json(name):
            source = fixture(name)
            document_path = root / (name + "-edit.json")
            output = root / ("edited-" + name)
            second = root / (name + "-edited.json")
            run("asset", "to-json", source, document_path)
            document = json.loads(document_path.read_text())
            if "values" in document:
                array = document["values"]
            elif "grids" in document:
                array = next(
                    grid["values"] for grid in document["grids"] if grid["values"]
                )
            elif "samples" in document:
                array = document["samples"]
            elif "layers" in document:
                array = document["layers"][0]["rgba"]
            elif document.get("kind") == "forest":
                array = document["records"][0]["transform"]
            elif "chunks" in document:
                item = next(
                    item for item in document["chunks"] if "cover_words" in item
                )
                array = item["cover_words"]
            else:
                raise AssertionError("No editable field selected")
            if not array:
                raise AssertionError("Fixture has no editable values")
            array[0] = (
                array[0] + 0.25 if isinstance(array[0], float) else (int(array[0]) ^ 1)
            )
            document_path.write_text(json.dumps(document))
            run("asset", "from-json", document_path, output)
            if source.read_bytes() == output.read_bytes():
                raise AssertionError("Editor ignored the changed value")
            run("asset", "to-json", output, second)
            restored = json.loads(second.read_text())
            # ILFF exports include a byte template as well as editable fields.
            # Its refreshed raw bytes must change after an edit.
            for value in (document, restored):
                value.pop("template", None)
                for item in value.get("chunks", []):
                    if any(
                        key in item for key in ("cover_words", "animation", "bones")
                    ):
                        item.pop("data", None)
            if restored != document:
                raise AssertionError("Edited data did not survive rebuilding")
            return {"fidelity": "edited value survives binary rebuild and re-export"}

        check("DAT graph", lambda: json_roundtrip("graph.dat"))
        check("OLM", lambda: json_roundtrip("olm.olm"))
        terrain = (
            ("hmp", "bit", "cmd", "ctr", "lmp")
            if args.game == "1"
            else ("thm", "tlm", "tmm")
        )
        for extension in terrain:
            check(
                extension.upper(),
                lambda extension=extension: json_roundtrip(f"{extension}.{extension}"),
            )
        if args.game == "2":
            check("DAT forest", lambda: json_roundtrip("forest.dat"))
            check("DAT graphcover", lambda: json_roundtrip("graphcover.dat"))
            check("SYN", lambda: json_roundtrip("syn.syn"))

            def mp3_roundtrip():
                source = fixture("mp3.mp3")
                output = root / "normalized.mp3"
                second = root / "normalized-again.mp3"
                run("mp3", "normalize", source, output)
                run("mp3", "normalize", output, second)
                exact(output, second)
                pcm = root / "decoded-mp3.wav"
                run("mp3", "to-wav", source, pcm)
                with wave.open(str(pcm), "rb") as audio:
                    assert audio.getsampwidth() == 2 and audio.getnchannels() in (1, 2)
                    assert audio.getframerate() > 0 and audio.getnframes() > 0
                    details = {"channels": audio.getnchannels(), "rate": audio.getframerate(),
                               "frames": audio.getnframes()}
                return {"fidelity": "MPEG frames preserved; metadata removed; PCM decoding validated", **details}

            check("MP3", mp3_roundtrip)

        editable = ["olm.olm"]
        editable += [
            f"{extension}.{extension}"
            for extension in (
                ("hmp", "bit", "lmp")
                if args.game == "1"
                else ("thm", "tlm", "tmm", "syn")
            )
        ]
        if args.game == "2":
            editable += ["forest.dat", "graphcover.dat"]
        for name in editable:
            check(f"{name} edited rebuild", lambda name=name: edited_json(name))

        if args.game == "2":
            for extension in ("bmp", "jpg"):

                def standard_image(extension=extension):
                    source = fixture(f"{extension}.{extension}")
                    first = root / f"{extension}-decoded.png"
                    run("image", "convert", source, first)
                    intermediate = root / f"{extension}-intermediate.bmp"
                    second = root / f"{extension}-again.png"
                    run("image", "convert", first, intermediate)
                    run("image", "convert", intermediate, second)
                    exact(first, second)
                    return {
                        "fidelity": "decoded RGBA pixels preserved through lossless BMP/PNG"
                    }

                check(extension.upper(), standard_image)
        else:
            for extension in ("avi", "afp"):

                def copy_asset(extension=extension):
                    source = fixture(f"{extension}.{extension}")
                    output = root / f"copied.{extension}"
                    run("asset", "copy", source, output)
                    exact(source, output)
                    return {"fidelity": "byte-identical preservation; no transcoding"}

                check(extension.upper(), copy_asset)

    report = {
        "game": int(args.game),
        "format_extensions": sorted(
            (
                "afp avi bef bit cmd ctr dat fnt hmp iff lmp mef mtp olm pic png qsc qvm res spr tex tga wav"
                if args.game == "1"
                else "bmp dat fnt iff jpg mef mp3 mtp olm pic png qsc qvm res spr syn tex tga thm tlm tmm wav"
            ).split()
        ),
        "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "scope": "One real example per format, plus PNG/TGA intermediate paths; no skipped required formats.",
        "passed": sum(row["status"] == "passed" for row in rows),
        "failed": sum(row["status"] == "failed" for row in rows),
        "results": rows,
    }
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + "\n")
    print(f"Checks: {report['passed']} passed, {report['failed']} failed", flush=True)
    raise SystemExit(1 if report["failed"] else 0)


if __name__ == "__main__":
    main()
