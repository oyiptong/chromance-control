import csv
from pathlib import Path

Import("env")


MIN_MARGIN_BYTES = 64 * 1024
MIN_MARGIN_FRACTION = 0.10


def _find_partition_csv() -> Path:
    name = env.GetProjectOption("board_build.partitions")
    packages = Path(env["PROJECT_PACKAGES_DIR"])
    candidates = [
        packages / "framework-arduinoespressif32" / "tools" / "partitions" / name,
        packages / "framework-arduinoespressif32" / "tools" / "partitions" / "default.csv",
    ]
    for p in candidates:
        if p.exists():
            return p
    raise RuntimeError(f"partition CSV not found for board_build.partitions={name}")


def _read_app_partition_size(csv_path: Path) -> int:
    app_sizes: list[int] = []
    with csv_path.open("r", encoding="utf-8") as f:
        reader = csv.reader(f)
        for row in reader:
            if not row or row[0].startswith("#"):
                continue
            if len(row) < 5:
                continue
            name = row[0].strip()
            ptype = row[1].strip()
            if ptype != "app":
                continue
            size_str = row[4].strip()
            size = int(size_str, 0)
            if name in ("app0", "app1", "ota_0", "ota_1"):
                app_sizes.append(size)
    if not app_sizes:
        raise RuntimeError(f"no OTA app partitions found in {csv_path}")
    return min(app_sizes)


def _check(target, source, env):
    firmware_bin = Path(str(target[0]))
    csv_path = _find_partition_csv()
    app_size = _read_app_partition_size(csv_path)

    fw_size = firmware_bin.stat().st_size
    margin = max(MIN_MARGIN_BYTES, int(app_size * MIN_MARGIN_FRACTION))
    limit = app_size - margin

    print(
        f"OTA partition check: csv={csv_path.name} app_size={app_size} fw_size={fw_size} margin={margin} limit={limit}"
    )
    if fw_size > limit:
        raise RuntimeError(
            f"firmware too large for OTA safety margin: fw_size={fw_size} > limit={limit} (app_size={app_size}, margin={margin})"
        )


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", _check)
