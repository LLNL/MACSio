#ifndef _MACSIO_TIMING_H
#define _MACSIO_TIMING_H

#include <mpi.h>

#include <stdio.h>

/*!
\defgroup MACSIO_TIMING MACSIO_TIMING
\brief Timing utilities 

MACSIO_TIMING utilities support the creation of a number of timers to time different sections of code.

Timers are initialized/started with a user-defined label, and an optional group mask and iteration number. 
A hash of the timer is computed from its label and group combined with the source file name and line number.
The resulting hash value is used to identify the timer.

Timers can be iterated and re-started. These are two different concepts. During iteration, the same timer can be
used to time the same section of code as it gets executed multiple times. Each time represents a different
iteration of the timer. The timer will keep track of the minimum time, maximum time, average time and variance
of times over all iterations it is invoked. However, apart from these running statistics, a timer maintains no memory
of past values. If a timer is iterated 10 times, it does not maintain knowledge of all 10 individual times. It
maintains only knowledge of the running statistics; min, max, avg, var. The algorithm it uses to maintain these
running statistics is the Knuth "online" algorithm.

Within any timer iteration, a timer can be stopped and restarted. If a timer is being used to time 
a block of code that involves some occasional and perhaps irrlevant alternative logic path,
the timer can be stopped until execution returns from the alternative path. Then the
timer can be restarted. This can only be done <em>within</em> a given iteration.

Timers can also be grouped into a small number of different classes. For example, it is possible to maintain
a set of timers for MIF file I/O apart from a set of timers being used to time a particular plugin's operations
to marshal a mesh or variable to/from persistent storage. Or, operations use to interact with filesystem metadata
directly (e.g. mkdir(2), chdir(2), readir(2), etc.) can be maintained separately from timers used for entirely
other purposes.

Finally, timers can be <em>reduced</em> across MPI ranks thereby creating a statistical summary of timer information
across processors.

A timer is initialized/started by a call to \c MACSIO_TIMING_StartTimer() or the convenience macro \c MT_StartTimer().
This call returns the timer's <em>ID</em> which is used in a subsequent call to \c MACSIO_TIMING_StopTimer() to stop
the timer.

\code
    MACSIO_TIMING_TimerId_t tid = MT_StartTimer("my timer", MACSIO_TIMING_GROUP_NONE, MACSIO_TIMING_ITER_AUTO);

    ...do some work here...

    MT_StopTimer(tid);
\endcode

In the above code, the call to MT_StartTimer starts a timer for a new (automatic) iteration. In this simple
examle, we do not worry about timer group masks.

By default, MACSIO_TIMING uses MPI_Wtime but a caller can set \c MACSIO_TIMING_UseMPI_Wtime() to zero to
instead use \c gettimeofday().

@{
*/

#ifdef __cplusplus
extern "C" {
#endif

#define MACSIO_TIMING_ITER_AUTO -1
#define MACSIO_TIMING_ITER_IGNORE -2
#define MACSIO_TIMING_INVALID_TIMER (~((MACSIO_TIMING_TimerId_t)0x0))
#define MACSIO_TIMING_NO_GROUP (((MACSIO_TIMING_GroupMask_t)0x0)
#define MACSIO_TIMING_ALL_GROUPS (~((MACSIO_TIMING_GroupMask_t)0x0))



/*!
\def MT_StartTimer
\brief Convenience macro for starting a timer
\param [in] LAB User defined timer label string
\param [in] GMASK User defined group mask. Use MACSIO_TIMING_NO_GROUP if timer grouping is not needed.
\param [in] ITER The iteration number. Use MACSIO_TIMING_ITER_IGNORE if timer iteration is not needed.
*/
#define MT_StartTimer(LAB, GMASK, ITER) MACSIO_TIMING_StartTimer(LAB, GMASK, ITER, __FILE__, __LINE__)

/*!
\def MT_StopTimer
\brief Convenience macro for stopping a timer
\param [in] ID The timer's hash id returned from a call to \c MT_StartTimer().
*/
#define MT_StopTimer(ID)                MACSIO_TIMING_StopTimer(ID)

/*!
\typedef
\brief Timer id
*/
typedef unsigned int             MACSIO_TIMING_TimerId_t;
typedef unsigned long long       MACSIO_TIMING_GroupMask_t;

extern int                       MACSIO_TIMING_UseMPI_Wtime;

/*!
\brief Create a group name and mask

A small number of groups (less than 64) can be defined into which timers can be grouped
Timers can be assigned to multiple groups by or'ing the resulting group masks.
*/
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

/*!@}*/

#endif /* _MACSIO_TIMING_H */
