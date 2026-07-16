# Collision Detection library using DX12

Goal: Compare Performance of ExecuteIndirect and Work Graphs.

Strategy: Implement a broad phase and narrow phase on GPU. As a start we'll only support spheres and capsules and eventually add more types. The narrow phase will utilize ExecuteIndirect or Work Graphs for branching for evaluating different shape combinations.

The input and expected output (`collision_test_data.bin`) can be generated with [Tics](https://github.com/timo-eberl/tics). Use a demo with only spheres and capsules. For example:
```bash
cmake -S . -B build_release/ -DCMAKE_BUILD_TYPE=Release -DTICS_ENABLE_DEBUG_VIEW=OFF -DTICS_BUILD_TESTS=OFF -DNARROW_BENCHMARK_STEPS=200 -DNARROW_BENCHMARK_PARTICLE_COUNT=100000 && cmake --build build_release/ --config Release && ./build_release/bin/benchmark_narrowphase
```
