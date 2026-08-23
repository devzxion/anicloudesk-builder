# AniCloud Native Desktop 4.0

This is the official native Windows, macOS, and Linux client. It is a C++20/Qt Quick application; it does not load or package the AniCloud website.

Anime catalog, details, episode, and stream requests are handled by the bundled C++ provider using the same MyAnimeList/MegaPlay data path as the mobile client. `https://api.anicloud.ink/public/api/v1` is used only for authentication, synced user data, maintenance, and notifications.

Native notifications stay available in the system tray after the window closes, poll the broadcast endpoint once per minute, and suppress duplicate alerts. AniCloud starts quietly at login by default, repairs its platform startup registration after updates, and checks both notifications and signed desktop updates; users can disable startup from Profile. A tray action fully quits the process. Public anime share links use `https://anicloud.ink/anime/{myAnimeListId}` and are forwarded to the running native instance through the `anicloud://` deep-link protocol.

## Reproducible toolchain

- CMake 3.28+
- Ninja 1.11+
- Qt 6.10.3 with Qt Declarative, Qt Multimedia, and its FFmpeg backend
- MSVC 2022 on Windows, Apple Clang/Xcode 16 on macOS, or GCC 12+ on Ubuntu 22.04-class Linux
- vcpkg at baseline `e55144c5a465b53bc71bd0b59111ea0b8bb039a5` (libsodium 1.0.22)
- QtKeychain 0.17.0, fetched and pinned by CMake

Install Qt with the platform maintenance tool or `aqtinstall`, set `CMAKE_PREFIX_PATH` to the Qt kit and `VCPKG_ROOT` to a bootstrapped vcpkg checkout, then run:

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Release builds must pass a non-empty `ANICLOUD_UPDATE_PUBLIC_KEY_HEX`:

```sh
cmake --preset release -DANICLOUD_UPDATE_PUBLIC_KEY_HEX=<64-hex-character-ed25519-public-key>
cmake --build --preset release
cmake --install out/build/release --prefix out/stage
```

The native data root is deliberately versioned as `native-v1`; no Electron token, preference, history, or downloaded file is imported or deleted.

See [RELEASING.md](RELEASING.md) for package and signed update-manifest requirements.
