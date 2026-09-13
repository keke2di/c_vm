from __future__ import annotations

import struct

MAGIC = b"CVM2"
FORMAT_VERSION = 3

HEADER_STRUCT = struct.Struct("<4sBBHIIII16s")


def encode_container(bytecode: bytes, pepper_id: int = 1, flags: int = 0) -> bytes:
    salt = b"\x00" * 16
    nonce = b"\x00" * 12

    header = HEADER_STRUCT.pack(
        MAGIC,
        FORMAT_VERSION,
        pepper_id,
        flags,
        len(bytecode),
        len(bytecode),
        len(salt),
        len(nonce),
        salt,
    )

    return header + nonce + bytecode