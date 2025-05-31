# BatchPIR
file(GLOB SOURCES "${PROJECT_SOURCE_DIR}/extern/BatchPIR/src/*.cpp")
list(REMOVE_ITEM SOURCES "${PROJECT_SOURCE_DIR}/extern/BatchPIR/src/main.cpp")

find_package(SEAL 4.1 QUIET PATHS "${PROJECT_SOURCE_DIR}/build/seal_4_1" NO_DEFAULT_PATH)

if (NOT SEAL_FOUND)
    message(STATUS "clone and install SEAL locally")
    execute_process(COMMAND ${CMAKE_COMMAND} -E env CC=clang CXX=clang++ ${CMAKE_COMMAND} -S . -B ${PROJECT_SOURCE_DIR}/extern/SEAL/build -DSEAL_USE_GAUSSIAN_NOISE=ON -DCMAKE_INSTALL_PREFIX=${PROJECT_SOURCE_DIR}/build/seal_4_1
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/extern/SEAL")
    execute_process(COMMAND ${CMAKE_COMMAND} --build ./build --target install --parallel
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/extern/SEAL")
    find_package(SEAL 4.1 REQUIRED PATHS "${PROJECT_SOURCE_DIR}/build/seal_4_1" NO_DEFAULT_PATH)
endif()

find_package(OpenMP REQUIRED)

add_library(fable-GC
    sort.cpp
    orcompact.cpp
    deduplicate.cpp
    lowmc.cpp
    subcube_query.cpp
    aes.cpp
    lookup.cpp
    ${SOURCES})
target_link_libraries(fable-GC
    PUBLIC SCI-GC fable-utils fmt::fmt oc::libOTe SEAL::seal OpenMP::OpenMP_CXX
)
target_include_directories(fable-GC PUBLIC ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/extern/BatchPIR/header)
target_compile_options(fable-GC PUBLIC -rdynamic)