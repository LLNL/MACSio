#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <macsio_log.h>

int MACSIO_LOG_DebugLevel = 0;
int mpi_errno = MPI_SUCCESS;
MACSIO_LOG_LogHandle_t *MACSIO_LOG_MainLog = 0;
MACSIO_LOG_LogHandle_t *MACSIO_LOG_StdErr = 0;

/* really just an internal function called via MACSIO_ERROR macro */
char const *
MACSIO_LOG_make_msg(const char *format, /* A printf-like error message. */
         ...                 /* Variable length debugging info specified by
                              * format to be printed out. */
         )
{
  static char error_buffer[1024];
  static int error_buffer_ptr = 0;
  size_t L,Lmax;
  char   tmp[sizeof(error_buffer)];
  va_list ptr;

  va_start(ptr, format);

  vsprintf(tmp, format, ptr);
  L    = strlen(tmp);
  Lmax = sizeof(error_buffer) - error_buffer_ptr - 1;
  if (Lmax < L)
     tmp[Lmax-1] = '\0';
  strcpy(error_buffer + error_buffer_ptr,tmp);

  va_end(ptr);

  return error_buffer;
}

void MACSIO_LOG_LogMsg(MACSIO_LOG_LogHandle_t *log, char const *fmt, ...)
{
    int i = 0;
    int is_stderr = log->logfile == fileno(stderr);
    char *msg, *buf;
    va_list ptr;

    msg = (char *) malloc(log->log_line_length);
    buf = (char *) malloc(log->log_line_length);

    if (is_stderr) sprintf(msg, "%06d: ", log->rank);
    va_start(ptr, fmt);
    vsnprintf(log?msg:&msg[8], log->log_line_length-1, fmt, ptr);
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
    if (is_stderr)
    {
        buf[i++] = '\n';
        buf[i++] = '\0';
    }
    else
    {
        while (i < log->log_line_length)
            buf[i++] = ' ';
        buf[log->log_line_length-1] = '\n';
    }

    if (is_stderr)
    {
        write(log->logfile, buf, sizeof(char) * strlen(buf));
    }
    else
    {
        off_t seek_offset = (log->rank * log->lines_per_proc + log->current_line) * log->log_line_length;
        pwrite(log->logfile, buf, sizeof(char) * log->log_line_length, seek_offset);
    }
    free(buf);

    log->current_line++;
    if (log->current_line == log->lines_per_proc)
        log->current_line = 1;
}

#ifdef HAVE_MPI
MACSIO_LOG_LogHandle_t *MACSIO_LOG_LogInit(MPI_Comm comm, char const *path, int line_len, int lines_per_proc)
#else
MACSIO_LOG_LogHandle_t *MACSIO_LOG_LogInit(int comm, char const *path, int line_len, int lines_per_proc)
#endif
{
    int rank=0, size=1;
    MACSIO_LOG_LogHandle_t *retval;

#ifdef HAVE_MPI
    MPI_Comm_size(comm, &size);
    MPI_Comm_rank(comm, &rank);
#endif

    if (path && rank == 0)
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

    retval = (MACSIO_LOG_LogHandle_t *) malloc(sizeof(MACSIO_LOG_LogHandle_t));
    /*retval->logfile = open(path, O_WRONLY|O_NONBLOCK);*/
    retval->logfile = path?open(path, O_WRONLY):fileno(stderr);
    retval->size = size;
    retval->rank = rank;
    retval->log_line_length = path?line_len:1024;
    retval->lines_per_proc = path?lines_per_proc:1000000;
    retval->current_line = 1;
    return retval;
}

#warning ADD ATEXIT FUNCTIONALITY TO CLOSE LOGS
void MACSIO_LOG_LogFinalize(MACSIO_LOG_LogHandle_t *log)
{
    if (log->logfile != fileno(stderr))
        close(log->logfile);
    free(log);
}
