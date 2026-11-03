param(
  [ValidateSet("oop", "procedural")]
  [string]$Backend = "oop",
  [string]$Compiler = "g++",
  [int]$Port = 8080,
  [string]$DatabasePath
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$deps = Join-Path $env:LOCALAPPDATA "CampusStore\deps"
$temporary = Join-Path $env:TEMP "campus-store-web-$([guid]::NewGuid().ToString('N'))"
$json = Join-Path $deps "nlohmann\json.hpp"
$sqliteDir = Join-Path $deps "sqlite-amalgamation-3500400"
$sqliteSource = Join-Path $sqliteDir "sqlite3.c"
$sqliteHeader = Join-Path $sqliteDir "sqlite3.h"
$sqliteArchive = Join-Path $deps "sqlite-amalgamation-3500400.zip"
$sqliteUrl = "https://www.sqlite.org/2025/sqlite-amalgamation-3500400.zip"
$sqliteSha256 = "1D3049DD0F830A025A53105FC79FD2AB9431AEA99E137809D064D8EE8356B032"

function Get-Dependency($Url, $Path, $Sha256) {
  if (!(Test-Path -LiteralPath $Path)) {
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Path) | Out-Null
    & curl.exe -fsSL $Url -o $Path
    if ($LASTEXITCODE -ne 0) { throw "Could not download dependency: $Url" }
  }
  $actual = (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
  if ($actual -ne $Sha256) { throw "Dependency checksum did not match for $Path. Delete the cached file and retry." }
}

$databaseFile = if ($DatabasePath) { [System.IO.Path]::GetFullPath($DatabasePath) } else { Join-Path $root "backend\data\campus_store.db" }
New-Item -ItemType Directory -Force -Path $temporary,(Split-Path -Parent $databaseFile) | Out-Null
try {
  if (!(Get-Command $Compiler -ErrorAction SilentlyContinue)) {
    throw "C++ compiler '$Compiler' was not found. Install MinGW and ensure g++ is on PATH."
  }
  if (!(Get-Command gcc -ErrorAction SilentlyContinue)) {
    throw "gcc was not found. Install MinGW and ensure gcc is on PATH."
  }

  Get-Dependency "https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp" `
    $json "9BEA4C8066EF4A1C206B2BE5A36302F8926F7FDC6087AF5D20B417D0CF103EA6"

  Get-Dependency $sqliteUrl $sqliteArchive $sqliteSha256
  if (!(Test-Path -LiteralPath $sqliteSource) -or !(Test-Path -LiteralPath $sqliteHeader)) {
    Expand-Archive -LiteralPath $sqliteArchive -DestinationPath $deps -Force
  }
  if (!(Test-Path -LiteralPath $sqliteSource) -or !(Test-Path -LiteralPath $sqliteHeader)) {
    throw "SQLite download did not contain the expected amalgamation source and header."
  }

  $sqliteObject = Join-Path $temporary "sqlite3.o"
  & gcc -O2 -DSQLITE_THREADSAFE=1 -DSQLITE_DEFAULT_FOREIGN_KEYS=1 -c $sqliteSource -o $sqliteObject
  if ($LASTEXITCODE -ne 0) { throw "SQLite compilation failed." }

  if ($Backend -eq "oop") {
    $variantSource = Join-Path $root "backend\include\src\main.cpp"
    $variantCode = "OOP"
  } else {
    $variantSource = Join-Path $root "procedural-version\main.cpp"
    $variantCode = "Procedural"
  }
  $output = Join-Path $temporary "campus-store-api.exe"
  $sources = @(
    (Join-Path $root "backend\include\routes\ApiServer.cpp"),
    (Join-Path $root "backend\include\Database\Database.cpp"),
    (Join-Path $root "backend\include\src\store.cpp"),
    (Join-Path $root "procedural-version\store.cpp"),
    $variantSource
  )
  & $Compiler -std=c++14 -O2 -Wall -Wextra `
    "-I$(Join-Path $root 'backend\include')" `
    "-I$deps" "-I$(Split-Path -Parent $json)" "-I$sqliteDir" `
    @sources $sqliteObject -lws2_32 -o $output
  if ($LASTEXITCODE -ne 0) { throw "The $Backend backend did not compile." }

  $env:CSMS_PORT = [string]$Port
  $env:CSMS_API_VARIANT = $variantCode
  $env:CSMS_DB_PATH = $databaseFile
  Write-Output "Opening Campus Store with the $Backend C++ backend at http://127.0.0.1:$Port/"
  Write-Output "Keep this terminal open while using the website. Press Ctrl+C to stop the server."
  & $output $root
  if ($LASTEXITCODE -ne 0) { throw "The website backend stopped with exit code $LASTEXITCODE." }
} finally {
  Remove-Item Env:\CSMS_PORT -ErrorAction SilentlyContinue
  Remove-Item Env:\CSMS_API_VARIANT -ErrorAction SilentlyContinue
  Remove-Item Env:\CSMS_DB_PATH -ErrorAction SilentlyContinue
  if (Test-Path -LiteralPath $temporary) {
    Remove-Item -LiteralPath $temporary -Recurse -Force
  }
}
