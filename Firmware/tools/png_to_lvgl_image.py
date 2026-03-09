#!/usr/bin/env python3
import argparse
from pathlib import Path

from PIL import Image


def rgb565_bytes(img: Image.Image):
    data = []
    for r, g, b, _a in img.getdata():
        val = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
        data.append(val & 0xFF)        # little-endian low byte
        data.append((val >> 8) & 0xFF) # little-endian high byte
    return data


def emit_h(name: str) -> str:
    guard = f"{name.upper()}_H"
    return f'''#ifndef {guard}
#define {guard}

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {{
#endif

LV_IMAGE_DECLARE({name});

#ifdef __cplusplus
}}
#endif

#endif /* {guard} */
'''


def emit_c(name: str, w: int, h: int, data: list[int]) -> str:
    stride = w * 2
    rows = []
    for i in range(0, len(data), 16):
        chunk = ", ".join(f"0x{b:02X}" for b in data[i:i+16])
        rows.append(f"    {chunk}")
    data_blob = ",\n".join(rows)

    return f'''#include "{name}.h"

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_IMAGE_{name.upper()}
#define LV_ATTRIBUTE_IMAGE_{name.upper()}
#endif

const LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_IMAGE_{name.upper()} uint8_t {name}_map[] = {{
{data_blob}
}};

const lv_image_dsc_t {name} = {{
    .header.magic = LV_IMAGE_HEADER_MAGIC,
    .header.cf = LV_COLOR_FORMAT_RGB565,
    .header.flags = 0,
    .header.w = {w},
    .header.h = {h},
    .header.stride = {stride},
    .header.reserved_2 = 0,
    .data_size = sizeof({name}_map),
    .data = {name}_map,
    .reserved = NULL,
    .reserved_2 = NULL,
}};
'''


def main():
    p = argparse.ArgumentParser(description="Convert PNG to LVGL v9 RGB565 C image")
    p.add_argument("--input", required=True, help="Path to input PNG")
    p.add_argument("--name", default="time_logo", help="C symbol/base filename")
    p.add_argument("--max-width", type=int, default=420)
    p.add_argument("--max-height", type=int, default=120)
    args = p.parse_args()

    root = Path(__file__).resolve().parents[1]
    out_dir = root / "src" / "assets"
    out_dir.mkdir(parents=True, exist_ok=True)

    img = Image.open(args.input).convert("RGBA")
    img.thumbnail((args.max_width, args.max_height), Image.Resampling.LANCZOS)

    # Blend transparent pixels over white background (home screen is white)
    bg = Image.new("RGBA", img.size, (255, 255, 255, 255))
    img = Image.alpha_composite(bg, img)

    data = rgb565_bytes(img)

    h_text = emit_h(args.name)
    c_text = emit_c(args.name, img.width, img.height, data)

    (out_dir / f"{args.name}.h").write_text(h_text, encoding="utf-8")
    (out_dir / f"{args.name}.c").write_text(c_text, encoding="utf-8")

    print(f"Wrote: {(out_dir / f'{args.name}.h')}")
    print(f"Wrote: {(out_dir / f'{args.name}.c')}")
    print(f"Image size: {img.width}x{img.height}")


if __name__ == "__main__":
    main()
