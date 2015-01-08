#ifndef _IFACEMAP_H
#define _IFACEMAP_H

#include <ifacemap.h>
#include <ioperf.h>

#define MAX_IFACES 32
#define MAX_IFACE_NAME 64

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Entry methods of an I/O library Interace
 */

/* Required methods */
typedef int                     (*InitInterfaceFunc) (IOPoptlist_t const *opts);
typedef struct IOPFileHandle_t* (*CreateFileFunc)   (char const *pathname, int flags, 
    IOPoptlist_t const *moreopts);
typedef struct IOPFileHandle_t* (*OpenFileFunc)     (char const *pathname, int flags,
    IOPoptlist_t const *moreopts);

/* Optional methods */
typedef IOPoptlist_t*           (*ProcessArgsFunc)  (int argi, int argc, char **argv);
typedef IOPoptlist_t*           (*QueryFeaturesFunc)(void);
typedef int                     (*IdentifyFileFunc) (char const *pathname, IOPoptlist_t const *moreopts);

typedef struct IOPIFaceHandle_t
{   char                 name[MAX_IFACE_NAME];
    char                 ext[MAX_IFACE_NAME];
    int                  slotUsed;
    InitInterfaceFunc    initInterfaceFunc;
    ProcessArgsFunc      processArgsFunc;
    QueryFeaturesFunc    queryFeaturesFunc;
    IdentifyFileFunc     identifyFileFunc;
    CreateFileFunc       createFileFunc;
    OpenFileFunc         openFileFunc;
} IOPIFaceHandle_t;

/*
 *  Compile time knowledge of known interfaces is built up in
 *  this array. It is a mostly empty array of interfaces hashed
 *  by their names. Each interface's object file includes 
 *  initialization code (that gets executed before 'main') that
 *  initializes an entry (assuming one is available) in this array.
 */
extern IOPIFaceHandle_t iface_map[MAX_IFACES];

extern void                IOPGetInterfaceIds(int *cnt, int **ids);
extern int                 IOPGetInterfaceId(char const *name);
extern char const         *IOPGetInterfaceName(int id);
extern IOPIFaceHandle_t const *IOPGetInterfaceByName(char const *name);
extern IOPIFaceHandle_t const *IOPGetInterfaceById(int id);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _IFACEMAP_H */
