#!/bin/bash

# Default build settings
BUILD=0
CONFIG=0
SLOW=0
CLEAN=0
DEBUG=0
VERBOSE=0
TEST=0

# Function to display help menu
show_help() {
    echo "Usage: $0 [OPTIONS]"
    echo "Builds the project with specified options."
    echo
    echo "Options:"
    echo "  -s, --slow      Build with just one core to avoid hangs."
    echo "  -c, --clean     Removes the 'build' directory before building."
    echo "  -d, --debug     Configures a debug build."
    echo "  -v, --verbose   Enables verbose output for the build process."
    echo "  -t, --test      Runs tests after building."
    echo "  -h, --help      Display this help message and exit."
    echo
}

# Parse command-line arguments
for arg in "$@"; do
    case "$arg" in
        -s|--slow)
            BUILD=1
            SLOW=1
            ;;
        -c|--clean)
            BUILD=1
            CLEAN=1
            CONFIG=1
            ;;
        -d|--debug)
            BUILD=1
            DEBUG=1
            CONFIG=1
            ;;
        -v|--verbose)
            VERBOSE=1
            BUILD=1
            ;;
        -t|--test)
            TEST=1
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            echo "Error: Unknown option '$arg'" >&2
            show_help
            exit 1
            ;;
    esac
done

# --- Build Logic ---
CMAKE_ARGS="-S . -B build"
BUILD_ARGS=""

# Step 1: Handle --clean option
if [ $CLEAN -eq 1 ]; then
    echo "Removing build directory..."
    rm -rf build
    echo "Creating build directory..."
    mkdir build
fi

# Handle -d --debug option
if [ $DEBUG -eq 1 ]; then
    echo "Configuring DEBUG build..."
    CMAKE_ARGS="-S . -B .. -DCMAKE_BUILD_TYPE=Debug"
fi

if [ $CONFIG -eq 1 ]; then
    echo "Running CMake CONFIG with> cmake "$CMAKE_ARGS
    cmake $CMAKE_ARGS
fi

# Step 4: Build the project

if [ $VERBOSE -eq 1 ]; then
    BUILD_ARGS+=" -VERBOSE=1"
fi

if [ $SLOW -eq 1 ]; then
    BUILD_ARGS+=" -j1"
else
    BUILD_ARGS+=" -j"
fi

if [ $BUILD -eq 1 ]; then
    echo "Building project with>  cmake --build . "$BUILD_ARGS
    cmake --build . -- $BUILD_ARGS
fi


# Step 5: Run the tests if the --test flag was passed
if [ $TEST -eq 1 ]; then
    echo "Running tests..."
    ctest --output-on-failure
fi

echo "Script finished successfully."
