# Collision Detection library using DX12

**Goal**: Compare Performance of ExecuteIndirect and Work Graphs.

## Strategy

Implement a broad phase and narrow phase on GPU. We'll support spheres, capsules and oriented bounding boxes (OBBs). This gives us 6 different combinations (sphere-sphere, sphere-capsule, sphere-box, capsule-capsule, capsule-box, box-box). Their narrow phase implementation differ significantly in complexity, which creates a heterogeneous workload that benefits from dynamic work distribution.

Compare the performance of multiple versions:
- Simple Naive:    Broad phase -> Potential pairs list -> Narrow phase (uber shader)
- ExecuteIndirect: Broad phase -> Potential binned pairs list -> ExecuteIndirect Narrow phase (~~individual shaders~~ uber shader)
- Work Graphs:     Broad phase -> Work Graphs Narrow phase (individual shaders)

## Instructions

The input and expected output (`collision_test_data.bin`) can be generated with [Tics](https://github.com/timo-eberl/tics). Use a demo with only the supported shapes. For example:
```bash
cmake -S . -B build_release/ -DCMAKE_BUILD_TYPE=Release -DTICS_ENABLE_DEBUG_VIEW=OFF -DTICS_BUILD_TESTS=OFF -DNARROW_BENCHMARK_STEPS=200 -DNARROW_BENCHMARK_PARTICLE_COUNT=300000 && cmake --build build_release/ --config Release && ./build_release/bin/benchmark_narrowphase
```

Build and run this project in RelWithDebInfo Configuration:
```bash
cmake -S . -B build/relwithdebinfo/ -DCDDX_ENABLE_PROFILER=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build/relwithdebinfo/ --config RelWithDebInfo && build/relwithdebinfo/collision_dx_app.exe
```

Alternatively, you can use scripts in the repositories to run the benchmarks that are used in the paper. After cloning tics and this repository:

```sh
cd ~/projects/tics/
git checkout 2d348fdbc063158c09a752598e01590516355489 # later commits might not work
./demos/benchmark_narrowphase/prepare_benchmarks.sh
mv -a ~/projects/tics/bench_narrow_data/ ~/projects/collision_detection_d3d/bench_narrow_data/
cd ~/projects/collision_detection_d3d/
./run_benchmark.sh
```

## Attribution

Third-party code and the respective licenses can be found in the following directories:
- `src/prefix_sum`
- `src/radix_sort`
