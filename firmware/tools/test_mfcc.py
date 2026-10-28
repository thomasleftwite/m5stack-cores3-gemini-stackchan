"""Compare device C++ MFCC against independent NumPy equations, synthetic PCM only."""
import json
import pathlib
import subprocess
import numpy as np

ROOT = pathlib.Path(__file__).resolve().parents[1]


def reference(samples):
    x = np.asarray(samples, dtype=np.float64) / 32768
    x -= x.mean()
    x = x - .97 * np.r_[x[0], x[:-1]]
    power = np.abs(np.fft.rfft(x * np.hanning(320), n=512)) ** 2
    edges = 700 * (10 ** (np.linspace(2595*np.log10(1+80/700), 2595*np.log10(1+7600/700), 42) / 2595) - 1)
    hz = np.arange(257) * 31.25
    mel = []
    for left, center, right in zip(edges[:-2], edges[1:-1], edges[2:]):
        weights = np.maximum(0, np.minimum((hz-left)/(center-left), (right-hz)/(right-center)))
        mel.append(np.log(max(1e-12, np.dot(weights, power))))
    dct = np.sqrt(2/40) * np.cos(np.pi * np.arange(1,13)[:, None] * (np.arange(40) + .5) / 40)
    return np.rint(dct @ mel * 8).astype(int)


def main():
    out = ROOT / '.pio/r13-tests'
    out.mkdir(parents=True, exist_ok=True)
    vswhere = pathlib.Path(r'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe')
    vs = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-property', 'installationPath'], text=True).strip()
    vcvars = pathlib.Path(vs) / 'VC/Auxiliary/Build/vcvars64.bat'
    exe = out / 'mfcc_reference.exe'
    command = f'"{vcvars}" >nul && cl /nologo /std:c++17 /utf-8 /EHsc /Fe:"{exe}" /Fo:"{out / "mfcc_reference.obj"}" "{ROOT / "tests/mfcc_reference.cpp"}"'
    runner = out / 'build_mfcc.ps1'
    runner.write_text("$buildCommand='" + command.replace("'", "''") + "'\n& cmd.exe /d /s /c $buildCommand\nexit $LASTEXITCODE\n", encoding='utf-8-sig')
    subprocess.run(['powershell.exe', '-NoProfile', '-File', str(runner)], check=True)
    cases = [json.loads(line) for line in subprocess.check_output([str(exe)], text=True).splitlines()]
    maximum = 0
    for case in cases:
        error = int(np.max(np.abs(reference(case['samples']) - case['mfcc'])))
        maximum = max(maximum, error)
        assert error <= 2, f'MFCC differs from independent FFT/Mel/DCT reference: {error}'
    assert np.max(np.abs(np.array(cases[4]['mfcc']) - cases[5]['mfcc'])) <= 2
    print(f'PASS: six synthetic MFCC cases match NumPy within {maximum} quantization units; level/DC invariance')


if __name__ == '__main__':
    main()
