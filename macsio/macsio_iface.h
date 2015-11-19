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

#warning ELIMINATE USE OF JSON HEADER FILE HERE
#include <json-cwx/json.h>

#define MACSIO_IFACE_MAX_COUNT 128
#define MACSIO_IFACE_MAX_NAME 64

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Entry methods of an I/O library Interace
 */

typedef void (*DumpFunc)(int argi, int argc, char **argv, json_object *main_obj, int dumpNum, double dumpTime);
typedef void (*LoadFunc)(int argi, int argc, char **argv, char const *path, json_object *main_obj, 
                             json_object **data_read_obj);
typedef int (*ProcessArgsFunc)  (int argi, int argc, char **argv);
typedef int (*QueryFeaturesFunc)(void);
typedef int (*IdentifyFileFunc) (char const *pathname);

#warning ALLOCATE MPI TAG IDS HERE TOO

#warning MAKE THE MAKEFILE LINK ANY .o FILES WITH A GIVEN NAME SCHEME
typedef struct MACSIO_IFACE_Handle_t
{   char                 name[MACSIO_IFACE_MAX_NAME];
    char                 ext[MACSIO_IFACE_MAX_NAME];
#warning DEFAULT FILE EXTENSION HERE
#warning Features: Async, compression, sif, grid types, uni-modal or bi-modal
    int                  slotUsed;
    ProcessArgsFunc      processArgsFunc;
    DumpFunc             dumpFunc;
    LoadFunc             loadFunc;
    QueryFeaturesFunc    queryFeaturesFunc;
    IdentifyFileFunc     identifyFileFunc;
} MACSIO_IFACE_Handle_t;

extern int                 MACSIO_IFACE_Register(MACSIO_IFACE_Handle_t const *iface);
extern void                MACSIO_IFACE_GetIds(int *cnt, int **ids);
extern void                MACSIO_IFACE_GetIdsMatchingFileExtension(int *cnt, int **ids, char const *ext);
extern int                 MACSIO_IFACE_GetId(char const *name);
extern char const         *MACSIO_IFACE_GetName(int id);
extern MACSIO_IFACE_Handle_t const *MACSIO_IFACE_GetByName(char const *name);
extern MACSIO_IFACE_Handle_t const *MACSIO_IFACE_GetById(int id);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _MACSIO_IFACE_H */
