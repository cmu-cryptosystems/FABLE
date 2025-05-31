add_library(fable-utils
    io_utils.cpp)
target_link_libraries(fable-utils
    PUBLIC fmt::fmt oc::libOTe SCI-OT
)
target_compile_features(fable-utils PUBLIC cxx_std_20)
