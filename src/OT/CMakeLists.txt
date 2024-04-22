add_library(batchlut-OT
    silent_ot.cpp
    splut.cpp)
target_link_libraries(batchlut-OT
    PUBLIC SCI-OT batchlut-utils fmt::fmt oc::libOTe
)
target_include_directories(batchlut-OT PUBLIC ${PROJECT_SOURCE_DIR}/src)
