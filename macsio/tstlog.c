#include <errno.h>

#include <macsio_log.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

int main (int argc, char **argv)
{
    int rank=0, size=1;

#ifdef HAVE_MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

    MACSIO_LOG_DebugLevel = 2; /* should only see debug messages level 1 and 2 */
    MACSIO_LOG_MainLog = MACSIO_LOG_LogInit(MPI_COMM_WORLD, "tstlog.log", 128, 20);
    MACSIO_LOG_StdErr = MACSIO_LOG_LogInit(MPI_COMM_WORLD, 0, 0, 0);

    if (rank == 1)
    {
        MACSIO_LOG_MSG(Dbg1, ("I am staring with processor 1"));
        MACSIO_LOG_MSG(Dbg2, ("Test output of a very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very, very long line"));
    }
    else if (rank == 2)
    {
        MACSIO_LOG_MSG(Warn, ("Starting on proc 2"));
        MACSIO_LOG_LogMsg(MACSIO_LOG_StdErr, "Logging a message to stderr for rank %d", rank);
    }
    else if (rank == 0)
    {
        errno = EOVERFLOW;
        mpi_errno = MPI_ERR_COMM;
        MACSIO_LOG_MSG(Err, ("I am here on proc 0"));
    }
    else
    {
        int i;
        MACSIO_LOG_MsgSeverity_t sev = rank%2?MACSIO_LOG_MsgDbg2:MACSIO_LOG_MsgDbg3;
        for (i = 0; i < 25; i++)
        {
            MACSIO_LOG_MSGV(sev, ("Outputing line %d for rank %d\n", i, rank));
        }
    }

    MACSIO_LOG_LogFinalize(MACSIO_LOG_StdErr);
    MACSIO_LOG_LogFinalize(MACSIO_LOG_MainLog);

#ifdef HAVE_MPI
    MPI_Finalize();
#endif

    return 0;

}
