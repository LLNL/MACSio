#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
  #ifndef WINDOWS_LEAN_AND_MEAN
    #define WINDOWS_LEAN_AND_MEAN
  #endif
  #include <windows.h>
#endif

#include <ifacemap.h>
#include <ioperf.h>
#include <options.h>
#include <util.h>

#include <silo.h>

/*
 *  BEGIN StringToDriver utility code from Silo's tests
 */

#define CHECK_SYMBOL(A)  if (!strncmp(str, #A, strlen(str))) return A

#define CHECK_SYMBOLN_INT(A)				\
if (!strncmp(tok, #A, strlen(#A)))			\
{							\
    int n = sscanf(tok, #A"=%d", &driver_ints[driver_nints]);\
    if (n == 1)						\
    {							\
        DBAddOption(opts, A, &driver_ints[driver_nints]);\
        driver_nints++;					\
        got_it = 1;					\
    }							\
}

#define CHECK_SYMBOLN_STR(A)				\
if (!strncmp(tok, #A, strlen(#A)))			\
{							\
    driver_strs[driver_nstrs] = strdup(&tok[strlen(#A)]+1);\
    DBAddOption(opts, A, driver_strs[driver_nstrs]);	\
    driver_nstrs++;					\
    got_it = 1;						\
}

#define CHECK_SYMBOLN_SYM(A)				\
if (!strncmp(tok, #A, strlen(#A)))			\
{							\
    driver_ints[driver_nints] = StringToDriver(&tok[strlen(#A)]+1);\
    DBAddOption(opts, A, &driver_ints[driver_nints]);	\
    driver_nints++;					\
    got_it = 1;						\
}

static DBoptlist *driver_opts[] = {0,0,0,0,0,0,0,0,0,0};
static int driver_opts_ids[] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1};
static int driver_ints[100];
static int driver_nints = 0;
static char *driver_strs[] = {0,0,0,0,0,0,0,0,0,0};
static int driver_nstrs = 0;
static const int driver_nopts = sizeof(driver_opts)/sizeof(driver_opts[0]);

static void CleanupDriverStuff()
{
    int i;
    for (i = 0; i < driver_nopts; i++)
    {
        if (driver_opts_ids[i] != -1) DBUnregisterFileOptionsSet(driver_opts_ids[i]);
        if (driver_opts[i]) DBFreeOptlist(driver_opts[i]);
    }
    for (i = 0; i < sizeof(driver_strs)/sizeof(driver_strs[0]); i++)
        if (driver_strs[i]) free(driver_strs[i]);
}

static void MakeDriverOpts(DBoptlist **_opts, int *opts_id)
{
    DBoptlist *opts = DBMakeOptlist(30);
    int i;

    for (i = 0; i < driver_nopts; i++)
    {
        if (driver_opts[i] == 0)
        {
            driver_opts[i] = opts;
            break;
        }
    }

    *_opts = opts;
    *opts_id = DBRegisterFileOptionsSet(opts);
     driver_opts_ids[i] = *opts_id;
}

static int StringToDriver(const char *str)
{
    DBoptlist *opts = 0;
    int opts_id = -1;

    CHECK_SYMBOL(DB_PDB);
    CHECK_SYMBOL(DB_PDBP);
    CHECK_SYMBOL(DB_HDF5);
    CHECK_SYMBOL(DB_HDF5_SEC2);
    CHECK_SYMBOL(DB_HDF5_STDIO);
    CHECK_SYMBOL(DB_HDF5_CORE);
    CHECK_SYMBOL(DB_HDF5_SPLIT);
    CHECK_SYMBOL(DB_HDF5_MPIO);
    CHECK_SYMBOL(DB_HDF5_MPIP);
    CHECK_SYMBOL(DB_HDF5_LOG);
    CHECK_SYMBOL(DB_HDF5_DIRECT);
    CHECK_SYMBOL(DB_HDF5_FAMILY);
    CHECK_SYMBOL(DB_HDF5_SILO);
    
    CHECK_SYMBOL(DB_FILE_OPTS_H5_DEFAULT_DEFAULT);
    CHECK_SYMBOL(DB_FILE_OPTS_H5_DEFAULT_SEC2);
    CHECK_SYMBOL(DB_FILE_OPTS_H5_DEFAULT_STDIO);
    CHECK_SYMBOL(DB_FILE_OPTS_H5_DEFAULT_CORE);
    CHECK_SYMBOL(DB_FILE_OPTS_H5_DEFAULT_LOG);
    CHECK_SYMBOL(DB_FILE_OPTS_H5_DEFAULT_SPLIT);
    CHECK_SYMBOL(DB_FILE_OPTS_H5_DEFAULT_DIRECT);
    CHECK_SYMBOL(DB_FILE_OPTS_H5_DEFAULT_FAMILY);
    CHECK_SYMBOL(DB_FILE_OPTS_H5_DEFAULT_MPIP);
    CHECK_SYMBOL(DB_FILE_OPTS_H5_DEFAULT_MPIO);
    CHECK_SYMBOL(DB_FILE_OPTS_H5_DEFAULT_SILO);

    CHECK_SYMBOL(DB_H5VFD_DEFAULT);
    CHECK_SYMBOL(DB_H5VFD_SEC2);
    CHECK_SYMBOL(DB_H5VFD_STDIO);
    CHECK_SYMBOL(DB_H5VFD_CORE);
    CHECK_SYMBOL(DB_H5VFD_LOG);
    CHECK_SYMBOL(DB_H5VFD_SPLIT);
    CHECK_SYMBOL(DB_H5VFD_DIRECT);
    CHECK_SYMBOL(DB_H5VFD_FAMILY);
    CHECK_SYMBOL(DB_H5VFD_MPIO);
    CHECK_SYMBOL(DB_H5VFD_MPIP);
    CHECK_SYMBOL(DB_H5VFD_SILO);

    if (!strncmp(str, "DB_HDF5_OPTS(", 13))
    {
        char *tok, *tmpstr;;

        MakeDriverOpts(&opts, &opts_id);

	tmpstr = strdup(&str[13]);
	errno = 0;
	tok = strtok(tmpstr, ",)");

        while (tok)
        {
            int got_it = 0;

            CHECK_SYMBOLN_SYM(DBOPT_H5_VFD)
            CHECK_SYMBOLN_SYM(DBOPT_H5_RAW_FILE_OPTS)
            CHECK_SYMBOLN_STR(DBOPT_H5_RAW_EXTENSION)
            CHECK_SYMBOLN_SYM(DBOPT_H5_META_FILE_OPTS)
            CHECK_SYMBOLN_STR(DBOPT_H5_META_EXTENSION)
            CHECK_SYMBOLN_INT(DBOPT_H5_CORE_ALLOC_INC)
            CHECK_SYMBOLN_INT(DBOPT_H5_CORE_NO_BACK_STORE)
            CHECK_SYMBOLN_INT(DBOPT_H5_META_BLOCK_SIZE)
            CHECK_SYMBOLN_INT(DBOPT_H5_SMALL_RAW_SIZE)
            CHECK_SYMBOLN_INT(DBOPT_H5_ALIGN_MIN)
            CHECK_SYMBOLN_INT(DBOPT_H5_ALIGN_VAL)
            CHECK_SYMBOLN_INT(DBOPT_H5_DIRECT_MEM_ALIGN)
            CHECK_SYMBOLN_INT(DBOPT_H5_DIRECT_BLOCK_SIZE)
            CHECK_SYMBOLN_INT(DBOPT_H5_DIRECT_BUF_SIZE)
            CHECK_SYMBOLN_STR(DBOPT_H5_LOG_NAME)
            CHECK_SYMBOLN_INT(DBOPT_H5_LOG_BUF_SIZE)
            CHECK_SYMBOLN_INT(DBOPT_H5_SIEVE_BUF_SIZE)
            CHECK_SYMBOLN_INT(DBOPT_H5_CACHE_NELMTS)
            CHECK_SYMBOLN_INT(DBOPT_H5_CACHE_NBYTES)
            CHECK_SYMBOLN_INT(DBOPT_H5_FAM_SIZE)
            CHECK_SYMBOLN_SYM(DBOPT_H5_FAM_FILE_OPTS)
            CHECK_SYMBOLN_INT(DBOPT_H5_SILO_BLOCK_SIZE)
            CHECK_SYMBOLN_INT(DBOPT_H5_SILO_BLOCK_COUNT)
            CHECK_SYMBOLN_INT(DBOPT_H5_SILO_LOG_STATS)
            CHECK_SYMBOLN_INT(DBOPT_H5_SILO_USE_DIRECT)
            CHECK_SYMBOLN_STR(DB_FILE_OPTS_H5_DEFAULT_DEFAULT)
            CHECK_SYMBOLN_STR(DB_FILE_OPTS_H5_DEFAULT_SEC2)
            CHECK_SYMBOLN_STR(DB_FILE_OPTS_H5_DEFAULT_STDIO)
            CHECK_SYMBOLN_STR(DB_FILE_OPTS_H5_DEFAULT_CORE)
            CHECK_SYMBOLN_STR(DB_FILE_OPTS_H5_DEFAULT_LOG)
            CHECK_SYMBOLN_STR(DB_FILE_OPTS_H5_DEFAULT_SPLIT)
            CHECK_SYMBOLN_STR(DB_FILE_OPTS_H5_DEFAULT_DIRECT)
            CHECK_SYMBOLN_STR(DB_FILE_OPTS_H5_DEFAULT_FAMILY)
            CHECK_SYMBOLN_STR(DB_FILE_OPTS_H5_DEFAULT_MPIP)
            CHECK_SYMBOLN_STR(DB_FILE_OPTS_H5_DEFAULT_MPIO)
            CHECK_SYMBOLN_STR(DB_FILE_OPTS_H5_DEFAULT_SILO)

            if (!got_it)
            {
                fprintf(stderr, "Unable to determine driver from string \"%s\"\n", tok);
	        exit(-1);
            }

	    tok = strtok(0, ",)");
	    if (errno != 0)
	    {
                fprintf(stderr, "Unable to determine driver from string \"%s\"\n", tok);
	        exit(-1);
	    }
        }

        free(tmpstr);

        return DB_HDF5_OPTS(opts_id);
    }

    fprintf(stderr, "Unable to determine driver from string \"%s\"\n", str);
    exit(-1);
}

/*
 *  END StringToDriver utility code from Silo's tests
 */

/* convenient name mapping macors */
#define FHNDL2(A) IOPFileHandle_ ## A ## _t
#define FHNDL FHNDL2(silo)
#define FNAME2(FUNC,A) FUNC ## _ ## A
#define FNAME(FUNC) FNAME2(FUNC,silo)
#define INAME2(A) #A
#define INAME INAME2(silo)

/* The driver's file handle "inherits" from the public handle */
typedef struct FHNDL
{
    IOPFileHandlePublic_t pub;
    DBfile *dbfile;
} FHNDL;

/* the name you want to assign to the interface */
static char const *iface_name = INAME;
static char const *iface_ext = INAME;

static const char *filename;
DBfile *dbfile;
static int has_mesh = 0;
static int driver = DB_HDF5;
static int show_all_errors = FALSE;

static int FNAME(close_file)(struct IOPFileHandle_t *fh, IOPoptlist_t const *moreopts);

static IOPFileHandle_t *make_file_handle(DBfile *dbfile)
{
    FHNDL *retval;
    retval = (FHNDL*) calloc(1,sizeof(FHNDL));
    retval->dbfile = dbfile;

    /* populate file, namespace and array methods here */
    retval->pub.closeFileFunc = FNAME(close_file);

    return (IOPFileHandle_t*) retval;
}

static IOPFileHandle_t *FNAME(create_file)(char const *pathname, int flags, IOPoptlist_t const *opts)
{
    DBfile *dbfile = DBCreate(filename, DB_CLOBBER, DB_LOCAL, "ioperf test file", driver);
    if (!dbfile) return 0;
#warning FIX ERROR CHECKING
    return make_file_handle(dbfile);
}

static IOPFileHandle_t *FNAME(open_file)(char const *pathname, int flags, IOPoptlist_t const *opts)
{
    DBfile *dbfile = DBOpen(filename,  DB_UNKNOWN, DB_APPEND); 
    if (!dbfile) return 0;
#warning FIX ERROR CHECKING
    return make_file_handle(dbfile);
}

static int FNAME(close_file)(struct IOPFileHandle_t *_fh, IOPoptlist_t const *moreopts)
{
    int retval;
    FHNDL *fh = (FHNDL*) _fh;
    retval = DBClose(fh->dbfile);
    free(fh);
    return retval;
}

static IOPoptlist_t *FNAME(process_args)(int argi, int argc, char *argv[])
{
    const int unknownArgsFlag = IOP_WARN;
    char driver_str[128];
    char compression_str[512];
    int cksums = 0;
    int hdf5friendly = 0;
    int show_all_errors = 0;

    strcpy(driver_str, "DB_HDF5");
    strcpy(compression_str, "");
    IOPProcessCommandLine(unknownArgsFlag, argi, argc, argv,
        "--driver %s",
            "Specify Silo's I/O driver (DB_PDB|DB_HDF5 or variants) [DB_HDF5]",
            &driver_str,
        "--checksums",
            "Enable checksum checks [no]",
            &cksums,
        "--hdf5friendly",
            "Generate HDF5 friendly files [no]",
            &hdf5friendly,
        "--compression %s",
            "Specify compression method to use",
            &compression_str,
        "--show-all-errors",
            "Show all errors Silo encounters",
            &show_all_errors,
    IOP_END_OF_ARGS);

#warning MOVE THIS STUFF TO register METHOD
    driver = StringToDriver(driver_str);
    DBSetEnableChecksums(cksums);
    DBSetFriendlyHDF5Names(hdf5friendly);
    DBSetCompression(compression_str);
    DBShowErrors(show_all_errors?DB_ALL_AND_DRVR:DB_ALL, NULL);

    return 0;
}

static int register_this_interface()
{
    unsigned int id = bjhash((unsigned char*)iface_name, strlen(iface_name), 0) % MAX_IFACES;
    if (strlen(iface_name) >= MAX_IFACE_NAME)
        IOP_ERROR(("interface name \"%s\" too long",iface_name) , IOP_FATAL);
    if (iface_map[id].slotUsed!= 0)
        IOP_ERROR(("hash collision for interface name \"%s\"",iface_name) , IOP_FATAL);

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
static int Write_silo(void *buf, size_t nbytes)
{
    static int n = 0;
    int dims[3] = {1, 1, 1};
    int status;

    dims[0] = nbytes / sizeof(double);
    if (!has_mesh)
    {
        char *coordnames[] = {"x"};
        void *coords[3] = {0, 0, 0};
        coords[0] = buf;
        has_mesh = 1;
        status = DBPutQuadmesh(dbfile, "mesh", coordnames, coords, dims, 1, DB_DOUBLE, DB_COLLINEAR, 0);
    }
    else
    {
        char dsname[64];
        sprintf(dsname, "data_%07d", n++);
        status = DBPutQuadvar1(dbfile, dsname, "mesh", buf, dims, 1, 0, 0, DB_DOUBLE, DB_NODECENT, 0);
    }

    if (status < 0) return 0;
    return nbytes;
}

static int Read_silo(void *buf, size_t nbytes)
{
    char dsname[64];
    static int n = 0;
    void *status;
    sprintf(dsname, "data_%07d", n++);
    status = DBGetQuadvar(dbfile, dsname);
    if (status == 0) return 0;
    return nbytes;
}
#endif
