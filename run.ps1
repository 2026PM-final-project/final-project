param([switch]$NoLaunch)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
$sourceTime = (Get-ChildItem -LiteralPath 'src' -Recurse -File | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1).LastWriteTimeUtc
$candidates = @('build/BombermanDemo.exe','build-vs/Release/BombermanDemo.exe','build-vs/Debug/BombermanDemo.exe')
function Get-CurrentExecutable {
    @($candidates | ForEach-Object { if (Test-Path -LiteralPath $_) { Get-Item -LiteralPath $_ } } |
      Where-Object { $_.LastWriteTimeUtc -ge $sourceTime } | Sort-Object LastWriteTimeUtc -Descending) | Select-Object -First 1
}
$executable = Get-CurrentExecutable
if (-not $executable) {
    if ((Get-Command g++.exe -ErrorAction SilentlyContinue) -or (Test-Path -LiteralPath 'C:/msys64/ucrt64/bin/g++.exe')) {
        & (Join-Path $PSScriptRoot 'build.ps1')
        if ($LASTEXITCODE -ne 0) { throw 'GCC build failed. Close any running game before rebuilding.' }
    } else {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio C++ tools or MSYS2 GCC, then build BombermanDemo.' }
        $msbuild = @(& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild/**/Bin/MSBuild.exe') | Select-Object -First 1
        if (-not $msbuild) { throw 'MSBuild not found. Install Visual Studio C++ tools.' }
        & $msbuild (Join-Path $PSScriptRoot 'Bomberman.sln') /t:Build /p:Configuration=Release /p:Platform=x64 /nologo /v:minimal
        if ($LASTEXITCODE -ne 0) { throw 'Visual Studio build failed. Close any running game before rebuilding.' }
    }
    $executable = Get-CurrentExecutable
}
if (-not $executable) { throw 'No executable matches the current sources. Build BombermanDemo first.' }
Write-Output ('Ready: ' + $executable.FullName)
if (-not $NoLaunch) { Start-Process -FilePath $executable.FullName -WorkingDirectory $executable.DirectoryName }
