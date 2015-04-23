#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <macsio_log.h>

/*!
\addtogroup MACSIO_LOG
@{
*/

int                     mpi_errno = MPI_SUCCESS;
int                     MACSIO_LOG_DebugLevel = 0;
MACSIO_LOG_LogHandle_t *MACSIO_LOG_MainLog = 0;
MACSIO_LOG_LogHandle_t *MACSIO_LOG_StdErr = 0;

typedef struct _log_flags_t
{
    unsigned int was_logged : 1; /**< Indicates if a message was ever logged to the log */
} log_flags_t;

typedef struct _MACSIO_LOG_LogHandle_t
{
#ifdef HAVE_MPI
    MPI_Comm comm;            /**< MPI Communicator of tasks that will issue messages to this log */
#else
    int comm;                 /**< Dummy arg for non-MPI compilation */
#endif
    char *pathname;           /**< Name of the log file */
    int logfile;              /**< Log file file descriptor */
    int rank;                 /**< Rank of the processor that created this log handle */
    int size;                 /**< Size of the communicator that created this log handle */
    int log_line_length;      /**< Maximum length of a message line in the log file */
    int lines_per_proc;       /**< Number of message lines allocated in the file for each processor */
#warning FIX USE OF MUTABLE HERE
    mutable int current_line; /**< Index into this processor's group of lines in the log file at
                                   which the next message will be written */
    mutable log_flags_t flags; /**< Informational flags regarding the log */
} MACSIO_LOG_LogHandle_t;

/*!
\brief Internal convenience method to build a message from a printf-style format string and args.

This method is public only because it is used within the \c MACSIO_LOG_MSG convenience macro.
*/
char const *
MACSIO_LOG_MakeMsg(
    char const *format, /**< [in] A printf-like error message format string. */
    ...                 /**< [in] Optional, variable length set of arguments for format to be printed out. */
)
{
#warning MAKE THIS THREAD SAFE BY ALLOCATING THE RETURNED STRING OR USE A LARGE CIRCULAR BUFFER
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

#warning ADD OPTION TO SPECIFY MORE LINES AT THE ROOT/MASTER PROCESSOR
/*!
\brief Initialize a log

All processors in the \c comm communicator must call this function
collectively with identical values for \c path, \c line_len, and \c lines_per_proc.

*/
MACSIO_LOG_LogHandle_t*
MACSIO_LOG_LogInit(
#ifdef HAVE_MPI
    MPI_Comm comm,        /**< [in] MPI Communicator of tasks that will issue messages to this log */
#else
    int comm,             /**< [in] Dummy arg for non-MPI compilation */
#endif
    char const *path,     /**< [in] The name of the log file */
    int line_len,         /**< [in] The length of each message line in the log file */
    int lines_per_proc    /**< [in] The number of message lines for each MPI task */
)
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
    mpi_errno = MPI_Barrier(comm);
#endif

    retval = (MACSIO_LOG_LogHandle_t *) malloc(sizeof(MACSIO_LOG_LogHandle_t));
    retval->pathname = path?strdup(path):0;
    retval->comm = comm;
#warning TURN OFF BUFFERING ON THE LOG FILE
    /*retval->logfile = open(path, O_WRONLY|O_NONBLOCK);*/
    retval->logfile = path?open(path, O_WRONLY):fileno(stderr);
    retval->size = size;
    retval->rank = rank;
    retval->log_line_length = path?line_len:1024;
    retval->lines_per_proc = path?lines_per_proc:1000000;
    retval->current_line = 1;
    retval->flags.was_logged = 0;
    return retval;
}

/*!
\brief Issue a printf-style message to a log

May be called independently by any processor in the communicator used to initialize the log.
*/
void
MACSIO_LOG_LogMsg(
    MACSIO_LOG_LogHandle_t const *log, /**< [in] The handle for the specified log */
    char const *fmt,             /**< [in] A printf-style format string for the log message */
    ...                          /**< [in] Optional, variable list of arguments for the format string. */
)
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
            buf[i] = '!';
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
    log->flags.was_logged = 1;
}

/*!
\brief Convenience method for building a detailed message for a log.
*/
void
MACSIO_LOG_LogMsgWithDetails(
    MACSIO_LOG_LogHandle_t const *log, /**< [in] Log handle to issue message to */
    char const *linemsg,               /**< [in] Caller's message string */
    MACSIO_LOG_MsgSeverity_t sevVal,   /**< [in] Caller's message severity value */
    char const *sevStr,                /**< [in] Caller's message severity abbreviation string */
    int sysErrno,                      /**< [in] Current (most recnet) system's errno */
    int mpiErrno,                      /**< [in] Current (most recent) MPI error */
    char const *theFile,               /**< [in] Caller's file name */
    int theLine                        /**< [in] Caller's line number within the file */
)
{
    char _sig[512], _msg[512], _err[512];
    char _mpistr[MPI_MAX_ERROR_STRING+1], _mpicls[MPI_MAX_ERROR_STRING+1];
    _sig[0] = _msg[0] = _err[0] = _mpistr[0] = _mpicls[0] = '\0';
    if (sevVal <= MACSIO_LOG_MsgDbg3 && sevVal >= MACSIO_LOG_DebugLevel)
        return;
    snprintf(_sig, sizeof(_sig), "%.4s:\"%s\":%d", sevStr, theFile, theLine);
    snprintf(_msg, sizeof(_msg), "%s", linemsg);
    if (sysErrno)
        snprintf(_err, sizeof(_err), "%d:\"%s\"", sysErrno, strerror(sysErrno));
#ifdef HAVE_MPI
    if (mpiErrno != MPI_SUCCESS)
    {
        int mpi_errcls, len;
        MPI_Error_class(mpiErrno, &mpi_errcls);
        MPI_Error_string(mpi_errcls, _mpicls, &len);
        _mpicls[len] = '\0';
        MPI_Error_string(mpiErrno, _mpistr, &len);
        _mpistr[len] = '\0';
    }
#endif
#warning CLEAN UP SO ONLY PRINT NON-EMPTY STRINGS
    MACSIO_LOG_LogMsg(log, "%s:%s:%s:%s:%s", _sig, _msg, _err, _mpistr, _mpicls);
    if (sevVal == MACSIO_LOG_MsgDie)
#ifdef HAVE_MPI
        MPI_Abort(MPI_COMM_WORLD, 0);
#else
        abort(sysErrno);
#endif
}

/*!
\brief Finalize and close an open log
Should be called collectively by all processors that created the log.
*/
void
MACSIO_LOG_LogFinalize(
    MACSIO_LOG_LogHandle_t *log /**< [in] The log to be closed */
)
{
    int was_logged = log->flags.was_logged;
    int reduced_was_logged = was_logged;

#warning ADD ATEXIT FUNCTIONALITY TO CLOSE LOGS
    if (log->logfile != fileno(stderr))
        close(log->logfile);

#ifdef HAVE_MPI
    MPI_Reduce(&was_logged, &reduced_was_logged, 1, MPI_INT, MPI_MAX, 0, log->comm);
#endif

    /* If there was no message logged, we remove the log */
    if (log->rank == 0 && !reduced_was_logged && log->pathname)
        unlink(log->pathname);

    if (log->pathname) free(log->pathname);
    free(log);
}

/*!@}*/
