# MultiTool (Bahasa Indonesia)

Toolkit Windows serba ada, ditulis dalam **C native (Win32)** — satu file `multi-tool.exe` (~0,6 MB, tanpa runtime/DLL tambahan).
Tampilan dark UI digambar sendiri (anti-aliasing, grafik live, tabel data).

**Dikembangkan oleh Exter Interactive — ALIF HAKIM** · berdasarkan MultiTool Python oleh Sobing4413.

## Cara build
```bat
scripts\build_msvc.bat     :: Visual Studio 2022 (atau buka multitool.sln -> Release | x64 -> Ctrl+Shift+B)
make                       :: MinGW-w64 (MSYS2), atau scripts\build_mingw.bat
cmake -S . -B build && cmake --build build --config Release
```
Detail: [docs/BUILDING.md](docs/BUILDING.md).

## Fitur
Lihat [docs/FEATURES.md](docs/FEATURES.md) — dashboard, monitor live, info sistem, jaringan, keamanan, **Malware Scan**, **Driver Scanner**,
proses/startup/aplikasi, cleanup, tools, power, pengaturan (6 tema, 18 bahasa).

## Struktur folder
Lihat bagian *Repository layout* di [README.md](README.md) dan penjelasan arsitektur di [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Catatan penting
* Fitur tertentu butuh **Administrator** (WiFi on/off & password, Windows\Temp/Prefetch, SFC, backup driver). Klik lencana *Standard user* di sidebar untuk restart sebagai admin.
* Scanner malware bersifat heuristik — **melengkapi, bukan menggantikan antivirus**. Tidak ada file yang dihapus otomatis; karantina bisa dipulihkan.
* Status uji: dikompilasi dengan MinGW-w64 dan diuji di Wine (semua halaman, ±35 fitur). Belum diuji di Windows asli maupun dibuild dengan MSVC.
* Lisensi: GNU GPL v3 (lihat `LICENSE`).
