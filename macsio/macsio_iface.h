#ifndef _MACSIO_IFACE_H
#define _MACSIO_IFACE_H

#include <json-c/json.h>

#define MACSIO_IFACE_MAX_COUNT 128
#define MACSIO_IFACE_MAX_NAME 64

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Entry methods of an I/O library Interace
 */

typedef void (*DumpFunc)(int argi, int argc, char **argv, json_object *main_obj, int dumpNum, double dumpTime);
typedef void (*LoadFunc)(int argi, int argc, char **argv, char const *path, json_object **main_obj);
typedef int (*ProcessArgsFunc)  (int argi, int argc, char **argv);
typedef int (*QueryFeaturesFunc)(void);
typedef int (*IdentifyFileFunc) (char const *pathname);

#warning MAKE THE MAKEFILE LINK ANY .o FILES WITH A GIVEN NAME SCHEME
typedef struct MACSIO_IFACE_Handle_t
{   char                 name[MACSIO_IFACE_MAX_NAME];
    char                 ext[MACSIO_IFACE_MAX_NAME];
#warning DEFAULT FILE EXTENSION HERE
#warning Features: Async, compression, sif, grid types, uni-modal or bi-modal
    int                  slotUsed;
    ProcessArgsFunc      processArgsFunc;
    DumpFunc             dumpFunc;
    LoadFunc             loadFunc;
    QueryFeaturesFunc    queryFeaturesFunc;
    IdentifyFileFunc     identifyFileFunc;
} MACSIO_IFACE_Handle_t;

extern int                 MACSIO_IFACE_Register(MACSIO_IFACE_Handle_t const *iface);
extern void                MACSIO_IFACE_GetIds(int *cnt, int **ids);
extern void                MACSIO_IFACE_GetIdsMatchingFileExtension(int *cnt, int **ids, char const *ext);
extern int                 MACSIO_IFACE_GetId(char const *name);
extern char const         *MACSIO_IFACE_GetName(int id);
extern MACSIO_IFACE_Handle_t const *MACSIO_IFACE_GetByName(char const *name);
extern MACSIO_IFACE_Handle_t const *MACSIO_IFACE_GetById(int id);

#ifdef __cplusplus
}
#endif

#endif /* #ifndef _MACSIO_IFACE_H */
