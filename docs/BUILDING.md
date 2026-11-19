# Building MultiTool

The result is always one file: `multi-tool.exe` (x64). All import libraries are system DLLs shipped with Windows 10/11.

## Visual Studio 2022 (recommended)
1. Install the *Desktop development with C++* workload (`.vsconfig` lists the required components).
2. Open `multitool.sln`, choose **Release | x64**, press `Ctrl+Shift+B`.
3. Output: `bin\x64\Release\multi-tool.exe`.

Command line: `scripts\build_msvc.bat`.
The CRT is linked statically (`/MT`) so no Visual C++ Redistributable is required.
`Directory.Build.props` defines the output folders (`bin\`, `obj\`) and the code-analysis rule set (`multitool.ruleset`).

> After adding, removing or renaming source files run `python tools/gen_vsproj.py` to refresh `multitool.vcxproj` and its filters.

## MinGW-w64
```
# MSYS2 (MinGW x64 shell)
pacman -S mingw-w64-x86_64-gcc make
make CC=gcc WINDRES=windres

# Linux / macOS cross-compile
sudo apt install gcc-mingw-w64-x86-64 make
make                    # uses x86_64-w64-mingw32-gcc
make DEBUG=1            # -O0 -g
```
`make i18n` regenerates `src/i18n/*` from `tools/language_source.py`.

## CMake
```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 && cmake --build build --config Release
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake     # cross-compile
```

## Troubleshooting
**`CVT1100: duplicate resource. type:MANIFEST, name:1` / `LNK1123: failure during conversion to COFF`**
The application manifest was embedded twice. In this repository the manifest (`res/multitool.manifest`) is embedded exactly once per toolchain:
* MSVC / MSBuild: by the manifest tool, from the `<Manifest Include="res\multitool.manifest" />` item of `multitool.vcxproj`.
* CMake + MSVC: by CMake, because the manifest is listed as a target source.
* MinGW / `make`: by `windres`, from `res/multitool.rc` (the `#ifdef __GNUC__` block).

If you still see the error after updating, **delete the old `bin\`, `obj\` and `build\` folders** (stale `*.embed.manifest.res` files from an earlier build keep the duplicate alive) and rebuild.
Do not add a second `1 RT_MANIFEST` line to the `.rc` file when building with MSVC.

## Conventions
* C11, sources are **pure ASCII**; non-ASCII glyphs are written as `\x....` escapes (see `U_*` macros in `core/common.h`).
* Include project headers by path relative to `src/` (e.g. `#include "core/util.h"`).
* Wide strings with `%ls` in every printf-style call (portable between MSVC and MinGW).

## Smoke test (Linux + Wine)
```
tests/make_fixtures.sh            # EICAR + obfuscated script fixtures
tests/smoke_wine.sh               # runs ~25 features headlessly via the --run= hook
```
Developer switches of the executable: `--page=N` opens page N; `--run=FP,IDX,OPT,OUTFILE[,TEXT]` runs one feature, writes its output and exits.
