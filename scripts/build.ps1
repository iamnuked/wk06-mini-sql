param(
    [string]$OutputDir = "build"
)

$ErrorActionPreference = "Stop"

function Get-ZigExe {
    if ($env:ZIG_EXE -and (Test-Path $env:ZIG_EXE)) {
        return $env:ZIG_EXE
    }

    $wingetPath = Join-Path $env:LOCALAPPDATA "Microsoft\WinGet\Packages\zig.zig_Microsoft.Winget.Source_8wekyb3d8bbwe\zig-x86_64-windows-0.15.2\zig.exe"
    if (Test-Path $wingetPath) {
        return $wingetPath
    }

    throw "zig.exe not found. Set ZIG_EXE or install Zig."
}

$zig = Get-ZigExe
New-Item -ItemType Directory -Force $OutputDir | Out-Null

$sources = @(
    "src/common.c",
    "src/ast.c",
    "src/lexer.c",
    "src/parser.c",
    "src/storage.c",
    "src/executor.c",
    "src/main.c"
)

$testSources = @(
    "src/common.c",
    "src/ast.c",
    "src/lexer.c",
    "src/parser.c",
    "src/storage.c",
    "src/executor.c",
    "tests/test_runner.c"
)

& $zig cc -std=c11 -Wall -Wextra -Werror -Iinclude @sources -o "$OutputDir/mini_sql.exe"
& $zig cc -std=c11 -Wall -Wextra -Werror -Iinclude @testSources -o "$OutputDir/test_runner.exe"

Write-Host "Build completed:"
Write-Host "  $OutputDir/mini_sql.exe"
Write-Host "  $OutputDir/test_runner.exe"
