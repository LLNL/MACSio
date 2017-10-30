# - Try to find libzfp
# Once done this will define
#  ZFP_FOUND - System has libzfp
#  ZFP_INCLUDE_DIRS - The libzfp include directories
#  ZFP_LIBRARIES - The libraries needed to use libzfp

FIND_PATH(WITH_ZFP_PREFIX
    NAMES include/zfp.h
)

FIND_LIBRARY(ZFP_LIBRARIES
    NAMES zfp
    HINTS ${WITH_ZFP_PREFIX}/lib
)

FIND_PATH(ZFP_INCLUDE_DIRS
    NAMES zfp.h
    HINTS ${WITH_ZFP_PREFIX}/include
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(ZFP DEFAULT_MSG
    ZFP_LIBRARIES
    ZFP_INCLUDE_DIRS
)

# Hide these vars from ccmake GUI
MARK_AS_ADVANCED(
	ZFP_LIBRARIES
	ZFP_INCLUDE_DIRS
)
