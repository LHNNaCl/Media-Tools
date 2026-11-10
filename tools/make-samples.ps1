$ErrorActionPreference = "Stop"
Set-Location (Join-Path $PSScriptRoot "..")

$ffmpeg = "D:\Install\FFmpeg\bin\ffmpeg.exe"
$bundled = "third_party\ffmpeg\ffmpeg.exe"
if (Test-Path $bundled) { $ffmpeg = $bundled }
if (-not (Test-Path $ffmpeg)) { throw "找不到 FFmpeg" }

$root = "tests\manual-samples"
New-Item -ItemType Directory -Force -Path $root, "$root\side" | Out-Null

function Invoke-Ffmpeg([string[]]$Arguments) {
    & $ffmpeg @Arguments
    if ($LASTEXITCODE -ne 0) { throw "FFmpeg 失败: $($Arguments -join ' ')" }
}

Invoke-Ffmpeg @("-y", "-f", "lavfi", "-i", "color=c=blue:s=320x240:d=2", "-f", "lavfi", "-i", "sine=frequency=440:duration=2", "-shortest", "-c:v", "libx264", "-pix_fmt", "yuv420p", "-c:a", "aac", "$root\basic.mp4")
Invoke-Ffmpeg @("-y", "-f", "lavfi", "-i", "color=c=red:s=320x240:d=2", "-f", "lavfi", "-i", "sine=frequency=440:duration=2", "-f", "lavfi", "-i", "sine=frequency=880:duration=2", "-map", "0:v", "-map", "1:a", "-map", "2:a", "-shortest", "-c:v", "libx264", "-pix_fmt", "yuv420p", "-c:a", "aac", "$root\two-audio.mp4")
Invoke-Ffmpeg @("-y", "-f", "lavfi", "-i", "color=c=blue:s=320x240:d=2", "-f", "lavfi", "-i", "sine=frequency=440:duration=2", "-shortest", "-c:v", "libx264", "-pix_fmt", "yuv420p", "-c:a", "aac", "$root\info-base.mp4")
$srt = Join-Path $env:TEMP "mediatool-sample.srt"
Set-Content -Encoding utf8 $srt "1`r`n00:00:00,000 --> 00:00:01,000`r`n你好`r`n"
$meta = Join-Path $env:TEMP "mediatool-chapter.txt"
Set-Content -Encoding utf8 $meta @"
;FFMETADATA1
[CHAPTER]
TIMEBASE=1/1000
START=10000
END=20000
title=开场
"@
Invoke-Ffmpeg @("-y", "-i", "$root\info-base.mp4", "-i", $srt, "-i", $meta, "-map", "0", "-map", "1", "-map_metadata", "2", "-c", "copy", "-c:s", "srt", "$root\info.mkv")
Remove-Item "$root\info-base.mp4" -Force
Invoke-Ffmpeg @("-y", "-f", "lavfi", "-i", "color=c=blue:s=1920x1080:d=1", "-f", "lavfi", "-i", "sine=frequency=440:duration=1", "-shortest", "-c:v", "libx264", "-pix_fmt", "yuv420p", "-c:a", "aac", "$root\tall.mp4")
Invoke-Ffmpeg @("-y", "-f", "lavfi", "-i", "color=c=blue:s=640x480:d=1", "-f", "lavfi", "-i", "sine=frequency=440:duration=1", "-shortest", "-c:v", "libx264", "-pix_fmt", "yuv420p", "-c:a", "aac", "$root\small.mp4")
Invoke-Ffmpeg @("-y", "-f", "lavfi", "-i", "color=c=blue:s=320x240:d=1", "-an", "-c:v", "libx264", "-pix_fmt", "yuv420p", "$root\merge-a.mp4")
Invoke-Ffmpeg @("-y", "-f", "lavfi", "-i", "color=c=green:s=640x360:d=1", "-an", "-c:v", "libx264", "-pix_fmt", "yuv420p", "$root\merge-b.mp4")
Invoke-Ffmpeg @("-y", "-f", "lavfi", "-i", "color=c=red:s=160x120:d=1", "-an", "-c:v", "libx264", "-pix_fmt", "yuv420p", "$root\merge-c.mp4")
Invoke-Ffmpeg @("-y", "-f", "lavfi", "-i", "color=c=blue:s=320x240:d=1", "-an", "-c:v", "libx264", "-pix_fmt", "yuv420p", "$root\clip2.mp4")
Invoke-Ffmpeg @("-y", "-f", "lavfi", "-i", "color=c=blue:s=320x240:d=1", "-an", "-c:v", "libx264", "-pix_fmt", "yuv420p", "$root\clip10.mp4")
Invoke-Ffmpeg @("-y", "-f", "lavfi", "-i", "sine=frequency=440:duration=0.4", "-c:a", "pcm_s16le", "$root\tone.wav")
Invoke-Ffmpeg @("-y", "-f", "lavfi", "-i", "color=c=blue:s=320x240:d=1", "-an", "-c:v", "libx264", "-pix_fmt", "yuv420p", "$root\side\clip.mp4")
Invoke-Ffmpeg @("-y", "-f", "lavfi", "-i", "color=c=blue:s=320x240:d=1", "-an", "-c:v", "libx264", "-pix_fmt", "yuv420p", "$root\side\other.mp4")
Set-Content -Encoding utf8 "$root\side\clip.ass" "[Script Info]`r`nScriptType: v4.00+`r`n`r`n[Events]`r`nFormat: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, Effect, Text`r`nDialogue: 0,0:00:00.00,0:00:01.00,Default,,0,0,0,,你好`r`n"
Set-Content -Encoding ascii "$root\bad.mp4" "this is not a video"

if (-not (Test-Path "tests\fixtures\image-sub.mkv")) {
    Write-Host "缺少图像字幕样例"
}
