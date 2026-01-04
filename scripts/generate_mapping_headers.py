import subprocess
from pathlib import Path

Import("env")


def _newer_than(target: Path, sources: list[Path]) -> bool:
    if not target.exists():
        return False
    t_mtime = target.stat().st_mtime
    return all(s.exists() and s.stat().st_mtime <= t_mtime for s in sources)


project_dir = Path(env["PROJECT_DIR"])
python_exe = env.get("PYTHONEXE", "python3")
gen = project_dir / "scripts" / "generate_ledmap.py"

include_generated = project_dir / "include" / "generated"
tmp_dir = project_dir / ".pio" / "mapping_tmp"
tmp_dir.mkdir(parents=True, exist_ok=True)

wiring_full = project_dir / "mapping" / "wiring.json"
wiring_bench = project_dir / "mapping" / "wiring_bench.json"

out_full = include_generated / "chromance_mapping_full.h"
out_bench = include_generated / "chromance_mapping_bench.h"

sources_full = [gen, wiring_full]
sources_bench = [gen, wiring_bench]

needs_full = not _newer_than(out_full, sources_full)
needs_bench = not _newer_than(out_bench, sources_bench)

if needs_full or needs_bench:
    include_generated.mkdir(parents=True, exist_ok=True)

if needs_full:
    subprocess.check_call(
        [
            str(python_exe),
            str(gen),
            "--wiring",
            str(wiring_full),
            "--out-ledmap",
            str(tmp_dir / "ledmap_full.json"),
            "--out-pixels",
            str(tmp_dir / "pixels_full.json"),
            "--out-header",
            str(out_full),
        ]
    )

if needs_bench:
    subprocess.check_call(
        [
            str(python_exe),
            str(gen),
            "--wiring",
            str(wiring_bench),
            "--out-ledmap",
            str(tmp_dir / "ledmap_bench.json"),
            "--out-pixels",
            str(tmp_dir / "pixels_bench.json"),
            "--out-header",
            str(out_bench),
        ]
    )

