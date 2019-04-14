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

#include <fcntl.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <macsio_log.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

/*!
\addtogroup MACSIO_LOG
@{
*/

#ifndef DOXYGEN_IGNORE_THIS /*[*/
#ifdef HAVE_MPI
int                     mpi_errno = MPI_SUCCESS;
#else
int                     mpi_errno = 0;
#endif
int                     MACSIO_LOG_DebugLevel = 0;
MACSIO_LOG_LogHandle_t *MACSIO_LOG_MainLog = 0;
MACSIO_LOG_LogHandle_t *MACSIO_LOG_StdErr = 0;
#endif /*] DOXYGEN_IGNORE_THIS */

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
    int extra_lines_proc0;    /**< Additional number of message lines for processor with MPI rank 0 */
//#warning FIX USE OF MUTABLE HERE
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
//#warning MAKE THIS THREAD SAFE BY ALLOCATING THE RETURNED STRING OR USE A LARGE CIRCULAR BUFFER
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
    int lines_per_proc,   /**< [in] The number of message lines for each MPI task */
    int extra_lines_proc0 /**< [in] The number of extra message lines for processor rank 0 */
)
{
    int rank=0, size=1;
    MACSIO_LOG_LogHandle_t *retval;

    if (line_len <= 0) line_len = MACSIO_LOG_DEFAULT_LINE_LENGTH;
    if (lines_per_proc <= 0) lines_per_proc = MACSIO_LOG_DEFAULT_LINE_COUNT;
    if (extra_lines_proc0 <= 0) extra_lines_proc0 = MACSIO_LOG_DEFAULT_EXTRA_LINES;

#ifdef HAVE_MPI
    MPI_Comm_size(comm, &size);
    MPI_Comm_rank(comm, &rank);
#endif

    /* Rank 0 "primes" the log file; creates it, populates it
       with processor header lines and spaces and closes it */
    if (path && rank == 0)
    {
        int i, filefd;
        char *linbuf = (char*) malloc(line_len * (lines_per_proc + extra_lines_proc0) * sizeof(char));
        memset(linbuf, '-', line_len * sizeof(char));
        memset(linbuf+line_len, ' ',  line_len * (lines_per_proc + extra_lines_proc0 - 1) * sizeof(char));
        for (i = 0; i < lines_per_proc+extra_lines_proc0; i++)
            linbuf[(i+1)*line_len-1] = '\n';
        filefd = open(path, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);
        for (i = 0; i < size; i++)
        {
            char tmp[32];
            sprintf(tmp, "Processor %06d", i);
            memcpy(linbuf+line_len/2-strlen(tmp)/2, tmp, strlen(tmp));
            if (i == 0)
                write(filefd, linbuf, sizeof(char) * line_len * (lines_per_proc+extra_lines_proc0));
            else
                write(filefd, linbuf, sizeof(char) * line_len * lines_per_proc);
        }
        free(linbuf);
        close(filefd);
    }

#ifdef HAVE_MPI
    mpi_errno = MPI_Barrier(comm);
#endif

    retval = (MACSIO_LOG_LogHandle_t *) malloc(sizeof(MACSIO_LOG_LogHandle_t));
    retval->pathname = path?strdup(path):0;
    retval->comm = comm;
    retval->logfile = path?open(path, O_WRONLY):fileno(stderr);
    retval->size = size;
    retval->rank = rank;
    retval->log_line_length = path?line_len:1024;
    retval->lines_per_proc = path?lines_per_proc:1000000;
    retval->extra_lines_proc0 = path?extra_lines_proc0:0;
    retval->current_line = 1; /* never write to line '0' to preserve "Processor XXXX" headings */
    retval->flags.was_logged = 0;
    errno = 0;
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

    msg = (char *) malloc(log->log_line_length+10);
    buf = (char *) malloc(log->log_line_length+10);

    if (is_stderr) sprintf(msg, "%06d: ", log->rank);
    va_start(ptr, fmt);
    vsnprintf(is_stderr?&msg[8]:msg, log->log_line_length-1, fmt, ptr);
    va_end(ptr);
    msg[log->log_line_length-1] = '\0';

    while (i < (int) strlen(msg) && i < log->log_line_length)
    {
        if (msg[i] == '\n')
            buf[i] = '!';
        else
            buf[i] = msg[i];
        i++;
    }
    free(msg);
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
        fflush(stderr); /* can never be sure stderr is UNbuffered */
    }
    else
    {
        int extra_lines = log->rank?log->extra_lines_proc0:0;
        off_t seek_offset = (log->rank * log->lines_per_proc + log->current_line + extra_lines) * log->log_line_length;
        pwrite(log->logfile, buf, sizeof(char) * log->log_line_length, seek_offset);
    }
    free(buf);

    log->current_line++;
    if (log->current_line == log->lines_per_proc + (log->rank==0?log->extra_lines_proc0:0))
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
#ifdef HAVE_MPI
    char _mpistr[MPI_MAX_ERROR_STRING+1], _mpicls[MPI_MAX_ERROR_STRING+1];
#else
    char _mpistr[2048+1], _mpicls[2048+1];
#endif
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
    MACSIO_LOG_LogMsg(log, "%s:%s:%s:%s:%s", _sig, _msg, _err, _mpistr, _mpicls);
    if (sevVal == MACSIO_LOG_MsgDie)
#ifdef HAVE_MPI
        MPI_Abort(MPI_COMM_WORLD, mpiErrno==MPI_SUCCESS?sysErrno:mpiErrno);
#else
        exit(sysErrno);
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

//#warning ADD ATEXIT FUNCTIONALITY TO CLOSE LOGS
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
