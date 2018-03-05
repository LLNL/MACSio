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

#include <adios.h>

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

}

typedef struct _user_data {
    int64_t groupId;
    const char *groupName;
} user_data_t;

static void *CreateADIOSFile(const char *fname, const char *nsname, void *userData)
{
    int64_t *retval = 0;
    int64_t adios_file = 0;
    
    user_data_t *ud = (user_data_t *) userData;
    adios_open(&adios_file, "domain", fname, "w", MPI_COMM_SELF);

    retval = (int64_t *) malloc(sizeof(int64_t));
    *retval = adios_file;

    return (void *) retval;
}

static void *OpenADIOSFile(const char *fname, const char *nsname,
                   MACSIO_MIF_ioFlags_t ioFlags, void *userData)
{
    int64_t *retval = 0;
    int64_t adios_file = 0;
    int64_t domain_group_id;
   
    user_data_t *ud = (user_data_t *) userData;
    adios_open(&adios_file, "domain", fname, "u", MPI_COMM_SELF);
        
    retval = (int64_t *) malloc(sizeof(int64_t));
    *retval = adios_file;

    return (void *) retval;
}

static void CloseADIOSFile(void *file, void *userData)
{
    int64_t adiosFile = *((int64_t*)file);
    adios_close(adiosFile);
}

/* To accomodate the no XML adios calls we need to define the adios file structure before opening.
 * This messes with the way MACSio does baton passing for file control so we need somewhere else to do it
 */
static void declare_adios_structure(int64_t *domain_group_id, json_object *main_obj, void *userData)
{
    /* calculate a maximum buffer size based on the mesh part_size */

    adios_set_max_buffer_size(json_object_path_get_int(main_obj, "clargs/part_size")/1048576 + 2);
    adios_declare_group (domain_group_id, "domain", "", adios_stat_default);
    adios_select_method(*domain_group_id, "MPI", "", "");

    char * dimensions = "nx,ny";
    int nx = JsonGetInt(main_obj,"problem/global/LogDims",0);
    int ny = JsonGetInt(main_obj,"problem/global/LogDims",1);
   
    adios_define_var(*domain_group_id, "nx", "", adios_integer, "","","");
    adios_define_var(*domain_group_id, "ny", "", adios_integer, "","","");
    adios_define_var(*domain_group_id, "XAxisCoords", "" , adios_double, "nx", "", "");
    adios_define_var(*domain_group_id, "YAxisCoords", "", adios_double, "ny", "", "");
    
    adios_define_mesh_rectilinear (dimensions, "XAxisCoords,YAxisCoords", "2", *domain_group_id, "rectilinearmesh");
    adios_define_mesh_timevarying("no", *domain_group_id, "rectilinearmesh");
    adios_define_var_mesh(*domain_group_id, "data", "rectilinearmesh");
    adios_define_var_centering(*domain_group_id, "data", "point");
    
    user_data_t *ud = (user_data_t *) userData;
    ud->groupId = *domain_group_id;
    ud->groupName = "domain";

}

static void write_quad_mesh_part(int64_t adiosfile, json_object *part, char *adios_mesh_type)
{
    json_object *coordobj;
    char const *coordnames[] = {"X","Y","Z"};
    void const *coords[3];
    int ndims = JsonGetInt(part, "Mesh/GeomDim");
    int dims[3] = {1,1,1};
    int dimsz[3] = {1,1,1};

    if (!strcmp(adios_mesh_type,"rectilinear"))	
        coordobj = JsonGetObj(part, "Mesh/Coords/XAxisCoords");
    else
        coordobj = JsonGetObj(part, "Mesh/Coords/XCoords");
    coords[0] = json_object_extarr_data(coordobj);
    dims[0] = JsonGetInt(part, "Mesh/LogDims", 0);
    dimsz[0] = dims[0]-1;
    if (ndims > 1)
    {
        if (!strcmp(adios_mesh_type,"rectilinear"))	
            coordobj = JsonGetObj(part, "Mesh/Coords/YAxisCoords");
        else
            coordobj = JsonGetObj(part, "Mesh/Coords/YCoords");
        coords[1] = json_object_extarr_data(coordobj);
        dims[1] = JsonGetInt(part, "Mesh/LogDims", 1);
        dimsz[1] = dims[1]-1;
    }
    if (ndims > 2)
    {
        if (!strcmp(adios_mesh_type,"rectilinear"))	
            coordobj = JsonGetObj(part, "Mesh/Coords/ZAxisCoords");
        else
            coordobj = JsonGetObj(part, "Mesh/Coords/ZCoords");
        coords[2] = json_object_extarr_data(coordobj);
        dims[2] = JsonGetInt(part, "Mesh/LogDims", 2);
        dimsz[2] = dims[2]-1;
    }

    adios_write(adiosfile, "nx", &dims[0]);
    adios_write(adiosfile, "XAxisCoords", coords[0]);

    if (ndims > 1)
    {
        adios_write(adiosfile, "ny", &dims[1]);
        adios_write(adiosfile, "YAxisCoords", coords[1]);
    }

    /*
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
    */
}

static void write_mesh_part(int64_t adios_loc, json_object *part)
{
    if (!strcmp(JsonGetStr(part, "Mesh/MeshType"), "rectilinear"))
        write_quad_mesh_part(adios_loc, part, "rectilinear");
    else if (!strcmp(JsonGetStr(part, "Mesh/MeshType"), "curvilinear"))
        write_quad_mesh_part(adios_loc, part, "curvilinear");
//    else if (!strcmp(JsonGetStr(part, "Mesh/MeshType"), "ucdzoo"))
//        write_ucdzoo_mesh_part(adios_loc, part, "ucdzoo");
//    else if (!strcmp(JsonGetStr(part, "Mesh/MeshType"), "arbitrary"))
//        write_ucdzoo_mesh_part(adios_loc, part, "arbitrary");
}

static void main_dump_mif(json_object *main_obj, int numFiles, int dumpn, double dumpt)
{
    int size, rank;
    int64_t *adiosFile_ptr;
    int64_t adiosFile;
    char fileName[256];
    int i, len;
    int *theData;
    user_data_t userData;

    MACSIO_MIF_ioFlags_t ioFlags = {MACSIO_MIF_WRITE};

    MACSIO_MIF_baton_t *bat = MACSIO_MIF_Init(numFiles, ioFlags, MACSIO_MAIN_Comm, 3,
        CreateADIOSFile, OpenADIOSFile, CloseADIOSFile, &userData);

    rank = json_object_path_get_int(main_obj, "parallel/mpi_rank");
    size = json_object_path_get_int(main_obj, "parallel/mpi_size");

    /* Construct name for the silo file */
    sprintf(fileName, "%s_adios_%05d_%03d.%s",
        json_object_path_get_string(main_obj, "clargs/filebase"),
        MACSIO_MIF_RankOfGroup(bat, rank),
        dumpn,
        iface_ext);

    int64_t domain_group_id;
    declare_adios_structure(&domain_group_id, main_obj, &userData);

    adiosFile_ptr = (int64_t *) MACSIO_MIF_WaitForBaton(bat, fileName, 0);
    adiosFile = *adiosFile_ptr;

    json_object *parts = json_object_path_get_array(main_obj, "problem/parts");

    
    for (int i = 0; i < json_object_array_length(parts); i++)
    {
        char domain_dir[256];
        json_object *this_part = json_object_array_get_idx(parts, i);

        snprintf(domain_dir, sizeof(domain_dir), "domain_%07d",
            json_object_path_get_int(this_part, "Mesh/ChunkID"));

	write_mesh_part(adiosFile, this_part);
    }

    /* Hand off the baton to the next processor. This winds up closing
     * the file so that the next processor that opens it can be assured
     * of getting a consistent and up to date view of the file's contents. */
    MACSIO_MIF_HandOffBaton(bat, &adiosFile);

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
    
    /* initialise adios using the global communicator */
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

    adios_finalize(MACSIO_MAIN_Rank);
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
