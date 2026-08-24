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

## 8b759a737001f2df473ff5eb80c2bab2a5ef6aeb (work graph added)

RX 9070 XT:
[dx12] simple_naive (avg over 10) upload=0.727ms [0.714-0.769] aabb_prep=0.036ms [0.030-0.057] broad=28.250ms [26.352-32.588] gap_narrow=0.217ms [0.161-0.518] narrow=0.156ms [0.121-0.169] gap_readback=0.131ms [0.091-0.161] readback=0.800ms [0.693-0.838] total=30.319ms cpu_total=32.467ms [30.393-36.974]
[dx12] simple_binned (avg over 10) upload=0.720ms [0.709-0.790] aabb_prep=0.030ms [0.015-0.035] broad=24.090ms [22.622-26.694] gap_narrow=0.164ms [0.138-0.196] narrow=0.082ms [0.066-0.085] gap_readback=0.125ms [0.111-0.162] readback=0.843ms [0.692-1.232] total=26.055ms cpu_total=27.796ms [26.319-30.533]
[dx12] execute_indirect (avg over 10) upload=0.708ms [0.690-0.710] aabb_prep=0.029ms [0.017-0.031] broad=24.163ms [23.068-26.765] dispatch_prep=0.023ms [0.012-0.025] narrow=0.087ms [0.068-0.099] gap_readback=0.167ms [0.133-0.191] readback=0.798ms [0.694-0.832] total=25.975ms cpu_total=27.846ms [26.680-30.209]
[dx12] work_graphs (avg over 10) upload=0.709ms [0.693-0.713] aabb_prep=0.031ms [0.017-0.039] init_graph=17.443ms [16.117-19.657] work_graph=106.963ms [105.921-109.590] gap_readback=0.197ms [0.162-0.259] readback=0.846ms [0.711-1.071] total=126.188ms cpu_total=128.001ms [126.046-131.195]

## 2dfad8cc909ab8d993c4fee15332350bcde889d3 (work graphs improved + Shader Model 6.8)

[dx12] simple_naive (avg over 10) upload=0.799ms [0.703-1.365] aabb_prep=0.069ms [0.020-0.309] broad=22.184ms [19.268-27.625] gap_narrow=0.196ms [0.149-0.232] narrow=0.159ms [0.120-0.177] gap_readback=0.135ms [0.089-0.170] readback=0.797ms [0.685-0.839] total=24.340ms cpu_total=26.507ms [23.374-31.694]
[dx12] simple_binned (avg over 10) upload=0.714ms [0.690-0.718] aabb_prep=0.035ms [0.017-0.042] broad=19.439ms [18.442-21.436] gap_narrow=0.159ms [0.136-0.176] narrow=0.084ms [0.068-0.095] gap_readback=0.130ms [0.088-0.153] readback=0.792ms [0.684-0.829] total=21.352ms cpu_total=23.165ms [22.074-25.218]
[dx12] execute_indirect (avg over 10) upload=0.710ms [0.693-0.713] aabb_prep=0.031ms [0.017-0.038] broad=18.481ms [17.903-19.097] dispatch_prep=0.023ms [0.013-0.025] narrow=0.086ms [0.069-0.090] gap_readback=0.172ms [0.136-0.193] readback=0.794ms [0.687-0.833] total=20.296ms cpu_total=22.781ms [21.670-28.316]
[dx12] work_graphs (avg over 10) upload=0.711ms [0.691-0.716] aabb_prep=0.030ms [0.017-0.036] broad_phase=17.953ms [17.581-18.869] work_graph=0.377ms [0.347-0.396] gap_readback=0.181ms [0.152-0.284] readback=0.795ms [0.697-0.833] total=20.047ms cpu_total=21.864ms [21.170-23.153]

## 1166f95becb685749c65dc97dc7e359a45b8ab6d (simple_naive with grid a)

RTX 3050:
[dx12] simple_naive_grid_a (avg over 10) upload=0.722ms [0.721-0.723] aabb_prep=0.044ms [0.043-0.047] build=1.094ms [0.548-3.270] query=15.928ms [13.443-21.813] gap_narrow=0.146ms [0.120-0.212] narrow=0.537ms [0.512-0.548] gap_readback=0.112ms [0.100-0.142] readback=1.360ms [1.254-1.405] total=19.943ms cpu_total=23.603ms [18.356-43.708]
[dx12] simple_binned (avg over 10) upload=0.721ms [0.720-0.722] aabb_prep=0.044ms [0.043-0.051] broad=64.460ms [62.668-74.433] gap_narrow=0.179ms [0.152-0.268] narrow=0.402ms [0.387-0.422] gap_readback=0.133ms [0.114-0.141] readback=1.361ms [1.254-1.405] total=67.298ms cpu_total=68.660ms [66.724-78.680]
[dx12] execute_indirect (avg over 10) upload=0.721ms [0.720-0.722] aabb_prep=0.044ms [0.043-0.045] broad=65.002ms [62.660-79.997] dispatch_prep=0.006ms [0.005-0.006] narrow=0.428ms [0.402-0.461] gap_readback=0.151ms [0.126-0.185] readback=1.361ms [1.254-1.405] total=67.713ms cpu_total=69.075ms [66.667-84.075]
[dx12] work_graphs (avg over 10) upload=0.721ms [0.720-0.723] aabb_prep=0.045ms [0.043-0.052] broad_phase=63.695ms [62.508-69.186] work_graph=1.736ms [1.668-1.770] gap_readback=0.162ms [0.147-0.194] readback=1.360ms [1.253-1.404] total=67.719ms cpu_total=69.218ms [67.695-74.568]

RX 9070 XT:
[dx12] simple_naive_grid_a (avg over 10) upload=0.739ms [0.705-0.776] aabb_prep=0.047ms [0.021-0.102] build=1.264ms [0.490-5.186] query=14.784ms [9.951-27.632] gap_narrow=0.171ms [0.132-0.217] narrow=0.176ms [0.138-0.321] gap_readback=0.136ms [0.096-0.198] readback=0.895ms [0.703-1.249] total=18.213ms cpu_total=20.662ms [14.569-33.228]
[dx12] simple_binned (avg over 10) upload=0.778ms [0.693-1.115] aabb_prep=0.036ms [0.018-0.048] broad=23.573ms [19.196-31.640] gap_narrow=0.200ms [0.142-0.376] narrow=0.118ms [0.067-0.258] gap_readback=0.273ms [0.092-1.580] readback=0.944ms [0.703-1.438] total=25.922ms cpu_total=27.868ms [22.575-39.211]
[dx12] execute_indirect (avg over 10) upload=0.716ms [0.694-0.723] aabb_prep=0.033ms [0.016-0.050] broad=21.739ms [18.043-33.894] dispatch_prep=0.024ms [0.012-0.032] narrow=0.102ms [0.068-0.240] gap_readback=0.178ms [0.148-0.256] readback=0.862ms [0.701-1.276] total=23.653ms cpu_total=25.590ms [21.665-38.250]
[dx12] work_graphs (avg over 10) upload=0.831ms [0.691-1.588] aabb_prep=0.032ms [0.016-0.046] broad_phase=21.163ms [17.942-31.327] work_graph=0.586ms [0.461-1.375] gap_readback=0.207ms [0.147-0.503] readback=0.925ms [0.716-1.152] total=23.746ms cpu_total=25.812ms [22.020-38.523]

RTX 5070 TI:
[dx12] simple_naive_grid_a (avg over 10) upload=0.013ms [0.012-0.018] aabb_prep=0.013ms [0.013-0.015] build=0.634ms [0.320-1.838] query=8.139ms [6.993-10.839] gap_narrow=0.185ms [0.100-0.425] narrow=0.121ms [0.115-0.124] gap_readback=0.094ms [0.070-0.166] readback=0.186ms [0.164-0.232] total=9.384ms cpu_total=11.823ms [9.445-21.497]
[dx12] simple_binned (avg over 10) upload=0.013ms [0.011-0.017] aabb_prep=0.013ms [0.012-0.013] broad=14.923ms [14.913-14.935] gap_narrow=0.166ms [0.112-0.292] narrow=0.085ms [0.079-0.088] gap_readback=0.107ms [0.076-0.154] readback=0.176ms [0.162-0.183] total=15.482ms cpu_total=16.743ms [16.548-17.063]
[dx12] execute_indirect (avg over 10) upload=0.014ms [0.012-0.019] aabb_prep=0.013ms [0.012-0.013] broad=14.885ms [14.862-14.925] dispatch_prep=0.007ms [0.006-0.008] narrow=0.103ms [0.098-0.107] gap_readback=0.136ms [0.103-0.162] readback=0.177ms [0.161-0.187] total=15.334ms cpu_total=16.550ms [16.224-16.730]
[dx12] work_graphs (avg over 10) upload=0.014ms [0.012-0.016] aabb_prep=0.013ms [0.012-0.013] broad_phase=14.823ms [14.796-14.908] work_graph=0.430ms [0.407-0.446] gap_readback=0.130ms [0.101-0.197] readback=0.175ms [0.163-0.181] total=15.585ms cpu_total=16.801ms [16.550-16.936]

## 4f7a3d5ecff30b0144cbcd271a11ebf842325096 (grid a query optimized)

RX 9070 XT:
[dx12] simple_naive_grid_a (avg over 10) upload=0.750ms [0.703-0.819] aabb_prep=0.057ms [0.021-0.107] build=1.094ms [0.375-3.035] query=0.383ms [0.344-0.615] gap_narrow=0.172ms [0.090-0.423] narrow=0.172ms [0.141-0.243] gap_readback=0.148ms [0.100-0.332] readback=0.908ms [0.694-1.170] total=3.684ms cpu_total=6.161ms [4.808-12.302]
[dx12] simple_binned (avg over 10) upload=0.717ms [0.690-0.725] aabb_prep=0.033ms [0.018-0.042] broad=21.891ms [20.431-24.145] gap_narrow=0.178ms [0.139-0.286] narrow=0.092ms [0.066-0.105] gap_readback=0.131ms [0.106-0.170] readback=0.812ms [0.692-0.858] total=23.854ms cpu_total=25.646ms [23.785-28.250]
[dx12] execute_indirect (avg over 10) upload=0.720ms [0.694-0.736] aabb_prep=0.039ms [0.016-0.106] broad=19.296ms [18.538-22.411] dispatch_prep=0.024ms [0.013-0.028] narrow=0.085ms [0.068-0.095] gap_readback=0.183ms [0.144-0.291] readback=0.809ms [0.690-0.855] total=21.156ms cpu_total=22.974ms [22.230-26.145]
[dx12] work_graphs (avg over 10) upload=0.713ms [0.692-0.719] aabb_prep=0.031ms [0.017-0.039] broad_phase=18.212ms [17.708-19.015] work_graph=0.501ms [0.454-0.525] gap_readback=0.168ms [0.149-0.180] readback=0.869ms [0.703-1.031] total=20.492ms cpu_total=22.992ms [21.536-28.692]

## 75d5a103ff5ba7a00cbbb83d0888891af4fb71a5 (added grid broad phase everywhere)

RX 9070 XT:
[dx12] simple_naive (avg over 10) upload=0.817ms [0.702-0.862] aabb_prep=0.111ms [0.020-0.138] build=0.992ms [0.606-2.854] query=0.619ms [0.342-1.071] gap_narrow=0.194ms [0.098-0.575] narrow=0.327ms [0.140-0.572] gap_readback=0.140ms [0.081-0.215] readback=0.880ms [0.701-1.236] total=4.081ms cpu_total=9.733ms [8.660-12.444]
[dx12] simple_binned (avg over 10) upload=0.718ms [0.691-0.724] aabb_prep=0.032ms [0.017-0.036] build=0.607ms [0.346-2.777] query=0.429ms [0.410-0.440] gap_narrow=0.125ms [0.110-0.164] narrow=0.091ms [0.074-0.095] gap_readback=0.119ms [0.079-0.153] readback=0.807ms [0.705-0.842] total=2.929ms cpu_total=5.141ms [4.398-9.565]
[dx12] execute_indirect (avg over 10) upload=0.719ms [0.691-0.724] aabb_prep=0.032ms [0.017-0.036] build=0.635ms [0.346-2.966] query=0.426ms [0.409-0.447] dispatch_prep=0.024ms [0.013-0.025] narrow=0.094ms [0.074-0.099] gap_readback=0.145ms [0.102-0.232] readback=0.823ms [0.701-0.953] total=2.899ms cpu_total=5.162ms [4.318-9.749]
[dx12] work_graphs (avg over 10) upload=0.719ms [0.693-0.725] aabb_prep=0.032ms [0.019-0.036] build=0.618ms [0.321-2.839] query=0.350ms [0.341-0.369] work_graph=0.573ms [0.540-0.606] gap_readback=0.133ms [0.118-0.163] readback=0.818ms [0.717-0.843] total=3.243ms cpu_total=5.740ms [4.585-12.999]

## 4980d6fa164561d4fd98ce601dd27901fd91c887 (added warmup run)

RX 9070 XT:
[dx12] simple_naive (avg over 10) upload=0.743ms [0.714-0.772] aabb_prep=0.042ms [0.029-0.055] build=0.795ms [0.363-3.548] query=0.554ms [0.340-0.794] gap_narrow=0.145ms [0.118-0.198] narrow=0.247ms [0.164-0.343] gap_readback=0.137ms [0.114-0.172] readback=0.838ms [0.707-0.965] total=3.501ms cpu_total=5.923ms [4.599-13.164]
[dx12] simple_binned (avg over 10) upload=0.739ms [0.708-0.770] aabb_prep=0.042ms [0.028-0.055] build=0.780ms [0.352-3.557] query=0.671ms [0.420-0.938] gap_narrow=0.140ms [0.116-0.166] narrow=0.131ms [0.092-0.178] gap_readback=0.149ms [0.114-0.257] readback=0.819ms [0.704-0.855] total=3.471ms cpu_total=5.711ms [4.477-12.163]
[dx12] execute_indirect (avg over 10) upload=0.738ms [0.708-0.767] aabb_prep=0.041ms [0.030-0.056] build=0.838ms [0.350-4.052] query=0.671ms [0.420-0.934] dispatch_prep=0.032ms [0.023-0.044] narrow=0.129ms [0.092-0.174] gap_readback=0.172ms [0.131-0.227] readback=0.819ms [0.711-0.850] total=3.441ms cpu_total=5.675ms [4.443-12.278]
[dx12] work_graphs (avg over 10) upload=0.738ms [0.710-0.768] aabb_prep=0.041ms [0.030-0.055] build=0.796ms [0.343-3.582] query=0.550ms [0.338-0.789] work_graph=0.859ms [0.578-1.177] gap_readback=0.170ms [0.122-0.374] readback=0.836ms [0.731-0.944] total=3.989ms cpu_total=6.474ms [4.810-15.019]

> query in execute_indirect and simple_binned are slower because they have to calculate the respective bin and read the shape_type for both objects.

## d8ecc7f91550d44665aff720fbe4956313dd1475 (optimized memory reads for execute_indirect and simple_binned)

RX 9070 XT:
[dx12] simple_naive (avg over 10) upload=0.745ms [0.696-0.770] aabb_prep=0.043ms [0.020-0.053] build=0.738ms [0.362-2.789] query=0.545ms [0.338-0.752] gap_narrow=0.147ms [0.093-0.170] narrow=0.254ms [0.142-0.352] gap_readback=0.138ms [0.078-0.165] readback=0.827ms [0.700-0.887] total=3.438ms cpu_total=5.716ms [4.527-10.304]
[dx12] simple_binned (avg over 10) upload=0.734ms [0.688-0.763] aabb_prep=0.041ms [0.018-0.055] build=0.698ms [0.356-2.639] query=0.566ms [0.366-0.869] gap_narrow=0.139ms [0.094-0.172] narrow=0.125ms [0.073-0.179] gap_readback=0.132ms [0.078-0.169] readback=0.832ms [0.698-0.947] total=3.267ms cpu_total=5.418ms [4.275-9.519]
[dx12] execute_indirect (avg over 10) upload=0.735ms [0.690-0.768] aabb_prep=0.041ms [0.017-0.056] build=0.739ms [0.358-3.078] query=0.563ms [0.368-0.860] dispatch_prep=0.033ms [0.016-0.045] narrow=0.125ms [0.075-0.181] gap_readback=0.164ms [0.131-0.209] readback=0.821ms [0.702-0.880] total=3.221ms cpu_total=5.342ms [4.229-9.924]
[dx12] work_graphs (avg over 10) upload=0.735ms [0.692-0.767] aabb_prep=0.041ms [0.019-0.056] build=0.703ms [0.339-2.662] query=0.502ms [0.337-0.746] work_graph=0.843ms [0.539-1.417] gap_readback=0.148ms [0.121-0.172] readback=0.827ms [0.714-0.879] total=3.799ms cpu_total=6.243ms [4.712-12.982]

RTX 5070 TI:
[dx12] simple_naive (avg over 10) upload=0.014ms [0.012-0.019] aabb_prep=0.019ms [0.019-0.020] build=0.450ms [0.297-1.528] query=0.272ms [0.259-0.290] gap_narrow=0.083ms [0.071-0.111] narrow=0.122ms [0.114-0.125] gap_readback=0.075ms [0.064-0.093] readback=0.179ms [0.167-0.189] total=1.214ms cpu_total=2.487ms [2.135-4.478]
[dx12] simple_binned (avg over 10) upload=0.013ms [0.012-0.017] aabb_prep=0.020ms [0.019-0.022] build=0.420ms [0.297-1.350] query=0.328ms [0.310-0.351] gap_narrow=0.085ms [0.072-0.152] narrow=0.082ms [0.077-0.085] gap_readback=0.073ms [0.062-0.092] readback=0.179ms [0.165-0.193] total=1.199ms cpu_total=2.394ms [2.102-4.347]
[dx12] execute_indirect (avg over 10) upload=0.013ms [0.012-0.017] aabb_prep=0.019ms [0.019-0.020] build=0.414ms [0.306-1.330] query=0.328ms [0.311-0.350] dispatch_prep=0.007ms [0.006-0.008] narrow=0.100ms [0.094-0.102] gap_readback=0.084ms [0.073-0.106] readback=0.179ms [0.163-0.200] total=1.144ms cpu_total=2.330ms [2.026-4.300]
[dx12] work_graphs (avg over 10) upload=0.013ms [0.012-0.018] aabb_prep=0.018ms [0.017-0.019] build=0.472ms [0.319-1.737] query=0.275ms [0.262-0.290] work_graph=0.428ms [0.401-0.456] gap_readback=0.077ms [0.066-0.097] readback=0.181ms [0.165-0.202] total=1.464ms cpu_total=2.651ms [2.320-4.990]

## fe7d6ab82abdf1d67d9326ac46327201bd5910db (Improve profiling)

RX 9070 XT:
[dx12] simple_naive (avg over 10)
    GPU: aabb_prep=0.051ms [0.038-0.074] build=0.778ms [0.309-3.553] query=0.697ms [0.395-0.944] gap_narrow=0.215ms [0.125-0.443] narrow=0.278ms [0.167-0.367] total=2.019ms [1.080ms-4.929ms]
    CPU: upload=2.288ms work=2.623ms download=1.940ms
[dx12] simple_binned (avg over 10)
    GPU: aabb_prep=0.048ms [0.035-0.062] build=0.789ms [0.335-3.551] query=0.712ms [0.404-0.958] gap_narrow=0.169ms [0.119-0.220] narrow=0.153ms [0.098-0.202] total=1.872ms [1.030ms-4.803ms]
    CPU: upload=1.903ms work=2.461ms download=1.887ms
[dx12] execute_indirect (avg over 10)
    GPU: aabb_prep=0.049ms [0.036-0.061] build=0.826ms [0.335-3.954] query=0.710ms [0.401-0.953] dispatch_prep=0.037ms [0.025-0.047] narrow=0.154ms [0.101-0.198] total=1.775ms [0.901ms-5.068ms]
    CPU: upload=1.967ms work=2.385ms download=1.889ms
[dx12] work_graphs (avg over 10)
    GPU: aabb_prep=0.044ms [0.034-0.062] build=0.721ms [0.328-3.429] query=0.594ms [0.392-0.917] work_graph=0.514ms [0.368-0.740] total=1.874ms [1.144ms-5.047ms]
    CPU: upload=2.444ms work=2.624ms download=2.049ms

## 7d91ce682615c4c96e1def29eca39b2c797ff707 (warmup + deferred validation to avoid amd downclocking)

RX 9070 XT:
[dx12] simple_naive (avg over 10)
    GPU: aabb_prep=0.030ms [0.030-0.031] build=0.332ms [0.296-0.356] query=0.411ms [0.393-0.434] gap_narrow=0.135ms [0.117-0.158] narrow=0.166ms [0.159-0.170] total=1.074ms [1.022ms-1.106ms]
    CPU: upload=1.364ms work=1.240ms download=1.873ms
[dx12] simple_binned (avg over 10)
    GPU: aabb_prep=0.030ms [0.030-0.031] build=0.334ms [0.316-0.419] query=0.421ms [0.402-0.442] gap_narrow=0.171ms [0.120-0.332] narrow=0.096ms [0.092-0.098] total=1.052ms [0.966ms-1.213ms]
    CPU: upload=1.031ms work=1.206ms download=1.805ms
[dx12] execute_indirect (avg over 10)
    GPU: aabb_prep=0.030ms [0.030-0.031] build=0.338ms [0.322-0.350] query=0.419ms [0.403-0.439] disp_prep=0.027ms [0.027-0.028] narrow=0.098ms [0.093-0.100] total=0.912ms [0.892ms-0.933ms]
    CPU: upload=1.026ms work=1.074ms download=1.778ms
[dx12] work_graphs (avg over 10)
    GPU: aabb_prep=0.030ms [0.030-0.031] build=0.326ms [0.297-0.358] query=0.410ms [0.394-0.429] work_graph=0.371ms [0.356-0.382] total=1.138ms [1.100ms-1.176ms]
    CPU: upload=1.035ms work=1.288ms download=1.777ms

RTX 5070 TI:
[dx12] simple_naive (avg over 10)
    GPU: aabb_prep=0.018ms [0.017-0.019] build=0.310ms [0.300-0.325] query=0.274ms [0.260-0.288] gap_narrow=0.090ms [0.073-0.178] narrow=0.120ms [0.114-0.126] total=0.813ms [0.779ms-0.925ms]
    CPU: upload=0.467ms work=0.937ms download=0.772ms
[dx12] simple_binned (avg over 10)
    GPU: aabb_prep=0.017ms [0.016-0.020] build=0.295ms [0.293-0.299] query=0.326ms [0.307-0.344] gap_narrow=0.076ms [0.074-0.078] narrow=0.082ms [0.076-0.086] total=0.796ms [0.770ms-0.810ms]
    CPU: upload=0.305ms work=0.889ms download=0.610ms
[dx12] execute_indirect (avg over 10)
    GPU: aabb_prep=0.018ms [0.016-0.020] build=0.315ms [0.305-0.381] query=0.328ms [0.311-0.351] disp_prep=0.007ms [0.006-0.008] narrow=0.099ms [0.095-0.102] total=0.767ms [0.736ms-0.832ms]
    CPU: upload=0.274ms work=0.863ms download=0.623ms
[dx12] work_graphs (avg over 10)
    GPU: aabb_prep=0.017ms [0.015-0.018] build=0.320ms [0.316-0.325] query=0.273ms [0.259-0.286] work_graph=0.423ms [0.402-0.434] total=1.033ms [1.018ms-1.055ms]
    CPU: upload=0.291ms work=1.115ms download=0.622ms

> Previous benchmarks use 100.000 objects and a container of 75x75x75
> Following benchmarks use 300.000 objects and a container of 300x75x75, resulting in the following amount of collisions:
> - Frame 160: 610731
> - Frame 161: 619437
> - Frame 162: 627299
> - Frame 163: 631637
> - Frame 164: 634208
> - Frame 165: 635344
> - Frame 166: 633324
> - Frame 167: 630826
> - Frame 168: 626098
> - Frame 169: 619037

## 415ff626e90ac8b6b1402009fb00cdd4c9334343

RX 9070 XT:
[dx12] simple_naive (avg over 10)
    GPU: aabb_prep=0.035ms [0.034-0.042] build=0.422ms [0.385-0.488] query=0.629ms [0.606-0.650] gap_narrow=0.121ms [0.107-0.160] narrow=0.294ms [0.287-0.300] total=1.502ms [1.473ms-1.567ms]
    CPU: upload=2.829ms work=1.648ms download=3.835ms
[dx12] simple_binned (avg over 10)
    GPU: aabb_prep=0.035ms [0.034-0.036] build=0.407ms [0.388-0.441] query=0.664ms [0.637-0.760] gap_narrow=0.117ms [0.103-0.135] narrow=0.187ms [0.181-0.203] total=1.410ms [1.360ms-1.575ms]
    CPU: upload=2.823ms work=1.551ms download=3.841ms
[dx12] execute_indirect (avg over 10)
    GPU: aabb_prep=0.035ms [0.035-0.037] build=0.413ms [0.394-0.454] query=0.665ms [0.638-0.765] disp_prep=0.024ms [0.023-0.027] narrow=0.189ms [0.182-0.205] total=1.327ms [1.286ms-1.488ms]
    CPU: upload=2.758ms work=1.485ms download=3.846ms
[dx12] work_graphs (avg over 10)
    GPU: aabb_prep=0.035ms [0.034-0.037] build=0.451ms [0.391-0.860] query=0.644ms [0.608-0.737] work_graph=0.686ms [0.658-0.758] total=1.816ms [1.732ms-2.191ms]
    CPU: upload=2.768ms work=1.967ms download=3.857ms

## 7543de3321fd54197bff7ee5cd2c83a58f66c700 (split execute indirect into 6 shaders)

RX 9070 XT:
[dx12] simple_naive (avg over 10)
    GPU: aabb_prep=0.035ms [0.034-0.035] build=0.421ms [0.408-0.455] query=0.633ms [0.612-0.652] gap_narrow=0.116ms [0.106-0.139] narrow=0.294ms [0.286-0.299] total=1.500ms [1.458ms-1.568ms]
    CPU: upload=2.822ms work=1.645ms download=3.847ms
[dx12] simple_binned (avg over 10)
    GPU: aabb_prep=0.035ms [0.034-0.036] build=0.435ms [0.389-0.660] query=0.665ms [0.632-0.770] gap_narrow=0.120ms [0.107-0.142] narrow=0.187ms [0.180-0.203] total=1.442ms [1.371ms-1.804ms]
    CPU: upload=2.815ms work=1.586ms download=3.838ms
[dx12] execute_indirect (avg over 10)
    GPU: aabb_prep=0.035ms [0.034-0.037] build=0.409ms [0.393-0.467] query=0.666ms [0.636-0.767] disp_prep=0.024ms [0.023-0.026] narrow=0.185ms [0.178-0.200] total=1.318ms [1.274ms-1.497ms]
    CPU: upload=2.773ms work=1.484ms download=4.195ms
[dx12] work_graphs (avg over 10)
    GPU: aabb_prep=0.035ms [0.034-0.038] build=0.406ms [0.386-0.448] query=0.642ms [0.608-0.751] work_graph=0.687ms [0.652-0.774] total=1.771ms [1.686ms-1.998ms]
    CPU: upload=2.791ms work=1.947ms download=3.890ms

## 05d1ccbc1bd28b869a7490058513df11c7b10863 (flatten functions to optimize register usage)

RX 9070 XT:
[dx12] simple_naive (avg over 10)
    GPU: aabb_prep=0.035ms [0.034-0.037] build=0.418ms [0.387-0.449] query=0.629ms [0.608-0.657] gap_narrow=0.132ms [0.105-0.172] narrow=0.296ms [0.289-0.300] total=1.510ms [1.444ms-1.562ms]
    CPU: upload=2.852ms work=1.669ms download=3.858ms
[dx12] simple_binned (avg over 10)
    GPU: aabb_prep=0.035ms [0.034-0.036] build=0.404ms [0.384-0.436] query=0.662ms [0.622-0.743] gap_narrow=0.115ms [0.102-0.155] narrow=0.176ms [0.170-0.189] total=1.392ms [1.343ms-1.532ms]
    CPU: upload=2.857ms work=1.541ms download=3.846ms
[dx12] execute_indirect (avg over 10)
    GPU: aabb_prep=0.035ms [0.034-0.037] build=0.418ms [0.396-0.462] query=0.662ms [0.630-0.759] disp_prep=0.024ms [0.023-0.026] narrow=0.182ms [0.175-0.196] total=1.321ms [1.259ms-1.460ms]
    CPU: upload=2.801ms work=1.484ms download=4.315ms
[dx12] work_graphs (avg over 10)
    GPU: aabb_prep=0.035ms [0.034-0.038] build=0.406ms [0.385-0.478] query=0.640ms [0.609-0.740] work_graph=0.685ms [0.661-0.754] total=1.767ms [1.693ms-2.009ms]
    CPU: upload=2.830ms work=1.921ms download=3.889ms

RTX 5070 TI:
[dx12] simple_naive (avg over 10)
    GPU: aabb_prep=0.029ms [0.028-0.030] build=0.368ms [0.359-0.403] query=0.532ms [0.508-0.560] gap_narrow=0.086ms [0.073-0.117] narrow=0.240ms [0.233-0.243] total=1.255ms [1.207ms-1.311ms]
    CPU: upload=0.721ms work=1.364ms download=1.337ms
[dx12] simple_binned (avg over 10)
    GPU: aabb_prep=0.029ms [0.027-0.030] build=0.378ms [0.362-0.412] query=0.633ms [0.616-0.656] gap_narrow=0.091ms [0.073-0.114] narrow=0.155ms [0.152-0.157] total=1.287ms [1.245ms-1.338ms]
    CPU: upload=0.738ms work=1.403ms download=1.362ms
[dx12] execute_indirect (avg over 10)
    GPU: aabb_prep=0.029ms [0.028-0.032] build=0.397ms [0.379-0.435] query=0.634ms [0.613-0.661] disp_prep=0.007ms [0.006-0.008] narrow=0.189ms [0.185-0.192] total=1.256ms [1.217ms-1.315ms]
    CPU: upload=0.753ms work=1.364ms download=1.348ms
[dx12] work_graphs (avg over 10)
    GPU: aabb_prep=0.027ms [0.026-0.030] build=0.396ms [0.382-0.428] query=0.529ms [0.506-0.553] work_graph=0.966ms [0.916-0.983] total=1.919ms [1.832ms-1.968ms]
    CPU: upload=0.800ms work=2.031ms download=1.375ms

## fff0db20954946448a277136eb314e759faaa7bb (undo split of execute indirect into 6 shaders)

RX 9070 XT:
[dx12] simple_naive (avg over 10)
    GPU: aabb_prep=0.034ms [0.034-0.035] build=0.417ms [0.390-0.464] query=0.633ms [0.617-0.651] gap_narrow=0.128ms [0.104-0.158] narrow=0.296ms [0.289-0.300] total=1.509ms [1.459ms-1.591ms]
    CPU: upload=2.848ms work=1.663ms download=3.866ms
[dx12] simple_binned (avg over 10)
    GPU: aabb_prep=0.035ms [0.034-0.036] build=0.418ms [0.383-0.454] query=0.664ms [0.640-0.732] gap_narrow=0.122ms [0.105-0.148] narrow=0.176ms [0.171-0.187] total=1.414ms [1.376ms-1.521ms]
    CPU: upload=2.846ms work=1.569ms download=3.864ms
[dx12] execute_indirect (avg over 10)
    GPU: aabb_prep=0.035ms [0.035-0.038] build=0.418ms [0.392-0.457] query=0.662ms [0.630-0.770] disp_prep=0.024ms [0.023-0.026] narrow=0.178ms [0.171-0.193] total=1.317ms [1.275ms-1.484ms]
    CPU: upload=2.800ms work=1.477ms download=3.882ms
[dx12] work_graphs (avg over 10)
    GPU: aabb_prep=0.035ms [0.034-0.037] build=0.404ms [0.387-0.436] query=0.639ms [0.611-0.740] work_graph=0.686ms [0.657-0.756] total=1.765ms [1.694ms-1.969ms]
    CPU: upload=2.788ms work=1.920ms download=3.868ms

RTX 3050:
[dx12] work_graphs (avg over 10)
    GPU: aabb_prep=0.129ms [0.126-0.131] build=1.170ms [1.085-1.389] query=2.252ms [2.010-2.891] work_graph=4.248ms [3.749-4.674] total=7.798ms [7.480ms-8.067ms]
    CPU: upload=3.267ms work=7.986ms download=4.425ms

## 37d382ca1a2b3001b587ab4e2285e8829c779e05 (work graphs "optimization")

RX 9070 XT:
[dx12] simple_naive (avg over 10)
    GPU: aabb_prep=0.034ms [0.034-0.035] build=0.502ms [0.471-0.536] query=0.632ms [0.607-0.653] gap_narrow=0.207ms [0.195-0.227] narrow=0.296ms [0.288-0.300] total=1.671ms [1.622ms-1.709ms]
    CPU: upload=2.858ms work=1.885ms download=3.973ms
[dx12] execute_indirect (avg over 10)
    GPU: aabb_prep=0.035ms [0.034-0.037] build=0.494ms [0.468-0.548] query=0.661ms [0.626-0.762] disp_prep=0.024ms [0.023-0.026] narrow=0.178ms [0.171-0.193] total=1.391ms [1.350ms-1.566ms]
    CPU: upload=2.869ms work=1.612ms download=3.988ms
[dx12] work_graphs (avg over 10)
    GPU: aabb_prep=0.035ms [0.034-0.037] build=0.483ms [0.468-0.554] query=0.641ms [0.604-0.740] work_graph=0.768ms [0.722-0.858] total=1.927ms [1.831ms-2.190ms]
    CPU: upload=2.844ms work=2.142ms download=3.975ms

RTX 3050:
[dx12] work_graphs (avg over 10)
    GPU: aabb_prep=0.128ms [0.125-0.132] build=1.127ms [1.103-1.147] query=2.123ms [2.016-2.238] work_graph=2.350ms [2.276-2.463] total=5.727ms [5.540ms-5.959ms]
    CPU: upload=2.934ms work=5.869ms download=4.110ms

RTX 5070 TI:
[dx12] simple_naive (avg over 10)
    GPU: aabb_prep=0.029ms [0.028-0.030] build=0.373ms [0.357-0.458] query=0.531ms [0.509-0.562] gap_narrow=0.087ms [0.074-0.114] narrow=0.240ms [0.234-0.243] total=1.260ms [1.204ms-1.389ms]
    CPU: upload=0.705ms work=1.354ms download=1.302ms
[dx12] execute_indirect (avg over 10)
    GPU: aabb_prep=0.029ms [0.027-0.030] build=0.383ms [0.364-0.491] query=0.634ms [0.613-0.657] disp_prep=0.007ms [0.006-0.008] narrow=0.172ms [0.167-0.175] total=1.225ms [1.182ms-1.318ms]
    CPU: upload=0.696ms work=1.323ms download=1.302ms
[dx12] work_graphs (avg over 10)
    GPU: aabb_prep=0.027ms [0.026-0.029] build=0.382ms [0.374-0.401] query=0.530ms [0.508-0.557] work_graph=0.587ms [0.567-0.596] total=1.526ms [1.486ms-1.558ms]
    CPU: upload=0.767ms work=1.620ms download=1.297ms

## e02381fa153717d2f33f5bf4036e345c78edb918 (the actual work graphs optimization!)

RX 9070 XT:
[dx12] simple_naive (avg over 10)
    GPU: aabb_prep=0.036ms [0.035-0.037] build=0.516ms [0.484-0.554] query=0.636ms [0.615-0.656] gap_narrow=0.222ms [0.202-0.266] narrow=0.299ms [0.291-0.304] total=1.710ms [1.660ms-1.815ms]
    CPU: upload=2.919ms work=1.940ms download=4.020ms
[dx12] execute_indirect (avg over 10)
    GPU: aabb_prep=0.037ms [0.036-0.038] build=0.510ms [0.478-0.570] query=0.670ms [0.629-0.777] disp_prep=0.024ms [0.023-0.027] narrow=0.183ms [0.176-0.198] total=1.424ms [1.368ms-1.610ms]
    CPU: upload=2.958ms work=1.661ms download=4.013ms
[dx12] work_graphs (avg over 10)
    GPU: aabb_prep=0.036ms [0.035-0.037] build=0.509ms [0.480-0.557] query=0.682ms [0.649-0.776] work_graph=0.281ms [0.272-0.293] total=1.508ms [1.449ms-1.636ms]
    CPU: upload=2.931ms work=1.735ms download=4.038ms

RTX 5070 TI:
[dx12] simple_naive (avg over 10)
    GPU: aabb_prep=0.029ms [0.027-0.031] build=0.361ms [0.354-0.387] query=0.530ms [0.510-0.560] gap_narrow=0.080ms [0.064-0.126] narrow=0.240ms [0.235-0.243] total=1.240ms [1.194ms-1.268ms]
    CPU: upload=0.674ms work=1.333ms download=1.295ms
[dx12] execute_indirect (avg over 10)
    GPU: aabb_prep=0.029ms [0.027-0.033] build=0.373ms [0.364-0.386] query=0.633ms [0.613-0.661] disp_prep=0.007ms [0.006-0.008] narrow=0.172ms [0.169-0.174] total=1.214ms [1.193ms-1.257ms]
    CPU: upload=0.713ms work=1.318ms download=1.304ms
[dx12] work_graphs (avg over 10)
    GPU: aabb_prep=0.029ms [0.027-0.030] build=0.391ms [0.384-0.419] query=0.738ms [0.720-0.757] work_graph=0.259ms [0.254-0.262] total=1.417ms [1.396ms-1.463ms]
    CPU: upload=0.753ms work=1.510ms download=1.286ms

RTX 3050:
[dx12] simple_naive (avg over 10)
    GPU: aabb_prep=0.130ms [0.127-0.134] build=1.210ms [1.080-1.776] query=2.116ms [2.010-2.230] gap_narrow=0.121ms [0.108-0.134] narrow=1.196ms [1.165-1.209] total=4.774ms [4.530ms-5.473ms]
    CPU: upload=2.969ms work=4.917ms download=4.183ms
[dx12] execute_indirect (avg over 10)
    GPU: aabb_prep=0.130ms [0.126-0.133] build=1.140ms [1.073-1.409] query=2.101ms [1.970-2.455] disp_prep=0.005ms [0.005-0.005] narrow=0.906ms [0.885-0.920] total=4.282ms [4.107ms-4.639ms]
    CPU: upload=2.967ms work=4.428ms download=4.149ms
[dx12] work_graphs (avg over 10)
    GPU: aabb_prep=0.128ms [0.125-0.132] build=1.138ms [1.107-1.174] query=2.112ms [1.998-2.245] work_graph=1.090ms [1.044-1.285] total=4.468ms [4.286ms-4.653ms]
    CPU: upload=2.956ms work=4.668ms download=4.156ms

## 7d12379c9c9a5ab96c0257f37c1b888333e9663e (reduce dx_entity to 32 byte)

RTX 5070 TI:
[dx12] simple_naive (avg over 10)
    GPU: aabb_prep=0.030ms [0.028-0.030] build=0.357ms [0.353-0.362] query=0.530ms [0.508-0.560] gap_narrow=0.078ms [0.068-0.116] narrow=0.238ms [0.232-0.240] total=1.232ms [1.200ms-1.288ms]
    CPU: upload=0.484ms work=1.327ms download=1.308ms
[dx12] execute_indirect (avg over 10)
    GPU: aabb_prep=0.030ms [0.028-0.032] build=0.383ms [0.358-0.470] query=0.634ms [0.614-0.654] disp_prep=0.007ms [0.006-0.007] narrow=0.160ms [0.157-0.164] total=1.213ms [1.170ms-1.296ms]
    CPU: upload=0.498ms work=1.306ms download=1.299ms
[dx12] work_graphs (avg over 10)
    GPU: aabb_prep=0.030ms [0.029-0.031] build=0.396ms [0.378-0.479] query=0.736ms [0.714-0.766] work_graph=0.249ms [0.244-0.258] total=1.411ms [1.375ms-1.507ms]
    CPU: upload=0.550ms work=1.502ms download=1.298ms

## 00af973159ebf834af013b05cbe95b678fb6cd76 (reduce dx_potential_pair to 8 byte)

RTX 5070 TI:
[dx12] simple_naive (avg over 10)
    GPU: aabb_prep=0.030ms [0.028-0.030] build=0.361ms [0.354-0.390] query=0.513ms [0.492-0.540] gap_narrow=0.078ms [0.072-0.099] narrow=0.236ms [0.230-0.240] total=1.218ms [1.196ms-1.242ms]
    CPU: upload=0.490ms work=1.312ms download=1.314ms
[dx12] execute_indirect (avg over 10)
    GPU: aabb_prep=0.030ms [0.029-0.031] build=0.369ms [0.361-0.401] query=0.618ms [0.599-0.641] disp_prep=0.007ms [0.006-0.009] narrow=0.160ms [0.157-0.162] total=1.185ms [1.159ms-1.212ms]
    CPU: upload=0.498ms work=1.277ms download=1.305ms
[dx12] work_graphs (avg over 10)
    GPU: aabb_prep=0.030ms [0.028-0.032] build=0.387ms [0.377-0.418] query=0.727ms [0.712-0.741] work_graph=0.247ms [0.240-0.252] total=1.391ms [1.369ms-1.439ms]
    CPU: upload=0.541ms work=1.480ms download=1.292ms
