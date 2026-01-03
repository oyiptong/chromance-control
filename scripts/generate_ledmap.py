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


def vertex_to_raster(vx: int, vy: int) -> Tuple[int, int]:
    # Default deterministic projection (see docs/architecture/wled_integration_preplan.md).
    return (28 * vx, 14 * vy)


def sample_segment_pixels(p0: Tuple[int, int], p1: Tuple[int, int]) -> List[Tuple[int, int]]:
    x0, y0 = p0
    x1, y1 = p1
    out: List[Tuple[int, int]] = []
    for k in range(LEDS_PER_SEGMENT):
        t = (k + 0.5) / LEDS_PER_SEGMENT
        x = int(round(x0 + t * (x1 - x0)))
        y = int(round(y0 + t * (y1 - y0)))
        out.append((x, y))
    return out


def parse_wiring(path: Path) -> List[SegmentRef]:
    data = json.loads(path.read_text())
    strips = data.get("strips")
    if not isinstance(strips, list) or not strips:
        raise ValueError("wiring JSON must contain non-empty 'strips' list")

    ordered: List[SegmentRef] = []
    for strip in strips:
        segs = strip.get("segments")
        if not isinstance(segs, list) or not segs:
            raise ValueError("each strip must contain non-empty 'segments' list")
        for s in segs:
            seg = int(s["seg"])
            direction = str(s["dir"])
            if seg < 1 or seg > len(SEGMENTS):
                raise ValueError(f"segment id out of range: {seg}")
            if direction not in ("a_to_b", "b_to_a"):
                raise ValueError(f"invalid dir for seg {seg}: {direction}")
            ordered.append(SegmentRef(seg=seg, direction=direction))

    used = [sr.seg for sr in ordered]
    if len(set(used)) != len(SEGMENTS):
        missing = sorted(set(range(1, len(SEGMENTS) + 1)) - set(used))
        dupes = sorted({s for s in used if used.count(s) > 1})
        raise ValueError(f"wiring must reference each segment exactly once; missing={missing} dupes={dupes}")

    return ordered


def build_pixels(ordered: Sequence[SegmentRef]) -> List[Tuple[int, int]]:
    pixels: List[Tuple[int, int]] = []
    for sr in ordered:
        (va, vb) = SEGMENTS[sr.seg - 1]
        a = vertex_to_raster(*va)
        b = vertex_to_raster(*vb)
        pts = sample_segment_pixels(a, b)
        if sr.direction == "b_to_a":
            pts.reverse()
        pixels.extend(pts)
    if len(pixels) != len(SEGMENTS) * LEDS_PER_SEGMENT:
        raise AssertionError("pixel count mismatch")
    return pixels


def compute_bounds(pixels: Sequence[Tuple[int, int]]) -> Tuple[int, int, int, int]:
    xs = [p[0] for p in pixels]
    ys = [p[1] for p in pixels]
    return min(xs), min(ys), max(xs), max(ys)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--wiring", required=True, type=Path, help="Path to wiring.json (strip order + per-seg direction)")
    ap.add_argument("--out-ledmap", required=True, type=Path, help="Output ledmap.json path")
    ap.add_argument("--out-pixels", type=Path, help="Optional output pixels.json path")
    args = ap.parse_args()

    ordered = parse_wiring(args.wiring)
    pixels = build_pixels(ordered)

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
            "width": width,
            "height": height,
            "pixels": [{"i": i, "x": x - min_x, "y": y - min_y} for i, (x, y) in enumerate(pixels)],
        }
        args.out_pixels.write_text(json.dumps(payload, indent=2))

    print(f"leds={len(pixels)} width={width} height={height} holes={width*height-len(pixels)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

