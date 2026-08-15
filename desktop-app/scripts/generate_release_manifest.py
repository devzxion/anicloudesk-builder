#!/usr/bin/env python3
"""Create and detach-sign the exact AniCloud desktop release manifest."""

from __future__ import annotations

import argparse
import base64
import hashlib
import json
from pathlib import Path

from nacl.signing import SigningKey


def classify(name: str) -> tuple[str, str, str]:
    lower = name.lower()
    platform = "windows" if "windows" in lower else "macos" if "macos" in lower else "linux"
    architecture = "arm64" if "arm64" in lower else "x64"
    if lower.endswith(".appimage"):
        kind = "appimage"
    elif lower.endswith(".deb"):
        kind = "deb"
    elif lower.endswith(".dmg"):
        kind = "dmg"
    elif "setup" in lower and lower.endswith(".exe"):
        kind = "installer"
    elif lower.endswith(".zip"):
        kind = "portable"
    else:
        raise ValueError(f"Unsupported release asset: {name}")
    return platform, architecture, kind


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--assets", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--base-url", required=True)
    parser.add_argument("--secret-key-base64", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    assets = []
    for path in sorted(args.assets.iterdir(), key=lambda value: value.name):
        if not path.is_file() or path.name.startswith("desktop-release-manifest"):
            continue
        platform, architecture, kind = classify(path.name)
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        assets.append({
            "platform": platform,
            "architecture": architecture,
            "kind": kind,
            "url": f"{args.base_url.rstrip('/')}/{path.name}",
            "size": path.stat().st_size,
            "sha256": digest,
        })

    manifest = json.dumps({"version": args.version, "assets": assets}, separators=(",", ":"), sort_keys=True).encode()
    secret = base64.b64decode(args.secret_key_base64, validate=True)
    if len(secret) == 64:
        secret = secret[:32]
    if len(secret) != 32:
        raise ValueError("UPDATE_ED25519_SECRET_KEY_BASE64 must decode to a 32-byte seed or 64-byte secret key")
    signature = SigningKey(secret).sign(manifest).signature
    args.output.write_bytes(manifest)
    args.output.with_suffix(args.output.suffix + ".sig").write_text(base64.b64encode(signature).decode() + "\n", encoding="ascii")


if __name__ == "__main__":
    main()
