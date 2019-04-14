#ifndef _MACSIO_IFACE_H
#define _MACSIO_IFACE_H
/*
Copyright (c) 2015, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.
Written by Mark C. Miller

LLNL-CODE-676051. All rights reserved.

This file is part of MACSio

Please also read the LICENSE file at the top of the source code directory or
folder hierarchy.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License (as published by the Free Software
Foundation) version 2, dated June 1991.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

//#warning ELIMINATE USE OF JSON HEADER FILE HERE
#include <json-cwx/json.h>

/*!
\defgroup MACSIO_IFACE MACSIO_IFACE
\brief Plugin Interface Specification

@{
*/


/*! \brief Maximum number of plugins possible */
#define MACSIO_IFACE_MAX_COUNT 128

/*! \brief Maximum length of any plugin's name */
#define MACSIO_IFACE_MAX_NAME 64

#ifdef __cplusplus
extern "C" {
#endif

/*! \brief Main mesh+field dump (write) function specification */
typedef void (*DumpFunc)(
    int argi, /**< [in] index of argv at which to start processing args */
    int argc, /**< [in] \c argc from main */
    char **argv, /**< [in] \c argv from main */
    json_object *main_obj, /**< [in] the main json data object to be dumped */
    int dumpNum, /**< [in] like a "cycle number" for the dump */
    double dumpTime /**< [in] like "time" for the dump */
);

/*! \brief Main mesh+field load (read) function specification */
typedef void (*LoadFunc)(
    int argi, /**< [in] index of argv at which to start processing args */
    int argc, /**< [in] \c argc from main */
    char **argv, /**< [in] \c argv from main */
    char const *path, /**< [in] file(s) to read data from */
    json_object *main_obj,
    json_object **data_read_obj /**< [out] returned data object read by the plugin */
);

/*! \brief Command-line argument processing function for plugin */
typedef int (*ProcessArgsFunc)(
    int argi, /**< [in] arg index at which to start processing */
    int argc, /**< [in] \c argc from main */
    char **argv /**< [in] \c argv from main */
);

/*! \brief Function to query plugin's features (not currently used) */
typedef int (*QueryFeaturesFunc)(void);

/*! \brief Function to ask plugin if it recognizes a given file */
typedef int (*IdentifyFileFunc)(
    char const *pathname /**< file(s) to be identified */
);

//#warning ALLOCATE MPI TAG IDS HERE TOO

/*! \brief Plugin interface handle type */
typedef struct MACSIO_IFACE_Handle_t
{   char                 name[MACSIO_IFACE_MAX_NAME]; /**< Name of this plugin */
    char                 ext[MACSIO_IFACE_MAX_NAME];  /**< File extension of files associated with this plugin */
//#warning DEFAULT FILE EXTENSION HERE
//#warning Features: Async, compression, sif, grid types, uni-modal or bi-modal
    int                  slotUsed;                    /**< [Internal] indicate if this position in table is used */
    ProcessArgsFunc      processArgsFunc;             /**< Plugin's command-line argument processing callback */
    DumpFunc             dumpFunc;                    /**< Plugin's main dump (write) function callback */
    LoadFunc             loadFunc;                    /**< Plugin's main load (read) function callback */
    QueryFeaturesFunc    queryFeaturesFunc;           /**< Plugin's callback to query its feature set (not in use) */
    IdentifyFileFunc     identifyFileFunc;            /**< Plugin's callback to indicate if it thinks it owns a file */
} MACSIO_IFACE_Handle_t;

/*! \brief Register a plugin with MACSIO

This function should be called from within a code-block which is executed as part
of a static initiliztion of the plugin's local, static variables like so...

\code
static int register_this_plugin(void)
{
    MACSIO_IFACE_Handle_t iface;
    iface.name = "foobar";
    iface.ext = ".fb";
    .
    .
    .
    MACSIO_IFACE_Register(&iface);
    return 0;
}
static int dummy = register_this_plugin()
\endcode

*/
extern int
MACSIO_IFACE_Register(
    MACSIO_IFACE_Handle_t const *iface /**< The interface specification for a new plugin */
);

/*! \brief Return all registered plugin ids */
extern void
MACSIO_IFACE_GetIds(
    int *cnt, /**< [out] the number of registered plugins available */
    int **ids /**< [out] the list of ids of the registered plugins */
);

/*! \brief Return plugin that \em recognize files with a given extension */
extern void
MACSIO_IFACE_GetIdsMatchingFileExtension(
    char const *ext, /**< [in] the file extension to query */
    int *cnt, /**< [out] the number of plugins that recognize the extension */
    int **ids /**< [out] the list of ids of the matching plugins */
);

/*!
\brief return id of plugin given its name
\return On error, returns -1. Otheriwse a non-negative valid plugin id.
*/
extern int
MACSIO_IFACE_GetId(
    char const *name
);

/*!
\brief Get name of plugin given its id
\return On error, returns (char*)0. Otheriwse a non-null name of valid plugin.
*/
extern char const *
MACSIO_IFACE_GetName(
    int id
);

/*!
\brief Get interface handle for a plugin given its name
\return On error, returns (MACSIO_IFACE_Handle_t*)0. Otherwise returns a
valid plugin interface handle.
*/
extern MACSIO_IFACE_Handle_t const *
MACSIO_IFACE_GetByName(
    char const *name
);

/*!
\brief Get interface handle for a plugin given its id
\return On error, returns (MACSIO_IFACE_Handle_t*)0. Otherwise returns a
valid plugin interface handle.
*/
extern MACSIO_IFACE_Handle_t const *
MACSIO_IFACE_GetById(
    int id
);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _MACSIO_IFACE_H */
