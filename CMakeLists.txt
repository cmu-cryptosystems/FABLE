cmake_minimum_required(VERSION 3.10)

project(
    BatchedLUT
    VERSION 0.0.1 
    LANGUAGES C CXX
)

message (STATUS "Checking if patches are applied...")

# Check if git is available
find_package(Git REQUIRED)

execute_process(
    COMMAND git apply ${PROJECT_SOURCE_DIR}/cmake/ezpc.patch --reverse --check
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/extern/EzPC
    OUTPUT_QUIET
    ERROR_VARIABLE PATCH_APPLIED_ERR
)
if (PATCH_APPLIED_ERR)
    message(STATUS "Applying Patch on EzPC")
    execute_process(
        COMMAND git apply ${PROJECT_SOURCE_DIR}/cmake/ezpc.patch
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/extern/EzPC
        OUTPUT_QUIET
        ERROR_QUIET
        COMMAND_ERROR_IS_FATAL ANY
    )
    message(STATUS "Patch applied to EzPC. ")
endif()

execute_process(
    COMMAND git apply ${PROJECT_SOURCE_DIR}/cmake/seal.patch --reverse --check
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/extern/EzPC/SCI/extern/SEAL
    OUTPUT_QUIET
    ERROR_VARIABLE PATCH_APPLIED_ERR
)
if (PATCH_APPLIED_ERR)
    message(STATUS "Applying Patch on SEAL")
    execute_process(
        COMMAND git apply ${PROJECT_SOURCE_DIR}/cmake/seal.patch
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}/extern/EzPC/SCI/extern/SEAL
        OUTPUT_QUIET
        ERROR_QUIET
        COMMAND_ERROR_IS_FATAL ANY
    )
    message(STATUS "Patch applied to SEAL. ")
endif()

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
if (NOT DEFINED LUT_MAX_LOG_SIZE) 
    set(LUT_MAX_LOG_SIZE 20)
endif()

find_package(FMT REQUIRED)
find_package(libOTe REQUIRED)

add_subdirectory(src)
add_subdirectory(test)
