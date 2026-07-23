# Collision Detection library using DX12

**Goal**: Compare Performance of ExecuteIndirect and Work Graphs.

## Strategy

Implement a broad phase and narrow phase on GPU. As a start we'll only support spheres and capsules and eventually add more types.

Compare the performance of multiple versions:
- Simple: Broad phase -> Potential pairs list -> Simple narrow phase (uber shader approach)
- Simple Sorted: Broad phase -> Potential pairs list -> Sort list by type -> Simple narrow phase (uber shader approach)
- ExecuteIndirect: Broad phase -> Potential pairs list -> Sort / Bin list by type -> ExecuteIndirect different narrow phase shaders
- Work Graphs: Broad phase -> Launch different narrow phase shaders

## Instructions

The input and expected output (`collision_test_data.bin`) can be generated with [Tics](https://github.com/timo-eberl/tics). Use a demo with only spheres and capsules. For example:
```bash
cmake -S . -B build_release/ -DCMAKE_BUILD_TYPE=Release -DTICS_ENABLE_DEBUG_VIEW=OFF -DTICS_BUILD_TESTS=OFF -DNARROW_BENCHMARK_STEPS=200 -DNARROW_BENCHMARK_PARTICLE_COUNT=100000 && cmake --build build_release/ --config Release && ./build_release/bin/benchmark_narrowphase
```

## To-Do (for a later point)

- [ ] Also do transformations (don't just take pre transformed collision data)
- [ ] Reduce output to 32 byte: `uint32_t a_index; uint32_t b_index; float depth; float point_a[3]; float normal[2];`
- [ ] Add box colliders
- [ ] Implement a proper broad phase
