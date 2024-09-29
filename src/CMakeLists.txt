add_compile_definitions(
    LUT_INPUT_SIZE=${LUT_INPUT_SIZE} 
    LUT_OUTPUT_SIZE=${LUT_OUTPUT_SIZE}
)

add_subdirectory(utils)
add_subdirectory(GC)
add_subdirectory(OT)

add_executable(batchlut "batchlut.cpp")
target_link_libraries(batchlut batchlut-GC)

add_executable(splut "bench_splut.cpp")
target_link_libraries(splut batchlut-GC batchlut-OT) 

add_executable(flutegen "gen_flute_lut.cpp" ${SOURCES})
target_link_libraries(flutegen batchlut-GC)
file(MAKE_DIRECTORY ${PROJECT_SOURCE_DIR}/flute_luts)

add_subdirectory(applications)