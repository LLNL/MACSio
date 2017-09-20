#ifndef _MACSIO_H
#define _MACSIO_H
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

#include <options.h>

#include <stddef.h>
#include <stdlib.h>

#define MACSIO_MAX_NAME 16 
#define MACSIO_MAX_NS_DEPTH 10
#define MACSIO_MAX_ABSNAME (MACSIO_MAX_NS_DEPTH*(MACSIO_MAX_NAME+1))

#ifdef __cplusplus
extern "C" {
#endif

//#warning REMOVE THIS TYPEDEF
typedef struct MACSIO_FileHandlePublic_t
{
    /* Public Data Members */

    /* Public Method Members */

    /* File level methods (not 'create' and 'open' are part of iface_info_t */
    int (*closeFileFunc)(struct MACSIO_FileHandle_t *fh, MACSIO_optlist_t const *moreopts);
    int (*syncMetaFunc)(struct MACSIO_FileHandle_t *fh, MACSIO_optlist_t const *moreopts);
    int (*syncDataFunc)(struct MACSIO_FileHandle_t *fh, MACSIO_optlist_t const *moreopts);

    /* Namespace methods */
    int (*createNamespaceFunc)(struct MACSIO_FileHandle_t *fh, char const *nsname, MACSIO_optlist_t const *moreopts);
    char const *(*setNamespaceFunc)(struct MACSIO_FileHandle_t *fh, char const *nsname, MACSIO_optlist_t const *moreopts);
    char const *(*getNamespaceFunc)(struct MACSIO_FileHandle_t *fh, MACSIO_optlist_t const *moreopts);

    /* Named array methods */
    int (*createArrayFunc)(struct MACSIO_FileHandle_t *fh, char const *arrname, int type,
        int const dims[4], MACSIO_optlist_t const *moreopts);
    /* use 'init' and 'next' as special names to manage iteration over all arrays */
    int (*getArrayInfoFunc)(struct MACSIO_FileHandle_t *fh, char const *arrname, int *type, int *dims[4], MACSIO_optlist_t const *moreopts);
    /* use a buf that points to null to indicate a read and a buf that points to non-null as a write */
    int (*defineArrayPartFunc)(struct MACSIO_FileHandle_t *fh, char const *arrname,
        int const starts[4], int const counts[4], int strides[4], void **buf, MACSIO_optlist_t const *moreopts);
    int (*startPendingArraysFunc)(struct MACSIO_FileHandle_t *fh);
    int (*finishPendingArraysFunc)(struct MACSIO_FileHandle_t *fh);

} MACSIO_FileHandlePublic_t;

typedef struct MACSIO_FileHandle_t {
    MACSIO_FileHandlePublic_t pub;
    /* private part follows per I/O-lib driver */
} MACSIO_FileHandle_t;

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _MACSIO_H */
