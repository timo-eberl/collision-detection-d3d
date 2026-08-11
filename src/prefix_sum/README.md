# Direct3D 12 GPU Prefix Sums

This repository contains a standalone Direct3D 12 implementation of a optimized GPU Prefix Sums (exclusive sum and inclusive max), adapted from [GPUPrefixSums](https://github.com/b0nes164/GPUPrefixSums) by Thomas Smith.

The original MSVC-only C++ runtime, dynamic compilation, and survey/benchmarking utilities were removed to provide a lightweight implementation compatible with MinGW and WSL.

## Credits & Adaptations

**What was adapted from the original repository:**
* **HLSL Shaders:** `ScanCommon.hlsl` and `ChainedScanDecoupledLookback.hlsl` were merged into a single, simplified `prefix_sum.hlsl` file.
* The slower "Reduce-Then-Scan" (RTS) fallback algorithm was removed.
* Constant buffers were replaced with direct Root 32-bit Constants (Push Constants).

**Important Technical Notes:**
* **Inclusive Max Emulation:** The codebase was modified to support an Inclusive Max operator. Because standard Shader Model 6.6 lacks a hardware `WavePrefixMax` intrinsic, it is emulated in software via an unrolled `WaveReadLaneAt` loop. This operator will have slightly lower throughput than standard addition.
* **CSDL Deadlock Risk:** The single-pass CSDL algorithm relies on thread-group scheduling behaviors to avoid hanging. This is safe on current desktop GPUs, but it is **not spec compliant** and might deadlock on architectures that lack forward progress guarantees.

* Supported hardware: RDNA 1+ (RX 5000), Turing+ (GTX 20).
* Relies on Shader Model 6.6, Wave Intrinsics, and forward-progress scheduling guarantees.

## Build and Run

```
cmake -S . -B build/ && cmake --build build/
./build/prefix_sum_test.exe
```
