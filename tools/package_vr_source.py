#!/usr/bin/env python3
"""Archive committed VR sources and pinned dependencies; never traverse build output."""
import argparse
import io
from pathlib import Path
import subprocess
import tarfile

ROOT = Path(__file__).resolve().parents[1]
DEPS = {
    "SDL": "5180c654d439ad7df5eb99481fd0e3f027db65de",
    "OpenXR-SDK": "f2448a8797c85814aa892efc1ab8707900fbcc78",
    "glslang": "b5782e52ee2f7b3e40bb9c80d15b47016e008bc9",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Output already exists")
    if not args.output.name.endswith(".tar.gz"):
        parser.error("Use a .tar.gz output")
    prefix = args.output.name.removesuffix(".tar.gz") + "/"
    archives = [(ROOT, "HEAD", prefix)]
    archives.extend((ROOT / "build/deps" / name, revision, prefix + "build/deps/" + name + "/")
                    for name, revision in DEPS.items())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(args.output, "w:gz") as target:
        for repo, revision, folder in archives:
            actual = subprocess.check_output(["git", "-C", str(repo), "rev-parse", "HEAD"], text=True).strip()
            if revision != "HEAD" and actual != revision:
                raise ValueError(f"Wrong dependency revision: {repo}")
            data = subprocess.check_output(["git", "-C", str(repo), "archive", "--format=tar", revision])
            with tarfile.open(fileobj=io.BytesIO(data), mode="r:") as source:
                for entry in source:
                    body = source.extractfile(entry) if entry.isfile() else None
                    entry.name = folder + entry.name
                    if entry.islnk():
                        entry.linkname = folder + entry.linkname
                    target.addfile(entry, body)
    print(args.output)


if __name__ == "__main__":
    main()
