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

#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

#include <macsio_log.h>
#include <macsio_timing.h>

int MACSIO_MAIN_Rank;
int MACSIO_MAIN_Size;
int MACSIO_MAIN_Comm;

static void dsleep(double delay)
{
    double dsec = floor(delay);
    long nsec = (long) ((delay - dsec) * 1e+9);
    time_t sec = (time_t) dsec;
    struct timespec ts = {sec, nsec};
    nanosleep(&ts, 0);
}

void func2();

void func4()
{
    static int iter = 0;
    double delay = (random() % 1000) / (double) 10000; /* random up to 1/10th second of sleep */
    MACSIO_TIMING_TimerId_t tid = MT_StartTimer("func4", MACSIO_TIMING_ALL_GROUPS, iter++);
    dsleep(delay);
    MT_StopTimer(tid);
}

void func3()
{
    static int iter = 0;
    MACSIO_TIMING_TimerId_t tid = MT_StartTimer("func3", MACSIO_TIMING_ALL_GROUPS, iter++);
    func4();
    func2();
    MT_StopTimer(tid);
}

void func2()
{
    static int iter = 0;
    MACSIO_TIMING_TimerId_t tid = MT_StartTimer("func2", MACSIO_TIMING_ALL_GROUPS, iter++);
    dsleep(0.02);
    MT_StopTimer(tid);
}

void func1()
{
    static int iter = 0;
    MACSIO_TIMING_TimerId_t tid = MT_StartTimer("func1", MACSIO_TIMING_ALL_GROUPS, iter);
    MACSIO_TIMING_TimerId_t tid2;

    func2();
    dsleep(0.01);
    tid2 = MT_StartTimer("call to func3 from func1", MACSIO_TIMING_ALL_GROUPS, iter++);
    func3();
    MT_StopTimer(tid2);
    func2();

    MT_StopTimer(tid);
}

int main(int argc, char **argv)
{
    int i, rank = 0, size = 1;
    MACSIO_TIMING_TimerId_t a, b;
    char **timer_strs;
    int ntimer_strs, maxstrlen;

#ifdef HAVE_MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

    MACSIO_LOG_DebugLevel = 1; /* should only see debug messages level 1 and 2 */
    srandom(0xDeadBeef); /* used to vary length of some timers */

    if (size > 8)
    {
        if (!rank)
            fprintf(stderr, "This test only appropriate for 8 or fewer processors\n");
#ifdef HAVE_MPI
        MPI_Abort(MPI_COMM_WORLD, 1);
#endif
        exit(1);
    }

    a = MT_StartTimer("main", MACSIO_TIMING_ALL_GROUPS, 0);

    func1();

    if (rank > 2)
        func4();

    b = MT_StartTimer("call to func3 from main", MACSIO_TIMING_ALL_GROUPS, 0);
    func3();
    MT_StopTimer(b);

    if (rank < 2)
        func1();

    MT_StopTimer(a);

    MACSIO_TIMING_DumpTimersToStrings(MACSIO_TIMING_ALL_GROUPS, &timer_strs, &ntimer_strs, &maxstrlen);

#ifdef HAVE_MPI
    {
        int rbuf[2], sbuf[2] = {ntimer_strs, maxstrlen};
        MPI_Allreduce(sbuf, rbuf, 2, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
        MACSIO_LOG_MainLog = MACSIO_LOG_LogInit(MPI_COMM_WORLD, "tsttiming.log", rbuf[1]+32, 2*rbuf[0]+4,0);
    }
#else
    MACSIO_LOG_MainLog = MACSIO_LOG_LogInit(0, "tsttiming.log", maxstrlen+4, ntimer_strs+4, 0);
#endif

    for (i = 0; i < ntimer_strs; i++)
    {
        MACSIO_LOG_MSG(Dbg1, (timer_strs[i]));
        free(timer_strs[i]);
    }
    free(timer_strs);

#ifdef HAVE_MPI
    MACSIO_TIMING_ReduceTimers(MPI_COMM_WORLD, 0);
    if (!rank)
    {
        MACSIO_TIMING_DumpReducedTimersToStrings(MACSIO_TIMING_ALL_GROUPS, &timer_strs, &ntimer_strs, &maxstrlen);
        MACSIO_LOG_MSG(Dbg1, ("#####################Reduced Timer Values######################"));
        for (i = 0; i < ntimer_strs; i++)
        {
            MACSIO_LOG_MSG(Dbg1, (timer_strs[i]));
            free(timer_strs[i]);
        }
        free(timer_strs);
    }
#endif

    MACSIO_LOG_LogFinalize(MACSIO_LOG_MainLog);

    MACSIO_TIMING_ClearTimers(MACSIO_TIMING_ALL_GROUPS);

#ifdef HAVE_MPI
    MPI_Finalize();
#endif

    return 0;
}
