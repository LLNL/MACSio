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

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <json-cwx/json.h>

#include <macsio_clargs.h>
#include <macsio_data.h>
#include <macsio_iface.h>
#include <macsio_log.h>
#include <macsio_main.h>
#include <macsio_mif.h>
#include <macsio_utils.h>

#include <exodusII.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

#define MAX_STRING_LEN 128

/*!
\addtogroup plugins
@{
*/

/*!
\addtogroup Exodus
@{
*/

/* the name you want to assign to the interface */
static char const *iface_name = "exodus";
static char const *iface_ext = "exoII";
static char *cpu_word_size = "double";
static char io_word_size[32];
static int use_nemesis = 0;
static char use_large_model[32];

typedef struct _ex_global_init_params {
    int file_creation_flags;
    int cpu_word_size;
    int io_word_size;
    int num_dim;
    int num_nodes;
    int num_elems;
    int num_elem_block;
    int num_node_sets;
    int num_side_sets;

    /* not really global initialization state but is useful to include
       in this struct to facilitate file open vs. create */
    int dumpn;
    double dumpt;
} ex_global_init_params_t;

static int process_args(int argi, int argc, char *argv[])
{
    const MACSIO_CLARGS_ArgvFlags_t argFlags = {
        MACSIO_CLARGS_WARN,
        MACSIO_CLARGS_TOMEM,
        MACSIO_CLARGS_ASSIGN_ON};
    char *io_word_size_ptr = &io_word_size[0];
    char *use_large_model_ptr = &use_large_model[0];

    MACSIO_CLARGS_ProcessCmdline(0, argFlags, argi, argc, argv,
        "--io_word_size %s", "double",
            "Specify precision of data that lands in exodus file as either \"float\" or \"double\"",
            &io_word_size_ptr,
        "--use_nemesis", "",
            "Output nemesis stuff too",
            &use_nemesis,
        "--use_large_model %s", "auto",
            "Use EX_LARGE_MODEL: specify one of \"always\", \"auto\", \"never\"",
            &use_large_model_ptr,
    MACSIO_CLARGS_END_OF_ARGS);

    return 0;
}

/*!
\brief MIF Create callback

This is a *create* callback. However, Exodus expects additional timesteps to be
written to the same file(s) as the time zero data went into (unless there are changes
in mesh connectivity/topology. So, this callback really only does file *creation*
during time-zero. Thereafter, it only does opens. We put an additional field into
ex_global_init_params (passed in here as userData), dumpn, to detect when
this callback is called for time zero or not.
*/
static void *CreateExodusFile(const char *fname, const char *nsname, void *userData)
{
    ex_global_init_params_t *params = (ex_global_init_params_t *) userData;
    int exoid;

    if (params->dumpn)
    {
        float version;
        int *exoid_ptr;

        exoid = ex_open(fname, EX_WRITE, &(params->cpu_word_size), &(params->io_word_size), &version);

        if (exoid >= 0)
        {
            int exo_err = ex_put_time(exoid, params->dumpn+1, &(params->dumpt));
            if (exo_err == 0)
            {
                exoid_ptr = (int *) malloc(sizeof(int));
                *exoid_ptr = exoid;
                return exoid_ptr;
            }
        }

        return 0;
    }

    exoid = ex_create(fname, params->file_creation_flags,
                &(params->cpu_word_size), &(params->io_word_size));

    if (exoid >= 0)
    {
        int exo_err;
        int *exoid_ptr = (int *) malloc(sizeof(int));
        *exoid_ptr = exoid;

        exo_err = ex_put_init(exoid, "MACSio performance test of Exodus library",
            params->num_dim, params->num_nodes, params->num_elems, params->num_elem_block,
            params->num_node_sets, params->num_side_sets);

        if (exo_err == 0)
            exo_err = ex_put_time(exoid, params->dumpn+1, &(params->dumpt));

        if (exo_err == 0)
            return exoid_ptr;

        return 0;
    }

    return 0;
}

//#warning HOW DO WE WRITE NEW TIMES TO SAME FILES WITH MACSIO_MIF INTERFACE?
static void *OpenExodusFile(const char *fname, const char *nsname, MACSIO_MIF_ioFlags_t ioFlags,
    void *userData)
{
    static const int OpenExodusFile_should_never_be_called = 1;
    assert(!OpenExodusFile_should_never_be_called);
}

static int CloseExodusFile(void *file, void *userData)
{
    int retval = 0;
    int exoid = *((int*) file);
    if (exoid >= 0)
        retval = ex_close(exoid);
    free(file);
    return retval;
}

static void get_exodus_global_init_params(json_object *main_obj, int dumpn, double dumpt,
    ex_global_init_params_t* params)
{
    int i;
    long long num_nodes = 0, num_elems = 0;
    long long const large_model = (1LL << 31); /* 2 Gigabytes */
    int num_dumps = JsonGetInt(main_obj, "clargs/num_dumps");
    json_object *parts = JsonGetObj(main_obj, "problem/parts");

    params->num_elem_block = json_object_array_length(parts);
    params->num_dim = JsonGetInt(parts, "0/Mesh/GeomDim");

    for (i = 0; i < params->num_elem_block; i++)
    {
        json_object *part = JsonGetObj(parts, "", i);

        int nnodes = 1, nzones = 1;
        int nodes[3], zones[3];

        nodes[0] = JsonGetInt(part, "Mesh/LogDims", 0);
        nnodes *= nodes[0];
        zones[0] = nodes[0]-1;
        nzones *= zones[0];
        if (params->num_dim > 1)
        {
            nodes[1] = JsonGetInt(part, "Mesh/LogDims", 1);
            nnodes *= nodes[1];
            zones[1] = nodes[1]-1;
            nzones *= zones[1];
        }
        if (params->num_dim > 2)
        {
            nodes[2] = JsonGetInt(part, "Mesh/LogDims", 2);
            nnodes *= nodes[2];
            zones[2] = nodes[2]-1;
            nzones *= zones[2];
        }

        num_nodes += nnodes;
        num_elems += nzones;
    }

    /* word sizes */
    if      (!strncasecmp(cpu_word_size, "double", 6))
        params->cpu_word_size = sizeof(double);
    else if (!strncasecmp(cpu_word_size, "float", 5))
        params->cpu_word_size = sizeof(float);

    if      (!strncasecmp(io_word_size, "double", 6))
        params->io_word_size = sizeof(double);
    else if (!strncasecmp(io_word_size, "float", 5))
        params->io_word_size = sizeof(float);

    /* File creation flags */
    params->file_creation_flags = EX_CLOBBER | EX_NOSHARE;
    if (!strncasecmp(use_large_model, "always", 6))
        params->file_creation_flags |= EX_LARGE_MODEL;
    else if (!strncasecmp(use_large_model, "never", 6))
        params->file_creation_flags |= EX_NORMAL_MODEL;
    else if (!strncasecmp(use_large_model, "auto", 4))
    {
        long long dataset_size = (long long) num_dumps * num_nodes * (long long) params->io_word_size;
        if (dataset_size > large_model)
        {
            params->file_creation_flags |= EX_LARGE_MODEL;
            MACSIO_LOG_MSG(Warn, ("Auto size estimate %lld requires EX_LARGE_MODEL", dataset_size));
        }
        else
            params->file_creation_flags |= EX_NORMAL_MODEL;
    }

    params->num_elems = (int) num_elems;
    params->num_nodes = (int) num_nodes;
    if ((long long) params->num_nodes != num_nodes)
        MACSIO_LOG_MSG(Warn, ("num_nodes %lld too large for int", num_nodes));

    params->num_node_sets = 0;
    params->num_side_sets = 0;

    /* this is needed to facilitate MIF create callback behavior */
    params->dumpn = dumpn;
    params->dumpt = dumpt;
}

static void write_rect_mesh_coords_all_parts(int exoid, ex_global_init_params_t const *params,
    json_object *parts, int **elem_block_coord_offsets)
{
    int p, exo_err, n = 0;
//#warning FIX BUG IN JSON-C LIB FOR THIS CALL
#if 0
    int num_parts = JsonGetInt(parts,"");
#else
    int num_parts = json_object_array_length(parts);
#endif
    int *coord_offsets = (int *) malloc(num_parts * sizeof(int));
    double *exo_x_coords = 0, *exo_y_coords = 0, *exo_z_coords = 0;

    exo_x_coords = (double *) malloc(params->num_nodes * sizeof(double));
    if (params->num_dim > 1)
        exo_y_coords = (double *) malloc(params->num_nodes * sizeof(double));
    if (params->num_dim > 2)
        exo_z_coords = (double *) malloc(params->num_nodes * sizeof(double));

//#warning WE ARE DUPING COORDS ON PART BOUNDARIES AND WE SHOULD NOT BE
    for (p = 0; p < num_parts; p++)
    {
        int i, j, k, dims[3] = {1,1,1};
        double *coords[3];
        json_object *coordobj;
        json_object *part = JsonGetObj(parts, "", p);

        coordobj = JsonGetObj(part, "Mesh/Coords/XAxisCoords");
        coords[0] = (double*) json_object_extarr_data(coordobj);
        dims[0] = JsonGetInt(part, "Mesh/LogDims", 0);
        if (params->num_dim > 1)
        {
            coordobj = JsonGetObj(part, "Mesh/Coords/YAxisCoords");
            coords[1] = (double*) json_object_extarr_data(coordobj);
            dims[1] = JsonGetInt(part, "Mesh/LogDims", 1);
        }
        if (params->num_dim > 2)
        {
            coordobj = JsonGetObj(part, "Mesh/Coords/ZAxisCoords");
            coords[2] = (double*) json_object_extarr_data(coordobj);
            dims[2] = JsonGetInt(part, "Mesh/LogDims", 2);
        }
   
        coord_offsets[p] = n;

        for (i = 0; i < dims[0]; i++)
        {
            for (j = 0; j < dims[1]; j++)
            {
                for (k = 0; k < dims[2]; k++, n++)
                {
                    exo_x_coords[n] = coords[0][i];
                    if (params->num_dim > 1)
                    {
                        exo_y_coords[n] = coords[1][j];
                        if (params->num_dim > 2)
                            exo_z_coords[n] = coords[2][k];
                    }
                }
            }
        }
    }

    exo_err = ex_put_coord(exoid, exo_x_coords, exo_y_coords, exo_z_coords);

    if (exo_x_coords) free(exo_x_coords);
    if (exo_y_coords) free(exo_y_coords);
    if (exo_z_coords) free(exo_z_coords);

    *elem_block_coord_offsets = coord_offsets;
}

static void write_mesh_coords_all_parts(int exoid, ex_global_init_params_t const *params,
    json_object *parts, int **elem_block_coord_offsets)
{
    if (!strcmp(JsonGetStr(parts, "0/Mesh/MeshType"), "rectilinear"))
        write_rect_mesh_coords_all_parts(exoid, params, parts, elem_block_coord_offsets);
}

/* Write the different mesh parts as different element blocks over a
   single set of coordinates */
static void write_mesh_part_blocks_and_vars(int exoid, ex_global_init_params_t const *params,
    json_object *part, int coord_offset, int dumpn, double dumpt)
{
    int i,j;
    int num_elems_in_dim[3] = {1,1,1};
    int elem_block_id = JsonGetInt(part, "Mesh/ChunkID")+1;
    int nodes_per_elem = params->num_dim==2?4:8;
    int num_elems_in_block = JsonGetInt(part, "Mesh/LogDims", 0)-1;
    json_object *vars_array = JsonGetObj(part, "Vars");
    int *connect;

    num_elems_in_dim[0] = JsonGetInt(part, "Mesh/LogDims", 0)-1;
    if (params->num_dim > 1)
    {
        num_elems_in_dim[1] = JsonGetInt(part, "Mesh/LogDims", 1)-1;
        num_elems_in_block *= (JsonGetInt(part, "Mesh/LogDims", 1)-1);
    }
    if (params->num_dim > 2)
    {
        num_elems_in_dim[2] = JsonGetInt(part, "Mesh/LogDims", 2)-1;
        num_elems_in_block *= (JsonGetInt(part, "Mesh/LogDims", 2)-1);
    }

    ex_put_elem_block(exoid, elem_block_id, params->num_dim==2?"QUAD":"HEX",
        num_elems_in_block, nodes_per_elem, 0);

//#warning ONLY NEED TO DO CONNECT STUFF ONCE
    connect = (int *) malloc(nodes_per_elem * num_elems_in_block * sizeof(int));

    for (i = 0; i < num_elems_in_block; i++)
        for (j = 0; j < nodes_per_elem; j++)
            connect[i*nodes_per_elem+j] =
                i/(num_elems_in_dim[0]*num_elems_in_dim[1]) +    /* Offset for crossing 3D slice dim boundaries */
                i/num_elems_in_dim[0] +                          /* Offset for crossing 2D row dim boundaries */
                coord_offset +                                   /* Offset for coordinates from each block */
                i +                                              /* Offset into this block of elems */
                JsonGetInt(part, "Mesh/Topology/Template", j) +  /* Offset for nodes in template elem */
                1;                                               /* Offset for 1-origin indexing */

    ex_put_elem_conn(exoid, elem_block_id, connect);
    if (connect) free(connect);
  
#if 0
    exo_err = ex_put_id_map(exoid, EX_NODE_MAP, node_map);
    exo_err = ex_put_id_map(exoid, EX_ELEM_MAP, elem_map);
#endif

    /* Output variables */
    {
        int ev = 0;
        int num_elem_vars = 0;
        char **elem_var_names = 0;

        for (i = 0; i < json_object_array_length(vars_array); i++)
        {
            json_object *varobj = json_object_array_get_idx(vars_array, i);
            if (strcmp(JsonGetStr(varobj, "centering"), "zone")) continue;
            num_elem_vars++;
        }
        if (num_elem_vars)
            elem_var_names = (char **) malloc(num_elem_vars * sizeof(char*));

        ex_put_var_param(exoid, "e", num_elem_vars);

        for (i = 0; i < json_object_array_length(vars_array); i++)
        {
            json_object *varobj = json_object_array_get_idx(vars_array, i);
            if (strcmp(JsonGetStr(varobj, "centering"), "zone")) continue;
            elem_var_names[ev] = (char *) malloc((MAX_STRING_LEN+1) * sizeof (char));
            snprintf(elem_var_names[ev], MAX_STRING_LEN, "%s", JsonGetStr(varobj, "name"));
            ev++;
        }

        if (num_elem_vars)
        {
	    ex_put_variable_names(exoid, EX_ELEM_BLOCK, num_elem_vars, elem_var_names);
            for (i = 0; i < num_elem_vars; i++)
                free(elem_var_names[i]);
            free(elem_var_names);
        }

//#warning MOVE TO MORE GLOBAL PLACE IN DUMP SEQUENCE
        ex_put_time(exoid, dumpn+1, &dumpt);

        ev = 1;
        for (i = 0; i < json_object_array_length(vars_array); i++)
        {
            json_object *varobj = json_object_array_get_idx(vars_array, i);
            json_object *dataobj = JsonGetObj(varobj, "data");
            json_extarr_type etype = json_object_extarr_type(dataobj);
            void const *dbuf = json_object_extarr_data(dataobj);
            void *vbuf = 0;

            if (strcmp(JsonGetStr(varobj, "centering"), "zone")) continue;

            if (params->cpu_word_size == sizeof(double) && etype != json_extarr_type_flt64)
                json_object_extarr_data_as_double(dataobj, (double**) &vbuf);
            else if (params->cpu_word_size == sizeof(float) && etype != json_extarr_type_flt32)
                json_object_extarr_data_as_float(dataobj, (float**) &vbuf);

	    ex_put_var(exoid, dumpn+1, EX_ELEM_BLOCK, ev++, elem_block_id, num_elems_in_block, vbuf?vbuf:dbuf);

            if (vbuf) free(vbuf);
        }
    }
}

static void WriteNemesis(json_object *main_obj, int exoid, int dumpn, MACSIO_MIF_baton_t *bat)
{
    int i, j;
    char const *file_ext = JsonGetStr(main_obj, "clargs/fileext");
    char const *file_base = JsonGetStr(main_obj, "clargs/filebase");
    int numChunks = JsonGetInt(main_obj, "problem/global/TotalParts");
    char **blockNames = (char **) malloc(numChunks * sizeof(char*));
    int *blockTypes = (int *) malloc(numChunks * sizeof(int));
}

static void WriteDecompMesh(json_object *main_obj, int exoid, int dumpn, MACSIO_MIF_baton_t *bat)
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

    for (i = 0; i < total_parts; i++)
        color[i] = MACSIO_DATA_GetRankOwningPart(main_obj, i);
    for (i = 0; i < ndims; i++)
        zdims[i] = dims[i]-1;
}

static void main_dump(int argi, int argc, char **argv, json_object *main_obj, int dumpn, double dumpt)
{
    int numGroups = -1;
    int rank, size;
    int *exoid_ptr;
    int *elem_block_coord_offsets = 0;
    char fileName[256];
    ex_global_init_params_t ex_globals;
    MACSIO_MIF_baton_t *bat;
    MACSIO_MIF_ioFlags_t ioFlags = {MACSIO_MIF_WRITE,
        JsonGetInt(main_obj, "clargs/exercise_scr")&0x1};

    /* Without this barrier, I get strange behavior with MACSIO_MIF interface */
//#warning CONFIRM THIS IS STILL NEEDED
    mpi_errno = MPI_Barrier(MACSIO_MAIN_Comm);

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
            MACSIO_LOG_MSG(Die, ("Exodus plugin doesn't support SIF mode"));
        }
        else if (modestr && strcmp(json_object_get_string(modestr), "MIFMAX"))
        {
            MACSIO_LOG_MSG(Warn, ("Exodus plugin supports only MIFMAX mode"));
        }
        numGroups = size;
    }
    else
    {
        char const * modestr = JsonGetStr(main_obj, "clargs/parallel_file_mode");
        if (!strcmp(modestr, "SIF"))
        {
            MACSIO_LOG_MSG(Die, ("Exodus plugin doesn't support SIF mode"));
        }
        else if (strcmp(modestr, "MIFMAX"))
        {
            MACSIO_LOG_MSG(Warn, ("Exodus plugin supports only MIFMAX mode"));
        }
        numGroups = size;
    }

    get_exodus_global_init_params(main_obj, dumpn, dumpt, &ex_globals);

    bat = MACSIO_MIF_Init(numGroups, ioFlags, MACSIO_MAIN_Comm, 1,
        CreateExodusFile, OpenExodusFile, CloseExodusFile, &ex_globals);

    /* Construct name for the file */
    sprintf(fileName, "%s_exodus_%05d.%s",
        JsonGetStr(main_obj, "clargs/filebase"),
        MACSIO_MIF_RankOfGroup(bat, rank),
        JsonGetStr(main_obj, "clargs/fileext"));

    MACSIO_UTILS_RecordOutputFiles(dumpn, fileName);

    /* Wait for write access to the file. All processors call this.
     * Some processors (the first in each group) return immediately
     * with write access to the file. Other processors wind up waiting
     * until they are given control by the preceeding processor in 
     * the group when that processor calls "HandOffBaton" */
    exoid_ptr = (int *) MACSIO_MIF_WaitForBaton(bat, fileName, 0);

    write_mesh_coords_all_parts(*exoid_ptr, &ex_globals,
        JsonGetObj(main_obj, "problem/parts"), &elem_block_coord_offsets);

    int numParts = JsonGetInt(main_obj, "problem/parts"); /* returns array length */
    for (int i = 0; i < numParts; i++)
    {
        json_object *this_part = JsonGetObj(main_obj, "problem/parts", i);
        write_mesh_part_blocks_and_vars(*exoid_ptr, &ex_globals, this_part, elem_block_coord_offsets[i], dumpn, dumpt);
    }
    if (elem_block_coord_offsets) free(elem_block_coord_offsets);

#if 0
    /* If this is the 'root' processor, also write Exodus's Nemesis objects */
    if (rank == 0)
    {
        WriteNemesis(main_obj, *exoid_ptr, dumpn, bat);

//#warning DECOMP MESH SHOULDN'T BE INCLUDED IN PERFORMANCE NUMBERS
        /* output a top-level quadmesh and vars to indicate processor decomp */
        if (MACSIO_LOG_DebugLevel >= 2)
        {
            WriteDecompMesh(main_obj, *exoid_ptr, dumpn, bat);
        }
    }
#endif

    /* Hand off the baton to the next processor. This winds up closing
     * the file so that the next processor that opens it can be assured
     * of getting a consistent and up to date view of the file's contents. */
    MACSIO_MIF_HandOffBaton(bat, exoid_ptr);

    /* We're done using MACSIO_MIF, so finish it off */
    MACSIO_MIF_Finish(bat);
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
