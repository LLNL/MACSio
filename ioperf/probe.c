#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#ifdef USE_GSL
#include <gsl/gsl_statistics.h>
#endif

#ifdef USE_MPI
#include <mpi.h>
#endif

#define STAT_WIN_SIZE 10
#define MAX_RAND (pow(2,32)-1)
#define BUF_SIZE ((size_t)1<<30) /* 1/4 Gigabyte */
#define TOT_BUFS 262144          /* 1024 Gigabytes */

static double time_random_accesses(char **bufs, int nbufs, int ntries, double *dummy_sum)
{
    int i;
    struct timeval tv0, tv1;

    gettimeofday(&tv0, 0);
    double sum = 0;
    for (i = 0; i < ntries; i++)
    {
        double frac = (double) random() / (double) MAX_RAND;
        assert(frac<1);
        size_t j = (size_t) ((double) nbufs * (double) BUF_SIZE * frac);
        char testval = *(bufs[j/BUF_SIZE] + j%BUF_SIZE);
        sum += ((double) ((int) testval));
        *(bufs[j/BUF_SIZE] + j%BUF_SIZE) = 'M';
    }
    gettimeofday(&tv1, 0);
    *dummy_sum = sum;

    return (double) (tv1.tv_sec*1e+6+tv1.tv_usec-(tv0.tv_sec*1e+6+tv0.tv_usec))/1e+3;

}

int main(int argc, char **argv)
{
    char *bufs[TOT_BUFS];
    size_t size = 0;
    int done = 0, alldone;
    int i = 0;
    int mpirank = 0;
    int mpisize = 1;
    int mpierr;
    char outfname[64];
    FILE *outf;
    double stat_window[STAT_WIN_SIZE];

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
    fprintf(outf, "Size(Gb)\tSpeed(ms)\tMean    \tStDev\n");
    while (!alldone)
    {
        double speed, size, dummy, mean=0, stdev=0;
        bufs[i] = (char*) calloc(BUF_SIZE,1);
        if (!bufs[i])
        {
            int j;
            for (j = 0; j < i; j++)
                free(bufs[j]);
            break;
        }
        speed = time_random_accesses(bufs, i+1, 1000000, &dummy);
        stat_window[i%STAT_WIN_SIZE] = speed;
        if (i >= STAT_WIN_SIZE - 1)
        {
#ifdef USE_GSL
            mean = gsl_stats_mean(stat_window, 1, STAT_WIN_SIZE);
            stdev = gsl_stats_sd_m(stat_window, 1, STAT_WIN_SIZE, mean);
            if (speed > mean + 10*stdev) done=1;
#endif
        }
        size = (double) (i+1) * (double) BUF_SIZE / (double) (1<<16) / (double) (1<<16);
        fprintf(outf, "%8.4f\t%8.4f\t%8.4f\t%8.4f\n", size, speed, mean, stdev);
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
