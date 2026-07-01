# Building from source

These are PowerPC Mac OS applications built with [Retro68](https://github.com/autc04/Retro68).
You only need to build if you want to modify or verify them — prebuilt binaries are in
[`dist/`](dist/) and attached to [Releases](../../releases).

## Prerequisites

- A built Retro68 toolchain (PowerPC target). The CMake toolchain file lives at
  `<retro68-build>/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake`.
- CMake ≥ 3.13, Python 3 (used by `tools/wrap-apm.py` to wrap the raw disk image in an
  Apple Partition Map so it mounts on OS 9).

## Build

Each variant builds the same way. For the silent everyday fix:

```sh
cd src/vbl-fix
cmake -B build -DCMAKE_TOOLCHAIN_FILE=<retro68-build>/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake
cmake --build build
```

For the tester build, use `src/vbl-autofix` instead. Outputs land in `build/`:

- `*.bin` — MacBinary (resource fork preserved; decode on the Mac side).
- `*.img` — Apple Partition Map disk image that mounts directly on OS 9.

Copy the app out of the mounted `.img` into **System Folder → Startup Items**.

## Notes

- Retro68 is PowerPC-only here, so these are PPC **applications**, not classic `INIT`s. An
  app in Startup Items runs after the Finder — early enough to re-arm VBL for the common
  freeze, but not early enough to catch the rare severe variant that hangs during the boot
  switch itself (see [TECHNICAL.md](TECHNICAL.md)). A driver-side fix or an `INIT` would be
  needed for that.
- The apps use only documented Toolbox / Device Manager calls (`PBControlSync` with
  `cscSetInterrupt`, `SlotVInstall`, Time Manager). They touch no driver or system file.
