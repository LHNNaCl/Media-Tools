$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

$cmakeText = Get-Content -Raw -Encoding UTF8 "CMakeLists.txt"
if ($cmakeText -notmatch 'project\(MediaTool VERSION ([0-9]+\.[0-9]+\.[0-9]+)') {
    throw "CMakeLists.txt is missing project(MediaTool VERSION x.y.z)"
}
$version = $Matches[1]

$cmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmakeCmd) { $cmake = $cmakeCmd.Source } else { $cmake = "D:\Install\CMake\bin\cmake.exe" }
if (-not (Test-Path $cmake)) { throw "找不到 CMake" }

$qt = "C:\Install\Qt\6.5.3\msvc2019_64"
$deploy = Join-Path $qt "bin\windeployqt.exe"
if (-not (Test-Path $deploy)) { throw "找不到 windeployqt" }

& $cmake -S . -B build -G "Visual Studio 17 2022" -A x64 "-DCMAKE_PREFIX_PATH=$qt"
if ($LASTEXITCODE -ne 0) { throw "CMake 配置失败" }
& $cmake --build build --config Release
if ($LASTEXITCODE -ne 0) { throw "编译失败" }
if (Test-Path publish) { Remove-Item -Recurse -Force publish }
& $cmake --install build --prefix publish --config Release
if ($LASTEXITCODE -ne 0) { throw "安装到 publish 失败" }

& $deploy --release --no-translations (Join-Path "publish" "MediaTool.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt 失败" }

$crt = Get-ChildItem "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Redist\MSVC\*\x64\Microsoft.VC143.CRT" -Directory -ErrorAction SilentlyContinue | Select-Object -First 1
if ($crt) { Copy-Item (Join-Path $crt.FullName "*.dll") "publish" -Force }

$ffmpegBin = "D:\Install\FFmpeg\bin"
if (Test-Path "third_party\ffmpeg\ffmpeg.exe") { $ffmpegBin = "third_party\ffmpeg" }
New-Item -ItemType Directory -Force -Path "publish\ffmpeg" | Out-Null
foreach ($name in @("ffmpeg.exe", "ffprobe.exe")) {
    $source = Join-Path $ffmpegBin $name
    if (-not (Test-Path $source)) { throw "缺少随包文件 $source" }
    Copy-Item $source (Join-Path "publish\ffmpeg" $name) -Force
}
foreach ($name in @("LICENSE.txt", "FFMPEG-LICENSE.txt", "SOURCE.txt")) {
    $source = Join-Path "third_party\licenses" $name
    if (-not (Test-Path $source)) { throw "缺少许可证 $source" }
}
New-Item -ItemType Directory -Force -Path "publish\licenses" | Out-Null
Copy-Item "third_party\licenses\*" "publish\licenses" -Force

New-Item -ItemType Directory -Force -Path artifacts | Out-Null
$zip = Join-Path "artifacts" "MediaTool-Portable-$version.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory((Resolve-Path "publish"), (Join-Path (Resolve-Path "artifacts") "MediaTool-Portable-$version.zip"))

$isccCmd = Get-Command ISCC -ErrorAction SilentlyContinue
$iscc = $null
if ($isccCmd) { $iscc = $isccCmd.Source }
if (-not $iscc) {
    foreach ($candidate in @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
        "$env:LocalAppData\Programs\Inno Setup 6\ISCC.exe"
    )) {
        if (Test-Path $candidate) { $iscc = $candidate; break }
    }
}
if ($iscc) {
    & $iscc "/DMyAppVersion=$version" "installer\MediaTool.iss"
    if ($LASTEXITCODE -ne 0) { throw "安装包生成失败" }
} else {
    Write-Host "未找到 Inno Setup，已跳过安装包。"
}

Write-Host "已发布 $version"
