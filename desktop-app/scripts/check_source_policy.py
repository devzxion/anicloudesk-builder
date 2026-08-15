#!/usr/bin/env python3
"""Static source policy for the browser-free native desktop build."""

from pathlib import Path

repo = Path(__file__).resolve().parents[2]
desktop = repo / "desktop-app"
forbidden_suffixes = {".html", ".htm", ".js", ".jsx", ".ts", ".tsx"}
forbidden_names = {"package.json", "package-lock.json", "anicloud-web.bundle"}
forbidden_terms = ("qtwebengine", "webview", "vite", "anicloud-web.bundle")

for path in desktop.rglob("*"):
    if not path.is_file() or "__pycache__" in path.parts:
        continue
    if path.suffix.lower() in forbidden_suffixes or path.name.lower() in forbidden_names:
        raise SystemExit(f"browser application file in native tree: {path.relative_to(repo)}")
    if path.suffix.lower() in {".cmake", ".txt", ".json", ".cpp", ".h", ".qml"}:
        source = path.read_text(encoding="utf-8", errors="ignore").lower()
        for term in forbidden_terms:
            if term in source:
                raise SystemExit(f"prohibited term {term!r} in {path.relative_to(repo)}")

for path in (repo / "src").rglob("*"):
    if path.suffix.lower() not in {".ts", ".tsx"}:
        continue
    source = path.read_text(encoding="utf-8")
    if "electron-native" in source or "AniCloudDesktop" in source:
        raise SystemExit(f"retired desktop bridge in website source: {path.relative_to(repo)}")

theme = (desktop / "qml" / "Theme.qml").read_text(encoding="utf-8").lower()
for color in ("#e50914", "#b20710", "#09090b"):
    if color not in theme:
        raise SystemExit(f"missing canonical theme color {color}")

print("native desktop source policy passed")
