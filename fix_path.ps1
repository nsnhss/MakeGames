# 修复系统 PATH 中的引号问题（需要管理员权限运行）
$current = [Environment]::GetEnvironmentVariable('Path', 'Machine')
$paths = $current -split ';'
$cleanPaths = @()
$hasMingw = $false
$hasNinja = $false

foreach ($p in $paths) {
    $trimmed = $p.Trim().Trim('"')
    if ($trimmed -eq '') { continue }
    
    if ($trimmed -match 'CLion.*mingw.*bin') {
        if (-not $hasMingw) {
            $cleanPaths += $trimmed
            $hasMingw = $true
        }
        continue
    }
    if ($trimmed -match 'CLion.*ninja') {
        if (-not $hasNinja) {
            $cleanPaths += $trimmed
            $hasNinja = $true
        }
        continue
    }
    $cleanPaths += $p
}

if (-not $hasMingw) {
    $cleanPaths += 'D:\JetBrains\CLion 2025.2.5\bin\mingw\bin'
}
if (-not $hasNinja) {
    $cleanPaths += 'D:\JetBrains\CLion 2025.2.5\bin\ninja\win\x64'
}

$newPath = ($cleanPaths -join ';')
[Environment]::SetEnvironmentVariable('Path', $newPath, 'Machine')

Write-Host "✅ 系统 PATH 已修复。关闭所有 PowerShell 窗口，重新打开验证："
Write-Host "   g++.exe --version"
Write-Host "   ninja.exe --version"
