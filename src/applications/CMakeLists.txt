add_executable(embedding "embedding.cpp")
target_link_libraries(embedding fable-GC)

add_executable(join "join.cpp")
target_link_libraries(join fable-GC)