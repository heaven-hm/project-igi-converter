"""Check a fully textured real IGI1 MEF bundle, plus genuinely missing textures."""
import argparse
import json
import re
import subprocess
import tempfile
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', required=True, type=Path)
    parser.add_argument('--model', required=True, type=Path)
    parser.add_argument('--dat', required=True, type=Path)
    parser.add_argument('--textures', required=True, type=Path)
    parser.add_argument('--reference-binary', type=Path)
    parser.add_argument('--report', required=True, type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='igi-bundle-textures-') as temporary:
        root = Path(temporary)

        def export(binary, name, textured):
            command = [str(binary.resolve()), 'mef', 'bundle', str(args.model.resolve()), '-o', str(root / name)]
            if textured:
                command += ['--dat', str(args.dat.resolve()), '--texdir', str(args.textures.resolve())]
            result = subprocess.run(command, capture_output=True, text=True, timeout=90, check=True)
            output = root / name / args.model.stem
            obj = (output / (args.model.stem + '.obj')).read_text()
            used = set()
            material = None
            for line in obj.splitlines():
                if line.startswith('usemtl mat_'):
                    material = int(line.removeprefix('usemtl mat_'))
                elif line.startswith('f '):
                    if material is None:
                        raise AssertionError('Exported face has no material')
                    used.add(material)
            warnings = set(map(int, re.findall(r'warning: texture slot (\d+)', result.stdout + result.stderr)))
            return output, used, warnings

        complete, used, warnings = export(args.binary, 'complete', True)
        if not used or warnings:
            raise AssertionError(f'Fully textured fixture has missing materials: {sorted(warnings)}')
        mtl = (complete / (args.model.stem + '.mtl')).read_text()
        for texture in re.findall(r'^map_Kd (.+)$', mtl, re.MULTILINE):
            if not (complete / texture).is_file():
                raise AssertionError(f'Referenced texture missing: {texture}')
        missing, missing_used, missing_warnings = export(args.binary, 'missing', False)
        if used != missing_used or missing_warnings != used:
            raise AssertionError('Missing-texture diagnostics do not match face materials')
        if (complete / (args.model.stem + '.obj')).read_bytes() != (missing / (args.model.stem + '.obj')).read_bytes():
            raise AssertionError('Texture availability changed geometry')
        if args.reference_binary:
            reference, reference_used, _ = export(args.reference_binary, 'reference', True)
            if used != reference_used:
                raise AssertionError('Referenced material indices changed')
            files = {p.name for p in reference.iterdir() if p.is_file()}
            if files != {p.name for p in complete.iterdir() if p.is_file()}:
                raise AssertionError('Bundle file inventory changed')
            for name in files:
                if (reference / name).read_bytes() != (complete / name).read_bytes():
                    raise AssertionError(f'Bundle bytes changed: {name}')
        report = {'used_materials': len(used), 'complete_warnings': len(warnings),
                  'missing_warnings': len(missing_warnings),
                  'reference_byte_identical': bool(args.reference_binary)}
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
