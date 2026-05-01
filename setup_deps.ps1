# AnyTXT Searcher 依赖库安装脚本
# 收集运行时 DLL 到 deps/bin/，确保 Qt 版本匹配

param(
    [string]$OutputDir = "$PSScriptRoot\deps\bin"
)

$depsDir = $OutputDir
New-Item -ItemType Directory -Path $depsDir -Force | Out-Null

# ── 1. Qt DLL (先跑 windeployqt，确保版本正确) ──
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
    Write-Host "[1/3] Qt DLLs (windeployqt): $windeploy"
    # Need a dummy exe for windeployqt to analyze
    $dummyExe = Join-Path $depsDir "anytxt-searcher.exe"
    Copy-Item "$PSScriptRoot\build\Release\anytxt-searcher.exe" $dummyExe -ErrorAction SilentlyContinue
    if (Test-Path $dummyExe) {
        & $windeploy --dir "$depsDir" "$dummyExe" --release --no-compiler-runtime 2>&1 | Out-Null
        Remove-Item $dummyExe -Force -ErrorAction SilentlyContinue
    } else {
        Write-Host "WARNING: Build the project first!"
    }
} else {
    Write-Host "[1/3] windeployqt not found, skipping Qt DLLs"
}

# ── 2. vcpkg 第三方 DLL (排除 Qt DLL，避免版本覆盖) ──
$vcpkgDirs = @(
    "C:\tools\vcpkg\installed\x64-windows\bin",
    "$env:LOCALAPPDATA\vcpkg\installed\x64-windows\bin",
    "$env:VCPKG_ROOT\installed\x64-windows\bin",
    "C:\vcpkg\installed\x64-windows\bin"
)
$vcpkgFound = $false
foreach ($dir in $vcpkgDirs) {
    if (Test-Path $dir) {
        Write-Host "[2/3] vcpkg DLLs: $dir"
        $skipped = 0
        $copied = 0
        Get-ChildItem $dir -Filter "*.dll" -ErrorAction SilentlyContinue | ForEach-Object {
            # ⚠ 跳过 Qt DLL，只用 windeployqt 的版本
            if ($_.Name -match '^Qt6') { $skipped++; return }
            if (-not (Test-Path (Join-Path $depsDir $_.Name))) {
                Copy-Item $_.FullName $depsDir -Force -ErrorAction SilentlyContinue
                $copied++
            }
        }
        Write-Host "   copied: $copied, skipped(Qt): $skipped"
        $vcpkgFound = $true
        break
    }
}
if (-not $vcpkgFound) { Write-Host "[2/3] vcpkg not found" }

# ── 3. 清理 windeployqt 留下的无用文件 ──
Get-ChildItem $depsDir -File | Where-Object { $_.Extension -notin @('.dll', '.exe') } |
    Remove-Item -Force -ErrorAction SilentlyContinue

# 统计
$count = (Get-ChildItem $depsDir -Filter "*.dll").Count
$size = "{0:N0} KB" -f ((Get-ChildItem $depsDir -Recurse | Measure-Object Length -Sum).Sum / 1KB)
Write-Host ""
Write-Host "[3/3] Done: $depsDir ($count DLLs, $size)"
Write-Host ""
Write-Host "使用方法:"
Write-Host "  copy deps\bin\*.dll  build\Release\"
Write-Host "  copy deps\bin\platforms  build\Release\platforms\  (如果存在)"
