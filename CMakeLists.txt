cmake_minimum_required(VERSION 3.10)

project(
    BatchedLUT
    VERSION 0.0.1 
    LANGUAGES C CXX
)

execute_process(COMMAND (git apply "${PROJECT_SOURCE_DIR}/cmake/ezpc.patch") || (git apply "${PROJECT_SOURCE_DIR}/cmake/ezpc.patch" --reverse --check)
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/extern/EzPC")
execute_process(COMMAND (git apply "${PROJECT_SOURCE_DIR}/cmake/seal.patch") || (git apply "${PROJECT_SOURCE_DIR}/cmake/seal.patch" --reverse --check)
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/extern/EzPC/SCI/extern/SEAL")

add_subdirectory(extern)

# Write built executables and libraries to bin/ and lib/, respectively.
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
endif()
if(NOT CMAKE_LIBRARY_OUTPUT_DIRECTORY)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
endif()
if(NOT CMAKE_ARCHIVE_OUTPUT_DIRECTORY)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib")
endif()

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

if (NOT DEFINED LUT_INPUT_SIZE) 
    set(LUT_INPUT_SIZE 20)
endif()
if (NOT DEFINED LUT_OUTPUT_SIZE) 
    set(LUT_OUTPUT_SIZE 20)
endif()

find_package(FMT REQUIRED)
find_package(libOTe REQUIRED)

add_subdirectory(src)
add_subdirectory(test)
