"""Summarize on-device world-rendering telemetry, excluding menus and warm-up.

Input: adb logcat -d -s VrApi:I SDL/APP:I '*:S' > quest.log
FPS statistics describe three-second reporting windows, not frame-time percentiles.
"""
import argparse
import json
import re
import statistics
from pathlib import Path

PATTERN = re.compile(
    r"^\S+ (?P<time>\d\d:\d\d:\d\d\.\d+)\s+(?P<pid>\d+)\s+\d+\s+I SDL/APP\s*: "
    r"vr perf: (?P<scene>world|panel) (?P<fps>[\d.]+) fps; CPU "
    r"game=(?P<game>[\d.]+) wait=(?P<wait>[\d.]+) upload=(?P<upload>[\d.]+) "
    r"draw=(?P<draw>[\d.]+) submit=(?P<submit>[\d.]+) ms; "
    r"draws=(?P<draws>[\d.]+) passes=(?P<passes>[\d.]+) flushes=(?P<flushes>[\d.]+) "
    r"readbacks=(?P<readbacks>[\d.]+) upload=(?P<megabytes>[\d.]+) MB/frame(?: culled=(?P<culled>[\d.]+))?"
)

def seconds(value):
    h, m, s = map(float, value.split(":"))
    return h * 3600 + m * 60 + s

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--pid", help="App PID; defaults to the latest reporting process")
    parser.add_argument("--warmup", type=float, default=6, help="Seconds after first world report to omit")
    args = parser.parse_args()
    rows = [m.groupdict() for line in args.log.read_text(encoding="utf-8-sig", errors="replace").splitlines()
            if (m := PATTERN.match(line))]
    if not rows:
        parser.error("No frame telemetry. Enable [vr] frame_stats = 1 and capture logcat.")
    pid = args.pid or rows[-1]["pid"]
    rows = [row for row in rows if row["pid"] == pid and row["scene"] == "world"]
    if not rows:
        parser.error("No world frames for this process.")
    start = seconds(rows[0]["time"])
    rows = [row for row in rows if (seconds(row["time"]) - start) % 86400 >= args.warmup]
    if not rows:
        parser.error("No world reports after warm-up.")
    fps = [float(row["fps"]) for row in rows]
    result = {
        "process_id": pid, "first_report": rows[0]["time"], "last_report": rows[-1]["time"],
        "world_reporting_windows": len(rows), "warmup_seconds": args.warmup,
        "window_fps_mean": round(statistics.mean(fps), 2),
        "window_fps_median": round(statistics.median(fps), 2),
        "window_fps_min": min(fps), "window_fps_max": max(fps),
        "note": "FPS distribution is across reporting windows, not individual frames; driving is not a deterministic benchmark.",
        "mean_cpu_elapsed_ms": {key: round(statistics.mean(float(row[key]) for row in rows), 3)
                                for key in ("game", "wait", "upload", "draw", "submit")},
        "mean_per_frame": {key: round(statistics.mean(float(row[key] or 0) for row in rows), 2)
                           for key in ("draws", "passes", "flushes", "readbacks", "megabytes", "culled")},
    }
    print(json.dumps(result, indent=2))

if __name__ == "__main__":
    main()
