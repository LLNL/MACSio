#ifndef _MACSIO_MIF_H
#define _MACSIO_MIF_H

#include <stdlib.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

/*!
\defgroup MACSIO_MIF Utilities to support Multiple Independent File Parallel I/O Mode
@{
*/

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MACSIO_MIF_READ=0x1,
    MACSIO_MIF_WRITE=0x3
} MACSIO_MIF_iomode_t;

typedef struct _MACSIO_MIF_baton_t MACSIO_MIF_baton_t;
typedef void *(*MACSIO_MIF_CreateCB)(const char *fname, const char *nsname, void *udata);
typedef void *(*MACSIO_MIF_OpenCB)  (const char *fname, const char *nsname, MACSIO_MIF_iomode_t iomode, void *udata);
typedef void  (*MACSIO_MIF_CloseCB) (void *file, void *udata);

#ifdef HAVE_MPI
extern MACSIO_MIF_baton_t *MACSIO_MIF_Init(int numFiles, MACSIO_MIF_iomode_t ioMode, MPI_Comm mpiComm, int mpiTag,
    MACSIO_MIF_CreateCB createCb, MACSIO_MIF_OpenCB openCb, MACSIO_MIF_CloseCB closeCb, void *userData);
#endif
extern void   MACSIO_MIF_Finish(MACSIO_MIF_baton_t *bat);
extern void * MACSIO_MIF_WaitForBaton(MACSIO_MIF_baton_t *Bat, const char *fname, const char *nsname);
extern void   MACSIO_MIF_HandOffBaton(const MACSIO_MIF_baton_t *Bat, void *file);
extern int    MACSIO_MIF_RankOfGroup(const MACSIO_MIF_baton_t *Bat, int rankInComm);
extern int    MACSIO_MIF_RankInGroup(const MACSIO_MIF_baton_t *Bat, int rankInComm);

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* #ifndef _MACSIO_MIF_H */
