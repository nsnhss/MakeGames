# package_release.ps1 — 组装引擎发布包（launcher + editor + game 运行时 + 示例项目）
# 用法: powershell -ExecutionPolicy Bypass -File tools/package_release.ps1
# 产物: release/MakeGames/ 目录 + release/MakeGames-yyyyMMdd-HHmm.zip
param(
    [switch]$SkipBuild   # 跳过构建（直接打包现有 cmake-build-release 产物）
)

$ErrorActionPreference = "Stop"
$repo = Split-Path $PSScriptRoot -Parent
$buildDir = Join-Path $repo "cmake-build-release"

# 1. 构建全部目标
if (-not $SkipBuild) {
    & (Join-Path $repo "build.ps1") all
    if ($LASTEXITCODE -ne 0) { throw "构建失败" }
}

# 2. 组装目录
$releaseRoot = Join-Path $repo "release"
$pkgDir = Join-Path $releaseRoot "MakeGames"
if (Test-Path $pkgDir) { Remove-Item $pkgDir -Recurse -Force }
New-Item -ItemType Directory -Path $pkgDir | Out-Null

# 可执行文件 + 运行时 DLL
$binaries = @(
    @("$buildDir\launcher\launcher.exe", "launcher.exe"),
    @("$buildDir\editor\editor.exe",     "editor.exe"),
    @("$buildDir\src\game.exe",         "game.exe"),
    @("$buildDir\launcher\SDL2.dll",     "SDL2.dll")
)
foreach ($pair in $binaries) {
    $src = $pair[0]
    if (-not (Test-Path $src)) { throw "缺少构建产物: $src（先跑 build.ps1 all）" }
    Copy-Item $src (Join-Path $pkgDir $pair[1])
}

# 引擎内置资源（字体 + 着色器，editor/game 运行时都需要）
Copy-Item (Join-Path $repo "assets") (Join-Path $pkgDir "assets") -Recurse

# 示例项目（已入库，随发布包分发）
$demo = Join-Path $repo "demo_project"
if (-not (Test-Path $demo)) { throw "demo_project 不存在；先运行 python tools/generate_demo_project.py" }
Copy-Item $demo (Join-Path $pkgDir "demo_project") -Recurse

# 3. 压缩
$stamp = Get-Date -Format "yyyyMMdd-HHmm"
$zipPath = Join-Path $releaseRoot "MakeGames-$stamp.zip"
if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
Compress-Archive -Path $pkgDir -DestinationPath $zipPath

Write-Host "`n✅ 发布包: $zipPath" -ForegroundColor Green
Write-Host "   目录:   $pkgDir" -ForegroundColor Green
