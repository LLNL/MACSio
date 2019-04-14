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

#ifdef HAVE_SILO
#include <lite_pdb.h> /* PDB Lite is part of Silo */
#else
#include <pdb.h>
#endif

/* Disable debugging messages */

/*!
\addtogroup plugins
@{
*/

/*!
\addtogroup PDB
@{
*/

/* the name you want to assign to the interface */
static char const *iface_name = "pdb";
static char const *iface_ext = "pdb";

static int process_args(int argi, int argc, char *argv[])
{
    const MACSIO_CLARGS_ArgvFlags_t argFlags = {MACSIO_CLARGS_WARN, MACSIO_CLARGS_TOMEM};

    MACSIO_CLARGS_ProcessCmdline(0, argFlags, argi, argc, argv,
    MACSIO_CLARGS_END_OF_ARGS);

    return 0;
}

static void *CreatePDBFile(const char *fname, const char *nsname, void *)
{
    PDBfile *retval = PD_create((char*)fname);
    return (void *) retval;
}

static void *OpenPDBFile(const char *fname, const char *nsname,
                   MACSIO_MIF_ioFlags_t ioFlags, void *)
{
    char *ax = "ax";
    char *rx = "rx";
    PDBfile *retval = PD_open((char*)fname, ioFlags.do_wr?ax:rx);
    return (void *) retval;
}

static int ClosePDBFile(void *file, void *userData)
{
    return PD_close((PDBfile*)file);
}

static void write_mesh_part(PDBfile *pdbfile, json_object *part_obj)
{
//#warning WERE SKPPING THE MESH (COORDS) OBJECT PRESENTLY
    int i;
    json_object *vars_array = json_object_path_get_array(part_obj, "Vars");

    for (i = 0; i < json_object_array_length(vars_array); i++)
    {
        int j;
        json_object *var_obj = json_object_array_get_idx(vars_array, i);
        json_object *data_obj = json_object_path_get_extarr(var_obj, "data");
        char const *varname = json_object_path_get_string(var_obj, "name");
        int ndims = json_object_extarr_ndims(data_obj);
        void const *buf = json_object_extarr_data(data_obj);
        char const *dtype = json_object_extarr_type(data_obj)==json_extarr_type_flt64?"double":"float";
        long ind[6*3];

        /* set start,stop,stride for each dim */
        for (j = 0; j < ndims; j++)
        {
            ind[3 * j] = 0;
            ind[3 * j + 1] =  json_object_extarr_dim(data_obj, j) - 1;
            ind[3 * j + 2] = 1;
        }

        PD_write_alt(pdbfile, (char*)varname, (char*)dtype, (void*)buf, ndims, ind);
    }
}

static void main_dump_mif(json_object *main_obj, int numFiles, int dumpn, double dumpt)
{
    int size, rank;
    char fileName[256];
    int i, len;
    PDBfile *pdbfile;
    MACSIO_MIF_ioFlags_t ioFlags = {MACSIO_MIF_WRITE,
        JsonGetInt(main_obj, "clargs/exercise_scr")&0x1};

//#warning DIFFERENT MPI TAGS FOR DIFFERENT PLUGINS AND CONTEXTS
    MACSIO_MIF_baton_t *bat = MACSIO_MIF_Init(numFiles, ioFlags, MACSIO_MAIN_Comm, 3,
        CreatePDBFile, OpenPDBFile, ClosePDBFile, 0);

    rank = json_object_path_get_int(main_obj, "parallel/mpi_rank");
    size = json_object_path_get_int(main_obj, "parallel/mpi_size");

    /* Construct name for the silo file */
    sprintf(fileName, "%s_pdb_%05d_%03d.%s",
        json_object_path_get_string(main_obj, "clargs/filebase"),
        MACSIO_MIF_RankOfGroup(bat, rank),
        dumpn,
        json_object_path_get_string(main_obj, "clargs/fileext"));

    MACSIO_UTILS_RecordOutputFiles(dumpn, fileName);

    pdbfile = (PDBfile*) MACSIO_MIF_WaitForBaton(bat, fileName, 0);

    json_object *parts = json_object_path_get_array(main_obj, "problem/parts");

    for (int i = 0; i < json_object_array_length(parts); i++)
    {
        char domain_dir[256];
        json_object *this_part = json_object_array_get_idx(parts, i);

        snprintf(domain_dir, sizeof(domain_dir), "domain_%07d",
            json_object_path_get_int(this_part, "Mesh/ChunkID"));

        PD_mkdir(pdbfile, domain_dir);
        PD_cd(pdbfile, domain_dir);

        write_mesh_part(pdbfile, this_part);

        PD_cd(pdbfile, "..");
    }

    /* Hand off the baton to the next processor. This winds up closing
     * the file so that the next processor that opens it can be assured
     * of getting a consistent and up to date view of the file's contents. */
    MACSIO_MIF_HandOffBaton(bat, pdbfile);

    /* We're done using MACSIO_MIF, so finish it off */
    MACSIO_MIF_Finish(bat);

}

static void main_dump(int argi, int argc, char **argv, json_object *main_obj,
    int dumpn, double dumpt)
{
    int rank, size, numFiles;

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
            MACSIO_LOG_MSG(Die, ("PDB plugin cannot handle SIF mode."));
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
        if (!strcmp(modestr, "MIFMAX"))
            numFiles = json_object_path_get_int(main_obj, "parallel/mpi_size");
        else if (!strcmp(modestr, "MIFAUTO"))
        {
            /* Call utility to determine optimal file count */
//#warning ADD UTILIT TO DETERMINE OPTIMAL FILE COUNT
        }
        main_dump_mif(main_obj, numFiles, dumpn, dumpt);
    }
}

static int register_this_interface()
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
