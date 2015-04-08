#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>

#include <ifacemap.h>
#include <macsio_clargs.h>
#include <macsio_log.h>
#include <macsio_main.h>
#include <macsio_mif.h>
#include <macsio_params.h>
#include <macsio_utils.h>
#include <options.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

#ifdef HAVE_SILO
#include <silo.h> /* for the Silo block based VFD option */
#endif

#include <H5pubconf.h>
#include <hdf5.h>

/* convenient name mapping macors */
#define FHNDL FHNDL2(hdf5)
#define FNAME2(FUNC,A) FUNC ## _ ## A
#define FNAME(FUNC) FNAME2(FUNC,hdf5)
#define INAME2(A) #A
#define INAME INAME2(hdf5)

/* the name you want to assign to the interface */
static char const *iface_name = INAME;
static char const *iface_ext = "h5";

static int use_log = 0;
static int silo_block_size = 0;
static int silo_block_count = 0;
static int sbuf_size = -1;
static int mbuf_size = -1;
static int rbuf_size = -1;
static int lbuf_size = 0;
static const char *filename;
static hid_t fid;
static hid_t dspc = -1;
static MACSIO_LOG_LogHandle_t *log;
static int show_errors = 0;

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

    if (h5status < 0)
    {
        if (fapl_id >= 0)
            H5Pclose(fapl_id);
        return 0;
    }

    return fapl_id;
}

static MACSIO_optlist_t *FNAME(process_args)(int argi, int argc, char *argv[])
{
    const MACSIO_CLARGS_ArgvFlags_t argFlags = {MACSIO_CLARGS_WARN, MACSIO_CLARGS_TOMEM};
    MACSIO_CLARGS_ProcessCmdline(0, argFlags, argi, argc, argv,
        "--show_errors",
            "Show low-level HDF5 errors",
            &show_errors,
        "--sieve_buf_size %d",
            "Specify sieve buffer size (see H5Pset_sieve_buf_size)",
            &sbuf_size,
        "--meta_block_size %d",
            "Specify size of meta data blocks (see H5Pset_meta_block_size)",
            &mbuf_size,
        "--small_block_size %d",
            "Specify threshold size for data blocks considered to be 'small' (see H5Pset_small_data_block_size)",
            &rbuf_size,
        "--log",
            "Use logging Virtual File Driver (see H5Pset_fapl_log)",
            &use_log,
#ifdef HAVE_SILO
        "--silo_fapl %d %d",
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
    char const *mesh_type = json_object_path_get_string(main_obj, "clargs/--part_type");
    char fileName[256];

    hid_t h5file_id;
    hid_t fapl_id = H5Pcreate(H5P_FILE_ACCESS);
    hid_t dxpl_id = H5Pcreate(H5P_DATASET_XFER);
    hid_t null_space_id = H5Screate(H5S_NULL);
    hid_t fspace_nodal_id, fspace_zonal_id;
    hsize_t global_log_dims_nodal[3];
    hsize_t global_log_dims_zonal[3];

    MPI_Info mpiInfo;

    MPI_Info_create(&mpiInfo);

#warning INCLUDE ARGS FOR ISTORE AND K_SYM
#warning INCLUDE ARG PROCESS FOR HINTS
#warning FAPL PROPS: ALIGNMENT 
#if H5_HAVE_PARALLEL
    H5Pset_fapl_mpio(fapl_id, MACSIO_MAIN_Comm, mpiInfo);
#endif

#warning FOR MIF, NEED A FILEROOT ARGUMENT OR CHANGE TO FILEFMT ARGUMENT
    /* Construct name for the HDF5 file */
    sprintf(fileName, "%s_hdf5_%03d.%s",
        json_object_path_get_string(main_obj, "clargs/--filebase"),
        dumpn,
        json_object_path_get_string(main_obj, "clargs/--fileext"));

    h5file_id = H5Fcreate(fileName, H5F_ACC_TRUNC, H5P_DEFAULT, fapl_id);

    /* Create an HDF5 Dataspace for the global whole of mesh and var objects in the file. */
    ndims = json_object_path_get_int(main_obj, "clargs/--part_dim");
    json_object *global_log_dims_array =
        json_object_path_get_array(main_obj, "problem/global/LogDims");
    for (i = 0; i < ndims; i++)
    {
        global_log_dims_nodal[i] = (hsize_t) json_object_get_int(
            json_object_array_get_idx(global_log_dims_array, i));
        global_log_dims_zonal[i] = global_log_dims_nodal[i] - 1;
    }
    fspace_nodal_id = H5Screate_simple(ndims, global_log_dims_nodal, 0);
    fspace_zonal_id = H5Screate_simple(ndims, global_log_dims_zonal, 0);

    /* Get the list of vars on the first part as a guide to loop over vars */
    json_object *part_array = json_object_path_get_array(main_obj, "problem/parts");
    json_object *first_part_obj = json_object_array_get_idx(part_array, 0);
    json_object *first_part_vars_array = json_object_path_get_array(first_part_obj, "Vars");

#warning XFER PL: independent, collective
    /* Used in all H5Dwrite calls */
#if H5_HAVE_PARALLEL
    H5Pset_dxpl_mpio(dxpl_id, H5FD_MPIO_COLLECTIVE);
#endif

    /* Loop over vars and then over parts */
#warning CURRENTLY ASSUMES ALL VARS EXIST ON ALL RANKS. BUT NOT ALL PARTS
    for (v = -1; v < json_object_array_length(first_part_vars_array); v++) /* -1 start is for Mesh */
    {

#warning SKIPPING MESH
        if (v == -1) continue; /* All ranks skip mesh (coords) for now */

        /* Inspect the first part's var object for name, datatype, etc. */
        json_object *var_obj = json_object_array_get_idx(first_part_vars_array, v);
        char const *varName = json_object_path_get_string(var_obj, "name");
        char const *centering = json_object_path_get_string(var_obj, "centering");
        json_object *dataobj = json_object_path_get_extarr(var_obj, "data");
#warning JUST ASSUMING TWO TYPES NOW. CHANGE TO A FUNCTION
        hid_t dtype_id = json_object_extarr_type(dataobj)==json_extarr_type_flt64? 
                H5T_NATIVE_DOUBLE:H5T_NATIVE_INT;
        hid_t fspace_id = strcmp(centering, "zone") ? fspace_nodal_id : fspace_zonal_id;

        /* Create the file dataset (using old-style H5Dcreate API here) */
#warning USING DEFAULT DCPL: LATER ADD COMPRESSION, ETC.
        hid_t ds_id = H5Dcreate1(h5file_id, varName, dtype_id, fspace_id, H5P_DEFAULT); 

        /* Loop to make write calls for this var for each part on this rank */
#warning USE NEW MULTI-DATASET API WHEN AVAILABLE TO AGLOMERATE ALL PARTS INTO ONE CALL
        for (p = 0; p < json_object_array_length(part_array); p++)
        {
            json_object *part_obj = json_object_array_get_idx(part_array, p);
            json_object *var_obj = 0;
            hid_t mspace_id = null_space_id;
            void const *buf = 0;

            /* reset file space selection to nothing */
            H5Sselect_none(fspace_id);

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
                json_object *mesh_dims_array = json_object_path_get_array(mesh_obj, "LogDims");
                for (i = 0; i < ndims; i++)
                {
                    starts[i] = json_object_get_int(json_object_array_get_idx(global_log_origin_array,i));
                    counts[i] = json_object_get_int(json_object_array_get_idx(mesh_dims_array,i));
                    if (!strcmp(centering, "zone"))
                        counts[i]--;
                }

                /* set selection of filespace */
                H5Sselect_hyperslab(fspace_id, H5S_SELECT_SET, starts, 0, counts, 0);

                /* set dataspace of data in memory */
                mspace_id = H5S_ALL;
                buf = json_object_extarr_data(extarr_obj);
            }

            H5Dwrite(ds_id, dtype_id, mspace_id, fspace_id, dxpl_id, buf);

        }

        H5Dclose(ds_id);
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
                   MACSIO_MIF_iomode_t ioMode, void *userData)
{
    hid_t *retval;
    hid_t h5File = H5Fopen(fname, ioMode == MACSIO_MIF_WRITE ? H5F_ACC_RDWR : H5F_ACC_RDONLY, H5P_DEFAULT);
    if (h5File >= 0)
    {
        if (ioMode == MACSIO_MIF_WRITE && nsname && userData)
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
        hid_t fspace_id, ds_id;
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
        ds_id = H5Dcreate1(h5loc, varname, dtype_id, fspace_id, H5P_DEFAULT); 
        H5Dwrite(ds_id, dtype_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, buf);
        H5Dclose(ds_id);
        H5Sclose(fspace_id);
    }
}

static void main_dump_mif(json_object *main_obj, int numFiles, int dumpn, double dumpt)
{
    int size, rank;
    int numaroups = 3;
    hid_t *h5File_ptr;
    hid_t h5File;
    hid_t h5Group;
    char fileName[256];
    int i, len;
    int *theData;
    user_data_t userData;

#warning MAKE WHOLE FILE USE HDF5 1.8 INTERFACE
#warning SET FILE AND DATASET PROPERTIES
#warning DIFFERENT MPI TAGS FOR DIFFERENT PLUGINS AND CONTEXTS
    MACSIO_MIF_baton_t *bat = MACSIO_MIF_Init(numFiles, MACSIO_MIF_WRITE, MACSIO_MAIN_Comm, 3,
        CreateHDF5File, OpenHDF5File, CloseHDF5File, &userData);

    rank = json_object_path_get_int(main_obj, "parallel/mpi_rank");
    size = json_object_path_get_int(main_obj, "parallel/mpi_size");

    /* Construct name for the silo file */
    sprintf(fileName, "%s_hdf5_%05d_%03d.%s",
        json_object_path_get_string(main_obj, "clargs/--filebase"),
        MACSIO_MIF_RankOfGroup(bat, rank),
        dumpn,
        json_object_path_get_string(main_obj, "clargs/--fileext"));

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

static void FNAME(main_dump)(int argi, int argc, char **argv, json_object *main_obj,
    int dumpn, double dumpt)
{
    int rank, size, numFiles;

#warning SET ERROR MODE OF HDF5 LIBRARY

#warning MAKE PLUGIN LOGGING A CL OPTION
#if 0
    log = Log_Init(MACSIO_MAIN_Comm, "macsio_hdf5.log", 128, 20);
#endif

    /* Without this barrier, I get strange behavior with Silo's MACSIO_MIF interface */
    mpi_errno = MPI_Barrier(MACSIO_MAIN_Comm);

    /* process cl args */
    FNAME(process_args)(argi, argc, argv);

    rank = json_object_path_get_int(main_obj, "parallel/mpi_rank");
    size = json_object_path_get_int(main_obj, "parallel/mpi_size");

#warning MOVE TO A FUNCTION
    /* ensure we're in MIF mode and determine the file count */
    json_object *parfmode_obj = json_object_path_get_array(main_obj, "clargs/--parallel_file_mode");
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
        char const * modestr = json_object_path_get_string(main_obj, "clargs/--parallel_file_mode");
        if (!strcmp(modestr, "SIF"))
        {
            float avg_num_parts = json_object_path_get_double(main_obj, "clargs/--avg_num_parts");
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

#if 0
    Log_Finalize(log);
#endif

}

static int register_this_interface()
{
    unsigned int id = MACSIO_UTILS_BJHash((unsigned char*)iface_name, strlen(iface_name), 0) % MACSIO_MAX_IFACES;
    if (strlen(iface_name) >= MACSIO_MAX_IFACE_NAME)
        MACSIO_LOG_MSG(Die, ("interface name \"%s\" too long",iface_name));
    if (iface_map[id].slotUsed!= 0)
        MACSIO_LOG_MSG(Die, ("hash collision for interface name \"%s\"",iface_name));

#warning DO HDF5 LIB WIDE (DEFAULT) INITITILIAZATIONS HERE
#warning ADD LINDSTROM COMPRESSION STUFF

    /* Take this slot in the map */
    iface_map[id].slotUsed = 1;
    strcpy(iface_map[id].name, iface_name);
    strcpy(iface_map[id].ext, iface_ext);

    /* Must define at least these two methods */
    iface_map[id].dumpFunc = FNAME(main_dump);
    iface_map[id].processArgsFunc = FNAME(process_args);

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
