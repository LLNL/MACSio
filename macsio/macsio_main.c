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
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

/*!
\addtogroup MACSIO_MAIN
@{
*/

#ifdef HAVE_SCR
#ifdef __cplusplus
extern "C" {
#endif
#include <scr.h>
#ifdef __cplusplus
}
#endif
#endif

#include <macsio_clargs.h>
#include <macsio_data.h>
#include <macsio_iface.h>
#include <macsio_log.h>
#include <macsio_main.h>
#include <macsio_timing.h>
#include <macsio_utils.h>
#include <macsio_work.h>

#include <json-cwx/json.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

#ifdef HAVE_CALIPER
#include <caliper/cali.h>
#ifdef HAVE_MPI
#include <caliper/cali-mpi.h>
#endif
#endif

#define MAX(A,B) (((A)>(B))?(A):(B))

extern char **enviornp;

#ifdef HAVE_MPI
MPI_Comm MACSIO_MAIN_Comm = MPI_COMM_WORLD;
#else
int MACSIO_MAIN_Comm = 0;
#endif

int MACSIO_MAIN_Size = 1;
int MACSIO_MAIN_Rank = 0;

static void handle_help_request_and_exit(int argi, int argc, char **argv)
{
    int rank = 0, i, n, *ids=0;;
    FILE *outFILE = (isatty(2) ? stderr : stdout);

#ifdef HAVE_MPI
    MPI_Comm_rank(MACSIO_MAIN_Comm, &rank);
#endif

    if (!rank)
    {
        MACSIO_IFACE_GetIds(&n, &ids);
        for (i = 0; i < n; i++)
        {
            const MACSIO_IFACE_Handle_t *iface = MACSIO_IFACE_GetById(ids[i]);
            if (iface->processArgsFunc)
            {
                fprintf(outFILE, "\nOptions specific to the \"%s\" I/O plugin\n", iface->name);
                (*(iface->processArgsFunc))(argi, argc, argv);
            }
        }
    }

#ifdef HAVE_MPI
    {   int result;
        if ((MPI_Initialized(&result) == MPI_SUCCESS) && result)
            MPI_Finalize();
    }
#endif

    exit(0);
}

static void handle_list_request_and_exit()
{
    int rank = 0, i, n, *ids = 0;
    FILE *outFILE = (isatty(2) ? stderr : stdout);
    char names_buf[1024];

#ifdef HAVE_MPI
    MPI_Comm_rank(MACSIO_MAIN_Comm, &rank);
#endif

    if (!rank)
    {
        names_buf[0] = '\0';
        MACSIO_IFACE_GetIds(&n, &ids);
        for (i = 0; i < n; i++)
        {
            char const *nm = MACSIO_IFACE_GetName(ids[i]);
            strcat(names_buf, "\"");
            strcat(names_buf, nm);
            strcat(names_buf, "\", ");
            if (!((i+1) % 10)) strcat(names_buf, "\n");
        }
        fprintf(outFILE, "List of available I/O-library plugins...\n");
        fprintf(outFILE, "%s\n", names_buf);
    }

#ifdef HAVE_MPI
    {   int result;
        if ((MPI_Initialized(&result) == MPI_SUCCESS) && result)
            MPI_Finalize();
    }
#endif

    exit(0);
}

static json_object *ProcessCommandLine(int argc, char *argv[], int *plugin_argi)
{
    MACSIO_CLARGS_ArgvFlags_t const argFlags = {MACSIO_CLARGS_ERROR,
                                                MACSIO_CLARGS_TOJSON,
                                                MACSIO_CLARGS_ASSIGN_ON};
    json_object *mainJargs = 0;
    int plugin_args_start = -1;
    int cl_result;

////#warning SUBGROUP OPTIONS INTO READ AND WRITE OPTIONS
////#warning MAYBE MAKE IT EASIER TO SPECIFY STRONG OR WEAK SCALING CASE
////#warning OPTION TO CONTROL TOTAL BUFFER SIZE IN LOWER-LAYERS
////#warning OPTION TO SET OUTPUT PRECISION TO FLOAT

    cl_result = MACSIO_CLARGS_ProcessCmdline((void**)&mainJargs, argFlags, 1, argc, argv,
        "--units_prefix_system %s", "\"binary\"",
            "Specify which SI units prefix system to use both in reporting performance\n"
            "data and in interpreting sizing modifiers to arguments. The options are\n"
            "\"binary\" and \"decimal\". For \"binary\" unit prefixes, sizes are reported\n"
            "in powers of 1024 and unit symbols Ki, Mi, Gi, Ti, Pi are used. For \"decimal\",\n"
            
        "sizes are reported in powers of 1000 and unit symbols are Kb, Mb, Gb, Tb, Pb.\n"
            "See http://en.wikipedia.org/wiki/Binary_prefix. for more information",
        "--interface %s", "miftmpl",
            "Specify the name of the interface to be tested. Use keyword 'list'\n"
            "to print a list of all known interface names and then exit.",
        "--parallel_file_mode %s %d", "MIF 4",
            "Specify the parallel file mode. There are several choices.\n"
            "Use 'MIF' for Multiple Independent File (Poor Man's) mode and then\n"
            "also specify the number of files. Or, use 'MIFFPP' for MIF mode and\n"
            "one file per processor or 'MIFOPT' for MIF mode and let the test\n"
            "determine the optimum file count. Use 'SIF' for SIngle shared File\n"
            "(Rich Man's) mode. If you also give a file count for SIF mode, then\n"
            "MACSio will perform a sort of hybrid combination of MIF and SIF modes.\n"
            "It will produce the specified number of files by grouping ranks in the\n"
            "the same way MIF does, but I/O within each group will be to a single,\n"
            "shared file using SIF mode.",
        "--avg_num_parts %f", "1",
            "The average number of mesh parts per MPI rank. Non-integral values\n"
            "are acceptable. For example, a value that is half-way between two\n"
            "integers, K and K+1, means that half the ranks have K mesh parts\n"
            "and half have K+1 mesh parts. As another example, a value of 2.75\n"
            "here would mean that 75% of the ranks get 3 parts and 25% of the\n"
            "ranks get 2 parts. Note that the total number of parts is this\n"
            "number multiplied by the MPI communicator size. If the result of that\n"
            "product is non-integral, it will be rounded and a warning message will\n"
            "be generated.",
        "--mesh_decomp %d %d %d", MACSIO_CLARGS_NODEFAULT,
            "The layout of parts in the mesh overriding the simple decomposition\n"
            "e.g. 4 8 1 will decompose into 32 parts in the stucture (x y z).",
        "--part_size %d", "80000",
            "Mesh part size in bytes. This becomes the nominal I/O request size\n"
            "used by each MPI rank when marshalling data. A following B|K|M|G\n"
            "character indicates 'B'ytes, 'K'ilo-, 'M'ega- or 'G'iga- bytes\n"
            "representing powers of either 1000 or 1024 according to the selected\n"
            "units prefix system. With no size modifier character, 'B' is assumed.\n"
            "Mesh and variable data is then sized by MACSio to hit this target byte\n"
            "count. However, due to constraints involved in creating valid mesh\n"
            "topology and variable data with realistic variation in features (e.g.\n"
            "zone- and node-centering), this target byte count is hit exactly for\n"
            "only the most frequently dumped objects and approximately for other objects.",
        "--part_mesh_dims %d %d %d", MACSIO_CLARGS_NODEFAULT,
            "Specify the number of elements in each dimension per mesh part.\n"
            "This overrides the part_size parameter and instead allows the size\n"
            "of the mesh to be determined by dimensions.\n"
            "e.g. 300 300 2, 300 300 0 (set final dimension to 0 for 2d",
        "--part_dim %d", "2",
                "Spatial dimension of parts; 1, 2, or 3",
        "--part_type %s", "rectilinear",
            "Options are 'uniform', 'rectilinear', 'curvilinear', 'unstructured'\n"
            "and 'arbitrary' (currently, only rectilinear is implemented)",
        "--part_map %s", MACSIO_CLARGS_NODEFAULT,
            "Specify the name of an ascii file containing part assignments to MPI ranks.\n"
            "The ith line in the file, numbered from 0, holds the MPI rank to which the\n"
            "ith part is to be assigned. (currently ignored)",
        "--vars_per_part %d", "20",
            "Number of mesh variable objects in each part. The smallest this can\n"
            "be depends on the mesh type. For rectilinear mesh it is 1. For\n"
            "curvilinear mesh it is the number of spatial dimensions and for\n"
            "unstructured mesh it is the number of spatial dimensions plus\n"
            "2^number of topological dimensions.",
        "--dataset_growth %f", MACSIO_CLARGS_NODEFAULT, 
            "The factor by which the volume of data will grow between dump iterations\n"
            "If no value is given or the value is <1.0 no dataset changes will take place.",
        "--topology_change_probability %f", "0.0",
            "The probability that the topology of the mesh (e.g. something fundamental\n"
            "about the mesh's structure) will change between dumps. A value of 1.0\n"
            "indicates it should be changed every dump. A value of 0.0, the default,\n"
            "indicates it will never change. A value of 0.1 indicates it will change\n"
            "about once every 10 dumps. Note: at present MACSio will not actually\n"
            "compute/construct a different topology. It will only inform a plugin\n"
            "that a given dump should be treated as a change in topology.",
        "--meta_type %s", "tabular",
            "Specify the type of metadata objects to include in each main dump.\n"
            "Options are 'tabular', 'amorphous'. For tabular type data, MACSio\n"
            "will generate a random set of tables of somewhat random structure\n"
            "and content. For amorphous, MACSio will generate a random hierarchy\n"
            "of random type and sized objects.",
////#warning MAY WANT SOME PORTIONS OF METADATA TO SCALE WITH MESH PIECE COUNT
        "--meta_size %d %d", "10000 50000",
            "Specify the size of the metadata objects on each processor and\n"
            "separately, the root (or master) processor (MPI rank 0). The size\n"
            "is specified in terms of the total number of bytes in the metadata\n"
            "objects MACSio creates. For example, a type of tabular and a size of\n"
            "10K bytes might result in 3 random tables; one table with 250 unnamed\n"
            "records where each record is an array of 3 doubles for a total of\n"
            "6000 bytes, another table of 200 records where each record is a\n"
            "named integer value where each name is length 8 chars for a total of\n"
            "2400 bytes and a 3rd table of 40 unnamed records where each record\n"
            "is a 40 byte struct comprised of ints and doubles for a total of 1600\n"
            "bytes.",
        "--num_dumps %d", "10",
            "Total number of dumps to marshal",
        "--max_dir_size %d", MACSIO_CLARGS_NODEFAULT,
            "The maximum number of filesystem objects (e.g. files or subdirectories)\n"
            "that MACSio will create in any one subdirectory. This is typically\n"
            "relevant only in MIF mode because MIF mode can wind up generating many\n"
            "will continue to create output files in the same directory until it has\n"
            "completed all dumps. Use a value of zero to force MACSio to put each\n"
            "dump in a separate directory but where the number of top-level directories\n"
            "is still unlimited. The result will be a 2-level directory hierarchy\n"
            "with dump directories at the top and individual dump files in each\n"
            "directory. A value > 0 will cause MACSio to create a tree-like directory\n"
            "structure where the files are the leaves and encompassing dir tree is\n"
            "created such as to maintain the max_dir_size constraint specified here.\n"
            "For example, if the value is set to 32 and the MIF file count is 1024,\n"
            "then each dump will involve a 3-level dir-tree; the top dir containing\n"
            "32 sub-dirs and each sub-dir containing 32 of the 1024 files for the\n"
            "dump. If more than 32 dumps are performed, then the dir-tree will really\n"
            "be 4 or more levels with the first 32 dumps' dir-trees going into the\n"
            "first dir, etc.",
#ifdef HAVE_SCR
        "--exercise_scr", "",
            "Exercise the Scalable Checkpoint and Restart (SCR)\n"
            "(https://computation.llnl.gov/project/scr/library) to marshal\n"
            "files. Note that this works only in MIFFPP mode. A request to exercise\n"
            "SCR in any other mode will be ignored and en error message generated.",
#endif
        "--compute_work_intensity %d", "1",
            "Add some work in between I/O phases. There are three levels of 'compute'\n"
            "that can be performed as follows:\n"
            "\tLevel 1: Perform a basic sleep operation\n"
            "\tLevel 2: Perform some simple FLOPS with randomly accessed data\n"
            "\tLevel 3: Solves the 2D Poisson equation via the Jacobi iterative method\n"
            "This input is intended to be used in conjunection with --compute_time\n"
            "which will roughly control how much time is spent doing work between iops",
        "--compute_time %f", "",
            "A rough lower bound on the number of seconds spent doing work between\n"
            "I/O phases. The type of work done is controlled by the --compute_work_intensity input\n"
            "and defaults to Level 1 (basic sleep).\n",
        "--debug_level %d", "0",
            "Set debugging level (1, 2 or 3) of log files. Higher numbers mean\n"
            "more frequent and detailed output. A value of zero, the default,\n"
            "turns all debugging output off. A value of 1 should not adversely\n"
            "effect performance. A value of 2 may effect performance and a value\n"
            "of 3 will almost certainly effect performance. For debug level 3,\n"
            "MACSio will generate ascii json files from each processor for the main\n"
            "dump object prior to starting dumps.",
        MACSIO_CLARGS_ARG_GROUP_BEG(Log File Options, Options to control size and shape of log file),
        "--log_file_name %s", "macsio-log.log",
            "The name of the log file.",
        "--log_line_cnt %d %d", "64 0",
            "Set number of lines per rank in the log file and number of extra lines\n"
            "for rank 0.",
        "--log_line_length %d", "128",
            "Set log file line length.",
        "--timings_file_name %s", "macsio-timings.log",
            "Specify the name of the timings file. Passing an empty string, \"\"\n"
            "will disable the creation of a timings file.",
        MACSIO_CLARGS_ARG_GROUP_END(Log File Options),
        "--alignment %d", MACSIO_CLARGS_NODEFAULT,
            "Not currently documented",
        "--filebase %s", "macsio",
            "Basename of generated file(s).",
        "--fileext %s", "",
            "Extension of generated file(s).",
        "--read_path %s", MACSIO_CLARGS_NODEFAULT,
            "Specify a path name (file or dir) to start reading for a read test.",
        "--num_loads %d", MACSIO_CLARGS_NODEFAULT,
            "Number of loads in succession to test.",
        "--no_validate_read", "",
            "Don't validate data on read.",
        "--read_mesh %s", MACSIO_CLARGS_NODEFAULT,
            "Specficify mesh name to read.",
        "--read_vars %s", MACSIO_CLARGS_NODEFAULT,
            "Specify variable names to read. \"all\" means all variables. If listing more\n"
            "than one, be sure to either enclose space separated list in quotes or\n"
            "use a comma-separated list with no spaces",
        "--time_randomize", "",
            "Make randomness in MACSio vary from dump to dump and run to run by\n"
            "using PRNGs seeded by time.",
#if 0
        MACSIO_CLARGS_LAST_ARG_SEPERATOR(plugin_args)
#endif
        "--plugin_args %n", MACSIO_CLARGS_NODEFAULT,
            "All arguments after this sentinel are passed to the I/O plugin\n"
            "plugin. The '%n' is a special designator for the builtin 'argi'\n"
            "value.",
    MACSIO_CLARGS_END_OF_ARGS);

    plugin_args_start = json_object_path_get_int(mainJargs, "argi");
    if (plugin_args_start == 0) plugin_args_start = -1;

    /* if we discovered help was requested, then print each plugin's help too */
    if (cl_result == MACSIO_CLARGS_HELP)
        handle_help_request_and_exit(plugin_args_start+1, argc, argv);

    if (!strcmp(json_object_path_get_string(mainJargs, "interface"), "list"))
        handle_list_request_and_exit();

#if 0
    /* sanity check some values */
    if (!strcmp(json_object_path_get_string(mainJargs, "interface"), ""))
        MACSIO_LOG_MSG(Die, ("no io-interface specified"));
#endif

    if (plugin_argi)
        *plugin_argi = plugin_args_start>-1?plugin_args_start+1:argc;


    return mainJargs;
}

static void
write_timings_file(char const *filename)
{
    char **timer_strs = 0, **rtimer_strs = 0;
    int i, ntimers, maxlen, rntimers = 0, rmaxlen = 0, rdata[3], rdata_out[3];
    MACSIO_LOG_LogHandle_t *timing_log;

    MACSIO_TIMING_DumpTimersToStrings(MACSIO_TIMING_ALL_GROUPS, &timer_strs, &ntimers, &maxlen);
    MACSIO_TIMING_ReduceTimers(MACSIO_MAIN_Comm, 0);
    if (MACSIO_MAIN_Rank == 0)
        MACSIO_TIMING_DumpReducedTimersToStrings(MACSIO_TIMING_ALL_GROUPS, &rtimer_strs, &rntimers, &rmaxlen);
    rdata[0] = maxlen > rmaxlen ? maxlen : rmaxlen;
    rdata[1] = ntimers;
    rdata[2] = rntimers;
#ifdef HAVE_MPI
    MPI_Allreduce(rdata, rdata_out, 3, MPI_INT, MPI_MAX, MACSIO_MAIN_Comm);
#endif

    /* add 32 chars to line len for log leader */
    timing_log = MACSIO_LOG_LogInit(MACSIO_MAIN_Comm, filename, rdata_out[0]+32, rdata_out[1], rdata_out[2]+1);

    /* dump this processor's timers */
    for (i = 0; i < ntimers; i++)
    {
        if (!timer_strs[i] || !strlen(timer_strs[i])) continue;
        MACSIO_LOG_MSGL(timing_log, Info, (timer_strs[i]));
        free(timer_strs[i]);
    }
    free(timer_strs);

    /* dump MPI reduced timers */
    if (MACSIO_MAIN_Rank == 0)
    {
        MACSIO_LOG_LogMsg(timing_log, "Reduced Timers...");

        for (i = 0; i < rntimers; i++)
        {
            if (!rtimer_strs[i] || !strlen(rtimer_strs[i])) continue;
            MACSIO_LOG_MSGL(timing_log, Info, (rtimer_strs[i]));
            free(rtimer_strs[i]);
        }
        free(rtimer_strs);
    }

    MACSIO_LOG_LogFinalize(timing_log);
}

static int
main_write(int argi, int argc, char **argv, json_object *main_obj)
{
    int rank = 0, dumpNum = 0, dumpCount = 0;
    unsigned long long problem_nbytes, dumpBytes = 0, summedBytes = 0;
    char nbytes_str[32], seconds_str[32], bandwidth_str[32];
    double dumpTime = 0;
    double timer_dt;
    double bandwidth, summedBandwidth;
    MACSIO_TIMING_GroupMask_t main_wr_grp = MACSIO_TIMING_GroupMask("main_write");
    double dump_loop_start, dump_loop_end;
    double min_dump_loop_start, max_dump_loop_end;
    int exercise_scr = JsonGetInt(main_obj, "clargs/exercise_scr");
    int work_intensity = JsonGetInt(main_obj, "clargs/compute_work_intensity");
    double work_dt = json_object_path_get_double(main_obj, "clargs/compute_time");

    /* Sanity check args */

    MACSIO_DATA_MakeRandomTable(100, 10000);

    /* Generate a static problem object to dump on each dump */
    json_object *problem_obj = MACSIO_DATA_GenerateTimeZeroDumpObject(main_obj,0);
    problem_nbytes = (unsigned long long) json_object_object_nbytes(problem_obj, JSON_C_FALSE);

////#warning MAKE JSON OBJECT KEY CASE CONSISTENT
    json_object_object_add(main_obj, "problem", problem_obj);

    /* Just here for debugging for the moment */
    if (MACSIO_LOG_DebugLevel >= 2)
    {
        char outfName[256];
        FILE *outf;
        int json_c_print_flags = JSON_C_TO_STRING_PRETTY | JSON_C_TO_STRING_SPACED;

        if (MACSIO_LOG_DebugLevel < 3)
            json_c_print_flags |= JSON_C_TO_STRING_NO_EXTARR_VALS;

        snprintf(outfName, sizeof(outfName), "main_obj_write_%03d.json", MACSIO_MAIN_Rank);
        outf = fopen(outfName, "w");
        fprintf(outf, "\"%s\"\n", json_object_to_json_string_ext(main_obj, json_c_print_flags));
        fclose(outf);
    }

////#warning WERE NOT GENERATING OR WRITING ANY METADATA STUFF

    dump_loop_start = MT_Time();
    dumpTime = 0.0;
    int total_dumps = json_object_path_get_int(main_obj, "clargs/num_dumps");

    MACSIO_UTILS_CreateFileStore(total_dumps, 1);

    double t;
    double maxT;
    double dt;
    double tNextBurstDump;
    double tNextTrickleDump;
    int dataset_evolved = 0;
    float factor = json_object_path_get_double(main_obj, "clargs/dataset_growth");
   
    int doWork = 0;
    if (work_dt > 0){
        doWork=1;
    } else {
        work_dt = 1;
    }

    dt = work_dt;
    maxT = total_dumps*dt;
    tNextBurstDump = dt;
    dumpNum = 0;
    t = 0;
////#warning THIS LOOP CURRENTLY JUST DOES A DUMP AFTER EVERY COMPUTE UP TO THE TOTAL NUMBER OF DUMPS. 
    while (t < maxT){

        if (doWork){
            MACSIO_WORK_DoComputeWork(&t, dt, work_intensity);
        }

        if (t >= tNextBurstDump || !doWork){
            int scr_need_checkpoint_flag = 1;
            MACSIO_TIMING_TimerId_t heavy_dump_tid;
#ifdef HAVE_SCR
            if (exercise_scr)
            SCR_Need_checkpoint(&scr_need_checkpoint_flag);
#endif

            const MACSIO_IFACE_Handle_t *iface = MACSIO_IFACE_GetByName(
                json_object_path_get_string(main_obj, "clargs/interface"));

            if (!strcmp(json_object_path_get_string(main_obj, "clargs/fileext"),"")){
                json_object_path_set_string(main_obj, "clargs/fileext", iface->ext);
            }

            /* log dump start */
            if (!exercise_scr || scr_need_checkpoint_flag){                
#ifdef HAVE_SCR
                int scr_valid = 0;
                if (exercise_scr)
                    SCR_Start_checkpoint();
#endif

                /* Start dump timer */
                heavy_dump_tid = MT_StartTimer("heavy dump", main_wr_grp, dumpNum);
////#warning REPLACE DUMPN AND DUMPT WITH A STATE TUPLE
                /* do the dump */
                //MACSIO_BurstDump(dt);

                
                (*(iface->dumpFunc))(argi, argc, argv, main_obj, dumpNum, dumpTime);
#ifdef HAVE_MPI
                mpi_errno = 0;
#endif
                errno = 0;

                timer_dt = MT_StopTimer(heavy_dump_tid);

#ifdef HAVE_SCR
                if (exercise_scr)
                    SCR_Complete_checkpoint(scr_valid);
#endif
            }

            /* stop timer */
            dumpTime += timer_dt;
            dumpBytes += problem_nbytes;
            dumpCount += 1;

            /* log dump timing */ // THE VOLUME OF DATA WRITTEN TO FILE =/= SIZE OF JSON PROBLEM OBJECT
            MACSIO_LOG_MSG(Info, ("Dump %02d BW: %s/%s = %s", dumpNum,
                    MU_PrByts(problem_nbytes, 0, nbytes_str, sizeof(nbytes_str)),
                    MU_PrSecs(timer_dt, 0, seconds_str, sizeof(seconds_str)),
                    MU_PrBW(problem_nbytes, timer_dt, 0, bandwidth_str, sizeof(bandwidth_str))));
            unsigned long long stat_bytes = MACSIO_UTILS_StatFiles(dumpNum);
            MACSIO_LOG_MSG(Info, ("Dump %02d Stat BW: %s/%s = %s", dumpNum,
                    MU_PrByts(stat_bytes, 0, nbytes_str, sizeof(nbytes_str)),
                    MU_PrSecs(timer_dt, 0, seconds_str, sizeof(seconds_str)),
                    MU_PrBW(stat_bytes, timer_dt, 0, bandwidth_str, sizeof(bandwidth_str))));
    
            dumpNum++;
            tNextBurstDump += dt;

            if (factor > 1.0){
                unsigned long long prev_bytes = MACSIO_UTILS_StatFiles(dumpNum-1);
                int growth_bytes = (prev_bytes*factor) - prev_bytes;
                if (growth_bytes > 0)
                    MACSIO_DATA_EvolveDataset(main_obj, &dataset_evolved, factor, growth_bytes);
            }
        } /* end of burst dump loop */

        if (t >= tNextTrickleDump){
        /* MACSIO_TrickleDump(dt); */
        } /*end of trickle dump loop */

        /* Increase the timestep if we aren't using the work routine to do so */
        if (!doWork) t++;
    } /* end of timetep loop */

    dump_loop_end = MT_Time();

    MACSIO_LOG_MSG(Info, ("Overall BW: %s/%s = %s",
        MU_PrByts(dumpBytes, 0, nbytes_str, sizeof(nbytes_str)),
        MU_PrSecs(dumpTime, 0, seconds_str, sizeof(seconds_str)),
        MU_PrBW(dumpBytes, dumpTime, 0, bandwidth_str, sizeof(bandwidth_str))));

    bandwidth = dumpBytes / dumpTime;
    summedBandwidth = bandwidth;
    min_dump_loop_start = dump_loop_start;
    max_dump_loop_end = dump_loop_end;

#ifdef HAVE_MPI
    MPI_Comm_rank(MACSIO_MAIN_Comm, &rank);
    MPI_Reduce(&bandwidth, &summedBandwidth, 1, MPI_DOUBLE, MPI_SUM, 0, MACSIO_MAIN_Comm);
    MPI_Reduce(&dumpBytes, &summedBytes, 1, MPI_UNSIGNED_LONG_LONG, MPI_SUM, 0, MACSIO_MAIN_Comm);
    MPI_Reduce(&dump_loop_start, &min_dump_loop_start, 1, MPI_DOUBLE, MPI_MIN, 0, MACSIO_MAIN_Comm);
    MPI_Reduce(&dump_loop_end, &max_dump_loop_end, 1, MPI_DOUBLE, MPI_MAX, 0, MACSIO_MAIN_Comm);
#endif

    if (rank == 0)
    {
        MACSIO_LOG_MSG(Info, ("Summed  BW: %s",
            MU_PrBW(summedBandwidth, 1.0, 0, bandwidth_str, sizeof(bandwidth_str))));
        MACSIO_LOG_MSG(Info, ("Total Bytes: %s; Last finisher - First starter = %s; BW = %s",
            MU_PrByts(summedBytes, 0, nbytes_str, sizeof(nbytes_str)),
            MU_PrSecs(max_dump_loop_end - min_dump_loop_start, 0, seconds_str, sizeof(seconds_str)),
            MU_PrBW(summedBytes, max_dump_loop_end - min_dump_loop_start, 0, bandwidth_str, sizeof(bandwidth_str))));
    }
    for (int j=0; j<total_dumps; j++){
        MACSIO_UTILS_StatFiles(j);
    }
    MACSIO_UTILS_CleanupFileStore();

    return (0);
}

////#warning DO WE REALLY CALL IT THE MAIN_OBJ HERE
static int
main_read(int argi, int argc, char **argv, json_object *main_obj)
{
    int loadNum;
    MACSIO_TIMING_GroupMask_t main_rd_grp = MACSIO_TIMING_GroupMask("main_read");

    for (loadNum = 0; loadNum < json_object_path_get_int(main_obj, "clargs/num_loads"); loadNum++)
    {
        json_object *data_read_obj;
        MACSIO_TIMING_TimerId_t heavy_load_tid;

        const MACSIO_IFACE_Handle_t *iface = MACSIO_IFACE_GetByName(
            json_object_path_get_string(main_obj, "clargs/interface"));

        /* log load start */

        /* Start load timer */
        heavy_load_tid = MT_StartTimer("heavy load", main_rd_grp, loadNum);

        /* do the load */
        (*(iface->loadFunc))(argi, argc, argv,
            JsonGetStr(main_obj, "clargs/read_path"), main_obj, &data_read_obj);

        /* stop timer */
        MT_StopTimer(heavy_load_tid);

        /* log load completion */

        /* Validate the data */
        if (JsonGetBool(main_obj, "clargs/validate_read"))
            MACSIO_DATA_ValidateDataRead(data_read_obj);
    }

    /* Just here for debugging for the moment */
    if (MACSIO_LOG_DebugLevel >= 3)
    {
        char outfName[256];
        FILE *outf;
        snprintf(outfName, sizeof(outfName), "main_obj_read_%03d.json", MACSIO_MAIN_Rank);
        outf = fopen(outfName, "w");
        fprintf(outf, "\"%s\"\n", json_object_to_json_string_ext(main_obj, JSON_C_TO_STRING_PRETTY));
        fclose(outf);
    }
    return (0);
}

static void InitializeDefaultPRNGs(void)
{
    double currtime = MT_Time();
    unsigned ucurrtim = ((unsigned *)&currtime)[0] ^ ((unsigned *)&currtime)[1];

    /* ensure all procs have same ucurrtim */
#ifdef HAVE_MPI
    MPI_Bcast(&ucurrtim, 1, MPI_UNSIGNED, 0, MACSIO_MAIN_Comm);
#endif

    /* Initialize the PRNGs */
    MACSIO_DATA_InitializeDefaultPRNGs((unsigned) MACSIO_MAIN_Rank, ucurrtim);
}

static void FinalizeDefaultPRNGs()
{
#ifdef HAVE_MPI
    /* Lets confirm the rank-invariant PRNGs agree and issue a message if not */
    unsigned rand_check[2] = {MD_random_rankinv(), MD_random_rankinv_tv()};
    unsigned rand_check_r[2];
    MPI_Reduce(rand_check, rand_check_r, 2, MPI_UNSIGNED, MPI_BXOR, 0, MACSIO_MAIN_Comm);
    if (MACSIO_MAIN_Rank == 0 && (rand_check_r[0] != 0 || rand_check_r[1] != 0))
    {
        MACSIO_LOG_MSG(Warn, ("rank-invariant PRNGs failed sanity check "
            "suggesting non-collective use."));
    }
#endif

    MACSIO_DATA_FinalizeDefaultPRNGs();
}

int
main(int argc, char *argv[])
{
    json_object *main_obj = json_object_new_object();
    json_object *parallel_obj = json_object_new_object();
    json_object *clargs_obj = 0;
    MACSIO_TIMING_GroupMask_t main_grp;
    MACSIO_TIMING_TimerId_t main_tid;
    int i, argi, exercise_scr = 0;
    double currtime;
    unsigned ucurrtim;

    /* quick pre-scan for scr cl flag */
    for (i = 0; i < argc && !exercise_scr; i++)
        exercise_scr = !strcmp("exercise_scr", argv[i]);

#ifdef HAVE_CALIPER
#ifdef HAVE_MPI
    /* Ensures Caliper's MPI runtime lib is loaded */
    cali_mpi_init();
#endif
    cali_config_preset("CALI_LOG_VERBOSITY", "0");
#endif

////#warning SHOULD WE BE USING MPI-3 API
#ifdef HAVE_MPI
    MPI_Init(&argc, &argv);
#ifdef HAVE_SCR
////#warning SANITY CHECK WITH MIFFPP
    if (exercise_scr)
        SCR_Init();
#endif
    MPI_Comm_dup(MPI_COMM_WORLD, &MACSIO_MAIN_Comm);
    MPI_Comm_set_errhandler(MACSIO_MAIN_Comm, MPI_ERRORS_RETURN);
    MPI_Comm_size(MACSIO_MAIN_Comm, &MACSIO_MAIN_Size);
    MPI_Comm_rank(MACSIO_MAIN_Comm, &MACSIO_MAIN_Rank);
    mpi_errno = MPI_SUCCESS;
#endif
    errno = 0;

    /* We need to init the error log pretty early in main for other
       components to have a log to write high priority error messages */
    MACSIO_LOG_StdErr = MACSIO_LOG_LogInit(MACSIO_MAIN_Comm, 0, 0, 0, 0);

    /* Start Timing Package */
    main_grp = MACSIO_TIMING_GroupMask("MACSIO main()");
    main_tid = MT_StartTimer("main", main_grp, MACSIO_TIMING_ITER_AUTO);

    InitializeDefaultPRNGs();

    /* Process the command line and put the results in the problem */
    clargs_obj = ProcessCommandLine(argc, argv, &argi);
    json_object_object_add(main_obj, "clargs", clargs_obj);

    strncpy(MACSIO_UTILS_UnitsPrefixSystem, JsonGetStr(clargs_obj, "units_prefix_system"),
        sizeof(MACSIO_UTILS_UnitsPrefixSystem));

    MACSIO_LOG_MainLog = MACSIO_LOG_LogInit(MACSIO_MAIN_Comm,
        JsonGetStr(clargs_obj, "log_file_name"),
        JsonGetInt(clargs_obj, "log_line_length"),
        JsonGetInt(clargs_obj, "log_line_cnt/0"),
        JsonGetInt(clargs_obj, "log_line_cnt/1"));

////#warning THESE INITIALIZATIONS SHOULD BE IN MACSIO_LOG
    MACSIO_LOG_DebugLevel = JsonGetInt(clargs_obj, "debug_level");

    /* Setup parallel information */
    json_object_object_add(parallel_obj, "mpi_size", json_object_new_int(MACSIO_MAIN_Size));
    json_object_object_add(parallel_obj, "mpi_rank", json_object_new_int(MACSIO_MAIN_Rank));
    json_object_object_add(main_obj, "parallel", parallel_obj);

////#warning SHOULD WE INCLUDE TOP-LEVEL INFO ON VAR NAMES AND WHETHER THEYRE RESTRICTED
////#warning CREATE AN IO CONTEXT OBJECT
    /* Acquire an I/O context handle from the plugin */

    /* Do a read or write test */
    if (strcmp(JsonGetStr(clargs_obj, "read_path"),"null"))
        main_read(argi, argc, argv, main_obj);
    else
        main_write(argi, argc, argv, main_obj);

    /* stop total timer */
    MT_StopTimer(main_tid);

    /* Write timings data file if requested */
    if (strlen(JsonGetStr(clargs_obj, "timings_file_name")))
        write_timings_file(JsonGetStr(clargs_obj, "timings_file_name"));

    MACSIO_TIMING_ClearTimers(MACSIO_TIMING_ALL_GROUPS);

    FinalizeDefaultPRNGs();

////#warning ATEXIT THESE
    if (json_object_put(main_obj) != 1)
    {
        MACSIO_LOG_MSG(Info, ("Unable to free main JSON object"));
    }
    MACSIO_TIMING_GroupMask(0);
    MACSIO_TIMING_ReduceTimers(MACSIO_MAIN_Comm, -1);
    json_object_apath_get_string(0,0); /* free circ cache */
    MACSIO_LOG_LogFinalize(MACSIO_LOG_MainLog);
    MACSIO_LOG_LogFinalize(MACSIO_LOG_StdErr);

#ifdef HAVE_SCR
    if (exercise_scr)
        SCR_Finalize();
#endif

#ifdef HAVE_MPI
    {   int result;
        if ((MPI_Initialized(&result) == MPI_SUCCESS) && result)
            MPI_Finalize();
    }
#endif

////#warning FIX RETVAL OF MAIN TO BE NON-ZERO WHEN ERRORS OCCUR
    return (0);
}

/*!@}*/
