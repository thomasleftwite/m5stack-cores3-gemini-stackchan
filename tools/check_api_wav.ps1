param([Parameter(Mandatory=$true)][string]$WaveFile)
$ErrorActionPreference = 'Stop'
$projectDir = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $projectDir '.pio/r13-tests'
New-Item -ItemType Directory -Path $outDir -Force | Out-Null
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vsRoot = & $vswhere -latest -products '*' -property installationPath
if (-not $vsRoot) { throw 'Visual C++ Build Tools not found' }
$vcvars = Join-Path $vsRoot 'VC/Auxiliary/Build/vcvars64.bat'
$source = Join-Path $PSScriptRoot 'check_api_wav.cpp'
$exe = Join-Path $outDir 'check_api_wav.exe'
$obj = Join-Path $outDir 'check_api_wav.obj'
$buildCommand = '"{0}" >nul && cl /nologo /std:c++17 /utf-8 /EHsc /W4 /Fe:"{1}" /Fo:"{2}" "{3}"' -f $vcvars,$exe,$obj,$source
& cmd.exe /d /s /c $buildCommand
if ($LASTEXITCODE -ne 0) { throw 'WAV checker did not compile' }
& $exe (Resolve-Path -LiteralPath $WaveFile).Path
if ($LASTEXITCODE -ne 0) { throw 'WAV rejected by firmware validation rules' }
