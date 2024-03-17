cmake_minimum_required(VERSION 3.10)

project(
    BatchedLUT
    VERSION 0.0.1 
    LANGUAGES C CXX
)

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

if (NOT DEFINED LUT_OUTPUT_SIZE) 
    set(LUT_OUTPUT_SIZE 16)
endif()

find_package(FMT REQUIRED)

add_subdirectory(src)
add_subdirectory(test)
