# compile_shaders.ps1 - Compile assets/shaders GLSL -> SPIR-V using glslc from Vulkan SDK
# Usage: powershell -ExecutionPolicy Bypass -File tools/compile_shaders.ps1
$ErrorActionPreference = "Stop"

$glslc = Join-Path $env:VULKAN_SDK "Bin/glslc.exe"
if (-not (Test-Path $glslc)) {
    Write-Error "glslc.exe not found: $glslc (check VULKAN_SDK env var)"
    exit 1
}

$shader_dir = Join-Path $PSScriptRoot "../assets/shaders"
$sources = Get-ChildItem -Path "$shader_dir\*" -Include *.vert, *.frag -File
if ($sources.Count -eq 0) {
    Write-Error "No shader sources found in $shader_dir"
    exit 1
}

$failed = 0
foreach ($src in $sources) {
    $out = "$($src.FullName).spv"
    Write-Output "glslc $($src.Name) -> $($src.Name).spv"
    & $glslc $src.FullName -o $out
    if ($LASTEXITCODE -ne 0) { $failed++ }
}

if ($failed -gt 0) {
    Write-Error "$failed shader(s) failed to compile"
    exit 1
}
Write-Output "All $($sources.Count) shaders compiled OK"
