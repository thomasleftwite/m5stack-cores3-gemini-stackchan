$ErrorActionPreference='Stop'
$reviewRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$reviewOut=Join-Path $reviewRoot '.pio/independent-review-20261028'
New-Item -ItemType Directory -Path $reviewOut -Force | Out-Null
$reviewVs=& 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe' -latest -products '*' -property installationPath
$reviewVc=Join-Path $reviewVs 'VC/Auxiliary/Build/vcvars64.bat'
$reviewCommand='"{0}" >nul && cl /nologo /std:c++17 /utf-8 /EHsc /O2 /D_USE_MATH_DEFINES /I"{1}/firmware/tests/reference_stubs" /I"{1}/firmware/lib/SimpleVox/src" /I"{1}/firmware/tests/storage_stubs" /Fe:"{2}/wake_independent_review.exe" /Fo:"{2}\\" "{1}/firmware/tests/wake_independent_review_20261028.cpp" "{1}/firmware/lib/SimpleVox/src/utility/simplevox_mfcc.cpp" "{1}/firmware/lib/SimpleVox/src/utility/simplevox_dtw.cpp" "{1}/firmware/src/ReferenceWake.cpp"' -f $reviewVc,$reviewRoot,$reviewOut
& cmd.exe /d /s /c $reviewCommand
if($LASTEXITCODE -ne 0) { throw 'Independent review compile failed' }
& (Join-Path $reviewOut 'wake_independent_review.exe')
if($LASTEXITCODE -ne 0) { throw 'Independent review failed' }
