param(
  [string]$Compiler = "g++",
  [switch]$TestsOnly
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path ([System.IO.Path]::GetTempPath()) "campus-store-case-study-$([guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

$targets = @()
try {
  $targets = @(
    @{
      Name = "procedural-tests"
      Sources = @("procedural-version\store.cpp", "tests\product-test.cpp")
      IncludePath = Join-Path $root "procedural-version"
    },
    @{
      Name = "oop-tests"
      Sources = @("backend\include\src\store.cpp", "tests\billing-test.cpp")
      IncludePath = Join-Path $root "backend\include"
    }
  )

  if (!$TestsOnly) {
    $targets += @(
      @{
        Name = "procedural-demo"
        Sources = @("procedural-version\store.cpp", "procedural-version\case-study-main.cpp")
        IncludePath = Join-Path $root "procedural-version"
      },
      @{
        Name = "oop-demo"
        Sources = @("backend\include\src\Store.cpp", "backend\include\src\case-study-main.cpp")
        IncludePath = Join-Path $root "backend\include"
      }
    )
  }

  foreach ($target in $targets) {
    $output = Join-Path $buildDir "$($target.Name).exe"
    $sources = @($target.Sources | ForEach-Object { Join-Path $root $_ })
    & $Compiler -std=c++14 -Wall -Wextra -Wpedantic "-I$($target.IncludePath)" @sources "-o" $output
    if ($LASTEXITCODE -ne 0) {
      throw "Compilation failed for $($target.Name) with exit code $LASTEXITCODE."
    }
  }

  foreach ($target in $targets) {
    if ($target.Name -notlike "*-tests") { continue }
    & (Join-Path $buildDir "$($target.Name).exe")
    if ($LASTEXITCODE -ne 0) {
      throw "Acceptance tests failed for $($target.Name) with exit code $LASTEXITCODE."
    }
  }

  if (!$TestsOnly) {
    foreach ($target in $targets) {
      if ($target.Name -notlike "*-demo") { continue }
      Write-Output "`n--- $($target.Name) ---"
      & (Join-Path $buildDir "$($target.Name).exe")
      if ($LASTEXITCODE -ne 0) {
        throw "Demo failed for $($target.Name) with exit code $LASTEXITCODE."
      }
    }
  }
} finally {
  if (Test-Path -LiteralPath $buildDir) {
    Remove-Item -LiteralPath $buildDir -Recurse -Force
  }
}
