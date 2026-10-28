$ErrorActionPreference='Stop'
$projectDir=Split-Path -Parent $PSScriptRoot
$outDir=Join-Path $projectDir '.pio/reference-tests'
New-Item -ItemType Directory -Path $outDir -Force | Out-Null
$vsRoot=& 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe' -latest -products '*' -property installationPath
$vcvars=Join-Path $vsRoot 'VC/Auxiliary/Build/vcvars64.bat'
$stubs=Join-Path $projectDir 'tests/reference_stubs'
$include=Join-Path $projectDir 'lib/SimpleVox/src'
$mfcc=Join-Path $include 'utility/simplevox_mfcc.cpp'
$dtw=Join-Path $include 'utility/simplevox_dtw.cpp'
$test=Join-Path $projectDir 'tests/simplevox_reference.cpp'
$storageStubs=Join-Path $projectDir 'tests/storage_stubs'
$wake=Join-Path $projectDir 'src/ReferenceWake.cpp'
$exe=Join-Path $outDir 'simplevox_reference.exe'
$buildCommand='"{0}" >nul && cl /nologo /std:c++17 /utf-8 /EHsc /O2 /D_USE_MATH_DEFINES /I"{1}" /I"{2}" /I"{8}" /Fe:"{3}" /Fo:"{4}\\" "{5}" "{6}" "{7}" "{9}"' -f $vcvars,$stubs,$include,$exe,$outDir,$test,$mfcc,$dtw,$storageStubs,$wake
& cmd.exe /d /s /c $buildCommand
if($LASTEXITCODE -ne 0) { throw 'Reference tests did not compile' }
& $exe
if($LASTEXITCODE -ne 0) { throw 'Reference tests failed' }
