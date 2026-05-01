$buildDir = "$PSScriptRoot\build\Release"
$deployDir = Join-Path $buildDir "deploy"
New-Item -ItemType Directory -Path $deployDir -Force | Out-Null
Copy-Item (Join-Path $buildDir "anytxt-searcher.exe") $deployDir
$windeploy = "C:\Qt6\6.11.0\msvc2022_64\bin\windeployqt.exe"
if (Test-Path $windeploy) { & $windeploy (Join-Path $deployDir "anytxt-searcher.exe") --release --no-compiler-runtime }
$vcpkgBin = "C:\tools\vcpkg\installed\x64-windows\bin"
Get-ChildItem $vcpkgBin -Filter "*.dll" -ErrorAction SilentlyContinue | ForEach-Object { Copy-Item $_.FullName $deployDir -Force }
Write-Host "Deployed to: $deployDir ($((Get-ChildItem $deployDir -Filter '*.dll').Count) DLLs)"
