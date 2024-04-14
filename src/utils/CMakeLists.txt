add_library(batchlut-utils
    io_utils.cpp)
target_link_libraries(batchlut-utils
    PUBLIC fmt::fmt SCI-OT oc::libOTe
)
