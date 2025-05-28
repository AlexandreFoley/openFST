cmake_minimum_required(VERSION 3.14)
project(OpenFst VERSION 1.8.4 LANGUAGES CXX)

# Options to match Bazel configurations
option(USE_LIBCPP "Use libc++ instead of stdlibc++" OFF)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)
option(USE_CXX17 "Build with C++17 features" ON)

# Optional extensions (matching configure.ac options)
####### The extensions are not supported by the CMakeLists.txt for now. ########
# option(ENABLE_COMPACT_FSTS "Enable CompactFst extensions" OFF)
# option(ENABLE_COMPRESS "Enable compression extension" OFF)
# option(ENABLE_CONST_FSTS "Enable ConstFst extensions" OFF)
# option(ENABLE_FAR "Enable FAR extensions" OFF)
# option(ENABLE_LINEAR_FSTS "Enable LinearTagger/ClassifierFst extensions" OFF)
# option(ENABLE_LOOKAHEAD_FSTS "Enable LookAheadFst extensions" OFF)
# option(ENABLE_MPDT "Enable MPDT extensions" OFF)
# option(ENABLE_NGRAM_FSTS "Enable NGramFst extension" OFF)
# option(ENABLE_PDT "Enable PDT extensions" OFF)
################################################################################
option(ENABLE_FSTSCRIPT "Build the fstscript library" ON)

# Set C++ standard to C++17 by default (matching Bazel config)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_program(CCACHE_PROGRAM ccache)
if (CCACHE_PROGRAM)
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
    set(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM})
endif ()

# Define config.h (similar to autoconf generated one)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/config.h
  @ONLY
)

# Create our custom openfst_config.h
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/openfst_config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/openfst_config.h
  @ONLY
)

# include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Include directories for all targets
# include_directories(
#   ${CMAKE_CURRENT_SOURCE_DIR}/src/include
#   ${CMAKE_CURRENT_SOURCE_DIR}/src  # For test headers
# )

# Core library
set(FST_SOURCES 
  "src/lib/compat.cc"  "src/lib/encode.cc"  "src/lib/flags.cc"   "src/lib/fst.cc"   "src/lib/fst-types.cc"
  "src/lib/mapped-file.cc"   "src/lib/properties.cc"   "src/lib/symbol-table.cc"  "src/lib/symbol-table-ops.cc" 
  "src/lib/weight.cc"   "src/lib/util.cc"
)

add_library(fst ${FST_SOURCES})
target_include_directories(fst PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src/include>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
# Add platform-specific flags:
target_compile_options(fst PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-fno-exceptions -Wno-deprecated-declarations>
  $<$<CXX_COMPILER_ID:MSVC>:/EHsc /wd4267 /wd4244>
)
set_target_properties(fst PROPERTIES 
  VERSION ${PROJECT_VERSION}
  SOVERSION 26 # From Makefile.am: libfst_la_LDFLAGS = -version-info 26:0:0
)

if(ENABLE_FSTSCRIPT)
# Script library (required for most binaries)
set(FSTSCRIPT_SOURCES
  "src/script/arciterator-class.cc"   "src/script/arcsort.cc"   "src/script/closure.cc"  "src/script/compile.cc" 
  "src/script/compose.cc"   "src/script/concat.cc"   "src/script/connect.cc"  "src/script/convert.cc" 
  "src/script/decode.cc"  "src/script/determinize.cc"   "src/script/difference.cc"   "src/script/disambiguate.cc" 
  "src/script/draw.cc"   "src/script/encode.cc"  "src/script/encodemapper-class.cc"   "src/script/epsnormalize.cc" 
  "src/script/equal.cc"   "src/script/equivalent.cc"   "src/script/fst-class.cc"  "src/script/getters.cc" 
  "src/script/info-impl.cc"   "src/script/info.cc"   "src/script/intersect.cc"   "src/script/invert.cc" 
  "src/script/isomorphic.cc"   "src/script/map.cc"  "src/script/minimize.cc"   "src/script/print.cc" 
  "src/script/project.cc"   "src/script/prune.cc"   "src/script/push.cc"   "src/script/randequivalent.cc"
  "src/script/randgen.cc"   "src/script/relabel.cc"   "src/script/replace.cc"   "src/script/reverse.cc" 
  "src/script/reweight.cc"   "src/script/rmepsilon.cc"  "src/script/shortest-distance.cc"   "src/script/shortest-path.cc" 
  "src/script/stateiterator-class.cc"   "src/script/synchronize.cc"  "src/script/text-io.cc"   "src/script/topsort.cc" 
  "src/script/union.cc"   "src/script/weight-class.cc"   "src/script/verify.cc"
)

add_library(fstscript ${FSTSCRIPT_SOURCES})
target_link_libraries(fstscript PUBLIC fst)
set_target_properties(fstscript PROPERTIES 
  VERSION ${PROJECT_VERSION}
  SOVERSION 26 # From Makefile.am: libfstscript_la_LDFLAGS = -version-info 26:0:0
)
# Set output directories using generator expressions
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/bin>)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/lib>)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY $<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/lib>)

# Add binaries (utilities)
set(FST_BINARIES
  fstarcsort fstclosure fstcompile fstcompose fstconcat fstconnect
  fstconvert fstdeterminize fstdifference fstdisambiguate fstdraw
  fstencode fstepsnormalize fstequal fstequivalent fstinfo fstintersect
  fstinvert fstisomorphic fstmap fstminimize fstprint fstproject
  fstprune fstpush fstrandgen fstrelabel fstreplace fstreverse
  fstreweight fstrmepsilon fstshortestdistance fstshortestpath fstsymbols
  fstsynchronize fsttopsort fstunion
)

# Create binary targets
foreach(BINARY ${FST_BINARIES})
  add_executable(${BINARY} src/bin/${BINARY}-main.cc src/bin/${BINARY}.cc)
  target_link_libraries(${BINARY} fstscript fst)
endforeach()
endif(ENABLE_FSTSCRIPT)

# Installation rules using generator expressions
include(GNUInstallDirs)
install(TARGETS fst fstscript
  EXPORT OpenFstTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY src/include/fst
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING PATTERN "*.h"
)

# Install binaries
if(ENABLE_FSTSCRIPT)
  install(TARGETS ${FST_BINARIES}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
endif()

# Export targets for usage in other projects
install(EXPORT OpenFstTargets
  FILE OpenFstTargets.cmake
  NAMESPACE OpenFst::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/OpenFst
)

# Testing setup
include(CTest)
option(BUILD_TESTING "Build and run tests" OFF)
if(BUILD_TESTING)
  enable_testing()
  
  # Define test sources
  set(FST_TEST_SRCS src/test/fst_test.cc)
  set(WEIGHT_TEST_SRCS src/test/weight_test.cc)
  set(ALGO_TEST_SRCS src/test/algo_test.cc)
  
  # Simple tests
  add_executable(fst_test ${FST_TEST_SRCS})
  target_link_libraries(fst_test PRIVATE fst fstscript)
  add_test(NAME fst_test COMMAND fst_test)
  
  add_executable(weight_test ${WEIGHT_TEST_SRCS})
  target_link_libraries(weight_test PRIVATE fst fstscript)
  add_test(NAME weight_test COMMAND weight_test)
  
  # Algo tests with different semiring types
  add_executable(algo_test_tropical ${ALGO_TEST_SRCS})
  target_compile_definitions(algo_test_tropical PRIVATE TEST_TROPICAL)
  target_link_libraries(algo_test_tropical PRIVATE fst fstscript)
  add_test(
    NAME algo_test_tropical 
    COMMAND algo_test_tropical 
  )
  
  add_executable(algo_test_log ${ALGO_TEST_SRCS})
  target_compile_definitions(algo_test_log PRIVATE TEST_LOG)
  target_link_libraries(algo_test_log PRIVATE fst fstscript)
  add_test(
    NAME algo_test_log 
    COMMAND algo_test_log 
  )
  
  add_executable(algo_test_minmax ${ALGO_TEST_SRCS})
  target_compile_definitions(algo_test_minmax PRIVATE TEST_MINMAX)
  target_link_libraries(algo_test_minmax PRIVATE fst fstscript)
  add_test(
    NAME algo_test_minmax 
    COMMAND algo_test_minmax 
  )
  
  add_executable(algo_test_lexicographic ${ALGO_TEST_SRCS})
  target_compile_definitions(algo_test_lexicographic PRIVATE TEST_LEXICOGRAPHIC)
  target_link_libraries(algo_test_lexicographic PRIVATE fst fstscript)
  add_test(
    NAME algo_test_lexicographic 
    COMMAND algo_test_lexicographic 
  )
  
  add_executable(algo_test_power ${ALGO_TEST_SRCS})
  target_compile_definitions(algo_test_power PRIVATE TEST_POWER)
  target_link_libraries(algo_test_power PRIVATE fst fstscript)
  add_test(
    NAME algo_test_power 
    COMMAND algo_test_power 
  )
  
  # Additional string arc tests from Makefile.am
  add_executable(algo_test_left_string ${ALGO_TEST_SRCS})
  target_compile_definitions(algo_test_left_string PRIVATE TEST_LEFT_STRING)
  target_link_libraries(algo_test_left_string PRIVATE fst fstscript)
  add_test(
    NAME algo_test_left_string 
    COMMAND algo_test_left_string 
  )
  
  add_executable(algo_test_right_string ${ALGO_TEST_SRCS})
  target_compile_definitions(algo_test_right_string PRIVATE TEST_RIGHT_STRING)
  target_link_libraries(algo_test_right_string PRIVATE fst fstscript)
  add_test(
    NAME algo_test_right_string 
    COMMAND algo_test_right_string 
  )
  
  add_executable(algo_test_gallic ${ALGO_TEST_SRCS})
  target_compile_definitions(algo_test_gallic PRIVATE TEST_GALLIC)
  target_link_libraries(algo_test_gallic PRIVATE fst fstscript)
  add_test(
    NAME algo_test_gallic 
    COMMAND algo_test_gallic
  )
endif()
