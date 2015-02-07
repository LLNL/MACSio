#ifndef _FILE_H
#define _FILE_H

#define IOP_MAX_NAME 16 
#define IOP_MAX_NS_DEPTH 10
#define IOP_MAX_ABSNAME (IOP_MAX_NS_DEPTH*(IOP_MAX_NAME+1))

typedef struct IOPFileHandlePublic_t
{
    int dummy;
} IOPFileHandlePublic_t;

typedef struct IOPFileHandle_t {
    IOPFileHandlePublic_t pub;
    /* private part follows per I/O-lib driver */
} IOPFileHandle_t;

extern IOPFileHandle_t *IOPCreateFileAll(IOPoptlist_t const *opts);
extern IOPFileHandle_t *IOPOpenFileAll(char const *pathname, int driver_id,
   int flags, IOPoptlist_t const *moreopts);
extern int IOPCloseFileAll(IOPFileHandle_t *fh, IOPoptlist_t const *moreopts);

extern int IOPCreateNamespace(IOPFileHandle_t *fh, char const *nsname, IOPoptlist_t const *moreopts);
extern char const *IOPSetNamespace(IOPFileHandle_t *fh, char const *nsname, IOPoptlist_t const *moreopts);
extern char const *IOPGetNamespace(IOPFileHandle_t *fh, IOPoptlist_t const *moreopts);

extern int IOPCreateArray(IOPFileHandle_t *fh, char const *arrname, int type,
    int const dims[4], IOPoptlist_t const *moreopts);
/* use 'init' and 'next' as special names to manage iteration over all arrays */
extern int IOPGetArrayInfo(IOPFileHandle_t *fh, char const *arrname, int *type, int *dims[4], IOPoptlist_t const *moreopts);
/* use a buf that points to null to indicate a read and a buf that points to non-null as a write */
extern int IOPDefineArrayPart(IOPFileHandle_t *fh, char const *arrname,
    int const starts[4], int const counts[4], int strides[4], void **buf, IOPoptlist_t const *moreopts);
extern int IOPStartPendingArrays(IOPFileHandle_t *fh);
extern int IOPFinishPendingArray(IOPFileHandle_t *fh);

extern int IOPCloseFileAll(IOPFileHandle_t *fh, IOPoptlist_t const *moreopts);
extern int IOPSyncMetaAll((IOPFileHandle_t *fh, IOPoptlist_t const *moreopts);
extern int IOPSyncDataAll(IOPFileHandle_t *fh, IOPoptlist_t const *moreopts);

#endif
