#include <map>
#include <string>

#include <ifacemap.h>
#include <macsio.h>
#include <options.h>
#include <util.h>

#ifdef HAVE_PARALLEL
#include <mpi.h>
#endif

#define H5_USE_16_API
#include <hdf5.h>

#ifdef HAVE_SILO
#include <silo.h>
#endif

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

using std::map;
using std::string;

typedef struct UnsyncedArrayInfo_t
{
    char absname[MACSIO_MAX_ABSNAME];
    int type;
    int dims[4];
} UnsyncedArrayInfo_t;

typedef struct PendingArrayInfo_t
{
    char absname[MACSIO_MAX_ABSNAME];
    int starts[4];
    int counts[4];
    int strides[4];
    void **buf;
} PendingArrayInfo_t;

/* The driver's file handle "inherits" from the public handle */
typedef struct FHNDL
{
    MACSIO_FileHandlePublic_t pub;
    hid_t fid;
    int file_in_sync_with_procs;
    int procs_in_sync_with_procs;
    map<string, hid_t> unsyncedGroupsMap;
    string cwgAbsPath;
    map<string, UnsyncedArrayInfo_t> unsyncedArraysMap;
    map<string, PendingArrayInfo_t> pendingArraysMap;
} FHNDL;

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

static int FNAME(close_file)(struct MACSIO_FileHandle_t *fh, MACSIO_optlist_t const *moreopts);
static int FNAME(sync_meta)(struct MACSIO_FileHandle_t *fh, MACSIO_optlist_t const *moreopts);
static int FNAME(sync_data)(struct MACSIO_FileHandle_t *fh, MACSIO_optlist_t const *moreopts);
static int FNAME(create_ns)(struct MACSIO_FileHandle_t *fh, char const *nsname, MACSIO_optlist_t const *moreopts);
static char const *FNAME(set_ns)(struct MACSIO_FileHandle_t *fh, char const *nsname, MACSIO_optlist_t const *moreopts);
static char const *FNAME(get_ns)(struct MACSIO_FileHandle_t *fh, MACSIO_optlist_t const *moreopts);
static int FNAME(define_array)(MACSIO_FileHandle_t *fh, char const *arrname, int type,
    int const dims[4], MACSIO_optlist_t const *moreopts);
static int FNAME(get_array_info)(MACSIO_FileHandle_t *fh, char const *arrname,
    int *type, int *dims[4], MACSIO_optlist_t const *moreopts);
static int FNAME(define_array_part)(MACSIO_FileHandle_t *fh, char const *arrname,
    int const starts[4], int const counts[4], int strides[4], void **buf, MACSIO_optlist_t const *moreopts);
static int FNAME(start_pending_arrays)(MACSIO_FileHandle_t *fh);
static int FNAME(finish_pending_arrays)(MACSIO_FileHandle_t *fh);

static char *copy_absname2(char *dst, char const *src)
{
    char *retval;
    if (strlen(src) + 1 > MACSIO_MAX_ABSNAME)
        MACSIO_ERROR(("name \"%s\" too long", src), MACSIO_FATAL);
    retval = strncpy(dst, src, MACSIO_MAX_ABSNAME);
    dst[MACSIO_MAX_ABSNAME-1] = '\0';
    return retval;
}

static char *form_and_copy_absname(char *dst, char const *src, char const *dir)
{
    if (strlen(src) + strlen(dir) + 1 + 1 > MACSIO_MAX_ABSNAME)
        MACSIO_ERROR(("attempt to form absolute name \"%s/%s\" too long", src, dir), MACSIO_FATAL);
    strncpy(dst, dir, MACSIO_MAX_ABSNAME);
    strcat(dst, "/");
    return strcat(dst, src);
}

static char *copy_absname(char *dst, char const *src, char const *dir)
{
    if (src[0] == '/')
        return copy_absname2(dst, src);
    else
        return form_and_copy_absname(dst, src, dir);
}

static MACSIO_FileHandle_t *make_file_handle(hid_t fid)
{
    FHNDL *retval;
    retval = (FHNDL*) calloc(1,sizeof(FHNDL));
    retval->fid = fid;
    retval->file_in_sync_with_procs = 1;
    retval->procs_in_sync_with_procs = 1;

    /* populate file, namespace and array methods here */
    retval->pub.closeFileFunc = FNAME(close_file);

    retval->pub.syncMetaFunc = FNAME(sync_meta);
    retval->pub.syncDataFunc = FNAME(sync_data);

    retval->pub.createNamespaceFunc = FNAME(create_ns);
    retval->pub.setNamespaceFunc = FNAME(set_ns);
    retval->pub.getNamespaceFunc = FNAME(get_ns);

#if 0
    retval->pub.defineArrayFunc = FNAME(define_array);
    retval->pub.getArrayInfoFunc = FNAME(get_array_info);
    retval->pub.defineArrayPartFunc = FNAME(define_array_part);
    retval->pub.startPendingArraysFunc = FNAME(start_pending_arrays);
    retval->pub.finishPendingArraysFunc = FNAME(finish_pending_arrays);
#endif
    
    return (MACSIO_FileHandle_t*) retval;
}

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
#endif
    if (use_log)
    {
        int flags = H5FD_LOG_LOC_IO|H5FD_LOG_NUM_IO|H5FD_LOG_TIME_IO|H5FD_LOG_ALLOC;

        if (lbuf_size > 0)
            flags = H5FD_LOG_ALL;

        h5status |= H5Pset_fapl_log(fapl_id, "macsio_hdf5_log.out", flags, lbuf_size);
    }

    if (h5status < 0)
    {
        if (fapl_id >= 0)
            H5Pclose(fapl_id);
        return 0;
    }

    return fapl_id;
}

static MACSIO_FileHandle_t *FNAME(create_file)(char const *pathname, int flags, MACSIO_optlist_t const *opts)
{
    hid_t fapl_id, fid;
    fapl_id = make_fapl();
    fid = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, fapl_id); 
    if (fid < 0) return 0;
#warning FIX ERROR CHECKING
    return make_file_handle(fid);
}

static MACSIO_FileHandle_t *FNAME(open_file)(char const *pathname, int flags, MACSIO_optlist_t const *opts)
{
    hid_t fapl_id, fid;
    fapl_id = make_fapl();
    fid = H5Fopen(filename,  H5F_ACC_RDWR, fapl_id); 
    if (fid < 0) return 0;
#warning FIX ERROR CHECKING
    return make_file_handle(fid);
}

static int FNAME(close_file)(struct MACSIO_FileHandle_t *_fh, MACSIO_optlist_t const *moreopts)
{
    int retval;
    FHNDL *fh = (FHNDL*) _fh;
    if (dspc != -1)
        H5Sclose(dspc);
    retval = H5Fclose(fh->fid);
    free(fh);
    return retval;
}

static int FNAME(sync_meta)(struct MACSIO_FileHandle_t *_fh, MACSIO_optlist_t const *moreopts)
{
    /* We need to sync all processor's view of the file's groups and datasets */
    FHNDL *fh = (FHNDL*) _fh;
#ifdef HAVE_PARALLEL
    MPI_Comm fileComm = fh->comm;
#endif

    /* Do an all gather of unsyncedGroups */

    /* Do an all gather of unsyncedArrayInfos */

    /* Unique the list of arrays */

    /* Loop to HDF5 creating relevant groups and datasets */

    return 0;
}

static int FNAME(sync_data)(struct MACSIO_FileHandle_t *_fh, MACSIO_optlist_t const *moreopts)
{
    int retval = 0;
    FHNDL *fh = (FHNDL*) _fh;
    return H5Fflush(fh->fid, H5F_SCOPE_GLOBAL) < 0 ? 1 : 0;
}

static int FNAME(create_ns)(struct MACSIO_FileHandle_t *_fh, char const *nsname, MACSIO_optlist_t const *moreopts)
{
    /* We don't actually do the H5Gcreate here. We only log the request for it.
     * We create it when we 'sync_file_meta'. */
    int retval;
    FHNDL *fh = (FHNDL*) _fh;
    fh->unsyncedGroupsMap[string(nsname)] = 0;
    return 0;
}

static char const *FNAME(set_ns)(struct MACSIO_FileHandle_t *_fh, char const *nsname, MACSIO_optlist_t const *moreopts)
{
    char const *retval;
    FHNDL *fh = (FHNDL*) _fh;
    retval = fh->cwgAbsPath.c_str();
    if (nsname[0] == '/')
        fh->cwgAbsPath = string(nsname);
    else
        fh->cwgAbsPath = string(retval) + "/" + string(nsname);
    return retval;
}

static char const *FNAME(get_ns)(struct MACSIO_FileHandle_t *_fh, MACSIO_optlist_t const *moreopts)
{
    int retval;
    FHNDL *fh = (FHNDL*) _fh;
    return fh->cwgAbsPath.c_str();
}

static int FNAME(define_array)(MACSIO_FileHandle_t *_fh, char const *arrname, int type,
    int const dims[4], MACSIO_optlist_t const *moreopts)
{
    /* Again, we don't actually do the H5Dcreate here. We only log the request for it.
     * We create it when we 'sync_file_meta'. */
    int retval;
    FHNDL *fh = (FHNDL*) _fh;
    UnsyncedArrayInfo_t info;
    copy_absname(info.absname, arrname, fh->cwgAbsPath.c_str());
    info.type = type;
    info.dims[0] = dims[0];
    info.dims[1] = dims[1];
    info.dims[2] = dims[2];
    info.dims[3] = dims[3];
    fh->unsyncedArraysMap[info.absname] = info;
    return 0;
}

static int FNAME(get_array_info)(MACSIO_FileHandle_t *_fh, char const *arrname,
    int *type, int *dims[4], MACSIO_optlist_t const *moreopts)
{
    /* Not yet implemented */
    *type = 0;
    (*dims)[0] = 0;
    (*dims)[1] = 0;
    (*dims)[2] = 0;
    (*dims)[3] = 0;
    return -1;
}

static int FNAME(define_array_part)(MACSIO_FileHandle_t *_fh, char const *arrname,
    int const starts[4], int const counts[4], int strides[4], void **buf, MACSIO_optlist_t const *moreopts)
{
    /* Again, we don't actually do the H5Dcreate here. We only log the request for it.
     * We create it when we 'sync_file_meta'. */
    int retval;
    FHNDL *fh = (FHNDL*) _fh;
    PendingArrayInfo_t info;
    copy_absname(info.absname, arrname, fh->cwgAbsPath.c_str());
    info.starts[0] = starts[0];
    info.starts[1] = starts[1];
    info.starts[2] = starts[2];
    info.starts[3] = starts[3];
    info.counts[0] = counts[0];
    info.counts[1] = counts[1];
    info.counts[2] = counts[2];
    info.counts[3] = counts[3];
    info.strides[0] = strides[0];
    info.strides[1] = strides[1];
    info.strides[2] = strides[2];
    info.strides[3] = strides[3];
    info.buf = buf;
#warning WE NEED TO COPY OPTIONS TOO FOR THINGS LIKE IND VS. COLL I/O
    fh->pendingArraysMap[info.absname] = info;
    return 0;
}

static int FNAME(start_pending_arrays)(MACSIO_FileHandle_t *_fh)
{
    /* without the async. i/o engine, HDF5 is blocking. So, we make this a no-op */
    return 0;
}

static int FNAME(finish_pending_arrays)(MACSIO_FileHandle_t *_fh)
{
    /* Here is where we actually do all the pending I/O operations */
    return 0;
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
    iface_map[id].createFileFunc = FNAME(create_file);
    iface_map[id].openFileFunc = FNAME(open_file);

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

#if 0
static int Write_hdf5(void *buf, size_t nbytes)
{
    hid_t dsid;
    herr_t n1, n2;
    char dsname[256];
    static int n = 0;
    if (dspc == -1)
    {
        hsize_t dims = nbytes;
        dspc = H5Screate_simple(1, &dims, &dims);
    }

    sprintf(dsname, "data_%07d", n++);
    dsid = H5Dcreate(fid, dsname, H5T_NATIVE_UCHAR, dspc, H5P_DEFAULT);
    if (dsid < 0) return 0;
    n1 = H5Dwrite(dsid, H5T_NATIVE_UCHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT, buf);
    n2 = H5Dclose(dsid);
    if (n1 < 0 || n2 < 0) return 0;
    return nbytes;
}

static int Read_hdf5(void *buf, size_t nbytes)
{
    hid_t dsid;
    herr_t n1, n2;
    char dsname[256];
    static int n = 0;
    if (dspc == -1)
    {
        hsize_t dims = nbytes;
        dspc = H5Screate_simple(1, &dims, &dims);
    }

    sprintf(dsname, "data_%07d", n++);
    dsid = H5Dopen(fid, dsname);
    if (dsid < 0) return 0;
    n1 = H5Dread(dsid, H5T_NATIVE_UCHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT, buf);
    n2  = H5Dclose(dsid);
    if (n1 < 0 || n2 < 0) return 0;
    return nbytes;
}
#endif
