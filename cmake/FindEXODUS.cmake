# - Try to find libexodus
# Once done this will define
#  EXODUS_FOUND - System has libexodus
#  EXODUS_INCLUDE_DIRS - The libexodus include directories
#  EXODUS_LIBRARIES - The libraries needed to use libexodus

FIND_PATH(WITH_EXODUS_PREFIX
    NAMES include/exodus.h
)

FIND_LIBRARY(EXODUS_LIBRARIES
    NAMES exodus
    HINTS ${WITH_EXODUS_PREFIX}/lib
) 

FIND_PATH(EXODUS_INCLUDE_DIRS
    NAMES exodusII.h
    HINTS ${WITH_EXODUS_PREFIX}/include
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(EXODUS DEFAULT_MSG
    EXODUS_LIBRARIES
    EXODUS_INCLUDE_DIRS
)

# Hide these vars from ccmake GUI
MARK_AS_ADVANCED(
	EXODUS_LIBRARIES
	EXODUS_INCLUDE_DIRS
)
