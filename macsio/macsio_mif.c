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

#ifdef HAVE_SCR
#ifdef __cplusplus
extern "C" {
#endif
#include <scr.h>
#ifdef __cplusplus
}
#endif
#endif

#include <macsio_mif.h>

#define MACSIO_MIF_BATON_OK  0
#define MACSIO_MIF_BATON_ERR 1
#define MACSIO_MIF_MIFMAX -1
#define MACSIO_MIF_MIFAUTO -2

/*! \struct _MACSIO_MIF_baton_t */
typedef struct _MACSIO_MIF_baton_t
{
    MACSIO_MIF_ioFlags_t ioFlags; /**< Various flags controlling behavior. */
#ifdef HAVE_MPI
    MPI_Comm mpiComm;           /**< The MPI communicator being used */
#else
    int mpiComm;                /**< Dummy MPI communicator */
#endif
    int commSize;               /**< The size of the MPI comm */
    int rankInComm;             /**< Rank of this processor in the MPI comm */
    int numGroups;              /**< Number of groups the MPI comm is divided into */
    int numGroupsWithExtraProc; /**< Number of groups that contain one extra proc/rank */
    int groupSize;              /**< Nominal size of each group (some groups have one extra) */
    int groupRank;              /**< Rank of this processor's group */
    int commSplit;              /**< Rank of the last MPI task assigned to +1 groups */
    int rankInGroup;            /**< Rank of this processor within its group */
    int procBeforeMe;           /**< Rank of processor before this processor in the group */
    int procAfterMe;            /**< Rank of processor after this processor in the group */
    mutable int mifErr;         /**< MIF error value */
    mutable int mpiErr;         /**< MPI error value */
    int mpiTag;                 /**< MPI message tag used for all messages here */
    MACSIO_MIF_CreateCB createCb; /**< Create file callback */
    MACSIO_MIF_OpenCB openCb;   /**< Open file callback */
    MACSIO_MIF_CloseCB closeCb; /**< Close file callback */
    void *clientData;           /**< Client data to be passed around in calls */
} MACSIO_MIF_baton_t;

//#warning ENSURE DIFFERENT INSTANCES USE DIFFERENT MPI TAGS
//#warning ADD A THROTTLE OPTION HERE FOR TOT FILES VS CONCURRENT FILES
//#warning FOR AUTO MODE, MUST HAVE A CALL TO QUERY FILE COUNT
MACSIO_MIF_baton_t *
MACSIO_MIF_Init(
    int numFiles,
    MACSIO_MIF_ioFlags_t ioFlags,
#ifdef HAVE_MPI
    MPI_Comm mpiComm,
#else
    int      mpiComm,
#endif
    int mpiTag,
    MACSIO_MIF_CreateCB createCb,
    MACSIO_MIF_OpenCB openCb,
    MACSIO_MIF_CloseCB closeCb,
    void *clientData
)
{
    int numGroups = numFiles;
    int commSize=1, rankInComm=0;
    int groupSize, numGroupsWithExtraProc, commSplit,
        groupRank, rankInGroup, procBeforeMe, procAfterMe;
    MACSIO_MIF_baton_t *ret = 0;

    procBeforeMe = -1;
    procAfterMe = -1;

#ifdef HAVE_MPI
    MPI_Comm_size(mpiComm, &commSize);
    MPI_Comm_rank(mpiComm, &rankInComm);
#endif

    groupSize              = commSize / numGroups;
    numGroupsWithExtraProc = commSize % numGroups;
    commSplit = numGroupsWithExtraProc * (groupSize + 1);

    if (rankInComm < commSplit)
    {
        groupRank = rankInComm / (groupSize + 1);
        rankInGroup = rankInComm % (groupSize + 1);
        if (rankInGroup < groupSize)
            procAfterMe = rankInComm + 1;
    }
    else
    {
        groupRank = numGroupsWithExtraProc + (rankInComm - commSplit) / groupSize; 
        rankInGroup = (rankInComm - commSplit) % groupSize;
        if (rankInGroup < groupSize - 1)
            procAfterMe = rankInComm + 1;
    }
    if (rankInGroup > 0)
        procBeforeMe = rankInComm - 1;

    if (createCb == 0 || openCb == 0 || closeCb == 0)
        return 0;

    ret = (MACSIO_MIF_baton_t *) malloc(sizeof(MACSIO_MIF_baton_t));
    ret->ioFlags = ioFlags;
    ret->commSize = commSize;
    ret->rankInComm = rankInComm;
    ret->numGroups = numGroups;
    ret->groupSize = groupSize;
    ret->numGroupsWithExtraProc = numGroupsWithExtraProc;
    ret->commSplit = commSplit;
    ret->groupRank = groupRank;
    ret->rankInGroup = rankInGroup;
    ret->procBeforeMe = procBeforeMe;
    ret->procAfterMe = procAfterMe;
    ret->mifErr = MACSIO_MIF_BATON_OK;
#ifdef HAVE_MPI
    ret->mpiErr = MPI_SUCCESS;
#else
    ret->mpiErr = 0;
#endif
    ret->mpiTag = mpiTag;
    ret->mpiComm = mpiComm;
    ret->createCb = createCb;
    ret->openCb = openCb;
    ret->closeCb = closeCb;
    ret->clientData = clientData;

    return ret;
}

void
MACSIO_MIF_Finish(
    MACSIO_MIF_baton_t *bat
)
{
    free(bat);
}

void *
MACSIO_MIF_WaitForBaton(
    MACSIO_MIF_baton_t *Bat,
    char const *fname,
    char const *nsname
)
{
    if (Bat->procBeforeMe != -1)
    {
        int mpi_err;
#ifdef HAVE_MPI
        MPI_Status mpi_stat;
        int baton;
        mpi_err = MPI_Recv(&baton, 1, MPI_INT, Bat->procBeforeMe,
                           Bat->mpiTag, Bat->mpiComm, &mpi_stat);
        if (mpi_err == MPI_SUCCESS && baton != MACSIO_MIF_BATON_ERR)
#else
        if (1)
#endif
        {
            if (Bat->ioFlags.use_scr)
            {
#ifdef HAVE_SCR
                char scr_filename[SCR_MAX_FILENAME];
                if (SCR_Route_file(fname, scr_filename) == SCR_SUCCESS)
                    return Bat->openCb(scr_filename, nsname, Bat->ioFlags, Bat->clientData);
                else
                    return Bat->openCb(fname, nsname, Bat->ioFlags, Bat->clientData);
#else
                return Bat->openCb(fname, nsname, Bat->ioFlags, Bat->clientData);
#endif
            }
            else
            {
                return Bat->openCb(fname, nsname, Bat->ioFlags, Bat->clientData);
            }
        }
        else
        {
            Bat->mifErr = MACSIO_MIF_BATON_ERR;
            Bat->mpiErr = mpi_err;
            return 0;
        }
    }
    else
    {
        if (Bat->ioFlags.do_wr)
        {
            if (Bat->ioFlags.use_scr)
            {
#ifdef HAVE_SCR
                char scr_filename[SCR_MAX_FILENAME];
                if (SCR_Route_file(fname, scr_filename) == SCR_SUCCESS)
                    return Bat->createCb(scr_filename, nsname, Bat->clientData);
                else
                    return Bat->createCb(fname, nsname, Bat->clientData);
#else
                return Bat->createCb(fname, nsname, Bat->clientData);
#endif
            }
            else
            {
                return Bat->createCb(fname, nsname, Bat->clientData);
            }
        }
        else
        {
            if (Bat->ioFlags.use_scr)
            {
#ifdef HAVE_SCR
                char scr_filename[SCR_MAX_FILENAME];
                if (SCR_Route_file(fname, scr_filename) == SCR_SUCCESS)
                    return Bat->openCb(scr_filename, nsname, Bat->ioFlags, Bat->clientData);
                else
                    return Bat->openCb(fname, nsname, Bat->ioFlags, Bat->clientData);
#else
                return Bat->openCb(fname, nsname, Bat->ioFlags, Bat->clientData);
#endif
            }
            else
            {
                return Bat->openCb(fname, nsname, Bat->ioFlags, Bat->clientData);
            }
        }
    }
}

int
MACSIO_MIF_HandOffBaton(
    MACSIO_MIF_baton_t const *Bat,
    void *file
)
{
    int retval = Bat->closeCb(file, Bat->clientData);
    if (Bat->procAfterMe != -1)
    {
        int mpi_err;
#ifdef HAVE_MPI
        int baton = Bat->mifErr;
        mpi_err = MPI_Ssend(&baton, 1, MPI_INT, Bat->procAfterMe,
                            Bat->mpiTag, Bat->mpiComm);
        if (mpi_err != MPI_SUCCESS)
#else
        if (0)
#endif
        {
            Bat->mifErr = MACSIO_MIF_BATON_ERR;
            Bat->mpiErr = mpi_err;
        }
    }
    return retval;
}

int
MACSIO_MIF_RankOfGroup(
    MACSIO_MIF_baton_t const *Bat,
    int rankInComm
)
{
    int retval;

    if (rankInComm < Bat->commSplit)
    {
        retval = rankInComm / (Bat->groupSize + 1);
    }
    else
    {
        retval = Bat->numGroupsWithExtraProc +
                 (rankInComm - Bat->commSplit) / Bat->groupSize; 
    }

    return retval;
}

int
MACSIO_MIF_RankInGroup(
    MACSIO_MIF_baton_t const *Bat,
    int rankInComm
)
{
    int retval;

    if (rankInComm < Bat->commSplit)
    {
        retval = rankInComm % (Bat->groupSize + 1);
    }
    else
    {
        retval = (rankInComm - Bat->commSplit) % Bat->groupSize;
    }

    return retval;
}
