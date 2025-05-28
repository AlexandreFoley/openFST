#!/bin/bash
# Script to test OpenFST installation without installing system-wide

set -e  # Exit on error

# Create temporary installation directory
INSTALL_DIR="$(pwd)/install_test"
rm -rf "${INSTALL_DIR}"
mkdir -p "${INSTALL_DIR}"

# Create build directory for OpenFST
BUILD_DIR="$(pwd)/build_test"
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

# Build and install OpenFST to temporary location
cd "${BUILD_DIR}"
cmake -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}" \
      -DBUILD_SHARED_LIBS=ON \
      -DENABLE_FSTSCRIPT=ON \
      -DENABLE_TESTS=OFF \
      ..
cmake --build . --config Release
cmake --install .

# Create a test project that uses OpenFST
TEST_PROJECT_DIR="${BUILD_DIR}/test_project"
mkdir -p "${TEST_PROJECT_DIR}"

# Create a simple test program
cat > "${TEST_PROJECT_DIR}/test_fst.cc" << EOL
#include <fst/fstlib.h>
#include <iostream>

int main() {
  // Create a simple FST
  fst::StdVectorFst fst;
  
  // Add state 0
  fst.AddState();  // 0 = start state
  fst.SetStart(0);
  
  // Add state 1
  fst.AddState();  // 1 = final state
  fst.SetFinal(1, fst::TropicalWeight::One());
  
  // Add arc from state 0 to state 1
  fst.AddArc(0, fst::StdArc(1, 1, 0, 1));  // Input=1, Output=1, Weight=0, NextState=1
  
  // Print number of states
  std::cout << "FST has " << fst.NumStates() << " states" << std::endl;
  
  return 0;
}
EOL

# Create CMakeLists.txt for the test project
cat > "${TEST_PROJECT_DIR}/CMakeLists.txt" << EOL
cmake_minimum_required(VERSION 3.14)
project(OpenFstTest)

set(CMAKE_CXX_STANDARD 17)

# Find OpenFST package
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_INSTALL_PREFIX}")
find_package(OpenFst REQUIRED)

# Create test executable
add_executable(test_fst test_fst.cc)
target_link_libraries(test_fst PRIVATE OpenFst::fst)
EOL

# Build the test project
cd "${TEST_PROJECT_DIR}"
cmake -DCMAKE_PREFIX_PATH="${INSTALL_DIR}" .
cmake --build .

# Run the test
echo "Running test program..."
./test_fst

# Cleanup if successful
# echo "Test successful! Cleaning up..."
# rm -rf "${INSTALL_DIR}"
# rm -rf "${BUILD_DIR}"