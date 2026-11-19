# Feature reference

## Dashboard / Live Monitor
Gauges (CPU, memory, system drive, battery or uptime), 2-minute performance graph, system card, top processes, quick actions, health score.
Live Monitor: CPU, RAM and network (download/upload, totals) graphs with a pause switch.

## Power & System
Scheduled shutdown / restart (through `shutdown.exe`, survives closing the app), sleep / hibernate timers, cancel, lock, countdown ring.

## Info & Monitoring
Full System Info · Hardware Summary · GPU/VGA (+ nvidia-smi) · Disk Space · Disk Health (SMART) · Battery (+ battery report) · Uptime · User Info · Health Report (.txt).

## Processes & Startup
* **Running programs** - PID, name, CPU %, memory, threads, path; sorting, filter, End process, End all matches, open location. Critical system processes are protected.
* **Startup manager** - registry Run/RunOnce (HKCU, HKLM, WOW6432Node) and Startup folders; remove entries.
* **Installed apps** - from the Uninstall registry keys; run the uninstaller.

## Network & Internet
Check IP · IP Detail (location, ISP, ASN, proxy) · Network Info · Ping (ICMP, 10 packets) · Traceroute (TTL probing) · DNS Lookup · Flush DNS ·
WiFi on/off · Saved WiFi passwords (WLAN API) · Speed test (WinHTTP down/up).

## Security & Privacy
Security Overview (7 checks, score) · Firewall (3 profiles) · Open Ports (TCP/UDP, v4/v6, owning process) · Antivirus / Defender · Suspicious connections ·
User accounts · Privacy Cleaner (recent files, thumbnails, DNS, temp).

## Malware Scan
Scans: Quick (processes, startup items, user folders, hosts) · Full · File · Folder. Indicators: EICAR, double extension, RTLO, system-name masquerading,
entropy/packers, RWX sections, entry point, API combinations (injection / keylogger / downloader), script obfuscation, location risk, digital signature
(`WinVerifyTrust` + catalogs). Score -> Low / Medium / High. Actions: quarantine (reversible), open location, copy SHA-256, VirusTotal, export CSV.
Defender: update definitions, quick/full scan, threat history (`MpCmdRun.exe`, `Get-MpThreat`).

## Driver Scanner
Inventory via SetupAPI/CfgMgr32 (name, class, provider, version, date, INF, hardware ID, problem code). States: OK, Old (>3 y), Outdated (>5 y), Generic,
Missing (Code 28), Unsigned (Code 52), Problem, Disabled, Update available. **Check Windows Update** queries `Microsoft.Update.Session` for real driver updates and
matches them by hardware ID. Also: find driver online, Optional Updates page, Device Manager, backup (`pnputil /export-driver`), CSV export, rescan hardware.

## Cleanup & Optimization
Clean temp / Windows\Temp / Prefetch · Empty Recycle Bin · Disk Benchmark (write + uncached read) · Large Files Finder (top 50).

## Shortcuts & Tools
App Launcher (29 apps) · Windows Settings shortcuts (20) · Screenshot · Quick actions (mute, volume via Core Audio, night light, airplane mode, clipboard,
serial / product key, dxdiag, SFC, God Mode) · Hash Calculator (SHA-256 / SHA-1 / MD5).

## Settings
6 accent colors, animations, auto-refresh, 18 languages (menu names from the original translation table; other text in English / Indonesian).
