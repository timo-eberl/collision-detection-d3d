#!/bin/bash
set -e

DATA_DIR="bench_narrow_data"
LOG_DIR="bench_narrow_logs"
EXECUTABLE="build/relwithdebinfo/collision_dx_app.exe"

mkdir -p "$LOG_DIR"

echo "=== Building Profiler Application ==="
cmake -S . -B build/relwithdebinfo/ \
    -DCDDX_ENABLE_PROFILER=ON \
    -DCDDX_ENABLE_VALIDATION=OFF \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo

cmake --build build/relwithdebinfo/ --config RelWithDebInfo --parallel

echo -e "\n=== Running Profiler on Benchmark Data ==="

if [ ! -d "$DATA_DIR" ]; then
    echo "Error: Directory '$DATA_DIR' not found."
    echo "Please copy the data folder from the generation step into this repository."
    exit 1
fi

for DATA_FILE in "$DATA_DIR"/*.bin; do
    if [ ! -e "$DATA_FILE" ]; then
        echo "No .bin files found in $DATA_DIR."
        break
    fi

    # Extract the base name (e.g. "bench_100_spheres_data") for the log file
    FILENAME=$(basename "$DATA_FILE")
    BASE_NAME="${FILENAME%.bin}"
    LOG_FILE="$LOG_DIR/${BASE_NAME}.log"

    echo "Profiling data: $FILENAME..."

    # The executable expects the file to be named exactly 'collision_test_data.bin'
    cp "$DATA_FILE" collision_test_data.bin

    # Run the executable and redirect stderr to the log file
    "$EXECUTABLE" 2> "$LOG_FILE"

    echo "  -> Saved logs to $LOG_FILE"
done

rm -f collision_test_data.bin

echo -e "\nAll profiling runs completed successfully! Check the '$LOG_DIR/' folder for your logs."
