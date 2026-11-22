# Native Windows/Linux PCM rate conversion baseline

Original allocation-free C++17 stereo-float linear resampler in `src/native_audio_resample.{hpp,cpp}` and deterministic cross-platform `fixtures/native_audio_resample_tests.cpp`.

## Purpose and limits

A game stream and a real audio endpoint (WASAPI mix format or Linux audio backend) can run at different sample rates. This component provides a **bounded reference fallback** for converting sample positions; it is not a device render loop, spatial engine, high-quality production resampler, XAudio2 substitute, audio codec, game stream or retail-game support.

The absolute, rational phase numerator in `ResampleCursor` prevents segment-boundary drift from repeated floating-point position accumulation. Calls may supply consecutive, overlapping decoded PCM input windows with their absolute first-frame offset. Supported rates are 8,000–384,000 Hz. At most 4,096 output frames and 65,536 input frames are accepted per call. Source/target rates are fixed until the owner intentionally resets the cursor.

The function preflights the complete required input range, including a next-frame interpolation neighbor only for fractional phase, **before modifying the output or cursor**. Insufficient input and invalid parameters fail without partial writes. A non-finite sample is replaced with silence per channel. The source and destination must be disjoint allocations, and the caller must ensure their passed pointer spans are valid.

## Evidence and next gates

A strict local Linux compiler/test run passed four scenarios: 48→96 kHz interpolation split across source windows versus one-shot oracle, 96→48 kHz decimation, atomic NeedMoreInput and exact-integer end-of-stream behavior, NaN/Inf sanitation and invalid rate/overflow rejection. Windows/MSVC and integrated Linux CTests are the next authority via GitHub CI.

**Linear interpolation can alias when reducing sample rates and is not suitable as the final premium audio path.** Compare production-grade SDL3 `SDL_AudioStream`, platform WASAPI/IAudioClient3, PipeWire and licensed high-quality polyphase filters during audio renderer integration. Do not claim imperceptible latency or sound quality without real output devices, loopback tests, source streams and objective measurements. No proprietary assets or third-party code added; no scheduled tasks or monitors created.
