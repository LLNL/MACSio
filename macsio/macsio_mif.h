#ifndef _MACSIO_MIF_H
#define _MACSIO_MIF_H

#include <stdlib.h>

#define MACSIO_MIF_BATON_OK  0 
#define MACSIO_MIF_BATON_ERR 1

#ifdef HAVE_MPI
#include <mpi.h>
#else
#define MPI_Comm int
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MACSIO_MIF_READ=0x1,
    MACSIO_MIF_WRITE=0x3
} MACSIO_MIF_iomode_t;

typedef void * (*MACSIO_MIF_CreateCB)(const char *fname, const char *nsname, void *udata);
typedef void * (*MACSIO_MIF_OpenCB)(const char *fname, const char *nsname, MACSIO_MIF_iomode_t iomode, void *udata);
typedef void   (*MACSIO_MIF_CloseCB)(void *file, void *udata);

typedef struct _MACSIO_MIF_baton_t
{
    MACSIO_MIF_iomode_t ioMode;
    int commSize;
    int rankInComm;
    int numGroups;
    int groupSize;
    int numGroupsWithExtraProc;
    int commSplit;
    int groupRank;
    int rankInGroup;
    int procBeforeMe;
    int procAfterMe;
    int mpiVal;
    int mpiTag;
    MPI_Comm mpiComm;
    MACSIO_MIF_CreateCB createCb;
    MACSIO_MIF_OpenCB openCb;
    MACSIO_MIF_CloseCB closeCb;
    void *userData;
} MACSIO_MIF_baton_t;

extern MACSIO_MIF_baton_t *MACSIO_MIF_Init(int numFiles, MACSIO_MIF_iomode_t ioMode, MPI_Comm mpiComm, int mpiTag,
    MACSIO_MIF_CreateCB createCb, MACSIO_MIF_OpenCB openCb, MACSIO_MIF_CloseCB closeCb, void *userData);
extern void   MACSIO_MIF_Finish(MACSIO_MIF_baton_t *bat);
extern void * MACSIO_MIF_WaitForBaton(MACSIO_MIF_baton_t *Bat, const char *fname, const char *nsname);
extern void   MACSIO_MIF_HandOffBaton(const MACSIO_MIF_baton_t *Bat, void *file);
extern int    MACSIO_MIF_GroupRank(const MACSIO_MIF_baton_t *Bat, int rankInComm);
extern int    MACSIO_MIF_RankInGroup(const MACSIO_MIF_baton_t *Bat, int rankInComm);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _MACSIO_MIF_H */
