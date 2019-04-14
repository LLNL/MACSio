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

#
# I think language CXX should only be needed on plugins due to the
# one statement on plugin's involving static initializers
#
SET_SOURCE_FILES_PROPERTIES(${mio_srcs} PROPERTIES LANGUAGE CXX)
SET_SOURCE_FILES_PROPERTIES(${PLUGIN_SRCS} PROPERTIES LANGUAGE CXX)

ADD_EXECUTABLE(macsio ${mio_srcs} ${PLUGIN_SRCS})
ADD_EXECUTABLE(tstlog tstlog.c macsio_log.c)
ADD_EXECUTABLE(tsttiming tsttiming.c macsio_timing.c macsio_log.c macsio_utils.c)
ADD_EXECUTABLE(tstprng tstprng.c macsio_data.c macsio_utils.c)
ADD_EXECUTABLE(tstclargs tstclargs.c macsio_clargs.c macsio_log.c macsio_utils.c)

IF(ENABLE_MPI)
    SET_TARGET_PROPERTIES(macsio PROPERTIES COMPILE_DEFINITIONS "HAVE_MPI")
    SET_TARGET_PROPERTIES(tstlog PROPERTIES COMPILE_DEFINITIONS "HAVE_MPI")
    SET_TARGET_PROPERTIES(tsttiming PROPERTIES COMPILE_DEFINITIONS "HAVE_MPI")
    SET_TARGET_PROPERTIES(tstprng PROPERTIES COMPILE_DEFINITIONS "HAVE_MPI")
    SET_TARGET_PROPERTIES(tstclargs PROPERTIES COMPILE_DEFINITIONS "HAVE_MPI")
ENDIF(ENABLE_MPI)
TARGET_LINK_LIBRARIES(macsio ${MIO_EXTERNAL_LIBS})
TARGET_LINK_LIBRARIES(tstlog ${MIO_EXTERNAL_LIBS})
TARGET_LINK_LIBRARIES(tsttiming ${MIO_EXTERNAL_LIBS})
TARGET_LINK_LIBRARIES(tstprng ${MIO_EXTERNAL_LIBS})
TARGET_LINK_LIBRARIES(tstclargs ${MIO_EXTERNAL_LIBS})

IF(ENABLE_MPI)
    SET(TEST_RUN ${MPIEXEC} ${MPIEXEC_NUMPROC_FLAG} 3)
ELSE(ENABLE_MPI)
    SET(TEST_RUN "")
ENDIF(ENABLE_MPI)
ADD_TEST(NAME tstlog COMMAND ${TEST_RUN} ./tstlog)
ADD_TEST(NAME tsttiming COMMAND ${TEST_RUN} ./tsttiming)
ADD_TEST(NAME tstprng COMMAND ${TEST_RUN} ./tstprng)
ADD_TEST(NAME tstclargs COMMAND ${TEST_RUN} ./tstclargs)
ADD_TEST(NAME miftmpl COMMAND ${TEST_RUN} ./macsio)
ADD_TEST(NAME silo COMMAND ${TEST_RUN} ./macsio --interface silo)
ADD_TEST(NAME pdb COMMAND ${TEST_RUN} ./macsio --interface pdb)
ADD_TEST(NAME hdf5 COMMAND ${TEST_RUN} ./macsio --interface hdf5)

INSTALL(TARGETS macsio RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX})

#
# This is to force test/check target to depend on changes to test execs
#
ADD_CUSTOM_TARGET(check COMMAND ${CMAKE_CTEST_COMMAND}
                  DEPENDS tstlog tsttiming tstprng tstclargs)
