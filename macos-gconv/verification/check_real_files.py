"""Run the native CLI against the supplied IGI 2 fixtures, without Python libraries."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--binary', type=Path, default=Path(__file__).resolve().parents[1] / 'build/gconv')
parser.add_argument('--samples', type=Path, default=Path('/private/tmp/neonix-user-samples'))
parser.add_argument('--demo', type=Path, default=Path('/private/tmp/neonix-igi2-download/samples'))
parser.add_argument('--downloads', type=Path, default=Path.home() / 'Downloads')
parser.add_argument('--report', type=Path, required=True)
parser.add_argument('--qsc', type=Path, default=Path('/private/tmp/neonix-level5-check/all-qsc'))
parser.add_argument('--level5', type=Path, default=Path.home() / 'Downloads/LEVEL5')
args = parser.parse_args()
records = []
def run(*arguments):
    result = subprocess.run([str(args.binary.resolve()), *map(str, arguments)], capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError(result.stderr or result.stdout)
    return result.stdout

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

with tempfile.TemporaryDirectory(prefix='gconv-cpp17-') as temporary:
    output = Path(temporary)
    fonts = [p for name in ('fnt-files', 'more-sprites') for p in (args.samples / name).rglob('*.fnt')]
    fonts += list((args.demo / 'fonts').rglob('*.fnt'))
    if not fonts:
        raise RuntimeError('No real font fixtures found')
    for n, source in enumerate(fonts):
        for extension in ('png', 'tga'):
            image = output / f'{n}.{extension}'
            packed = output / f'{n}-{extension}.fnt'
            run('fnt', 'extract', source, image)
            run('fnt', 'pack', source, image, packed)
            assert source.read_bytes() == packed.read_bytes(), str(source)
            records.append({'font': source.name, 'source_sha256': digest(source), 'format': extension, 'byte_identical': True})
    textures = [p for name in ('more-sprites', 'sprites-res', 'menusystem-res', 'menusystem1-res', 'tex-files')
                for p in (args.samples / name).rglob('*') if p.suffix.lower() in ('.spr', '.pic', '.tex')]
    textures += list((args.demo / 'spr').rglob('*.spr'))
    if not textures:
        raise RuntimeError('No real texture fixtures found')
    for n, source in enumerate(textures):
        directory = output / f'texture-{n}'
        run('tex', 'extract', source, directory)
        packed = output / f'texture-{n}{source.suffix}'
        run('tex', 'pack', source, directory, packed)
        assert source.read_bytes() == packed.read_bytes(), str(source)
        records.append({'texture': source.name, 'source_sha256': digest(source), 'byte_identical': True})
    for n, name in enumerate(('sprites.res', 'menusystem.res', 'menusystem (1).res')):
        source = args.downloads / name
        directory, extracted = output / f'res-{n}', output / f'repacked-{n}'
        packed = output / f'res-{n}.res'
        run('res', 'extract', source, directory)
        run('res', 'pack', directory, packed)
        run('res', 'extract', packed, extracted)
        original = {p.relative_to(directory): p.read_bytes() for p in directory.rglob('*') if p.is_file()}
        rebuilt = {p.relative_to(extracted): p.read_bytes() for p in extracted.rglob('*') if p.is_file()}
        assert original == rebuilt, str(source)
        records.append({'resource': source.name, 'source_sha256': digest(source), 'files': len(original), 'payloads_identical': True})
    qsc_files = list(args.qsc.rglob('*.qsc'))
    for n, source in enumerate(qsc_files):
        packed = output / f'script-{n}.qvm'
        run('qsc', 'compile', source, packed)
        originals = list(args.level5.rglob(source.stem + '.qvm'))
        assert any(packed.read_bytes() == p.read_bytes() for p in originals), str(source)
        records.append({'qsc': source.name, 'original_byte_identical': True})
    oracle_input = Path('/private/tmp/gconv-native-test/editor.tga')
    oracle_output = Path('/private/tmp/gconv-native-test/image/editor.tex')
    if oracle_input.exists() and oracle_output.exists():
        native = output / 'native.tex'
        run('tex', 'make', oracle_input, native)
        original = oracle_output.read_bytes()
        actual = native.read_bytes()
        assert actual[32:] == original[32:len(actual)]
        assert actual[:20] == original[:20] and actual[22:32] == original[22:32]
        records.append({'original_dll_texture_oracle': True, 'base_pixels_identical': True, 'note': 'Native make emits base level only; original defaults include mipmaps.'})
args.report.parent.mkdir(parents=True, exist_ok=True)
args.report.write_text(json.dumps(records, indent=2) + '\n')
print(f'PASS: {len(fonts)} fonts / {len(fonts)*2} round trips, {len(textures)} texture round trips, 3 resource payload round trips, {len(qsc_files)} original QVM matches')
