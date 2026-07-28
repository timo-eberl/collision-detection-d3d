# Collision Detection library using DX12

**Goal**: Compare Performance of ExecuteIndirect and Work Graphs.

## Strategy

Implement a broad phase and narrow phase on GPU. We'll support spheres, capsules and oriented bounding boxes (OBBs). This gives us 6 different combinations (sphere-sphere, sphere-capsule, sphere-box, capsule-capsule, capsule-box, box-box). Their narrow phase implementation differ significantly in complexity, which creates a heterogeneous workload that benefits from dynamic work distribution.

Compare the performance of multiple versions:
- Simple Naive:    Broad phase -> Potential pairs list -> Narrow phase (uber shader)
- Simple Binned:   Broad phase -> Potential binned pairs list -> Dispatch Narrow Phase (same uber shader) multiple times
- ExecuteIndirect: Broad phase -> Potential binned pairs list -> ExecuteIndirect different narrow phase shaders
- Work Graphs:     Broad phase -> Launch different narrow phase shaders

## Instructions

The input and expected output (`collision_test_data.bin`) can be generated with [Tics](https://github.com/timo-eberl/tics). Use a demo with only the supported shapes. For example:
```bash
cmake -S . -B build_release/ -DCMAKE_BUILD_TYPE=Release -DTICS_ENABLE_DEBUG_VIEW=OFF -DTICS_BUILD_TESTS=OFF -DNARROW_BENCHMARK_STEPS=200 -DNARROW_BENCHMARK_PARTICLE_COUNT=100000 && cmake --build build_release/ --config Release && ./build_release/bin/benchmark_narrowphase
```

## To-Do (for a later point)

- [x] Also do transformations (don't just take pre transformed collision data)
- [x] Add box colliders
- [ ] Use groupshared memory for shape data
- [x] Reduce output to 32 byte: `uint32_t a_index; uint32_t b_index; float depth; float point_a[3]; float normal[2];`
- [ ] Implement a proper broad phase
