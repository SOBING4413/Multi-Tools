# Changelog

All notable changes to this project are documented here. Format: [Keep a Changelog](https://keepachangelog.com/), versioning: [SemVer](https://semver.org/).

## [3.0.0] - 2026-11
### Added
- Complete rewrite in native C (Win32); single `multi-tool.exe`, no Python.
- Custom dark UI: anti-aliased renderer, gauges, live graphs, sortable/filterable data tables, splash screen, per-monitor DPI, dark title bar.
- **Malware Scan**: heuristic scanner, quarantine/restore, VirusTotal lookup, Microsoft Defender integration.
- **Driver Scanner**: SetupAPI inventory, outdated/missing/unsigned detection, Windows Update driver lookup, backup, CSV export.
- Disk Health (SMART), Health Report, Installed Apps + Uninstall, Large Files Finder, Hash Calculator, Startup removal.
- Native ICMP ping/traceroute, WLAN API (WiFi passwords, no locale-dependent parsing), Core Audio volume control.
- Project infrastructure: layered `src/` tree, Visual Studio solution (generated), `Directory.Build.props`, rule set, Makefile, CMake, CI workflow, docs, smoke tests.

### Fixed
- MSVC / CMake-MSVC link error `CVT1100 duplicate resource. type:MANIFEST, name:1`: the manifest is now embedded once per toolchain (see docs/BUILDING.md).

### Changed
- All features of the Python version are kept; 18 languages preserved (translation table generated from the original source).
