$buildDir = "$PSScriptRoot\build\Release"
$deployDir = Join-Path $buildDir "deploy"
New-Item -ItemType Directory -Path $deployDir -Force | Out-Null
Copy-Item (Join-Path $buildDir "anytxt-searcher.exe") $deployDir

# Auto-detect windeployqt
$windeploy = Get-Command "windeployqt" -ErrorAction SilentlyContinue
if (-not $windeploy) {
    $qtPaths = @("$env:QTDIR\bin\windeployqt.exe",
                 "C:\Qt\6.*\msvc*\bin\windeployqt.exe",
                 "C:\Qt\6.*\mingw*\bin\windeployqt.exe")
    foreach ($pattern in $qtPaths) {
        $found = Get-ChildItem -Path $pattern -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) { $windeploy = $found.FullName; break }
    }
}
if ($windeploy) {
    Write-Host "Running: $windeploy"
    & $windeploy (Join-Path $deployDir "anytxt-searcher.exe") --release --no-compiler-runtime 2>&1 | Out-Null
} else {
    Write-Host "Warning: windeployqt not found, Qt DLLs must be copied manually"
}

# Copy vcpkg dependency DLLs
$vcpkgDirs = @("C:\tools\vcpkg\installed\x64-windows\bin",
               "$env:VCPKG_ROOT\installed\x64-windows\bin",
               "C:\vcpkg\installed\x64-windows\bin")
foreach ($dir in $vcpkgDirs) {
    if (Test-Path $dir) {
        Get-ChildItem $dir -Filter "*.dll" -ErrorAction SilentlyContinue | 
            ForEach-Object { Copy-Item $_.FullName $deployDir -Force -ErrorAction SilentlyContinue }
        break
    }
}

$count = (Get-ChildItem $deployDir -Filter '*.dll').Count
$size = '{0:N0} KB' -f ((Get-ChildItem $deployDir -Recurse | Measure-Object Length -Sum).Sum / 1KB)
Write-Host "Deploy OK: $deployDir ($count DLLs, $size)"
