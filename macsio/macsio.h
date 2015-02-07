#ifndef _MACSIO_H
#define _MACSIO_H

#include <options.h>

#include <stddef.h>

#define MACSIO_MAX_NAME 16 
#define MACSIO_MAX_NS_DEPTH 10
#define MACSIO_MAX_ABSNAME (MACSIO_MAX_NS_DEPTH*(MACSIO_MAX_NAME+1))

typedef struct MACSIO_FileHandlePublic_t
{
    /* Public Data Members */

    /* Public Method Members */

    /* File level methods (not 'create' and 'open' are part of iface_info_t */
    int (*closeFileFunc)(struct MACSIO_FileHandle_t *fh, MACSIO_optlist_t const *moreopts);
    int (*syncMetaFunc)(struct MACSIO_FileHandle_t *fh, MACSIO_optlist_t const *moreopts);
    int (*syncDataFunc)(struct MACSIO_FileHandle_t *fh, MACSIO_optlist_t const *moreopts);

    /* Namespace methods */
    int (*createNamespaceFunc)(struct MACSIO_FileHandle_t *fh, char const *nsname, MACSIO_optlist_t const *moreopts);
    char const *(*setNamespaceFunc)(struct MACSIO_FileHandle_t *fh, char const *nsname, MACSIO_optlist_t const *moreopts);
    char const *(*getNamespaceFunc)(struct MACSIO_FileHandle_t *fh, MACSIO_optlist_t const *moreopts);

    /* Named array methods */
    int (*createArrayFunc)(MACSIO_FileHandle_t *fh, char const *arrname, int type,
        int const dims[4], MACSIO_optlist_t const *moreopts);
    /* use 'init' and 'next' as special names to manage iteration over all arrays */
    int (*getArrayInfoFunc)(MACSIO_FileHandle_t *fh, char const *arrname, int *type, int *dims[4], MACSIO_optlist_t const *moreopts);
    /* use a buf that points to null to indicate a read and a buf that points to non-null as a write */
    int (*defineArrayPartFunc)(MACSIO_FileHandle_t *fh, char const *arrname,
        int const starts[4], int const counts[4], int strides[4], void **buf, MACSIO_optlist_t const *moreopts);
    int (*startPendingArraysFunc)(MACSIO_FileHandle_t *fh);
    int (*finishPendingArraysFunc)(MACSIO_FileHandle_t *fh);

} MACSIO_FileHandlePublic_t;

typedef struct MACSIO_FileHandle_t {
    MACSIO_FileHandlePublic_t pub;
    /* private part follows per I/O-lib driver */
} MACSIO_FileHandle_t;

#endif /* #ifndef _MACSIO_H */
