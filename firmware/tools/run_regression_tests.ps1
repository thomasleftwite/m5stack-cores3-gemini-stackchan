param([ValidateSet('all','voice','json','mfcc','speech')][string]$Only='all', [int]$JsonLengthBytes=0, [string]$ReferencePython=$env:STACKCHAN_REFERENCE_PYTHON)
$ErrorActionPreference = 'Stop'
$projectDir = Split-Path -Parent $PSScriptRoot
$outDir = Join-Path $projectDir '.pio/r13-tests'
New-Item -ItemType Directory -Path $outDir -Force | Out-Null
$vsRoot = & 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe' -latest -products '*' -property installationPath
if (-not $vsRoot) { throw 'Visual C++ Build Tools not found' }
$vcvars = Join-Path $vsRoot 'VC/Auxiliary/Build/vcvars64.bat'
if ($Only -in @('all','voice')) {
    $exe = Join-Path $outDir 'voice_gate.exe'
    $source = Join-Path $projectDir 'tests/voice_gate.cpp'
    $wakeSource = Join-Path $projectDir 'src/LocalWake.cpp'
    $stubs = Join-Path $projectDir 'tests/storage_stubs'
    $buildCommand = '"{0}" >nul && cl /nologo /std:c++17 /utf-8 /EHsc /I"{1}" /Fe:"{2}" /Fo:"{3}\\" "{4}" "{5}"' -f $vcvars,$stubs,$exe,$outDir,$source,$wakeSource
    & cmd.exe /d /s /c $buildCommand
    if ($LASTEXITCODE -ne 0) { throw 'Voice regression did not compile' }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw 'Voice regression failed' }
}
if ($Only -in @('all','json')) {
    if (-not $JsonLengthBytes) {
        $ini = Get-Content -LiteralPath (Join-Path $projectDir 'platformio.ini') -Raw
        $setting = [regex]::Match($ini, 'ARDUINOJSON_STRING_LENGTH_SIZE=(\d+)')
        $JsonLengthBytes = if ($setting.Success) { [int]$setting.Groups[1].Value } else { 2 }
    }
    $exe = Join-Path $outDir 'json_audio_limits.exe'
    $source = Join-Path $projectDir 'tests/json_audio_limits.cpp'
    $include = Join-Path $projectDir '.pio/libdeps/m5stack-cores3/ArduinoJson/src'
    $object = Join-Path $outDir 'json_audio_limits.obj'
    $buildCommand = '"{0}" >nul && cl /nologo /std:c++17 /utf-8 /EHsc /I"{1}" /DARDUINOJSON_STRING_LENGTH_SIZE={2} /Fe:"{3}" /Fo:"{4}" "{5}"' -f $vcvars,$include,$JsonLengthBytes,$exe,$object,$source
    & cmd.exe /d /s /c $buildCommand
    if ($LASTEXITCODE -ne 0) { throw 'JSON regression did not compile' }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw 'JSON audio size regression failed' }
}
if ($Only -in @('all','mfcc')) {
    if (-not $ReferencePython) {
        $bundledPython = Join-Path $env:USERPROFILE '.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
        $ReferencePython = if (Test-Path -LiteralPath $bundledPython) { $bundledPython } else { 'python' }
    }
    & $ReferencePython -B (Join-Path $PSScriptRoot 'test_mfcc.py')
    if ($LASTEXITCODE -ne 0) { throw 'Independent MFCC reference verification failed (requires NumPy)' }
}
if ($Only -in @('all','speech')) {
    $vadDir = Join-Path $projectDir 'lib/libfvad'
    $vadOut = Join-Path $outDir 'vad'
    New-Item -ItemType Directory -Path $vadOut -Force | Out-Null
    $cSources = @(Get-ChildItem -LiteralPath (Join-Path $vadDir 'src') -Recurse -Filter '*.c' | Sort-Object FullName)
    $sourceArgs = ($cSources | ForEach-Object { '"' + $_.FullName + '"' }) -join ' '
    $buildCommand = '"{0}" >nul && cl /nologo /std:c11 /O2 /c /Fo:"{1}\\" {2}' -f $vcvars,$vadOut,$sourceArgs
    & cmd.exe /d /s /c $buildCommand
    if ($LASTEXITCODE -ne 0) { throw 'Speech classifier C sources did not compile' }
    $objectArgs = ($cSources | ForEach-Object { '"' + (Join-Path $vadOut ($_.BaseName + '.obj')) + '"' }) -join ' '
    $speechExe = Join-Path $outDir 'speech_gate.exe'
    $speechSource = Join-Path $projectDir 'tests/speech_gate.cpp'
    $speechObject = Join-Path $outDir 'speech_gate.obj'
    $includeDir = Join-Path $vadDir 'include'
    $buildCommand = '"{0}" >nul && cl /nologo /std:c++17 /EHsc /O2 /I"{1}" /Fe:"{2}" /Fo:"{3}" "{4}" {5}' -f $vcvars,$includeDir,$speechExe,$speechObject,$speechSource,$objectArgs
    & cmd.exe /d /s /c $buildCommand
    if ($LASTEXITCODE -ne 0) { throw 'Speech gate tests did not compile' }
    & $speechExe
    if ($LASTEXITCODE -ne 0) { throw 'Speech gate tests failed' }
}
