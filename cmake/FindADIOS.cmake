# - Try to find libadios
# Once done this will define
#  ADIOS_FOUND - System has libadios
#  ADIOS_INCLUDE_DIRS - The libadios include directories
#  ADIOS_LIBRARIES - The libraries needed to use libadios

FIND_PATH(WITH_ADIOS_PREFIX
    NAMES include/adios.h
)

FIND_LIBRARY(ADIOS_LIBRARIES
    NAMES adios
    HINTS ${WITH_ADIOS_PREFIX}/lib
)

FIND_PATH(ADIOS_INCLUDE_DIRS
    NAMES adios.h
    HINTS ${WITH_ADIOS_PREFIX}/include
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ADIOS DEFAULT_MSG
    ADIOS_LIBRARIES
    ADIOS_INCLUDE_DIRS
)

# Hide these vars from ccmake GUI
MARK_AS_ADVANCED(
	ADIOS_LIBRARIES
	ADIOS_INCLUDE_DIRS
)
