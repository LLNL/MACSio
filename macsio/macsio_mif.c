#include <stdlib.h>

#include <macsio_mif.h>

/*!
\addtogroup MACSIO_MIF
@{
*/

#define MACSIO_MIF_BATON_OK  0
#define MACSIO_MIF_BATON_ERR 1

typedef struct _MACSIO_MIF_baton_t
{
    MACSIO_MIF_iomode_t ioMode; /**< Indicates if reading or writing */
#ifdef HAVE_MPI
    MPI_Comm mpiComm;           /**< The MPI communicator being used */
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
    int mpiVal;                 /**< MPI status value */
    int mpiTag;                 /**< MPI message tag used for all messages here */
    MACSIO_MIF_CreateCB createCb; /**< Create file callback */
    MACSIO_MIF_OpenCB openCb;   /**< Open file callback */
    MACSIO_MIF_CloseCB closeCb; /**< Close file callback */
    void *clientData;           /**< Client data to be passed around in calls */
} MACSIO_MIF_baton_t;

/*!
\brief Begin a MACSIO_MIF code-block
*/
MACSIO_MIF_baton_t *MACSIO_MIF_Init(
    int numFiles,                   /**< [in] Number of resultant files. Note: this is entirely independent of
                                    number of processors. Typically, this number is chosen to match
                                    the number of independent I/O pathways between the nodes the
                                    application is executing on and the filesystem. */
    MACSIO_MIF_iomode_t ioMode,     /**< [in] Indicate if the operation is for reading or writing files */
    MPI_Comm mpiComm,               /**< [in] The MPI communicator being used for I/O */
    int mpiTag,                     /**< [in] MPI message tag used by all messaging in MACSIO_MIF */
    MACSIO_MIF_CreateCB createCb,   /**< [in] Callback MACSIO_MIF uses to create a group's file */
    MACSIO_MIF_OpenCB openCb,       /**< [in] Callback MACSIO_MIF uses to open a group's file */
    MACSIO_MIF_CloseCB closeCb,     /**< [in] Callback MACSIO_MIF uses to close a group's file */
    void *clientData                /**< [in] Client specific data MACSIO_MIF will pass to callbacks */
)
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
    ret->clientData = clientData;

    return ret;
}

/*!
\brief End a MACSIO_MIF code-block
*/
void MACSIO_MIF_Finish(
    MACSIO_MIF_baton_t *bat /**< [in] The MACSIO_MIF baton handle */
)
{
    free(bat);
}

/*!
\brief Wait for exclusive access to the group's file.
*/
void * MACSIO_MIF_WaitForBaton(
    MACSIO_MIF_baton_t *Bat, /**< [in] The MACSIO_MIF baton handle */
    char const *fname,       /**< [in] The filename */
    char const *nsname       /**< [in] The namespace within the file to be used for objects in this code block.  */
)
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
            return Bat->openCb(fname, nsname, Bat->ioMode, Bat->clientData);
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
            return Bat->createCb(fname, nsname, Bat->clientData);
        else
            return Bat->openCb(fname, nsname, Bat->ioMode, Bat->clientData);
    }
}

/*!
\brief Release exclusive access to the group's file.
*/
void MACSIO_MIF_HandOffBaton(
    MACSIO_MIF_baton_t const *Bat, /**< [in] The MACSIO_MIF baton handle */ 
    void *file                     /**< [in] A void pointer to the group's file handle */
)
{
    Bat->closeCb(file, Bat->clientData);
    if (Bat->procAfterMe != -1)
    {
        int baton = Bat->mpiVal;
        MPI_Ssend(&baton, 1, MPI_INT, Bat->procAfterMe,
            Bat->mpiTag, Bat->mpiComm);
    }
}

/*!
\brief Rank of the group in which a given (global) rank exists.
*/
int MACSIO_MIF_RankOfGroup(
    MACSIO_MIF_baton_t const *Bat, /**< [in] The MACSIO_MIF baton handle */
    int rankInComm                 /**< [in] The (global) rank of a proccesor for which rank in group is desired */
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

/*!
\brief Rank within a group of a given (global) rank
*/
int MACSIO_MIF_RankInGroup(
    MACSIO_MIF_baton_t const *Bat, /**< [in] The MACSIO_MIF baton handle */
    int rankInComm                 /**< [in] The (global) rank of a processor for which the rank within it's group is desired */
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

/*!@}*/
