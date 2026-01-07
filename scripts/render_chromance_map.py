#!/usr/bin/env python3
"""
Render a visual map of the Chromance layout from a WLED-style ledmap.json.

Outputs:
  - SVG with LED positions, segment IDs, and vertex IDs.
  - PNG converted from SVG via rsvg-convert (if available).

Vertex IDs are derived deterministically from the canonical topology list (SEGMENTS)
in scripts/generate_ledmap.py:
  - unique (vx, vy) endpoints
  - sorted lexicographically by (vx, vy)
  - vertex_id = index in that sorted list (0-based)
"""

from __future__ import annotations

import argparse
import ast
import json
import math
import shutil
import subprocess
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Tuple


LEDS_PER_SEGMENT = 14


def vertex_to_raster(vx: int, vy: int) -> Tuple[int, int]:
    return (28 * vx, 14 * vy)


def round_half_away_from_zero(v: float) -> int:
    return int(math.floor(v + 0.5)) if v >= 0 else int(math.ceil(v - 0.5))


def sample_segment_pixels(p0: Tuple[int, int], p1: Tuple[int, int]) -> List[Tuple[int, int]]:
    x0, y0 = p0
    x1, y1 = p1
    out: List[Tuple[int, int]] = []
    for k in range(LEDS_PER_SEGMENT):
        t = (k + 0.5) / LEDS_PER_SEGMENT
        x = round_half_away_from_zero(x0 + t * (x1 - x0))
        y = round_half_away_from_zero(y0 + t * (y1 - y0))
        out.append((x, y))
    return out


def load_segments_from_generator(generator_path: Path) -> List[Tuple[Tuple[int, int], Tuple[int, int]]]:
    tree = ast.parse(generator_path.read_text(encoding="utf-8"), filename=str(generator_path))
    for node in tree.body:
        if isinstance(node, ast.AnnAssign):
            target = node.target
            if isinstance(target, ast.Name) and target.id == "SEGMENTS":
                value = ast.literal_eval(node.value)
                return [((int(a[0]), int(a[1])), (int(b[0]), int(b[1]))) for (a, b) in value]
        if isinstance(node, ast.Assign):
            for target in node.targets:
                if isinstance(target, ast.Name) and target.id == "SEGMENTS":
                    value = ast.literal_eval(node.value)
                    # Expected shape: List[Tuple[Tuple[int,int],Tuple[int,int]]]
                    return [((int(a[0]), int(a[1])), (int(b[0]), int(b[1]))) for (a, b) in value]
    raise ValueError(f"Could not find SEGMENTS assignment in {generator_path}")


def compute_generator_shift(segments: Sequence[Tuple[Tuple[int, int], Tuple[int, int]]]) -> Tuple[int, int, int, int]:
    pts: List[Tuple[int, int]] = []
    for va, vb in segments:
        pts.extend(sample_segment_pixels(vertex_to_raster(*va), vertex_to_raster(*vb)))
    xs = [p[0] for p in pts]
    ys = [p[1] for p in pts]
    return min(xs), min(ys), max(xs), max(ys)


def load_ledmap(path: Path) -> Tuple[int, int, List[int]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    width = int(data["width"])
    height = int(data["height"])
    m = data["map"]
    if not isinstance(m, list):
        raise ValueError("ledmap.json: expected 'map' list")
    out: List[int] = [int(v) for v in m]
    if len(out) != width * height:
        raise ValueError(f"ledmap.json: map length {len(out)} != width*height {width*height}")
    return width, height, out


def build_index_positions(width: int, height: int, m: Sequence[int]) -> Dict[int, Tuple[int, int]]:
    pos: Dict[int, Tuple[int, int]] = {}
    for j, v in enumerate(m):
        if v < 0:
            continue
        x = j % width
        y = j // width
        pos[v] = (x, y)
    return pos


def svg_escape(s: str) -> str:
    return (
        s.replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
        .replace("'", "&apos;")
    )


def write_svg(
    *,
    out_path: Path,
    width_units: int,
    height_units: int,
    led_positions: Iterable[Tuple[int, int]],
    seg_lines: Sequence[Tuple[int, int, int, int]],
    seg_labels: Sequence[Tuple[float, float, str]],
    vertex_points: Sequence[Tuple[int, int, str]],
    scale: int,
    pad_units: int,
    flip_x: bool = False,
) -> None:
    # Compute draw bounds (allow negative vertex coords).
    xs: List[float] = []
    ys: List[float] = []
    for x, y in led_positions:
        xs.append(x)
        ys.append(y)
    for x0, y0, x1, y1 in seg_lines:
        xs.extend([x0, x1])
        ys.extend([y0, y1])
    for x, y, _t in vertex_points:
        xs.append(x)
        ys.append(y)

    min_x = min(xs) if xs else 0
    max_x = max(xs) if xs else (width_units - 1)
    min_y = min(ys) if ys else 0
    max_y = max(ys) if ys else (height_units - 1)

    min_x -= pad_units
    min_y -= pad_units
    max_x += pad_units
    max_y += pad_units

    view_w = (max_x - min_x + 1)
    view_h = (max_y - min_y + 1)

    svg_w = int(view_w * scale)
    svg_h = int(view_h * scale)

    def tx(x: float) -> float:
        return ((max_x - x) if flip_x else (x - min_x)) * scale

    def ty(y: float) -> float:
        # SVG y grows down; ledmap y grows down (row-major), so no inversion.
        return (y - min_y) * scale

    led_r = max(1.0, scale * 0.35)
    vtx_r = max(2.0, scale * 0.55)
    seg_font = max(14.0, scale * 2.6)
    vtx_font = max(14.0, scale * 2.4)
    text_halo = max(3.0, scale * 0.55)

    parts: List[str] = []
    parts.append('<?xml version="1.0" encoding="UTF-8"?>')
    parts.append(
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{svg_w}" height="{svg_h}" viewBox="0 0 {svg_w} {svg_h}">'
    )
    parts.append(f'<rect x="0" y="0" width="{svg_w}" height="{svg_h}" fill="#ffffff"/>')

    # Segments (lines)
    parts.append('<g id="segments" stroke="#9aa0a6" stroke-width="1.5" fill="none" opacity="0.9">')
    for x0, y0, x1, y1 in seg_lines:
        parts.append(
            f'<line x1="{tx(x0):.2f}" y1="{ty(y0):.2f}" x2="{tx(x1):.2f}" y2="{ty(y1):.2f}"/>'
        )
    parts.append("</g>")

    # LEDs (points)
    parts.append('<g id="leds" fill="#111111" opacity="0.65">')
    for x, y in led_positions:
        parts.append(f'<circle cx="{tx(x):.2f}" cy="{ty(y):.2f}" r="{led_r:.2f}"/>')
    parts.append("</g>")

    # Segment labels
    parts.append(
        f'<g id="segment_labels" font-family="ui-monospace, SFMono-Regular, Menlo, monospace" '
        f'font-size="{seg_font:.1f}" font-weight="700" fill="#1a73e8" stroke="#ffffff" '
        f'stroke-width="{text_halo:.1f}" paint-order="stroke" text-anchor="middle">'
    )
    for x, y, text in seg_labels:
        parts.append(f'<text x="{tx(x):.2f}" y="{ty(y):.2f}">{svg_escape(text)}</text>')
    parts.append("</g>")

    # Vertex points + labels
    parts.append('<g id="vertices" fill="#d93025" opacity="0.95">')
    for x, y, _text in vertex_points:
        parts.append(f'<circle cx="{tx(x):.2f}" cy="{ty(y):.2f}" r="{vtx_r:.2f}"/>')
    parts.append("</g>")
    parts.append(
        f'<g id="vertex_labels" font-family="ui-monospace, SFMono-Regular, Menlo, monospace" '
        f'font-size="{vtx_font:.1f}" font-weight="700" fill="#d93025" stroke="#ffffff" '
        f'stroke-width="{text_halo:.1f}" paint-order="stroke" text-anchor="start">'
    )
    for x, y, text in vertex_points:
        parts.append(f'<text x="{tx(x) + 4:.2f}" y="{ty(y) - 4:.2f}">{svg_escape(text)}</text>')
    parts.append("</g>")

    # Legend
    parts.append(
        '<g id="legend" font-family="ui-sans-serif, system-ui, -apple-system, Segoe UI, Roboto, sans-serif" font-size="12" fill="#202124">'
    )
    parts.append(f'<text x="{tx(min_x + 1):.2f}" y="{ty(min_y + 2):.2f}">Chromance map (from ledmap.json)</text>')
    parts.append(f'<text x="{tx(min_x + 1):.2f}" y="{ty(min_y + 5):.2f}">S# = segment id, V# = vertex id</text>')
    parts.append("</g>")

    parts.append("</svg>")
    out_path.write_text("\n".join(parts), encoding="utf-8")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--ledmap", type=Path, default=Path("mapping/ledmap.json"))
    ap.add_argument("--generator", type=Path, default=Path("scripts/generate_ledmap.py"))
    ap.add_argument("--out-svg", type=Path, default=Path("chromance_map.svg"))
    ap.add_argument("--out-png", type=Path, default=Path("chromance_map.png"))
    ap.add_argument("--out-svg-front", type=Path, default=Path("chromance_map_front.svg"))
    ap.add_argument("--out-png-front", type=Path, default=Path("chromance_map_front.png"))
    ap.add_argument("--out-svg-back", type=Path, default=Path("chromance_map_back.svg"))
    ap.add_argument("--out-png-back", type=Path, default=Path("chromance_map_back.png"))
    ap.add_argument("--scale", type=int, default=6)
    ap.add_argument("--pad", type=int, default=6, help="padding in ledmap units")
    args = ap.parse_args()

    width, height, m = load_ledmap(args.ledmap)
    idx_pos = build_index_positions(width, height, m)
    led_positions = list(idx_pos.values())

    segments = load_segments_from_generator(args.generator)
    min_x, min_y, max_x, max_y = compute_generator_shift(segments)
    expected_w = (max_x - min_x + 1)
    expected_h = (max_y - min_y + 1)
    if expected_w != width or expected_h != height:
        print(
            f"WARNING: ledmap dims ({width}x{height}) != generator bounds ({expected_w}x{expected_h}); proceeding anyway"
        )

    # Vertex IDs: unique endpoints, lexicographic by (vx, vy).
    verts = sorted({v for seg in segments for v in seg})
    vid_by_vert = {v: i for i, v in enumerate(verts)}

    # Segment geometry in ledmap coords (shift by min_x/min_y).
    seg_lines: List[Tuple[int, int, int, int]] = []
    seg_labels: List[Tuple[float, float, str]] = []
    for seg_id, (va, vb) in enumerate(segments, start=1):
        ax, ay = vertex_to_raster(*va)
        bx, by = vertex_to_raster(*vb)
        ax -= min_x
        ay -= min_y
        bx -= min_x
        by -= min_y
        seg_lines.append((ax, ay, bx, by))

        pts = sample_segment_pixels(vertex_to_raster(*va), vertex_to_raster(*vb))
        pts_shifted = [(x - min_x, y - min_y) for (x, y) in pts]
        cx = sum(x for x, _y in pts_shifted) / len(pts_shifted)
        cy = sum(y for _x, y in pts_shifted) / len(pts_shifted)
        seg_labels.append((cx, cy, f"S{seg_id}"))

    vertex_points: List[Tuple[int, int, str]] = []
    for (vx, vy), vid in vid_by_vert.items():
        x, y = vertex_to_raster(vx, vy)
        x -= min_x
        y -= min_y
        vertex_points.append((x, y, f"V{vid}"))

    write_svg(
        out_path=args.out_svg,
        width_units=width,
        height_units=height,
        led_positions=led_positions,
        seg_lines=seg_lines,
        seg_labels=seg_labels,
        vertex_points=vertex_points,
        scale=args.scale,
        pad_units=args.pad,
    )
    print(f"Wrote {args.out_svg}")

    write_svg(
        out_path=args.out_svg_back,
        width_units=width,
        height_units=height,
        led_positions=led_positions,
        seg_lines=seg_lines,
        seg_labels=seg_labels,
        vertex_points=vertex_points,
        scale=args.scale,
        pad_units=args.pad,
        flip_x=False,
    )
    print(f"Wrote {args.out_svg_back}")

    write_svg(
        out_path=args.out_svg_front,
        width_units=width,
        height_units=height,
        led_positions=led_positions,
        seg_lines=seg_lines,
        seg_labels=seg_labels,
        vertex_points=vertex_points,
        scale=args.scale,
        pad_units=args.pad,
        flip_x=True,
    )
    print(f"Wrote {args.out_svg_front}")

    rsvg = shutil.which("rsvg-convert")
    if rsvg is None:
        print("NOTE: rsvg-convert not found; skipping PNG generation.")
        return 0

    subprocess.run([rsvg, str(args.out_svg), "-o", str(args.out_png)], check=True)
    print(f"Wrote {args.out_png}")

    subprocess.run([rsvg, str(args.out_svg_back), "-o", str(args.out_png_back)], check=True)
    print(f"Wrote {args.out_png_back}")

    subprocess.run([rsvg, str(args.out_svg_front), "-o", str(args.out_png_front)], check=True)
    print(f"Wrote {args.out_png_front}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
