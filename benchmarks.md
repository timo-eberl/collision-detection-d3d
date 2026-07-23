Benchmarks use 100.000 bodies simulated over 200 frames.

## f9d5b82bb37db6090fa083596382dcc3875d7714 (Simple Single Pass)

[dx12] simple (avg over 200) upload=0.467ms [0.455-0.747] kernel=25.873ms [21.087-36.166] cmd_gap=0.191ms [0.103-6.954] readback=0.798ms [0.012-2.918] total=27.330ms cpu_total=31.428ms [23.783-344.861]

## a888fa8e5a5c2f41f4b7ee3876af8d5381ec04f0 (Simple Two Pass)

[dx12] simple (avg over 200) upload=0.465ms [0.458-0.480] broad=25.169ms [22.425-31.953] gap_narrow=0.153ms [0.117-0.387] narrow=0.037ms [0.011-0.081] gap_readback=0.422ms [0.082-62.944] readback=0.794ms [0.012-3.007] total=27.040ms cpu_total=30.524ms [24.688-309.018]
