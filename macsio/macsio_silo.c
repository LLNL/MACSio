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
#include <macsio_main.h>
#include <macsio_mif.h>
#include <log.h>
#include <options.h>
#include <util.h>

#include <silo.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

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
#define FHNDL2(A) MACSIO_FileHandle_ ## A ## _t
#define FHNDL FHNDL2(silo)
#define FNAME2(FUNC,A) FUNC ## _ ## A
#define FNAME(FUNC) FNAME2(FUNC,silo)
#define INAME2(A) #A
#define INAME INAME2(silo)

/* The driver's file handle "inherits" from the public handle */
typedef struct FHNDL
{
    MACSIO_FileHandlePublic_t pub;
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
#warning MOVE LOG HANDLE TO IO CONTEXT
static MACSIO_LogHandle_t *log;

static MACSIO_optlist_t *FNAME(process_args)(int argi, int argc, char *argv[])
{
    const MACSIO_ArgvFlags_t argFlags = {MACSIO_WARN, MACSIO_ARGV_TOMEM};
    char driver_str[128];
    char compression_str[512];
    int cksums = 0;
    int hdf5friendly = 0;
    int show_all_errors = 0;

    strcpy(driver_str, "DB_HDF5");
    strcpy(compression_str, "");
    MACSIO_ProcessCommandLine(0, argFlags, argi, argc, argv,
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
    MACSIO_END_OF_ARGS);

#warning MOVE THIS STUFF TO register METHOD
    driver = StringToDriver(driver_str);
    DBSetEnableChecksums(cksums);
    DBSetFriendlyHDF5Names(hdf5friendly);
    DBSetCompression(compression_str);
#if 0
    DBShowErrors(show_all_errors?DB_ALL_AND_DRVR:DB_ALL, NULL);
#endif
    DBShowErrors(DB_ALL_AND_DRVR, 0);

    return 0;
}

static void *CreateSiloFile(const char *fname, const char *nsname, void *userData)
{
    int driverId = *((int*) userData);
    DBfile *siloFile = DBCreate(fname, DB_CLOBBER, DB_LOCAL, "macsio output file", driverId);
    if (siloFile && nsname)
    {
        DBMkDir(siloFile, nsname);
        DBSetDir(siloFile, nsname);
    }
    return (void *) siloFile;
}

static void *OpenSiloFile(const char *fname, const char *nsname, MACSIO_MIF_iomode_t ioMode,
    void *userData)
{
    int driverId = *((int*) userData);
    /*Log(log, "DBOpen(\"%s\", %d, %d)", fname, driverId, ioMode==MACSIO_MIF_WRITE ? DB_APPEND : DB_READ);*/
    DBfile *siloFile = DBOpen(fname, driverId, ioMode==MACSIO_MIF_WRITE ? DB_APPEND : DB_READ);
    /*Log(log, "siloFile = %p", siloFile);*/
    if (siloFile && nsname)
    {
        if (ioMode == MACSIO_MIF_WRITE)
            DBMkDir(siloFile, nsname);
        DBSetDir(siloFile, nsname);
    }
    return (void *) siloFile;
}

static void CloseSiloFile(void *file, void *userData)
{
    DBfile *siloFile = (DBfile *) file;
    if (siloFile)
        DBClose(siloFile);
}

static void write_rect_mesh_part(DBfile *dbfile, json_object *part)
{
    json_object *coordobj;
    char const *coordnames[] = {"X","Y","Z"};
    void const *coords[3];
    int ndims = JsonGetInt(part, "Mesh/GeomDim");
    int dims[3] = {1,1,1};
    int dimsz[3] = {1,1,1};

    coordobj = JsonGetObj(part, "Mesh/Coords/XAxisCoords");
    coords[0] = json_object_extarr_data(coordobj);
    dims[0] = JsonGetInt(part, "Mesh/LogDims", 0);
    dimsz[0] = dims[0]-1;
    if (ndims > 1)
    {
        coordobj = JsonGetObj(part, "Mesh/Coords/YAxisCoords");
        coords[1] = json_object_extarr_data(coordobj);
        dims[1] = JsonGetInt(part, "Mesh/LogDims", 1);
        dimsz[1] = dims[1]-1;
        
    }
    if (ndims > 2)
    {
        coordobj = JsonGetObj(part, "Mesh/Coords/ZAxisCoords");
        coords[2] = json_object_extarr_data(coordobj);
        dims[2] = JsonGetInt(part, "Mesh/LogDims", 2);
        dimsz[2] = dims[2]-1;
    }

    DBPutQuadmesh(dbfile, "mesh", (char**) coordnames, coords,
        dims, ndims, DB_DOUBLE, DB_COLLINEAR, 0);

    json_object *vars_array = JsonGetObj(part, "Vars");
    for (int i = 0; i < json_object_array_length(vars_array); i++)
    {
        json_object *varobj = json_object_array_get_idx(vars_array, i);
        int cent = strcmp(JsonGetStr(varobj, "centering"),"zone")?DB_NODECENT:DB_ZONECENT;
        int *d = cent==DB_NODECENT?dims:dimsz;
        json_object *dataobj = JsonGetObj(varobj, "data");
        int dtype = json_object_extarr_type(dataobj)==json_extarr_type_flt64?DB_DOUBLE:DB_INT;
        
        DBPutQuadvar1(dbfile, JsonGetStr(varobj, "name"), "mesh",
            (void *)json_object_extarr_data(dataobj), d, ndims, 0, 0, dtype, cent, 0); 
    }
}

static void write_mesh_part(DBfile *dbfile, json_object *part)
{
    if (!strcmp(JsonGetStr(part, "Mesh/MeshType"), "rectilinear"))
        write_rect_mesh_part(dbfile, part);
}

static void WriteMultiXXXObjects(json_object *main_obj, DBfile *siloFile, MACSIO_MIF_baton_t *bat)
{
    int i, j;
    char const *file_ext = JsonGetStr(main_obj, "clargs/--fileext");
    char const *file_base = JsonGetStr(main_obj, "clargs/--filebase");
    int numChunks = JsonGetInt(main_obj, "problem/global/TotalParts");
    char **blockNames = (char **) malloc(numChunks * sizeof(char*));
    int *blockTypes = (int *) malloc(numChunks * sizeof(int));

    /* Go to root directory in the silo file */
    DBSetDir(siloFile, "/");

    /* Construct the lists of individual object names */
    for (i = 0; i < numChunks; i++)
    {
        int rank_owning_chunk = MACSIO_GetRankOwningPart(main_obj, i);
        int groupRank = MACSIO_MIF_GroupRank(bat, rank_owning_chunk);
        blockNames[i] = (char *) malloc(1024);
        if (groupRank == 0)
        {
            /* this mesh block is in the file 'root' owns */
            sprintf(blockNames[i], "/domain_%07d/mesh", i);
        }
        else
        {
#warning USE SILO NAMESCHEMES INSTEAD
            sprintf(blockNames[i], "%s_silo_%05d.%s:/domain_%07d/mesh",
                JsonGetStr(main_obj, "clargs/--filebase"),
                groupRank, 
                JsonGetStr(main_obj, "clargs/--fileext"),
                i);
        }
        blockTypes[i] = DB_QUADMESH;
    }

    /* Write the multi-block objects */
    DBPutMultimesh(siloFile, "multi_mesh", numChunks, blockNames, blockTypes, 0);

    json_object *first_part = JsonGetObj(main_obj, "problem/parts", 0);
    json_object *vars_array = JsonGetObj(first_part, "Vars");
    int numVars = json_object_array_length(vars_array);
    for (j = 0; j < numVars; j++)
    {
        char const *varname = JsonGetStr(vars_array, "", j, "name");

        for (i = 0; i < numChunks; i++)
        {
            int rank_owning_chunk = MACSIO_GetRankOwningPart(main_obj, i);
            int groupRank = MACSIO_MIF_GroupRank(bat, rank_owning_chunk);
            if (groupRank == 0)
            {
                /* this mesh block is in the file 'root' owns */
                sprintf(blockNames[i], "/domain_%07d/%s", i, varname);
            }
            else
            {
#warning USE SILO NAMESCHEMES INSTEAD
                sprintf(blockNames[i], "%s_silo_%05d.%s:/domain_%07d/%s",
                    JsonGetStr(main_obj, "clargs/--filebase"),
                    groupRank, 
                    JsonGetStr(main_obj, "clargs/--fileext"),
                    i,varname);
            }
            blockTypes[i] = DB_QUADVAR;
        }

        /* Write the multi-block objects */
        DBPutMultivar(siloFile, varname, numChunks, blockNames, blockTypes, 0);

    }

    /* Clean up */
    for (i = 0; i < numChunks; i++)
        free(blockNames[i]);
    free(blockNames);
    free(blockTypes);
}

#warning HOW IS A NEW DUMP CLASS HANDLED
static void FNAME(main_dump)(int argi, int argc, char **argv, json_object *main_obj, int dumpn, double dumpt)
{
    DBfile *siloFile;
    int numGroups = -1;
    int rank, size;
    char fileName[256];
    MACSIO_MIF_baton_t *bat;

#warning MAKE LOGGING A CL OPTION
#if 0
    log = Log_Init(MPI_COMM_WORLD, "macsio_silo.log", 128, 20);
#endif

#warning NEED TO PASS OR HAVE A FUNCTION THAT PRODUCES MPI COMM
    /* Without this barrier, I get strange behavior with Silo's MACSIO_MIF interface */
    MPI_Barrier(MPI_COMM_WORLD);

    /* process cl args */
    FNAME(process_args)(argi, argc, argv);

    rank = JsonGetInt(main_obj, "parallel/mpi_rank");
    size = JsonGetInt(main_obj, "parallel/mpi_size");

#warning MOVE TO A FUNCTION
    /* ensure we're in MIF mode and determine the file count */
    json_object *parfmode_obj = JsonGetObj(main_obj, "clargs/--parallel_file_mode");
    if (parfmode_obj)
    {
        json_object *modestr = json_object_array_get_idx(parfmode_obj, 0);
        json_object *filecnt = json_object_array_get_idx(parfmode_obj, 1);
#warning ERRORS NEED TO GO TO LOG FILES AND ERROR BEHAVIOR NEEDS TO BE HONORED
        if (!strcmp(json_object_get_string(modestr), "SIF"))
        {
            MACSIO_ERROR(("Silo plugin doesn't support SIF mode"), MACSIO_FATAL);
        }
        else if (strcmp(json_object_get_string(modestr), "MIF"))
        {
            MACSIO_ERROR(("Ignoring non-standard MIF mode"), MACSIO_WARN);
        }
        numGroups = json_object_get_int(filecnt);
    }
    else
    {
        char const * modestr = JsonGetStr(main_obj, "clargs/--parallel_file_mode");
        if (!strcmp(modestr, "SIF"))
        {
            MACSIO_ERROR(("Silo plugin doesn't support SIF mode"), MACSIO_FATAL);
        }
        else if (!strcmp(modestr, "MIFMAX"))
            numGroups = JsonGetInt(main_obj, "parallel/mpi_size");
        else if (!strcmp(modestr, "MIFAUTO"))
        {
            /* Call utility to determine optimal file count */
#warning ADD UTILIT TO DETERMINE OPTIMAL FILE COUNT
        }
    }

    /* Initialize MACSIO_MIF, pass a pointer to the driver type as the user data. */
    bat = MACSIO_MIF_Init(numGroups, MACSIO_MIF_WRITE, MPI_COMM_WORLD, 1,
        CreateSiloFile, OpenSiloFile, CloseSiloFile, &driver);

    /* Construct name for the silo file */
    sprintf(fileName, "%s_silo_%05d.%s",
        JsonGetStr(main_obj, "clargs/--filebase"),
        MACSIO_MIF_GroupRank(bat, rank),
        JsonGetStr(main_obj, "clargs/--fileext"));

    /* Wait for write access to the file. All processors call this.
     * Some processors (the first in each group) return immediately
     * with write access to the file. Other processors wind up waiting
     * until they are given control by the preceeding processor in 
     * the group when that processor calls "HandOffBaton" */
    siloFile = (DBfile *) MACSIO_MIF_WaitForBaton(bat, fileName, 0);

    json_object *parts = JsonGetObj(main_obj, "problem/parts");
    int numParts = json_object_array_length(parts);

    for (int i = 0; i < numParts; i++)
    {
        char domain_dir[256];
        json_object *this_part = JsonGetObj(main_obj, "problem/parts", i);

        snprintf(domain_dir, sizeof(domain_dir), "domain_%07d", JsonGetInt(this_part, "Mesh/ChunkID"));
 
        DBMkDir(siloFile, domain_dir);
        DBSetDir(siloFile, domain_dir);

        write_mesh_part(siloFile, this_part);

        DBSetDir(siloFile, "..");
    }

    /* If this is the 'root' processor, also write Silo's multi-XXX objects */
    if (rank == 0)
        WriteMultiXXXObjects(main_obj, siloFile, bat);

    /* Hand off the baton to the next processor. This winds up closing
     * the file so that the next processor that opens it can be assured
     * of getting a consistent and up to date view of the file's contents. */
    MACSIO_MIF_HandOffBaton(bat, siloFile);

    /* We're done using MACSIO_MIF, so finish it off */
    MACSIO_MIF_Finish(bat);

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
