# Contributing

1. **Build** following [docs/BUILDING.md](docs/BUILDING.md). Keep the build free of new warnings (MinGW: `make`, MSVC: `/W3`).
2. **Layout** - put code in the right layer (`core`, `gfx`, `modules`, `features`, `ui`, `app`) and respect the dependency direction in [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
3. **Style** - C11, 4 spaces, ASCII-only sources (use `\x....` escapes / `U_*` macros), `%ls` for wide strings in printf-style calls, `.clang-format` provided.
4. **Text** - user-visible strings via `T2(en, id)` or a key in `tools/language_source.py` (then `make i18n`).
5. **Project files** - after adding/removing sources run `python tools/gen_vsproj.py`.
6. **Test** - `tests/smoke_wine.sh` (Linux + Wine) or exercise the feature manually; update `CHANGELOG.md`.
7. **Safety** - anything destructive (delete, kill, quarantine, registry writes) needs a confirmation and must never target system-critical objects.
