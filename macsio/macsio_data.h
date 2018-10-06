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

\defgroup PRNGs Randomization
\brief Randomization and Pseudo Random Number Generators

MACSio creates and initializes several pseudo random number generators (PRNGs),
all of which utilize the C standard library
<a href="http://man7.org/linux/man-pages/man3/random.3.html">random()</a>
method. The ids (e.g. handles) to each are stored in the main json object under
the tag "random_generator_ids". Each generator has a different name and purpose
and should be used accordingly.

Pseudo Random Number Generator (PRNG) support is handled by maintaining
a series of state vectors for calls to initstate/setstate of C library
random() and friends. Multiple different PRNGs are supported each having
zero or more special properties depending on how they are initialized
(e.g. seed used from run to run and/or from rank to rank). However,
properties of a PRNG also depend on usage. For example, if a PRNG is
intended to cause each rank to generate same data, then it is up to the
caller to ensure it is called consistently (e.g. collectively) across ranks.
Otherwise, indeterminent behavior will result.

The PRNGs MACSio creates are...

- "naive": The naive prng is fine for any randomization that does not need special behaviors.
  This is the one that is expected to be used most of the time. 
- "naive_tv": is like naive except that it is seeded based on current time and so
  will change from run to run (e.g. is Time Varying). It should be used in place
  of naive where randomization from run to run is desired.
- "naive_rv": like naive but ensures variation across ranks by seeding based on rank
- "naive_rtv": like naive_rv but also varies from run to run 
- "rank_invariant": is neither seeded based on rank or time nor shall be used in way that would
  cause inconsistency across ranks. It is up to the caller to ensure it is called
  consistently (e.g. collectively) across ranks. Otherwise indeterminent behavior
  may result.
- "rank_invariant_tv": like rank_invariant except causes variation in randomization from run to run

To use one of these PRNGs, you would do something like...

\code{.c}
 int prng_id = JsonGetInt(main_obj, "random_generator_ids/naive") 
 long random_value = MACSIO_DATA_GetValPRNG(prng_id);
\endcode

If these PRNGs are not sufficient, developers are free to create other PRNGs for
other specific purposes.

@{
*/

/*!
\brief Create a Pseudo Random Number Generator (PRNG)
\return An integer identifier for the created PRNG
*/
extern int  MACSIO_DATA_CreatePRNG(
    unsigned seed /**< seed for initializing random number generator */
);

/*!
\brief Reset a PRNG
This will cause the specified PRNG to restart the sequence of pseudo
random numbers it generates.
*/
extern void MACSIO_DATA_ResetPRNG(
    int prng_id /**< identifier for the PRNG to reset */
);

/*!
\brief Get next value from PRNG
\return Next value from the PRNG sequence. Logically equivalent to calling
        random().
*/
extern long MACSIO_DATA_GetValPRNG(
    int prng_id /**< identifer of the desired PRNG to get a value from */
);

/*!
\brief Free resources associated a PRNG
*/
extern void MACSIO_DATA_DestroyPRNG(
    int prng_id /**< identifer of the desired PRNG to free */
);

/*!
\def MD_random
\brief Get next random value from naive PRNG. Replacement for random()
*/
#define MD_random() MACSIO_DATA_GetValPRNG(0)

/*!
\def MD_random_naive
\brief Get next random value from naive PRNG. Same as MD_random().
The naive prng is fine for any randomization that does not need special behaviors.
This is the one that is expected to be used most of the time.
*/
#define MD_random_naive() MACSIO_DATA_GetValPRNG(0)

/*!
\def MD_random_naive_tv
\brief Get next random value from naive, time-variant PRNG
naive_tv is like naive except that it is seeded based on current time and so
will change from run to run (e.g. is time-variant). It should be used in place
of naive when randomization from run to run is desired.
*/
#define MD_random_naive_tv() MACSIO_DATA_GetValPRNG(1)

/*!
\def MD_random_naive_rv
\brief Get next random value from naive, rank-variant PRNG
Like naive but *guarantees* variation across ranks.
*/
#define MD_random_naive_rv() MACSIO_DATA_GetValPRNG(2)

/*!
\def MD_random_naive_rtv
\brief Get next random value from naive, rank- and time-variant PRNG
Like naive_rv but guarantees variation from run to run.
*/
#define MD_random_naive_rtv() MACSIO_DATA_GetValPRNG(3)

/*!
\def MD_random_rankinv
\brief Get next random value from rank-invariant PRNG
rank_invariant is neither seeded based on rank nor shall be used in way that would
cause inconsistency across ranks. Failing to call it collectively, will cause
indeterminent behavior.
*/
#define MD_random_rankinv() MACSIO_DATA_GetValPRNG(4)

/*!
\def MD_random_rankinv_tv
\brief Get next random value from rank-invariant but time-variant PRNG
like rank_invariant except causes variation in randomization from run to run
*/
#define MD_random_rankinv_tv() MACSIO_DATA_GetValPRNG(5)

/*!
\brief Initialize Pseudo Random Number Generators (PRNGs)
Should be called near beginning of application startup.
*/
extern void MACSIO_DATA_InitializePRNGs(
    unsigned mpi_rank, /**< unsigned representation of MPI rank of calling processor */
    unsigned curr_time /**< unsigned but otherwise arbitrary representation of current time
                            that is equal on all ranks yet guaranteed to vary from run to run. */ 
);

/*!
\brief Free up resources for PRNGs
Should be called near the termination of application.
*/
extern void MACSIO_DATA_FinalizePRNGs(void);

/*!@}*/

extern struct json_object *MACSIO_DATA_MakeRandomObject(int maxd, int nraw, int nmeta, unsigned maxds);
extern struct json_object *MACSIO_DATA_MakeRandomTable(int nrecs, int totbytes);
extern struct json_object *MACSIO_DATA_GenerateTimeZeroDumpObject(struct json_object *main_obj,
                               int *rank_owning_chunkId);
extern int                 MACSIO_DATA_GetRankOwningPart(struct json_object *main_obj, int chunkId);
extern int                 MACSIO_DATA_ValidateDataRead(struct json_object *main_obj);
extern int                 MACSIO_DATA_SimpleAssignKPartsToNProcs(int k, int n, int my_rank,
                               int *my_part_cnt, int **my_part_ids);
extern struct json_object *MACSIO_DATA_EvolveDataset(struct json_object *main_obj, int *dataset_evolved, float factor, int growth_bytes);

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* _MACSIO_DATA_H */
