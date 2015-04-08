#ifndef _MACSIO_TIMING_H
#define _MACSIO_TIMING_H

#include <mpi.h>

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MACSIO_TIMING_ITER_AUTO -1
#define MACSIO_TIMING_ITER_IGNORE -2
#define MACSIO_TIMING_INVALID_TIMER (~((MACSIO_TIMING_TimerId_t)0x0))
#define MACSIO_TIMING_NO_GROUP (((MACSIO_TIMING_GroupMask_t)0x0)
#define MACSIO_TIMING_ALL_GROUPS (~((MACSIO_TIMING_GroupMask_t)0x0))

#define MT_StartTimer(LAB, GMASK, ITER) MACSIO_TIMING_StartTimer(LAB, GMASK, ITER, __FILE__, __LINE__)
#define MT_StopTimer(ID)          MACSIO_TIMING_StopTimer(ID)

typedef unsigned int             MACSIO_TIMING_TimerId_t;
typedef unsigned long long       MACSIO_TIMING_GroupMask_t;

extern int                       MACSIO_TIMING_UseMPI_Wtime;

extern MACSIO_TIMING_GroupMask_t MACSIO_TIMING_GroupName(char const *grpName);

extern MACSIO_TIMING_TimerId_t   MACSIO_TIMING_StartTimer(char const *timer_label,
                                     MACSIO_TIMING_GroupMask_t gmask, int iter_num, char const *__file__, int __line__);
extern double                    MACSIO_TIMING_StopTimer(MACSIO_TIMING_TimerId_t id);

extern void                      MACSIO_TIMING_DumpTimersToStrings(MACSIO_TIMING_GroupMask_t gmask,
                                     char ***strs, int *nstrs, int *maxlen);
#ifdef HAVE_MPI
extern void                      MACSIO_TIMING_ReduceTimers(MPI_Comm comm, int root);
#else
extern void                      MACSIO_TIMING_ReduceTimers(int comm, int root);
#endif
extern void                      MACSIO_TIMING_DumpReducedTimersToStrings(MACSIO_TIMING_GroupMask_t gmask, char ***strs, int *nstrs, int *maxlen);
extern void                      MACSIO_TIMING_ClearTimers(MACSIO_TIMING_GroupMask_t gmask);

#ifdef __cplusplus
}
#endif

#endif /* _MACSIO_TIMING_H */
