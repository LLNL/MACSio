#include <unistd.h>
#include <time.h>

#include <macsio_log.h>
#include <macsio_timing.h>

void func2();

void func4()
{
    static int iter = 0;
    int nanoSecsOfSleeps = 200000;
    struct timespec ts = {0, nanoSecsOfSleeps};
    MACSIO_TIMING_TimerId_t tid = MT_StartTimer("func4", MACSIO_TIMING_ALL_GROUPS, iter++);
    nanosleep(&ts, 0);
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
    sleep(2);
    MT_StopTimer(tid);
}

void func1()
{
    static int iter = 0;
    MACSIO_TIMING_TimerId_t tid = MT_StartTimer("func1", MACSIO_TIMING_ALL_GROUPS, iter);
    MACSIO_TIMING_TimerId_t tid2;

    func2();
    sleep(1);
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

    MACSIO_LOG_DebugLevel = 1; /* should only see debug messages level 1 */

    if (size > 8)
    {
        if (!rank)
            fprintf(stderr, "This test only appropriate for 8 or fewer processors\n");
        MPI_Abort(MPI_COMM_WORLD, 1);
    }

    if (!rank)
    {
        fprintf(stderr, "Note: This test involves several calls to "
            "sleep. It takes ~9 seconds to complete\n");
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
    MACSIO_LOG_MainLog = MACSIO_LOG_LogInit(MPI_COMM_WORLD, "foobar.log", ntimer_strs+4, maxstrlen+4);
#else
    MACSIO_LOG_MainLog = MACSIO_LOG_LogInit(0, "tsttiming.log", ntimer_strs+4, maxstrlen+4);
#endif

#if 0
    printf("ntimer_strs = %d, maxstrlen = %d\n", ntimer_strs, maxstrlen);

    for (i = 0; i < ntimer_strs; i++)
    {
        MACSIO_LOG_MSG(Dbg1, (timer_strs[i]));
        free(timer_strs[i]);
    }
    free(timer_strs);
#endif
    
    MACSIO_LOG_LogFinalize(MACSIO_LOG_MainLog);

#ifdef HAVE_MPI
    MPI_Finalize();
#endif

    return 0;
}
