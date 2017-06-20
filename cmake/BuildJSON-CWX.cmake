##
## Build JSON-CWX at *configure* time
## Using CMake ExternalProject
##
 
CONFIGURE_FILE(json-cwx/CMakeLists.txt.in json-cwx/CMakeLists.txt)

EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
    RESULT_VARIABLE   result
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/json-cwx
)

IF(result)
    MESSAGE(FATAL_ERROR "CMake step for json-cwx failed: ${result}")
ENDIF(result)

EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} --build .
    RESULT_VARIABLE   result
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/json-cwx
)

IF(result)
    MESSAGE(FATAL_ERROR "Build step for json-cwx failed: ${result}")
ENDIF(result)

# Add json-cwx directories. Defines libjson-cwx and libjson-cwx-shared targets.
ADD_SUBDIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/json-cwx/json-cwx)
#ADD_SUBDIRECTORY(${CMAKE_CURRENT_BINARY_DIR}/json-cwx/build)
