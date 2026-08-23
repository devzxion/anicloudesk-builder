# Releasing AniCloud Desktop

Push `desktop-v4.0.7` or dispatch the Desktop Release workflow with that tag. Production builds intentionally fail unless the Ed25519 manifest key and public release token are present.

Required GitHub Actions secrets:

- `UPDATE_ED25519_SECRET_KEY_BASE64` (32-byte seed or 64-byte Ed25519 secret key)
- `RELEASE_GITHUB_TOKEN` (fine-grained token with Contents: write access to `devz-on/anicloudesk`)

The workflow creates an unsigned NSIS installer and portable ZIP, ad-hoc-signed and unnotarized Intel/Apple Silicon DMGs, an x64 AppImage and DEB, then emits `desktop-release-manifest.json` plus its detached Ed25519 signature. It publishes those integrity-verified assets to the public `devz-on/anicloudesk` release repository. The client only accepts an asset whose platform, architecture, size, and SHA-256 are covered by that verified manifest.
