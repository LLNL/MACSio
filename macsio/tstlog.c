#include <log.h>

#ifdef PARALLEL
#include <mpi.h>
#endif

int main (int argc, char **argv)
{
    int rank=0, size=1;
    MACSIO_LogHandle_t *log;

#ifdef PARALLEL
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

    log = Log_Init(MPI_COMM_WORLD, "tstlog.out", 128, 20);

    if (rank == 1)
    {
        Log(log, "I am staring with processor 1");
        Log(log, "Test output of a very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very long line");
    }
    else if (rank == 2)
    {
        Log(log, "Starting on proc 2");
    }
    else if (rank == 0)
    {
        Log(log, "I am here on proc 0");
    }
    else
    {
        int i;
        for (i = 0; i < 25; i++)
        {
            char tmp[256];
            sprintf(tmp, "Outputing line %d for rank %d\n", i, rank);
            Log(log, tmp);
        }
    }

    Log_Finalize(log);

#ifdef PARALLEL
    MPI_Finalize();
#endif

    return 0;

}
