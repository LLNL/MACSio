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

/*!
\defgroup MACSIO_PRNGS MACSIO_PRNGS
\brief Randomization and Pseudo Random Number Generators

@{
*/

/*!
\brief Create a Pseudo Random Number Generator (PRNG)
\return An integer identifier for the created PRNG
*/
extern int
MACSIO_DATA_CreatePRNG(
    unsigned seed /**< seed for initializing random number generator */
);

/*!
\brief Reset a PRNG
This will cause the specified PRNG to restart the sequence of pseudo
random numbers it generates.
*/
extern void
MACSIO_DATA_ResetPRNG(
    int prng_id /**< identifier for the PRNG to reset */
);

/*!
\brief Get next value from PRNG
\return Next value from the PRNG sequence. Logically equivalent to calling
        random().
*/
extern long
MACSIO_DATA_GetValPRNG(
    int prng_id /**< identifer of the desired PRNG to get a value from */
);

/*!
\brief Free resources associated a PRNG
*/
extern void
MACSIO_DATA_DestroyPRNG(
    int prng_id /**< identifer of the desired PRNG to free */
);

/*!
\def MD_random_naive
\brief Get next random value from naive PRNG. Same as MD_random().

The naive prng is fine for any randomization that does not need special behaviors.
This is the one that is expected to be used most of the time.
*/
#define MD_random_naive() MACSIO_DATA_GetValPRNG(0)

/*!
\def MD_random
\brief Convenience shorthand for MD_random_naive()
*/
#define MD_random() MACSIO_DATA_GetValPRNG(0)

/*!
\def MD_random_naive_tv
\brief Get next random value from naive, time-variant PRNG

The naive_tv PRNG is like naive except that it is seeded based on current time and so
will change from run to run (e.g. is time-variant). It should be used in place
of MD_random_naive() when randomization from run to run is required.
*/
#define MD_random_naive_tv() MACSIO_DATA_GetValPRNG(1)

/*!
\def MD_random_naive_rv
\brief Get next random value from naive, rank-variant PRNG

Like MD_random_naive() but *guarantees* variation across ranks.
*/
#define MD_random_naive_rv() MACSIO_DATA_GetValPRNG(2)

/*!
\def MD_random_naive_rtv
\brief Get next random value from naive, rank- and time-variant PRNG

Like MD_random_naive_rv() but guarantees variation from run to run.
*/
#define MD_random_naive_rtv() MACSIO_DATA_GetValPRNG(3)

/*!
\def MD_random_rankinv
\brief Get next random value from rank-invariant PRNG

The rank_invariant PRNG is neither seeded based on rank nor shall be used in way
that would cause inconsistency across ranks. Failing to call it collectively will
likely cause indeterminent behavior. There is no enforcement of this requirement.
However, a sanity check is performed at application termination...there is a
collective MPI_BXOR reduce on the current value from this PRNG and if the result
is non-zero, an error message is generated in the logs.
*/
#define MD_random_rankinv() MACSIO_DATA_GetValPRNG(4)

/*!
\def MD_random_rankinv_tv
\brief Get next random value from rank-invariant but time-variant PRNG

Like the MD_random_rankinv() except causes variation in randomization
from run to run
*/
#define MD_random_rankinv_tv() MACSIO_DATA_GetValPRNG(5)

/*!
\brief Initialize default Pseudo Random Number Generators (PRNGs)
Should be called near beginning of application startup.
Creates the (default) PRNGs described here. Applications are free
to create others as needed.
*/
extern void
MACSIO_DATA_InitializeDefaultPRNGs(
    unsigned mpi_rank, /**< unsigned representation of MPI rank of calling processor */
    unsigned curr_time /**< unsigned but otherwise arbitrary representation of current time but
                            which is equal on all ranks yet guaranteed to vary from run to run. */ 
);

/*!
\brief Free up resources for default PRNGs
Should be called near the termination of application.
*/
extern void
MACSIO_DATA_FinalizeDefaultPRNGs(void);

/*!@}*/

/*!
\defgroup MACSIO_RANDOBJ MACSIO_RANDOBJ
\brief Construct a random JSON object

@{
*/
extern struct json_object *
MACSIO_DATA_MakeRandomObject(
    int maxd,       /**< maximum depth of object tree */
    int nraw,       /**< # bytes of raw data (in extarr objects) in final object */
    int nmeta,      /**< # bytes of meta data (in non-extarr objects) in final object */
    unsigned maxds  /**< maximum size of any given raw (extarr) object */
);

/*!
\brief Construct a random table of some number of random JSON objects
*/
extern struct json_object *
MACSIO_DATA_MakeRandomTable(
    int nrecs,    /**< number of records in the table */
    int totbytes  /**< total bytes in the table */
);

/*!@}*/

extern struct json_object *
MACSIO_DATA_GenerateTimeZeroDumpObject(
     struct json_object *main_obj, /**< The main JSON object holding mesh, field, amorphous data */
     int *rank_owning_chunkId /**< missing this info */
);

/*!
\brief Given a chunkId, return rank of owning task
*/
extern int
MACSIO_DATA_GetRankOwningPart(
    struct json_object *main_obj,
    int chunkId
);

/*!
\brief Not yet implemented
*/
extern int
MACSIO_DATA_ValidateDataRead(
    struct json_object *main_obj
);

/*!
\brief Not yet implemented
*/
extern int MACSIO_DATA_SimpleAssignKPartsToNProcs(
    int k,
    int n,
    int my_rank,
    int *my_part_cnt,
    int **my_part_ids
);

/*!
\brief Vary mesh and field data including changing not only values but sizes
*/
extern struct json_object *
MACSIO_DATA_EvolveDataset(
    struct json_object *main_obj,
    int *dataset_evolved,
    float factor,
    int growth_bytes
);

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* _MACSIO_DATA_H */
