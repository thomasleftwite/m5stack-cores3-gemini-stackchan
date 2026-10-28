# SimpleVox detection subset

Source: https://github.com/thomasleftwite/stackchan-chatgpt/tree/fcdd3b214e29f2efdcf31c12a691e7c9552b6e31/lib/SimpleVox

Upstream author: MechaUma, MIT, version 0.0.1. Only MFCC, feature storage interface and cosine DTW are included. UI, network, device configuration, capture controller and SimpleVox VAD are excluded. ESP-DSP is supplied by the existing pinned ESP32 Arduino framework; no M5Unified/framework upgrade is required. Original notices are retained. Any local hardening is documented in the project comparison record.

Local hardening: clamp Mel energy to 1e-12 before log10 so exact silence/zero padding cannot create NaN/Inf. The finite-silence regression failed on the original source and passes with this one-line guard. Positive-energy calculations, global mean/stddev normalization, frame/hop configuration and cosine DTW are retained.

S03 diagnostic only: a runtime selector can compare the existing ESP-DSP dispatch with its ANSI FFT/direct bit-reversal/real-postprocessing implementations. The default and the production detector retain the reference dispatch. No SDK/framework upgrade is made. The host FFT stand-in now models ESP-DSP's packed Nyquist value in data[1].

S04: reserve three extra floats and align the FFT workspace to a 16-byte boundary while retaining the owning allocation for correct release. Native S03 identical-input diagnostics differed at offset 12 (optimized other=333, ANSI=392), while offset 8 produced 392 in both. This prevents heap layout from selecting an unsupported workspace alignment; no MFCC/DTW threshold or capture change.
