#ifndef _MACSIO_DATA_H
#define _MACSIO_DATA_H
/*
Copyright (c) 2015, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.
Written by Mark C. Miller

LLNL-CODE-676051. All rights reserved.

This file is part of MACSio

Please also read the LICENSE file at the top of the source code directory or
folder hierarchy.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License (as published by the Free Software
Foundation) version 2, dated June 1991.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

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
extern struct json_object *MACSIO_DATA_EvolveDataset(json_object *main_obj, int *dataset_evolved);

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* _MACSIO_DATA_H */
