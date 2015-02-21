#include <map>
#include <string>

#include <ifacemap.h>
#include <macsio_main.h>
#include <macsio_params.h>
#include <log.h>
#include <options.h>
#include <util.h>

#ifdef PARALLEL
#include <mpi.h>
#endif

#define H5_USE_16_API
#include <hdf5.h>

#ifdef HAVE_SILO
#include <silo.h>
#endif
#include <pmpio.h>

#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* convenient name mapping macors */
#define FHNDL2(A) MACSIO_FileHandle_ ## A ## _t
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
static MACSIO_LogHandle_t *log;

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
    const MACSIO_ArgvFlags_t argFlags = {MACSIO_WARN, MACSIO_ARGV_TOMEM};
    MACSIO_ProcessCommandLine(0, argFlags, argi, argc, argv,
        "--sieve-buf-size %d",
            "Specify sieve buffer size (see H5Pset_sieve_buf_size)",
            &sbuf_size,
        "--meta-block-size %d",
            "Specify size of meta data blocks (see H5Pset_meta_block_size)",
            &mbuf_size,
        "--small-block-size %d",
            "Specify threshold size for data blocks considered to be 'small' (see H5Pset_small_data_block_size)",
            &rbuf_size,
        "--log",
            "Use logging Virtual File Driver (see H5Pset_fapl_log)",
            &use_log,
#ifdef HAVE_SILO
        "--silo-fapl %d %d",
            "Use Silo's block-based VFD and specify block size and block count", 
            &silo_block_size, &silo_block_count,
#endif
           MACSIO_END_OF_ARGS);
    return 0;
}

static void main_dump_sif(json_object *main_obj, int dumpn, double dumpt)
{
#ifdef PARALLEL
    hid_t fapl_id = H5Pcreate(H5P_FILE_ACCESS);
    MPI_Info mpiInfo;
    char fileName[256];

#warning INCLUDE ARGS FOR ISTORE AND K_SYM
#warning INCLUDE ARG PROCESS FOR HINTS
    MPI_Info_create(&mpiInfo);

#if 0
    H5Pset_fapl_mpio(fapl_id, MPI_COMM_WORLD, mpiInfo);
#endif

    /* Construct name for the silo file */
    sprintf(fileName, "%s_hdf5.%s",
        json_object_path_get_string(main_obj, "clargs/--filebase"),
        json_object_path_get_string(main_obj, "clargs/--fileext"));

    hid_t h5File = H5Fcreate(fileName, H5F_ACC_TRUNC, H5P_DEFAULT, fapl_id);

    H5Fclose(h5File);

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
            ud->groupId = H5Gcreate(h5File, nsname, 0);
        }
        retval = (hid_t *) malloc(sizeof(hid_t));
        *retval = h5File;
    }
    return (void *) retval;
}

static void *OpenHDF5File(const char *fname, const char *nsname,
                   PMPIO_iomode_t ioMode, void *userData)
{
    hid_t *retval;
    hid_t h5File = H5Fopen(fname, ioMode == PMPIO_WRITE ? H5F_ACC_RDWR : H5F_ACC_RDONLY, H5P_DEFAULT);
    if (h5File >= 0)
    {
        if (ioMode == PMPIO_WRITE && nsname && userData)
        {
            user_data_t *ud = (user_data_t *) userData;
            ud->groupId = H5Gcreate(h5File, nsname, 0);
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

static void main_dump_mif(json_object *main_obj, int numFiles, int dumpn, double dumpt)
{
    int size, rank;
    int numGroups = 3;
    hid_t *h5File_ptr;
    hid_t h5File;
    hid_t h5Group;
    char fileName[256];
    int i, len;
    int *theData;
    user_data_t userData;

#warning DIFFERENT MPI TAGS FOR DIFFERENT PLUGINS AND CONTEXTS
    PMPIO_baton_t *bat = PMPIO_Init(numFiles, PMPIO_WRITE, MPI_COMM_WORLD, 3,
        CreateHDF5File, OpenHDF5File, CloseHDF5File, &userData);

    /* Construct name for the silo file */
    sprintf(fileName, "%s_hdf5_%05d.%s",
        json_object_path_get_string(main_obj, "clargs/--filebase"),
        PMPIO_GroupRank(bat, rank),
        json_object_path_get_string(main_obj, "clargs/--fileext"));

    h5File_ptr = (hid_t *) PMPIO_WaitForBaton(bat, fileName, 0);
    h5File = *h5File_ptr;
    h5Group = userData.groupId;

    json_object *parts = json_object_path_get_array(main_obj, "problem/parts");
    int numParts = json_object_array_length(parts);

    for (int i = 0; i < numParts; i++)
    {
        char domain_dir[256];
        json_object *this_part = json_object_array_get_idx(parts, i);

        snprintf(domain_dir, sizeof(domain_dir), "domain_%07d",
            json_object_path_get_int(this_part, "Mesh/ChunkID"));
 
#if 0
        DBMkDir(siloFile, domain_dir);
        DBSetDir(siloFile, domain_dir);

        write_mesh_part(h5File, this_part);

        DBSetDir(siloFile, "..");
#endif
    }

    /* If this is the 'root' processor, also write Silo's multi-XXX objects */
#if 0
    if (rank == 0)
        WriteMultiXXXObjects(main_obj, siloFile, bat);
#endif

    /* Hand off the baton to the next processor. This winds up closing
     * the file so that the next processor that opens it can be assured
     * of getting a consistent and up to date view of the file's contents. */
    PMPIO_HandOffBaton(bat, h5File_ptr);

    /* We're done using PMPIO, so finish it off */
    PMPIO_Finish(bat);

#if 0
    Log_Finalize(log);
#endif
}

static void FNAME(main_dump)(int argi, int argc, char **argv, json_object *main_obj,
    int dumpn, double dumpt)
{
    int rank, size, numFiles;

#warning SET ERROR MODE OF HDF5 LIBRARY

#warning MAKE LOGGING A CL OPTION
#if 0
    log = Log_Init(MPI_COMM_WORLD, "macsio_hdf5.log", 128, 20);
#endif

    /* Without this barrier, I get strange behavior with Silo's PMPIO interface */
    MPI_Barrier(MPI_COMM_WORLD);

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
        if (strcmp(json_object_get_string(modestr), "MIF"))
        {
            MACSIO_ERROR(("Ignoring non-standard MIF mode"), MACSIO_WARN);
        }
        numFiles = json_object_get_int(filecnt);

        main_dump_mif(main_obj, numFiles, dumpn, dumpt);
    }
    else
    {
        char const * modestr = json_object_path_get_string(main_obj, "clargs/--parallel_file_mode");
        if (!strcmp(modestr, "SIF"))
        {
            main_dump_sif(main_obj, dumpn, dumpt);
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
    unsigned int id = bjhash((unsigned char*)iface_name, strlen(iface_name), 0) % MAX_IFACES;
    if (strlen(iface_name) >= MAX_IFACE_NAME)
        MACSIO_ERROR(("interface name \"%s\" too long",iface_name) , MACSIO_FATAL);
    if (iface_map[id].slotUsed!= 0)
        MACSIO_ERROR(("hash collision for interface name \"%s\"",iface_name) , MACSIO_FATAL);

#warning DO HDF5 LIB WIDE (DEFAULT) INITITILIAZATIONS HERE

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
