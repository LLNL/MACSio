#ifndef MACSIO_MAIN_H
#define MACSIO_MAIN_H

#include <json-c/json_object.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int MACSIO_GetRankOwningPart(json_object *main_obj, int chunkId);

#ifdef __cplusplus
}
#endif

#endif /* MACSIO_MAIN_H */
