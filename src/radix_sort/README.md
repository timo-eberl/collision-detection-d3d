# Direct3D 12 GPU Radix Sort Test

This repository contains a standalone Direct3D 12 implementation of a key-value GPU Radix Sort, adapted from [GPUSorting](https://github.com/b0nes164/GPUSorting) by Thomas Smith.

The original MSVC-only C++ runtime and dynamic compilation were removed to provide a lightweight implementation compatible with MinGW and WSL.

## Credits

**What was adapted from the original repository:**
* **HLSL Shaders:** `DeviceRadixSort.hlsl`, `SortCommon.hlsl`, `SweepCommon.hlsl`, `Utility.hlsl`
* **Tuning Parameters:** The specific hardware magic numbers (keys-per-thread, partition limits, and Wave32 locks) from the original C++ runtime tuning were moved into offline DXC compile steps.

Support for keys-only sort and older hardware was removed. Specifically, we rely on Shader Model 6.6, native 16-bit integer types and native support for Wave Intrinsics.

* Supported hardware: RDNA 1+ (RX 5000), Turing+ (GTX 20).
* Optimized for: AMD RX 9000, Nvidia RTX 5000

## Build and Run

```
cmake -S . -B build/ && cmake --build build/
./build/sort_test.exe
```
