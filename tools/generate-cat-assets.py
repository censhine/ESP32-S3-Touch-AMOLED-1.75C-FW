#!/usr/bin/env python3
"""Rasterize the approved ginger kitten paths into flash-resident LVGL RGB565A8."""

import base64
from io import BytesIO
import json
from pathlib import Path
import subprocess

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "firmware/components/XiaozhiApp/assets"
INK = "#674537"


def svg(body, x, y, width, height):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
            f'viewBox="{x} {y} {width} {height}"><defs><radialGradient id="fur">'
            '<stop stop-color="#FFDA94"/><stop offset="1" stop-color="#F5B657"/>'
            '</radialGradient></defs>' + body + '</svg>')


def raster_all(assets):
    payload = [svg(body, *crop) for _, body, crop in assets]
    result = subprocess.run(
        ["node", str(ROOT / "tools/render-cat-svg.cjs")],
        input=json.dumps(payload).encode(), capture_output=True, check=True,
    )
    images = []
    for encoded, (_, _, crop) in zip(json.loads(result.stdout), assets):
        image = Image.open(BytesIO(base64.b64decode(encoded))).convert("RGBA")
        assert image.size == crop[2:], image.size
        images.append(image)
    return images


def face():
    # Geometry and palette follow the approved draw_design.py cat() artwork.
    return f'''<g stroke="{INK}" stroke-width="4" stroke-linecap="round" stroke-linejoin="round">
    <ellipse cx="150" cy="220" rx="92" ry="9" fill="#E6D9C7" opacity=".45" stroke="none"/>
    <path d="M62 100 C52 78 40 34 51 23 C59 15 94 45 112 63 C138 54 164 55 189 63 C211 42 244 16 252 26 C263 40 252 82 243 102 C268 123 272 150 254 172 C234 199 191 209 149 209 C106 209 64 198 45 172 C27 148 35 122 62 100Z" fill="url(#fur)"/>
    <path d="M59 39 Q58 65 72 86 L99 69Z" fill="#E99689" stroke="none"/>
    <path d="M242 41 L204 69 L233 86 Q244 61 242 41Z" fill="#E99689" stroke="none"/>
    <path d="M49 146 Q68 140 84 145 Q107 133 130 153 Q150 170 171 153 Q193 135 218 145 Q237 138 254 146 C265 176 211 207 150 207 C89 207 39 180 49 146Z" fill="#FFF6E4" stroke="none"/>
    <path d="M137 64 L143 81 M153 61 L154 79 M169 65 L165 82" fill="none" stroke="#CE8C42" stroke-width="6"/>
    <path d="M83 84 Q99 76 112 82 M181 82 Q198 72 212 81" fill="none"/>
    <path d="M142 158 Q150 153 158 158 Q157 162 150 166 Q143 162 142 158Z" fill="#B66E66" stroke="none"/>
    <path d="M47 148 L25 143 M46 160 L21 161 M252 148 L275 141 M254 159 L279 160" fill="none" stroke-width="3"/>
    </g>'''


def eye(gaze=(0, 0), mode="open"):
    if mode == "happy":
        detail = '<path d="M80 130 Q104 103 128 130" fill="none" stroke-width="6"/>'
    elif mode == "closed":
        detail = '<path d="M80 130 Q104 149 128 130" fill="none" stroke-width="5"/>'
    else:
        gx, gy = gaze
        ix, iy = 104 + gx, 129 + gy
        detail = (f'<ellipse cx="104" cy="126" rx="35" ry="36" fill="#FFFEF5"/>'
                  f'<ellipse cx="{ix}" cy="{iy}" rx="23" ry="29" fill="#539C8F" stroke="none"/>'
                  f'<ellipse cx="{ix + 2}" cy="{iy + 1}" rx="14" ry="24" fill="#263F39" stroke="none"/>'
                  f'<ellipse cx="{ix - 7}" cy="{iy - 12}" rx="7.8" ry="9.5" fill="white" stroke="none"/>'
                  f'<circle cx="{ix + 9}" cy="{iy + 12}" r="3.6" fill="#EFF7D9" stroke="none"/>')
    return f'<g stroke="{INK}" stroke-width="4" stroke-linecap="round">{detail}</g>'


def mouth(opened):
    if opened:
        return (f'<g stroke="{INK}" stroke-linecap="round" stroke-linejoin="round">'
                '<path d="M135 171 Q150 181 166 171 C165 198 139 198 135 171Z" fill="#713F39" stroke-width="3"/>'
                '<path d="M142 187 Q150 179 160 187 Q152 197 142 187" fill="#EF9B97" stroke="none"/></g>')
    return (f'<path d="M150 166 L150 172 Q141 182 132 173 M150 172 Q159 182 168 172" '
            f'fill="none" stroke="{INK}" stroke-width="3.5" stroke-linecap="round" stroke-linejoin="round"/>')


def rgb565a8(image):
    color, alpha = bytearray(), bytearray()
    raw = image.tobytes()
    for offset in range(0, len(raw), 4):
        r, g, b, a = raw[offset:offset + 4]
        pixel = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        color.extend((pixel & 255, pixel >> 8))
        alpha.append(a)
    return color + alpha


def main():
    # Crops retain the whiskers and floor shadow. The root places this at (10, 17).
    assets = [("face", face(), (20, 22, 260, 207))]
    for name, gaze, mode in (
        ("eye_center", (2, 0), "open"),
        ("eye_left", (-6, -1), "open"),
        ("eye_right", (7, -2), "open"),
        ("eye_up", (7, -7), "open"),
        ("eye_closed", (0, 0), "closed"),
        ("eye_happy", (0, 0), "happy"),
    ):
        assets.append((name, eye(gaze, mode), (68, 87, 72, 80)))
    assets.append(("blush", '<ellipse cx="66" cy="158" rx="17" ry="8" fill="#F5A598" opacity=".65"/>',
                   (49, 150, 34, 16)))
    assets.extend((
        ("mouth_closed", mouth(False), (128, 164, 44, 32)),
        ("mouth_open", mouth(True), (128, 164, 44, 36)),
    ))
    header = ['#pragma once', '#include "lvgl.h"', '', '#ifdef __cplusplus', 'extern "C" {', '#endif', '']
    source = ['// Generated by tools/generate-cat-assets.py from the approved ginger kitten drawing.',
              '#include "cat_avatar_assets.h"', '']
    total = 0
    for (name, _, crop), image in zip(assets, raster_all(assets)):
        data = rgb565a8(image)
        total += len(data)
        ident = 'xiaozhi_cat_' + name
        header.append(f'extern const lv_image_dsc_t {ident};')
        source.append(f'static const uint8_t {ident}_pixels[] = {{')
        for offset in range(0, len(data), 24):
            source.append('    ' + ', '.join(f'0x{value:02x}' for value in data[offset:offset + 24]) + ',')
        source.extend(('};', f'const lv_image_dsc_t {ident} = {{',
                       f'    {{LV_IMAGE_HEADER_MAGIC, LV_COLOR_FORMAT_RGB565A8, 0, {crop[2]}, {crop[3]}, {crop[2] * 2}, 0}},',
                       f'    sizeof({ident}_pixels), {ident}_pixels, NULL, NULL,', '};', ''))
    header.extend(('', '#ifdef __cplusplus', '}', '#endif', ''))
    assert total < 350 * 1024, total
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'cat_avatar_assets.h').write_text('\n'.join(header))
    (OUT / 'cat_avatar_assets.c').write_text('\n'.join(source))
    print(f'Generated {len(assets)} RGB565A8 layers, {total:,} bytes total')


if __name__ == '__main__':
    main()
