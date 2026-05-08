<#
.SYNOPSIS
    Build AnyTXT Searcher MSI installer from Release build
.DESCRIPTION
    Requires WiX Toolset v3 (candle.exe + light.exe) or v7 (wix.exe).
    Install with: winget install WiXToolset.WiXCLI
#>

param(
    [string]$ReleaseDir = ""
)

# Auto-detect Release dir
if (-not $ReleaseDir) {
    $possible = @(
        "C:\Users\Shuch\.openclaw\workspace\anytxt-searcher-qt\build\Release",
        "C:\Users\Shuch\.openclaw\workspace\anytxt-searcher-qt\build",
        (Join-Path $PSScriptRoot "..")
    )
    foreach ($p in $possible) {
        if (Test-Path (Join-Path $p "anytxt-searcher.exe")) { $ReleaseDir = $p; break }
    }
}

if (-not $ReleaseDir -or -not (Test-Path (Join-Path $ReleaseDir "anytxt-searcher.exe"))) {
    Write-Host "✗ Cannot find anytxt-searcher.exe. Pass -ReleaseDir or place script near it." -ForegroundColor Red
    exit 1
}

$ReleaseDir = (Resolve-Path $ReleaseDir).Path
$WxsFile = Join-Path $PSScriptRoot "anytxt-searcher.wxs"
$OutputMsi = Join-Path $PSScriptRoot "AnyTXT-Searcher-x64.msi"

# ---- Detect WiX ----
$CandlePath = $null; $LightPath = $null; $WixPath = $null
$cmd = Get-Command "candle.exe" -ErrorAction SilentlyContinue
if ($cmd) { $CandlePath = $cmd.Source }
$cmd = Get-Command "light.exe" -ErrorAction SilentlyContinue
if ($cmd) { $LightPath = $cmd.Source }
$cmd = Get-Command "wix.exe" -ErrorAction SilentlyContinue
if ($cmd) { $WixPath = $cmd.Source }

if (-not $CandlePath -and -not $WixPath) {
    Write-Host "╔══════════════════════════════════════════════════════════╗" -ForegroundColor Red
    Write-Host "║  WiX Toolset not found! Install with:                  ║" -ForegroundColor Red
    Write-Host "║    winget install WiXToolset.WiXCLI                    ║" -ForegroundColor Red
    Write-Host "╚══════════════════════════════════════════════════════════╝" -ForegroundColor Red
    exit 1
}

# ---- Get version from exe ----
$Version = "1.0.0.0"
try {
    $vi = [System.Diagnostics.FileVersionInfo]::GetVersionInfo((Join-Path $ReleaseDir "anytxt-searcher.exe"))
    $v = "$($vi.FileMajorPart).$($vi.FileMinorPart).$($vi.FileBuildPart).$($vi.FilePrivatePart)"
    if ($v -ne "0.0.0.0") { $Version = $v }
} catch {}

Write-Host "═══ AnyTXT Searcher MSI Builder ═══" -ForegroundColor Cyan
Write-Host "Version: $Version" -ForegroundColor Cyan
Write-Host "Release dir: $ReleaseDir" -ForegroundColor Cyan
Write-Host "Output: $OutputMsi" -ForegroundColor Cyan

Push-Location $PSScriptRoot
try {
    if ($WixPath) {
        Write-Host "`nBuilding with WiX v7..." -ForegroundColor Green
        & $WixPath build $WxsFile -o $OutputMsi -arch x64 -d BuildDir="$ReleaseDir" -d Version=$Version
    } else {
        Write-Host "`nBuilding with WiX v3..." -ForegroundColor Green
        Write-Host "  Candle: $CandlePath" -ForegroundColor Gray
        Write-Host "  Light:  $LightPath" -ForegroundColor Gray

        & $CandlePath -arch x64 -dBuildDir="$ReleaseDir" -dVersion=$Version `
            -out "anytxt-searcher.wixobj" $WxsFile
        if ($LASTEXITCODE -ne 0) { Write-Host "Candle failed!" -ForegroundColor Red; exit 1 }

        & $LightPath -out $OutputMsi -ext WixUIExtension "anytxt-searcher.wixobj"
        if ($LASTEXITCODE -ne 0) { Write-Host "Light failed!" -ForegroundColor Red; exit 1 }
    }

    if (Test-Path $OutputMsi) {
        $size = (Get-Item $OutputMsi).Length / 1MB
        Write-Host "`n✓ MSI created: $OutputMsi" -ForegroundColor Green
        Write-Host "  Size: $([math]::Round($size, 1)) MB" -ForegroundColor Green
        Write-Host "`nInstall: msiexec /i `"$OutputMsi`"" -ForegroundColor Cyan
        Write-Host "Silent:  msiexec /quiet /norestart /i `"$OutputMsi`"" -ForegroundColor Cyan
    }
} finally { Pop-Location }
