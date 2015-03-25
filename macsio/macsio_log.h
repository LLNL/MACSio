#ifndef _MACSIO_LOG_H
#define _MACSIO_LOG_H

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
the log will be used for, the number of message lines per MPI task to allocate and the maximum length
of any message. Rank 0 initializes the text file with all space characters except for header lines
to distinguish each processor's group of lines in the file. Note that for a large run on say 10^5
processors, it may take rank 0 several seconds to create this file.

Messages are restricted to a single line of text. Any embedded new-line characters are removed
from a message and replaced with a '!' character. If a processor's message is longer than the
maximum length of a line for the log, the message is truncated to fit within the line. As
processors issue messages, they are written to the processors group of lines in round-robin 
fashion. As the set of messages issued from a processor reaches the end of its group of lines
within the file, it starts issuing new messages at the beginning of the group. So, older messages
can wind up getting overwritten by newer messages. However, all the most recent messages prior
to any significant error will at least be captured in the log.

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

Examples of the use of the use of MACSIO_LOG can be found in tstlog.c

@{
*/

#ifdef HAVE_MPI
#define MACSIO_LOG_MSG2(LINEMSG, SEVERITY, ERRNO, MPI_ERRNO, THEFILE, THELINE)         \
do                                                                                     \
{                                                                                      \
    int _errno = ERRNO;                                                                \
    char _sig[512], _msg[512], _err[512];                                              \
    char _mpistr[MPI_MAX_ERROR_STRING+1], _mpicls[MPI_MAX_ERROR_STRING+1];             \
    MACSIO_LOG_MsgSeverity_t _sev = MACSIO_LOG_Msg ## SEVERITY;                        \
    _sig[0] = _msg[0] = _err[0] = _mpistr[0] = _mpicls[0] = '\0';                      \
    if (_sev <= MACSIO_LOG_MsgDbg3 && _sev >= MACSIO_LOG_DebugLevel)                   \
        break;                                                                         \
    snprintf(_sig, sizeof(_sig), "%.4s:\"%s\":%d", #SEVERITY, THEFILE, THELINE);       \
    snprintf(_msg, sizeof(_msg), "%s", MACSIO_LOG_make_msg LINEMSG);                   \
    if (_errno)                                                                        \
        snprintf(_err, sizeof(_err), "%d:\"%s\"", _errno, strerror(_errno));           \
    if (MPI_ERRNO != MPI_SUCCESS)                                                      \
    {                                                                                  \
        int mpi_errcls, len;                                                           \
        MPI_Error_class(MPI_ERRNO, &mpi_errcls);                                       \
        MPI_Error_string(mpi_errcls, _mpicls, &len);                                   \
        _mpicls[len] = '\0';                                                           \
        MPI_Error_string(MPI_ERRNO, _mpistr, &len);                                    \
        _mpistr[len] = '\0';                                                           \
    }                                                                                  \
    MACSIO_LOG_LogMsg(MACSIO_LOG_MainLog, "%s:%s:%s:%s:%s", _sig, _msg, _err, _mpistr, _mpicls);\
    if (_sev == MACSIO_LOG_MsgDie)                                                     \
        MPI_Abort(MPI_COMM_WORLD, 0);                                                  \
    ERRNO = 0;                                                                         \
    MPI_ERRNO = MPI_SUCCESS;                                                           \
}                                                                                      \
while (0)
#else
#define MACSIO_LOG_MSG2(LINEMSG, SEVERITY, ERRNO, MPI_ERRNO, THEFILE, THELINE)         \
do                                                                                     \
{                                                                                      \
    int _errno = ERRNO;                                                                \
    char _sig[512], _msg[512], _err[512];                                              \
    char _mpistr[MPI_MAX_ERROR_STRING+1], _mpicls[MPI_MAX_ERROR_STRING+1];             \
    MACSIO_LOG_MsgSeverity_t _sev = MACSIO_LOG_Msg ## SEVERITY;                        \
    _sig[0] = _msg[0] = _err[0] = _mpistr[0] = _mpicls[0] = '\0';                      \
    if (MACSIO_LOG_MsgDbg1 <= _sev <= MACSIO_LOG_MsgDbg3)                              \
    if (_sev <= MACSIO_LOG_MsgDbg3 && _sev < MACSIO_LOG_DebugLevel)                    \
        break;                                                                         \
    snprintf(_sig, sizeof(_sig), "%.4s:\"%s\":%d", #SEVERITY, THEFILE, THELINE);       \
    snprintf(_msg, sizeof(_msg), MACSIO_LOG_make_msg LINEMSG);                         \
    if (_errno)                                                                        \
        snprintf(_err, sizeof(_err), "%d:\"%s\", _errno, strerror(_errno));            \
    MACSIO_LOG_LogMsg(MACSIO_LOG_MainLog, "%s:%s:%s:%s:%s", _sig, _msg, _err, _mpistr, _mpicls);\
    if (_sev == MACSIO_LOG_MsgDie)                                                     \
        abort(_errno);                                                                 \
    ERRNO = 0;                                                                         \
    MPI_ERRNO = MPI_SUCCESS;                                                           \
}                                                                                      \
while (0)
#endif

/* handles case when severity level is a run-time variable */
#define MACSIO_LOG_MSGV(VSEVERITY, MSG)                              \
do                                                                   \
{                                                                    \
    switch (VSEVERITY)                                               \
    {                                                                \
        case MACSIO_LOG_MsgDbg1: {MACSIO_LOG_MSG(Dbg1, MSG); break;} \
        case MACSIO_LOG_MsgDbg2: {MACSIO_LOG_MSG(Dbg2, MSG); break;} \
        case MACSIO_LOG_MsgDbg3: {MACSIO_LOG_MSG(Dbg3, MSG); break;} \
        case MACSIO_LOG_MsgWarn: {MACSIO_LOG_MSG(Warn, MSG); break;} \
        case MACSIO_LOG_MsgErr:  {MACSIO_LOG_MSG(Err, MSG); break; } \
        case MACSIO_LOG_MsgDie:  {MACSIO_LOG_MSG(Die, MSG); break; } \
    }                                                                \
}while(0)

/* Convenience method for detailed messaging log to the main macsio log */
#define MACSIO_LOG_MSG(SEVERITY, MSG) MACSIO_LOG_MSG2(MSG, SEVERITY, errno, mpi_errno, __FILE__, __LINE__)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MACSIO_LOG_LogHandle_t
{
    int logfile;
    int rank;
    int size;
    int log_line_length;
    int lines_per_proc;
    int current_line;
} MACSIO_LOG_LogHandle_t;

typedef enum _MACSIO_LOG_MsgSeverity_t
{
    MACSIO_LOG_MsgDbg1,
    MACSIO_LOG_MsgDbg2,
    MACSIO_LOG_MsgDbg3,
    MACSIO_LOG_MsgWarn,
    MACSIO_LOG_MsgErr,
    MACSIO_LOG_MsgDie
} MACSIO_LOG_MsgSeverity_t;

char const * MACSIO_LOG_make_msg(const char *format, ...);
#ifdef HAVE_MPI
extern MACSIO_LOG_LogHandle_t *MACSIO_LOG_LogInit(MPI_Comm comm, char const *path, int line_len, int lines_per_proc);
#else
extern MACSIO_LOG_LogHandle_t *MACSIO_LOG_LogInit(int comm, char const *path, int line_len, int lines_per_proc);
#endif
extern void MACSIO_LOG_LogMsg(MACSIO_LOG_LogHandle_t *log, char const *fmt, ...);
extern void MACSIO_LOG_LogFinalize(MACSIO_LOG_LogHandle_t *log);

extern MACSIO_LOG_LogHandle_t *MACSIO_LOG_MainLog;
extern MACSIO_LOG_LogHandle_t *MACSIO_LOG_StdErr;
extern int MACSIO_LOG_DebugLevel;
extern int mpi_errno;

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* _MACSIO_LOG_H */
