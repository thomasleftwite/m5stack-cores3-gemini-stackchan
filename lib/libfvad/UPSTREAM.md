# libfvad source pin

Source: https://github.com/dpirch/libfvad

Commit: 99fd86bbb8093a8d0433db3da1d8959c1682adcc (v1.0). Archive SHA256: 485ea8906a0f5a1c42d944299f9b715a9b70887629b60b7554e44ab82b033ef7

Vendored C sources and license files only. No audio fixtures or generated binaries.

Project additions: `include/speech_vad_bridge.h`, `src/speech_vad_bridge.c`, and `library.json`. The adapter uses the pinned 16 kHz core in mode 3 and distinguishes active speech from hangover. Original upstream C/H sources are unchanged. The PlatformIO manifest selects C11 and wrapping signed arithmetic.
