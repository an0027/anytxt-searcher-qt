# deps.ps1 — 部署 AnyTXT Searcher 运行时 DLL
# 用法: PowerShell -ExecutionPolicy Bypass -File deps.ps1

$releaseDir = "$PSScriptRoot\build\Release"

if (-not (Test-Path "$releaseDir\anytxt-searcher.exe")) {
    Write-Host "请先编译: cmake --build build --config Release"
    exit 1
}

# 1. 找到 windeployqt
$windeploy = Get-Command "windeployqt" -ErrorAction SilentlyContinue
if (-not $windeploy) {
    $found = Get-ChildItem "C:\Qt\*\msvc*\bin\windeployqt.exe" | Select-Object -First 1
    if ($found) { $windeploy = $found.FullName }
}
if (-not $windeploy) {
    $found = Get-ChildItem "C:\Qt\*\mingw*\bin\windeployqt.exe" | Select-Object -First 1
    if ($found) { $windeploy = $found.FullName }
}
if (-not $windeploy) {
    Write-Host "找不到 windeployqt。请把 windeployqt.exe 所在目录加入 PATH。"
    exit 1
}
Write-Host "windeployqt: $windeploy"

# 2. windeployqt 部署 Qt DLL（版本绝对正确）
& $windeploy "$releaseDir\anytxt-searcher.exe" --release --no-compiler-runtime 2>&1 | Out-Null
Write-Host "Qt DLLs deployed"

# 3. 从 vcpkg 复制第三方 DLL（跳过 Qt，避免版本覆盖）
$vcpkgDirs = @(
    "C:\tools\vcpkg\installed\x64-windows\bin",
    "$env:LOCALAPPDATA\vcpkg\installed\x64-windows\bin",
    "$env:VCPKG_ROOT\installed\x64-windows\bin",
    "C:\vcpkg\installed\x64-windows\bin"
)
foreach ($dir in $vcpkgDirs) {
    if (Test-Path $dir) {
        Get-ChildItem $dir -Filter "*.dll" | ForEach-Object {
            if ($_.Name -notmatch '^Qt6') {  # 跳过 Qt DLL
                Copy-Item $_.FullName $releaseDir -Force -ErrorAction SilentlyContinue
            }
        }
        Write-Host "vcpkg DLLs: $dir"
        break
    }
}

$count = (Get-ChildItem $releaseDir -Filter "*.dll").Count
Write-Host "完成: $releaseDir ($count DLLs)"
