#ifndef _IFACEMAP_H
#define _IFACEMAP_H

#include <ifacemap.h>
#include <json-c/json.h>

#define MACSIO_MAX_IFACES 128
#define MACSIO_MAX_IFACE_NAME 64

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Entry methods of an I/O library Interace
 */

/* Required methods */
typedef void              (*DumpFunc)(int argi, int argc, char **argv, json_object *main_obj, int dumpNum, double dumpTime);

/* Optional methods */
typedef int (*ProcessArgsFunc)  (int argi, int argc, char **argv);
typedef int (*QueryFeaturesFunc)(void);
typedef int (*IdentifyFileFunc) (char const *pathname);

#warning MAKE THE MAKEFILE LINK ANY .o FILES WITH A GIVEN NAME SCHEME
typedef struct MACSIO_IFaceHandle_t
{   char                 name[MACSIO_MAX_IFACE_NAME];
    char                 ext[MACSIO_MAX_IFACE_NAME];
    int                  slotUsed;
    ProcessArgsFunc      processArgsFunc;
    DumpFunc             dumpFunc;
    QueryFeaturesFunc    queryFeaturesFunc;
    IdentifyFileFunc     identifyFileFunc;
} MACSIO_IFaceHandle_t;

/*
 *  Compile time knowledge of known interfaces is built up in
 *  this array. It is a mostly empty array of interfaces hashed
 *  by their names. Each interface's object file includes 
 *  initialization code (that gets executed before 'main') that
 *  initializes an entry (assuming one is available) in this array.
 */
extern MACSIO_IFaceHandle_t iface_map[MACSIO_MAX_IFACES];

extern void                MACSIO_GetInterfaceIds(int *cnt, int **ids);
extern int                 MACSIO_GetInterfaceId(char const *name);
extern char const         *MACSIO_GetInterfaceName(int id);
extern MACSIO_IFaceHandle_t const *MACSIO_GetInterfaceByName(char const *name);
extern MACSIO_IFaceHandle_t const *MACSIO_GetInterfaceById(int id);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _IFACEMAP_H */
