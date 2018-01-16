# - Try to find libjson-cwx
# Once done this will define
#  JSON-CWX_FOUND - System has libjson-cwx
#  JSON-CWX_INCLUDE_DIRS - The libjson-cwx include directories
#  JSON-CWX_LIBRARIES - The libraries needed to use libjson-cwx

FIND_PATH(WITH_JSON-CWX_PREFIX
    NAMES include/json-cwx/json.h
)

FIND_LIBRARY(JSON-CWX_LIBRARIES
    NAMES json-cwx
    HINTS ${WITH_JSON-CWX_PREFIX}/lib
)

FIND_PATH(JSON-CWX_INCLUDE_DIRS
    NAMES json-cwx/json.h
    HINTS ${WITH_JSON-CWX_PREFIX}/include
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(JSON-CWX DEFAULT_MSG
    JSON-CWX_LIBRARIES
    JSON-CWX_INCLUDE_DIRS
)

# Hide these vars from ccmake GUI
MARK_AS_ADVANCED(
	JSON-CWX_LIBRARIES
	JSON-CWX_INCLUDE_DIRS
)
