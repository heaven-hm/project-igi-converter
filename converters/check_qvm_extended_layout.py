"""Check an external IGI2 QVM fixture containing an extended payload.

No proprietary fixture is included. An optional launcher supports CrossOver.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--launcher', type=Path)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    binary, source = args.binary.resolve(), args.source.resolve()
    original = source.read_bytes()
    source_hash = hashlib.sha256(original).hexdigest()
    old_offset = struct.unpack_from('<I', original, 60)[0]
    if not 64 <= old_offset < len(original):
        raise ValueError('Fixture must contain an IGI2 extended payload')
    tail = original[old_offset:]
    evidence = []

    def run(*command):
        with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
            return subprocess.run(
                [str((args.launcher or binary).resolve()), *map(str, command)],
                stdout=stdout, stderr=stderr, timeout=90,
            ).returncode

    with tempfile.TemporaryDirectory() as folder:
        root = Path(folder).resolve()
        script, output = root / 'source.qsc', root / 'rebuilt.qvm'
        assert run('qvm', 'decompile', source, '-o', script) == 0
        assert run('qsc', 'compile', script, output) == 0
        assert output.read_bytes() == original
        evidence.append('Extended QVM round trip is byte-identical')

        text = script.read_text()
        match = re.search(r'"[^"\\\r\n]+"', text)
        if not match:
            raise ValueError('Fixture needs a nonempty string literal to edit')
        script.write_text(text[:match.end() - 1] + 'X' + text[match.end() - 1:])
        assert run('qsc', 'compile', script, output) == 0
        changed = output.read_bytes()
        new_offset = struct.unpack_from('<I', changed, 60)[0]
        assert new_offset != old_offset
        assert new_offset == len(changed) - len(tail)
        assert changed[new_offset:] == tail
        evidence.append('Edited script relocates and preserves the payload')

        layout = Path(str(script) + '.qvm-layout')
        document = json.loads(layout.read_text())
        document['trailer_offset'] = 1
        layout.write_text(json.dumps(document))
        before = output.read_bytes()
        assert run('qsc', 'compile', script, output) != 0
        assert output.read_bytes() == before
        evidence.append('Invalid metadata preserves existing binary output')

        layout.write_text('{"unrelated":true}')
        script.write_text('preserve existing source')
        assert run('qvm', 'decompile', source, '-o', script) != 0
        assert script.read_text() == 'preserve existing source'
        evidence.append('Unrelated metadata preserves existing source output')

    assert hashlib.sha256(source.read_bytes()).hexdigest() == source_hash
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps({
        'binary_sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
        'source_sha256': source_hash, 'checks': evidence, 'passed': len(evidence),
    }, indent=2) + '\n')
    print(f'{len(evidence)} extended QVM checks passed')


if __name__ == '__main__':
    main()
