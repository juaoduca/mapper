#!/bin/bash

# A script to automate the build and test process for a CMake project.
# This script assumes you are running it from the root of your project directory.
#
# Commands:
# 1. Navigates up one directory (unnecessary if running from project root,
#    but included to match the user's request).
# 2. Deletes the old 'build' directory and creates a new one.
# 3. Changes into the new 'build' directory.
# 4. Runs CMake to configure the project.
# 5. Builds the project using all available CPU cores.
# 6. Runs all tests with CTest, displaying detailed output for failures.

# Use 'set -e' to exit immediately if a command exits with a non-zero status.
#set -e

# Step 1: Navigate up one directory (only if necessary)
# This is here because your original request included "cd ..".
# If you are already at the project root, you can remove this line.
# cd ..

# Step 2: Clean up old build files and create a new directory.
echo "Removing old 'build' directory and creating a new one..."
rm -rf build
mkdir build

# Step 3: Change into the build directory.
echo "Navigating into the 'build' directory..."
cd build

# Step 4 & 5: Configure and build the project with CMake.
# 'cmake ..' configures the project.
# 'cmake --build .' builds it. The '-j' flag automatically uses all available CPU cores for a faster build.
echo "Configuring and building the project..."
cmake ..
cmake --build . -j

# Step 6: Run the tests with CTest.
# The '--output-on-failure' flag is very useful for debugging test failures.
echo "Running tests..."
ctest --output-on-failure

echo "Script finished successfully."