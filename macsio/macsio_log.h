#ifndef _MACSIO_LOG_H
#define _MACSIO_LOG_H
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

#ifdef HAVE_MPI
#include <mpi.h>
#endif

#include <errno.h>
#include <stdio.h>

/*!
\defgroup MACSIO_LOG MACSIO_LOG
\brief Message logging utilities

MACSIO_LOG is a small utility for logging various kinds of messages from processors. This
includes debugging messages, warnings and errors.

A MACSIO_LOG is a single text file that is divided into groups of message lines. Each processor
gets its own group of lines within the file it may write to. Rank 0 gets the first group
of lines. Rank 1, the next group of lines and so forth. Each line has a maximum length too.

When a MACSIO_LOG is created with \c MACSIO_LOG_InitLog(), the caller specifies the MPI communicator
the log will be used for, the number of message lines per MPI task to allocate plus a count of 
extra lines for rank 0 and the maximum length of any message. Rank 0 initializes the text file
with all space characters except for header lines to distinguish each processor's group of lines
in the file. Note that for a large run on say 10^5 processors, it may take rank 0 several seconds
to create this file.

Messages are restricted to a single line of text. Any embedded new-line characters are removed
from a message and replaced with a '!' character. If a processor's message is longer than the
maximum length of a line for the log, the message is truncated to fit within the line. As
processors issue messages, they are written to the processors group of lines in round-robin 
fashion. As the set of messages issued from a processor reaches the end of its group of lines
within the file, it starts issuing new messages at the beginning of the group. So, older messages
can wind up getting overwritten by newer messages. However, all the most recent messages prior
to any significant error will at least be captured in the log.

Parallelism in writing to a MACSIO_LOG file is achieved by ensuring no two processor attempt
to write data in overlapping regions in the file \em by using \c pwrite() to do the actual
writes.

MACSIO's main creates a default log, \c MACSIO_LOG_MainLog, on the \c MACSIO_MAIN_Comm. That
log is probably the only log needed by MACSIO proper or any of its plugins. The convenience macro,
\c MACSIO_LOG_MSG(SEV, MSG), is the only method one need to worry about to log messages to the
main log. That macro will also capture more detailed information regarding error states around
the time the message issue issued including the file and line number, the system errno and the
most recent MPI error state.

If you use \c MACSIO_LOG_MSG, messages are allowed one of several severities; Dbg1, Dbg2, Dbg3,
Warn, Err and Die. A severity of Die causes an abort. Depending on the current debug level
setting, Dbg1-3 messages may or may not be recorded to a log.

MACSIO's main also creates a log for issuing messages to stderr, \c MACSIO_LOG_StdErr. However,
there is no convenience macro like \c MACSIO_LOG_MSG() for logging messages to \c MACSIO_LOG_StdErr.
A caller simply has to use the MACSIO_LOG interface methods to issue messages to \c MACSIO_LOG_StdErr.

However, any plugin or other portion of MACSIO may, optionally, create its own, private, log using
\c MACSIO_LOG_InitLog(). Once a log is created, any processor can independently issue messages
to the log. There is no parallel communication involved in issuing messages to logs.

Examples of the use of MACSIO_LOG can be found in tstlog.c

@{
*/

#ifndef DOXYGEN_IGNORE_THIS /*[*/

#ifdef HAVE_MPI
#define MACSIO_LOG_MSG2(LOG, MSG, SEVVAL, SEVSTR, ERRNO, MPI_ERRNO, THEFILE, THELINE) \
do{                                                                                   \
    MACSIO_LOG_LogMsgWithDetails(LOG, MACSIO_LOG_MakeMsg MSG, SEVVAL, SEVSTR, ERRNO,  \
        MPI_ERRNO, THEFILE, THELINE);                                                 \
    ERRNO = 0;                                                                        \
    MPI_ERRNO = MPI_SUCCESS;                                                          \
}while(0)
#else
#define MACSIO_LOG_MSG2(LOG, MSG, SEVVAL, SEVSTR, ERRNO, MPI_ERRNO, THEFILE, THELINE) \
do{                                                                                   \
    MACSIO_LOG_LogMsgWithDetails(LOG, MACSIO_LOG_MakeMsg MSG, SEVVAL, SEVSTR, ERRNO,  \
        MPI_ERRNO, THEFILE, THELINE);                                                 \
    ERRNO = 0;                                                                        \
}while(0)
#endif

#endif /*] DOXYGEN_IGNORE_THIS */

/*!
\def Default per-rank line count
*/
#define MACSIO_LOG_DEFAULT_LINE_COUNT 64

/*!
\def Default extra line count for rank 0
*/
#define MACSIO_LOG_DEFAULT_EXTRA_LINES 64

/*!
\def Default line length
*/
#define MACSIO_LOG_DEFAULT_LINE_LENGTH 128

/*!
\def MACSIO_LOG_MSG
\brief Convenience macro for logging a message to the main log
\param [in] SEV Abbreviated message severity (e.g. 'Dbg1', 'Warn')
\param [in] MSG Caller's sprintf-style message enclosed in parenthises (e.g. '("Rank %d failed",rank))'
*/
#define MACSIO_LOG_MSG(SEV, MSG) MACSIO_LOG_MSG2(MACSIO_LOG_MainLog, MSG, MACSIO_LOG_Msg ## SEV, #SEV, errno, mpi_errno, __FILE__, __LINE__)

/*!
\def MACSIO_LOG_MSGV
\brief Alterantive to \c MACSIO_LOG_MSG when severity is a runtime variable
\param [in] VSEV Runtime variable in which message severity is stored
\param [in] MSG Caller's sprintf-style message enclosed in parenthises (e.g. '("Rank %d failed",rank))'
*/
#define MACSIO_LOG_MSGV(VSEV, MSG)                                   \
do                                                                   \
{                                                                    \
    switch (VSEV)                                                    \
    {                                                                \
        case MACSIO_LOG_MsgDbg1: {MACSIO_LOG_MSG(Dbg1, MSG); break;} \
        case MACSIO_LOG_MsgDbg2: {MACSIO_LOG_MSG(Dbg2, MSG); break;} \
        case MACSIO_LOG_MsgDbg3: {MACSIO_LOG_MSG(Dbg3, MSG); break;} \
        case MACSIO_LOG_MsgInfo: {MACSIO_LOG_MSG(Info, MSG); break;} \
        case MACSIO_LOG_MsgWarn: {MACSIO_LOG_MSG(Warn, MSG); break;} \
        case MACSIO_LOG_MsgErr:  {MACSIO_LOG_MSG(Err, MSG); break; } \
        case MACSIO_LOG_MsgDie:  {MACSIO_LOG_MSG(Die, MSG); break; } \
    }                                                                \
}while(0)

/*!
\def MACSIO_LOG_MSGL
\brief Convenience macro for logging a message to any specific log
\param [in] LOG The log handle
\param [in] SEV Abbreviated message severity (e.g. 'Dbg1', 'Warn')
\param [in] MSG Caller's sprintf-style message enclosed in parenthises (e.g. '("Rank %d failed",rank))'
*/
#define MACSIO_LOG_MSGL(LOG, SEV, MSG) MACSIO_LOG_MSG2(LOG, MSG, MACSIO_LOG_Msg ## SEV, #SEV, errno, mpi_errno, __FILE__, __LINE__)

/*!
\def MACSIO_LOG_MSGLV
\brief Convenience macro for logging a message with variable severity to any specific log
\param [in] LOG The log handle
\param [in] VSEV Runtime variable in which message severity is stored
\param [in] MSG Caller's sprintf-style message enclosed in parenthises (e.g. '("Rank %d failed",rank))'
*/
#define MACSIO_LOG_MSGLV(LOG, VSEV, MSG)                             \
do                                                                   \
{                                                                    \
    switch (VSEV)                                                    \
    {                                                                \
        case MACSIO_LOG_MsgDbg1: {MACSIO_LOG_MSGL(LOG, Dbg1, MSG); break;} \
        case MACSIO_LOG_MsgDbg2: {MACSIO_LOG_MSGL(LOG, Dbg2, MSG); break;} \
        case MACSIO_LOG_MsgDbg3: {MACSIO_LOG_MSGL(LOG, Dbg3, MSG); break;} \
        case MACSIO_LOG_MsgInfo: {MACSIO_LOG_MSGL(LOG, Info, MSG); break;} \
        case MACSIO_LOG_MsgWarn: {MACSIO_LOG_MSGL(LOG, Warn, MSG); break;} \
        case MACSIO_LOG_MsgErr:  {MACSIO_LOG_MSGL(LOG, Err, MSG); break; } \
        case MACSIO_LOG_MsgDie:  {MACSIO_LOG_MSGL(LOG, Die, MSG); break; } \
    }                                                                \
}while(0)

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _MACSIO_LOG_MsgSeverity_t
{
    MACSIO_LOG_MsgDbg1,  /**< Debug level 1: For coarse grained debugging messages (rare enough performance isn't effected) */
    MACSIO_LOG_MsgDbg2,  /**< Debug level 2: For moderate grained debugging messages (may effect performance) */
    MACSIO_LOG_MsgDbg3,  /**< Debug level 3: For fine grained debugging messages (most likely effects performance) */
    MACSIO_LOG_MsgInfo,  /**< Informational messages */
    MACSIO_LOG_MsgWarn,  /**< Warnings of minor problems that can be recovered from without undue effects */
    MACSIO_LOG_MsgErr,   /**< Error conditions that result in a change in expected/anticipated behavior */
    MACSIO_LOG_MsgDie    /**< Unrecoverable errors */
} MACSIO_LOG_MsgSeverity_t;

typedef struct _MACSIO_LOG_LogHandle_t MACSIO_LOG_LogHandle_t;

/*!
\brief Error code returned by most recent MPI call

MACSIO uses \c mpi_errno much like the system's \c errno. However, there is no way to enforce
that any particular MPI function calls set mpi_errno. MACSIO relies upon the honor system of
develepors to always make MPI calls by setting \c mpi_errno to the return value of those calls.
Assuming this practice is followed throughout MACSIO and any of its plugins, then the global
variable \c mpi_errno should always hold the MPI error return value of the most recent MPI
call.
*/
extern int                     mpi_errno;

/*!
\brief Filtering level for debugging messages

MACSIO generates 3 levels of debug messages numbered 1, 2 and 3. Each level is intended to
represent more and more detailed debugging messages. Level 1 messages are issued rare enough
by MACSIO (e.g. coarse grained) that they will not impact performance. Level 3 messages can
be issued often enough by MACSIO that they will almost certainly impact performance. Level 2
messages are somewhere in between. The global variable \c MACSIO_LOG_DebugLevel is used to
filter what messages generated by MACSIO that will actually make it into a detailed log
message. If \c MACSIO_LOG_DebugLevel is set to level \c N, then messages at and below
\c N debug level will appear in a log when written via MACSIO_LOG_LogMsgWithDetails. However,
messages above this level will be discarded. As developers write code blocks in MACSIO,
developers must choose what kind of messages are issued and, for debugging kinds of messages,
the appropriate debug level.
*/
extern int                     MACSIO_LOG_DebugLevel;

/*!
\brief Log handle for MACSIO's main log

Typically, developers will use \c MACSIO_LOG_MSG macro to log messages. Such messages
are automatically logged to the main log. The main log is created by MACSIO's main.
*/
extern MACSIO_LOG_LogHandle_t *MACSIO_LOG_MainLog;


/*!
\brief Log handle for MACSIO's stderr output

In rare cases, developers may want to issues messages to stderr. In this case,
MACSIO's stderr log handle should be used.
*/
extern MACSIO_LOG_LogHandle_t *MACSIO_LOG_StdErr;

extern char const * MACSIO_LOG_MakeMsg(const char *format, ...);
#ifdef HAVE_MPI
extern MACSIO_LOG_LogHandle_t *MACSIO_LOG_LogInit(MPI_Comm comm, char const *path, int line_len, int lines_per_proc, int extra_lines_proc0);
#else
extern MACSIO_LOG_LogHandle_t *MACSIO_LOG_LogInit(int comm, char const *path, int line_len, int lines_per_proc, int extra_lines_proc0);
#endif
extern void MACSIO_LOG_LogMsg(MACSIO_LOG_LogHandle_t const *log, char const *fmt, ...);
extern void MACSIO_LOG_LogMsgWithDetails(MACSIO_LOG_LogHandle_t const *log, char const *linemsg,
    MACSIO_LOG_MsgSeverity_t sevVal, char const *sevStr,
    int sysErrno, int mpiErrno, char const *theFile, int theLine);
extern void MACSIO_LOG_LogFinalize(MACSIO_LOG_LogHandle_t *log);

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* _MACSIO_LOG_H */
