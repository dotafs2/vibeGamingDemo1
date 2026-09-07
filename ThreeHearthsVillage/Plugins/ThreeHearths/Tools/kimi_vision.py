"""Bounded image transport, independent of the immutable financial policy.

The ledger still reserves the full model context. Raising the HTTP envelope
does not raise the text/context/output/concurrency or money allowance.
"""
import base64
import binascii
import struct
import zlib

MAX_IMAGE_BYTES = 512 * 1024
MAX_REQUEST_BYTES = 768 * 1024
MAX_DIMENSION = 1024


def validate_png_url(url):
    prefix = 'data:image/png;base64,'
    if not isinstance(url, str) or not url.startswith(prefix):
        raise ValueError('Only inline PNG observations are supported')
    if len(url) > len(prefix) + 4 * ((MAX_IMAGE_BYTES + 2) // 3):
        raise ValueError('Image is too large')
    try:
        data = base64.b64decode(url[len(prefix):], validate=True)
    except (ValueError, binascii.Error):
        raise ValueError('Invalid image encoding') from None
    if len(data) > MAX_IMAGE_BYTES or data[:8] != b'\x89PNG\r\n\x1a\n':
        raise ValueError('Invalid PNG')
    # Inspect every chunk, cap decompression, and accept only the non-animated
    # 8-bit RGB/RGBA screenshots emitted by UE. No remote fetch or image library.
    pos, compressed, width, height, channels, ended = 8, bytearray(), 0, 0, 0, False
    while pos + 12 <= len(data):
        size = struct.unpack('>I', data[pos:pos+4])[0]
        kind = data[pos+4:pos+8]
        end = pos + 12 + size
        if end > len(data):
            raise ValueError('Truncated PNG')
        payload = data[pos+8:pos+8+size]
        if zlib.crc32(kind + payload) & 0xffffffff != struct.unpack('>I', data[end-4:end])[0]:
            raise ValueError('PNG checksum mismatch')
        if pos == 8:
            if kind != b'IHDR' or size != 13:
                raise ValueError('Missing PNG header')
            width, height, depth, color, compression, filtering, interlace = struct.unpack('>IIBBBBB', payload)
            if not (1 <= width <= MAX_DIMENSION and 1 <= height <= MAX_DIMENSION and depth == 8
                    and color in (2, 6) and compression == filtering == interlace == 0):
                raise ValueError('Unsupported screenshot dimensions or format')
            channels = 3 if color == 2 else 4
        elif kind == b'IHDR' or kind in (b'acTL', b'fcTL', b'fdAT'):
            raise ValueError('Duplicate header or animation')
        if kind == b'IDAT':
            compressed.extend(payload)
        if kind == b'IEND':
            if size != 0 or end != len(data):
                raise ValueError('Invalid PNG end')
            ended = True
            break
        pos = end
    if not ended or not compressed:
        raise ValueError('Incomplete PNG')
    limit = height * (width * channels + 1)
    decoder = zlib.decompressobj()
    raw = decoder.decompress(bytes(compressed), limit + 1)
    if len(raw) != limit or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
        raise ValueError('Invalid PNG pixel stream')
    if any(raw[row * (width * channels + 1)] > 4 for row in range(height)):
        raise ValueError('Invalid PNG filter')
    return width, height
