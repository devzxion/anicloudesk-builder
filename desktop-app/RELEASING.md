# Releasing AniCloud Desktop

Push `desktop-v4.0.0` or dispatch the Desktop Release workflow with that tag. Production builds intentionally fail unless the Windows certificate, Apple Developer ID/notarization credentials, and Ed25519 manifest key are present.

Required GitHub Actions secrets:

- `WINDOWS_CERTIFICATE_BASE64`, `WINDOWS_CERTIFICATE_PASSWORD`
- `APPLE_CERTIFICATE_BASE64`, `APPLE_CERTIFICATE_PASSWORD`, `APPLE_SIGNING_IDENTITY`
- `APPLE_ID`, `APPLE_TEAM_ID`, `APPLE_APP_PASSWORD`
- `UPDATE_ED25519_SECRET_KEY_BASE64` (32-byte seed or 64-byte Ed25519 secret key)
- `RELEASE_GITHUB_TOKEN` (fine-grained token with Contents: write access to `devz-on/anicloudesk`)

The workflow creates a signed NSIS installer and portable ZIP, notarized Intel/Apple Silicon DMGs, an x64 AppImage and DEB, then emits `desktop-release-manifest.json` plus its detached Ed25519 signature. It publishes those verified assets to the public `devz-on/anicloudesk` release repository. The client only accepts an asset whose platform, architecture, size, and SHA-256 are covered by that verified manifest.
