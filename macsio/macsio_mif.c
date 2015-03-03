#include <stdlib.h>

#include <macsio_mif.h>

MACSIO_MIF_baton_t * MACSIO_MIF_Init(int numFiles, MACSIO_MIF_iomode_t ioMode, MPI_Comm mpiComm, int mpiTag,
    MACSIO_MIF_CreateCB createCb, MACSIO_MIF_OpenCB openCb, MACSIO_MIF_CloseCB closeCb, void *userData)
{
    int numGroups = numFiles;
    int commSize, rankInComm;
    int groupSize, numGroupsWithExtraProc, commSplit,
        groupRank, rankInGroup, procBeforeMe, procAfterMe;
    MACSIO_MIF_baton_t *ret = 0;

    procBeforeMe = -1;
    procAfterMe = -1;

    MPI_Comm_size(mpiComm, &commSize);
    MPI_Comm_rank(mpiComm, &rankInComm);

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
    ret->ioMode = ioMode;
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
    ret->mpiVal = MACSIO_MIF_BATON_OK;
    ret->mpiTag = mpiTag;
    ret->mpiComm = mpiComm;
    ret->createCb = createCb;
    ret->openCb = openCb;
    ret->closeCb = closeCb;
    ret->userData = userData;

    return ret;
}

void MACSIO_MIF_Finish(MACSIO_MIF_baton_t *bat)
{
    free(bat);
}

void * MACSIO_MIF_WaitForBaton(MACSIO_MIF_baton_t *Bat, const char *fname, const char *nsname)
{
    if (Bat->procBeforeMe != -1)
    {
        MPI_Status mpi_stat;
        int baton;
        int mpi_err =
            MPI_Recv(&baton, 1, MPI_INT, Bat->procBeforeMe,
                Bat->mpiTag, Bat->mpiComm, &mpi_stat);
        if (mpi_err == MPI_SUCCESS && baton != MACSIO_MIF_BATON_ERR)
        {
            return Bat->openCb(fname, nsname, Bat->ioMode, Bat->userData);
        }
        else
        {
            Bat->mpiVal = MACSIO_MIF_BATON_ERR;
            return 0;
        }
    }
    else
    {
        if (Bat->ioMode == MACSIO_MIF_WRITE)
            return Bat->createCb(fname, nsname, Bat->userData);
        else
            return Bat->openCb(fname, nsname, Bat->ioMode, Bat->userData);
    }
}

void MACSIO_MIF_HandOffBaton(const MACSIO_MIF_baton_t *Bat, void *file)
{
    Bat->closeCb(file, Bat->userData);
    if (Bat->procAfterMe != -1)
    {
        int baton = Bat->mpiVal;
        MPI_Ssend(&baton, 1, MPI_INT, Bat->procAfterMe,
            Bat->mpiTag, Bat->mpiComm);
    }
}

int MACSIO_MIF_GroupRank(const MACSIO_MIF_baton_t *Bat, int rankInComm)
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

int MACSIO_MIF_RankInGroup(const MACSIO_MIF_baton_t *Bat, int rankInComm)
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
