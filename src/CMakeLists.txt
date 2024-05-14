add_subdirectory(utils)
add_subdirectory(GC)
add_subdirectory(OT)

file(GLOB SOURCES "${PROJECT_SOURCE_DIR}/extern/BatchPIR/src/*.cpp")
list(REMOVE_ITEM SOURCES "${PROJECT_SOURCE_DIR}/extern/BatchPIR/src/main.cpp")
add_executable(batchlut "batchlut.cpp" ${SOURCES})

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
target_link_libraries(batchlut batchlut-GC fmt::fmt SEAL::seal OpenMP::OpenMP_CXX oc::libOTe)
target_include_directories(batchlut PRIVATE ${PROJECT_SOURCE_DIR}/extern/BatchPIR/header)
add_compile_definitions(batchlut
    LUT_INPUT_SIZE=${LUT_INPUT_SIZE} 
    LUT_OUTPUT_SIZE=${LUT_OUTPUT_SIZE}
)

add_executable(flutegen "gen_flute_lut.cpp" ${SOURCES})
target_include_directories(flutegen PRIVATE ${PROJECT_SOURCE_DIR}/extern/BatchPIR/header)
target_link_libraries(flutegen batchlut-GC fmt::fmt SEAL::seal)
file(MAKE_DIRECTORY ${PROJECT_SOURCE_DIR}/flute_luts)