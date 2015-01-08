#ifndef _OPTIONS_H
#define _OPTIONS_H

#include <stdlib.h>
#include <string.h>

#define IOP_API
#define LSOFH_SIZE 1024

typedef enum _IOPoptid_t {
    OPTID_NOT_SET = 0,

    /* These test entries can be removed but
       their presence is harmless. */
    TEST_INT_OPTID,
    TEST_DBL_OPTID,
    TEST_STR_OPTID,
    /* Array valued option ids must come in pairs,
       the second appending "_SIZE" */
    TEST_INTARR,
    TEST_INTARR_SIZE,
    TEST_DBLARR,
    TEST_DBLARR_SIZE,

    IOINTERFACE_NAME,
    PARALLEL_FILE_MODE,
    PARALLEL_FILE_COUNT,
    REQUEST_SIZE,
    NUM_REQUESTS,
    SIZE_NOISE,
    INIT_FILE_SIZE,
    ALIGNMENT,
    FILENAME_SPRINTF,
    RAND_FILENAME,
    NO_MPI,
    PRINT_TIMING_DETAILS,
    MPI_RANK,
    MPI_SIZE,

    OPTID_LAST_OPT

} IOPoptid_t;

typedef struct _IOPoptlist_t {
    IOPoptid_t    *options;     /* Vector of option identifiers */
    void         **values;      /* Vector of pointers to option values */
    int            numopts;     /* Number of options defined */
    int            maxopts;     /* Total length of option/value arrays */
} IOPoptlist_t;

#ifdef __cplusplus
extern "C" {
#endif

IOP_API extern IOPoptlist_t *IOPMakeOptlist();
IOP_API extern int           IOPFreeOptlist(IOPoptlist_t *);
IOP_API extern int           IOPAddOption(IOPoptlist_t *, IOPoptid_t id, void *);
IOP_API extern void const   *IOPGetOption(IOPoptlist_t const *, IOPoptid_t id);
IOP_API extern int           IOPAddIntOption(IOPoptlist_t*, IOPoptid_t id, int val);
IOP_API extern int           IOPAddIntArrOption2(IOPoptlist_t*, IOPoptid_t id, IOPoptid_t sid, int const *vals, int cnt);
IOP_API extern int           IOPGetIntOption(IOPoptlist_t const *, IOPoptid_t id);
IOP_API extern int const    *IOPGetIntArrOption(IOPoptlist_t const *, IOPoptid_t id);
IOP_API extern int           IOPAddDblOption(IOPoptlist_t*, IOPoptid_t id, double val);
IOP_API extern int           IOPAddDblArrOption2(IOPoptlist_t*, IOPoptid_t id, IOPoptid_t sid, double const *vals, int cnt);
IOP_API extern double        IOPGetDblOption(IOPoptlist_t const *, IOPoptid_t id);
IOP_API extern double const *IOPGetDblArrOption(IOPoptlist_t const *, IOPoptid_t id);
IOP_API extern int           IOPAddStrOption(IOPoptlist_t*, IOPoptid_t id, char const *val);
IOP_API extern char const   *IOPGetStrOption(IOPoptlist_t const *, IOPoptid_t id);
IOP_API extern int           IOPClearOption(IOPoptlist_t *, IOPoptid_t id);
IOP_API extern int           IOPClearOptlist(IOPoptlist_t *);

/* a couple of back-door hacks (don't use 'em) to integrate with ProcessCommandLine utility */
IOP_API extern void         *IOPGetMutableOption(IOPoptlist_t const *, IOPoptid_t id);
IOP_API extern void         *IOPGetMutableArrOption(IOPoptlist_t const *, IOPoptid_t id);
#ifdef __cplusplus
}
#endif

/* convenience macros for array valued options enforcing opt id/_size pairs */
#define IOPAddIntArrOption(OL,ID,ARR,CNT) IOPAddIntArrOption2(OL,ID,ID##_SIZE,ARR,CNT)
#define IOPAddDblArrOption(OL,ID,ARR,CNT) IOPAddDblArrOption2(OL,ID,ID##_SIZE,ARR,CNT)
#define IOPClearArrOption(OL,ID) (IOPClearOption(OL,ID##_SIZE), IOPClearOption(OL,ID))

/* convenience macro to add literal string but via heap */
#define IOPAddLiteralStrOptionFromHeap(L, A, STR)                \
{                                                                \
    char * A ## _str = (char*) calloc(LSOFH_SIZE,sizeof(char));  \
    strcpy(A ## _str, STR);                                      \
    IOPAddStrOption(L, A, A ## _str);                            \
}

#endif /* #ifndef _OPTIONS_H */
