#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <timing.h>

static double GetCurrentTime()
{
    static int first = 1;

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

/* Ensure timer is initialized at load time. Again, this approach
   *requires* C++ compiler because it involves initialization with
   non-constant expressions. */
static int dummy = GetCurrentTime();

typedef struct timerec_t
{
    char *blockName;
    MACSIO_ioop_t op;
    double time;
    int depth;
} timerec_t;

static timerec_t *timerecs = 0;
static int timerecssize = 0;
static int ntimerecs = 0;

static void RecordTimer(char const *name, MACSIO_ioop_t op, double time, int depth)
{
    if (ntimerecs == timerecssize)
    {
        int newtimerecssize = 2 * timerecssize + 1;
        timerecs = (timerec_t *) realloc(timerecs, newtimerecssize * sizeof(timerec_t));
        timerecssize = newtimerecssize;
    }
    timerecs[ntimerecs].blockName = strdup(name);
    timerecs[ntimerecs].op = op;
    timerecs[ntimerecs].time = time;
    timerecs[ntimerecs].depth = depth;
    ntimerecs++;
}

void DumpTimings(FILE *f)
{
    int i;
    for (i = ntimerecs-1; i >= 0; i--)
    {
        fprintf(f?f:stdout, "%*s%s %f\n", 3*(timerecs[i].depth-1), " ", timerecs[i].blockName, timerecs[i].time);
        free(timerecs[i].blockName);
    }
    free(timerecs);
    timerecs = 0;
    timerecssize = 0;
    ntimerecs = 0;
}

#define INIT_TVSIZE 100
static double *timevals = (double*) calloc(INIT_TVSIZE, sizeof(double));
static int timevalssize = INIT_TVSIZE;
static int ntimevals = 0;
static int nslotsused = 0;

MACSIO_TimerId_t StartTimer()
{
    double time = GetCurrentTime();
    int i, idx = -1;

    nslotsused++;

    /* search for a unused slot (gets bad if we have many open timers) */
    for (i = 0; i < ntimevals && idx < 0; i++)
    {
        if (timevals[i] <= 0)
            idx = i;
    }

    if (idx != -1)
    {
        timevals[idx] = time;
        return (MACSIO_TimerId_t) idx;
    }

    /* increase size of open timers array if necessary (unlikely we'll ever get here) */
    if (ntimevals == timevalssize)
    {
        int newtimevalssize = 2 * timevalssize;
        timevals = (double*) realloc(timevals, newtimevalssize * sizeof(double));
        for (i = timevalssize; i < newtimevalssize; i++)
            timevals[i] = 0;
        timevalssize = newtimevalssize;
    }

    timevals[ntimevals] = time;
    ntimevals++;
    return (MACSIO_TimerId_t) (ntimevals-1);
}

double StopTimer(MACSIO_TimerId_t id, MACSIO_ioop_t op, char const *blockName)
{
    double deltat = GetCurrentTime() - timevals[(int)id];
    timevals[(int)id] = 0;
    RecordTimer(blockName, op, deltat, nslotsused);
    nslotsused--;
    return deltat;
}

#if 0
static void AddTimingInfo(ioop_t op, size_t size, double t0, double t1)
{
    static timeinfo_t *tinfo=0;
    static int i=0;
    static int max=100;

    if (op == OP_OUTPUT_TIMINGS || op == OP_OUTPUT_SUMMARY)
    {
        int j;
        const char *opnms[] = {"WRITE", "READ", "OPEN", "CLOSE", "SEEK", "ERROR"};
        double tottime=0, totwrtime=0, totrdtime=0, toterrtime=0;
        size_t totwrbytes=0, totrdbytes=0;
        double wrfastest=0, wrslowest=DBL_MAX, rdfastest=0, rdslowest=DBL_MAX;
        int wrfastesti=-1, wrslowesti=-1, rdfastesti=-1, rdslowesti=-1;
        double wravg, rdavg;

        if (op == OP_OUTPUT_TIMINGS)
            fprintf(stdout, "i\top\tt0\t\tt1\t\tsize\n");
        for (j=0; j<i; j++)
        {
            if (op == OP_OUTPUT_TIMINGS)
            {
                fprintf(stdout, "%d\t%s\t%f\t%f\t%zd\n",
                    j,opnms[tinfo[j].op],tinfo[j].t0,tinfo[j].t1,tinfo[j].size);
            }

            if (tinfo[j].op != OP_ERROR)
            {
                tottime += (tinfo[j].t1-tinfo[j].t0);
            }
            else
            {
                toterrtime += (tinfo[j].t1-tinfo[j].t0);
            }

            if (tinfo[j].op == OP_WRITE)
            {
                double wrtime = tinfo[j].t1-tinfo[j].t0;
                double wrspeed = wrtime?(tinfo[j].size/wrtime):0;
                totwrtime += wrtime;
                totwrbytes += tinfo[j].size;
                if (wrspeed > wrfastest)
                {
                    wrfastest = wrspeed;
                    wrfastesti = j;
                }
                if (wrspeed < wrslowest)
                {
                    wrslowest = wrspeed;
                    wrslowesti = j;
                }
            }
            else if (tinfo[j].op == OP_READ)
            {
                double rdtime = tinfo[j].t1-tinfo[j].t0;
                double rdspeed = rdtime?(tinfo[j].size/rdtime):0;
                totrdtime += rdtime;
                totrdbytes += tinfo[j].size;
                if (rdspeed > rdfastest)
                {
                    rdfastest = rdspeed;
                    rdfastesti = j;
                }
                if (rdspeed < rdslowest)
                {
                    rdslowest = rdspeed;
                    rdslowesti = j;
                }
            }
        }

        fprintf(stdout, "=============================================================\n");
        fprintf(stdout, "==========================Summary============================\n");
        fprintf(stdout, "=============================================================\n");
        if (totwrbytes)
        {
            fprintf(stdout, "**************************Writes****************************\n");
            fprintf(stdout, "Total:   %zd bytes in %f seconds = %f Mb/s\n",
                totwrbytes, tottime*1e-6, totwrbytes/(tottime*1e-6)/(1<<20));
            fprintf(stdout, "Average: %zd bytes in %f seconds = %f Mb/s\n",
                totwrbytes, totwrtime*1e-6, totwrbytes/(totwrtime*1e-6)/(1<<20));
            if (wrfastesti>=0)
                fprintf(stdout, "Fastest: %zd bytes in %f seconds = %f Mb/s (iter=%d)\n",
                tinfo[wrfastesti].size, (tinfo[wrfastesti].t1-tinfo[wrfastesti].t0)*1e-6,
                wrfastest*1e+6/(1<<20), wrfastesti);
            if (wrslowesti>=0)
                fprintf(stdout, "Slowest: %zd bytes in %f seconds = %f Mb/s (iter=%d)\n",
                tinfo[wrslowesti].size, (tinfo[wrslowesti].t1-tinfo[wrslowesti].t0)*1e-6,
                wrslowest*1e+6/(1<<20), wrslowesti);
        }
        if (totrdbytes)
        {
            fprintf(stdout, "**************************Reads*****************************\n");
            fprintf(stdout, "Total:   %zd bytes in %f seconds = %f Mb/s\n",
                totrdbytes, tottime*1e-6, totrdbytes/(tottime*1e-6)/(1<<20));
            fprintf(stdout, "Average: %zd bytes in %f seconds = %f Mb/s\n",
                totrdbytes, totrdtime*1e-6, totrdbytes/(totrdtime*1e-6)/(1<<20));
            if (rdfastesti>=0)
                fprintf(stdout, "Fastest: %zd bytes in %f seconds = %f Mb/s (iter=%d)\n",
                tinfo[rdfastesti].size, (tinfo[rdfastesti].t1-tinfo[rdfastesti].t0)*1e-6,
                rdfastest*1e+6/(1<<20),rdfastesti);
            if (rdslowesti>=0)
                fprintf(stdout, "Slowest: %zd bytes in %f seconds = %f Mb/s (iter=%d)\n",
                tinfo[rdslowesti].size, (tinfo[rdslowesti].t1-tinfo[rdslowesti].t0)*1e-6,
                rdslowest*1e+6/(1<<20), rdslowesti);
        } 

        return;
    }

    if (tinfo==0 || i==max-1)
    {
        max = max*1.5;
        tinfo = (timeinfo_t*) realloc(tinfo, max*sizeof(timeinfo_t));
    }

    tinfo[i].op = op;
    tinfo[i].t0 = t0;
    tinfo[i].t1 = t1;
    tinfo[i].size = size;
    i++;
}
#endif
