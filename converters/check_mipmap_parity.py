"""Compare native texture creation with externally supplied original GConv outputs."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--binary', required=True, type=Path)
    parser.add_argument('--cases', required=True, type=Path)
    parser.add_argument('--report', required=True, type=Path)
    args = parser.parse_args()
    binary = args.binary.resolve()
    cases = args.cases.resolve()
    rows = []
    with tempfile.TemporaryDirectory(prefix='igi-mipmap-parity-') as directory:
        root = Path(directory)
        for index, case in enumerate(json.loads(cases.read_text())):
            source = (cases.parent / case['input']).resolve()
            reference = (cases.parent / case['reference']).resolve()
            before = source.read_bytes()
            output = root / f'case-{index}.tex'
            subprocess.run([str(binary), 'tex', 'make', str(source), str(output),
                            '--mipmaps'], check=True, capture_output=True, timeout=90)
            if output.read_bytes() != reference.read_bytes():
                raise AssertionError(f"Original GConv mismatch: {case['name']}")
            images = root / f'case-{index}-images'
            subprocess.run([str(binary), 'tex', 'extract', str(output), str(images)],
                           check=True, capture_output=True, timeout=90)
            if len(list(images.glob('*.tga'))) != case['levels']:
                raise AssertionError(f"Incomplete mip chain: {case['name']}")
            rejected = subprocess.run([str(binary), 'tex', 'make', str(source),
                                       str(source), '--mipmaps'], capture_output=True,
                                      timeout=90)
            if rejected.returncode == 0 or source.read_bytes() != before:
                raise AssertionError(f"Source protection failed: {case['name']}")
            rows.append({'name': case['name'], 'status': 'passed',
                         'mip_levels': case['levels'],
                         'reference_sha256': hashlib.sha256(reference.read_bytes()).hexdigest(),
                         'fidelity': 'byte-identical', 'source_overwrite_guard': 'passed'})
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps({'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
                                      'cases': rows}, indent=2))
    print(f'{len(rows)} original-reference mipmap checks passed')


if __name__ == '__main__':
    main()
