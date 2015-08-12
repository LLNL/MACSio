#ifndef _MACSIO_CLAGS_H
#define _MACSIO_CLAGS_H
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* error modes */
#define MACSIO_CLARGS_WARN 0
#define MACSIO_CLARGS_ERROR 1

/* route modes */
#define MACSIO_CLARGS_TOMEM     0 
#define MACSIO_CLARGS_TOJSON    1

/* default modes */
#define MACSIO_CLARGS_ASSIGN_OFF 0
#define MACSIO_CLARGS_ASSIGN_ON 0

#define MACSIO_CLARGS_HELP  -1
#define MACSIO_CLARGS_OK 0
#define MACSIO_CLARGS_GRP_SEP_STR "macsio_args_group_"
#define MACSIO_CLARGS_GRP_BEG MACSIO_CLARGS_GRP_SEP_STR "beg_"
#define MACSIO_CLARGS_GRP_END MACSIO_CLARGS_GRP_SEP_STR "end_"
#define MACSIO_CLARGS_ARG_GROUP_BEG(GRPNAME) MACSIO_CLARGS_GRP_BEG #GRPNAME, MACSIO_CLARGS_NODEFAULT
#define MACSIO_CLARGS_ARG_GROUP_END(GRPNAME) MACSIO_CLARGS_GRP_END #GRPNAME, MACSIO_CLARGS_NODEFAULT, ""
#define MACSIO_CLARGS_END_OF_ARGS "macsio_end_of_args"
#define MACSIO_CLARGS_NODEFAULT (void*)0

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _MACSIO_CLARGS_ArgvFlags_t
{
    unsigned int error_mode    : 1;
    unsigned int route_mode    : 2;
    unsigned int defaults_mode : 1;
} MACSIO_CLARGS_ArgvFlags_t;

#warning RE-THINK THESE NAMES
extern int MACSIO_CLARGS_ProcessCmdline(void **retobj, MACSIO_CLARGS_ArgvFlags_t flags, int argi, int argc, char **argv, ...);

#ifdef __cplusplus
}
#endif

#endif /* _UTIL_H */
