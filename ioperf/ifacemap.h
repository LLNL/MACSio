#ifndef _IFACEMAP_H
#define _IFACEMAP_H

#include <ifacemap.h>
#include <macsio.h>

#define MAX_IFACES 32
#define MAX_IFACE_NAME 64

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Entry methods of an I/O library Interace
 */

/* Required methods */
typedef int                     (*InitInterfaceFunc) (MACSIO_optlist_t const *opts);
typedef struct MACSIO_FileHandle_t* (*CreateFileFunc)   (char const *pathname, int flags, 
    MACSIO_optlist_t const *moreopts);
typedef struct MACSIO_FileHandle_t* (*OpenFileFunc)     (char const *pathname, int flags,
    MACSIO_optlist_t const *moreopts);

/* Optional methods */
typedef MACSIO_optlist_t*           (*ProcessArgsFunc)  (int argi, int argc, char **argv);
typedef MACSIO_optlist_t*           (*QueryFeaturesFunc)(void);
typedef int                     (*IdentifyFileFunc) (char const *pathname, MACSIO_optlist_t const *moreopts);

typedef struct MACSIO_IFaceHandle_t
{   char                 name[MAX_IFACE_NAME];
    char                 ext[MAX_IFACE_NAME];
    int                  slotUsed;
    InitInterfaceFunc    initInterfaceFunc;
    ProcessArgsFunc      processArgsFunc;
    QueryFeaturesFunc    queryFeaturesFunc;
    IdentifyFileFunc     identifyFileFunc;
    CreateFileFunc       createFileFunc;
    OpenFileFunc         openFileFunc;
} MACSIO_IFaceHandle_t;

/*
 *  Compile time knowledge of known interfaces is built up in
 *  this array. It is a mostly empty array of interfaces hashed
 *  by their names. Each interface's object file includes 
 *  initialization code (that gets executed before 'main') that
 *  initializes an entry (assuming one is available) in this array.
 */
extern MACSIO_IFaceHandle_t iface_map[MAX_IFACES];

extern void                MACSIO_GetInterfaceIds(int *cnt, int **ids);
extern int                 MACSIO_GetInterfaceId(char const *name);
extern char const         *MACSIO_GetInterfaceName(int id);
extern MACSIO_IFaceHandle_t const *MACSIO_GetInterfaceByName(char const *name);
extern MACSIO_IFaceHandle_t const *MACSIO_GetInterfaceById(int id);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _IFACEMAP_H */
