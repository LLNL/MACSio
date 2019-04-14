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

#include <errno.h>
#include <stdlib.h>
#include <strings.h>

#include <macsio_log.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

int MACSIO_MAIN_Rank;
int MACSIO_MAIN_Size;
int MACSIO_MAIN_Comm;

int main (int argc, char **argv)
{
    int i;
    int rank=0, size=1;
    int num_cols = 128, num_rows = 20, extra_lines = 20;
#ifdef HAVE_MPI
    MPI_Comm comm = MPI_COMM_WORLD;
#else
    int comm = 0;
#endif

    for (i = 0; i < argc; i++)
    {
        if (!strncasecmp(argv[i], "num_cols=", 9))
            num_cols = strtol(argv[i]+9, 0, 10);
        else if (!strncasecmp(argv[i], "num_rows=", 9))
            num_rows = strtol(argv[i]+9, 0, 10);
        else if (!strncasecmp(argv[i], "extra_lines=", 9))
            extra_lines = strtol(argv[i]+9, 0, 10);
    }

#ifdef HAVE_MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_size(comm, &size);
    MPI_Comm_rank(comm, &rank);
#endif

    MACSIO_LOG_DebugLevel = 2; /* should only see debug messages level 1 and 2 */
    MACSIO_LOG_MainLog = MACSIO_LOG_LogInit(comm, "tstlog.log", num_cols, num_rows, extra_lines);
    MACSIO_LOG_StdErr = MACSIO_LOG_LogInit(comm, 0, 0, 0, 0);

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
#ifdef HAVE_MPI_
        mpi_errno = MPI_ERR_COMM;
#else
        mpi_errno = 0;
#endif

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
