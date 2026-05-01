# AnyTXT Searcher 依赖库安装脚本
# 从 vcpkg + Qt 安装目录收集所有运行时 DLL 到 deps/bin/

param(
    [string]$OutputDir = "$PSScriptRoot\deps\bin"
)

$depsDir = $OutputDir
New-Item -ItemType Directory -Path $depsDir -Force | Out-Null

# ── vcpkg DLLs (先复制，这些是主要的) ──
$vcpkgDirs = @(
    "C:\tools\vcpkg\installed\x64-windows\bin",
    "$env:LOCALAPPDATA\vcpkg\installed\x64-windows\bin",
    "$env:VCPKG_ROOT\installed\x64-windows\bin",
    "C:\vcpkg\installed\x64-windows\bin"
)
$vcpkgFound = $false
foreach ($dir in $vcpkgDirs) {
    if (Test-Path $dir) {
        Write-Host "vcpkg DLLs: $dir"
        Get-ChildItem $dir -Filter "*.dll" -ErrorAction SilentlyContinue |
            ForEach-Object { Copy-Item $_.FullName $depsDir -Force -ErrorAction SilentlyContinue }
        $vcpkgFound = $true
        break
    }
}
if (-not $vcpkgFound) { Write-Host "Warning: vcpkg not found at any expected path" }

# ── Qt DLL using windeployqt ──
$windeploy = Get-Command "windeployqt" -ErrorAction SilentlyContinue
if (-not $windeploy) {
    $found = Get-ChildItem "C:\Qt\*\msvc*\bin\windeployqt.exe" -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($found) { $windeploy = $found.FullName }
}
if (-not $windeploy) {
    $found = Get-ChildItem "C:\Qt\*\mingw*\bin\windeployqt.exe" -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($found) { $windeploy = $found.FullName }
}

if ($windeploy) {
    Write-Host "Qt windeployqt: $windeploy"
    $exePath = "$depsDir\_dummy_.exe"
    # Dummy exe not needed - windeployqt can work with --list
    & $windeploy --dir "$depsDir" --release --no-compiler-runtime 2>&1 | Out-Null
} else {
    Write-Host "windeployqt not found - search for Qt DLLs manually"
    $qtRoots = Get-ChildItem "C:\Qt" -Directory -ErrorAction SilentlyContinue
    foreach ($qtDir in $qtRoots) {
        $verDirs = Get-ChildItem $qtDir.FullName -Directory -ErrorAction SilentlyContinue
        foreach ($verDir in $verDirs) {
            $bin = Join-Path $verDir.FullName "bin"
            if (Test-Path $bin) {
                Write-Host "Qt DLLs: $bin"
                Get-ChildItem $bin -Filter "Qt6*.dll" -ErrorAction SilentlyContinue |
                    ForEach-Object { Copy-Item $_.FullName $depsDir -Force -ErrorAction SilentlyContinue }
                # platform plugin
                $platSrc = "$bin\..\plugins\platforms\qwindows.dll"
                $platDst = "$depsDir\..\platforms"
                if (Test-Path $platSrc) {
                    New-Item -ItemType Directory -Path $platDst -Force | Out-Null
                    Copy-Item $platSrc $platDst -Force -ErrorAction SilentlyContinue
                }
                break
            }
        }
    }
}

$count = (Get-ChildItem $depsDir -Filter "*.dll" -Recurse).Count
$size = "{0:N0} KB" -f ((Get-ChildItem $depsDir -Recurse | Measure-Object Length -Sum).Sum / 1KB)
Write-Host ""
Write-Host "===== Done ====="
Write-Host "DLLs: $depsDir ($count files, $size)"
Write-Host "Usage: copy deps\bin\*.dll to your exe folder"
