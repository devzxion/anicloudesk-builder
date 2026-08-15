#!/usr/bin/env python3
"""Reject browser payloads and prohibited native dependencies from a staged package."""

import argparse
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("root", type=Path)
args = parser.parse_args()

forbidden_names = {"index.html", "package.json", "package-lock.json", "anicloud-web.bundle"}
forbidden_fragments = ("electron", "chromium", "qtwebengine", "resources.pak", "app.asar")
offenders = []
for path in args.root.rglob("*"):
    if not path.is_file():
        continue
    lowered = path.name.lower()
    if lowered in forbidden_names or any(fragment in lowered for fragment in forbidden_fragments):
        offenders.append(str(path.relative_to(args.root)))
if offenders:
    raise SystemExit("prohibited desktop package files:\n" + "\n".join(offenders))
print(f"native package policy passed for {args.root}")
