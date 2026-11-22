param([switch]$TestOnly)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
New-Item -ItemType Directory -Path 'build' -Force | Out-Null
$compiler = Get-Command g++.exe -ErrorAction SilentlyContinue
$compilerPath = if ($compiler) { $compiler.Source } else { $null }
if (-not $compiler -and (Test-Path -LiteralPath 'C:\msys64\ucrt64\bin\g++.exe')) {
    $compilerPath = 'C:\msys64\ucrt64\bin\g++.exe'
}
if (-not $compilerPath) { throw 'g++ not found. Use CMake with Visual Studio 2022 C++ or install MSYS2 UCRT64 GCC.' }
$common = @('-std=c++17','-O2','-Wall','-Wextra','-Wpedantic','-static','-static-libgcc','-static-libstdc++','-Isrc')
& $compilerPath @common 'src/game.cpp' 'src/scores.cpp' 'tests/test_game.cpp' '-o' 'build/bomber_tests.exe'
if ($LASTEXITCODE -ne 0) { throw 'Tests failed to compile.' }
& '.\build\bomber_tests.exe'
if ($LASTEXITCODE -ne 0) { throw 'Rules test failed.' }
if (-not $TestOnly) {
    & $compilerPath @common '-mwindows' 'src/game.cpp' 'src/scores.cpp' 'src/main.cpp' 'src/renderer.cpp' 'src/retro.cpp' 'src/audio.cpp' '-lopengl32' '-lgdi32' '-lwinmm' '-o' 'build/BombermanDemo.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Demo failed to compile. Close the running demo before rebuilding.' }
    Write-Host 'Ready: build\BombermanDemo.exe'
}
