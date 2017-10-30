# - Try to find libszip
# Once done this will define
#  SZIP_FOUND - System has libszip
#  SZIP_INCLUDE_DIRS - The libszip include directories
#  SZIP_LIBRARIES - The libraries needed to use libszip

FIND_PATH(WITH_SZIP_PREFIX
    NAMES include/szlib.h
)

FIND_LIBRARY(SZIP_LIBRARIES
    NAMES sz
    HINTS ${WITH_SZIP_PREFIX}/lib
)

FIND_PATH(SZIP_INCLUDE_DIRS
    NAMES szlib.h
    HINTS ${WITH_SZIP_PREFIX}/include
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(SZIP DEFAULT_MSG
    SZIP_LIBRARIES
    SZIP_INCLUDE_DIRS
)

# Hide these vars from ccmake GUI
MARK_AS_ADVANCED(
	SZIP_LIBRARIES
	SZIP_INCLUDE_DIRS
)
