#!/bin/bash
set -e

# Default build directory
BUILD_DIR="build"

# Parse argument
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 {fcds|concurrent}"
    exit 1
fi

# Configure flags based on the mode
case "$1" in
    fcds)
        FCDS_FLAG="-DFCDS=ON"
        CONC_FLAG="-DCONC_MINHASH=OFF"
        ;;
    concurrent)
        FCDS_FLAG="-DFCDS=OFF"
        CONC_FLAG="-DCONC_MINHASH=ON"
        ;;
    *)
        echo "Unknown build mode: $1"
        echo "Usage: $0 {fcds|concurrent}"
        exit 1
        ;;
esac

# Create and enter build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Run CMake and compile
cmake .. $FCDS_FLAG $CONC_FLAG
make -j$(nproc)
