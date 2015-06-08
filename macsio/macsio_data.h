#ifndef _MACSIO_DATA_H
#define _MACSIO_DATA_H

/*!
\defgroup MACSIO_DATA MACSIO_DATA
\brief Data (Mesh) Generation

@{
*/

#ifdef __cplusplus
extern "C" {
#endif

extern struct json_object *MACSIO_DATA_MakeRandomObject(int nthings);
extern struct json_object *MACSIO_DATA_GenerateTimeZeroDumpObject(json_object *main_obj,
                               int *rank_owning_chunkId);
extern int                 MACSIO_DATA_GetRankOwningPart(json_object *main_obj, int chunkId);
extern int                 MACSIO_DATA_ValidateDataRead(json_object *main_obj);
extern int                 MACSIO_DATA_SimpleAssignKPartsToNProcs(int k, int n, int my_rank,
                               int *my_part_cnt, int **my_part_ids);

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* _MACSIO_DATA_H */
