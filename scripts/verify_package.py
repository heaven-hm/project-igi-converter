"""Verify a release archive and run its extracted native executable on real fixtures."""

import argparse
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--game", choices=("1", "2"), required=True)
    parser.add_argument("--samples", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    digest = hashlib.sha256(args.archive.read_bytes()).hexdigest()
    expected = args.archive.with_suffix(".zip.sha256").read_text().split()[0]
    if digest != expected:
        raise SystemExit("Archive checksum mismatch")
    name = f"igi{args.game}conv"
    with tempfile.TemporaryDirectory(prefix="igi-package-check-") as temporary:
        root = Path(temporary)
        with zipfile.ZipFile(args.archive) as archive:
            for item in archive.infolist():
                relative = Path(item.filename)
                if relative.is_absolute() or ".." in relative.parts:
                    raise SystemExit("Unsafe archive path")
                if relative.suffix.lower() in {".dll", ".pdb", ".res", ".qvm", ".mef"}:
                    raise SystemExit("Unexpected game asset or runtime dependency")
                archive.extract(item, root)
                mode = (item.external_attr >> 16) & 0o777
                if mode and not item.is_dir():
                    (root / relative).chmod(mode)
        folder = root / name
        binary = folder / (name + (".exe" if sys.platform == "win32" else ""))
        subprocess.run([str(binary), "--version"], check=True)
        subprocess.run([str(binary), "--help"], check=True, stdout=subprocess.DEVNULL)
        if sys.platform == "darwin":
            app_binary = folder / f"{name}.app" / "Contents" / "MacOS" / name
            if app_binary.read_bytes() != binary.read_bytes():
                raise SystemExit("App bundle differs from the tested CLI")
            binary = app_binary
        checker = Path(__file__).resolve().parents[1] / "verification/check_format_coverage.py"
        subprocess.run(
            [sys.executable, str(checker), "--binary", str(binary), "--game", args.game,
             "--samples", str(args.samples.resolve()), "--report", str(args.report.resolve())],
            check=True,
        )
    print("Release archive and extracted executable verified:", args.archive)


if __name__ == "__main__":
    main()
