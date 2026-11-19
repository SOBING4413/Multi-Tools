# Architecture

MultiTool is a single-process Win32 application written in C11. There is no UI toolkit: the window is painted by a small **software renderer**
into a 32-bit DIB and blitted to the screen; only text boxes (EDIT) and the output log (RichEdit) are native child windows.

## Layers (dependencies point downwards)

```
app        main.c (WinMain, WndProc) - shell.c (sidebar, top bar, navigation) - splash.c
ui         pages.c (router) - pages/*.c (one module per page) - widgets.c - table.c - logview.c - livedata.c - uiutil.c
features   features.c (table) - system.c network.c cleanup.c security.c tools.c
modules    sys  procs  sysmgr  driver_scan  malware_scan  hash  power_control
gfx        gfx.c  (AA shapes, text, icons, theme)
i18n       i18n.c + generated tables
core       common.h  util  out  settings  version
```

* **core** - string helpers (`wfmt`, `wdup`, `fmt_size`...), process execution with output capture (`run_capture`, `run_ps`), tiny JSON reader,
  styled output sink `MtOut` (UI log *or* report file), INI settings.
* **gfx** - `gfx_rrect`, `gfx_ring`, `gfx_line`, `gfx_poly`, `gfx_graph`, text through GDI, vector icons. Shapes use signed-distance fields for anti-aliasing.
* **modules** - UI-independent back-ends. `driver_scan` and `malware_scan` expose plain C APIs and report progress through callbacks.
* **features** - every feature is `void f_xxx(MtOut* o, const FeatArgs* a)`. `features.c` describes them declaratively (name, description, input kind,
  options, confirmation text); the generic category page renders any `FeatureDef`.
* **ui** - immediate-mode widgets: each frame the page paints and registers *hot spots* (`hot_add(rect, id)`); mouse events are resolved against the previous frame.
  `pages.c` forwards events to the module of the active page (`pg_<page>_paint / _click / _layout / ...`, declared in `ui/pages/pages.h`).
* **app** - owns the window, timers (1 s data tick, 33 ms animation tick), DPI handling and the shell.

## Threading model
The UI thread does all painting. Long operations run in worker threads (`job_run`, one slot per activity, see `ui/internal.h`).
Workers never touch controls: they `PostMessage` to the main window (`WM_APP_LOG`, `WM_APP_DONE`, `WM_APP_TABLE`, `WM_APP_SCAN`, ...),
and `pages_app_msg` dispatches the payloads. Cancellation is cooperative through `Job.cancel`.

## Adding a feature
1. Implement `void f_myfeature(MtOut* o, const FeatArgs* a)` in the matching `src/features/*.c`; declare it in `features/feat.h`.
2. Add one entry to the table in `features/features.c` (name, description, input / options).
3. If you added files: `python tools/gen_vsproj.py`.

## Adding a page
Create `src/ui/pages/<name>.c` exporting the `pg_<name>_*` functions, declare them in `pages/pages.h`, add the page id to `ui/ui.h`
(`PG_*`), the title/icon in `ui/pagemeta.c`, a sidebar row in `app/shell.c`, and the `case` in `ui/pages.c`.

## Localization
`tools/language_source.py` (81 keys x 18 languages) -> `python tools/gen_i18n.py` -> `src/i18n/i18n_data.c` + `i18n_keys.h` (pure ASCII).
Texts without a key use `T2(english, indonesian)`.
