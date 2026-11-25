"""Exercise every distinct asset in an externally supplied corpus manifest.

Fixtures and detailed reports remain local. This checks CLI conversion fidelity,
not GUI interaction or acceptance of edited assets by the original games.
"""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import wave

from check_format_coverage import resource_payloads


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--launcher', type=Path)
    parser.add_argument('--game', type=int, choices=(1, 2), required=True)
    parser.add_argument('--report', type=Path, required=True)
    parser.add_argument('--extension', action='append')
    args = parser.parse_args()
    rows = json.loads(args.manifest.read_text())['files']
    unique = {}
    for row in rows:
        if row['game'] == args.game and (
            not args.extension or row['extension'] in args.extension
        ):
            unique.setdefault((row['sha256'], row['extension']), row)
    executable = (args.launcher or args.binary).resolve()
    results = []
    args.report.parent.mkdir(parents=True, exist_ok=True)

    def run(*command):
        with tempfile.TemporaryFile() as stdout, tempfile.TemporaryFile() as stderr:
            result = subprocess.run(
                [str(executable), *map(str, command)], stdout=stdout,
                stderr=stderr, timeout=90,
            )
            if result.returncode:
                stdout.seek(0)
                stderr.seek(0)
                detail = (stdout.read() + stderr.read()).decode('utf-8', 'replace')
                raise AssertionError(f'{command[0:2]}: exit {result.returncode}: {detail[-3000:]}')

    def exact(first, second):
        if digest(first) != digest(second):
            raise AssertionError('Round trip changed native bytes')

    def exercise(source, root):
        ext = source.suffix.lower()
        rebuilt = root / ('rebuilt' + ext)
        document = root / 'asset.json'
        if ext == '.qvm':
            text = root / 'script.qsc'
            run('qvm', 'decompile', source, '-o', text)
            if any(line.lstrip().startswith('//') for line in text.read_text().splitlines()):
                raise AssertionError('Decompiled script contains comments')
            run('qsc', 'compile', text, '-o', rebuilt) if args.game == 1 else run('qsc', 'compile', text, rebuilt)
            exact(source, rebuilt)
        elif ext == '.qsc':
            binary = root / 'script.qvm'
            run('qsc', 'compile', source, '-o', binary) if args.game == 1 else run('qsc', 'compile', source, binary)
            run('qvm', 'decompile', binary, '-o', root / 'script.qsc')
            run('qsc', 'compile', root / 'script.qsc', '-o', rebuilt) if args.game == 1 else run('qsc', 'compile', root / 'script.qsc', rebuilt)
            exact(binary, rebuilt)
        elif ext == '.res':
            directory = root / 'contents'
            run('res', 'extract', source, directory)
            run('res', 'pack', directory, rebuilt)
            if resource_payloads(source) != resource_payloads(rebuilt):
                raise AssertionError('Resource names or payloads changed')
        elif ext in ('.tex', '.spr', '.pic', '.anm'):
            run('tex', 'extract', source, root / 'bitmaps')
            run('tex', 'pack', source, root / 'bitmaps', rebuilt)
            exact(source, rebuilt)
        elif ext == '.fnt':
            for bitmap in ('atlas.png', 'atlas.tga'):
                run('fnt', 'extract', source, root / bitmap)
                run('fnt', 'pack', source, root / bitmap, rebuilt)
                exact(source, rebuilt)
        elif ext == '.wav':
            first, second = root / 'pcm.wav', root / 'second.wav'
            run('wav', 'extract', source, first)
            run('wav', 'pack', source, first, rebuilt)
            run('wav', 'extract', rebuilt, second)
            with wave.open(str(first)) as a, wave.open(str(second)) as b:
                if a.getparams() != b.getparams():
                    raise AssertionError('PCM parameters changed')
                before, after = a.readframes(a.getnframes()), b.readframes(b.getnframes())
            if source.read_bytes()[:4] == b'ILSF' and struct.unpack_from('<H', source.read_bytes(), 4)[0] >= 2:
                values = struct.unpack(f'<{len(before)//2}h', before)
                changed = struct.unpack(f'<{len(after)//2}h', after)
                error = sum(abs(x-y) for x, y in zip(values, changed)) / max(1, len(values))
                if error > 1024:
                    raise AssertionError(f'ADPCM mean sample error {error} exceeds 1024')
                return {'fidelity': 'PCM parameters preserved; ADPCM re-encoded', 'mean_error': error}
            exact(first, second)
        elif ext == '.mp3':
            run('mp3', 'normalize', source, rebuilt)
            run('mp3', 'normalize', rebuilt, root / 'second.mp3')
            exact(rebuilt, root / 'second.mp3')
            run('mp3', 'to-wav', source, root / 'pcm.wav')
            with wave.open(str(root / 'pcm.wav')) as pcm:
                if pcm.getnframes() == 0:
                    raise AssertionError('Decoded audio is empty')
        elif ext == '.mtp' or (ext == '.dat' and source.read_bytes().lstrip().startswith(b'***')):
            mapping = root / 'mapping.dat'
            if ext == '.mtp':
                run('mtp', 'to-dat', source, '-o', mapping)
            else:
                shutil.copyfile(source, mapping)
            run('dat', 'to-mtp', mapping, '-o', rebuilt)
            run('mtp', 'to-dat', rebuilt, '-o', root / 'again.dat')
            run('dat', 'export', mapping, '-o', document)
            run('dat', 'export', root / 'again.dat', '-o', root / 'again.json')
            if json.loads(document.read_text()) != json.loads((root / 'again.json').read_text()):
                raise AssertionError('Mapping metadata changed')
        elif ext in ('.png', '.tga', '.bmp', '.jpg'):
            run('image', 'convert', source, root / 'first.png')
            run('image', 'convert', root / 'first.png', root / 'image.bmp')
            run('image', 'convert', root / 'image.bmp', root / 'second.png')
            exact(root / 'first.png', root / 'second.png')
        elif ext in ('.avi', '.afp'):
            run('asset', 'copy', source, rebuilt)
            exact(source, rebuilt)
        elif ext == '.bef':
            return {'status': 'uncovered', 'reason': 'Standalone BEF needs its sibling hierarchy and clips; covered by IFF bundle matrix'}
        else:
            family = 'iff' if ext == '.iff' else 'asset'
            run(family, 'to-json', source, document)
            run(family, 'from-json', document, rebuilt)
            exact(source, rebuilt)
            if ext == '.mef':
                model = root / 'model.obj'
                run('mef', 'export', source, '-o', model)
                lines = model.read_text().splitlines()
                if not any(line.startswith('v ') for line in lines) or not any(line.startswith('f ') for line in lines):
                    raise AssertionError('Model export has no vertices or faces')
            data = json.loads(document.read_text())
            edited = False
            if data.get('kind') == 'graph' and data.get('layout') == 'legacy-tagged':
                record = next((record for record in data['records'] if record['type'] == 0x0808), None)
                if record:
                    record['value'][0] += 0.125
                    edited = True
            elif ext == '.olm' and data.get('layers'):
                data['layers'][0]['rgba'][0] ^= 1
                edited = True
            if edited:
                document.write_text(json.dumps(data))
                run(family, 'from-json', document, rebuilt)
                if source.read_bytes() == rebuilt.read_bytes():
                    raise AssertionError('Rebuild ignored the edited value')
                run(family, 'to-json', rebuilt, root / 'edited.json')
                if json.loads((root / 'edited.json').read_text()) != data:
                    raise AssertionError('Edited data or untouched metadata changed during rebuild')
            return {'fidelity': 'byte-identical; edited value preserved' if edited else 'byte-identical', 'layout': {
                key: value for key, value in data.items()
                if key in ('format', 'version', 'type', 'meshType', 'magic') and isinstance(value, (str, int))
            }}
        return {'fidelity': 'conversion round trip preserved'}

    with args.report.open('w') as output:
        output.write(json.dumps({'event': 'start', 'game': args.game,
            'binary_sha256': digest(args.binary), 'assets': len(unique)}) + '\n')
        for index, row in enumerate(unique.values(), 1):
            result = {**row, 'status': 'passed'}
            try:
                with tempfile.TemporaryDirectory(prefix='igi-corpus-') as temporary:
                    root = Path(temporary)
                    source = root / ('source' + row['extension'])
                    shutil.copyfile(row['path'], source)
                    if digest(source) != row['sha256']:
                        raise AssertionError('Fixture changed since inventory')
                    result.update(exercise(source, root))
                    if digest(source) != row['sha256']:
                        raise AssertionError('Converter modified input')
            except Exception as error:
                result.update(status='failed', error=str(error))
            output.write(json.dumps(result) + '\n')
            output.flush()
            results.append(result)
            if index % 50 == 0 or result['status'] != 'passed':
                print(index, '/', len(unique), row['extension'], result['status'], result.get('error', '')[:180], flush=True)
        summary = {state: sum(r['status'] == state for r in results) for state in ('passed', 'failed', 'uncovered')}
        output.write(json.dumps({'event': 'summary', **summary}) + '\n')
        print(summary, flush=True)
    raise SystemExit(1 if summary['failed'] or summary['uncovered'] else 0)


if __name__ == '__main__':
    main()
