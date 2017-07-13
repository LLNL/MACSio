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

/* Disable debugging messages */

/*!
\addtogroup plugins
@{
*/

/*!
\addtogroup ADIOS
@{
*/

/* the name you want to assign to the interface */
static char const *iface_name = "adios";
static char const *iface_ext = "bp";

static int use_log = 0;
static int no_collective = 0;
static int no_single_chunk = 0;
static int sbuf_size = -1;
static int mbuf_size = -1;
static int rbuf_size = -1;
static int lbuf_size = 0;
static const char *filename;
static int64_t fd_p;
static int show_errors = 0;
static char compression_alg_str[64];
static char compression_params_str[512];

static int process_args(int argi, int argc, char *argv[])
{
    const MACSIO_CLARGS_ArgvFlags_t argFlags = {MACSIO_CLARGS_WARN, MACSIO_CLARGS_TOMEM};

    char *c_alg = compression_alg_str;
    char *c_params = compression_params_str;

    MACSIO_CLARGS_ProcessCmdline(0, argFlags, argi, argc, argv,
        MACSIO_CLARGS_END_OF_ARGS);

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

static void *CreateADIOSFile(const char *fname, const char *nsname, void *userData)
{
    int *retval = 0;
    int64_t adios_file;
    adios_open(&adios_file, nsname, fname, "w", MACSIO_MAIN_Comm);
        
    retval = (int64_t *) malloc(sizeof(int64_t));
    *retval = adios_file;

    return (void *) retval;
}

static void *OpenHDF5File(const char *fname, const char *nsname,
                   MACSIO_MIF_ioFlags_t ioFlags, void *userData)
{
    int *retval = 0;
    int64_t adios_file;
    adios_open(&adios_file, nsname, fname, "u", MACSIO_MAIN_Comm);
        
    retval = (int64_t *) malloc(sizeof(int64_t));
    *retval = adios_file;

    return (void *) retval;
}

static void CloseHDF5File(void *file, void *userData)
{
    adios_close(*((int64_t) file));
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

    /* Without this barrier, I get strange behavior with Silo's MACSIO_MIF interface */
    mpi_errno = MPI_Barrier(MACSIO_MAIN_Comm);

    /* process cl args */
    process_args(argi, argc, argv);

    rank = json_object_path_get_int(main_obj, "parallel/mpi_rank");
    size = json_object_path_get_int(main_obj, "parallel/mpi_size");

    adios_init_noxml(MACSIO_MAIN_Comm);

    /* ensure we're in MIF mode and determine the file count */
    json_object *parfmode_obj = json_object_path_get_array(main_obj, "clargs/parallel_file_mode");
    if (parfmode_obj){
        json_object *modestr = json_object_array_get_idx(parfmode_obj, 0);
        json_object *filecnt = json_object_array_get_idx(parfmode_obj, 1);
        if (!strcmp(json_object_get_string(modestr), "SIF")){
            main_dump_sif(main_obj, dumpn, dumpt);
        } else {
            numFiles = json_object_get_int(filecnt);
            main_dump_mif(main_obj, numFiles, dumpn, dumpt);
        }
    } else {
        char const * modestr = json_object_path_get_string(main_obj, "clargs/parallel_file_mode");
        if (!strcmp(modestr, "SIF")) {
            float avg_num_parts = json_object_path_get_double(main_obj, "clargs/avg_num_parts");
            if (avg_num_parts == (float ((int) avg_num_parts)))
                main_dump_sif(main_obj, dumpn, dumpt);
            else {
#warning CURRENTLY, SIF CAN WORK ONLY ON WHOLE PART COUNTS
                MACSIO_LOG_MSG(Die, ("ADIOS plugin cannot currently handle SIF mode where "
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

    /* Populate information about this plugin */
    strcpy(iface.name, iface_name);
    strcpy(iface.ext, iface_ext);
    iface.dumpFunc = main_dump;
    iface.processArgsFunc = process_args;

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
