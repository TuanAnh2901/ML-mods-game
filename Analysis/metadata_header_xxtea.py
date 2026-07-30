"""Offline decoder for the metadata header transform at GameAssembly + 0x43A040."""

import struct


MASK32 = 0xFFFFFFFF
DELTA = 0x9E3779B9


def _word(data: bytes, start: int) -> int:
    return struct.unpack_from("<I", data, start)[0]


def _derive_key_words(key: bytes) -> list[int]:
    if len(key) != 16:
        raise ValueError("The transform requires exactly 16 key bytes.")

    # The first word deliberately substitutes byte 6 for byte 3 in the native routine.
    return [
        key[0] | (key[1] << 8) | (key[2] << 16) | (key[6] << 24),
        _word(key, 4),
        _word(key, 8),
        _word(key, 12),
    ]


def _mix(sum_value: int, y: int, z: int, index: int, key: list[int]) -> int:
    key_index = (index & 3) ^ ((sum_value >> 2) & 3)
    left = ((z >> 5) ^ ((y << 2) & MASK32)) + ((y >> 3) ^ ((z << 4) & MASK32))
    right = (sum_value ^ y) + (key[key_index] ^ z)
    return (left ^ right) & MASK32


def decrypt_header(encrypted: bytes, key: bytes) -> bytes:
    if not encrypted:
        return b""

    word_count = (len(encrypted) + 3) // 4
    padded = encrypted.ljust(word_count * 4, b"\0")
    values = list(struct.unpack(f"<{word_count}I", padded))
    key_words = _derive_key_words(key)
    sum_value = ((6 + 52 // word_count) * DELTA) & MASK32

    while sum_value:
        y = values[0]
        for index in range(word_count - 1, 0, -1):
            z = values[index - 1]
            y = values[index] = (values[index] - _mix(sum_value, y, z, index, key_words)) & MASK32

        z = values[-1]
        values[0] = (values[0] - _mix(sum_value, y, z, 0, key_words)) & MASK32
        sum_value = (sum_value - DELTA) & MASK32

    decoded = bytearray(struct.pack(f"<{word_count}I", *values)[:len(encrypted)])
    if len(decoded) >= 4:
        # GameAssembly's word-to-byte converter clears the final trailer word before
        # returning the header buffer.
        decoded[-4:] = b"\0\0\0\0"

    return bytes(decoded)
