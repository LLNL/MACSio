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

// #ifdef HAVE_SILO
// #include <silo.h> /* for the Silo block based VFD option */
// #endif

#include <H5pubconf.h>
#include <hdf5.h>
#include <H5Tpublic.h>

/*! \brief H5Z-ZFP generic interface for setting rate mode */
#define H5Pset_zfp_rate_cdata(R, N, CD)          \
do { if (N>=4) {double *p = (double *) &CD[2];   \
CD[0]=CD[1]=CD[2]=CD[3]=0;                       \
CD[0]=1; *p=R; N=4;}} while(0)

/*! \brief H5Z-ZFP generic interface for setting precision mode */
#define H5Pset_zfp_precision_cdata(P, N, CD)  \
do { if (N>=3) {CD[0]=CD[1]=CD[2];            \
CD[0]=2;                 \
CD[2]=P; N=3;}} while(0)

/*! \brief H5Z-ZFP generic interface for setting accuracy mode */
#define H5Pset_zfp_accuracy_cdata(A, N, CD)      \
do { if (N>=4) {double *p = (double *) &CD[2];   \
CD[0]=CD[1]=CD[2]=CD[3]=0;                       \
CD[0]=3; *p=A; N=4;}} while(0)

/*! \brief H5Z-ZFP generic interface for setting expert mode */
#define H5Pset_zfp_expert_cdata(MiB, MaB, MaP, MiE, N, CD) \
do { if (N>=6) { CD[0]=CD[1]=CD[2]=CD[3]=CD[4]=CD[5]=0;    \
CD[0]=4;                                 \
CD[2]=MiB; CD[3]=MaB; CD[4]=MaP;                           \
CD[5]=(unsigned int)MiE; N=6;}} while(0)

/*!
\addtogroup plugins
@{
*/

/*!
\defgroup MACSIO_PLUGIN_HDF5 MACSIO_PLUGIN_HDF5
@{
*/

/*! \brief name of this plugin */
static char const *iface_name = "hdf5";

/*! \brief file extension for files managed by this plugin */
static char const *iface_ext = "h5";

static int use_log = 0; /**< Use HDF5's logging fapl */
static int no_collective = 0; /**< Use HDF5 independent (e.g. not collective) I/O */
static int no_single_chunk = 0; /**< disable single chunking */
static int silo_block_size = 0; /**< block size for silo block-based VFD */
static int silo_block_count = 0; /**< block count for silo block-based VFD */
static int sbuf_size = -1; /**< HDF5 library sieve buf size */
static int mbuf_size = -1; /**< HDF5 library meta blocck size */
static int rbuf_size = -1; /**< HDF5 library small data block size */
static int lbuf_size = 0;  /**< HDF5 library log flags */
static const char *filename;
static hid_t fid;
static hid_t dspc = -1;
static int show_errors = 0;
static char compression_alg_str[64];
static char compression_params_str[512];

/*! \brief create HDF5 library file access property list */
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

/*!
\brief Utility to parse compression string command-line args

Does a case-insensitive search of \c src_str for \c token_to_match
not including the trailing format specifier. Upon finding a match, 
performs a scanf at the location of the match into temporary 
memory confirming the scan will actually succeed. Then, performs
the scanf again, storying the result to the memory indicated in
\c val_ptr.

\returns 0 on error, 1 on success
*/
static int
get_tokval(
    char const *src_str, /**< CL arg string to be parsed */
    char const *token_to_match, /**< a token in the string to be matched including a trailing scanf format specifier */
    void *val_ptr /**< Pointer to memory where parsed value should be placed */
)
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

/*!
\brief create HDF5 library dataset creation property list

If the dataset size is below the \c minsize threshold, no special
storage layout or compression action is taken.

Chunking is initially set to \em single-chunk. However, for szip
compressor, chunking can be set by command-line arguments.

*/
static hid_t
make_dcpl(
    char const *alg_str, /**< compression algorithm string */
    char const *params_str, /**< compression params string */
    hid_t space_id, /**< HDF5 dataspace id for the dataset */
    hid_t dtype_id /**< HDF5 datatype id for the dataset */
)
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
    else if (!strncasecmp(alg_str, "zfp", 3))
    {
        unsigned int cd_values[10];
        int cd_nelmts = 10;

        /* Setup ZFP filter and add to HDF5 pipeline using generic interface. */
        if (zfp_rate != -1)
            H5Pset_zfp_rate_cdata(zfp_rate, cd_nelmts, cd_values);
        else if (zfp_precision != -1)
            H5Pset_zfp_precision_cdata(zfp_precision, cd_nelmts, cd_values);
        else if (zfp_accuracy != -1)
            H5Pset_zfp_accuracy_cdata(zfp_accuracy, cd_nelmts, cd_values);
        else
            H5Pset_zfp_rate_cdata(0.0, cd_nelmts, cd_values); /* to get ZFP library defaults */

        /* Add filter to the pipeline via generic interface */
        if (H5Pset_filter(retval, 32013, H5Z_FLAG_MANDATORY, cd_nelmts, cd_values) < 0)
            MACSIO_LOG_MSG(Warn, ("Unable to set up H5Z-ZFP compressor"));
    }
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

/*!
\brief Process command-line arguments an set local variables */
static int
process_args(
    int argi, /**< argument index to start processing \c argv */
    int argc, /**< \c argc from main */
    char *argv[] /**< \c argv from main */
)
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
            "individual algorithm-specific parameters for those algorithms available\n"
            "in the current build.\n"
            "\n"
            "minsize=%d : min. size of dataset (in terms of a count of values)\n"
            "    upon which compression will even be attempted. Default is 1024.\n"
            "shuffle=<int>: Boolean (zero or non-zero) to indicate whether to use\n"
            "    HDF5's byte shuffling filter *prior* to compression. Default depends\n"
            "    on algorithm. By default, shuffling is NOT used for zfp but IS\n"
            "    used with all other algorithms.\n"
            "\n"
            "Available compression algorithms...\n"
            "\n"
            "\"zfp\"\n"
            "    Use Peter Lindstrom's ZFP compression (computation.llnl.gov/casc/zfp)\n"
            "    Note: Whether this compression is available is determined entirely at\n"
            "    run-time using the H5Z-ZFP compresser as a generic filter. This means\n"
            "    all that is necessary is to specify the HDF5_PLUGIN_PATH environnment\n" 
            "    variable with a path to the shared lib for the filter.\n"
            "    The following ZFP options are *mutually*exclusive*. In any command-line\n"
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
#ifdef HAVE_SZIP
            "\"szip\"\n"
            "    method=%s : specify 'ec' for entropy coding or 'nn' for nearest\n"
            "        neighbor. Default is 'nn'\n"
            "    block=%d : (pixels-per-block) must be an even integer <= 32. See\n"
            "        See H5Pset_szip in HDF5 documentation for more information.\n"
            "        Default is 32.\n"
            "    chunk=%d:%d : colon-separated dimensions specifying chunk size in\n"
            "        each dimension higher than the first (fastest varying) dimension.\n"
            "\n"
#endif
            "\"gzip\"\n"
            "    level=%d : A value in the range [1,9], inclusive, trading off time to\n"
            "        compress with amount of compression. Level=1 results in best speed\n"
            "        but worst compression whereas level=9 results in best compression\n"
            "        but worst speed. Values outside [1,9] are clamped. Default is 9.\n"
            "\n"
            "Examples:\n"
            "    --compression zfp rate=18.5\n"
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

/*! \brief Single shared file implementation of main dump */
static void
main_dump_sif(
    json_object *main_obj, /**< main json data object to dump */
    int dumpn, /**< dump number (like a cycle number) */
    double dumpt /**< dump time */
)
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

//#warning WE ARE DOING SIF SLIGHTLY WRONG, DUPLICATING SHARED NODES
//#warning INCLUDE ARGS FOR ISTORE AND K_SYM
//#warning INCLUDE ARG PROCESS FOR HINTS
//#warning FAPL PROPS: ALIGNMENT 
#if H5_HAVE_PARALLEL
    H5Pset_fapl_mpio(fapl_id, MACSIO_MAIN_Comm, mpiInfo);
#endif

//#warning FOR MIF, NEED A FILEROOT ARGUMENT OR CHANGE TO FILEFMT ARGUMENT
    /* Construct name for the HDF5 file */
    sprintf(fileName, "%s_hdf5_%03d.%s",
        json_object_path_get_string(main_obj, "clargs/filebase"),
        dumpn,
        json_object_path_get_string(main_obj, "clargs/fileext"));

    MACSIO_UTILS_RecordOutputFiles(dumpn, fileName);
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

//#warning SKIPPING MESH
        if (v == -1) continue; /* All ranks skip mesh (coords) for now */

        /* Inspect the first part's var object for name, datatype, etc. */
        json_object *var_obj = json_object_array_get_idx(first_part_vars_array, v);
        char const *varName = json_object_path_get_string(var_obj, "name");
        char *centering = strdup(json_object_path_get_string(var_obj, "centering"));
        json_object *dataobj = json_object_path_get_extarr(var_obj, "data");
//#warning JUST ASSUMING TWO TYPES NOW. CHANGE TO A FUNCTION
        hid_t dtype_id = json_object_extarr_type(dataobj)==json_extarr_type_flt64? 
                H5T_NATIVE_DOUBLE:H5T_NATIVE_INT;
        hid_t fspace_id = H5Scopy(strcmp(centering, "zone") ? fspace_nodal_id : fspace_zonal_id);
        hid_t dcpl_id = make_dcpl(compression_alg_str, compression_params_str, fspace_id, dtype_id);

        /* Create the file dataset (using old-style H5Dcreate API here) */
//#warning USING DEFAULT DCPL: LATER ADD COMPRESSION, ETC.
        
        hid_t ds_id = H5Dcreate1(h5file_id, varName, dtype_id, fspace_id, dcpl_id); 
        H5Sclose(fspace_id);
        H5Pclose(dcpl_id);

        /* Loop to make write calls for this var for each part on this rank */
//#warning USE NEW MULTI-DATASET API WHEN AVAILABLE TO AGLOMERATE ALL PARTS INTO ONE CALL
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

/*! \brief User data for MIF callbacks */
typedef struct _user_data {
    hid_t groupId; /**< HDF5 hid_t of current group */
} user_data_t;

/*! \brief MIF create file callback for HDF5 MIF mode */
static void *
CreateHDF5File(
    const char *fname, /**< file name */
    const char *nsname, /**< curent task namespace name */
    void *userData /**< user data specific to current task */
)
{
    hid_t *retval = 0;
    hid_t h5File;
    hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
    H5Pset_fclose_degree(fapl, H5F_CLOSE_SEMI);
    h5File = H5Fcreate(fname, H5F_ACC_TRUNC, H5P_DEFAULT, fapl);
    H5Pclose(fapl);
    if (h5File >= 0)
    {
//#warning USE NEWER GROUP CREATION SETTINGS OF HDF5
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

/*! \brief MIF Open file callback for HFD5 plugin MIF mode */
static void *
OpenHDF5File(
    const char *fname, /**< filename */
    const char *nsname, /**< namespace name for current task */
    MACSIO_MIF_ioFlags_t ioFlags, /* io flags */
    void *userData /**< task specific user data for current task */
) 
{
    hid_t *retval;
    hid_t h5File;
    hid_t fapl = H5Pcreate(H5P_FILE_ACCESS);
    H5Pset_fclose_degree(fapl, H5F_CLOSE_SEMI);
    h5File = H5Fopen(fname, ioFlags.do_wr ? H5F_ACC_RDWR : H5F_ACC_RDONLY, fapl);
    H5Pclose(fapl);
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

/*! \brief MIF close file callback for HDF5 plugin MIF mode */
static int
CloseHDF5File( 
    void *file, /**< void* to hid_t of file to cose */
    void *userData /**< task specific user data */
)
{
    const unsigned int obj_flags = H5F_OBJ_LOCAL | H5F_OBJ_DATASET |
        H5F_OBJ_GROUP | H5F_OBJ_DATATYPE | H5F_OBJ_ATTR;
    int noo;
    herr_t close_retval;

    if (userData)
    {
        user_data_t *ud = (user_data_t *) userData;
        H5Gclose(ud->groupId);
    }

    /* Check for any open objects in this file */
    noo = H5Fget_obj_count(fid, obj_flags);
    close_retval = H5Fclose(*((hid_t*) file));
    free(file);

    if (noo > 0) return -1;
    return (int) close_retval;
}

/*! \brief Write individual mesh part in MIF mode */
static void
write_mesh_part(
    hid_t h5loc, /**< HDF5 group id into which to write */
    json_object *part_obj /**< JSON object for the mesh part to write */
)
{
//#warning WERE SKPPING THE MESH (COORDS) OBJECT PRESENTLY
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

/*! \brief Main dump output for HDF5 plugin MIF mode */
static void
main_dump_mif( 
   json_object *main_obj, /**< main data object to dump */
   int numFiles, /**< MIF file count */
   int dumpn, /**< dump number (like a cycle number) */
   double dumpt /**< dump time */
)
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

//#warning MAKE WHOLE FILE USE HDF5 1.8 INTERFACE
//#warning SET FILE AND DATASET PROPERTIES
//#warning DIFFERENT MPI TAGS FOR DIFFERENT PLUGINS AND CONTEXTS
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

    MACSIO_UTILS_RecordOutputFiles(dumpn, fileName);
    
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

/*!
\brief Main dump callback for HDF5 plugin

Selects between MIF and SSF output.
*/
static void
main_dump(
    int argi, /**< arg index at which to start processing \c argv */
    int argc, /**< \c argc from main */
    char **argv, /**< \c argv from main */
    json_object *main_obj, /**< main json data object to dump */
    int dumpn, /**< dump number */
    double dumpt /**< dump time */
)
{
    int rank, size, numFiles;

//#warning SET ERROR MODE OF HDF5 LIBRARY

    /* Without this barrier, I get strange behavior with Silo's MACSIO_MIF interface */
#ifdef HAVE_MPI
    mpi_errno = MPI_Barrier(MACSIO_MAIN_Comm);
#endif

    /* process cl args */
    process_args(argi, argc, argv);

    rank = json_object_path_get_int(main_obj, "parallel/mpi_rank");
    size = json_object_path_get_int(main_obj, "parallel/mpi_size");

//#warning MOVE TO A FUNCTION
    /* ensure we're in MIF mode and determine the file count */
    json_object *parfmode_obj = json_object_path_get_array(main_obj, "clargs/parallel_file_mode");
    if (parfmode_obj)
    {
        json_object *modestr = json_object_array_get_idx(parfmode_obj, 0);
        json_object *filecnt = json_object_array_get_idx(parfmode_obj, 1);
//#warning ERRORS NEED TO GO TO LOG FILES AND ERROR BEHAVIOR NEEDS TO BE HONORED
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
//#warning CURRENTLY, SIF CAN WORK ONLY ON WHOLE PART COUNTS
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
//#warning ADD UTILIT TO DETERMINE OPTIMAL FILE COUNT
        }
        main_dump_mif(main_obj, numFiles, dumpn, dumpt);
    }
}

/*! \brief Function called during static initialization to register the plugin */
static int
register_this_interface()
{
    MACSIO_IFACE_Handle_t iface;

    if (strlen(iface_name) >= MACSIO_IFACE_MAX_NAME)
        MACSIO_LOG_MSG(Die, ("Interface name \"%s\" too long", iface_name));

//#warning DO HDF5 LIB WIDE (DEFAULT) INITITILIAZATIONS HERE

    /* Populate information about this plugin */
    strcpy(iface.name, iface_name);
    strcpy(iface.ext, iface_ext);
    iface.dumpFunc = main_dump;
    iface.processArgsFunc = process_args;

    /* Register custom compression methods with HDF5 library */
    H5dont_atexit();

    /* Register this plugin */
    if (!MACSIO_IFACE_Register(&iface))
        MACSIO_LOG_MSG(Die, ("Failed to register interface \"%s\"", iface_name));

    return 0;
}

/*! \brief Static initializer statement to cause plugin registration at link time

this one statement is the only statement requiring compilation by
a C++ compiler. That is because it involves initialization and non
constant expressions (a function call in this case). This function
call is guaranteed to occur during *initialization* (that is before
even 'main' is called) and so will have the effect of populating the
iface_map array merely by virtue of the fact that this code is linked
with a main.
*/
static int dummy = register_this_interface();

/*!@}*/

/*!@}*/
