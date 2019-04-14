#ifndef _MACSIO_TIMING_H
#define _MACSIO_TIMING_H
/*
Copyright (c) 2015, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.
Written by Mark C. Miller

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

#ifdef HAVE_MPI
#include <mpi.h>
#endif

#include <stdio.h>
#include <string.h>

/*!
\defgroup MACSIO_TIMING MACSIO_TIMING
\brief Timing utilities 

@{
*/

#ifdef __cplusplus
extern "C" {
#endif

/*!
\def MACSIO_TIMING_ITER_AUTO
\brief Automatic iteration numbering
Use for \c iter arg to \c MT_StartTimer() for automatic iteration numbering
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
\brief May be returned from \c MT_StartTimer()
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
\brief Shorthand for \c MACSIO_TIMING_GetCurrentTime()
*/
#define MT_Time MACSIO_TIMING_GetCurrentTime

/*!
\def __BASEFILE__
\brief Same as \c basename(__FILE__)
Used to strip unnecessary path terms from \c __FILE__ in messages
*/
#define __BASEFILE__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

/*!
\def MT_StartTimer
\brief Shorthand for \c MACSIO_TIMING_StartTimer()
\param [in] LAB User defined timer label string
\param [in] GMASK User defined group mask. Use MACSIO_TIMING_NO_GROUP if timer grouping is not needed.
\param [in] ITER The iteration number. Use MACSIO_TIMING_ITER_IGNORE if timer iteration is not needed.
*/
#define MT_StartTimer(LAB, GMASK, ITER) MACSIO_TIMING_StartTimer(LAB, GMASK, ITER, __BASEFILE__, __LINE__)

/*!
\def MT_StopTimer
\brief Shorthand for \c MACSIO_TIMING_StopTimer()
\param [in] ID The timer's hash id returned from a call to \c MT_StartTimer().
*/
#define MT_StopTimer(ID) MACSIO_TIMING_StopTimer(ID)

typedef unsigned int MACSIO_TIMING_TimerId_t;
typedef unsigned long long MACSIO_TIMING_GroupMask_t;

/*!
\brief Integer variable to control function used to get timer values

A non-zero value indicates that MACSIO_TIMING should use \c MPI_Wtime(). Otherwise, it will
use \c gettimeofday().
*/
extern int MACSIO_TIMING_UseMPI_Wtime;

/*!
\brief Create a group name and mask

A small number of groups (less than 64) can be defined into which timers can be grouped
Timers can be assigned to multiple groups by or'ing the resulting group masks.
*/
extern MACSIO_TIMING_GroupMask_t
MACSIO_TIMING_GroupMask(
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
MACSIO_TIMING_StopTimer(
    MACSIO_TIMING_TimerId_t id /**< The timer's ID, returned from MACSIO_TIMING_StartTimer() */
);

/*!
\brief Get specific data field from a timer

Possible field names are...

  - "start_time" time last iteration was started
  - "iter_count" number of times the timer has been triggered (iterated)
  - "iter_time" total time over all iterations
  - "min_iter" iteration at which minimum time was observed
  - "max_iter" iteration at which maximum time was observed
  - "min_rank" task rank where minimum time occurred (only valid on reduced timers)
  - "max_rank" task rank where maximum time occurred (only valid on reduced timers)
  - "depth" how deeply this timer is nested within other timers
  - "total_time" total time spent in this timer
  - "min_time"  minimum time observed for this timer
  - "max_time"  maximum time observed for this timer
  - "running_mean" current average time for this timer
  - "running_var"  current variance for this timer

Where applicable, returned values are over either
  - all iterations (when using non-reduced timers)
or
  - all iterations and ranks (when using reduced timers).
*/
extern double
MACSIO_TIMING_GetTimerDatum(
    MACSIO_TIMING_TimerId_t tid, /**< The timer's ID, returned from MACSIO_TIMING_StartTimer() */
    char const *field            /**< The name of the field from the timer to return */
);

/*!
\brief Get data field a specific reduced timer

For field names, see \c MACSIO_TIMING_GetTimerDatum()
*/
extern double
MACSIO_TIMING_GetReducedTimerDatum(
    MACSIO_TIMING_TimerId_t tid, /**< The timer's ID, returned from MACSIO_TIMING_StartTimer() */
    char const *field            /**< The name of the field from the timer to return */
);

/*!
\brief Dump timers to ascii strings

This call will find all used timers in the hash table matching the specified \c gmask group mask and dumps
each timer to a string. For convenience, the maximum length of the strings is also returned. This is to
facilitate dumping the strings to a MACSIO_LOG. Pass \c MACSIO_TIMING_ALL_GROUPS if you don't care about
timer groups.

Each timer is dumped to a string with the following informational fields catenated together and 
separated by colons...

  - TOT=%10.5f summed total time spent in this timer over all iterations (and over all ranks when reduced)
  - CNT=%04d summed total total number of iterations in this timer (and over all ranks when reduced)
  - MIN=%8.5f(%4.2f):%06d
    - minimum time recorded for this timer over all iterations (and ranks when reduced)
    - (%4.2f) the number of standard deviations of the min from the mean time
    - :%06d task rank where the minimum was observed (only valid when reduced)
  - AVG=%8.5f the mean time of this timer over all iterations (and ranks when reduced)
  - MAX=%8.5f(%4.2f):%06d,
    - maximum time recorded for this timer over all iterations (and ranks when reduced)
    - (%4.2f) the number of standard deviations of the max from the mean time
    - :%06d task rank where the maximum was observed. (only valid when reduced)
  - DEV=%8.8f standard deviation observed for all iterations of this timer
  - FILE=\%s the source file where this timer is triggered
  - LINE=\%d the source line number where this timer is triggered
  - LAB=\%s the user-defined label for this timer
*/
extern void
MACSIO_TIMING_DumpTimersToStrings(
    MACSIO_TIMING_GroupMask_t gmask, /**< Group mask to filter only timers belonging to specific groups */
    char ***strs, /**< Array of strings, one for each timer, returned to caller. Caller must free. */
    int *nstrs,                      /**< Number of strings returned to caller */
    int *maxlen                      /**< The maximum length of all strings */
); 

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
    int root       /**< The MPI rank of the root task to be used for the reduction */
);

/*!
\brief Dump reduced timers to ascii strings

Similar to \c DumpTimersToStrings except this call dumps reduced timers
*/
extern void
MACSIO_TIMING_DumpReducedTimersToStrings(
    MACSIO_TIMING_GroupMask_t gmask, /**< Group mask to filter only timers belonging to specific groups */
    char ***strs, /**< Array of strings, one for each timer, returned to caller. Caller must free */
    int *nstrs,                      /**< Number of strings returned to caller */
    int *maxlen                      /**< The maximum length of all strings */
);

/*!
\brief Clear a group of timers

Clears and resets timers of specified group.
Pass \c MACSIO_TIMING_ALL_GROUPS to clear all timers.

*/
extern void
MACSIO_TIMING_ClearTimers(
    MACSIO_TIMING_GroupMask_t gmask /**< Group mask to filter only timers belonging to specific groups */
);

/*!
\brief Get current time

Depending on current value of \c MACSIO_TIMING_UseMPI_Wtime, uses either
\c MPI_WTime() or \c gettimeofday().
*/
extern double
MACSIO_TIMING_GetCurrentTime(void);

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* _MACSIO_TIMING_H */
