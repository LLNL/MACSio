# - Try to find libnetcdf
# Once done this will define
#  NETCDF_FOUND - System has libnetcdf
#  NETCDF_INCLUDE_DIRS - The libnetcdf include directories
#  NETCDF_LIBRARIES - The libraries needed to use libnetcdf

FIND_PATH(WITH_NETCDF_PREFIX
    NAMES include/netcdf.h
) 
FIND_LIBRARY(NETCDF_LIBRARIES
    NAMES netcdf
    HINTS ${WITH_NETCDF_PREFIX}/lib
)
FIND_PATH(NETCDF_INCLUDE_DIRS
    NAMES netcdf.h
    HINTS ${WITH_NETCDF_PREFIX}/include
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(NETCDF DEFAULT_MSG
    NETCDF_LIBRARIES
    NETCDF_INCLUDE_DIRS
)

# Hide these vars from ccmake GUI
MARK_AS_ADVANCED(
    NETCDF_LIBRARIES
    NETCDF_INCLUDE_DIRS
)
