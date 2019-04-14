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

#include <json-cwx/json.h>

#include <macsio_clargs.h>
#include <macsio_data.h>
#include <macsio_iface.h>
#include <macsio_log.h>
#include <macsio_main.h>
#include <macsio_mif.h>
#include <macsio_utils.h>

#include <silo.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

/*!
\addtogroup plugins
@{
*/

/*!
\addtogroup Silo
@{
*/

#define NARRVALS(Arr) (sizeof(Arr)/sizeof(Arr[0]))

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

/* the name you want to assign to the interface */
static char const *iface_name = "silo";
static char const *iface_ext = "silo";

static const char *filename;
static int has_mesh = 0;
static int driver = DB_HDF5;
static int show_all_errors = FALSE;
//#warning MOVE LOG HANDLE TO IO CONTEXT

static int process_args(int argi, int argc, char *argv[])
{
    const MACSIO_CLARGS_ArgvFlags_t argFlags = {
        MACSIO_CLARGS_WARN,
        MACSIO_CLARGS_TOMEM,
        MACSIO_CLARGS_ASSIGN_OFF};
    char *driver_str = 0;
    char *compression_str = 0;
    int cksums = 0;
    int hdf5friendly = 0;
    int show_all_errors = 0;

    MACSIO_CLARGS_ProcessCmdline(0, argFlags, argi, argc, argv,
        "--driver %s", "DB_HDF5",
            "Specify Silo's I/O driver (DB_PDB|DB_HDF5 or variants)",
            &driver_str,
        "--checksums", "",
            "Enable checksum checks",
            &cksums,
        "--hdf5friendly", "",
            "Generate HDF5 friendly files",
            &hdf5friendly,
        "--compression %s", MACSIO_CLARGS_NODEFAULT,
            "Specify compression method to use",
            &compression_str,
        "--show-all-errors", "",
            "Show all errors Silo encounters",
            &show_all_errors,
    MACSIO_CLARGS_END_OF_ARGS);

    if (driver_str)
    {
        driver = StringToDriver(driver_str);
        free(driver_str);
    }
    if (compression_str)
    {
        if (*compression_str)
            DBSetCompression(compression_str);
        free(compression_str);
    }
    DBSetEnableChecksums(cksums);
    DBSetFriendlyHDF5Names(hdf5friendly);
    DBShowErrors(show_all_errors?DB_ALL_AND_DRVR:DB_ALL, NULL);

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

static void *OpenSiloFile(const char *fname, const char *nsname, MACSIO_MIF_ioFlags_t ioFlags,
    void *userData)
{
    int driverId = *((int*) userData);
    DBfile *siloFile = DBOpen(fname, driverId, ioFlags.do_wr ? DB_APPEND : DB_READ);
    if (siloFile && nsname)
    {
        if (ioFlags.do_wr)
            DBMkDir(siloFile, nsname);
        DBSetDir(siloFile, nsname);
    }
    return (void *) siloFile;
}

static int CloseSiloFile(void *file, void *userData)
{
    DBfile *siloFile = (DBfile *) file;
    if (siloFile)
        return DBClose(siloFile);
    return -1;
}

static void write_quad_mesh_part(DBfile *dbfile, json_object *part, int silo_mesh_type)
{
    json_object *coordobj;
    char const *coordnames[] = {"X","Y","Z"};
    void const *coords[3];
    int ndims = JsonGetInt(part, "Mesh/GeomDim");
    int dims[3] = {1,1,1};
    int dimsz[3] = {1,1,1};

//#warning SHOULD REALLY MAKE COORD MEMBER NAMES FOR ALL CASES INSTEAD OF XAXISCOORDS AND XCOORDS
    if (silo_mesh_type == DB_COLLINEAR)	
        coordobj = JsonGetObj(part, "Mesh/Coords/XAxisCoords");
    else
        coordobj = JsonGetObj(part, "Mesh/Coords/XCoords");
    coords[0] = json_object_extarr_data(coordobj);
    dims[0] = JsonGetInt(part, "Mesh/LogDims", 0);
    dimsz[0] = dims[0]-1;
    if (ndims > 1)
    {
        if (silo_mesh_type == DB_COLLINEAR)	
            coordobj = JsonGetObj(part, "Mesh/Coords/YAxisCoords");
        else
            coordobj = JsonGetObj(part, "Mesh/Coords/YCoords");
        coords[1] = json_object_extarr_data(coordobj);
        dims[1] = JsonGetInt(part, "Mesh/LogDims", 1);
        dimsz[1] = dims[1]-1;
        
    }
    if (ndims > 2)
    {
        if (silo_mesh_type == DB_COLLINEAR)	
            coordobj = JsonGetObj(part, "Mesh/Coords/ZAxisCoords");
        else
            coordobj = JsonGetObj(part, "Mesh/Coords/ZCoords");
        coords[2] = json_object_extarr_data(coordobj);
        dims[2] = JsonGetInt(part, "Mesh/LogDims", 2);
        dimsz[2] = dims[2]-1;
    }

    DBPutQuadmesh(dbfile, "mesh", (char**) coordnames, coords,
        dims, ndims, DB_DOUBLE, silo_mesh_type, 0);

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

static void write_ucdzoo_mesh_part(DBfile *dbfile, json_object *part, char const *topo_name)
{
    json_object *coordobj, *topoobj;
    char const *coordnames[] = {"X","Y","Z"};
    void const *coords[3];
    int ndims = JsonGetInt(part, "Mesh/GeomDim");
    int nnodes = 1, nzones = 1;
    int dims[3] = {1,1,1};
    int dimsz[3] = {1,1,1};

    coordobj = JsonGetObj(part, "Mesh/Coords/XCoords");
    coords[0] = json_object_extarr_data(coordobj);
    dims[0] = JsonGetInt(part, "Mesh/LogDims", 0);
    dimsz[0] = dims[0]-1;
    nnodes *= dims[0];
    nzones *= dimsz[0];
    if (ndims > 1)
    {
        coordobj = JsonGetObj(part, "Mesh/Coords/YCoords");
        coords[1] = json_object_extarr_data(coordobj);
        dims[1] = JsonGetInt(part, "Mesh/LogDims", 1);
        dimsz[1] = dims[1]-1;
        nnodes *= dims[1];
        nzones *= dimsz[1];
    }
    if (ndims > 2)
    {
        coordobj = JsonGetObj(part, "Mesh/Coords/ZCoords");
        coords[2] = json_object_extarr_data(coordobj);
        dims[2] = JsonGetInt(part, "Mesh/LogDims", 2);
        dimsz[2] = dims[2]-1;
        nnodes *= dims[2];
        nzones *= dimsz[2];
    }

    if (ndims == 1 || !strcmp(topo_name, "ucdzoo"))
    {
        DBPutUcdmesh(dbfile, "mesh", ndims, (DBCAS_t) coordnames,
            coords, nnodes, nzones, "zl", NULL, DB_DOUBLE, NULL);
    }
    else if (!strcmp(topo_name, "arbitrary"))
    {
        DBoptlist *ol = DBMakeOptlist(1);
        DBAddOption(ol, DBOPT_PHZONELIST, (char *) "phzl");
        DBPutUcdmesh(dbfile, "mesh", ndims, (DBCAS_t) coordnames,
            coords, nnodes, nzones, NULL, NULL, DB_DOUBLE, ol);
        DBFreeOptlist(ol);
    }

    /* Write out explicit topology */
    if (ndims == 1 || !strcmp(topo_name, "ucdzoo"))
    {
        json_object *topoobj = JsonGetObj(part, "Mesh/Topology");
        json_object *nlobj = JsonGetObj(topoobj, "Nodelist");
        int const *nodelist = (int const *) json_object_extarr_data(nlobj);
        int lnodelist = json_object_extarr_nvals(nlobj);
        int shapetype, shapesize, shapecnt = nzones;

        if (!strcmp(JsonGetStr(topoobj, "ElemType"), "Beam2"))
        {
            shapesize = 2;
            shapetype = DB_ZONETYPE_BEAM;
        }
        else if (!strcmp(JsonGetStr(topoobj, "ElemType"), "Quad4"))
        {
            shapesize = 4;
            shapetype = DB_ZONETYPE_QUAD;
        }
        else if (!strcmp(JsonGetStr(topoobj, "ElemType"), "Hex8"))
        {
            shapesize = 8;
            shapetype = DB_ZONETYPE_HEX;
        }

        DBPutZonelist2(dbfile, "zl", nzones, ndims, nodelist, lnodelist, 0, 0, 0,
            &shapetype, &shapesize, &shapecnt, 1,NULL);
    }
    else if (!strcmp(topo_name, "arbitrary"))
    {
        json_object *topoobj = JsonGetObj(part, "Mesh/Topology");
        json_object *nlobj = JsonGetObj(topoobj, "Nodelist");
        int const *nodelist = (int const *) json_object_extarr_data(nlobj);
        int lnodelist = json_object_extarr_nvals(nlobj);
        json_object *ncobj = JsonGetObj(topoobj, "NodeCounts");
        int const *nodecnt = (int const *) json_object_extarr_data(ncobj);
        int nfaces = json_object_extarr_nvals(ncobj);
        json_object *flobj = JsonGetObj(topoobj, "Facelist");
        int const *facelist = (int const *) json_object_extarr_data(flobj);
        int lfacelist = json_object_extarr_nvals(flobj);
        json_object *fcobj = JsonGetObj(topoobj, "FaceCounts");
        int const *facecnt = (int const *) json_object_extarr_data(fcobj);
        int i;

        DBPutPHZonelist(dbfile, "phzl",
            nfaces, nodecnt, lnodelist, nodelist, NULL,
            nzones, facecnt, lfacelist, facelist,
            0, 0, nzones-1, NULL);
    }

    json_object *vars_array = JsonGetObj(part, "Vars");
    for (int i = 0; i < json_object_array_length(vars_array); i++)
    {
        json_object *varobj = json_object_array_get_idx(vars_array, i);
        int cent = strcmp(JsonGetStr(varobj, "centering"),"zone")?DB_NODECENT:DB_ZONECENT;
        int cnt = cent==DB_NODECENT?nnodes:nzones;
        json_object *dataobj = JsonGetObj(varobj, "data");
        int dtype = json_object_extarr_type(dataobj)==json_extarr_type_flt64?DB_DOUBLE:DB_INT;
        
        DBPutUcdvar1(dbfile, JsonGetStr(varobj, "name"), "mesh",
            (void*)json_object_extarr_data(dataobj), cnt, NULL, 0, dtype, cent, NULL);

    }
}

static void write_mesh_part(DBfile *dbfile, json_object *part)
{
    if (!strcmp(JsonGetStr(part, "Mesh/MeshType"), "rectilinear"))
        write_quad_mesh_part(dbfile, part, DB_COLLINEAR);
    else if (!strcmp(JsonGetStr(part, "Mesh/MeshType"), "curvilinear"))
        write_quad_mesh_part(dbfile, part, DB_NONCOLLINEAR);
    else if (!strcmp(JsonGetStr(part, "Mesh/MeshType"), "ucdzoo"))
        write_ucdzoo_mesh_part(dbfile, part, "ucdzoo");
    else if (!strcmp(JsonGetStr(part, "Mesh/MeshType"), "arbitrary"))
        write_ucdzoo_mesh_part(dbfile, part, "arbitrary");
}

static void WriteMultiXXXObjects(json_object *main_obj, DBfile *siloFile, int dumpn, MACSIO_MIF_baton_t *bat)
{
    int i, j;
    char const *file_ext = JsonGetStr(main_obj, "clargs/fileext");
    char const *file_base = JsonGetStr(main_obj, "clargs/filebase");
    int numChunks = JsonGetInt(main_obj, "problem/global/TotalParts");
    char **blockNames = (char **) malloc(numChunks * sizeof(char*));
    int *blockTypes = (int *) malloc(numChunks * sizeof(int));
    int mblockType, vblockType;

    if (!strcmp(JsonGetStr(main_obj, "problem/parts",0,"Mesh/MeshType"), "rectilinear"))
    {
        mblockType = DB_QUADMESH;
        vblockType = DB_QUADVAR;
    }
    else if (!strcmp(JsonGetStr(main_obj, "problem/parts",0,"Mesh/MeshType"), "curvilinear"))
    {
        mblockType = DB_QUADMESH;
        vblockType = DB_QUADVAR;
    }
    else if (!strcmp(JsonGetStr(main_obj, "problem/parts",0,"Mesh/MeshType"), "ucdzoo"))
    {
        mblockType = DB_UCDMESH;
        vblockType = DB_UCDVAR;
    }
    else if (!strcmp(JsonGetStr(main_obj, "problem/parts",0,"Mesh/MeshType"), "arbitrary"))
    {
        mblockType = DB_UCDMESH;
        vblockType = DB_UCDVAR;
    }

    /* Go to root directory in the silo file */
    DBSetDir(siloFile, "/");

    /* Construct the lists of individual object names */
    for (i = 0; i < numChunks; i++)
    {
        int rank_owning_chunk = MACSIO_DATA_GetRankOwningPart(main_obj, i);
        int groupRank = MACSIO_MIF_RankOfGroup(bat, rank_owning_chunk);
        blockNames[i] = (char *) malloc(1024);
        if (groupRank == 0)
        {
            /* this mesh block is in the file 'root' owns */
            sprintf(blockNames[i], "/domain_%07d/mesh", i);
        }
        else
        {
//#warning USE SILO NAMESCHEMES INSTEAD
            sprintf(blockNames[i], "%s_silo_%05d_%03d.%s:/domain_%07d/mesh",
                JsonGetStr(main_obj, "clargs/filebase"),
                groupRank, dumpn,
                JsonGetStr(main_obj, "clargs/fileext"),
                i);
        }
        blockTypes[i] = mblockType ;
    }

    /* Write the multi-block objects */
    DBPutMultimesh(siloFile, "multi_mesh", numChunks, blockNames, blockTypes, 0);

    json_object *first_part = JsonGetObj(main_obj, "problem/parts", 0);
    json_object *vars_array = JsonGetObj(first_part, "Vars");
    int numVars = json_object_array_length(vars_array);
    for (j = 0; j < numVars; j++)
    {
        for (i = 0; i < numChunks; i++)
        {
            int rank_owning_chunk = MACSIO_DATA_GetRankOwningPart(main_obj, i);
            int groupRank = MACSIO_MIF_RankOfGroup(bat, rank_owning_chunk);
            if (groupRank == 0)
            {
                /* this mesh block is in the file 'root' owns */
                sprintf(blockNames[i], "/domain_%07d/%s", i,
                    JsonGetStr(vars_array, "", j, "name"));
            }
            else
            {
//#warning USE SILO NAMESCHEMES INSTEAD
                sprintf(blockNames[i], "%s_silo_%05d_%03d.%s:/domain_%07d/%s",
                    JsonGetStr(main_obj, "clargs/filebase"),
                    groupRank,
                    dumpn,
                    JsonGetStr(main_obj, "clargs/fileext"),
                    i,
                    JsonGetStr(vars_array, "", j, "name"));
            }
            blockTypes[i] = vblockType;
        }

        /* Write the multi-block objects */
        DBPutMultivar(siloFile, JsonGetStr(vars_array, "", j, "name"), numChunks, blockNames, blockTypes, 0);

//#warning WRITE MULTIBLOCK DOMAIN ASSIGNMENT AS A TINY QUADMESH OF SAME PHYSICAL SIZE OF MESH BUT FEWER ZONES

    }

    /* Clean up */
    for (i = 0; i < numChunks; i++)
        free(blockNames[i]);
    free(blockNames);
    free(blockTypes);
}

static void WriteDecompMesh(json_object *main_obj, DBfile *siloFile, int dumpn, MACSIO_MIF_baton_t *bat)
{
    int i, ndims = JsonGetInt(main_obj, "clargs/part_dim");
    int total_parts = JsonGetInt(main_obj, "problem/global/TotalParts");
    int zdims[3] = {0, 0, 0}, dims[3] = {0, 0, 0};
    double bounds[6] = {0, 0, 0, 0, 0, 0};
    char const *coordnames[] = {"X","Y","Z"};
    double *coords[3];
    double *color = (double *) malloc(total_parts * sizeof(double));

    if (ndims >= 1)
    {
        double *x, xdelta;
        dims[0] = JsonGetInt(main_obj, "problem/global/PartsLogDims/0")+1;
        bounds[0] = JsonGetInt(main_obj, "problem/global/Bounds/0");
        bounds[3] = JsonGetInt(main_obj, "problem/global/Bounds/3");
        x = (double*) malloc(dims[0] * sizeof(double));
        xdelta = MACSIO_UTILS_XDelta(dims, bounds);
        for (i = 0; i < dims[0]; i++)
            x[i] = bounds[0] + i * xdelta;
        coords[0] = x;
    }

    if (ndims >= 2)
    {
        double *y, ydelta;
        dims[1] = JsonGetInt(main_obj, "problem/global/PartsLogDims/1")+1;
        bounds[1] = JsonGetInt(main_obj, "problem/global/Bounds/1");
        bounds[4] = JsonGetInt(main_obj, "problem/global/Bounds/4");
        y = (double*) malloc(dims[1] * sizeof(double));
        ydelta = MACSIO_UTILS_YDelta(dims, bounds);
        for (i = 0; i < dims[1]; i++)
            y[i] = bounds[1] + i * ydelta;
        coords[1] = y;
    }

    if (ndims >= 3)
    {
        double *z, zdelta;
        dims[2] = JsonGetInt(main_obj, "problem/global/PartsLogDims/2")+1;
        bounds[2] = JsonGetInt(main_obj, "problem/global/Bounds/2");
        bounds[5] = JsonGetInt(main_obj, "problem/global/Bounds/5");
        z = (double*) malloc(dims[2] * sizeof(double));
        zdelta = MACSIO_UTILS_ZDelta(dims, bounds);
        for (i = 0; i < dims[2]; i++)
            z[i] = bounds[2] + i * zdelta;
        coords[2] = z;
    }

    DBPutQuadmesh(siloFile, "part_mesh", (char**) coordnames, coords,
        dims, ndims, DB_DOUBLE, DB_COLLINEAR, 0);
    
    for (i = 0; i < total_parts; i++)
        color[i] = MACSIO_DATA_GetRankOwningPart(main_obj, i);
    for (i = 0; i < ndims; i++)
        zdims[i] = dims[i]-1;

    DBPutQuadvar1(siloFile, "decomp", "part_mesh", color,
        zdims, ndims, NULL, 0, DB_DOUBLE, DB_ZONECENT, 0);
}

//#warning HOW IS A NEW DUMP CLASS HANDLED
//#warning ADD TIMING LOGIC
static void main_dump(int argi, int argc, char **argv, json_object *main_obj, int dumpn, double dumpt)
{
    DBfile *siloFile;
    int numGroups = -1;
    int rank, size;
    char fileName[256];
    MACSIO_MIF_baton_t *bat;
    MACSIO_MIF_ioFlags_t ioFlags = {MACSIO_MIF_WRITE,
        JsonGetInt(main_obj, "clargs/exercise_scr")&0x1};

    /* Without this barrier, I get strange behavior with Silo's MACSIO_MIF interface */
//#warning CONFIRM THIS IS STILL NEEDED
#ifdef HAVE_MPI
    mpi_errno = MPI_Barrier(MACSIO_MAIN_Comm);
#endif

    /* process cl args */
    process_args(argi, argc, argv);

    rank = JsonGetInt(main_obj, "parallel/mpi_rank");
    size = JsonGetInt(main_obj, "parallel/mpi_size");

//#warning MOVE TO A FUNCTION
    /* ensure we're in MIF mode and determine the file count */
    json_object *parfmode_obj = JsonGetObj(main_obj, "clargs/parallel_file_mode");
    if (parfmode_obj)
    {
        json_object *modestr = json_object_array_get_idx(parfmode_obj, 0);
        json_object *filecnt = json_object_array_get_idx(parfmode_obj, 1);
        if (modestr && !strcmp(json_object_get_string(modestr), "SIF"))
        {
            MACSIO_LOG_MSG(Die, ("Silo plugin doesn't support SIF mode"));
        }
        else if (modestr && strcmp(json_object_get_string(modestr), "MIF"))
        {
            MACSIO_LOG_MSG(Warn, ("Ignoring non-standard MIF mode"));
        }
        if (filecnt)
            numGroups = json_object_get_int(filecnt);
        else
            numGroups = size;
    }
    else
    {
        char const * modestr = JsonGetStr(main_obj, "clargs/parallel_file_mode");
        if (!strcmp(modestr, "SIF"))
        {
            MACSIO_LOG_MSG(Die, ("Silo plugin doesn't support SIF mode"));
        }
        else if (!strcmp(modestr, "MIFMAX"))
            numGroups = JsonGetInt(main_obj, "parallel/mpi_size");
        else if (!strcmp(modestr, "MIFAUTO"))
        {
            /* Call utility to determine optimal file count */
//#warning ADD UTILIT TO DETERMINE OPTIMAL FILE COUNT
        }
    }

    /* Initialize MACSIO_MIF, pass a pointer to the driver type as the user data. */
    bat = MACSIO_MIF_Init(numGroups, ioFlags, MACSIO_MAIN_Comm, 1,
        CreateSiloFile, OpenSiloFile, CloseSiloFile, &driver);

    /* Construct name for the silo file */
//#warning CHANGE NAMING SCHEME SO LS WORKS BETTER
    sprintf(fileName, "%s_silo_%05d_%03d.%s",
        JsonGetStr(main_obj, "clargs/filebase"),
        MACSIO_MIF_RankOfGroup(bat, rank),
        dumpn,
        JsonGetStr(main_obj, "clargs/fileext"));

    MACSIO_UTILS_RecordOutputFiles(dumpn, fileName);

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
    {
        WriteMultiXXXObjects(main_obj, siloFile, dumpn, bat);

//#warning DECOMP MESH SHOULDN'T BE INCLUDED IN PERFORMANCE NUMBERS
        /* output a top-level quadmesh and vars to indicate processor decomp */
        if (MACSIO_LOG_DebugLevel >= 2)
        {
            WriteDecompMesh(main_obj, siloFile, dumpn, bat);
        }
    }

    /* Hand off the baton to the next processor. This winds up closing
     * the file so that the next processor that opens it can be assured
     * of getting a consistent and up to date view of the file's contents. */
    MACSIO_MIF_HandOffBaton(bat, siloFile);

    /* We're done using MACSIO_MIF, so finish it off */
    MACSIO_MIF_Finish(bat);
}

//#warning TO BE MOVED TO SILO LIBRARY
static void DBSplitMultiName(char *mname, char **file, char **dir, char **obj)
{
    char *_file, *_dir, *_obj;
    char *colon = strchr(mname, ':');
    char *lastslash = strrchr(mname, '/');

    /* Split the incoming string by inserting null chars */
    if (colon)     *colon     = '\0';
    if (lastslash) *lastslash = '\0';

    if (colon)
    {
        if (file) *file = mname;
        if (lastslash)
        {
            if (dir) *dir = colon+1;
            if (obj) *obj = lastslash+1;
        }
        else
        {
            if (dir) *dir = 0;
            if (obj) *obj = colon+1;
        }
    }
    else
    {
        if (file) *file = 0;
        if (lastslash)
        {
            if (dir) *dir = mname;
            if (obj) *obj = lastslash+1;
        }
        else
        {
            if (dir) *dir = 0;
            if (obj) *obj = mname;
        }
    }
}

//#warning TO BE MOVED TO SILO LIBRARY
static char const *
DBGetFilename(DBfile const *f)
{
    return f->pub.name;
}

//#warning SHOULD USE MACSIO_MIF FOR READ TOO BUT INTERFACE IS LACKING
static
void main_load(int argi, int argc, char **argv, char const *path, json_object *main_obj, json_object **data_read_obj)
{
    int my_rank = JsonGetInt(main_obj, "parallel/mpi_rank");
    int mpi_size = JsonGetInt(main_obj, "parallel/mpi_rank");
    int i, num_parts, my_part_cnt, use_ns = 0, maxlen = 0, bcast_data[3];
    char *all_meshnames, *my_meshnames;
    char *var_names_list = strdup(JsonGetStr(main_obj, "clargs/read_vars"));
    char *var_names_list_orig = var_names_list;
    char *vname;
    int *my_part_ids, *all_part_cnts;
    DBfile *partFile = 0;
    int silo_driver = DB_UNKNOWN;

    /* Open the root file */
    if (my_rank == 0)
    {
        DBfile *rootFile = DBOpen(path, DB_UNKNOWN, DB_READ);
        DBmultimesh *mm = DBGetMultimesh(rootFile, JsonGetStr(main_obj, "clargs/read_mesh"));

        /* Examine multimesh for count of mesh pieces and count of files */
        num_parts = mm->nblocks;
        use_ns = mm->block_ns ? 1 : 0;

        /* Reformat all the meshname strings to a single, long buffer */
        if (!use_ns)
        {
            int i;
            for (i = 0; i < num_parts; i++)
            {
                int len = strlen(mm->meshnames[i]);
                if (len > maxlen) maxlen = len;
            }
            maxlen++; /* for nul char */
            all_meshnames = (char *) calloc(num_parts * maxlen, sizeof(char));
            for (i = 0; i < num_parts; i++)
                strcpy(&all_meshnames[i*maxlen], mm->meshnames[i]);
        }

        bcast_data[0] = num_parts;
        bcast_data[1] = use_ns;
        bcast_data[2] = maxlen;

        DBClose(rootFile);
    }
#ifdef HAVE_MPI
    MPI_Bcast(bcast_data, NARRVALS(bcast_data), MPI_INT, 0, MACSIO_MAIN_Comm);
#endif
    num_parts = bcast_data[0];
    use_ns    = bcast_data[1];
    maxlen    = bcast_data[2];

#if 0
    MACSIO_DATA_SimpleAssignKPartsToNProcs(num_parts, mpi_size, my_rank, &all_part_cnts,
        &my_part_cnt, &my_part_ids);
#endif

    my_meshnames = (char *) calloc(my_part_cnt * maxlen, sizeof(char));

    if (use_ns)
    {
    }
#ifdef HAVE_MPI
    else
    { 
        int *displs = 0;

        if (my_rank == 0)
        {
            displs = (int *) malloc(num_parts * sizeof(int));
            displs[0] = 0;
            for (i = 1; i < num_parts; i++)
               displs[i] = displs[i-1] + all_part_cnts[i-1] * maxlen;
        }

        /* MPI_scatter the block names or the external arrays for any namescheme */
        MPI_Scatterv(all_meshnames, all_part_cnts, displs, MPI_CHAR,
                 my_meshnames, my_part_cnt, MPI_CHAR, 0, MACSIO_MAIN_Comm);

        if (my_rank == 0)
            free(displs);
    }
#endif

    /* Iterate finding correct file/dir combo and reading mesh pieces and variables */
    for (i = 0; i < my_part_cnt; i++)
    {
        char *partFileName, *partDirName, *partObjName;
        DBObjectType silo_objtype;

        DBSplitMultiName(&my_meshnames[i*maxlen], &partFileName, &partDirName, &partObjName);

        /* if filename is different from current, close current */
        if (partFile && strcmp(DBGetFilename(partFile), partFileName))
            DBClose(partFile);

        /* Open the file containing this part */
        partFile = DBOpen(partFileName, silo_driver, DB_READ);
        silo_driver = DBGetDriverType(partFile);

        DBSetDir(partFile, partDirName);

        /* Get the mesh part */
        silo_objtype = (DBObjectType) DBInqMeshtype(partFile, partObjName);

        switch (silo_objtype)
        {
            case DB_QUADRECT:
            case DB_QUADCURV:
            case DB_QUADMESH:
            {
                DBquadmesh *qm = DBGetQuadmesh(partFile, partObjName);
                break;
            }
            case DB_UCDMESH:
            {
                break;
            }
            case DB_POINTMESH:
            {
                break;
            }
            default:
            {
            }
        }

        while (vname = strsep(&var_names_list, ", "))
        {
            DBObjectType silo_vartype = DBInqVarType(partFile, vname);
            switch (silo_vartype)
            {
                case DB_QUADVAR:
                {
                    DBquadvar *qv = DBGetQuadvar(partFile, vname);
                    break;
                }
                case DB_POINTVAR:
                {
                    break;
                }
                case DB_UCDVAR:
                {
                    break;
                }
                default: continue;
            }
        }
        free(var_names_list_orig);

        /* Add the mesh part to the returned json object */

        DBSetDir(partFile, "/");
    }

    free(my_meshnames);
    if (my_rank == 0)
        free(all_meshnames);
}

static int register_this_interface()
{
    MACSIO_IFACE_Handle_t iface;

    if (strlen(iface_name) >= MACSIO_IFACE_MAX_NAME)
        MACSIO_LOG_MSG(Die, ("Interface name \"%s\" too long",iface_name));

    /* Take this slot in the map */
    strcpy(iface.name, iface_name);
    strcpy(iface.ext, iface_ext);

    /* Must define at least these two methods */
    iface.dumpFunc = main_dump;
    iface.loadFunc = main_load;
    iface.processArgsFunc = process_args;

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
