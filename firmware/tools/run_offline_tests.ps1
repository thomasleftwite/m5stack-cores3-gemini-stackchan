$ErrorActionPreference = 'Stop'
$projectDir = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $projectDir '.pio/r13-tests'
New-Item -ItemType Directory -Path $outDir -Force | Out-Null
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vsRoot = & $vswhere -latest -products '*' -property installationPath
if (-not $vsRoot) { throw 'Visual C++ Build Tools not found' }
$vcvars = Join-Path $vsRoot 'VC/Auxiliary/Build/vcvars64.bat'
$source = Join-Path $projectDir 'tests/offline.cpp'
$exe = Join-Path $outDir 'offline.exe'
$obj = Join-Path $outDir 'offline.obj'
$buildCommand = '"{0}" >nul && cl /nologo /std:c++17 /utf-8 /EHsc /W4 /Fe:"{1}" /Fo:"{2}" "{3}"' -f $vcvars,$exe,$obj,$source
& cmd.exe /d /s /c $buildCommand
if ($LASTEXITCODE -ne 0) { throw 'C++ tests did not compile' }
& $exe
if ($LASTEXITCODE -ne 0) { throw 'C++ offline test failed' }
$storageSource = Join-Path $projectDir 'tests/wake_storage.cpp'
$wakeSource = Join-Path $projectDir 'src/LocalWake.cpp'
$stubs = Join-Path $projectDir 'tests/storage_stubs'
$storageExe = Join-Path $outDir 'wake_storage.exe'
$buildCommand = '"{0}" >nul && cl /nologo /std:c++17 /utf-8 /EHsc /W4 /I"{1}" /Fe:"{2}" /Fo:"{3}\\" "{4}" "{5}"' -f $vcvars,$stubs,$storageExe,$outDir,$storageSource,$wakeSource
& cmd.exe /d /s /c $buildCommand
if ($LASTEXITCODE -ne 0) { throw 'Storage tests did not compile' }
& $storageExe
if ($LASTEXITCODE -ne 0) { throw 'Storage tests failed' }
$retrySource = Join-Path $projectDir 'tests/reply_retry.cpp'
$retryExe = Join-Path $outDir 'reply_retry.exe'
$retryObj = Join-Path $outDir 'reply_retry.obj'
$buildCommand = '"{0}" >nul && cl /nologo /std:c++17 /utf-8 /EHsc /W4 /Fe:"{1}" /Fo:"{2}" "{3}"' -f $vcvars,$retryExe,$retryObj,$retrySource
& cmd.exe /d /s /c $buildCommand
if ($LASTEXITCODE -ne 0) { throw 'Reply retry tests did not compile' }
& $retryExe
if ($LASTEXITCODE -ne 0) { throw 'Reply retry tests failed' }
