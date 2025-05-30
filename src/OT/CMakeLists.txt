add_library(batchlut-OT
    silent_ot.cpp
    splut.cpp)
target_link_libraries(batchlut-OT PUBLIC batchlut-utils)
target_include_directories(batchlut-OT PUBLIC ${PROJECT_SOURCE_DIR}/src)
