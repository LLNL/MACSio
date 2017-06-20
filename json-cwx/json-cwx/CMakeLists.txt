PROJECT(JSON-CWX C)

INCLUDE(GNUInstallDirs)

LIST(APPEND install_headers
    json.h
)
INSTALL(FILES ${install_headers} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/json-cwx)

LIST(APPEND srcs
    arraylist.c
    debug.c
    json_c_version.c
    json_crc.c
    json_object.c
    json_object_iterator.c
    json_tokener.c
    json_util.c
    linkhash.c
    printbuf.c
    random_seed.c
)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

# Build Library
ADD_LIBRARY(json-cwx-shared SHARED ${srcs})
SET_TARGET_PROPERTIES(json-cwx-shared PROPERTIES OUTPUT_NAME json-cwx CLEAN_DIRECT_OUTPUT 1)
INSTALL(TARGETS json-cwx-shared DESTINATION ${CMAKE_INSTALL_LIBDIR})

ADD_LIBRARY(json-cwx STATIC ${srcs})
SET_TARGET_PROPERTIES(json-cwx PROPERTIES OUTPUT_NAME json-cwx CLEAN_DIRECT_OUTPUT 1)
INSTALL(TARGETS json-cwx DESTINATION ${CMAKE_INSTALL_LIBDIR})

## TODO: CMakeLists for test directory
OPTION(ENABLE_TESTS OFF)
IF(ENABLE_TESTS)
    ADD_SUBDIRECTORY(tests)
ENDIF(ENABLE_TESTS)

## TODO: files to configure
# config.h
# json_config.h
# json-cwx.pc
# json-cwx-uninstalled.pc
