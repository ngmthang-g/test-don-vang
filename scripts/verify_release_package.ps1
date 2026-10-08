# T20 Windows x64 Release package validation; run after Prepare downloadable package.
# Deliberately separate from YAML to keep PowerShell expressions unaltered.
$ErrorActionPreference = 'Stop'
$artifact = Join-Path (Split-Path -Parent $PSScriptRoot) 'artifact'
$expected = @('CongCuHoTroGameRanhTay_10.6.exe', 'ThanLongCleanRouteBridge.dll')
$found = @(Get-ChildItem -Path $artifact -File | Select-Object -ExpandProperty Name)
if ($found.Count -ne 3) {
  throw "T20 package must contain exactly 3 files, got $($found.Count)"
}
foreach ($name in @($expected) + @('SHA256SUMS.txt')) {
  if ($name -cnotin $found) { throw "T20 missing packaged file: $name" }
}
$records = @(Get-Content (Join-Path $artifact 'SHA256SUMS.txt'))
if ($records.Count -ne $expected.Count) {
  throw "T20 checksum manifest must contain exactly two records"
}
for ($i = 0; $i -lt $expected.Count; $i++) {
  $row = $records[$i]
  if ($row -cnotmatch '^([0-9a-f]{64})  ([A-Za-z0-9_.-]+)$') {
    throw "T20 malformed checksum row $i"
  }
  $recordedHash = $Matches[1]
  $recordedName = $Matches[2]
  if ($recordedName -cne $expected[$i]) { throw "T20 unexpected checksum filename: $recordedName" }
  $p = Join-Path $artifact $recordedName
  $actualHash = (Get-FileHash -Algorithm SHA256 $p).Hash.ToLowerInvariant()
  if ($actualHash -cne $recordedHash) { throw "T20 checksum mismatch: $recordedName" }
  $bytes = [System.IO.File]::ReadAllBytes($p)
  if ($bytes.Length -lt 256 -or [BitConverter]::ToUInt16($bytes, 0) -ne 0x5a4d) {
    throw "T20 expected MZ executable: $recordedName"
  }
  $offset = [BitConverter]::ToInt32($bytes, 0x3c)
  if ($offset -lt 64 -or ($offset + 24) -gt $bytes.Length) {
    throw "T20 invalid PE header: $recordedName"
  }
  if ([BitConverter]::ToUInt32($bytes, $offset) -ne 0x00004550 -or
      [BitConverter]::ToUInt16($bytes, $offset + 4) -ne 0x8664) {
    throw "T20 executable must be PE x64: $recordedName"
  }
}
Write-Host 'T20 Release package PASS: 2 verified x64 PE binaries, 2 SHA256 hashes, no extras'
