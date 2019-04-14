#ifndef _MACSIO_MSF_H
#define _MACSIO_MSF_H
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
\defgroup MACSIO_MSF MACSIO_MSF
\brief MSF Description

@{
*/

#ifdef __cplusplus
extern "C" {
#endif

#define MACSIO_MSF_READ  0
#define MACSIO_MSF_WRITE 1
#define MACSIO_MSF_SCR_OFF 0
#define MACSIO_MSF_SCR_ON 1

typedef struct _MACSIO_MSF_ioFlags_t
{
    unsigned int do_wr : 1;
    unsigned int use_scr : 1;
} MACSIO_MSF_ioFlags_t;

typedef struct _MACSIO_MSF_baton_t MACSIO_MSF_baton_t;

//#warning ENSURE DIFFERENT INSTANCES USE DIFFERENT MPI TAGS
#ifdef HAVE_MPI
extern MACSIO_MSF_baton_t *MACSIO_MSF_Init(int numFiles, MACSIO_MSF_ioFlags_t ioFlags,
    MPI_Comm mpiComm, int mpiTag,
    void *userData);
#else
extern MACSIO_MSF_baton_t *MACSIO_MSF_Init(int numFiles, MACSIO_MSF_ioFlags_t ioFlags,
    int mpiComm, int mpiTag,
    void *userData);
#endif
extern void   MACSIO_MSF_Finish(MACSIO_MSF_baton_t *bat);
extern int    MACSIO_MSF_RankOfGroup(const MACSIO_MSF_baton_t *Bat, int rankInComm);
extern int    MACSIO_MSF_RankInGroup(const MACSIO_MSF_baton_t *Bat, int rankInComm);
extern int    MACSIO_MSF_SizeOfGroup(const MACSIO_MSF_baton_t *Bat);
#ifdef HAVE_MPI
extern MPI_Comm MACSIO_MSF_CommOfGroup(MACSIO_MSF_baton_t const *Bat);
#endif
extern int MACSIO_MSF_RootOfGroup(MACSIO_MSF_baton_t const *Bat);

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* #ifndef _MACSIO_MSF_H */
