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

## 43cca3a1ec1c84c677aa53725150b8aec6cba25d (reduce output to 32 bytes)

[dx12] simple (avg over 10) upload=0.721ms [0.720-0.724] aabb_prep=0.044ms [0.043-0.044] broad=53.993ms [52.462-56.315] gap_narrow=0.239ms [0.153-0.665] narrow=0.555ms [0.543-0.575] gap_readback=0.123ms [0.110-0.156] readback=1.360ms [1.254-1.404] total=57.035ms cpu_total=58.536ms [56.875-61.288]

## b97c1b18dc68bfd1e1847d3a26f97b13f5a94cf4 (binning added)

RTX 3050:
[dx12] simple_naive (avg over 10) upload=0.865ms [0.720-2.150] aabb_prep=0.043ms [0.042-0.044] broad=55.889ms [52.476-62.549] gap_narrow=0.207ms [0.144-0.587] narrow=0.545ms [0.520-0.558] gap_readback=0.138ms [0.092-0.389] readback=1.360ms [1.254-1.404] total=59.048ms cpu_total=60.756ms [56.851-67.738]
[dx12] simple_binned (avg over 10) upload=0.862ms [0.720-2.137] aabb_prep=0.044ms [0.043-0.044] broad=54.116ms [52.278-63.841] gap_narrow=0.177ms [0.129-0.289] narrow=0.405ms [0.385-0.437] gap_readback=0.121ms [0.105-0.134] readback=1.360ms [1.254-1.405] total=57.085ms cpu_total=58.494ms [56.195-69.384]

RX 9070 XT:
[dx12] simple_naive (avg over 10) upload=0.687ms [0.683-0.697] aabb_prep=0.023ms [0.017-0.029] broad=31.576ms [30.096-33.348] gap_narrow=0.180ms [0.152-0.232] narrow=0.139ms [0.127-0.148] gap_readback=0.130ms [0.095-0.236] readback=0.819ms [0.695-1.077] total=33.554ms cpu_total=35.802ms [34.949-37.153]
[dx12] simple_binned (avg over 10) upload=0.682ms [0.680-0.686] aabb_prep=0.020ms [0.016-0.028] broad=28.281ms [27.438-29.397] gap_narrow=0.297ms [0.150-1.188] narrow=0.082ms [0.067-0.162] gap_readback=0.217ms [0.090-1.059] readback=0.954ms [0.732-1.366] total=30.533ms cpu_total=32.569ms [31.116-33.165]

## 2044e50d22e0a4ed3f087f442b078bf2d380741d (execute indirect added)

RTX 3050:
[dx12] simple_naive (avg over 10) upload=0.721ms [0.720-0.723] aabb_prep=0.044ms [0.043-0.045] broad=52.973ms [52.070-56.230] gap_narrow=0.225ms [0.148-0.703] narrow=0.546ms [0.519-0.559] gap_readback=0.120ms [0.105-0.146] readback=1.361ms [1.254-1.405] total=55.989ms cpu_total=57.499ms [56.349-61.396]
[dx12] simple_binned (avg over 10) upload=0.720ms [0.720-0.722] aabb_prep=0.044ms [0.043-0.045] broad=52.975ms [52.134-56.175] gap_narrow=0.172ms [0.143-0.246] narrow=0.412ms [0.382-0.496] gap_readback=0.121ms [0.106-0.134] readback=1.360ms [1.254-1.405] total=55.804ms cpu_total=57.353ms [56.073-60.842]
[dx12] execute_indirect (avg over 10) upload=0.721ms [0.720-0.722] aabb_prep=0.044ms [0.043-0.045] broad=52.702ms [52.430-52.930] dispatch_prep=0.006ms [0.005-0.006] narrow=0.428ms [0.398-0.480] gap_readback=0.143ms [0.115-0.174] readback=1.360ms [1.254-1.405] total=55.404ms cpu_total=56.771ms [56.226-57.169]

RX 9070 XT:
[dx12] simple_naive (avg over 10) upload=0.703ms [0.693-0.714] aabb_prep=0.052ms [0.016-0.090] broad=27.001ms [25.954-28.536] gap_narrow=0.144ms [0.130-0.170] narrow=0.179ms [0.120-0.218] gap_readback=0.103ms [0.086-0.136] readback=0.828ms [0.694-1.079] total=29.011ms cpu_total=31.085ms [29.949-32.808]
[dx12] simple_binned (avg over 10) upload=0.685ms [0.683-0.690] aabb_prep=0.021ms [0.014-0.026] broad=23.834ms [23.097-24.783] gap_narrow=0.139ms [0.116-0.152] narrow=0.113ms [0.062-0.152] gap_readback=0.140ms [0.074-0.426] readback=0.808ms [0.693-0.853] total=25.740ms cpu_total=27.537ms [26.528-28.712]
[dx12] execute_indirect (avg over 10) upload=0.684ms [0.682-0.687] aabb_prep=0.022ms [0.014-0.045] broad=23.754ms [22.699-24.551] dispatch_prep=0.013ms [0.011-0.019] narrow=0.132ms [0.061-0.442] gap_readback=0.146ms [0.118-0.218] readback=0.824ms [0.694-1.043] total=25.576ms cpu_total=27.575ms [26.295-29.139]
