import struct
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
COMPILER = "compiler.cli"
PACKER = "packer.pack"
STUB = ROOT / "vm_c" / "stub.exe"

PROGRAM = """
def greet(name, punctuation="!"):
    return "Hello " + name + punctuation

print(greet("cVM"))
""".lstrip()


def run(command):
    return subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=120)


def read_imports(data):
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    section_count = struct.unpack_from("<H", data, pe + 6)[0]
    optional_size = struct.unpack_from("<H", data, pe + 20)[0]
    optional = pe + 24
    magic = struct.unpack_from("<H", data, optional)[0]
    directories = optional + (112 if magic == 0x20B else 96)
    import_rva = struct.unpack_from("<I", data, directories + 8)[0]
    table = optional + optional_size
    sections = [
        struct.unpack_from("<8sIIII", data, table + index * 40)
        for index in range(section_count)
    ]

    def offset(rva):
        for _, virtual_size, virtual_address, raw_size, raw_pointer in sections:
            if virtual_address <= rva < virtual_address + max(virtual_size, raw_size):
                return raw_pointer + rva - virtual_address
        raise ValueError(f"RVA {rva:#x} is outside every section")

    dlls = []
    position = offset(import_rva)
    while True:
        descriptor = struct.unpack_from("<IIIII", data, position)
        if descriptor[3] == 0:
            break
        name = offset(descriptor[3])
        dlls.append(data[name:data.index(b"\0", name)].decode("ascii"))
        position += 20
    return dlls


def check_imports_only_kernel32(data):
    dlls = [dll.upper() for dll in read_imports(data)]
    if dlls != ["KERNEL32.DLL"]:
        raise AssertionError(f"stub imports {dlls}")


def check_no_temp_file_apis(data):
    for name in (b"GetTempPath", b"GetTempFileName"):
        if name in data:
            raise AssertionError(f"stub references {name.decode()}")


def check_version_resource(data):
    for text in ("FileDescription", "cVM application", "ProductVersion"):
        if text.encode("utf-16-le") not in data:
            raise AssertionError(f"version resource is missing {text!r}")


def check_manifest(data):
    if b'requestedExecutionLevel level="asInvoker"' not in data:
        raise AssertionError("manifest is missing")


def check_packed_program(tmp):
    source = tmp / "program.py"
    cvm = tmp / "program.cvm"
    exe = tmp / "program.exe"
    source.write_text(PROGRAM, encoding="utf-8")

    result = run([sys.executable, "-m", COMPILER, str(source), "-o", str(cvm)])
    if result.returncode != 0:
        raise AssertionError(f"compile failed: {result.stderr}")

    result = run([sys.executable, "-m", PACKER, str(STUB), str(cvm), str(exe)])
    if result.returncode != 0:
        raise AssertionError(f"pack failed: {result.stderr}")

    expected = run([sys.executable, str(source)])
    actual = run([str(exe)])
    if actual.returncode != 0 or actual.stdout != expected.stdout:
        raise AssertionError(
            f"expected {expected.stdout!r}, got {actual.stdout!r} "
            f"(exit {actual.returncode}, stderr {actual.stderr!r})"
        )


def main():
    quiet = "--quiet" in sys.argv[1:]

    if not STUB.exists():
        print(f"stub.exe not found: {STUB}")
        print("Build the VM first.")
        return 1

    data = STUB.read_bytes()

    with tempfile.TemporaryDirectory(prefix="cvm_stub_") as directory:
        tmp = Path(directory)
        tests = [
            ("imports_only_kernel32", lambda: check_imports_only_kernel32(data)),
            ("no_temp_file_apis", lambda: check_no_temp_file_apis(data)),
            ("version_resource", lambda: check_version_resource(data)),
            ("manifest", lambda: check_manifest(data)),
            ("packed_program_matches_cpython", lambda: check_packed_program(tmp)),
        ]

        passed = 0

        for name, test in tests:
            try:
                test()
                passed += 1
                if not quiet:
                    print(f"{name}: PASS")
            except Exception as exc:
                print(f"{name}: FAIL")
                print(exc)

    if not quiet:
        print(f"stub size: {len(data)} bytes")

    print()
    print(f"{passed} passed, {len(tests) - passed} failed, {len(tests)} total")
    return 0 if passed == len(tests) else 1


if __name__ == "__main__":
    sys.exit(main())
