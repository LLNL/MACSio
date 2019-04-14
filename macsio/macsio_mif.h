#ifndef _MACSIO_MIF_H
#define _MACSIO_MIF_H
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

#include <stdlib.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

/*!
\defgroup MACSIO_MIF MACSIO_MIF
\brief Utilities supporting Multiple Indpendent File (MIF) Parallel I/O
@{
*/

#ifdef __cplusplus
extern "C" {
#endif

#define MACSIO_MIF_READ  0
#define MACSIO_MIF_WRITE 1

/*!
\brief Bit Field struct for I/O flags
*/
typedef struct _MACSIO_MIF_ioFlags_t
{
    unsigned int do_wr : 1;   /**< bit0: 1=write, 0=read */
    unsigned int use_scr : 1; /**< bit1: 1=use SCR, 0=don't use SCR */
} MACSIO_MIF_ioFlags_t;

/*!
\brief Opaque struct holding private implementation of MACSIO_MIF_baton_t
*/
typedef struct _MACSIO_MIF_baton_t MACSIO_MIF_baton_t;

typedef void *(*MACSIO_MIF_CreateCB)(const char *fname, const char *nsname, void *udata);
typedef void *(*MACSIO_MIF_OpenCB)  (const char *fname, const char *nsname,
                                        MACSIO_MIF_ioFlags_t ioFlags, void *udata);
typedef int   (*MACSIO_MIF_CloseCB) (void *file, void *udata);

/*!
\brief Initialize MACSIO_MIF for a MIF I/O operation

Creates and returns a MACSIO_MIF \em baton object establishing the mapping
between MPI ranks and file groups for a MIF I/O operation.

All processors in the \c mpiComm communicator must call this function
collectively with identical values for \c numFiles, \c ioFlags, and \c mpiTag.

If a given execution context has multiple active MACSIO_MIF_baton_t objects,
the caller must ensure that each passed a different \c mpiTag.

The resultant \em baton object is used in subsequent calls to WaitFor and
HandOff the baton to the next processor in each group.

The \c createCb, \c openCb, \c closeCb callback functions are used by MACSIO_MIF
to execute baton waits and handoffs during which time a group's file will be
closed by the HandOff function and opened by the WaitFor method except for the
first processor in each group which will create the file.

Processors in the \c mpiComm communicator are broken into \c numFiles groups.
If there is a remainder, \em R, after dividing the communicator size into
\c numFiles groups, then the first \em R groups will have one additional
processor.

\returns The MACSIO_MIF \em baton object
*/
extern MACSIO_MIF_baton_t *
MACSIO_MIF_Init(
    int numFiles,                   /**< [in] Number of resultant files. Note: this is entirely independent of
                                         number of tasks. Typically, this number is chosen to match
                                         the number of independent I/O pathways between the nodes the
                                         application is executing on and the filesystem. Pass MACSIO_MIF_MAX for
                                         file-per-processor. Pass MACSIO_MIF_AUTO (currently not supported) to
                                         request that MACSIO_MIF determine and use an optimum file count. */
    MACSIO_MIF_ioFlags_t ioFlags,   /**< [in] See \ref MACSIO_MIF_ioFlags_t for meaning of flags. */
#ifdef HAVE_MPI
    MPI_Comm mpiComm,               /**< [in] The MPI communicator containing all the MPI ranks that will
                                         marshall data in the MIF I/O operation. */
#else
    int      mpiComm,               /**< [in] Dummy arg (ignored) for MPI communicator */
#endif
    int mpiTag,                     /**< [in] MPI message tag MACSIO_MIF will use in all MPI messages for
                                         this MIF I/O context. */
    MACSIO_MIF_CreateCB createCb,   /**< [in] Callback MACSIO_MIF should use to create a group's file */
    MACSIO_MIF_OpenCB openCb,       /**< [in] Callback MACSIO_MIF should use to open a group's file */
    MACSIO_MIF_CloseCB closeCb,     /**< [in] Callback MACSIO_MIF should use to close a group's file */
    void *clientData                /**< [in] Optional, client specific data MACSIO_MIF will pass to callbacks */
);

/*!
\brief End a MACSIO_MIF I/O operation and free resources
*/
extern void MACSIO_MIF_Finish(
    MACSIO_MIF_baton_t *bat /**< [in] The MACSIO_MIF baton handle */
);

/*!
\brief Wait for exclusive access to the group's file

All tasks in \c mpiComm argument to \c MACSIO_MIF_Init() call this function
collectively. For the first task in each group, this call returns immediately.
For all others in the group, it blocks, waiting for the task \em before it to
finish its work on the group's file and call \c MACSIO_MIF_HandOffBaton().

\returns A void pointer to whatever data instance the \c createCb or \c openCb
methods return. The caller must cast this returned pointer to the correct type.

*/
extern void *
MACSIO_MIF_WaitForBaton(
    MACSIO_MIF_baton_t *Bat, /**< [in] The MACSIO_MIF baton handle */
    char const *fname,       /**< [in] The filename */
    char const *nsname       /**< [in] The namespace within the file to be used for this task's objects.  */
);

/*!
\brief Release exclusive access to the group's file

This function is called only by the current task holding exclusive access
to a group's file and closes the group's file for the calling task handing
off control to the next task in the group.

\returns The integer value returned from the \c MACSIO_MIF_CloseCB callback.
*/
extern int
MACSIO_MIF_HandOffBaton(
    MACSIO_MIF_baton_t const *Bat, /**< [in] The MACSIO_MIF baton handle */
    void *file                     /**< [in] A void pointer to the group's file handle */
);

/*!
\brief Rank of the group in which a given (global) rank exists.

Given the rank of a task in \c mpiComm used in the MACSIO_MIF_Init()
call, this function returns the rank of the \em group in which the given
task exists. This function can be called from any rank and will
return correct values for any rank it is passed.
*/
extern int
MACSIO_MIF_RankOfGroup(
    MACSIO_MIF_baton_t const *Bat, /**< [in] The MACSIO_MIF baton handle */
    int rankInComm                 /**< [in] The (global) rank of a task for which the rank of its group is desired */
);

/*!
\brief Rank within a group of a given (global) rank

Given the rank of a task in \c mpiComm used in the MACSIO_MIF_Init()
call, this function returns its rank within its group. This function can
be called from any rank and will return correct values for any rank it is
passed.
*/
extern int
MACSIO_MIF_RankInGroup(
    MACSIO_MIF_baton_t const *Bat, /**< [in] The MACSIO_MIF baton handle */
    int rankInComm                 /**< [in] The (global) rank of a task for which it's rank in a group is desired */
);

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* #ifndef _MACSIO_MIF_H */
