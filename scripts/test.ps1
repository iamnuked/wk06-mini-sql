$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "build"
$dbRoot = Join-Path $root "tests\tmp\functional_db"
$sqlDir = Join-Path $root "tests\tmp\functional_sql"

& (Join-Path $PSScriptRoot "build.ps1") -OutputDir $buildDir

& (Join-Path $buildDir "test_runner.exe")

New-Item -ItemType Directory -Force $dbRoot, $sqlDir, (Join-Path $dbRoot "demo") | Out-Null
Set-Content -Path (Join-Path $dbRoot "demo\students.schema") -Value "id|name|major" -NoNewline
Set-Content -Path (Join-Path $dbRoot "demo\students.data") -Value "" -NoNewline

@"
INSERT INTO demo.students (id, name, major) VALUES (1, 'Alice', 'DB');
INSERT INTO demo.students (id, name, major) VALUES (2, 'Bob', 'AI');
SELECT * FROM demo.students;
SELECT name FROM demo.students WHERE id = 2;
"@ | Set-Content -Path (Join-Path $sqlDir "workflow.sql") -NoNewline

$output = & (Join-Path $buildDir "mini_sql.exe") $dbRoot (Join-Path $sqlDir "workflow.sql")
$outputText = ($output | Out-String)

if ($LASTEXITCODE -ne 0) {
    throw "mini_sql execution failed"
}

if ($outputText -notmatch "INSERT 0 1") {
    throw "Expected INSERT output was not found."
}

if ($outputText -notmatch "Alice") {
    throw "Expected SELECT output for Alice was not found."
}

if ($outputText -notmatch "Bob") {
    throw "Expected SELECT output for Bob was not found."
}

if ($outputText -notmatch "\(1 rows\)") {
    throw "Expected filtered SELECT row count was not found."
}

Write-Host "All functional tests passed."
