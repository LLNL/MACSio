#ifndef _TIMING_H
#define _TIMING_H

#include <stdio.h>

typedef int IOPTimerId_t;

typedef enum IOPioop_t
{
    TEST_TIMERS,
    FILE_CREATE,
    FILE_OPEN,
    FILE_CLOSE
    /* need to add more op tags here */
} IOPioop_t;

#ifdef __cplusplus
extern "C" {
#endif

extern IOPTimerId_t StartTimer();
extern double StopTimer(IOPTimerId_t id, IOPioop_t op, char const *blockName);
extern void DumpTimings(FILE *f);

#ifdef __cplusplus
}
#endif

#endif /* _TIMING_H */
