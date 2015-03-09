#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <log.h>

void Log(MACSIO_LogHandle_t *log, char const *fmt, ...)
{
    int i = 0;
    off_t seek_offset;
    char *msg = (char *) malloc(log->log_line_length);
    char *buf = (char *) malloc(log->log_line_length);
    va_list ptr;

    va_start(ptr, fmt);
    vsnprintf(msg, log->log_line_length-1, fmt, ptr);
    va_end(ptr);
    msg[log->log_line_length-1] = '\0';

    while (i < strlen(msg) && i < log->log_line_length)
    {
        if (msg[i] == '\n')
            buf[i] = ' ';
        else
            buf[i] = msg[i];
        i++;
    }
    while (i < log->log_line_length)
        buf[i++] = ' ';
    buf[log->log_line_length-1] = '\n';

    seek_offset = (log->rank * log->lines_per_proc + log->current_line) * log->log_line_length;

    i = pwrite(log->logfile, buf, sizeof(char) * log->log_line_length, seek_offset);
    free(buf);

    log->current_line++;
    if (log->current_line == log->lines_per_proc)
        log->current_line = 1;
}

MACSIO_LogHandle_t *Log_Init(MPI_Comm comm, char const *path, int line_len, int lines_per_proc)
{
    int rank=0, size=1;
    MACSIO_LogHandle_t *retval;

#ifdef HAVE_MPI
    MPI_Comm_size(comm, &size);
    MPI_Comm_rank(comm, &rank);
#endif

    if (rank == 0)
    {
        int i, filefd;
        char *linbuf = (char*) malloc(line_len * lines_per_proc * sizeof(char));
        memset(linbuf, '-', line_len * sizeof(char));
        memset(linbuf+line_len, ' ', line_len * (lines_per_proc-1) * sizeof(char));
        for (i = 0; i < lines_per_proc; i++)
            linbuf[(i+1)*line_len-1] = '\n';
        filefd = open(path, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);
        for (i = 0; i < size; i++)
        {
            char tmp[32];
            sprintf(tmp, "Processor %06d", i);
            memcpy(linbuf+line_len/2-strlen(tmp)/2, tmp, strlen(tmp));
            write(filefd, linbuf, sizeof(char) * line_len * lines_per_proc);
        }
        close(filefd);
    }

#ifdef HAVE_MPI
    MPI_Barrier(comm);
#endif

    retval = (MACSIO_LogHandle_t *) malloc(sizeof(MACSIO_LogHandle_t));
    /*retval->logfile = open(path, O_WRONLY|O_NONBLOCK);*/
    retval->logfile = open(path, O_WRONLY);
    retval->size = size;
    retval->rank = rank;
    retval->log_line_length = line_len;
    retval->lines_per_proc = lines_per_proc;
    retval->current_line = 1;
    return retval;
}

void Log_Finalize(MACSIO_LogHandle_t *log)
{
    close(log->logfile);
    free(log);
}
