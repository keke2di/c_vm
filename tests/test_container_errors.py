import struct
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
COMPILER = "compiler.cli"
PACKER = "packer.pack"
STUB = ROOT / "vm_c" / "stub.exe"

HEADER_STRUCT = struct.Struct("<4sBBHIIII16s")


def run(command):
    return subprocess.run(
        command,
        cwd=ROOT,
        capture_output=True,
        text=True,
    )


def compile_valid(tmp):
    source = tmp / "test.py"
    cvm = tmp / "test.cvm"

    source.write_text(
        """
x = 123
print(x)
""".strip() + "\n",
        encoding="utf-8",
    )

    result = run([
        sys.executable,
        "-m",
        COMPILER,
        str(source),
        "-o",
        str(cvm),
    ])

    if result.returncode != 0:
        raise RuntimeError(result.stderr)

    return cvm


def pack(cvm, exe):
    result = run([
        sys.executable,
        "-m",
        PACKER,
        str(STUB),
        str(cvm),
        str(exe),
    ])

    if result.returncode != 0:
        raise RuntimeError(result.stderr)


def execute(exe):
    return subprocess.run(
        [str(exe)],
        cwd=ROOT,
        capture_output=True,
        text=True,
    )


def parse_container(data):
    if len(data) < HEADER_STRUCT.size:
        raise ValueError("container too small")

    (
        magic,
        version,
        pepper_id,
        flags,
        plain_len,
        ct_len,
        salt_len,
        nonce_len,
        salt,
    ) = HEADER_STRUCT.unpack_from(data, 0)

    offset = HEADER_STRUCT.size

    if len(data) < offset + nonce_len + ct_len:
        raise ValueError("container truncated")

    nonce = data[offset:offset + nonce_len]
    offset += nonce_len

    payload = data[offset:offset + ct_len]

    return {
        "magic": magic,
        "version": version,
        "pepper_id": pepper_id,
        "flags": flags,
        "plain_len": plain_len,
        "ct_len": ct_len,
        "salt_len": salt_len,
        "nonce_len": nonce_len,
        "salt": salt,
        "nonce": nonce,
        "payload": payload,
    }


def rebuild_container(info):
    header = HEADER_STRUCT.pack(
        info["magic"],
        info["version"],
        info["pepper_id"],
        info["flags"],
        info["plain_len"],
        info["ct_len"],
        info["salt_len"],
        info["nonce_len"],
        info["salt"],
    )

    return header + info["nonce"] + info["payload"]


def find_code_offset(payload):
    pos = 4

    for expected_tag in (0x01, 0x02):
        if pos + 5 > len(payload):
            raise RuntimeError("truncated section")

        tag = payload[pos]
        section_len = struct.unpack_from("<I", payload, pos + 1)[0]

        if tag != expected_tag:
            raise RuntimeError("unexpected section")

        pos += 5 + section_len

    if pos + 9 > len(payload) or payload[pos] != 0x03:
        raise RuntimeError("missing function section")

    pos += 5
    num_functions = struct.unpack_from("<I", payload, pos)[0]
    pos += 4

    if num_functions == 0:
        raise RuntimeError("no functions")

    if pos + 12 > len(payload):
        raise RuntimeError("truncated function")

    pos += 4
    pos += 2
    pos += 2

    code_len_offset = pos
    code_len = struct.unpack_from("<I", payload, pos)[0]
    pos += 4

    if code_len == 0:
        raise RuntimeError("empty function")

    if pos + code_len > len(payload):
        raise RuntimeError("truncated code")

    return pos, code_len_offset, code_len


def make_executable(tmp, data, name):
    cvm = tmp / f"{name}.cvm"
    exe = tmp / f"{name}.exe"

    cvm.write_bytes(data)
    pack(cvm, exe)

    return exe


def expect_runtime_failure(exe):
    result = execute(exe)

    if result.returncode == 0:
        raise AssertionError(
            f"expected runtime failure, got success with {result.stdout!r}"
        )


def main():
    tests = []

    with tempfile.TemporaryDirectory() as directory:
        tmp = Path(directory)

        cvm = compile_valid(tmp)
        original = cvm.read_bytes()
        info = parse_container(original)

        code_offset, _, code_len = find_code_offset(info["payload"])

        def invalid_opcode():
            mutated = dict(info)
            payload = bytearray(info["payload"])
            payload[code_offset] = 0xFE
            mutated["payload"] = bytes(payload)

            exe = make_executable(
                tmp,
                rebuild_container(mutated),
                "invalid_opcode",
            )
            expect_runtime_failure(exe)

        tests.append(("invalid_opcode", invalid_opcode))

        def invalid_constant_index():
            mutated = dict(info)
            payload = bytearray(info["payload"])
            payload[code_offset] = 0x01
            payload[code_offset + 1:code_offset + 5] = (
                0xFFFFFFFF
            ).to_bytes(4, "little")
            mutated["payload"] = bytes(payload)

            exe = make_executable(
                tmp,
                rebuild_container(mutated),
                "invalid_constant_index",
            )
            expect_runtime_failure(exe)

        tests.append(
            ("invalid_constant_index", invalid_constant_index)
        )

        def invalid_jump_target():
            mutated = dict(info)
            payload = bytearray(info["payload"])
            payload[code_offset] = 0x20
            payload[code_offset + 1:code_offset + 5] = (
                0xFFFFFFFF
            ).to_bytes(4, "little")
            mutated["payload"] = bytes(payload)

            exe = make_executable(
                tmp,
                rebuild_container(mutated),
                "invalid_jump_target",
            )
            expect_runtime_failure(exe)

        tests.append(("invalid_jump_target", invalid_jump_target))

        def invalid_local_slot():
            mutated = dict(info)
            payload = bytearray(info["payload"])
            payload[code_offset] = 0x30
            payload[code_offset + 1:code_offset + 5] = (
                0xFFFFFFFF
            ).to_bytes(4, "little")
            mutated["payload"] = bytes(payload)

            exe = make_executable(
                tmp,
                rebuild_container(mutated),
                "invalid_local_slot",
            )
            expect_runtime_failure(exe)

        tests.append(("invalid_local_slot", invalid_local_slot))

        def invalid_function_index():
            mutated = dict(info)
            payload = bytearray(info["payload"])
            payload[code_offset] = 0x40
            payload[code_offset + 1:code_offset + 5] = (
                0xFFFFFFFF
            ).to_bytes(4, "little")
            mutated["payload"] = bytes(payload)

            exe = make_executable(
                tmp,
                rebuild_container(mutated),
                "invalid_function_index",
            )
            expect_runtime_failure(exe)

        tests.append(
            ("invalid_function_index", invalid_function_index)
        )

        def bad_pepper_id():
            mutated = dict(info)
            mutated["pepper_id"] = 2

            exe = make_executable(
                tmp,
                rebuild_container(mutated),
                "bad_pepper_id",
            )
            expect_runtime_failure(exe)

        tests.append(("bad_pepper_id", bad_pepper_id))

        def bad_flags():
            mutated = dict(info)
            mutated["flags"] = 1

            exe = make_executable(
                tmp,
                rebuild_container(mutated),
                "bad_flags",
            )
            expect_runtime_failure(exe)

        tests.append(("bad_flags", bad_flags))

        def mismatched_plaintext_length():
            mutated = dict(info)
            mutated["plain_len"] = info["plain_len"] - 1

            exe = make_executable(
                tmp,
                rebuild_container(mutated),
                "mismatched_plaintext_length",
            )
            expect_runtime_failure(exe)

        tests.append(
            (
                "mismatched_plaintext_length",
                mismatched_plaintext_length,
            )
        )

        def trailing_physical_data():
            exe = make_executable(
                tmp,
                original + b"\x00",
                "trailing_physical_data",
            )
            expect_runtime_failure(exe)

        tests.append(
            (
                "trailing_physical_data",
                trailing_physical_data,
            )
        )

        passed = 0

        for name, test in tests:
            try:
                test()
                print(f"{name}: PASS")
                passed += 1
            except Exception as exc:
                print(f"{name}: FAIL")
                print(exc)

    print()
    print(f"{passed} passed, {len(tests) - passed} failed, {len(tests)} total")

    return 0 if passed == len(tests) else 1


if __name__ == "__main__":
    sys.exit(main())