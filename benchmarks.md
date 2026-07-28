Benchmarks use 100.000 bodies simulated over 200 frames on an AMD Radeon RX 9070 XT.

## f9d5b82bb37db6090fa083596382dcc3875d7714 (Simple Single Pass)

[dx12] simple (avg over 200) upload=0.467ms [0.455-0.747] kernel=25.873ms [21.087-36.166] cmd_gap=0.191ms [0.103-6.954] readback=0.798ms [0.012-2.918] total=27.330ms cpu_total=31.428ms [23.783-344.861]

## a888fa8e5a5c2f41f4b7ee3876af8d5381ec04f0 (Simple Two Pass)

[dx12] simple (avg over 200) upload=0.465ms [0.458-0.480] broad=25.169ms [22.425-31.953] gap_narrow=0.153ms [0.117-0.387] narrow=0.037ms [0.011-0.081] gap_readback=0.422ms [0.082-62.944] readback=0.794ms [0.012-3.007] total=27.040ms cpu_total=30.524ms [24.688-309.018]

> Previous benchmarks use capsules and spheres
> Following benchmarks use capsules, spheres and boxes

## 9154af61f1a64517141e8a05f0e043279c5e4afc (Simple)

[dx12] simple (avg over 200) upload=0.732ms [0.679-1.667] aabb_gitprep=0.068ms [0.013-1.920] broad=30.390ms [22.580-37.170] gap_narrow=0.211ms [0.141-1.217] narrow=0.072ms [0.014-0.199] gap_readback=0.150ms [0.090-1.023] readback=0.795ms [0.010-3.297] total=32.417ms cpu_total=34.272ms [26.346-43.972]

> Previous benchmarks use RX 9070 XT
> Following benchmarks use RTX 3050

> Previous benchmarks measure 200 frames
> Following benchmarks measure Frames 160.169 (where collision count is high)

## ee1516065114da65ba1ed88e9f79ee03420583a7

[dx12] simple (avg over 10) upload=0.721ms [0.721-0.724] aabb_prep=0.046ms [0.043-0.053] broad=56.340ms [56.169-56.547] gap_narrow=0.273ms [0.160-0.732] narrow=0.585ms [0.529-0.627] gap_readback=0.122ms [0.110-0.147] readback=3.377ms [2.987-3.612] total=61.464ms cpu_total=65.391ms [64.384-66.663]

## Copy Queue experiment

[dx12] simple (avg over 10) aabb_prep=0.044ms [0.043-0.045] broad=56.461ms [56.240-56.607] cpu_sync=0.244ms [0.176-0.591] narrow=0.584ms [0.530-0.618] total=57.333ms cpu_total=69.713ms [66.804-72.819]

> Following benchmarks have changes to the demo scene: non-uniform object sizes, bigger container

## ee1516065114da65ba1ed88e9f79ee03420583a7

[dx12] simple (avg over 10) upload=0.721ms [0.720-0.723] aabb_prep=0.044ms [0.043-0.045] broad=54.342ms [52.311-56.684] gap_narrow=0.263ms [0.150-0.768] narrow=0.558ms [0.545-0.583] gap_readback=0.131ms [0.109-0.157] readback=2.714ms [2.501-2.802] total=58.772ms cpu_total=61.447ms [59.103-63.957]
