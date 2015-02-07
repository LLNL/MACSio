#ifndef _OPTIONS_H
#define _OPTIONS_H

#include <stdlib.h>
#include <string.h>

#define MACSIO_API
#define LSOFH_SIZE 1024

typedef enum _MACSIO_optid_t {
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
    PART_SIZE,
    AVG_NUM_PARTS,
    PART_DISTRIBUTION,
    VARS_PER_PART,
    NUM_DUMPS,
    ALIGNMENT,
    FILENAME_SPRINTF,
    PRINT_TIMING_DETAILS,
    MPI_RANK,
    MPI_SIZE,

    OPTID_LAST_OPT

} MACSIO_optid_t;

typedef struct _MACSIO_optlist_t {
    MACSIO_optid_t    *options;     /* Vector of option identifiers */
    void         **values;      /* Vector of pointers to option values */
    int            numopts;     /* Number of options defined */
    int            maxopts;     /* Total length of option/value arrays */
} MACSIO_optlist_t;

#ifdef __cplusplus
extern "C" {
#endif

MACSIO_API extern MACSIO_optlist_t *MACSIO_MakeOptlist();
MACSIO_API extern int           MACSIO_FreeOptlist(MACSIO_optlist_t *);
MACSIO_API extern int           MACSIO_AddOption(MACSIO_optlist_t *, MACSIO_optid_t id, void *);
MACSIO_API extern void const   *MACSIO_GetOption(MACSIO_optlist_t const *, MACSIO_optid_t id);
MACSIO_API extern int           MACSIO_AddIntOption(MACSIO_optlist_t*, MACSIO_optid_t id, int val);
MACSIO_API extern int           MACSIO_AddIntArrOption2(MACSIO_optlist_t*, MACSIO_optid_t id, MACSIO_optid_t sid, int const *vals, int cnt);
MACSIO_API extern int           MACSIO_GetIntOption(MACSIO_optlist_t const *, MACSIO_optid_t id);
MACSIO_API extern int const    *MACSIO_GetIntArrOption(MACSIO_optlist_t const *, MACSIO_optid_t id);
MACSIO_API extern int           MACSIO_AddDblOption(MACSIO_optlist_t*, MACSIO_optid_t id, double val);
MACSIO_API extern int           MACSIO_AddDblArrOption2(MACSIO_optlist_t*, MACSIO_optid_t id, MACSIO_optid_t sid, double const *vals, int cnt);
MACSIO_API extern double        MACSIO_GetDblOption(MACSIO_optlist_t const *, MACSIO_optid_t id);
MACSIO_API extern double const *MACSIO_GetDblArrOption(MACSIO_optlist_t const *, MACSIO_optid_t id);
MACSIO_API extern int           MACSIO_AddStrOption(MACSIO_optlist_t*, MACSIO_optid_t id, char const *val);
MACSIO_API extern char const   *MACSIO_GetStrOption(MACSIO_optlist_t const *, MACSIO_optid_t id);
MACSIO_API extern int           MACSIO_ClearOption(MACSIO_optlist_t *, MACSIO_optid_t id);
MACSIO_API extern int           MACSIO_ClearOptlist(MACSIO_optlist_t *);

/* a couple of back-door hacks (don't use 'em) to integrate with ProcessCommandLine utility */
MACSIO_API extern void         *MACSIO_GetMutableOption(MACSIO_optlist_t const *, MACSIO_optid_t id);
MACSIO_API extern void         *MACSIO_GetMutableArrOption(MACSIO_optlist_t const *, MACSIO_optid_t id);
#ifdef __cplusplus
}
#endif

/* convenience macros for array valued options enforcing opt id/_size pairs */
#define MACSIO_AddIntArrOption(OL,ID,ARR,CNT) MACSIO_AddIntArrOption2(OL,ID,ID##_SIZE,ARR,CNT)
#define MACSIO_AddDblArrOption(OL,ID,ARR,CNT) MACSIO_AddDblArrOption2(OL,ID,ID##_SIZE,ARR,CNT)
#define MACSIO_ClearArrOption(OL,ID) (MACSIO_ClearOption(OL,ID##_SIZE), MACSIO_ClearOption(OL,ID))

/* convenience macro to add literal string but via heap */
#define MACSIO_AddLiteralStrOptionFromHeap(L, A, STR)                \
{                                                                \
    char * A ## _str = (char*) calloc(LSOFH_SIZE,sizeof(char));  \
    strcpy(A ## _str, STR);                                      \
    MACSIO_AddStrOption(L, A, A ## _str);                            \
}

#endif /* #ifndef _OPTIONS_H */
