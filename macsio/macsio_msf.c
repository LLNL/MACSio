/*
Copyright (c) 2015, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.
Written by James Dickson

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

#include <macsio_msf.h>
#include <macsio_log.h>

#define MACSIO_MSF_BATON_OK  0
#define MACSIO_MSF_BATON_ERR 1

/*!
\addtogroup MACSIO_MSF
@{
*/

typedef struct _MACSIO_MSF_baton_t
{
    MACSIO_MSF_ioFlags_t ioFlags; /**< Various flags controlling behavior. */
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
    int *groupRanks;            /**< Array of all of the ranks in the group */
    int groupRoot;          /**< Rank of the root process for this group */
    int procBeforeMe;           /**< Rank of processor before this processor in the group */
    int procAfterMe;            /**< Rank of processor after this processor in the group */
    mutable int MSFErr;         /**< MSF error value */
    mutable int mpiErr;         /**< MPI error value */
    int mpiTag;                 /**< MPI message tag used for all messages here */
    void *clientData;           /**< Client data to be passed around in calls */
} MACSIO_MSF_baton_t;


MACSIO_MSF_baton_t *MACSIO_MSF_Init(
    int numFiles,                   
    MACSIO_MSF_ioFlags_t ioFlags,
#ifdef HAVE_MPI
    MPI_Comm mpiComm,               
#else
    int      mpiComm,
#endif
    int mpiTag,
    void *clientData
)
{
    int numGroups = numFiles;
    int commSize, rankInComm;
    int groupSize, numGroupsWithExtraProc, commSplit,
        groupRank, rankInGroup, procBeforeMe, procAfterMe;
    MACSIO_MSF_baton_t *ret = 0;

    procBeforeMe = -1;
    procAfterMe = -1;

#ifdef HAVE_MPI
    MPI_Comm_size(mpiComm, &commSize);
    MPI_Comm_rank(mpiComm, &rankInComm);
#endif

    if (commSize < numGroups){
        MACSIO_LOG_MSG(Die, ("More files than ranks!"));
    }

    groupSize              = commSize / numGroups;
    numGroupsWithExtraProc = commSize % numGroups;
    commSplit = numGroupsWithExtraProc * (groupSize + 1);

    if (rankInComm < commSplit)
    {
        groupRank = rankInComm / (groupSize + 1);
        rankInGroup = rankInComm % (groupSize + 1);
        if (rankInGroup < groupSize)
            procAfterMe = rankInComm + 1;
        groupSize++;
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

    /* Create group communicator */
    int groupRootRank;
    int *groupRanks = (int*)malloc(sizeof(int) * groupSize);
#ifdef HAVE_MPI
    MPI_Comm groupComm;
    MPI_Comm_split(mpiComm, groupRank, rankInGroup, &groupComm);
 
    /* Gather the ranks of each process in this group */
    MPI_Allgather(&rankInComm, 1, MPI_INT, groupRanks, 1, MPI_INT, groupComm);

    /* Broadcast the rank from the group root to rest of group */
    int amIRoot = rankInGroup == 0 ? rankInComm : -1;
    int *rankArray = (int*)malloc(sizeof(int) * groupSize);
    MPI_Allgather(&amIRoot, 1, MPI_INT, rankArray, 1, MPI_INT, groupComm);
    for (int i = 0; i<groupSize; i++){
        if (rankArray[i] > -1){
            groupRootRank = rankArray[i];
            break;
        }
    }
#endif

    ret = (MACSIO_MSF_baton_t *) malloc(sizeof(MACSIO_MSF_baton_t));
    ret->ioFlags = ioFlags;
    ret->commSize = commSize;
    ret->rankInComm = rankInComm;
    ret->numGroups = numGroups;
    ret->groupSize = groupSize;
    ret->numGroupsWithExtraProc = numGroupsWithExtraProc;
    ret->commSplit = commSplit;
    ret->groupRank = groupRank;
    ret->rankInGroup = rankInGroup;
    ret->groupRanks = groupRanks;
    ret->groupRoot = groupRootRank;
    ret->procBeforeMe = procBeforeMe;
    ret->procAfterMe = procAfterMe;
    ret->MSFErr = MACSIO_MSF_BATON_OK;
#ifdef HAVE_MPI
    ret->mpiErr = MPI_SUCCESS;
    ret->mpiComm = groupComm;
#else
    ret->mpiErr = 0;
#endif
    ret->mpiTag = mpiTag;
    ret->clientData = clientData;

    return ret;
}

void MACSIO_MSF_Finish(
    MACSIO_MSF_baton_t *bat
)
{
#ifdef HAVE_MPI
    MPI_Comm_free(&(bat->mpiComm));
#endif
    free(bat);
}

int MACSIO_MSF_RankOfGroup(
    MACSIO_MSF_baton_t const *Bat,
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

int MACSIO_MSF_RankInGroup(
    MACSIO_MSF_baton_t const *Bat,
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

int MACSIO_MSF_SizeOfGroup(
    MACSIO_MSF_baton_t const *Bat
)
{
    return Bat->groupSize;
}

#ifdef HAVE_MPI
MPI_Comm MACSIO_MSF_CommOfGroup(
    MACSIO_MSF_baton_t const *Bat
)
{
    return Bat->mpiComm;
}
#endif

int MACSIO_MSF_RootOfGroup(
    MACSIO_MSF_baton_t const *Bat
)
{
    return Bat->groupRoot;
}
/*!@}*/
