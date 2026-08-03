from pathlib import Path
import struct
import zlib
from SCons.Script import Import

Import("env")

PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))
OUTPUT_CPP = PROJECT_DIR / "src/generated_wallpaper.cpp"
OUTPUT_H = PROJECT_DIR / "src/generated_wallpaper.h"
SCREEN_W = 320
SCREEN_H = 170
PNG_SIG = b"\x89PNG\r\n\x1a\n"

def _truthy(value):
    return str(value).strip().lower() in {"1", "true", "yes", "on"}

def _wallpaper_color_order():
    order = env.GetProjectOption("custom_wallpaper_color_order", "rgb").strip().lower()
    if order not in {"rgb", "bgr"}:
        raise ValueError("custom_wallpaper_color_order must be rgb or bgr")
    return order


def _low_data_mode():
    custom_value = env.GetProjectOption("custom_low_data_mode", "no")
    build_flags = env.GetProjectOption("build_flags", [])
    if isinstance(build_flags, str):
        build_flags = build_flags.splitlines()
    return _truthy(custom_value) or any("LOW_DATA_MODE=1" in str(flag) for flag in build_flags)

def _write_disabled():
    OUTPUT_H.write_text("""#ifndef GENERATED_WALLPAPER_H\n#define GENERATED_WALLPAPER_H\n\n#include <lvgl.h>\n\n#define APP_WALLPAPER_ENABLED 0\n\n#endif\n""")
    OUTPUT_CPP.write_text("""#include <generated_wallpaper.h>\n""")

def _paeth(a, b, c):
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c

def _read_png_rgba(path):
    raw = path.read_bytes()
    if not raw.startswith(PNG_SIG):
        raise ValueError(f"Wallpaper must be an 8-bit PNG: {path}")

    pos = len(PNG_SIG)
    width = height = color_type = bit_depth = None
    idat = bytearray()
    palette = []

    while pos < len(raw):
        length = struct.unpack(">I", raw[pos:pos + 4])[0]
        chunk_type = raw[pos + 4:pos + 8]
        data = raw[pos + 8:pos + 8 + length]
        pos += 12 + length

        if chunk_type == b"IHDR":
            width, height, bit_depth, color_type, compression, filter_method, interlace = struct.unpack(">IIBBBBB", data)
            if bit_depth != 8 or color_type not in (2, 3, 6) or interlace != 0:
                raise ValueError("Wallpaper PNG must be non-interlaced 8-bit RGB, RGBA, or indexed color")
            if compression != 0 or filter_method != 0:
                raise ValueError("Unsupported PNG compression/filter method")
        elif chunk_type == b"PLTE":
            palette = [tuple(data[i:i + 3]) for i in range(0, len(data), 3)]
        elif chunk_type == b"IDAT":
            idat.extend(data)
        elif chunk_type == b"IEND":
            break

    if width is None or height is None:
        raise ValueError("Invalid PNG: missing IHDR")

    if color_type == 3 and not palette:
        raise ValueError("Indexed PNG is missing a palette")

    channels = 4 if color_type == 6 else 1 if color_type == 3 else 3
    bpp = channels
    stride = width * channels
    decompressed = zlib.decompress(bytes(idat))
    rows = []
    offset = 0
    prev = bytearray(stride)

    for _ in range(height):
        filter_type = decompressed[offset]
        offset += 1
        scanline = bytearray(decompressed[offset:offset + stride])
        offset += stride

        for i in range(stride):
            left = scanline[i - bpp] if i >= bpp else 0
            up = prev[i]
            upper_left = prev[i - bpp] if i >= bpp else 0
            if filter_type == 1:
                scanline[i] = (scanline[i] + left) & 0xFF
            elif filter_type == 2:
                scanline[i] = (scanline[i] + up) & 0xFF
            elif filter_type == 3:
                scanline[i] = (scanline[i] + ((left + up) >> 1)) & 0xFF
            elif filter_type == 4:
                scanline[i] = (scanline[i] + _paeth(left, up, upper_left)) & 0xFF
            elif filter_type != 0:
                raise ValueError(f"Unsupported PNG filter: {filter_type}")

        rows.append(scanline)
        prev = scanline

    pixels = []
    for row in rows:
        for x in range(width):
            i = x * channels
            if color_type == 3:
                idx = row[i]
                if idx >= len(palette):
                    raise ValueError("Indexed PNG references a palette entry that does not exist")
                pixels.append(palette[idx])
            else:
                r = row[i]
                g = row[i + 1]
                b = row[i + 2]
                pixels.append((r, g, b))
    return width, height, pixels

def _resize_nearest(width, height, pixels):
    if (width, height) == (SCREEN_W, SCREEN_H):
        return pixels
    resized = []
    for y in range(SCREEN_H):
        src_y = min(height - 1, (y * height) // SCREEN_H)
        row_base = src_y * width
        for x in range(SCREEN_W):
            src_x = min(width - 1, (x * width) // SCREEN_W)
            resized.append(pixels[row_base + src_x])
    return resized

def _rgb565_values(pixels):
    color_order = _wallpaper_color_order()
    values = []
    for r, g, b in pixels:
        if color_order == "bgr":
            r, b = b, r
        values.append(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3))
    return values

def _write_enabled(image_path):
    width, height, pixels = _read_png_rgba(image_path)
    pixels = _resize_nearest(width, height, pixels)
    values = _rgb565_values(pixels)

    lines = []
    for i in range(0, len(values), 12):
        chunk = values[i:i + 12]
        lines.append("    " + ", ".join(f"{{ .full = 0x{value:04X} }}" for value in chunk) + ",")

    OUTPUT_H.write_text("""#ifndef GENERATED_WALLPAPER_H\n#define GENERATED_WALLPAPER_H\n\n#include <lvgl.h>\n\n#define APP_WALLPAPER_ENABLED 1\nLV_IMG_DECLARE(generated_wallpaper);\n\n#endif\n""")

    OUTPUT_CPP.write_text("""#include <generated_wallpaper.h>\n\nconst LV_ATTRIBUTE_MEM_ALIGN lv_color_t generated_wallpaper_map[] = {\n%s\n};\n\nconst lv_img_dsc_t generated_wallpaper = {\n    .header = {\n        .cf = LV_IMG_CF_TRUE_COLOR,\n        .always_zero = 0,\n        .reserved = 0,\n        .w = 320,\n        .h = 170,\n    },\n    .data_size = sizeof(generated_wallpaper_map),\n    .data = (const uint8_t *)generated_wallpaper_map,\n};\n""" % "\n".join(lines))

if _low_data_mode():
    _write_disabled()
else:
    configured_path = env.GetProjectOption("custom_wallpaper_path", "asset/galaxy.png").strip()
    image_path = Path(configured_path)
    if not image_path.is_absolute():
        image_path = PROJECT_DIR / image_path
    if not image_path.exists():
        raise FileNotFoundError(f"Wallpaper image not found: {image_path}")
    _write_enabled(image_path)
