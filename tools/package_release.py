#!/usr/bin/env python3
"""Build a release archive: the executable, the libraries it needs and the
licences. Players choose their own game installation on first launch.

On Windows (an .exe binary, requires Python pefile) the non-system DLLs are
followed recursively and each must resolve to an x64 file. On Linux and
macOS, pass the SDL3 library built next to the executable with --library.
The archive is a .zip or a .tar.gz, from the --output name.
"""
import argparse
import hashlib
from pathlib import Path
import tarfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
SYSTEM_DLLS = set("""advapi32 bcrypt cfgmgr32 comdlg32 crypt32 d3d11 d3d12
    dbghelp dwmapi dxgi gdi32 hid imm32 iphlpapi kernel32 msvcrt ntdll ole32
    oleaut32 powrprof psapi rpcrt4 secur32 setupapi shell32 shlwapi ucrtbase
    user32 userenv version winhttp winmm winspool wintrust ws2_32 wtsapi32
    xinput1_4""".split())


def imports(path):
    import pefile
    with pefile.PE(str(path)) as pe:
        if pe.FILE_HEADER.Machine != 0x8664:
            raise ValueError(f"Not a Windows x64 executable/DLL: {path}")
        return [entry.dll.decode("ascii") for attribute in
                ("DIRECTORY_ENTRY_IMPORT", "DIRECTORY_ENTRY_DELAY_IMPORT")
                for entry in getattr(pe, attribute, [])]


def dll_dependencies(binary, directories):
    available = {}
    for directory in directories:
        for path in directory.glob("*"):
            if path.is_file() and path.suffix.lower() == ".dll":
                available.setdefault(path.name.lower(), path)
    files = {binary.name.lower(): binary}
    pending = [binary]
    while pending:
        for name in imports(pending.pop()):
            key = name.lower()
            if key in files or key.removesuffix(".dll") in SYSTEM_DLLS or \
                    key.startswith(("api-ms-win-", "ext-ms-win-")):
                continue
            if key not in available:
                raise ValueError(f"Missing runtime DLL {name}; add its directory with --dll-dir")
            files[key] = available[key]
            pending.append(available[key])
    return list(files.values())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", required=True, type=Path)
    parser.add_argument("--dll-dir", action="append", default=[], type=Path)
    parser.add_argument("--library", action="append", default=[], type=Path)
    parser.add_argument("--license", action="append", default=[], type=Path)
    parser.add_argument("--include", action="append", default=[], type=Path,
                        help="additional file placed beside the executable (repeatable)")
    parser.add_argument("--output", required=True, type=Path, help="archive to create")
    args = parser.parse_args()
    binary = args.binary.resolve(strict=True)
    name = args.output.name
    if name.endswith(".tar.gz"):
        folder = name.removesuffix(".tar.gz")
    elif name.endswith(".zip"):
        folder = name.removesuffix(".zip")
    else:
        parser.error("The output must be a .zip or .tar.gz")
    if args.output.exists():
        parser.error(f"Output already exists: {args.output}")
    for path in [*args.library, *args.license, *args.include]:
        if not path.is_file():
            parser.error(f"Missing file: {path}")

    if binary.suffix.lower() == ".exe":
        files = dll_dependencies(binary, [binary.parent, *args.dll_dir])
        # OpenXR is loaded dynamically by SDL, so it is not an executable import.
        for library in args.library:
            files += dll_dependencies(library, [library.parent, binary.parent, *args.dll_dir])
        files = list({path.name.lower(): path for path in files}.values())
    else:
        files = [binary, *args.library]
    entries = [(path, f"{folder}/{path.name}") for path in files]
    entries += [(path, f"{folder}/{path.name}") for path in args.include]
    licenses = [(ROOT / "LICENSE", "OpenCMR2-LICENSE"),
                (ROOT / "third_party/enet/LICENSE", "ENet-LICENSE"),
                (ROOT / "src/video/COPYING.LGPLv2.1", "Bink-decoder-COPYING.LGPLv2.1")]
    licenses += [(path, f"{index + 1}-{path.name}") for index, path in enumerate(args.license)]
    entries += [(path, f"{folder}/Licenses/{license_name}") for path, license_name in licenses]

    # Symbolic links (such as libSDL3.so.0) are stored as the files they name.
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if name.endswith(".zip"):
        with zipfile.ZipFile(args.output, "w", zipfile.ZIP_DEFLATED) as archive:
            for path, arcname in entries:
                archive.write(path.resolve(), arcname)
    else:
        with tarfile.open(args.output, "w:gz", dereference=True) as archive:
            for path, arcname in entries:
                archive.add(path, arcname)
    digest = hashlib.sha256(args.output.read_bytes()).hexdigest()
    Path(f"{args.output}.sha256").write_text(f"{digest}  {name}\n")
    print(f"{args.output}: {', '.join(path.name for path in files)}")


if __name__ == "__main__":
    main()
