import os
import sys
import argparse
import struct
from pathlib import Path

def pack(stub_path: str, container_path: str, output_path: str) -> None:
    with open(stub_path, 'rb') as f:
        stub_data = f.read()

    with open(container_path, 'rb') as f:
        container_data = f.read()

    output_parent = Path(output_path).parent
    output_parent.mkdir(parents=True, exist_ok=True)

    offset = len(stub_data)
    length = len(container_data)

    with open(output_path, 'wb') as f:
        f.write(stub_data)
        f.write(container_data)
        f.write(struct.pack('<Q', offset))
        f.write(struct.pack('<I', length))
        f.write(b'CVMT')

    print(f"[OK] Packed {output_path}")
    print(f"      Stub size: {len(stub_data)} bytes")
    print(f"      Blob size: {length} bytes")
    print(f"      Total:    {len(stub_data) + length + 16} bytes")

def main():
    parser = argparse.ArgumentParser(description="Pack a .cvm container into a stub EXE")
    parser.add_argument("stub", help="Path to stub.exe (built from vm_c/)")
    parser.add_argument("container", help="Path to .cvm container file")
    parser.add_argument("output", help="Path for the final EXE")
    args = parser.parse_args()

    if not os.path.exists(args.stub):
        print(f"Error: stub not found: {args.stub}", file=sys.stderr)
        sys.exit(1)
    if not os.path.exists(args.container):
        print(f"Error: container not found: {args.container}", file=sys.stderr)
        sys.exit(1)

    pack(args.stub, args.container, args.output)

if __name__ == "__main__":
    main()
