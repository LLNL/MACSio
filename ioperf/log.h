#ifndef _LOG_H
#define _LOG_H

#ifdef PARALLEL
#include <mpi.h>
#else
typedef int MPI_Comm;
#endif

#include <stdio.h>

typedef struct MACSIO_LogHandle_t
{
    int logfile;
    int rank;
    int size;
    int log_line_length;
    int lines_per_proc;
    int current_line;
} MACSIO_LogHandle_t;

extern MACSIO_LogHandle_t *Log_Init(MPI_Comm comm, char const *path, int line_len, int lines_per_proc);
extern void Log(MACSIO_LogHandle_t *log, char const *msg);
extern void Log_Finalize(MACSIO_LogHandle_t *log);

#endif /* _LOG_H */
