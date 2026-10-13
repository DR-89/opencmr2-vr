#!/usr/bin/env python3
"""Import original PC assets into the local Quest debug app without shared-storage permissions."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import tarfile
import tempfile
import uuid


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--adb', default='adb')
    parser.add_argument('--serial')
    args = parser.parse_args()
    root = args.data.resolve(strict=True)
    required = ('Game/Bigfiles/common.bfl', 'FrontEnd/common.bfl', 'Sounds/surface.bfl')
    for relative in required:
        if not (root / relative).is_file():
            parser.error(f'Missing original game data: {relative}')
    adb = [args.adb] + (['-s', args.serial] if args.serial else [])
    run_as = ['run-as', 'org.opencmr2.vr']
    subprocess.run(adb + ['shell'] + run_as + ['mkdir', '-p', 'files/Data'], check=True)
    files = sorted(path for path in root.rglob('*') if path.is_file() and not path.is_symlink())
    total = sum(path.stat().st_size for path in files)
    print(f'Transferring {len(files)} files ({total / 1024**2:.1f} MiB) to private app storage.', flush=True)
    # ADB's sync transport verifies completion; a large exec-in stream can be
    # silently truncated if the Windows ADB server restarts during the copy.
    remote = f'/data/local/tmp/opencmr2-vr-{uuid.uuid4().hex}.tar'
    try:
        with tempfile.TemporaryDirectory(prefix='opencmr2-quest-') as temporary:
            archive_path = Path(temporary) / 'data.tar'
            with tarfile.open(archive_path, mode='w', format=tarfile.GNU_FORMAT) as archive:
                for path in files:
                    archive.add(path, arcname=path.relative_to(root).as_posix(), recursive=False)
            subprocess.run(adb + ['push', str(archive_path), remote], check=True)
            # The shell opens the temporary file; extraction runs as the app.
            # The remote name is generated above and contains no shell input.
            subprocess.run(adb + ['shell', f'run-as org.opencmr2.vr tar -x -C files/Data < {remote}'], check=True)
    finally:
        subprocess.run(adb + ['shell', 'rm', '-f', remote], check=False)
    for relative in required:
        expected = hashlib.sha256((root / relative).read_bytes()).hexdigest()
        actual = subprocess.check_output(adb + ['shell'] + run_as + ['sha256sum', f'files/Data/{relative}'], text=True).split()[0]
        if actual != expected:
            raise RuntimeError(f'Transfer verification failed: {relative}')
    print('Transfer complete; required archive SHA-256 checksums match.', flush=True)


if __name__ == '__main__':
    main()
