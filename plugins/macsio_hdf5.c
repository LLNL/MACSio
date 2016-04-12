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

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <json-cwx/json.h>

#include <macsio_clargs.h>
#include <macsio_iface.h>
#include <macsio_log.h>
#include <macsio_main.h>
#include <macsio_mif.h>
#include <macsio_utils.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

#ifdef HAVE_SILO
#include <silo.h> /* for the Silo block based VFD option */
#endif

#include <H5pubconf.h>
#include <hdf5.h>
#include <H5Tpublic.h>

/* Disable debugging messages */

/*!
\addtogroup plugins
@{
*/

/*!
\addtogroup HDF5
@{
*/

#ifdef HAVE_ZFP
/*!
\addtogroup ZFP As HDF5 Compression Filter

Copyright (c) 2014-2015, RWTH Aachen University, JARA - Juelich Aachen Research Alliance.
Produced at the RWTH Aachen University, Germany.
Written by Jens Henrik Göbbert (goebbert@jara.rwth-aachen.de)
All rights reserved.

This file is part of the H5zfp library.
For details, see http://www.jara.org/de/research/jara-hpc/.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the disclaimer below.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the disclaimer (as noted below) in the
documentation and/or other materials provided with the distribution.

3. Neither the name of the University nor the names of its contributors may
be used to endorse or promote products derived from this software without
specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
@{
*/

#include <zfp.h>

#define PUSH_ERR(func, minor, str)                                      \
    H5Epush1(__FILE__, func, __LINE__, H5E_PLINE, minor, str)
#define H5Z_class_t_vers 2
#define ZFP_H5FILTER_ID 299

static htri_t H5Z_can_apply_zfp(hid_t dcpl_id,          /* dataset creation property list */
                                hid_t type_id,   /* datatype */
                                hid_t space_id); /* a dataspace describing a chunk */

static herr_t H5Z_set_local_zfp(hid_t dcpl_id,          /* dataset creation property list */
                                hid_t type_id,   /* datatype */
                                hid_t space_id); /* dataspace describing the chunk */

static size_t H5Z_filter_zfp(unsigned int flags,
                             size_t cd_nelmts,
                             const unsigned int cd_values[],
                             size_t nbytes,
                             size_t *buf_size,
                             void **buf);

static int H5Z_register_zfp(void);

/* write a 64 bit unsigned integer to a buffer in big endian order. */
static void zfp_write_uint64(void* buf, uint64_t num) {
    uint8_t* b = static_cast<uint8_t*>(buf);
    uint64_t pow28 = (uint64_t)1 << 8;
    for (int ii = 7; ii >= 0; ii--) {
        b[ii] = num % pow28;
        num = num / pow28;
    }
}

/* read a 64 bit unsigned integer from a buffer big endian order. */
static uint64_t zfp_read_uint64(void* buf) {
    uint8_t* b = static_cast<uint8_t*>(buf);
    uint64_t num = 0, pow28 = (uint64_t)1 << 8, cp = 1;
    for (int ii = 7; ii >= 0; ii--) {
        num += b[ii] * cp;
        cp *= pow28;
    }
    return num;
}

/* write a 64 bit signed integer to a buffer in big endian order. */
static void zfp_write_int64(void* buf, int64_t num) {
    uint64_t pow2x = (uint64_t)1 << 63;
    uint64_t num_uint = (num + pow2x);
    zfp_write_uint64(buf, num_uint);
}

/* read a 64 bit signed integer from a buffer big endian order. */
static int64_t zfp_read_int64(void* buf) {
    uint64_t num_uint = zfp_read_uint64(buf);
    uint64_t pow2x = (uint64_t)1 << 63;
    return (num_uint -pow2x);
}

/* write a 32 bit unsigned integer to a buffer in big endian order. */
static void zfp_write_uint32(void* buf, uint32_t num) {
    uint8_t* b = static_cast<uint8_t*>(buf);
    uint32_t pow28 = (uint32_t)1 << 8;
    for (int ii = 3; ii >= 0; ii--) {
        b[ii] = num % pow28;
        num = num / pow28;
    }
}

/* read a 32 bit unsigned integer from a buffer big endian order. */
static uint32_t zfp_read_uint32(void* buf) {
    uint8_t* b = static_cast<uint8_t*>(buf);
    uint32_t num = 0, pow28 = (uint32_t)1 << 8, cp = 1;
    for (int ii = 3; ii >= 0; ii--) {
        num += b[ii] * cp;
        cp *= pow28;
    }
    return num;
}

/* write a 32 bit signed integer to a buffer in big endian order. */
static void zfp_write_int32(void* buf, int32_t num) {
    uint32_t pow2x = (uint32_t)1 << 31;
    uint32_t num_uint = (num + pow2x);
    zfp_write_uint32(buf, num_uint);
}

/* read a 32 bit signed integer from a buffer big endian order. */
static int32_t zfp_read_int32(void* buf) {
    uint32_t num_uint = zfp_read_uint32(buf);
    uint32_t pow2x = (uint32_t)1 << 31;
    return (num_uint -pow2x);
}

const H5Z_class2_t ZFP_H5Filter[1] = {{
    H5Z_CLASS_T_VERS,                         /* H5Z_class_t version          */
    (H5Z_filter_t)(ZFP_H5FILTER_ID),          /* Filter id number             */
    1,                                        /* encoder_present flag (set to true) */
    1,                                        /* decoder_present flag (set to true) */
    "Lindstrom-ZFP compression; See http://computation.llnl.gov/casc/zfp/",
                                              /* Filter name for debugging    */
    (H5Z_can_apply_func_t) H5Z_can_apply_zfp, /* The "can apply" callback     */
    (H5Z_set_local_func_t) H5Z_set_local_zfp, /* The "set local" callback     */
    (H5Z_func_t) H5Z_filter_zfp,              /* The actual filter function   */
}};

/*!
\brief Is filter valid for this dataset?

Before a dataset is created 'can_apply' callback for any filter
used in the dataset creation property list are called.

\return valid: non-negative
\return invalid: zero
\return error: negative
*/
static herr_t
H5Z_can_apply_zfp(hid_t dcpl_id, hid_t type_id, hid_t space_id)
{
    hid_t dclass;
    size_t dsize;
    hid_t dntype;
    size_t ndims;
    unsigned int flags;

    size_t cd_nelmts_in = 5;
    unsigned int cd_values_in[]  = {0,0,0,0,0};

    /* check object identifier */
    /* ----------------------- */

    /* Check if object exists as datatype - try to get pointer to memory referenced by id */
    if(H5Iis_valid(type_id)) {
      PUSH_ERR("H5Z_can_apply_zfp", H5E_CALLBACK, "no valid type");
      return -1;
    }

#if 0
    /* TODO: print datatype name if debug-mode */
    ssize_t dname_size = 0;
    if(0 >= (dname_size = H5Iget_name(type_id, NULL, 0))) {
      PUSH_ERR("H5Z_can_apply_zfp", H5E_CALLBACK, "no valid datatype name");
      return -1;
    }
    dname_size = dname_size+1;
    char dname[dname_size];
    H5Iget_name( type_id, (char *)(&dname), (size_t)dname_size );
    fprintf(stdout,"   H5Z_can_apply_zfp: dname = %d, '%.*s' \n",(int)dname_size, (int)dname_size, dname);
#endif

    /* check datatype ( which describes elements of a dataset ) */
    /* -------------- */

    /* check datatype's class - try to get datatype class identifier
     * typedef enum H5T_class_t {
     * 		H5T_NO_CLASS = -1,  error
     * 		H5T_INTEGER  = 0,   integer types
     * 		H5T_FLOAT    = 1,   floating-point types
     */
    if(H5T_NO_CLASS == (dclass = H5Tget_class(type_id))) {
      PUSH_ERR("H5Z_can_apply_zfp", H5E_CALLBACK, "no valid datatype class");
      return -1;
    }
#ifndef NDEBUG
    fprintf(stdout,"   H5Z_can_apply_zfp: dclass = %d", dclass);
    if(dclass != H5T_FLOAT) {
      fprintf(stdout," ... NOT SUPPORTED => skip compression\n");
      return 0;
    };
    fprintf(stdout,"\n");
#else
    if(dclass != H5T_FLOAT) {return 0;}; /* only support floats */
#endif

    /* check datatype's native-type - must be double or float */
#if 0
/*  TODO: this check results in register a new native type instead of test for a type
    dntype = H5Tget_native_type(type_id, H5T_DIR_DESCEND); //H5T_DIR_ASCEND);
    if(     H5T_NATIVE_FLOAT  == dntype) { fprintf(stdout,"\nzfp_h5_can_apply: dntype = %d \n", dntype); }
    else if(H5T_NATIVE_DOUBLE == dntype) { fprintf(stdout,"\nzfp_h5_can_apply: dntype = %d \n", dntype); }
    else {
      PUSH_ERR("H5Z_can_apply_zfp", H5E_CALLBACK, "no valid datatype type");
    }
*/
#endif

    /* check datatype's byte-size per value - must be single(==4) or double(==8) */
    if(0 == (dsize = H5Tget_size(type_id))) {
      PUSH_ERR("H5Z_can_apply_zfp", H5E_CALLBACK, "no valid datatype size");
      return -1;
    }
#ifndef NDEBUG
    fprintf(stdout,"   H5Z_can_apply_zfp: dsize = %d ", (int)dsize);
    if(dsize != 4 && dsize != 8) {
      fprintf(stdout," ... NOT SUPPORTED => skip compression\n");
      return 0;
    }
    fprintf(stdout,"\n");
#else
    if(dsize != 4 && dsize != 8) { return 0;}; /* only support 4byte and 8byte floats */
#endif

    /* check chunk    */
    /* -------------- */

    if(0 == (ndims = H5Sget_simple_extent_ndims(space_id))) {
      PUSH_ERR("H5Z_can_apply_zfp", H5E_CALLBACK, "no valid no. space dimensions");
      return -1;
    }
    hsize_t dims[ndims];
    H5Sget_simple_extent_dims(space_id, dims, NULL);

#ifndef NDEBUG
    fprintf(stdout,"   H5Z_can_apply_zfp: ndims = %d ", (int)ndims);
    for (size_t ii=0; ii < ndims; ii++) { fprintf(stdout," %d,",(int)dims[ii]); }
    if(ndims != 1 && ndims != 2 && ndims != 3) {
      fprintf(stdout," ... NOT SUPPORTED => skip compression\n");
      return 0;
    }
    fprintf(stdout,"\n");
#else
    if(ndims != 1 && ndims != 2 && ndims != 3) { return 0;}; /* only support 1d, 2d and 3d arrays */
#endif

    /* check filter */
    /* ------------ */

    /* get current filter values */
    if(H5Pget_filter_by_id2(dcpl_id, ZFP_H5FILTER_ID,
                            &flags, &cd_nelmts_in, cd_values_in,
                            0, NULL, NULL) < 0) { return -1; }

    /* check no. elements    */
    /* -------------- */
    long dcount = 1;
    int dcount_threshold = cd_values_in[4];
    if(dcount_threshold <= 0) { dcount_threshold = 64; }
    for ( size_t ii=0; ii < ndims; ii++) { dcount = dcount *dims[ii]; }

#ifndef NDEBUG
    fprintf(stdout,"   H5Z_can_apply_zfp: dcount = %ld ", dcount);
    if(dcount < dcount_threshold) {
      fprintf(stdout," ... TOO SMALL => skip compression\n");
      return 0;
    }
    fprintf(stdout,"\n");
#else
    if(dcount < dcount_threshold) { return 0; } /* only support datasets with count >= threshold */
#endif

#ifndef NDEBUG
    fprintf(stdout,"   H5Z_can_apply_zfp: cd_values = [");
    for (size_t ii=0; ii < cd_nelmts_in; ii++) { fprintf(stdout," %d,",cd_values_in[ii]); }
    fprintf(stdout,"]\n");
#endif
    return 1;

 cleanupAndFail:
    return 0;
}

/*!
\brief Set compression parameters

Only called on compression, not on reverse.

\return Success: Non-negative
\return Failure: Negative
*/
static herr_t
H5Z_set_local_zfp(hid_t dcpl_id, hid_t type_id, hid_t space_id)
{
    size_t dsize;
    size_t ndims;

    unsigned int flags;
    size_t cd_nelmts = 5;
    size_t cd_nelmts_max = 12;
    unsigned int cd_values_in[]  = {0,0,0,0,0};
    unsigned int cd_values_out[] = {0,0,0,0,0,0,0,0,0,0,0,0};
    hsize_t dims[6];

    /* check datatype's byte-size per value - must be single(==4) or double(==8) */
    if(0 == (dsize = H5Tget_size(type_id))) {
      PUSH_ERR("H5Z_set_local_zfp", H5E_CALLBACK, "no valid datatype size");
      return -1;
    }

    /* check chunk    */
    /* -------------- */

    if(0 == (ndims = H5Sget_simple_extent_ndims(space_id))) {
      PUSH_ERR("H5Z_set_local_zfp", H5E_CALLBACK, "no valid no. space dimensions");
      return -1;
    }
    H5Sget_simple_extent_dims(space_id, dims, NULL);

    /* check filter */
    /* ------------ */

    /* get current filter values */
    if(H5Pget_filter_by_id2(dcpl_id, ZFP_H5FILTER_ID,
                            &flags, &cd_nelmts, cd_values_in, /*cd_nelmts (inout) */
                            0, NULL, NULL) < 0) { return -1; }

    /* correct number of parameters and fill missing with defaults
          ignore value-ids larger 5
          add zero for non-existent value-ids <= 4
          add default threshold for value-id == 5 */
    if(cd_nelmts != 5) {
        if(cd_nelmts < 5) cd_values_in[4] = 64;    /* set default threshold = 64 (already used and not required any more) */
        if(cd_nelmts < 4) cd_values_in[3] = -1074; /* set default minexp    = -1074 */
        if(cd_nelmts < 3) cd_values_in[2] = 0;     /* set default maxprec   = 0  (set default in compression filter) */
        if(cd_nelmts < 2) cd_values_in[1] = 0;     /* set default maxbits   = 0  (set default in compression filter) */
        if(cd_nelmts < 1) cd_values_in[0] = 0;     /* set default minbits   = 0  (set default in compression filter) */
        cd_nelmts = 5;
    }

    /* modify cd_values to pass them to filter-func */
    /* -------------------------------------------- */

    /* First 4+ndims slots reserved. Move any passed options to higher addresses. */
    size_t valshift = 4 +ndims;
    for (size_t ii = 0; ii < cd_nelmts && ii +valshift < cd_nelmts_max; ii++) {
        cd_values_out[ii + valshift] = cd_values_in[ii];
    }

    int16_t version = ZFP_VERSION;
    cd_values_out[0] = version >> 4;      /* major version */
    cd_values_out[1] = version & 0x000f;  /* minor version */
    cd_values_out[2] = dsize;             /* bytes per element */
    cd_values_out[3] = ndims;             /* number of dimensions */
    for (size_t ii = 0; ii < ndims; ii++)
      cd_values_out[ii +4] = dims[ii];    /* size of dimensions */

    cd_nelmts = cd_nelmts + valshift;

    if(H5Pmodify_filter(dcpl_id, ZFP_H5FILTER_ID, flags, cd_nelmts, cd_values_out)) {
      PUSH_ERR("H5Z_set_local_zfp", H5E_CALLBACK, "modify filter not possible");
      return -1;
    }

    return 1;

 cleanupAndFail:
    return 0;
}

/*!
\brief Only called if data in dataset (not if only zeros)

\return Zero on failure
*/
static size_t
H5Z_filter_zfp(unsigned int flags,
               size_t cd_nelmts,               /* config data elements */
               const unsigned int cd_values[], /* config data values */
               size_t nbytes,                  /* number of bytes */
               size_t *buf_size,               /* buffer size */
               void **buf)                     /* buffer */
{
    unsigned int zfp_major_ver, zfp_minor_ver;
    size_t dsize;
    size_t ndims;

    size_t buf_size_out = -1;
    size_t nbytes_out = -1;
    bool dp = false;

    void* tmp_buf = NULL;
    void* zfp_buf = NULL;
    void* out_buf = NULL;

    /* get config values
      - from H5Z_set_local_zfp(..) on compression
      - from chunk-header on reverse */
    if (cd_nelmts < 4) {
        PUSH_ERR("H5Z_filter_zfp", H5E_CALLBACK, "Not enough parameters.");
        return 0;
    }
    zfp_major_ver = cd_values[0];  /* not used - we could add it to the header ? */
    zfp_minor_ver = cd_values[1];  /* not used - we could add it to the header ? */
    dsize         = cd_values[2];  /* bytes per element */
    ndims         = cd_values[3];  /* number of dimensions */
    hsize_t dims[ndims];
    for (size_t ii = 0; ii < ndims; ii++)
      dims[ii] = cd_values[ii +4]; /* size of dimensions */

    /* get size of out_buf
    -----------------------*/

    /* size of floating-point type in bytes */
    if(dsize == 8) { dp = true; }
    else if(dsize == 4) {dp = false; }
    else {
      PUSH_ERR("H5Z_filter_zfp", H5E_CALLBACK, "unknown precision type");
      return 0;
    }
    size_t typesize = dp ? sizeof(double) : sizeof(float);

    /* effective array dimensions */
    uint nx = 0;
    uint ny = 0;
    uint nz = 0;
    if(ndims > 0) nx = dims[0];
    if(ndims > 1) ny = dims[1];
    if(ndims > 2) nz = dims[2];

    /* initialize zfp parameters */
    zfp_params params;
    zfp_init(&params);
    zfp_set_type(&params, dp ? ZFP_TYPE_DOUBLE : ZFP_TYPE_FLOAT);
    if(ndims > 0) zfp_set_size_1d(&params, nx);
    if(ndims > 1) zfp_set_size_2d(&params, nx, ny);
    if(ndims > 2) zfp_set_size_3d(&params, nx, ny, nz);

    /* decompress data */
    if (flags & H5Z_FLAG_REVERSE) {

        /* read 16byte header = (minbits, maxbits, maxprec, minexp) */
        tmp_buf = *buf;
        params.minbits = zfp_read_uint32((char*) tmp_buf + 0);
        params.maxbits = zfp_read_uint32((char*) tmp_buf + 4);
        params.maxprec = zfp_read_uint32((char*) tmp_buf + 8);
        params.minexp  = zfp_read_int32 ((char*) tmp_buf +12);

        /* allocate for uncompressed */
        size_t buf_size_in = std::max(params.nx, 1u)
                           * std::max(params.ny, 1u)
                           * std::max(params.nz, 1u)
                           * ((params.type == ZFP_TYPE_DOUBLE) ? sizeof(double) : sizeof(float));
        out_buf = malloc(buf_size_in);
        if (out_buf == NULL) {
          PUSH_ERR("H5Z_filter_zfp", H5E_CALLBACK, "Could not allocate output buffer.");
          return 0;
        }

#ifndef NDEBUG
        fprintf(stdout, "   H5Z_filter_zfp: decompress: [%d, (%d, %d, %d), %d, %d, %d, %d] = %d \n",
                dp, nx, ny, nz,
                params.minbits, params.maxbits, params.maxprec, params.minexp,
                (int)buf_size_in);
#endif

        /* buf[16+] -> out_buf */
        zfp_buf = (char*) tmp_buf +16;

#if 0
        /* decompress 1D, 2D, or 3D floating-point array */
        // int                         /* nonzero on success */
        // zfp_decompress(
        //   const zfp_params* params, /* array meta data and compression parameters */
        //   void* out,                /* decompressed floating-point data */
        //   const void* in,           /* compressed stream */
        //   size_t insize             /* bytes allocated for compressed stream */
        // );
#endif
        if (zfp_decompress(&params, out_buf, zfp_buf, buf_size_in) == 0) {
           PUSH_ERR("H5Z_filter_zfp", H5E_CALLBACK, "decompression failed");
           free(out_buf);
           return 0;
        }
        nbytes_out = buf_size_in;

      /* compress data */
      } else {

        /* compression parameter (0 == no compression) */
        uint minbits = cd_values[4+ndims +0]; /* minimum number of bits per 4^d values in d dimensions (= maxbits for fixed rate) */
        uint maxbits = cd_values[4+ndims +1]; /* maximum number of bits per 4^d values in d dimensions */
        uint maxprec = cd_values[4+ndims +2]; /* maximum number of bits of precision per value (0 for fixed rate) */
        int  minexp  = cd_values[4+ndims +3]; /* minimum bit plane coded (soft error tolerance = 2^minexp; -1024 for fixed rate) */

        /* correct compression parameters if zero initialized */
        uint blocksize = 1u << (2 * ndims); // number of floating-point values per block
        if(minbits <= 0 || minbits > 4096) minbits = blocksize * CHAR_BIT * typesize;    /* {1, ..., 4096}     == 12 bits */
        if(maxbits <= 0 || maxbits > 4096) maxbits = blocksize * CHAR_BIT * typesize;    /* {1, ..., 4096}     == 12 bits */
        if(maxprec <= 0 || maxprec > 64)   maxprec = CHAR_BIT * typesize;                /* {1, ..., 64}       ==  6 bits */
        if(minexp  < -1074) minexp = -1074;                                              /* {-1074, ..., 1023} ==  12 bits */
        if(minexp  >  1023) minexp =  1023;

        /* set compression parameters */
        params.minbits = minbits;
        params.maxbits = maxbits;
        params.maxprec = maxprec;
        params.minexp  = minexp;

        /* max.(!) size of compressed data */
        size_t buf_size_maxout = zfp_estimate_compressed_size(&params);
        if (buf_size_maxout == 0) {
          std::cerr << "invalid compression parameters" << std::endl;
          return 0;
        }

        /* allocate for header + compressed data */
        tmp_buf = malloc(buf_size_maxout +16);
        if (tmp_buf == NULL) {
          PUSH_ERR("H5Z_filter_zfp", H5E_CALLBACK, "Could not allocate output buffer.");
          return 0;
        }

        /* ptr to data (skipping header) */
        zfp_buf = (char*) tmp_buf +16;

#if 0
        /* compress 1D, 2D, or 3D floating-point array */
        // size_t                      /* byte size of compressed stream (0 on failure) */
        // zfp_compress(
        //  const zfp_params* params, /* array meta data and compression parameters */
        //  const void* in,           /* uncompressed floating-point data */
        //  void* out,                /* compressed stream (must be large enough) */
        //  size_t outsize            /* bytes allocated for compressed stream */
        // );
#endif
        buf_size_out = zfp_compress(&params, *buf, zfp_buf, buf_size_maxout);
        if (buf_size_out == 0) {
          PUSH_ERR("H5Z_filter_zfp", H5E_CALLBACK, "compression failed");
          free(out_buf);
          return 0;
        }

#ifndef NDEBUG
        fprintf(stdout, "   H5Z_filter_zfp: compress(out): [%d, (%d, %d, %d), %d, %d, %d, %d] = %d \n",
                dp, nx, ny, nz,
                params.minbits, params.maxbits, params.maxprec, params.minexp,
                (int)buf_size_out);
#endif

        /* add 32byte header = (minbits, maxbits, maxprec, minexp) */
        zfp_write_uint32((char*) tmp_buf + 0, (uint32_t) minbits);
        zfp_write_uint32((char*) tmp_buf + 4, (uint32_t) maxbits);
        zfp_write_uint32((char*) tmp_buf + 8, (uint32_t) maxprec);
        zfp_write_int32 ((char*) tmp_buf +12, ( int32_t) minexp);

        nbytes_out = buf_size_out +16;

        /* realloc (free partly) if required */
        if(buf_size_out < buf_size_maxout) {
          out_buf = realloc(tmp_buf, nbytes_out); /* free out_buf partly (which was not used by ZFP::compress(..)) */
          if (out_buf == NULL) {
            PUSH_ERR("H5Z_filter_zfp", H5E_CALLBACK, "Could not reallocate output buffer.");
            return 0;
          }
        } else {
            out_buf = tmp_buf;
        }
    }

    /* flip buffer and return
    -------------------------*/
    free(*buf);
    *buf = out_buf;
    *buf_size = nbytes_out;

    return nbytes_out;
}

static int
H5Z_register_zfp(void)
{
    H5Zregister(ZFP_H5Filter);
}

#endif /* HAVE_ZFP */

/*!@}*/

/* the name you want to assign to the interface */
static char const *iface_name = "hdf5";
static char const *iface_ext = "h5";

static int use_log = 0;
static int no_collective = 0;
static int no_single_chunk = 0;
static int silo_block_size = 0;
static int silo_block_count = 0;
static int sbuf_size = -1;
static int mbuf_size = -1;
static int rbuf_size = -1;
static int lbuf_size = 0;
static const char *filename;
static hid_t fid;
static hid_t dspc = -1;
static int show_errors = 0;
static char compression_alg_str[64];
static char compression_params_str[512];

static hid_t make_fapl()
{
    hid_t fapl_id = H5Pcreate(H5P_FILE_ACCESS);
    herr_t h5status = 0;

    if (sbuf_size >= 0)
        h5status |= H5Pset_sieve_buf_size(fapl_id, sbuf_size);

    if (mbuf_size >= 0)
        h5status |= H5Pset_meta_block_size(fapl_id, mbuf_size);

    if (rbuf_size >= 0)
        h5status |= H5Pset_small_data_block_size(fapl_id, mbuf_size);

#if 0
    if (silo_block_size && silo_block_count)
    {
        h5status |= H5Pset_fapl_silo(fapl_id);
        h5status |= H5Pset_silo_block_size_and_count(fapl_id, (hsize_t) silo_block_size,
            silo_block_count);
    }
    else
    if (use_log)
    {
        int flags = H5FD_LOG_LOC_IO|H5FD_LOG_NUM_IO|H5FD_LOG_TIME_IO|H5FD_LOG_ALLOC;

        if (lbuf_size > 0)
            flags = H5FD_LOG_ALL;

        h5status |= H5Pset_fapl_log(fapl_id, "macsio_hdf5_log.out", flags, lbuf_size);
    }
#endif

    {
        H5AC_cache_config_t config;

        /* Acquire a default mdc config struct */
        config.version = H5AC__CURR_CACHE_CONFIG_VERSION;
        H5Pget_mdc_config(fapl_id, &config);
#define MAINZER_PARAMS 1
#if MAINZER_PARAMS
        config.set_initial_size = (hbool_t) 1;
        config.initial_size = 16 * 1024;
        config.min_size = 8 * 1024;
        config.epoch_length = 3000;
        config.lower_hr_threshold = 0.95;
#endif
        H5Pset_mdc_config(fapl_id, &config);
    }

    if (h5status < 0)
    {
        if (fapl_id >= 0)
            H5Pclose(fapl_id);
        return 0;
    }

    return fapl_id;
}

static int
get_tokval(char const *src_str, char const *token_to_match, void *val_ptr)
{
    int toklen;
    char dummy[16];
    void *val_ptr_tmp = &dummy[0];

    if (!src_str) return 0;
    if (!token_to_match) return 0;

    toklen = strlen(token_to_match)-2;

    if (strncasecmp(src_str, token_to_match, toklen))
        return 0;

    /* First, check matching sscanf *without* risk of writing to val_ptr */
    if (sscanf(&src_str[toklen], &token_to_match[toklen], val_ptr_tmp) != 1)
        return 0;

    sscanf(&src_str[toklen], &token_to_match[toklen], val_ptr);
    return 1;
}

static hid_t make_dcpl(char const *alg_str, char const *params_str, hid_t space_id, hid_t dtype_id)
{
    int shuffle = -1;
    int minsize = -1;
    int gzip_level = -1;
    int zfp_precision = -1;
    unsigned int szip_pixels_per_block = 0;
    float zfp_rate = -1;
    float zfp_accuracy = -1;
    char szip_method[64], szip_chunk_str[64];
    char *token, *string, *tofree;
    int ndims;
    hsize_t dims[4], maxdims[4];
    hid_t retval = H5Pcreate(H5P_DATASET_CREATE);

    szip_method[0] = '\0';
    szip_chunk_str[0] = '\0';

    /* Initially, set contiguous layout. May reset to chunked later */
    H5Pset_layout(retval, H5D_CONTIGUOUS);

    if (!alg_str || !strlen(alg_str))
        return retval;

    ndims = H5Sget_simple_extent_ndims(space_id);
    H5Sget_simple_extent_dims(space_id, dims, maxdims);

    /* We can make a pass through params string without being specific about
       algorithm because there are presently no symbol collisions there */
    tofree = string = strdup(params_str);
    while ((token = strsep(&string, ",")) != NULL)
    {
        if (get_tokval(token, "minsize=%d", &minsize))
            continue;
        if (get_tokval(token, "shuffle=%d", &shuffle))
            continue;
        if (get_tokval(token, "level=%d", &gzip_level))
            continue;
        if (get_tokval(token, "rate=%f", &zfp_rate))
            continue;
        if (get_tokval(token, "precision=%d", &zfp_precision))
            continue;
        if (get_tokval(token, "accuracy=%f", &zfp_accuracy))
            continue;
        if (get_tokval(token, "method=%s", szip_method))
            continue;
        if (get_tokval(token, "block=%u", &szip_pixels_per_block))
            continue;
        if (get_tokval(token, "chunk=%s", szip_chunk_str))
            continue;
    }
    free(tofree);

    /* check for minsize compression threshold */
    minsize = minsize != -1 ? minsize : 1024;
    if (H5Sget_simple_extent_npoints(space_id) < minsize)
        return retval;

    /*
     * Ok, now handle various properties related to compression
     */
 
    /* Initially, as a default in case nothing else is selected,
       set chunk size equal to dataset size (e.g. single chunk) */
    H5Pset_chunk(retval, ndims, dims);

    if (!strncasecmp(alg_str, "gzip", 4))
    {
        if (shuffle == -1 || shuffle == 1)
            H5Pset_shuffle(retval);
        H5Pset_deflate(retval, gzip_level!=-1?gzip_level:9);
    }
#ifdef HAVE_ZFP
    else if (!strncasecmp(alg_str, "lindstrom-zfp", 13))
    {
        int i;
        zfp_params params;
        unsigned int cd_values[32];

        /* We don't shuffle zfp. That is handled internally to zfp */

        zfp_init(&params);
        params.type = H5Tget_size(dtype_id) == 4 ? ZFP_TYPE_FLOAT : ZFP_TYPE_DOUBLE;
        if (ndims >= 1) params.nx = dims[0];
        if (ndims >= 2) params.ny = dims[1];
        if (ndims >= 3) params.nz = dims[2];
        if (zfp_rate != -1)
            zfp_set_rate(&params, (double) zfp_rate);
        else if (zfp_precision != -1)
            zfp_set_precision(&params, zfp_precision);
        else if (zfp_accuracy != -1)
            zfp_set_accuracy(&params, (double) zfp_accuracy);
        else
            zfp_set_rate(&params, 0.0); /* default rate-constrained */

        i = 0;
        cd_values[i++] = (unsigned int) params.minbits;
        cd_values[i++] = (unsigned int) params.maxbits;
        cd_values[i++] = (unsigned int) params.maxprec;
        cd_values[i++] = (unsigned int) params.minexp;
        cd_values[i++] = (unsigned int) minsize;
        H5Pset_filter(retval, ZFP_H5FILTER_ID, H5Z_FLAG_OPTIONAL, i, cd_values);
    }
#endif
    else if (!strncasecmp(alg_str, "szip", 4))
    {
#ifdef HAVE_SZIP
        unsigned int method = H5_SZIP_NN_OPTION_MASK;
        int const szip_max_blocks_per_scanline = 128; /* from szip lib */

        if (shuffle == -1 || shuffle == 1)
            H5Pset_shuffle(retval);

        if (szip_pixels_per_block == 0)
            szip_pixels_per_block = 32;
        if (!strcasecmp(szip_method, "ec"))
            method = H5_SZIP_EC_OPTION_MASK;

        H5Pset_szip(retval, method, szip_pixels_per_block);

        if (strlen(szip_chunk_str))
        {
            hsize_t chunk_dims[3] = {0, 0, 0};
            int i, vals[3];
            int nvals = sscanf(szip_chunk_str, "%d:%d:%d", &vals[0], &vals[1], &vals[2]);
            if (nvals == ndims)
            {
                for (i = 0; i < ndims; i++)
                    chunk_dims[i] = vals[i];
            }
            else if (nvals == ndims-1)
            {
                chunk_dims[0] = szip_max_blocks_per_scanline * szip_pixels_per_block;
                for (i = 1; i < ndims; i++)
                    chunk_dims[i] = vals[i-1];
            }
            for (i = 0; i < ndims; i++)
            {
                if (chunk_dims[i] > dims[i]) chunk_dims[i] = dims[0];
                if (chunk_dims[i] == 0) chunk_dims[i] = dims[0];
            }
            H5Pset_chunk(retval, ndims, chunk_dims);
        }
#else
        static int have_issued_warning = 0;
        if (!have_issued_warning)
            MACSIO_LOG_MSG(Warn, ("szip compressor not available in this build"));
        have_issued_warning = 1;
#endif
    }

    return retval;
}

static int process_args(int argi, int argc, char *argv[])
{
    const MACSIO_CLARGS_ArgvFlags_t argFlags = {MACSIO_CLARGS_WARN, MACSIO_CLARGS_TOMEM};

    char *c_alg = compression_alg_str;
    char *c_params = compression_params_str;

    MACSIO_CLARGS_ProcessCmdline(0, argFlags, argi, argc, argv,
        "--show_errors", "",
            "Show low-level HDF5 errors",
            &show_errors,
        "--compression %s %s", MACSIO_CLARGS_NODEFAULT,
            "The first string argument is the compression algorithm name. The second\n"
            "string argument is a comma-separated set of params of the form\n"
            "'param1=val1,param2=val2,param3=val3. The various algorithm names and\n"
            "their parameter meanings are described below. Note that some parameters are\n"
            "not specific to any algorithm. Those are described first followed by\n"
            "individual algorithm-specific parameters.\n"
            "\n"
            "minsize=%d : min. size of dataset (in terms of a count of values)\n"
            "    upon which compression will even be attempted. Default is 1024.\n"
            "shuffle=<int>: Boolean (zero or non-zero) to indicate whether to use\n"
            "    HDF5's byte shuffling filter *prior* to compression. Default depends\n"
            "    on algorithm. By default, shuffling is NOT used for lindstrom-zfp but IS\n"
            "    used with all other algorithms.\n"
            "\n"
#ifdef HAVE_ZFP
            "\"lindstrom-zfp\"\n"
            "    Use Peter Lindstrom's ZFP compression (computation.llnl.gov/casc/zfp)\n"
            "    The following options are *mutually*exclusive*. In any command-line\n"
            "    specifying more than one of the following options, only the last\n"
            "    specified will be honored.\n"
            "        rate=%f : target # bits per compressed output datum. Fractional values\n"
            "            are permitted. 0 selects defaults: 4 bits/flt or 8 bits/dbl.\n"
            "            Use this option to hit a target compressed size but where error\n"
            "            varies. OTOH, use one of the following two options for fixed\n"
            "            error but amount of compression, if any, varies.\n"
            "        precision=%d : # bits of precision to preserve in each input datum.\n"
            "        accuracy=%f : absolute error tolerance in each output datum.\n"
            "            In many respects, 'precision' represents a sort of relative error\n"
            "            tolerance while 'accuracy' represents an absolute tolerance.\n"
            "            See http://en.wikipedia.org/wiki/Accuracy_and_precision.\n"
            "\n"
#endif
            "\"szip\"\n"
            "    method=%s : specify 'ec' for entropy coding or 'nn' for nearest\n"
            "        neighbor. Default is 'nn'\n"
            "    block=%d : (pixels-per-block) must be an even integer <= 32. See\n"
            "        See H5Pset_szip in HDF5 documentation for more information.\n"
            "        Default is 32.\n"
            "    chunk=%d:%d : colon-separated dimensions specifying chunk size in\n"
            "        each dimension higher than the first (fastest varying) dimension.\n"
            "\n"
            "\"gzip\"\n"
            "    level=%d : A value in the range [1,9], inclusive, trading off time to\n"
            "        compress with amount of compression. Level=1 results in best speed\n"
            "        but worst compression whereas level=9 results in best compression\n"
            "        but worst speed. Values outside [1,9] are clamped. Default is 9.\n"
            "\n"
            "Examples:\n"
            "    --compression lindstrom-zfp rate=18.5\n"
            "    --compression gzip minsize=1024,level=9\n"
            "    --compression szip shuffle=0,options=nn,pixels_per_block=16\n"
            "\n",
            &c_alg, &c_params,
        "--no_collective", "",
            "Use independent, not collective, I/O calls in SIF mode.",
            &no_collective,
        "--no_single_chunk", "",
            "Do not single chunk the datasets (currently ignored).",
            &no_single_chunk,
        "--sieve_buf_size %d", MACSIO_CLARGS_NODEFAULT,
            "Specify sieve buffer size (see H5Pset_sieve_buf_size)",
            &sbuf_size,
        "--meta_block_size %d", MACSIO_CLARGS_NODEFAULT,
            "Specify size of meta data blocks (see H5Pset_meta_block_size)",
            &mbuf_size,
        "--small_block_size %d", MACSIO_CLARGS_NODEFAULT,
            "Specify threshold size for data blocks considered to be 'small'\n"
            "(see H5Pset_small_data_block_size)",
            &rbuf_size,
        "--log", "",
            "Use logging Virtual File Driver (see H5Pset_fapl_log)",
            &use_log,
#ifdef HAVE_SILO
        "--silo_fapl %d %d", MACSIO_CLARGS_NODEFAULT,
            "Use Silo's block-based VFD and specify block size and block count", 
            &silo_block_size, &silo_block_count,
#endif
           MACSIO_CLARGS_END_OF_ARGS);

    if (!show_errors)
        H5Eset_auto1(0,0);
    return 0;
}

static void main_dump_sif(json_object *main_obj, int dumpn, double dumpt)
{
#ifdef HAVE_MPI
    int ndims;
    int i, v, p;
    char const *mesh_type = json_object_path_get_string(main_obj, "clargs/part_type");
    char fileName[256];
    int use_part_count;

    hid_t h5file_id;
    hid_t fapl_id = make_fapl();
    hid_t dxpl_id = H5Pcreate(H5P_DATASET_XFER);
    hid_t dcpl_id = H5Pcreate(H5P_DATASET_CREATE);
    hid_t null_space_id = H5Screate(H5S_NULL);
    hid_t fspace_nodal_id, fspace_zonal_id;
    hsize_t global_log_dims_nodal[3];
    hsize_t global_log_dims_zonal[3];

    MPI_Info mpiInfo = MPI_INFO_NULL;

#warning WE ARE DOING SIF SLIGHTLY WRONG, DUPLICATING SHARED NODES
#warning INCLUDE ARGS FOR ISTORE AND K_SYM
#warning INCLUDE ARG PROCESS FOR HINTS
#warning FAPL PROPS: ALIGNMENT 
#if H5_HAVE_PARALLEL
    H5Pset_fapl_mpio(fapl_id, MACSIO_MAIN_Comm, mpiInfo);
#endif

#warning FOR MIF, NEED A FILEROOT ARGUMENT OR CHANGE TO FILEFMT ARGUMENT
    /* Construct name for the HDF5 file */
    sprintf(fileName, "%s_hdf5_%03d.%s",
        json_object_path_get_string(main_obj, "clargs/filebase"),
        dumpn,
        json_object_path_get_string(main_obj, "clargs/fileext"));

    h5file_id = H5Fcreate(fileName, H5F_ACC_TRUNC, H5P_DEFAULT, fapl_id);

    /* Create an HDF5 Dataspace for the global whole of mesh and var objects in the file. */
    ndims = json_object_path_get_int(main_obj, "clargs/part_dim");
    json_object *global_log_dims_array =
        json_object_path_get_array(main_obj, "problem/global/LogDims");
    json_object *global_parts_log_dims_array =
        json_object_path_get_array(main_obj, "problem/global/PartsLogDims");
    /* Note that global zonal array is smaller in each dimension by one *ON*EACH*BLOCK*
       in the associated dimension. */
    for (i = 0; i < ndims; i++)
    {
        int parts_log_dims_val = JsonGetInt(global_parts_log_dims_array, "", i);
        global_log_dims_nodal[ndims-1-i] = (hsize_t) JsonGetInt(global_log_dims_array, "", i);
        global_log_dims_zonal[ndims-1-i] = global_log_dims_nodal[ndims-1-i] -
            JsonGetInt(global_parts_log_dims_array, "", i);
    }
    fspace_nodal_id = H5Screate_simple(ndims, global_log_dims_nodal, 0);
    fspace_zonal_id = H5Screate_simple(ndims, global_log_dims_zonal, 0);

    /* Get the list of vars on the first part as a guide to loop over vars */
    json_object *part_array = json_object_path_get_array(main_obj, "problem/parts");
    json_object *first_part_obj = json_object_array_get_idx(part_array, 0);
    json_object *first_part_vars_array = json_object_path_get_array(first_part_obj, "Vars");

    /* Dataset transfer property list used in all H5Dwrite calls */
#if H5_HAVE_PARALLEL
    if (no_collective)
        H5Pset_dxpl_mpio(dxpl_id, H5FD_MPIO_INDEPENDENT);
    else
        H5Pset_dxpl_mpio(dxpl_id, H5FD_MPIO_COLLECTIVE);
#endif


    /* Loop over vars and then over parts */
    /* currently assumes all vars exist on all ranks. but not all parts */
    for (v = -1; v < json_object_array_length(first_part_vars_array); v++) /* -1 start is for Mesh */
    {

#warning SKIPPING MESH
        if (v == -1) continue; /* All ranks skip mesh (coords) for now */

        /* Inspect the first part's var object for name, datatype, etc. */
        json_object *var_obj = json_object_array_get_idx(first_part_vars_array, v);
        char const *varName = json_object_path_get_string(var_obj, "name");
        char *centering = strdup(json_object_path_get_string(var_obj, "centering"));
        json_object *dataobj = json_object_path_get_extarr(var_obj, "data");
#warning JUST ASSUMING TWO TYPES NOW. CHANGE TO A FUNCTION
        hid_t dtype_id = json_object_extarr_type(dataobj)==json_extarr_type_flt64? 
                H5T_NATIVE_DOUBLE:H5T_NATIVE_INT;
        hid_t fspace_id = H5Scopy(strcmp(centering, "zone") ? fspace_nodal_id : fspace_zonal_id);
        hid_t dcpl_id = make_dcpl(compression_alg_str, compression_params_str, fspace_id, dtype_id);

        /* Create the file dataset (using old-style H5Dcreate API here) */
#warning USING DEFAULT DCPL: LATER ADD COMPRESSION, ETC.
        
        hid_t ds_id = H5Dcreate1(h5file_id, varName, dtype_id, fspace_id, dcpl_id); 
        H5Sclose(fspace_id);
        H5Pclose(dcpl_id);

        /* Loop to make write calls for this var for each part on this rank */
#warning USE NEW MULTI-DATASET API WHEN AVAILABLE TO AGLOMERATE ALL PARTS INTO ONE CALL
        use_part_count = (int) ceil(json_object_path_get_double(main_obj, "clargs/avg_num_parts"));
        for (p = 0; p < use_part_count; p++)
        {
            json_object *part_obj = json_object_array_get_idx(part_array, p);
            json_object *var_obj = 0;
            hid_t mspace_id = H5Scopy(null_space_id);
            void const *buf = 0;

            fspace_id = H5Scopy(null_space_id);

            /* this rank actually has something to contribute to the H5Dwrite call */
            if (part_obj)
            {
                int i;
                hsize_t starts[3], counts[3];
                json_object *vars_array = json_object_path_get_array(part_obj, "Vars");
                json_object *mesh_obj = json_object_path_get_object(part_obj, "Mesh");
                json_object *var_obj = json_object_array_get_idx(vars_array, v);
                json_object *extarr_obj = json_object_path_get_extarr(var_obj, "data");
                json_object *global_log_origin_array =
                    json_object_path_get_array(part_obj, "GlobalLogOrigin");
                json_object *global_log_indices_array =
                    json_object_path_get_array(part_obj, "GlobalLogIndices");
                json_object *mesh_dims_array = json_object_path_get_array(mesh_obj, "LogDims");
                for (i = 0; i < ndims; i++)
                {
                    starts[ndims-1-i] =
                        json_object_get_int(json_object_array_get_idx(global_log_origin_array,i));
                    counts[ndims-1-i] =
                        json_object_get_int(json_object_array_get_idx(mesh_dims_array,i));
                    if (!strcmp(centering, "zone"))
                    {
                        counts[ndims-1-i]--;
                        starts[ndims-1-i] -=
                            json_object_get_int(json_object_array_get_idx(global_log_indices_array,i));
                    }
                }

                /* set selection of filespace */
                fspace_id = H5Dget_space(ds_id);
                H5Sselect_hyperslab(fspace_id, H5S_SELECT_SET, starts, 0, counts, 0);

                /* set dataspace of data in memory */
                mspace_id = H5Screate_simple(ndims, counts, 0);
                buf = json_object_extarr_data(extarr_obj);
            }

            H5Dwrite(ds_id, dtype_id, mspace_id, fspace_id, dxpl_id, buf);
            H5Sclose(fspace_id);
            H5Sclose(mspace_id);

        }

        H5Dclose(ds_id);
        free(centering);
    }

    H5Sclose(fspace_nodal_id);
    H5Sclose(fspace_zonal_id);
    H5Sclose(null_space_id);
    H5Pclose(dxpl_id);
    H5Pclose(fapl_id);
    H5Fclose(h5file_id);

#endif
}

typedef struct _user_data {
    hid_t groupId;
} user_data_t;

static void *CreateHDF5File(const char *fname, const char *nsname, void *userData)
{
    hid_t *retval = 0;
    hid_t h5File = H5Fcreate(fname, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    if (h5File >= 0)
    {
#warning USE NEWER GROUP CREATION SETTINGS OF HDF5
        if (nsname && userData)
        {
            user_data_t *ud = (user_data_t *) userData;
            ud->groupId = H5Gcreate1(h5File, nsname, 0);
        }
        retval = (hid_t *) malloc(sizeof(hid_t));
        *retval = h5File;
    }
    return (void *) retval;
}

static void *OpenHDF5File(const char *fname, const char *nsname,
                   MACSIO_MIF_ioFlags_t ioFlags, void *userData)
{
    hid_t *retval;
    hid_t h5File = H5Fopen(fname, ioFlags.do_wr ? H5F_ACC_RDWR : H5F_ACC_RDONLY, H5P_DEFAULT);
    if (h5File >= 0)
    {
        if (ioFlags.do_wr && nsname && userData)
        {
            user_data_t *ud = (user_data_t *) userData;
            ud->groupId = H5Gcreate1(h5File, nsname, 0);
        }
        retval = (hid_t *) malloc(sizeof(hid_t));
        *retval = h5File;
    }
    return (void *) retval;
}

static void CloseHDF5File(void *file, void *userData)
{
    if (userData)
    {
        user_data_t *ud = (user_data_t *) userData;
        H5Gclose(ud->groupId);
    }
    H5Fclose(*((hid_t*) file));
    free(file);
}

static void write_mesh_part(hid_t h5loc, json_object *part_obj)
{
#warning WERE SKPPING THE MESH (COORDS) OBJECT PRESENTLY
    int i;
    json_object *vars_array = json_object_path_get_array(part_obj, "Vars");

    for (i = 0; i < json_object_array_length(vars_array); i++)
    {
        int j;
        hsize_t var_dims[3];
        hid_t fspace_id, ds_id, dcpl_id;
        json_object *var_obj = json_object_array_get_idx(vars_array, i);
        json_object *data_obj = json_object_path_get_extarr(var_obj, "data");
        char const *varname = json_object_path_get_string(var_obj, "name");
        int ndims = json_object_extarr_ndims(data_obj);
        void const *buf = json_object_extarr_data(data_obj);
        hid_t dtype_id = json_object_extarr_type(data_obj)==json_extarr_type_flt64? 
                H5T_NATIVE_DOUBLE:H5T_NATIVE_INT;

        for (j = 0; j < ndims; j++)
            var_dims[j] = json_object_extarr_dim(data_obj, j);

        fspace_id = H5Screate_simple(ndims, var_dims, 0);
        dcpl_id = make_dcpl(compression_alg_str, compression_params_str, fspace_id, dtype_id);
        ds_id = H5Dcreate1(h5loc, varname, dtype_id, fspace_id, dcpl_id); 
        H5Dwrite(ds_id, dtype_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, buf);
        H5Dclose(ds_id);
        H5Pclose(dcpl_id);
        H5Sclose(fspace_id);
    }
}

static void main_dump_mif(json_object *main_obj, int numFiles, int dumpn, double dumpt)
{
    int size, rank;
    hid_t *h5File_ptr;
    hid_t h5File;
    hid_t h5Group;
    char fileName[256];
    int i, len;
    int *theData;
    user_data_t userData;
    MACSIO_MIF_ioFlags_t ioFlags = {MACSIO_MIF_WRITE,
        JsonGetInt(main_obj, "clargs/exercise_scr")&0x1};

#warning MAKE WHOLE FILE USE HDF5 1.8 INTERFACE
#warning SET FILE AND DATASET PROPERTIES
#warning DIFFERENT MPI TAGS FOR DIFFERENT PLUGINS AND CONTEXTS
    MACSIO_MIF_baton_t *bat = MACSIO_MIF_Init(numFiles, ioFlags, MACSIO_MAIN_Comm, 3,
        CreateHDF5File, OpenHDF5File, CloseHDF5File, &userData);

    rank = json_object_path_get_int(main_obj, "parallel/mpi_rank");
    size = json_object_path_get_int(main_obj, "parallel/mpi_size");

    /* Construct name for the silo file */
    sprintf(fileName, "%s_hdf5_%05d_%03d.%s",
        json_object_path_get_string(main_obj, "clargs/filebase"),
        MACSIO_MIF_RankOfGroup(bat, rank),
        dumpn,
        json_object_path_get_string(main_obj, "clargs/fileext"));

    h5File_ptr = (hid_t *) MACSIO_MIF_WaitForBaton(bat, fileName, 0);
    h5File = *h5File_ptr;
    h5Group = userData.groupId;

    json_object *parts = json_object_path_get_array(main_obj, "problem/parts");

    for (int i = 0; i < json_object_array_length(parts); i++)
    {
        char domain_dir[256];
        json_object *this_part = json_object_array_get_idx(parts, i);
        hid_t domain_group_id;

        snprintf(domain_dir, sizeof(domain_dir), "domain_%07d",
            json_object_path_get_int(this_part, "Mesh/ChunkID"));
 
        domain_group_id = H5Gcreate1(h5File, domain_dir, 0);

        write_mesh_part(domain_group_id, this_part);

        H5Gclose(domain_group_id);
    }

    /* If this is the 'root' processor, also write Silo's multi-XXX objects */
#if 0
    if (rank == 0)
        WriteMultiXXXObjects(main_obj, siloFile, bat);
#endif

    /* Hand off the baton to the next processor. This winds up closing
     * the file so that the next processor that opens it can be assured
     * of getting a consistent and up to date view of the file's contents. */
    MACSIO_MIF_HandOffBaton(bat, h5File_ptr);

    /* We're done using MACSIO_MIF, so finish it off */
    MACSIO_MIF_Finish(bat);

}

static void main_dump(int argi, int argc, char **argv, json_object *main_obj,
    int dumpn, double dumpt)
{
    int rank, size, numFiles;

#warning SET ERROR MODE OF HDF5 LIBRARY

    /* Without this barrier, I get strange behavior with Silo's MACSIO_MIF interface */
    mpi_errno = MPI_Barrier(MACSIO_MAIN_Comm);

    /* process cl args */
    process_args(argi, argc, argv);

    rank = json_object_path_get_int(main_obj, "parallel/mpi_rank");
    size = json_object_path_get_int(main_obj, "parallel/mpi_size");

#warning MOVE TO A FUNCTION
    /* ensure we're in MIF mode and determine the file count */
    json_object *parfmode_obj = json_object_path_get_array(main_obj, "clargs/parallel_file_mode");
    if (parfmode_obj)
    {
        json_object *modestr = json_object_array_get_idx(parfmode_obj, 0);
        json_object *filecnt = json_object_array_get_idx(parfmode_obj, 1);
#warning ERRORS NEED TO GO TO LOG FILES AND ERROR BEHAVIOR NEEDS TO BE HONORED
        if (!strcmp(json_object_get_string(modestr), "SIF"))
        {
            main_dump_sif(main_obj, dumpn, dumpt);
        }
        else
        {
            numFiles = json_object_get_int(filecnt);
            main_dump_mif(main_obj, numFiles, dumpn, dumpt);
        }
    }
    else
    {
        char const * modestr = json_object_path_get_string(main_obj, "clargs/parallel_file_mode");
        if (!strcmp(modestr, "SIF"))
        {
            float avg_num_parts = json_object_path_get_double(main_obj, "clargs/avg_num_parts");
            if (avg_num_parts == (float ((int) avg_num_parts)))
                main_dump_sif(main_obj, dumpn, dumpt);
            else
            {
#warning CURRENTLY, SIF CAN WORK ONLY ON WHOLE PART COUNTS
                MACSIO_LOG_MSG(Die, ("HDF5 plugin cannot currently handle SIF mode where "
                    "there are different numbers of parts on each MPI rank. "
                    "Set --avg_num_parts to an integral value." ));
            }
        }
        else if (!strcmp(modestr, "MIFMAX"))
            numFiles = json_object_path_get_int(main_obj, "parallel/mpi_size");
        else if (!strcmp(modestr, "MIFAUTO"))
        {
            /* Call utility to determine optimal file count */
#warning ADD UTILIT TO DETERMINE OPTIMAL FILE COUNT
        }
        main_dump_mif(main_obj, numFiles, dumpn, dumpt);
    }
}

static int register_this_interface()
{
    MACSIO_IFACE_Handle_t iface;

    if (strlen(iface_name) >= MACSIO_IFACE_MAX_NAME)
        MACSIO_LOG_MSG(Die, ("Interface name \"%s\" too long", iface_name));

#warning DO HDF5 LIB WIDE (DEFAULT) INITITILIAZATIONS HERE

    /* Populate information about this plugin */
    strcpy(iface.name, iface_name);
    strcpy(iface.ext, iface_ext);
    iface.dumpFunc = main_dump;
    iface.processArgsFunc = process_args;

    /* Register custom compression methods with HDF5 library */
    H5dont_atexit();
#ifdef HAVE_ZFP
    H5Z_register_zfp();
#endif

    /* Register this plugin */
    if (!MACSIO_IFACE_Register(&iface))
        MACSIO_LOG_MSG(Die, ("Failed to register interface \"%s\"", iface_name));

    return 0;
}

/* this one statement is the only statement requiring compilation by
   a C++ compiler. That is because it involves initialization and non
   constant expressions (a function call in this case). This function
   call is guaranteed to occur during *initialization* (that is before
   even 'main' is called) and so will have the effect of populating the
   iface_map array merely by virtue of the fact that this code is linked
   with a main. */
static int dummy = register_this_interface();

/*!@}*/

/*!@}*/
