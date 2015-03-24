#ifndef _MACSIO_LOG_H
#define _MACSIO_LOG_H

#ifdef HAVE_MPI
#include <mpi.h>
#endif

#include <errno.h>
#include <stdio.h>

/* two variants of MACSIO_LOG_MSG2 for MPI_Abort or just abort */
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

#endif /* _MACSIO_LOG_H */
