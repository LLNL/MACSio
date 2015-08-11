#ifndef _MACSIO_TIMING_H
#define _MACSIO_TIMING_H
/*
Copyright (c) 2015, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.
Written by Mark C. Miller

CODE-<R&R number> All rights reserved.

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

/*!
\def MACSIO_TIMING_ITER_AUTO
\brief Automatic iteration numbering
Use for \c iter argument to \c StartTimer() when you don't want to manager iteration numbering
of the timer explicitly.
*/
#define MACSIO_TIMING_ITER_AUTO -1

/*!
\def MACSIO_TIMING_ITER_IGNORE
\brief What is this?
*/
#define MACSIO_TIMING_ITER_IGNORE -2

/*!
\def MACSIO_TIMING_INVALID_TIMER
\brief Maybe returned from \c StartTimer()
*/
#define MACSIO_TIMING_INVALID_TIMER (~((MACSIO_TIMING_TimerId_t)0x0))

/*!
\def MACSIO_TIMING_NO_GROUP
\brief Group mask when timer is not assigned to any group
*/
#define MACSIO_TIMING_NO_GROUP (((MACSIO_TIMING_GroupMask_t)0x0)

/*!
\def MACSIO_TIMING_ALL_GROUPS
\brief Group mask representing all groups
*/
#define MACSIO_TIMING_ALL_GROUPS (~((MACSIO_TIMING_GroupMask_t)0))

/*!
\def MT_Time
\brief Convenience macro for getting current time
*/
#define MT_Time MACSIO_TIMING_GetCurrentTime

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
#define MT_StopTimer(ID) MACSIO_TIMING_StopTimer(ID)

typedef unsigned int             MACSIO_TIMING_TimerId_t;
typedef unsigned long long       MACSIO_TIMING_GroupMask_t;

/*!
\brief Integer variable to control function used to get timer values

A non-zero value indicates that MACSIO_TIMING should use \c MPI_Wtime. Otherwise, it will
use \c gettimeofday().
*/
extern int                       MACSIO_TIMING_UseMPI_Wtime;

/*!
\brief Create a group name and mask

A small number of groups (less than 64) can be defined into which timers can be grouped
Timers can be assigned to multiple groups by or'ing the resulting group masks.
*/
extern MACSIO_TIMING_GroupMask_t MACSIO_TIMING_GroupMask(
    char const *grpName /**< Name of the group for which group mask is needed */
);

/*!
\brief Create/Start a timer

This call either creates a new timer and starts it or starts a new iteration of an existing timer.
\return A hash derived from a string catenation the \c label, \c gmask, \c file and \c line.
*/
extern MACSIO_TIMING_TimerId_t
MACSIO_TIMING_StartTimer(
    char const *label,               /**< User defined label to be assigned to the timer */
    MACSIO_TIMING_GroupMask_t gmask, /**< Mask to indicate the timer's group membership */
    int iter_num,                    /**< Iteration number */
    char const *file,                /**< The source file name */
    int line                         /**< The source file line number*/);

/*!
\brief Stop a timer

This call stops a currently running timer.
\return Returns the time for the current iteration of the timer
*/
extern double
MACSIO_TIMING_StopTimer(MACSIO_TIMING_TimerId_t id /**< The timer's ID, returned from a call to StartTimer */);

/*!
\brief Get data from a specific timer

For field names, see definition of timerInfo_t
*/
extern double
MACSIO_TIMING_GetTimer(
    MACSIO_TIMING_TimerId_t tid, /**< The timer's ID, returned from a call to StartTimer */
    char const *field            /**< The name of the field from the timer to return */
);

/*!
\brief Get data from a specific reduced timer
*/
extern double
MACSIO_TIMING_GetReducedTimer(
    MACSIO_TIMING_TimerId_t tid, /**< The timer's ID, returned from a call to StartTimer */
    char const *field            /**< The name of the field from the timer to return */
);

/*!
\brief Dump timers to ascii strings

This call will find all used timers in the hash table matching the specified \c gmask group mask and dumps
each timer to a string. For convenience, the maximum length of the strings is also returned. This is to
facilitate dumping the strings to a MACSIO_LOG.
*/
extern void MACSIO_TIMING_DumpTimersToStrings(
    MACSIO_TIMING_GroupMask_t gmask, /**< Group mask to filter only timers belonging to specific groups */
    char ***strs,                    /**< An array of strings, one for each timer, returned to caller. Caller is responsible for freeing */
    int *nstrs,                      /**< Number of strings returned to caller */
    int *maxlen                      /**< The maximum length of all strings */); 

/*!
\brief Reduce timers across MPI tasks

Computes a parallel reduction across MPI tasks of all timers.
*/
extern void
MACSIO_TIMING_ReduceTimers(
#ifdef HAVE_MPI
    MPI_Comm comm, /**< The MPI communicator to use for the reduction */
#else
    int comm,      /**< Dummy value for non-parallel builds */
#endif
    int root       /**< The MPI rank of the root task to be used for the reduction */);

/*!
\brief Dump reduced timers to ascii strings

Similar to \c DumpTimersToStrings except this call dumps reduced timers
*/
extern void
MACSIO_TIMING_DumpReducedTimersToStrings(
    MACSIO_TIMING_GroupMask_t gmask, /**< Group mask to filter only timers belonging to specific groups */
    char ***strs,                    /**< An array of strings, one for each timer, returned to caller. Caller is responsible for freeing */
    int *nstrs,                      /**< Number of strings returned to caller */
    int *maxlen                      /**< The maximum length of all strings */);

/*!
\brief Clear a group of timers

Clears and resets timers of specified group
*/
extern void MACSIO_TIMING_ClearTimers(
    MACSIO_TIMING_GroupMask_t gmask /**< Group mask to filter only timers belonging to specific groups */);

/*!
\brief Get current time
*/
extern double MACSIO_TIMING_GetCurrentTime(void);

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* _MACSIO_TIMING_H */
