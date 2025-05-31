add_library(fable-OT
    silent_ot.cpp
    splut.cpp)
target_link_libraries(fable-OT PUBLIC fable-utils)
target_include_directories(fable-OT PUBLIC ${PROJECT_SOURCE_DIR}/src)
