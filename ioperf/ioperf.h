#ifndef _IOPERF_H
#define _IOPERF_H

#include <options.h>

#include <stddef.h>

#define IOP_MAX_NAME 16 
#define IOP_MAX_NS_DEPTH 10
#define IOP_MAX_ABSNAME (IOP_MAX_NS_DEPTH*(IOP_MAX_NAME+1))

typedef struct IOPFileHandlePublic_t
{
    /* Public Data Members */

    /* Public Method Members */

    /* File level methods (not 'create' and 'open' are part of iface_info_t */
    int (*closeFileFunc)(struct IOPFileHandle_t *fh, IOPoptlist_t const *moreopts);
    int (*syncMetaFunc)(struct IOPFileHandle_t *fh, IOPoptlist_t const *moreopts);
    int (*syncDataFunc)(struct IOPFileHandle_t *fh, IOPoptlist_t const *moreopts);

    /* Namespace methods */
    int (*createNamespaceFunc)(struct IOPFileHandle_t *fh, char const *nsname, IOPoptlist_t const *moreopts);
    char const *(*setNamespaceFunc)(struct IOPFileHandle_t *fh, char const *nsname, IOPoptlist_t const *moreopts);
    char const *(*getNamespaceFunc)(struct IOPFileHandle_t *fh, IOPoptlist_t const *moreopts);

    /* Named array methods */
    int (*createArrayFunc)(IOPFileHandle_t *fh, char const *arrname, int type,
        int const dims[4], IOPoptlist_t const *moreopts);
    /* use 'init' and 'next' as special names to manage iteration over all arrays */
    int (*getArrayInfoFunc)(IOPFileHandle_t *fh, char const *arrname, int *type, int *dims[4], IOPoptlist_t const *moreopts);
    /* use a buf that points to null to indicate a read and a buf that points to non-null as a write */
    int (*defineArrayPartFunc)(IOPFileHandle_t *fh, char const *arrname,
        int const starts[4], int const counts[4], int strides[4], void **buf, IOPoptlist_t const *moreopts);
    int (*startPendingArraysFunc)(IOPFileHandle_t *fh);
    int (*finishPendingArraysFunc)(IOPFileHandle_t *fh);

} IOPFileHandlePublic_t;

typedef struct IOPFileHandle_t {
    IOPFileHandlePublic_t pub;
    /* private part follows per I/O-lib driver */
} IOPFileHandle_t;

#endif /* #ifndef _IOPERF_H */
