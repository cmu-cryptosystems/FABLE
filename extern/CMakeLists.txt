set(SCI_PATH ${PROJECT_SOURCE_DIR}/extern/EzPC/SCI)

find_package(SEAL 3.3.2 EXACT QUIET PATHS ${PROJECT_SOURCE_DIR}/build NO_DEFAULT_PATH)
if (NOT SEAL_FOUND)
    message(STATUS "SEAL 3.3.2 was not found: clone and install SEAL locally")
    find_package(Git REQUIRED)
    execute_process(
        COMMAND git apply ${PROJECT_SOURCE_DIR}/cmake/seal.patch --reverse --check
        WORKING_DIRECTORY ${SCI_PATH}/extern/SEAL
        OUTPUT_QUIET
        ERROR_VARIABLE PATCH_APPLIED_ERR
    )
    if (PATCH_APPLIED_ERR)
        message(STATUS "Applying Patch on SEAL")
        execute_process(
            COMMAND git apply ${PROJECT_SOURCE_DIR}/cmake/seal.patch
            WORKING_DIRECTORY ${SCI_PATH}/extern/SEAL
            OUTPUT_QUIET
            ERROR_QUIET
            COMMAND_ERROR_IS_FATAL ANY
        )
        message(STATUS "Patch applied to SEAL. ")
    endif()
    execute_process(COMMAND ${CMAKE_COMMAND} -DCMAKE_INSTALL_PREFIX=${PROJECT_SOURCE_DIR}/build .
            WORKING_DIRECTORY "${SCI_PATH}/extern/SEAL/native/src")
    execute_process(COMMAND ${CMAKE_COMMAND} --build . --target install --parallel
        WORKING_DIRECTORY "${SCI_PATH}/extern/SEAL/native/src")
    find_package(SEAL 3.3.2 EXACT REQUIRED PATHS "${PROJECT_SOURCE_DIR}/build/" NO_DEFAULT_PATH)
endif()

add_subdirectory(EzPC/SCI)