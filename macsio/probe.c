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

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#ifdef USE_MPI
#include <mpi.h>
#endif

#define BUF_SIZE ((size_t)1<<27) /* 1/8th Gigabyte */
#define TOT_BUFS 8192            /* 1024 Gigabytes */

static int buf_ids[TOT_BUFS];

/* return time in micro-seconds to randomly stride
   (a fraction of a pagesize) through all bufs
   normalized by the number of samples taken. */
static double time_random_accesses(char **bufs, int nbufs, double *dummy_sum)
{
    struct timeval tv0, tv1;
#ifdef _SC_PAGESIZE
    int pagesize = (int) sysconf(_SC_PAGESIZE);
#elif PAGESIZE
    int pagesize = (int) sysconf(PAGESIZE);
#endif
    unsigned long long i = random() % pagesize;
    unsigned long long maxi = nbufs * BUF_SIZE;
    int n = 1;

    gettimeofday(&tv0, 0);
    double sum = 0;
    while (i < maxi)
    {
        char testval = *(bufs[i/BUF_SIZE] + i%BUF_SIZE);
        sum += ((double) ((int) testval));
        *(bufs[i/BUF_SIZE] + i%BUF_SIZE) = 'M';
        i += (random() % pagesize);
        n++;
    }
    gettimeofday(&tv1, 0);
    *dummy_sum = sum;

    return (double) (tv1.tv_sec*1e+6+tv1.tv_usec-(tv0.tv_sec*1e+6+tv0.tv_usec))/n;
}

int main(int argc, char **argv)
{
    char *bufs[TOT_BUFS];
    size_t size = 0;
    int done = 0, alldone;
    int i = 0;
    double mean=0, var=0;
    int mpirank = 0;
    int mpisize = 1;
    int mpierr;
    char outfname[64];
    FILE *outf;

#ifdef USE_MPI
    MPI_Init(&argc, &argv);
    MPI_Errhandler_set(MPI_COMM_WORLD, MPI_ERRORS_RETURN);
    MPI_Comm_size(MPI_COMM_WORLD, &mpisize);
    MPI_Comm_rank(MPI_COMM_WORLD, &mpirank);
#endif

    if (mpisize == 1)
        snprintf(outfname, sizeof(outfname), "probe.txt");
    else
        snprintf(outfname, sizeof(outfname), "probe-%05d.txt", mpirank);
    outf = fopen(outfname, "w");
    setbuf(outf, 0);

    /* loop to allocate more memory and test time to randomly access it */
    srandom(0xDeadBeef);
    fprintf(outf, "Size(Gb)\tSpeed(us)\tMean    \tStDev\n");
    while (!alldone)
    {
        double speed, size, dummy, lmean, stdev;
        int nlong = 0;
        bufs[i] = (char*) calloc(BUF_SIZE,1);
        if (!bufs[i])
        {
            int j;
            for (j = 0; j < i; j++)
                free(bufs[j]);
            break;
        }
        /* why 2 calls? Mallocs above don't actually do anything. The
           memory is only allocated when its written to. So, first call
           ensures all mallocs are indeed allocated. Second call then 
           times access w/o costs of lazy allocation involved */
        speed = time_random_accesses(bufs, i+1, &dummy);
        dummy = 0;
        speed = time_random_accesses(bufs, i+1, &dummy);
        lmean = mean;
        mean = mean + (speed-mean)/(i+1);
        var = var + (speed-mean)*(speed-lmean);
        stdev = sqrt(var/(i+1));
        size = (double) (i+1) * (double) BUF_SIZE / pow(2,30);
        fprintf(outf, "%8.4f\t%8.4f\t%8.4f\t%8.4f\n", size, speed, mean, stdev);
        if (i > 3 && speed > mean + 1.5*stdev)
        {
            nlong++;
            if (nlong > 1) done = 1;
        }
        else
        {
            nlong = 0;
        }
#if USE_MPI
        mpierr = MPI_Allreduce(&done, &alldone, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
        if (mpierr != MPI_SUCCESS)
            break;
#else
        alldone = done;
#endif
        i++;
    }
    fclose(outf);

#if USE_MPI
    MPI_Finalize();
#endif

}
