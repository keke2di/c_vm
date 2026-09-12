import argparse
import os
import subprocess
from pathlib import Path


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

    sources = [
        root / "value.c",
        root / "vm.c",
        root / "loader.c",
        root / "stub.c",
    ]

    objects = []

    for obj in root.glob("*.obj"):
        obj.unlink()

    for artifact in ("stub.exe", "stub.exp", "stub.lib", "stub.pdb", "stub.ilk"):
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
            "/MD",
        ]
        link_flags = [
            "/nologo",
            "/LTCG",
            "/INCREMENTAL:NO",
            "/MACHINE:X64",
            "/SUBSYSTEM:CONSOLE",
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
            "/MD",
            "/DCVM_DEBUG",
        ]
        link_flags = [
            "/nologo",
            "/INCREMENTAL",
            "/DEBUG",
            "/MACHINE:X64",
            "/SUBSYSTEM:CONSOLE",
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

    if not objects:
        print("No native source files found.")
        return 1

    libs = [
        "kernel32.lib",
        "user32.lib",
        "advapi32.lib",
        "bcrypt.lib",
        "msvcrt.lib",
        "vcruntime.lib",
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