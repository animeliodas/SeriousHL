<#
  SeriousHL installer: Half-Life movement for Serious Sam Classic (The First Encounter / The Second Encounter).

  Install.cmd runs this. It finds the games (Steam libraries, GOG, or folders given / dropped on Install.cmd), makes
  sure Serious Sam Classics Patch 1.9.4 is there (offers to download it from its GitHub release, checked by SHA-256),
  copies SeriousHL.dll into Bin\Plugins and puts a shortcut on the desktop.

    install.ps1 [-Uninstall] [-Yes] [-NoShortcut] [-NoAssets | -AssetsOnly] [-HalfLife "<Half-Life folder>"] ["<game folder>" ...]

  Offline: put Classics Patch's zip (TFE1.05_2025_10_15.zip / TSE1.07_2025_10_15.zip) next to this file.

  Half-Life's own sounds (and later its weapon models): taken from the player's Half-Life (found in Steam's libraries,
  or -HalfLife) into the game's Sounds\SeriousHL (and Models\SeriousHL). They're Valve's: they stay on this machine.
  Without Half-Life SeriousHL uses Sam's.
#>
# (no positional binding: a game's folder must never land in -HalfLife)
[CmdletBinding(PositionalBinding = $false)]
param(
  [switch]$Uninstall,
  [switch]$Yes,
  [switch]$NoShortcut,
  [switch]$NoAssets,
  [switch]$AssetsOnly,
  [string]$HalfLife = '',
  [Parameter(ValueFromRemainingArguments = $true)][string[]]$Folders
)
$ErrorActionPreference = 'Stop'
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path

# Classics Patch 1.9.4 (github.com/SamClassicPatch/SuperProject/releases/tag/1.9.4)
$Patch = @{
  TFE = @{ Url = 'https://github.com/SamClassicPatch/SuperProject/releases/download/1.9.4/TFE1.05_2025_10_15.zip'
           Sha256 = '4265acf8c6814dedb5b20e2f640482325b92117256ce02e107c5429e6b462aa9'; Size = 901219 }
  TSE = @{ Url = 'https://github.com/SamClassicPatch/SuperProject/releases/download/1.9.4/TSE1.07_2025_10_15.zip'
           Sha256 = '2cd5dcbf9809b63c83bc55e5bf61b9f679460216cf3a188db09cb043df8c6d0b'; Size = 995427 }
}
$Titles = @{ TFE = 'Serious Sam TFE'; TSE = 'Serious Sam TSE' }

function Ask([string]$Question, [bool]$Default) {
  if ($Yes) { return $Default }
  $hint = if ($Default) { '[Y/n]' } else { '[y/N]' }
  $answer = Read-Host "$Question $hint"
  if ([string]::IsNullOrWhiteSpace($answer)) { return $Default }
  return $answer -match '^[yYdD]'
}

# TFE or TSE, by what's in Bin (Steam and GOG alike); $null if it isn't Serious Sam Classic
function Get-Kind([string]$Dir) {
  if (Test-Path (Join-Path $Dir 'Bin\EntitiesMP.dll')) { return 'TSE' }
  if (Test-Path (Join-Path $Dir 'Bin\Entities.dll')) { return 'TFE' }
  return $null
}

function Find-Games {
  $found = @()
  # Steam: the main folder and every library in libraryfolders.vdf
  $steam = (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
  $libraries = @()
  if ($steam) {
    $steam = $steam -replace '/', '\'
    $libraries += $steam
    $vdf = Join-Path $steam 'steamapps\libraryfolders.vdf'
    if (Test-Path $vdf) {
      foreach ($m in [regex]::Matches((Get-Content $vdf -Raw), '"path"\s+"([^"]+)"')) { $libraries += ($m.Groups[1].Value -replace '\\\\', '\') }
    }
  }
  foreach ($lib in ($libraries | Sort-Object -Unique)) {
    foreach ($name in 'Serious Sam Classic The First Encounter', 'Serious Sam Classic The Second Encounter') {
      $dir = Join-Path $lib "steamapps\common\$name"
      if (Get-Kind $dir) { $found += $dir }
    }
  }
  # GOG
  foreach ($key in Get-ChildItem 'HKLM:\SOFTWARE\WOW6432Node\GOG.com\Games' -ErrorAction SilentlyContinue) {
    $p = Get-ItemProperty $key.PSPath -ErrorAction SilentlyContinue
    if ($p.gameName -like '*Serious Sam*' -and $p.path -and (Get-Kind $p.path)) { $found += $p.path }
  }
  return $found | Sort-Object -Unique  # (case-insensitive: Steam gives one path in two spellings)
}

function Install-ClassicsPatch([string]$Dir, [string]$Kind) {
  $p = $Patch[$Kind]
  $local = Join-Path $Here (Split-Path $p.Url -Leaf)
  if (Test-Path $local) {
    $zip = Join-Path $env:TEMP ("SeriousHL_" + (Split-Path $p.Url -Leaf))
    Copy-Item $local $zip -Force
    Write-Host "  from $local"
  } else {
    $zip = Join-Path $env:TEMP ("SeriousHL_" + (Split-Path $p.Url -Leaf))
    Write-Host "  downloading $($p.Url)"
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $p.Url -OutFile $zip -UseBasicParsing
  }
  $hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
  if ($hash -ne $p.Sha256) {
    Remove-Item $zip -Force
    throw "the download doesn't match Classics Patch 1.9.4 (SHA-256 $hash): not installed"
  }
  # as its instructions say: the archive's contents into the game folder, replacing (the originals stay, *_OG)
  Expand-Archive -Path $zip -DestinationPath $Dir -Force
  Remove-Item $zip -Force
  Write-Host "  Classics Patch 1.9.4 installed"
}

# Half-Life's "valve" folder: -HalfLife (the game's folder or valve itself), else Steam's libraries
function Find-HalfLife {
  $candidates = @()
  if ($HalfLife) { $candidates += $HalfLife; $candidates += (Join-Path $HalfLife 'valve') }
  $steam = (Get-ItemProperty 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue).SteamPath
  if ($steam) {
    $steam = $steam -replace '/', '\'
    $libraries = @($steam)
    $vdf = Join-Path $steam 'steamapps\libraryfolders.vdf'
    if (Test-Path $vdf) {
      foreach ($m in [regex]::Matches((Get-Content $vdf -Raw), '"path"\s+"([^"]+)"')) { $libraries += ($m.Groups[1].Value -replace '\\\\', '\') }
    }
    foreach ($lib in ($libraries | Sort-Object -Unique)) { $candidates += (Join-Path $lib 'steamapps\common\Half-Life\valve') }
  }
  foreach ($c in $candidates) {
    if ($c -and (Test-Path (Join-Path $c 'sound\weapons'))) { return (Resolve-Path $c).Path }
  }
  return $null
}

# A wave as Serious Engine reads it: RIFF, WAVE, a 16-byte "fmt ", "data" - nothing else (Half-Life's have LIST and cue)
function Write-CleanWave([string]$Source, [string]$Target) {
  $b = [IO.File]::ReadAllBytes($Source)
  if ($b.Length -lt 12 -or [Text.Encoding]::ASCII.GetString($b, 0, 4) -ne 'RIFF') { return $false }
  $fmt = $null; $data = $null
  $i = 12
  while ($i + 8 -le $b.Length) {
    $id = [Text.Encoding]::ASCII.GetString($b, $i, 4)
    $len = [BitConverter]::ToInt32($b, $i + 4)
    if ($len -lt 0 -or $i + 8 + $len -gt $b.Length) { $len = $b.Length - $i - 8 }
    if ($id -eq 'fmt ' -and $len -ge 16) { $fmt = New-Object byte[] 16; [Array]::Copy($b, $i + 8, $fmt, 0, 16) }
    if ($id -eq 'data') { $data = New-Object byte[] $len; [Array]::Copy($b, $i + 8, $data, 0, $len) }
    $i += 8 + $len + ($len -band 1)
  }
  if ($fmt -eq $null -or $data -eq $null -or [BitConverter]::ToInt16($fmt, 0) -ne 1) { return $false }
  $ms = New-Object IO.MemoryStream
  $w = New-Object IO.BinaryWriter $ms
  $w.Write([Text.Encoding]::ASCII.GetBytes('RIFF')); $w.Write([int](4 + 8 + 16 + 8 + $data.Length + ($data.Length -band 1)))
  $w.Write([Text.Encoding]::ASCII.GetBytes('WAVE'))
  $w.Write([Text.Encoding]::ASCII.GetBytes('fmt ')); $w.Write([int]16); $w.Write($fmt)
  $w.Write([Text.Encoding]::ASCII.GetBytes('data')); $w.Write([int]$data.Length); $w.Write($data)
  if ($data.Length -band 1) { $w.Write([byte]0) }
  $w.Flush()
  [IO.File]::WriteAllBytes($Target, $ms.ToArray())
  return $true
}

function Install-HalfLifeAssets([string]$Dir, [string]$Valve) {
  $root = Join-Path $Dir 'Sounds\SeriousHL'
  $n = 0
  foreach ($sub in 'weapons', 'items', 'player', 'common', 'fvox') {
    $out = Join-Path $root $sub
    New-Item -ItemType Directory -Force -Path $out | Out-Null
    foreach ($f in Get-ChildItem (Join-Path $Valve "sound\$sub") -Filter *.wav -ErrorAction SilentlyContinue) {
      if (Write-CleanWave $f.FullName (Join-Path $out $f.Name)) { $n++ }
    }
  }
  Write-Host "  $n sounds from Half-Life ($Valve) -> $root"
  # what the HEV suit says (its sentences, said word by word from fvox)
  $sentences = Join-Path $Valve 'sound\sentences.txt'
  if (Test-Path -LiteralPath $sentences) { Copy-Item -LiteralPath $sentences -Destination (Join-Path $root 'sentences.txt') -Force }
  # the guns in your hands (view models): copied as they are
  $models = Join-Path $Dir 'Models\SeriousHL'
  New-Item -ItemType Directory -Force -Path $models | Out-Null
  $m = 0
  foreach ($f in Get-ChildItem (Join-Path $Valve 'models') -Filter 'v_*.mdl' -ErrorAction SilentlyContinue) {
    Copy-Item -LiteralPath $f.FullName -Destination (Join-Path $models $f.Name) -Force
    $m++
  }
  Write-Host "  $m view models from Half-Life -> $models"
  # the HUD's pictures and their lists (hud.txt, weapon_*.txt; Half-Life 25th anniversary's 1280 and 2560 sets too)
  $sprites = Join-Path $models 'sprites'
  $s = 0
  foreach ($f in Get-ChildItem (Join-Path $Valve 'sprites') -Recurse -File -Include '*.spr', '*.txt' -ErrorAction SilentlyContinue) {
    $rel = $f.FullName.Substring((Join-Path $Valve 'sprites').Length).TrimStart('\')
    $out = Join-Path $sprites $rel
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $out) | Out-Null
    Copy-Item -LiteralPath $f.FullName -Destination $out -Force
    $s++
  }
  Write-Host "  $s HUD sprites and lists from Half-Life -> $sprites"
}

function New-Shortcut([string]$Dir, [string]$Kind) {
  $desktop = [Environment]::GetFolderPath('Desktop')
  $link = Join-Path $desktop "$($Titles[$Kind]) (Half-Life movement).lnk"
  $shell = New-Object -ComObject WScript.Shell
  $s = $shell.CreateShortcut($link)
  $s.TargetPath = Join-Path $Dir 'Bin\SeriousSam_Custom.exe'
  $s.WorkingDirectory = Join-Path $Dir 'Bin'
  $s.Save()
  Write-Host "  shortcut: $link"
}

function Install-Into([string]$Dir) {
  $kind = Get-Kind $Dir
  Write-Host ""
  Write-Host "$($Titles[$kind]): $Dir"
  $bin = Join-Path $Dir 'Bin'
  $core = Join-Path $bin 'ClassicsCore.dll'
  if (-not (Test-Path $core)) {
    Write-Host "  Serious Sam Classics Patch isn't installed here; SeriousHL is a plugin of it (version 1.9.4)."
    if (-not (Ask "  Download Classics Patch 1.9.4 from its GitHub release (about 10 MB) and install it?" $true)) {
      Write-Host "  skipped: install Classics Patch 1.9.4 (github.com/SamClassicPatch/SuperProject/releases), then run this again"
      return
    }
    Install-ClassicsPatch $Dir $kind
  } elseif ((Get-Item $core).Length -ne $Patch[$kind].Size) {
    Write-Host "  Classics Patch is here, but maybe not 1.9.4 (SeriousHL is built for 1.9.4)."
    if (Ask "  Install Classics Patch 1.9.4 over it?" $false) { Install-ClassicsPatch $Dir $kind }
  }
  $plugins = Join-Path $bin 'Plugins'
  New-Item -ItemType Directory -Force -Path $plugins | Out-Null
  Copy-Item (Join-Path $Here "$kind\SeriousHL.dll") (Join-Path $plugins 'SeriousHL.dll') -Force
  Write-Host "  SeriousHL installed: $plugins\SeriousHL.dll"
  if (-not $NoAssets) { Install-AssetsInto $Dir }
  if (-not $NoShortcut -and (Ask "  Put a shortcut on the desktop (it starts the game with Classics Patch)?" $true)) { New-Shortcut $Dir $kind }
}

function Install-AssetsInto([string]$Dir) {
  $valve = Find-HalfLife
  if (-not $valve) {
    Write-Host "  Half-Life not found (or give -HalfLife): Sam's sounds stay"
    return
  }
  if (Ask "  Take Half-Life's weapon sounds from your Half-Life ($valve)? They stay on this PC" $true) { Install-HalfLifeAssets $Dir $valve }
}

function Uninstall-From([string]$Dir) {
  $kind = Get-Kind $Dir
  $dll = Join-Path $Dir 'Bin\Plugins\SeriousHL.dll'
  if (Test-Path $dll) { Remove-Item $dll -Force; Write-Host "removed $dll" }
  foreach ($sub in 'Sounds\SeriousHL', 'Models\SeriousHL') {
    $p = Join-Path $Dir $sub
    if (Test-Path $p) { Remove-Item $p -Recurse -Force; Write-Host "removed $p (Half-Life's files)" }
  }
  $link = Join-Path ([Environment]::GetFolderPath('Desktop')) "$($Titles[$kind]) (Half-Life movement).lnk"
  if (Test-Path $link) { Remove-Item $link -Force; Write-Host "removed $link" }
}

Write-Host "SeriousHL - Half-Life movement for Serious Sam Classic"
$games = @()
foreach ($f in $Folders) {
  if (-not $f) { continue }
  $f = $f.Trim('"')
  if (Get-Kind $f) { $games += (Resolve-Path $f).Path } else { Write-Host "not a Serious Sam Classic folder: $f" }
}
if ($games.Count -eq 0) { $games = @(Find-Games) }
if ($games.Count -eq 0) {
  $f = Read-Host "Serious Sam Classic not found. Drag its folder (the one with Bin) here and press Enter"
  $f = $f.Trim().Trim('"')
  if ($f -and (Get-Kind $f)) { $games = @((Resolve-Path $f).Path) } else { Write-Host "not a Serious Sam Classic folder"; exit 1 }
}

$failed = $false
foreach ($g in $games) {
  try {
    if ($Uninstall) { Uninstall-From $g } elseif ($AssetsOnly) { Install-AssetsInto $g } else { Install-Into $g }
  } catch {
    Write-Host "  failed: $($_.Exception.Message)" -ForegroundColor Red
    $failed = $true
  }
}
if (-not $Uninstall) {
  Write-Host ""
  Write-Host "Start the game from the shortcut (or Bin\SeriousSam_Custom.exe; in Steam: launch option +game ClassicsPatchMod)."
  Write-Host "The console (~): hl_bAutoJump 0 - a jump per press; hl_bBhopCap 1 - Steam Half-Life's speed cap; hl_bEnable 0 - Sam's own movement."
}
if ($failed) { exit 1 }
