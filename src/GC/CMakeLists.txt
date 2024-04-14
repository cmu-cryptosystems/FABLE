add_library(batchlut-GC
    sort.cpp
    orcompact.cpp
    deduplicate.cpp
    lowmc.cpp
    subcube_query.cpp)
target_link_libraries(batchlut-GC
    PUBLIC SCI-GC batchlut-utils fmt::fmt
)
target_include_directories(batchlut-GC PUBLIC ${PROJECT_SOURCE_DIR}/src)
