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

[dx12] simple_naive (avg over 10) upload=0.817ms [0.702-0.862] aabb_prep=0.111ms [0.020-0.138] build=0.992ms [0.606-2.854] query=0.619ms [0.342-1.071] gap_narrow=0.194ms [0.098-0.575] narrow=0.327ms [0.140-0.572] gap_readback=0.140ms [0.081-0.215] readback=0.880ms [0.701-1.236] total=4.081ms cpu_total=9.733ms [8.660-12.444]
[dx12] simple_binned (avg over 10) upload=0.718ms [0.691-0.724] aabb_prep=0.032ms [0.017-0.036] build=0.607ms [0.346-2.777] query=0.429ms [0.410-0.440] gap_narrow=0.125ms [0.110-0.164] narrow=0.091ms [0.074-0.095] gap_readback=0.119ms [0.079-0.153] readback=0.807ms [0.705-0.842] total=2.929ms cpu_total=5.141ms [4.398-9.565]
[dx12] execute_indirect (avg over 10) upload=0.719ms [0.691-0.724] aabb_prep=0.032ms [0.017-0.036] build=0.635ms [0.346-2.966] query=0.426ms [0.409-0.447] dispatch_prep=0.024ms [0.013-0.025] narrow=0.094ms [0.074-0.099] gap_readback=0.145ms [0.102-0.232] readback=0.823ms [0.701-0.953] total=2.899ms cpu_total=5.162ms [4.318-9.749]
[dx12] work_graphs (avg over 10) upload=0.719ms [0.693-0.725] aabb_prep=0.032ms [0.019-0.036] build=0.618ms [0.321-2.839] query=0.350ms [0.341-0.369] work_graph=0.573ms [0.540-0.606] gap_readback=0.133ms [0.118-0.163] readback=0.818ms [0.717-0.843] total=3.243ms cpu_total=5.740ms [4.585-12.999]

## 4980d6fa164561d4fd98ce601dd27901fd91c887 (added warmup run)

[dx12] simple_naive (avg over 10) upload=0.743ms [0.714-0.772] aabb_prep=0.042ms [0.029-0.055] build=0.795ms [0.363-3.548] query=0.554ms [0.340-0.794] gap_narrow=0.145ms [0.118-0.198] narrow=0.247ms [0.164-0.343] gap_readback=0.137ms [0.114-0.172] readback=0.838ms [0.707-0.965] total=3.501ms cpu_total=5.923ms [4.599-13.164]
[dx12] simple_binned (avg over 10) upload=0.739ms [0.708-0.770] aabb_prep=0.042ms [0.028-0.055] build=0.780ms [0.352-3.557] query=0.671ms [0.420-0.938] gap_narrow=0.140ms [0.116-0.166] narrow=0.131ms [0.092-0.178] gap_readback=0.149ms [0.114-0.257] readback=0.819ms [0.704-0.855] total=3.471ms cpu_total=5.711ms [4.477-12.163]
[dx12] execute_indirect (avg over 10) upload=0.738ms [0.708-0.767] aabb_prep=0.041ms [0.030-0.056] build=0.838ms [0.350-4.052] query=0.671ms [0.420-0.934] dispatch_prep=0.032ms [0.023-0.044] narrow=0.129ms [0.092-0.174] gap_readback=0.172ms [0.131-0.227] readback=0.819ms [0.711-0.850] total=3.441ms cpu_total=5.675ms [4.443-12.278]
[dx12] work_graphs (avg over 10) upload=0.738ms [0.710-0.768] aabb_prep=0.041ms [0.030-0.055] build=0.796ms [0.343-3.582] query=0.550ms [0.338-0.789] work_graph=0.859ms [0.578-1.177] gap_readback=0.170ms [0.122-0.374] readback=0.836ms [0.731-0.944] total=3.989ms cpu_total=6.474ms [4.810-15.019]

> query in execute_indirect and simple_binned are slower because they have to calculate the respective bin and read the shape_type for both objects.

## d8ecc7f91550d44665aff720fbe4956313dd1475 (optimized memory reads for execute_indirect and simple_binned)

[dx12] simple_naive (avg over 10) upload=0.745ms [0.696-0.770] aabb_prep=0.043ms [0.020-0.053] build=0.738ms [0.362-2.789] query=0.545ms [0.338-0.752] gap_narrow=0.147ms [0.093-0.170] narrow=0.254ms [0.142-0.352] gap_readback=0.138ms [0.078-0.165] readback=0.827ms [0.700-0.887] total=3.438ms cpu_total=5.716ms [4.527-10.304]
[dx12] simple_binned (avg over 10) upload=0.734ms [0.688-0.763] aabb_prep=0.041ms [0.018-0.055] build=0.698ms [0.356-2.639] query=0.566ms [0.366-0.869] gap_narrow=0.139ms [0.094-0.172] narrow=0.125ms [0.073-0.179] gap_readback=0.132ms [0.078-0.169] readback=0.832ms [0.698-0.947] total=3.267ms cpu_total=5.418ms [4.275-9.519]
[dx12] execute_indirect (avg over 10) upload=0.735ms [0.690-0.768] aabb_prep=0.041ms [0.017-0.056] build=0.739ms [0.358-3.078] query=0.563ms [0.368-0.860] dispatch_prep=0.033ms [0.016-0.045] narrow=0.125ms [0.075-0.181] gap_readback=0.164ms [0.131-0.209] readback=0.821ms [0.702-0.880] total=3.221ms cpu_total=5.342ms [4.229-9.924]
[dx12] work_graphs (avg over 10) upload=0.735ms [0.692-0.767] aabb_prep=0.041ms [0.019-0.056] build=0.703ms [0.339-2.662] query=0.502ms [0.337-0.746] work_graph=0.843ms [0.539-1.417] gap_readback=0.148ms [0.121-0.172] readback=0.827ms [0.714-0.879] total=3.799ms cpu_total=6.243ms [4.712-12.982]
