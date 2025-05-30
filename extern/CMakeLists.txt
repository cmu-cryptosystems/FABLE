set(SCI_PATH ${PROJECT_SOURCE_DIR}/extern/EzPC/SCI)

find_package(SEAL 3.3.2 EXACT QUIET PATHS ${PROJECT_SOURCE_DIR}/build NO_DEFAULT_PATH)
if (NOT SEAL_FOUND)
    message(STATUS "SEAL 3.3.2 was not found: clone and install SEAL locally")
    execute_process(COMMAND ${CMAKE_COMMAND} -DCMAKE_INSTALL_PREFIX=${PROJECT_SOURCE_DIR}/build .
            WORKING_DIRECTORY "${SCI_PATH}/extern/SEAL/native/src")
    execute_process(COMMAND ${CMAKE_COMMAND} --build . --target install --parallel
        WORKING_DIRECTORY "${SCI_PATH}/extern/SEAL/native/src")
    find_package(SEAL 3.3.2 EXACT REQUIRED PATHS "${PROJECT_SOURCE_DIR}/build/" NO_DEFAULT_PATH)
endif()

add_subdirectory(EzPC/SCI)