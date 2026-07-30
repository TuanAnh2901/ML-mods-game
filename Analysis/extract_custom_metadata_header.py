"""Extract the custom decrypted header used by this game's metadata loader."""

import argparse
import pathlib

from metadata_header_xxtea import decrypt_header


HEADER_SIZE = 676
HEADER_OFFSET = 4
HEADER_KEY = b"fc48e86b730833ef"


def extract_header(metadata_path: pathlib.Path) -> bytes:
    raw = metadata_path.read_bytes()
    encrypted_start = HEADER_OFFSET
    encrypted_end = encrypted_start + HEADER_SIZE
    if len(raw) < encrypted_end:
        raise ValueError(f"Metadata file is shorter than the encrypted header: {metadata_path}")

    return decrypt_header(raw[encrypted_start:encrypted_end], HEADER_KEY)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("metadata", type=pathlib.Path)
    parser.add_argument("output", type=pathlib.Path)
    args = parser.parse_args()

    args.output.write_bytes(extract_header(args.metadata))


if __name__ == "__main__":
    main()
