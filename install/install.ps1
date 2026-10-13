<#
  SeriousHL installer: Half-Life movement for Serious Sam Classic (The First Encounter / The Second Encounter).

  Install.cmd runs this. It finds the games (Steam libraries, GOG, or folders given / dropped on Install.cmd), makes
  sure Serious Sam Classics Patch 1.9.4 is there (offers to download it from its GitHub release, checked by SHA-256),
  copies SeriousHL.dll into Bin\Plugins and puts a shortcut on the desktop.

    install.ps1 [-Uninstall] [-Yes] [-NoShortcut] ["<game folder>" ...]

  Offline: put Classics Patch's zip (TFE1.05_2025_10_15.zip / TSE1.07_2025_10_15.zip) next to this file.
#>
param(
  [switch]$Uninstall,
  [switch]$Yes,
  [switch]$NoShortcut,
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
  if (-not $NoShortcut -and (Ask "  Put a shortcut on the desktop (it starts the game with Classics Patch)?" $true)) { New-Shortcut $Dir $kind }
}

function Uninstall-From([string]$Dir) {
  $kind = Get-Kind $Dir
  $dll = Join-Path $Dir 'Bin\Plugins\SeriousHL.dll'
  if (Test-Path $dll) { Remove-Item $dll -Force; Write-Host "removed $dll" }
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
    if ($Uninstall) { Uninstall-From $g } else { Install-Into $g }
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
