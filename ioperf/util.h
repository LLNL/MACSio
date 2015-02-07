#ifndef _UTIL_H
#define _UTIL_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MACSIO_FATAL 1
#define MACSIO_WARN 2

/* helpful memory allocation wrappers */
#define ALLOC(T) ((T*)calloc((size_t)1,sizeof(T)))
#define ALLOC_N(T,N) ((T*)((N)>0?calloc((size_t)(N),sizeof(T)):0))
#define REALLOC(P,T,N) REALLOC_N((P),(T),(N))
#define REALLOC_N(P,T,N) ((T*)((N)>0?realloc((P),(size_t)((N)*sizeof(T))):0))
#define FREE(M) if(M){free(M);(M)=0;} 

/* two variants of MACSIO_ERROR2 for MPI_Abort or just abort */
#ifdef HAVE_PARALLEL
#define MACSIO_ERROR2(LINEMSG, SEVERITY, ERRNO, THEFILE, THELINE)                          \
{                                                                                       \
    fprintf(stderr, "Encountered error in file \"%s\" at line %d\n", THEFILE, THELINE); \
    fprintf(stderr, "MACSIO %s error message:\n\t\"%s\"\n", SEVERITY==MACSIO_WARN?"warning":"fatal",_iop_errmsg LINEMSG);             \
    if (ERRNO)                                                                          \
        fprintf(stderr, "Most recent system error message:\n\t\"%s\"\n", strerror(ERRNO));\
    fprintf(stderr, "\t'ioperf --help' may provide additional information\n");          \
    if (SEVERITY == MACSIO_FATAL)                                                          \
        MPI_Abort();                                                                    \
    ERRNO = 0;                                                                          \
}
#else
#define MACSIO_ERROR2(LINEMSG, SEVERITY, ERRNO, THEFILE, THELINE)                          \
{                                                                                       \
    fprintf(stderr, "Encountered error in file \"%s\" at line %d\n", THEFILE, THELINE); \
    fprintf(stderr, "MACSIO %s error message:\n\t\"%s\"\n", SEVERITY==MACSIO_WARN?"warning":"fatal",_iop_errmsg LINEMSG);             \
    if (ERRNO)                                                                          \
        fprintf(stderr, "Most recent system error message:\n\t\"%s\"\n", strerror(ERRNO));\
    fprintf(stderr, "\t'ioperf --help' may provide additional information\n");          \
    if (SEVERITY == MACSIO_FATAL)                                                          \
        abort();                                                                        \
    ERRNO = 0;                                                                          \
}
#endif

#define MACSIO_ERROR(MSG,SEVERITY) MACSIO_ERROR2(MSG, SEVERITY, errno, __FILE__, __LINE__)

#define MACSIO_ARGV_HELP  -1
#define MACSIO_ARGV_ERROR 1
#define MACSIO_ARGV_OK 0
#define MACSIO_END_OF_ARGS		"end_of_args"

#ifdef __cplusplus
extern "C" {
#endif

extern char const *_iop_errmsg(const char *format, ...);
extern int MACSIO_ProcessCommandLine(int unknownArgsFlag, int argi, int argc, char **argv, ...);
extern unsigned int bjhash(const unsigned char *k, unsigned int length, unsigned int initval);

#ifdef __cplusplus
}
#endif

#endif /* _UTIL_H */
