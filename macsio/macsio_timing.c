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

#include <macsio_timing.h>
#include <macsio_utils.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

#ifdef HAVE_CALIPER
#include <caliper/cali.h>
#endif

#include <cfloat>
#include <climits>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define MACSIO_TIMING_HASH_TABLE_SIZE 10007

int MACSIO_TIMING_UseMPI_Wtime = 1;

static double get_current_time()
{
    static int first = 1;

#ifdef HAVE_MPI
    if (MACSIO_TIMING_UseMPI_Wtime)
    {
        static double t0;
        if (first)
        {
            first = 0;
            t0 = MPI_Wtime();
            return 0;
        }
        return MPI_Wtime() - t0;
    }
#endif

#if defined(_WIN32)

    static struct _timeb T0;
           struct _timeb T1;
    int ms;

    /* initilize T0 */
    if (first)
    {
        first = 0;
        _ftime(&T0);
        return 0.0;
    }

    _ftime(T1);

    ms = (int) difftime(T1.time, T0.time);

    if (ms == 0)
    {
        ms = T1.millitm - T0.millitm;
    }
    else
    {
        ms =  ((ms - 1) * 1000);
        ms += (1000 - T0.millitm) + T1.millitm;
    }

    return (ms/1000.);

#else

    static struct timeval T0;
           struct timeval T1;

    if (first)
    {
        first = 0;
        gettimeofday(&T0, 0);
        return 0.0;
    }

    gettimeofday(&T1, 0);

    return (double) (T1.tv_sec - T0.tv_sec) +
           (double) (T1.tv_usec - T0.tv_usec) / 1000000.;

#endif
}

/* A small collection of strings to be associated with different
   timer groups. That is, collections of timers that are used to
   time different phases of some larger class of activity. For
   example for MIF-I/O, there are various activities that we may
   time including initial creates, handoffs (close->open) as well
   as each processor's work on the file. */
static int const maxTimerGroups = sizeof(MACSIO_TIMING_GroupMask_t)*8;
static char *timerGroupNames[maxTimerGroups];
static int timerGroupCount = 0;
static MACSIO_TIMING_GroupMask_t
group_mask_from_name(char const *grpName)
{
    int i;

    if (grpName == 0)
    {
        if (timerGroupCount == 0)
        {
            for (i = 0; i < maxTimerGroups; i++)
                timerGroupNames[i] = 0;
        }
        else
        {
            for (i = 0; i < maxTimerGroups; i++)
            {
                if (timerGroupNames[i])
                    free(timerGroupNames[i]);
                timerGroupNames[i] = 0;
            }
            timerGroupCount = 0;
        }
        return 0;
    }

    for (i = 0; i < maxTimerGroups; i++)
    {
        if (timerGroupNames[i] && !strcmp(timerGroupNames[i], grpName))
            return ((MACSIO_TIMING_GroupMask_t)1)<<i;
    }

    if (timerGroupCount == maxTimerGroups)
        return 0;

    timerGroupNames[timerGroupCount] = strdup(grpName);
    timerGroupCount++;
    return ((MACSIO_TIMING_GroupMask_t)1)<<(timerGroupCount-1);
}

MACSIO_TIMING_GroupMask_t
MACSIO_TIMING_GroupMask(char const *grpName)
{
    return group_mask_from_name(grpName);
}

typedef struct _timerInfo_t
{
    /* If you change this structure in any way, you need to change the code that
       creates its MPI datatype in ReduceTimers */
    int __line__;                    /**< Source file line # of StartTimer call */ 
    int iter_count;                  /**< Total number of iterations this timer was invoked. */
    int min_iter;                    /**< Iteration at which min time was seen */           
    int max_iter;                    /**< Iteration at which max time was seen */
    int min_rank;                    /**< MPI rank of processor with minimum time (only used in reductions) */
    int max_rank;                    /**< MPI rank of processor with maximum time (only used in reductions) */
    int iter_num;                    /**< Iteration number of current timer */
    int depth;                       /**< Depth of this timer relative to other active timers */
    int is_restart;                  /**< Is this timer restarting the current iteration */

    double total_time;               /**< Total cummulative time spent in this timer over all iterations */
    double min_time;                 /**< Min over all iterations this timer ran */
    double max_time;                 /**< Mix over all iterations this timer ran */
    double running_mean;             /**< Running mean of timer iterations */
    double running_var;              /**< Running variance of timer iterations */
    double start_time;               /**< Time at which current iteration of this timer was started */
    double total_time_this_iter;     /**< Cummulative time spent in the current iteration (for restarts) */

    MACSIO_TIMING_GroupMask_t gmask; /**< User defined bit mask for group membership of this timer. */

    char __file__[32];               /**< Source file name for StartTimer call */
    char label[64];                  /**< User defined label given to the timer */
} timerInfo_t;

static timerInfo_t timerHashTable[MACSIO_TIMING_HASH_TABLE_SIZE];
static timerInfo_t reducedTimerTable[MACSIO_TIMING_HASH_TABLE_SIZE];

#ifdef HAVE_CALIPER
typedef struct _caliperAttributeInfo_t {
    cali_id_t attr;                  /**< Caliper attribute id for the timer */
    cali_id_t iter_attr;             /**< Caliper attribute id for the iteration */
} caliperAttributeInfo_t;

static caliperAttributeInfo_t caliperAttributeInfo[MACSIO_TIMING_HASH_TABLE_SIZE];
#endif

MACSIO_TIMING_TimerId_t MACSIO_TIMING_StartTimer(
    char const *label,
    MACSIO_TIMING_GroupMask_t gmask,
    int iter_num,
    char const *__file__,
    int __line__
)
{
    int n = 0;
    int len = strlen(__file__) + strlen(label) + 64;
    char* _label = (char *) malloc(len);
    int len2 = snprintf(_label, len, "%s:%05d:%016llX:%s", __file__, __line__, gmask, label);
    MACSIO_TIMING_TimerId_t tid = MACSIO_UTILS_BJHash((unsigned char*)_label, len2, 0) % MACSIO_TIMING_HASH_TABLE_SIZE;
    int inc = (tid > MACSIO_TIMING_HASH_TABLE_SIZE / 2) ? -1 : 1;

#ifdef HAVE_CALIPER
    char* _cali_iter_label = NULL;
#endif
    
    free(_label);

    /* Find the timer's slot in the hash table */
    while (n < MACSIO_TIMING_HASH_TABLE_SIZE)
    {
        if (!strlen(timerHashTable[tid].label))
        {
            /* Starting a new timer for the first time/iteration */
            strncpy(timerHashTable[tid].__file__, __file__, sizeof(timerHashTable[tid].__file__));
            timerHashTable[tid].__line__ = __line__;
            strncpy(timerHashTable[tid].label, label, sizeof(timerHashTable[tid].label));
            timerHashTable[tid].gmask = gmask;

            timerHashTable[tid].total_time = 0;
            timerHashTable[tid].iter_count = 0;
            timerHashTable[tid].min_time =  DBL_MAX;
            timerHashTable[tid].max_time = -DBL_MAX;
            timerHashTable[tid].min_iter =  INT_MAX;
            timerHashTable[tid].max_iter = -INT_MAX;
            timerHashTable[tid].running_mean = 0;
            timerHashTable[tid].running_var = 0;
            timerHashTable[tid].iter_num = iter_num;
            timerHashTable[tid].total_time_this_iter = 0;
            timerHashTable[tid].is_restart = 0;

            timerHashTable[tid].depth = 0;
            timerHashTable[tid].start_time = get_current_time();

#ifdef HAVE_CALIPER
            _cali_iter_label = (char*) malloc(5 + 1 + strlen(label));
            snprintf(_cali_iter_label, len, "iter#%s", label);

            caliperAttributeInfo[tid].attr = cali_find_attribute("annotation");
            caliperAttributeInfo[tid].iter_attr =
                cali_create_attribute(_cali_iter_label, CALI_TYPE_INT, CALI_ATTR_ASVALUE | CALI_ATTR_SKIP_EVENTS);

            free(_cali_iter_label);

            cali_begin_int(caliperAttributeInfo[tid].iter_attr, timerHashTable[tid].iter_num);
            cali_begin_string(caliperAttributeInfo[tid].attr, timerHashTable[tid].label);
#endif
            return tid;
        }

        if (strncmp(timerHashTable[tid].label, label, sizeof(timerHashTable[tid].label)) == 0 &&
            strncmp(timerHashTable[tid].__file__, __file__, sizeof(timerHashTable[tid].__file__)) == 0 &&
            timerHashTable[tid].__line__ == __line__)
        {
            /* Another iteration of or re-starting an existing timer */
            timerHashTable[tid].is_restart = 0;
            if (iter_num == timerHashTable[tid].iter_num)
                timerHashTable[tid].is_restart = 1;
            else if (iter_num == MACSIO_TIMING_ITER_AUTO)
                timerHashTable[tid].iter_num++;
            else
                timerHashTable[tid].iter_num = iter_num;
            timerHashTable[tid].start_time = get_current_time();

#ifdef HAVE_CALIPER
            cali_begin_int(caliperAttributeInfo[tid].iter_attr, timerHashTable[tid].iter_num);
            cali_begin_string(caliperAttributeInfo[tid].attr, timerHashTable[tid].label);
#endif
            
            return tid;
        }

        /* We have a collision. Try next slot in table. */
        if (inc == 1 && tid == MACSIO_TIMING_HASH_TABLE_SIZE-1)
            tid = 0;
        else if (inc == -1 && tid == 0)
            tid = MACSIO_TIMING_HASH_TABLE_SIZE-1;
        else
            tid += inc;
        n++;
    }

//#warning LOG THIS ERROR
    /* log a fatal error */

    return MACSIO_TIMING_INVALID_TIMER;
}

double MACSIO_TIMING_StopTimer(MACSIO_TIMING_TimerId_t tid)
{
    double stop_time = get_current_time();
    double timer_time = stop_time - timerHashTable[tid].start_time;

    if (tid >= MACSIO_TIMING_HASH_TABLE_SIZE) return DBL_MAX;

#ifdef HAVE_CALIPER
    cali_end(caliperAttributeInfo[tid].attr);
    cali_end(caliperAttributeInfo[tid].iter_attr);
#endif
    
    if (timerHashTable[tid].is_restart)
    {
        timerHashTable[tid].total_time_this_iter += timer_time;
        timerHashTable[tid].total_time += timer_time;
//#warning UPDATE MIN/MAX TOO
    }
    else
    {
        double var;
        int n = timerHashTable[tid].iter_count;
        double x = timer_time;
        double mean = timerHashTable[tid].running_mean;
        double M2 = timerHashTable[tid].running_var;
        double delta = x - mean;

        /* Running or "online" algorithm (due to Knuth) for updating mean and variance
           via recurrence relation */
        n++;
        mean = mean + delta / n;
        M2 = M2 + delta * (x - mean);
        if (n < 2)
            var = 0;
        else
            var = M2/(n-1);

        /* Now, update the time record */
        timerHashTable[tid].iter_count = n;
        timerHashTable[tid].running_mean = mean;
        timerHashTable[tid].running_var = var;
        timerHashTable[tid].total_time += timer_time;

        if (timer_time < timerHashTable[tid].min_time)
        {
            timerHashTable[tid].min_time = timer_time;
            timerHashTable[tid].min_iter = timerHashTable[tid].iter_num;
        }
        if (timer_time > timerHashTable[tid].max_time)
        {
            timerHashTable[tid].max_time = timer_time;
            timerHashTable[tid].max_iter = timerHashTable[tid].iter_num;
        }
    }

    return timer_time;
}

static double
get_timer_datum(
    timerInfo_t const *table,
    MACSIO_TIMING_TimerId_t tid,
    char const *field)
{
    if (tid >= MACSIO_TIMING_HASH_TABLE_SIZE) return -1;

    if      (!strncmp(field, "__line__", 8))
        return table[tid].__line__;
    else if (!strncmp(field, "start_time", 10))
        return table[tid].start_time;
    else if (!strncmp(field, "iter_count", 10))
        return table[tid].iter_count;
    else if (!strncmp(field, "iter_time", 9))
        return table[tid].total_time_this_iter;
    else if (!strncmp(field, "min_iter", 8))
        return table[tid].min_iter;
    else if (!strncmp(field, "max_iter", 8))
        return table[tid].max_iter;
    else if (!strncmp(field, "min_rank", 8))
        return table[tid].min_rank;
    else if (!strncmp(field, "max_rank", 8))
        return table[tid].max_rank;
    else if (!strncmp(field, "iter_num", 8))
        return table[tid].iter_num;
    else if (!strncmp(field, "depth", 5))
        return table[tid].depth;
    else if (!strncmp(field, "total_time", 10))
        return table[tid].total_time;
    else if (!strncmp(field, "min_time", 8))
        return table[tid].min_time;
    else if (!strncmp(field, "max_time", 8))
        return table[tid].max_time;
    else if (!strncmp(field, "running_mean", 12))
        return table[tid].running_mean;
    else if (!strncmp(field, "running_var", 11))
        return table[tid].running_var;

    return -1;
}

double MACSIO_TIMING_GetTimerDatum(MACSIO_TIMING_TimerId_t tid, char const *field)
{
    return get_timer_datum(timerHashTable, tid, field);
}

double MACSIO_TIMING_GetReducedTimerDatum(MACSIO_TIMING_TimerId_t tid, char const *field)
{
    return get_timer_datum(reducedTimerTable, tid, field);
}

static void
clear_timers(timerInfo_t *table, MACSIO_TIMING_GroupMask_t gmask)
{
    int i;
    for (i = 0; i < MACSIO_TIMING_HASH_TABLE_SIZE; i++)
    {
        if (!strlen(table[i].label)) continue;

        if (!(table[i].gmask & gmask)) continue;

        memset(table[i].label, 0, sizeof(table[i].label));
        memset(table[i].__file__, 0, sizeof(table[i].__file__));
        table[i].__line__ = 0;
        table[i].gmask = 0;

        table[i].total_time = 0;
        table[i].iter_count = 0;
        table[i].min_time =  DBL_MAX;
        table[i].max_time = -DBL_MAX;
        table[i].min_iter =  INT_MAX;
        table[i].max_iter = -INT_MAX;
        table[i].running_mean = 0;
        table[i].running_var = 0;
        table[i].iter_num = 0;
        table[i].total_time_this_iter = 0;

        table[i].is_restart = 0;
        table[i].depth = 0;
        table[i].start_time = 0;
    }
}

#ifdef HAVE_MPI
static void
reduce_a_timerinfo(
    void *a,		/**< [in] first input for MPI_User_function */
    void *b,		/**< [in,out] second input arg for MPI_User_function and reduced output */
    int *len,		/**< [in] number of values in A and B buffers */
    MPI_Datatype *type	/**< [in] type of values in A and B buffers */
)
{
    int i;
    timerInfo_t *a_info = (timerInfo_t*) a;
    timerInfo_t *b_info = (timerInfo_t*) b;

    for (i = 0; i < *len; i++)
    {
        if (strlen(a_info[i].label) == 0 && strlen(b_info[i].label) == 0)
            continue;

        /* If filenames don't match, record that fact by setting b (out) to all '~' chars */
        if (strcmp(a_info[i].__file__, b_info[i].__file__))
        {
            int j = 0;
            while (b_info[i].__file__[j])
                b_info[i].__file__[j++] = '~';
        }
    
        /* If line numbers don't match, record that fact as INT_MAX */
        if (a_info[i].__line__ != b_info[i].__line__)
            b_info[i].__line__ = INT_MAX;

        /* If labels don't match, record that fact by setting b (out) to all '~' chars */
        if (strcmp(a_info[i].label, b_info[i].label))
        {
            int j = 0;
            while (b_info[i].label[j])
                b_info[i].label[j++] = '~';
        }
    
        /* If groups don't match, record that fact as ALL_GROUPS */
        if (a_info[i].gmask != b_info[i].gmask)
            b_info[i].gmask = MACSIO_TIMING_ALL_GROUPS;

        b_info[i].total_time += a_info[i].total_time;

        if (a_info[i].min_time < b_info[i].min_time)
        {
            b_info[i].min_time = a_info[i].min_time;
            b_info[i].min_iter = a_info[i].min_iter;
            b_info[i].min_rank = a_info[i].min_rank;
        }

        if (a_info[i].max_time > b_info[i].max_time)
        {
            b_info[i].max_time = a_info[i].max_time;
            b_info[i].max_iter = a_info[i].max_iter;
            b_info[i].max_rank = a_info[i].max_rank;
        }

        /* Handle running update to mean and variance */
        {
            double cnt_a = a_info[i].iter_count;
            double cnt_b = b_info[i].iter_count;
            double avg_a = a_info[i].running_mean;
            double avg_b = b_info[i].running_mean;
            double var_a = a_info[i].running_var;
            double var_b = b_info[i].running_var;

            double avg, var;
            double cnt = cnt_a + cnt_b;
            double cnt_ratio = cnt_a > cnt_b ? cnt_a / cnt_b : cnt_b / cnt_a;
            double delta = avg_b - avg_a;

            if (cnt_ratio < 1.01 && cnt_a > 1e+4)
                avg = (cnt_a * avg_a + cnt_b * avg_b) / cnt;
            else
                avg = avg_a + delta * cnt_b / cnt;

            if (cnt < 2)
                var = 0;
            else
                var = var_a + var_b + delta * delta * cnt_a * cnt_b / cnt;

            b_info[i].iter_count = cnt;
            b_info[i].running_mean = avg;
            b_info[i].running_var = var;
        }
    }
}
#endif

void
MACSIO_TIMING_ReduceTimers(
#ifdef HAVE_MPI
    MPI_Comm comm,
#else
    int comm,
#endif
    int root
)
{
    static int first = 1;
#ifdef HAVE_MPI
    static MPI_Op timerinfo_reduce_op;
    static MPI_Datatype str_32_mpi_type;
    static MPI_Datatype str_64_mpi_type;
    static MPI_Datatype timerinfo_mpi_type;
    int i, rank = 0;

    MPI_Comm_rank(comm, &rank);

    if (root == -1)
    {
        MPI_Op_free(&timerinfo_reduce_op);
        MPI_Type_free(&str_32_mpi_type);
        MPI_Type_free(&str_64_mpi_type);
        MPI_Type_free(&timerinfo_mpi_type);
        first = 1;
        return;
    }

    if (first)
    {
        int i;
        MPI_Aint offsets[5];
        int lengths[5];
        MPI_Datatype types[5];

        MPI_Op_create(reduce_a_timerinfo, 0, &timerinfo_reduce_op);
        MPI_Type_contiguous(32, MPI_CHAR, &str_32_mpi_type);
        MPI_Type_commit(&str_32_mpi_type);
        MPI_Type_contiguous(64, MPI_CHAR, &str_64_mpi_type);
        MPI_Type_commit(&str_64_mpi_type);

        lengths[0] = 9;
        types[0] = MPI_INT;
        MPI_Get_address(&timerHashTable[0], offsets);
        lengths[1] = 7;
        types[1] = MPI_DOUBLE;
        MPI_Get_address(&timerHashTable[0].total_time, offsets+1);
        lengths[2] = 1;
        types[2] = MPI_UNSIGNED_LONG_LONG;
        MPI_Get_address(&timerHashTable[0].gmask, offsets+2);
        lengths[3] = 1;
        types[3] = str_32_mpi_type;
        MPI_Get_address(&timerHashTable[0].__file__[0], offsets+3);
        lengths[4] = 1;
        types[4] = str_64_mpi_type;
        MPI_Get_address(&timerHashTable[0].label[0], offsets+4);
        for (i = 4; i >= 0; offsets[i] -= offsets[0], i--);
        MPI_Type_create_struct(5, lengths, offsets, types, &timerinfo_mpi_type);
        MPI_Type_commit(&timerinfo_mpi_type);

        first = 0;
    }

    clear_timers(reducedTimerTable, MACSIO_TIMING_ALL_GROUPS);
    for (i = 0; i < MACSIO_TIMING_HASH_TABLE_SIZE; i++)
        timerHashTable[i].min_rank = timerHashTable[i].max_rank = rank;

    MPI_Reduce(timerHashTable, reducedTimerTable, MACSIO_TIMING_HASH_TABLE_SIZE,
        timerinfo_mpi_type, timerinfo_reduce_op, root, comm);
#endif
}

static void
dump_timers_to_strings(
    timerInfo_t const *table,
    MACSIO_TIMING_GroupMask_t gmask,
    char ***strs,
    int *nstrs,
    int *maxlen
)
{
    char **_strs;
    int pass, _nstrs, _maxlen = 0;
    int const max_str_size = 1024;

    for (pass = 0; pass < 2; pass++)
    {
        int i;

        _nstrs = 0;
        for (i = 0; i < MACSIO_TIMING_HASH_TABLE_SIZE; i++)
        {
            int len;
            double min_in_stddev_steps_from_mean = 0, max_in_stddev_steps_from_mean = 0;
            double dev;

            if (!strlen(table[i].label)) continue;

            if (!(table[i].gmask & gmask)) continue;

            _nstrs++;
            if (pass == 0) continue; /* only count the strings on first pass */

            _strs[_nstrs-1] = (char *) malloc(max_str_size);

            dev = sqrt(table[i].running_var);
            if (dev > 0)
            {
                min_in_stddev_steps_from_mean = (table[i].running_mean - table[i].min_time) / dev;
                max_in_stddev_steps_from_mean = (table[i].max_time - table[i].running_mean) / dev;
            }

//#warning USE COLUMN HEADINGS INSTEAD
//#warning HANDLE INDENTATION HERE
            len = snprintf(_strs[_nstrs-1], max_str_size,
                "TOT=%10.5f,CNT=%04d,MIN=%8.5f(%4.2f):%06d,AVG=%8.5f,MAX=%8.5f(%4.2f):%06d,DEV=%8.8f:FILE=%s:LINE=%d:LAB=%s",
                table[i].total_time,
                table[i].iter_count,
                table[i].min_time, min_in_stddev_steps_from_mean, table[i].min_rank,
                table[i].running_mean,
                table[i].max_time, max_in_stddev_steps_from_mean, table[i].max_rank,
                dev,
                table[i].__file__,
                table[i].__line__,
                table[i].label);

            if (len > _maxlen) _maxlen = len;
        }

        if (pass == 0)
            _strs = (char **) malloc(_nstrs * sizeof(char*));
    }

    *strs = _strs;
    *nstrs = _nstrs;
    *maxlen = _maxlen;
}

void
MACSIO_TIMING_DumpTimersToStrings(
    MACSIO_TIMING_GroupMask_t gmask,
    char ***strs,
    int *nstrs,
    int *maxlen
)
{
    dump_timers_to_strings(timerHashTable, gmask, strs, nstrs, maxlen);
}

void MACSIO_TIMING_DumpReducedTimersToStrings(
    MACSIO_TIMING_GroupMask_t gmask,
    char ***strs,
    int *nstrs,
    int *maxlen
)
{
    dump_timers_to_strings(reducedTimerTable, gmask, strs, nstrs, maxlen);
}

void MACSIO_TIMING_ClearTimers(MACSIO_TIMING_GroupMask_t gmask)
{
    clear_timers(timerHashTable, gmask);
    clear_timers(reducedTimerTable, MACSIO_TIMING_ALL_GROUPS);
}

double MACSIO_TIMING_GetCurrentTime(void)
{
    return get_current_time();
}
