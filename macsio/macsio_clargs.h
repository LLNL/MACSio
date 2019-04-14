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

/*!
\defgroup MACSIO_CLARGS MACSIO_CLARGS
\brief Command-line argument parsing utilities 

@{
*/

/* error modes */
#define MACSIO_CLARGS_WARN 0
#define MACSIO_CLARGS_ERROR 1

/* route modes */
#define MACSIO_CLARGS_TOMEM     0 
#define MACSIO_CLARGS_TOJSON    1

/* default modes */
#define MACSIO_CLARGS_ASSIGN_OFF 0
#define MACSIO_CLARGS_ASSIGN_ON 1

#define MACSIO_CLARGS_HELP  -1
#define MACSIO_CLARGS_OK 0

#define MACSIO_CLARGS_GRP_SEP_STR "macsio_args_group_"
#define MACSIO_CLARGS_GRP_BEG MACSIO_CLARGS_GRP_SEP_STR "beg_"
#define MACSIO_CLARGS_GRP_END MACSIO_CLARGS_GRP_SEP_STR "end_"

/*!
\brief Begin option group (for TOJSON routing invokation)
*/
#define MACSIO_CLARGS_ARG_GROUP_BEG(GRPNAME,GRPHELP) MACSIO_CLARGS_GRP_BEG #GRPNAME, MACSIO_CLARGS_NODEFAULT, #GRPHELP

/*!
\brief End option group (for TOJSON routing invokation)
*/
#define MACSIO_CLARGS_ARG_GROUP_END(GRPNAME) MACSIO_CLARGS_GRP_END #GRPNAME, MACSIO_CLARGS_NODEFAULT, ""

/*!
\brief Begin option group (for TOMEM routing invokation)
*/
#define MACSIO_CLARGS_ARG_GROUP_BEG2(GRPNAME,GRPHELP) MACSIO_CLARGS_GRP_BEG #GRPNAME, MACSIO_CLARGS_NODEFAULT, #GRPHELP, 0

/*!
\brief End option group (for TOMEM routing invokation)
*/
#define MACSIO_CLARGS_ARG_GROUP_END2(GRPNAME)         MACSIO_CLARGS_GRP_END #GRPNAME, MACSIO_CLARGS_NODEFAULT, "", 0

/*!
\brief Moniker to terminate argument list
*/
#define MACSIO_CLARGS_END_OF_ARGS "macsio_end_of_args"

/*!
\brief Value to indicate no default specified
*/
#define MACSIO_CLARGS_NODEFAULT (void*)0

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _MACSIO_CLARGS_ArgvFlags_t
{
    unsigned int error_mode    : 1; /**< 0=warn, 1=abort */
    unsigned int route_mode    : 2; /**< 0=(TOMEM) scalar variables, 1=(TOJSON) json_object, 2,3 unused */
    unsigned int defaults_mode : 1; /**< 0=assign defaults, 1=do not assign defaults */
} MACSIO_CLARGS_ArgvFlags_t;

/*!
\brief Command-line argument parsing, default values and help output

Avoid definining any argument with the substring \c \--help or \c \--no-strict.
These are command-line option keywords reserved by MACSio. \c \--help is a
request to print usage and exit. \c --no-strict disables strict command-line
option error response which ordinarily causes an abort and instead issues
warnings.

In parallel, this function must be called collectively by all ranks in
\c MPI_COMM_WORLD. \c argc and \c argv on task rank 0 are broadcast to all tasks.
If request for help (e.g. \c \--help is in \c argv) or error(s) are encountered,
all tasks are broadcast this outcome. Only task rank 0 will print usage or 
error messages.

\return An integer value of either \c MACSIO_CLARGS_OK, \c MACSIO_CLARGS_HELP
        or \c MACSIO_CLARGS_ERROR.

*/
extern int
MACSIO_CLARGS_ProcessCmdline(
   void **retobj,       /**< [in/out] Optional void pointer to a returned object encoding the command
                             line options. Currently only JSON_C object is supported. Must specify
                             TOJSON routing. */
   MACSIO_CLARGS_ArgvFlags_t flags, /* flags to control behavior (see \c MACSIO_CLARGS_ArgvFlags_t) */
   int argi,            /**< [in] First arg index at which to to start processing \c argv */
   int argc,            /**< [in] \c argc from main */
   char **argv,         /**< [in] \c argv from main */
   ...                  /**< [in] Comma-separated list of 3 (TOJSON) or 4 (TOMEM) arguments per
                             command-line option;
                             1) option name and scanf format specifier(s) for command-line option;
                             2) default value(s) as a string;
                             3) help-string for command-line argument;
                             4) pointer to memory location to store parsed value(s) for TOMEM routing.
                                Not present for TOJSON routing. */
);


#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* _UTIL_H */
