add_library(batchlut-utils
    io_utils.cpp)
target_link_libraries(batchlut-utils
    PUBLIC fmt::fmt oc::libOTe SCI-OT
)
target_compile_features(batchlut-utils PUBLIC cxx_std_20)
