#!/usr/bin/env python3
import argparse
import json
import re
import sys


LINE_RE = re.compile(
    r'^(?:(?P<pid>\d+)\s+)?'
    r'(?P<time>\d+:\d+:\d+(?:\.\d+)?)\s+'
    r'(?P<syscall>[A-Za-z0-9_]+)'
    r'\((?P<args>.*)\)\s+=\s+'
    r'(?P<result>.*)$'
)

END_TIME_RE = re.compile(r'<(?P<dur>\d+\.\d+)>$')


def parse_timestamp_us(time_str: str) -> int:
    h, m, s = time_str.split(":")
    if "." in s:
        sec, frac = s.split(".", 1)
        usec = (frac + "000000")[:6]
    else:
        sec = s
        usec = "0"
    return ((int(h) * 3600) + (int(m) * 60) + int(sec)) * 1_000_000 + int(usec)


def parse_line(line: str):
    line = line.strip()
    if not line or line.startswith("+++ exited with") or line.startswith("strace:"):
        return None

    match = LINE_RE.match(line)
    if not match:
        return None

    result_part = match.group("result")
    dur_match = END_TIME_RE.search(result_part)
    duration_us = 0
    if dur_match:
        duration_us = int(float(dur_match.group("dur")) * 1_000_000)

    pid = int(match.group("pid") or 1)
    ts = parse_timestamp_us(match.group("time"))

    event_args = {
        "args": match.group("args"),
        "result": result_part,
    }

    if match.group("syscall") == "ioctl":
        ioctl_req = match.group("args").split(",", 1)[1].strip() if "," in match.group("args") else ""
        event_args["ioctl_request"] = ioctl_req

    return {
        "name": match.group("syscall"),
        "cat": "syscall",
        "ph": "X",
        "ts": ts,
        "dur": duration_us,
        "pid": pid,
        "tid": pid,
        "args": event_args,
    }


def main():
    parser = argparse.ArgumentParser(
        description="Convert strace -f -t -T output into a Perfetto/Chrome trace JSON."
    )
    parser.add_argument("strace_file", help="Input strace log")
    parser.add_argument(
        "-o",
        "--output",
        help="Output JSON file. Default: stdout",
    )
    parser.add_argument(
        "--trace-name",
        default="strace",
        help="Trace metadata name",
    )
    args = parser.parse_args()

    events = []
    with open(args.strace_file, "r", encoding="utf-8", errors="ignore") as f:
        for line in f:
            event = parse_line(line)
            if event:
                events.append(event)

    if events:
        base_ts = min(event["ts"] for event in events)
        for event in events:
            event["ts"] -= base_ts

    payload = {
        "traceEvents": [
            {
                "name": "process_name",
                "ph": "M",
                "pid": 1,
                "tid": 1,
                "args": {"name": args.trace_name},
            }
        ] + events,
    }

    text = json.dumps(payload, indent=2, ensure_ascii=False)
    if args.output:
        with open(args.output, "w", encoding="utf-8") as f:
            f.write(text)
            f.write("\n")
    else:
        sys.stdout.write(text)
        sys.stdout.write("\n")


if __name__ == "__main__":
    main()
