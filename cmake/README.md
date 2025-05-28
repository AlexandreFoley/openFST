# Building OpenFst with CMake

This document explains how to build OpenFst using CMake.

## Requirements

- CMake 3.14 or later
- C++17 compiler
- (Optional) Abseil library
- (Optional) GoogleTest for tests

## Build Instructions

### Basic Build

```bash
mkdir build && cd build
cmake ..
make
```

### Install

```bash
mkdir build && cd build
cmake ..
make
make install
```

### Building with Options

```bash
mkdir build && cd build
cmake .. -DBUILD_SHARED_LIBS=ON -DFST_ENABLE_TESTING=ON
make
```

## Available CMake Options

- `BUILD_SHARED_LIBS` - Build shared libraries (default: ON)
- `FST_ENABLE_TESTING` - Enable unit tests (default: ON)
- `FST_USE_ABSL` - Use Abseil libraries (default: OFF)
- `FST_NO_DYNAMIC_LINKING` - Disable dynamic linking (default: OFF)

## Using OpenFst in Your CMake Projects

After installing OpenFst, you can include it in your own CMake projects:

```cmake
find_package(OpenFst REQUIRED COMPONENTS far pdt)

add_executable(my_app my_app.cpp)
target_link_libraries(my_app OpenFst::fst)
```

## Available Components

The following components can be included with `find_package`:

- `far` - Fst ARchive library
- `pdt` - PushDown Transducers library
- `mpdt` - Multi PushDown Transducers library
- `ngram` - N-gram language models library
- `compact` - Compact FSTs library
- `compress` - Compressed FSTs library
- `linear` - Linear FSTs library
- `lookahead` - Lookahead FSTs library
- `special` - Special FSTs library
