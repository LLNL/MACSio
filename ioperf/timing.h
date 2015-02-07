#ifndef _TIMING_H
#define _TIMING_H

#include <stdio.h>

typedef int MACSIO_TimerId_t;

typedef enum MACSIO_ioop_t
{
    TEST_TIMERS,
    FILE_CREATE,
    FILE_OPEN,
    FILE_CLOSE
    /* need to add more op tags here */
} MACSIO_ioop_t;

#ifdef __cplusplus
extern "C" {
#endif

extern MACSIO_TimerId_t StartTimer();
extern double StopTimer(MACSIO_TimerId_t id, MACSIO_ioop_t op, char const *blockName);
extern void DumpTimings(FILE *f);

#ifdef __cplusplus
}
#endif

#endif /* _TIMING_H */
