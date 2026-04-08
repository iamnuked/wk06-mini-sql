$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build"
$demoDb = Join-Path $root "tests\tmp\demo_db"
$sourceDb = Join-Path $root "examples\db"
$sqlFile = Join-Path $root "examples\sql\demo_workflow.sql"

& (Join-Path $PSScriptRoot "build.ps1") -OutputDir $buildDir

if (Test-Path $demoDb) {
    Remove-Item -Recurse -Force $demoDb
}

New-Item -ItemType Directory -Force $demoDb | Out-Null
Copy-Item -Path (Join-Path $sourceDb "*") -Destination $demoDb -Recurse -Force

& (Join-Path $buildDir "mini_sql.exe") $demoDb $sqlFile
