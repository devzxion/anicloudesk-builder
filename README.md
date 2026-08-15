# AniCloud Native Desktop Builder

This private repository is the isolated build source for AniCloud Native Desktop 4.0. It contains only the C++20/Qt 6 desktop client and its packaging workflow. It does not contain or package Electron, Chromium, Qt WebEngine, HTML, React, Vite output, or the AniCloud website. Verified production assets are published to the public [`devz-on/anicloudesk`](https://github.com/devz-on/anicloudesk) release repository.

Production tag `desktop-v4.0.0` builds and publishes:

- Windows x64: signed NSIS installer and portable ZIP
- macOS 13+: signed and notarized x64 and arm64 DMGs
- Ubuntu 22.04-class Linux x64: AppImage and DEB
- Detached Ed25519-signed release manifest and SHA-256 checksums

Build requirements and local commands are documented in [desktop-app/README.md](desktop-app/README.md). Signing and release secrets are documented in [desktop-app/RELEASING.md](desktop-app/RELEASING.md).

The production workflow intentionally fails when any required Windows, Apple, or manifest-signing credential is unavailable.
