#!/usr/bin/env python3
"""Check staged files for current-year copyright coverage.

This validates only files with a header line that includes both:
  - "Copyright"
  - "Altera" or "FreeRTOS"
"""

from __future__ import annotations

import datetime
import os
import re
import subprocess
import sys
from typing import Iterable, List


OWNER_PATTERNS = (
    re.compile(r"\bAltera\b", re.IGNORECASE),
    re.compile(r"\bFreeRTOS\b", re.IGNORECASE),
)
COPYRIGHT_PATTERN = re.compile(r"copyright", re.IGNORECASE)
YEAR_RANGE_PATTERN = re.compile(r"((?:19|20)\d{2})(?:\s*-\s*((?:19|20)\d{2}))?")
HEADER_LINE_LIMIT = 80


def run_git(args: List[str]) -> bytes:
    return subprocess.check_output(["git"] + args)


def get_staged_files() -> List[str]:
    output = run_git(["diff", "--cached", "--name-only", "-z", "--diff-filter=ACM"])
    entries = output.decode("utf-8", errors="replace").split("\0")
    return [entry for entry in entries if entry]


def read_staged_file(path: str) -> bytes:
    return run_git(["show", f":{path}"])


def header_lines_from_bytes(data: bytes) -> List[str]:
    if b"\0" in data:
        return []
    text = data.decode("utf-8", errors="replace")
    return text.splitlines()[:HEADER_LINE_LIMIT]


def is_target_header_line(line: str) -> bool:
    if not COPYRIGHT_PATTERN.search(line):
        return False
    return any(pattern.search(line) for pattern in OWNER_PATTERNS)


def line_includes_year(line: str, year: int) -> bool:
    for match in YEAR_RANGE_PATTERN.finditer(line):
        start = int(match.group(1))
        end = int(match.group(2) or match.group(1))
        if start <= year <= end:
            return True
    return False


def check_file(path: str, year: int) -> List[str]:
    try:
        data = read_staged_file(path)
    except subprocess.CalledProcessError:
        return []

    header_lines = header_lines_from_bytes(data)
    if not header_lines:
        return []

    target_lines = [line for line in header_lines if is_target_header_line(line)]
    if not target_lines:
        return []

    for line in target_lines:
        if line_includes_year(line, year):
            return []

    return target_lines


def main() -> int:
    try:
        repo_root = run_git(["rev-parse", "--show-toplevel"]).decode("utf-8").strip()
    except subprocess.CalledProcessError:
        print("Error: not inside a git repository.")
        return 2

    os.chdir(repo_root)

    year = datetime.datetime.now().year
    failures = {}

    for path in get_staged_files():
        missing_lines = check_file(path, year)
        if missing_lines:
            failures[path] = missing_lines

    if not failures:
        return 0

    print("Copyright year check failed.")
    print(f"Update the year range to include {year} for Altera/FreeRTOS headers:")
    for path, lines in failures.items():
        print(f"  - {path}")
        for line in lines:
            print(f"    {line.strip()}")
    print("\nExample updates:")
    print(f"  Copyright (C) 2025-2026 Altera Corporation")
    print(f"  Copyright (C) 2025-2026;2028 Altera Corporation")
    return 1


if __name__ == "__main__":
    sys.exit(main())
