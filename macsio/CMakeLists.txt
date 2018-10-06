# Header files need not be enumerated
INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR})

# Source files
SET(mio_srcs
    macsio_clargs.c
    macsio_mif.c
    macsio_msf.c
    macsio_iface.c
    macsio_timing.c
    macsio_utils.c
    macsio_log.c
    macsio_data.c
    macsio_work.c
    macsio_main.c
)

SET_SOURCE_FILES_PROPERTIES(${mio_srcs} PROPERTIES LANGUAGE CXX)
SET_SOURCE_FILES_PROPERTIES(${PLUGIN_SRCS} PROPERTIES LANGUAGE CXX)

ADD_EXECUTABLE(macsio ${mio_srcs} ${PLUGIN_SRCS})
ADD_EXECUTABLE(tstlog tstlog.c macsio_log.c)
ADD_EXECUTABLE(tsttiming tsttiming.c macsio_timing.c macsio_log.c macsio_utils.c)
ADD_EXECUTABLE(tstprng tstprng.c macsio_data.c macsio_utils.c)

IF(ENABLE_MPI)
    SET_TARGET_PROPERTIES(macsio PROPERTIES COMPILE_DEFINITIONS "HAVE_MPI")
    SET_TARGET_PROPERTIES(tstlog PROPERTIES COMPILE_DEFINITIONS "HAVE_MPI")
    SET_TARGET_PROPERTIES(tsttiming PROPERTIES COMPILE_DEFINITIONS "HAVE_MPI")
    SET_TARGET_PROPERTIES(tstprng PROPERTIES COMPILE_DEFINITIONS "HAVE_MPI")
ENDIF(ENABLE_MPI)
TARGET_LINK_LIBRARIES(macsio ${MIO_EXTERNAL_LIBS})
TARGET_LINK_LIBRARIES(tstlog ${MIO_EXTERNAL_LIBS})
TARGET_LINK_LIBRARIES(tsttiming ${MIO_EXTERNAL_LIBS})
TARGET_LINK_LIBRARIES(tstprng ${MIO_EXTERNAL_LIBS})

INSTALL(TARGETS macsio RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})
