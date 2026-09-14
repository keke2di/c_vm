import argparse
import os
import re
import shutil
import subprocess
from pathlib import Path


def find_rc():
    sdk_bin = os.environ.get("WindowsSdkVerBinPath")
    if sdk_bin:
        candidate = Path(sdk_bin) / "x64" / "rc.exe"
        if candidate.exists():
            return candidate
    found = shutil.which("rc.exe")
    return Path(found) if found else None


def write_version_header(root):
    init = (root.parent / "compiler" / "__init__.py").read_text(encoding="utf-8")
    match = re.search(r'__version__ = "(\d+)\.(\d+)\.(\d+)"', init)
    if not match:
        raise SystemExit("could not read __version__ from compiler/__init__.py")
    major, minor, patch = match.groups()
    (root / "stub_version.h").write_text(
        f"#define CVM_VERSION_MAJOR {major}\n"
        f"#define CVM_VERSION_MINOR {minor}\n"
        f"#define CVM_VERSION_PATCH {patch}\n"
        f'#define CVM_VERSION_STRING "{major}.{minor}.{patch}"\n',
        encoding="utf-8",
    )


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog="build.py",
        description="Build the cVM native runtime and executable stub",
    )
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument(
        "--release",
        action="store_true",
        help="Build an optimized release binary",
    )
    mode.add_argument(
        "--debug",
        action="store_true",
        help="Build a debug binary with VM debug logging",
    )
    args = parser.parse_args(argv)

    release = not args.debug

    vc_install = os.environ.get("VCToolsInstallDir")
    if not vc_install:
        print("VCToolsInstallDir not set. Run from Developer Command Prompt.")
        return 1

    root = Path(__file__).resolve().parent
    bin_dir = Path(vc_install) / "bin" / "HostX64" / "x64"
    cl_exe = bin_dir / "cl.exe"
    link_exe = bin_dir / "link.exe"
    lib_dir = Path(vc_install) / "lib" / "x64"

    ucrt_lib = os.environ.get("UCRTLIBPATH")
    um_lib = os.environ.get("UMLIBPATH")

    if not ucrt_lib or not um_lib:
        sdk_dir = os.environ.get("WindowsSdkDir")
        sdk_ver = os.environ.get("WindowsSDKVersion", "").strip("\\/")
        if sdk_dir and sdk_ver:
            sdk_root = Path(sdk_dir) / "Lib" / sdk_ver
            ucrt_lib = ucrt_lib or str(sdk_root / "ucrt" / "x64")
            um_lib = um_lib or str(sdk_root / "um" / "x64")

    if not ucrt_lib or not um_lib:
        print("Windows SDK library paths not found in the environment.")
        print("Run from an MSVC Developer Command Prompt.")
        return 1

    if not cl_exe.exists():
        print(f"cl not found at {cl_exe}")
        return 1

    sources = sorted(root.glob("*.c"))
    if not sources:
        print("No native source files found.")
        return 1

    objects = []

    for obj in root.glob("*.obj"):
        obj.unlink()

    for artifact in ("stub.exe", "stub.exp", "stub.lib", "stub.pdb", "stub.ilk", "stub.res"):
        path = root / artifact
        if path.exists():
            path.unlink()

    if release:
        compile_flags = [
            "/nologo",
            "/c",
            "/W3",
            "/GS",
            "/O2",
            "/GL",
            "/MT",
        ]
        link_flags = [
            "/nologo",
            "/LTCG",
            "/INCREMENTAL:NO",
            "/MACHINE:X64",
            "/SUBSYSTEM:CONSOLE",
            "/MANIFEST:NO",
        ]
        mode_name = "release"
    else:
        compile_flags = [
            "/nologo",
            "/c",
            "/W3",
            "/GS",
            "/Od",
            "/Zi",
            "/MTd",
            "/DCVM_DEBUG",
        ]
        link_flags = [
            "/nologo",
            "/INCREMENTAL",
            "/DEBUG",
            "/MACHINE:X64",
            "/SUBSYSTEM:CONSOLE",
            "/MANIFEST:NO",
        ]
        mode_name = "debug"

    for src in sources:
        if not src.exists():
            continue

        obj = src.with_suffix(".obj")

        cmd = [
            str(cl_exe),
            *compile_flags,
            f"/I{root}",
            f"/Fo{obj}",
            str(src),
        ]

        print(f"Compiling {src.name} ({mode_name})...")
        subprocess.check_call(cmd)
        objects.append(obj)

    rc_exe = find_rc()
    if not rc_exe:
        print("rc.exe not found. Run from Developer Command Prompt.")
        return 1

    write_version_header(root)
    resource = root / "stub.res"
    print("Compiling stub.rc...")
    subprocess.check_call([str(rc_exe), "/nologo", f"/fo{resource}", "stub.rc"], cwd=root)
    objects.append(resource)

    runtime_suffix = "" if release else "d"
    libs = [
        "kernel32.lib",
        f"libcmt{runtime_suffix}.lib",
        f"libvcruntime{runtime_suffix}.lib",
        f"libucrt{runtime_suffix}.lib",
    ]

    libpaths = [
        f"/LIBPATH:{lib_dir}",
        f"/LIBPATH:{ucrt_lib}",
        f"/LIBPATH:{um_lib}",
    ]

    output = root / "stub.exe"

    cmd = [
        str(link_exe),
        *link_flags,
        *libpaths,
        *libs,
        *(str(obj) for obj in objects),
        f"/OUT:{output}",
    ]

    print(f"Linking stub.exe ({mode_name})...")
    subprocess.check_call(cmd)

    print(f"stub.exe created successfully: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())