# MakeGames 一键编译脚本
# 使用方式: .\build.ps1 [launcher|editor|all|test]

param(
    [string]$Target = "all"
)

$ErrorActionPreference = "Stop"

# 自动探测编译工具链，优先级：
#   1. CLion 捆绑的 MinGW / Ninja（覆盖常见安装位置，取版本号最大的一个）
#   2. 仓库内置 mingw_link/（MinGW）
#   3. 系统 PATH 中的 ninja
$clionRoots = @(
    "D:\JetBrains",
    "$env:LOCALAPPDATA\Programs",
    "C:\Program Files\JetBrains"
)

$mingwPath = $null
$ninjaPath = $null
foreach ($root in $clionRoots) {
    if (-not (Test-Path $root)) { continue }
    $candidate = Get-ChildItem $root -Directory -Filter "CLion*" -ErrorAction SilentlyContinue |
        Where-Object { Test-Path (Join-Path $_.FullName "bin\mingw\bin\g++.exe") } |
        Sort-Object Name -Descending |
        Select-Object -First 1
    if ($candidate) {
        $mingwPath = Join-Path $candidate.FullName "bin\mingw\bin"
        $ninjaPath = Join-Path $candidate.FullName "bin\ninja\win\x64"
        Write-Host "使用工具链: $candidate" -ForegroundColor DarkGray
        break
    }
}

# 回退：仓库内置 MinGW
if (-not $mingwPath) {
    $bundled = Join-Path $PSScriptRoot "mingw_link\bin"
    if (Test-Path (Join-Path $bundled "g++.exe")) {
        $mingwPath = $bundled
        Write-Host "使用仓库内置 MinGW: $mingwPath" -ForegroundColor DarkGray
    }
}

# 回退：系统 PATH 中的 ninja
if (-not $ninjaPath -or -not (Test-Path (Join-Path $ninjaPath "ninja.exe"))) {
    $ninjaCmd = Get-Command ninja -ErrorAction SilentlyContinue
    if ($ninjaCmd) { $ninjaPath = Split-Path $ninjaCmd.Source }
}

if (-not $mingwPath) {
    Write-Error "未找到 MinGW（CLion 捆绑或仓库内置 mingw_link/）。"
    exit 1
}
if (-not $ninjaPath -or -not (Test-Path (Join-Path $ninjaPath "ninja.exe"))) {
    Write-Error "未找到 Ninja（CLion 捆绑或系统 PATH）。"
    exit 1
}

$env:PATH = "$mingwPath;$ninjaPath;" + $env:PATH

$buildDir = "$PSScriptRoot\cmake-build-release"
if (-not (Test-Path $buildDir)) {
    Write-Error "构建目录不存在: $buildDir`n请先运行 CMake 配置生成构建系统。"
    exit 1
}

Set-Location $buildDir

switch ($Target.ToLower()) {
    "launcher" {
        Write-Host "=== 编译 launcher ===" -ForegroundColor Cyan
        ninja launcher
    }
    "editor" {
        Write-Host "=== 编译 editor ===" -ForegroundColor Cyan
        ninja editor
    }
    "all" {
        Write-Host "=== 编译 launcher + editor ===" -ForegroundColor Cyan
        ninja launcher editor
    }
    "test" {
        Write-Host "=== 编译并运行单元测试 ===" -ForegroundColor Cyan
        ninja engine_tests
        if ($LASTEXITCODE -eq 0) { ctest --output-on-failure }
    }
    default {
        Write-Error "未知目标: $Target`n用法: .\build.ps1 [launcher|editor|all|test]"
        exit 1
    }
}

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n✅ 编译成功" -ForegroundColor Green
} else {
    Write-Host "`n❌ 编译失败" -ForegroundColor Red
    exit $LASTEXITCODE
}
