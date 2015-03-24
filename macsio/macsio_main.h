#ifndef MACSIO_MAIN_H
#define MACSIO_MAIN_H

#include <json-c/json_object.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_MPI
extern MPI_Comm MACSIO_MAIN_Comm;
#else
extern int MACSIO_MAIN_Comm;
#endif
extern int MACSIO_MAIN_Size;
extern int MACSIO_MAIN_Rank;

extern int MACSIO_MAIN_GetRankOwningPart(json_object *main_obj, int chunkId);

#ifdef __cplusplus
}
#endif

#endif /* MACSIO_MAIN_H */
