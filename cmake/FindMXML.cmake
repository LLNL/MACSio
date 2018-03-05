# - Try to find libmxml
# Once done this will define
#  MXML_FOUND - System has libmxml
#  MXML_INCLUDE_DIRS - The libmxml include directories
#  MXML_LIBRARIES - The libraries needed to use libmxml

FIND_PATH(WITH_MXML_PREFIX
    NAMES include/mxml.h
)

FIND_LIBRARY(MXML_LIBRARIES
    NAMES mxml
    HINTS ${WITH_MXML_PREFIX}/lib
)

FIND_PATH(MXML_INCLUDE_DIRS
    NAMES mxml.h
    HINTS ${WITH_MXML_PREFIX}/include
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(MXML DEFAULT_MSG
    MXML_LIBRARIES
    MXML_INCLUDE_DIRS
)

# Hide these vars from ccmake GUI
MARK_AS_ADVANCED(
	MXML_LIBRARIES
	MXML_INCLUDE_DIRS
)
