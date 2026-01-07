#!/usr/bin/env python3
"""
Generate a WLED-style ledmap.json for the Chromance layout.

Inputs:
  - A wiring JSON describing strip order and per-segment direction.
Output:
  - ledmap.json: { "width": int, "height": int, "map": [int|-1, ...] } row-major
  - pixels.json (optional): { "pixels": [{"i":0,"x":..,"y":..}, ...], "width":.., "height":.. }

Wiring JSON format (minimal):
{
  "strips": [
    { "name": "strip1", "segments": [ {"seg": 1, "dir": "a_to_b"}, {"seg": 2, "dir": "b_to_a"} ] },
    { "name": "strip2", "segments": [ ... ] }
  ]
}

Where seg is 1..40 referencing SEGMENTS below, and dir selects which endpoint is LED0->LED13.
"""

from __future__ import annotations

import argparse
import json
import math
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Tuple


LEDS_PER_SEGMENT = 14

# Segment topology list (40 undirected edges) in vertex coordinates (vx, vy).
# Endpoints are labeled (a, b); wiring chooses direction a_to_b vs b_to_a.
SEGMENTS: List[Tuple[Tuple[int, int], Tuple[int, int]]] = [
    ((0, 1), (0, 3)),
    ((0, 1), (1, 0)),
    ((0, 1), (1, 2)),
    ((0, 3), (1, 4)),
    ((1, 0), (2, 1)),
    ((1, 2), (1, 4)),
    ((1, 2), (2, 1)),
    ((1, 4), (1, 6)),
    ((1, 4), (2, 3)),
    ((1, 4), (2, 5)),
    ((1, 6), (2, 7)),
    ((2, 1), (2, 3)),
    ((2, 1), (3, 0)),
    ((2, 1), (3, 2)),
    ((2, 3), (3, 4)),
    ((2, 5), (2, 7)),
    ((2, 5), (3, 4)),
    ((2, 7), (3, 6)),
    ((2, 7), (3, 8)),
    ((3, 6), (3, 4)),
    ((3, 6), (4, 7)),
    ((3, 8), (4, 7)),
    ((3, 0), (4, 1)),
    ((3, 2), (3, 4)),
    ((3, 2), (4, 1)),
    ((3, 4), (4, 3)),
    ((3, 4), (4, 5)),
    ((4, 1), (4, 3)),
    ((4, 1), (5, 0)),
    ((4, 1), (5, 2)),
    ((4, 3), (5, 4)),
    ((4, 5), (4, 7)),
    ((4, 5), (5, 4)),
    ((4, 7), (5, 6)),
    ((5, 6), (5, 4)),
    ((5, 0), (6, 1)),
    ((5, 2), (5, 4)),
    ((5, 2), (6, 1)),
    ((5, 4), (6, 3)),
    ((6, 1), (6, 3)),
]


@dataclass(frozen=True)
class SegmentRef:
    seg: int  # 1..40
    direction: str  # "a_to_b" or "b_to_a"

@dataclass(frozen=True)
class OrderedSegment:
    seg: int  # 1..40
    direction: str  # "a_to_b" or "b_to_a"
    strip_index: int  # 0-based, per wiring file order
    segment_index_in_strip: int  # 0-based, per wiring file order


def vertex_to_raster(vx: int, vy: int) -> Tuple[int, int]:
    # Default deterministic projection (see docs/architecture/wled_integration_preplan.md).
    return (28 * vx, 14 * vy)

def round_half_away_from_zero(v: float) -> int:
    # Python's built-in round() uses bankers rounding (ties-to-even), which can
    # produce collisions for symmetric segments near shared vertices (e.g. 13.5
    # and 14.5 both round to 14). We want deterministic "mathy" rounding.
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


def parse_wiring(path: Path) -> Tuple[str, bool, List[OrderedSegment]]:
    data = json.loads(path.read_text())
    mapping_version = str(data.get("mappingVersion", ""))
    is_bench_subset = bool(data.get("isBenchSubset", False))
    strips = data.get("strips")
    if not isinstance(strips, list) or not strips:
        raise ValueError("wiring JSON must contain non-empty 'strips' list")

    ordered: List[OrderedSegment] = []
    for strip_index, strip in enumerate(strips):
        segs = strip.get("segments")
        if not isinstance(segs, list) or not segs:
            raise ValueError("each strip must contain non-empty 'segments' list")
        for segment_index_in_strip, s in enumerate(segs):
            seg = int(s["seg"])
            direction = str(s["dir"])
            if seg < 1 or seg > len(SEGMENTS):
                raise ValueError(f"segment id out of range: {seg}")
            if direction not in ("a_to_b", "b_to_a"):
                raise ValueError(f"invalid dir for seg {seg}: {direction}")
            ordered.append(
                OrderedSegment(
                    seg=seg,
                    direction=direction,
                    strip_index=strip_index,
                    segment_index_in_strip=segment_index_in_strip,
                )
            )

    used = [os.seg for os in ordered]
    if len(set(used)) != len(used):
        dupes = sorted({s for s in used if used.count(s) > 1})
        raise ValueError(f"wiring contains duplicate segment ids: dupes={dupes}")
    if not is_bench_subset:
        if len(set(used)) != len(SEGMENTS):
            missing = sorted(set(range(1, len(SEGMENTS) + 1)) - set(used))
            raise ValueError(f"wiring must reference each segment exactly once; missing={missing}")
    else:
        if not used:
            raise ValueError("bench wiring must reference at least one segment")

    return mapping_version, is_bench_subset, ordered


def build_pixels(ordered: Sequence[OrderedSegment]) -> List[Tuple[int, int]]:
    pixels: List[Tuple[int, int]] = []
    for os in ordered:
        (va, vb) = SEGMENTS[os.seg - 1]
        a = vertex_to_raster(*va)
        b = vertex_to_raster(*vb)
        pts = sample_segment_pixels(a, b)
        if os.direction == "b_to_a":
            pts.reverse()
        pixels.extend(pts)
    return pixels


def compute_bounds(pixels: Sequence[Tuple[int, int]]) -> Tuple[int, int, int, int]:
    xs = [p[0] for p in pixels]
    ys = [p[1] for p in pixels]
    return min(xs), min(ys), max(xs), max(ys)

def build_global_to_strip_tables(ordered: Sequence[OrderedSegment]) -> Tuple[List[int], List[int]]:
    global_to_strip: List[int] = []
    global_to_local: List[int] = []
    for os in ordered:
        base = os.segment_index_in_strip * LEDS_PER_SEGMENT
        for k in range(LEDS_PER_SEGMENT):
            global_to_strip.append(os.strip_index)
            if os.direction == "a_to_b":
                global_to_local.append(base + k)
            else:
                global_to_local.append(base + (LEDS_PER_SEGMENT - 1 - k))
    return global_to_strip, global_to_local

def build_global_to_segment_tables(
    ordered: Sequence[OrderedSegment],
) -> Tuple[List[int], List[int], List[int]]:
    global_to_seg: List[int] = []
    global_to_seg_k: List[int] = []
    global_to_dir: List[int] = []
    for os in ordered:
        for k in range(LEDS_PER_SEGMENT):
            global_to_seg.append(os.seg)
            global_to_seg_k.append(k)
            global_to_dir.append(0 if os.direction == "a_to_b" else 1)
    return global_to_seg, global_to_seg_k, global_to_dir

def write_mapping_header(
    *,
    out_path: Path,
    mapping_version: str,
    is_bench_subset: bool,
    width: int,
    height: int,
    pixel_x: Sequence[int],
    pixel_y: Sequence[int],
    global_to_strip: Sequence[int],
    global_to_local: Sequence[int],
    global_to_seg: Sequence[int],
    global_to_seg_k: Sequence[int],
    global_to_dir: Sequence[int],
) -> None:
    if len(pixel_x) != len(pixel_y):
        raise ValueError("pixel_x/pixel_y length mismatch")
    if len(global_to_strip) != len(pixel_x) or len(global_to_local) != len(pixel_x):
        raise ValueError("global_to_* length mismatch")
    if (
        len(global_to_seg) != len(pixel_x)
        or len(global_to_seg_k) != len(pixel_x)
        or len(global_to_dir) != len(pixel_x)
    ):
        raise ValueError("global_to_seg/global_to_seg_k/global_to_dir length mismatch")

    led_count = len(pixel_x)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    def format_values(values: Sequence[int], *, per_line: int) -> str:
        lines: List[str] = []
        for i in range(0, len(values), per_line):
            chunk = ", ".join(str(int(v)) for v in values[i : i + per_line])
            suffix = "," if i + per_line < len(values) else ""
            lines.append(f"  {chunk}{suffix}")
        return "\n".join(lines)

    def arr_int16(name: str, values: Sequence[int]) -> str:
        body = format_values(values, per_line=16)
        return f"constexpr int16_t {name}[LED_COUNT] = {{\n{body}\n}};"

    def arr_u8(name: str, values: Sequence[int]) -> str:
        body = format_values(values, per_line=24)
        return f"constexpr uint8_t {name}[LED_COUNT] = {{\n{body}\n}};"

    def arr_u16(name: str, values: Sequence[int]) -> str:
        body = format_values(values, per_line=16)
        return f"constexpr uint16_t {name}[LED_COUNT] = {{\n{body}\n}};"

    def arr_i8(name: str, values: Sequence[int], *, count_name: str) -> str:
        body = format_values(values, per_line=24)
        return f"constexpr int8_t {name}[{count_name}] = {{\n{body}\n}};"

    def arr_u8_counted(name: str, values: Sequence[int], *, count_name: str) -> str:
        body = format_values(values, per_line=24)
        return f"constexpr uint8_t {name}[{count_name}] = {{\n{body}\n}};"

    # Topology tables (canonical, shared across full/bench; filtering is done by segment presence).
    # Vertex IDs are stable: unique (vx,vy) endpoints sorted lexicographically.
    vertices = sorted({v for seg in SEGMENTS for v in seg})
    vertex_id_by_coord: Dict[Tuple[int, int], int] = {v: i for i, v in enumerate(vertices)}
    vertex_vx = [vx for (vx, _vy) in vertices]
    vertex_vy = [vy for (_vx, vy) in vertices]

    seg_vertex_a = [0]
    seg_vertex_b = [0]
    for (va, vb) in SEGMENTS:
        seg_vertex_a.append(vertex_id_by_coord[va])
        seg_vertex_b.append(vertex_id_by_coord[vb])

    header = "\n".join(
        [
            "#pragma once",
            "",
            "#include <stdint.h>",
            "",
            "// AUTOGENERATED FILE — do not hand-edit.",
            "// Generated by: scripts/generate_ledmap.py",
            "",
            "namespace chromance {",
            "namespace mapping {",
            "",
            f'constexpr const char* MAPPING_VERSION = "{mapping_version}";',
            f"constexpr bool IS_BENCH_SUBSET = {'true' if is_bench_subset else 'false'};",
            f"constexpr uint16_t LED_COUNT = {led_count};",
            f"constexpr uint16_t WIDTH = {int(width)};",
            f"constexpr uint16_t HEIGHT = {int(height)};",
            "",
            f"constexpr uint8_t SEGMENT_COUNT = {len(SEGMENTS)};",
            f"constexpr uint8_t VERTEX_COUNT = {len(vertices)};",
            "",
            arr_int16("pixel_x", pixel_x),
            arr_int16("pixel_y", pixel_y),
            arr_u8("global_to_strip", global_to_strip),
            arr_u16("global_to_local", global_to_local),
            arr_u8("global_to_seg", global_to_seg),
            arr_u8("global_to_seg_k", global_to_seg_k),
            arr_u8("global_to_dir", global_to_dir),
            "",
            arr_i8("vertex_vx", vertex_vx, count_name="VERTEX_COUNT"),
            arr_i8("vertex_vy", vertex_vy, count_name="VERTEX_COUNT"),
            arr_u8_counted("seg_vertex_a", seg_vertex_a, count_name="SEGMENT_COUNT + 1"),
            arr_u8_counted("seg_vertex_b", seg_vertex_b, count_name="SEGMENT_COUNT + 1"),
            "",
            "}  // namespace mapping",
            "}  // namespace chromance",
            "",
        ]
    )
    out_path.write_text(header)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--wiring", required=True, type=Path, help="Path to wiring.json (strip order + per-seg direction)")
    ap.add_argument("--out-ledmap", required=True, type=Path, help="Output ledmap.json path")
    ap.add_argument("--out-pixels", type=Path, help="Optional output pixels.json path")
    ap.add_argument("--out-header", type=Path, help="Optional output C++ header (include/generated/*.h)")
    args = ap.parse_args()

    mapping_version, is_bench_subset, ordered = parse_wiring(args.wiring)
    pixels = build_pixels(ordered)
    global_to_strip, global_to_local = build_global_to_strip_tables(ordered)
    global_to_seg, global_to_seg_k, global_to_dir = build_global_to_segment_tables(ordered)

    if len(pixels) != len(global_to_strip):
        raise AssertionError("pixel/global_to_* count mismatch")

    min_x, min_y, max_x, max_y = compute_bounds(pixels)
    width = max_x - min_x + 1
    height = max_y - min_y + 1

    m = [-1] * (width * height)
    seen: Dict[Tuple[int, int], int] = {}
    for i, (x, y) in enumerate(pixels):
        xn = x - min_x
        yn = y - min_y
        key = (xn, yn)
        if key in seen:
            raise ValueError(f"collision at {key}: led {seen[key]} and led {i}")
        seen[key] = i
        m[xn + yn * width] = i

    args.out_ledmap.parent.mkdir(parents=True, exist_ok=True)
    args.out_ledmap.write_text(json.dumps({"width": width, "height": height, "map": m}, indent=2))

    if args.out_pixels is not None:
        args.out_pixels.parent.mkdir(parents=True, exist_ok=True)
        payload = {
            "mappingVersion": mapping_version,
            "isBenchSubset": is_bench_subset,
            "width": width,
            "height": height,
            "pixels": [{"i": i, "x": x - min_x, "y": y - min_y} for i, (x, y) in enumerate(pixels)],
        }
        args.out_pixels.write_text(json.dumps(payload, indent=2))

    if args.out_header is not None:
        write_mapping_header(
            out_path=args.out_header,
            mapping_version=mapping_version,
            is_bench_subset=is_bench_subset,
            width=width,
            height=height,
            pixel_x=[x - min_x for (x, _y) in pixels],
            pixel_y=[y - min_y for (_x, y) in pixels],
            global_to_strip=global_to_strip,
            global_to_local=global_to_local,
            global_to_seg=global_to_seg,
            global_to_seg_k=global_to_seg_k,
            global_to_dir=global_to_dir,
        )

    print(f"leds={len(pixels)} width={width} height={height} holes={width*height-len(pixels)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
