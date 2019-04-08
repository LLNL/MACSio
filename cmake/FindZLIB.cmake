# - Try to find zlib
# Once done this will define
#  ZLIB_FOUND - System has zlib
#  ZLIB_INCLUDE_DIRS - The zlib include directories
#  ZLIB_LIBRARIES - The libraries needed to use zlib

FIND_PATH(WITH_ZLIB_PREFIX
    NAMES include/zlib.h
)

FIND_LIBRARY(ZLIB_LIBRARIES
    NAMES z
    HINTS ${WITH_ZLIB_PREFIX}/lib
)

FIND_PATH(ZLIB_INCLUDE_DIRS
    NAMES zlib.h
    HINTS ${WITH_ZLIB_PREFIX}/include
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ZLIB DEFAULT_MSG
    ZLIB_LIBRARIES
    ZLIB_INCLUDE_DIRS
)

# Hide these vars from ccmake GUI
MARK_AS_ADVANCED(
	ZLIB_LIBRARIES
	ZLIB_INCLUDE_DIRS
)
