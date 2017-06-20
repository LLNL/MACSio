# Header files need not be enumerated
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

# Source files
SET(mio_srcs
    macsio_clargs.c
    macsio_mif.c
    macsio_iface.c
    macsio_timing.c
    macsio_utils.c
    macsio_log.c
    macsio_data.c
    macsio_main.c
)

SET_SOURCE_FILES_PROPERTIES(${mio_srcs} PROPERTIES LANGUAGE CXX)

ADD_EXECUTABLE(macsio ${mio_srcs} ${PLUGIN_SRCS})
TARGET_LINK_LIBRARIES(macsio ${MIO_EXTERNAL_LIBS})
