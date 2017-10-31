# - Try to find libtyphonio
# Once done this will define
#  TYPHONIO_FOUND - System has libtyphonio
#  TYPHONIO_INCLUDE_DIRS - The libtyphonio include directories
#  TYPHONIO_LIBRARIES - The libraries needed to use libtyphonio

FIND_PATH(WITH_TYPHONIO_PREFIX
    NAMES include/typhonio.h
)

FIND_LIBRARY(TYPHONIO_LIBRARIES
    NAMES typhonio
    HINTS ${WITH_TYPHONIO_PREFIX}/lib
)

FIND_PATH(TYPHONIO_INCLUDE_DIRS
    NAMES typhonio.h
    HINTS ${WITH_TYPHONIO_PREFIX}/include
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(TYPHONIO DEFAULT_MSG
    TYPHONIO_LIBRARIES
    TYPHONIO_INCLUDE_DIRS
)

# Hide these vars from ccmake GUI
MARK_AS_ADVANCED(
	TYPHONIO_LIBRARIES
	TYPHONIO_INCLUDE_DIRS
)
