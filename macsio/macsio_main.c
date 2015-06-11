#warning ADD COPYRIGHT INFORMATION TO ALL FILES
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

#include <json-c/json.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

/*!
 * \mainpage
 *
 * MACSio is a Multi-purpose, Application-Centric, Scalable I/O proxy application.
 *
 * It is designed to support a number of goals with respect to parallel I/O performance benchmarking
 * including the ability to test and compare various I/O libraries and I/O paradigms, to predict
 * scalable performance of real applications and to help identify where improvements in I/O performance
 * can be made.
 *
 * For an overview of MACSio's design goals and outline of its design, please see
 * <A HREF="../../macsio_design_intro_final_html/macsio_design_intro_final.htm">this design document.</A>
 * 
 * MACSio is capable of generating a wide variety of mesh and variable data and of amorphous metadata
 * typical of HPC multi-physics applications. Currently, the only supported mesh type in MACSio is 
 * a rectilinear, multi-block type mesh in 2 or 3 dimensions. However, some of the functions to generate other
 * mesh types such as curvilinear, block-structured AMR, unstructured, unstructured-AMR and arbitrary
 * are already available. In addition, regardless of the particular type of mesh MACSio generates for 
 * purposes of I/O performance testing, it stores and marshalls all of the resultant data in an uber
 * JSON-C object that is passed around witin MACSio and between MACSIO and its I/O plugins.
 *
 * MACSio employs a very simple algorithm to generate and then decompose a mesh in parallel. However, the
 * decomposition is also general enough to create multiple mesh pieces on individual MPI ranks and for
 * the number of mesh pieces vary to between MPI ranks. At present, there is no support to explicitly specify
 * a particular arrangement of mesh pieces and MPI ranks. However, such enhancement can be easily made at
 * a later date.
 *
 * MACSio's command-line arguments are designed to give the user control over the nominal I/O request sizes
 * emitted from MPI ranks for mesh bulk data and for amorphous metadata. The user specifies a size, in bytes,
 * for mesh pieces. MACSio then computes a mesh part size, in nodes, necessary to hit this target byte count for
 * double precision data. MACSio will determine an N dimensional logical size of a mesh piece that is a close
 * to equal dimensional as possible. In addition, the user specifies an average number of mesh pieces that will be
 * assigned to each MPI rank. This does not have to be a whole number. When it is a whole number, each MPI rank
 * has the same number of mesh pieces. When it is not, some processors have one more mesh piece than others.
 * This is common of HPC multi-physics applications. Together, the total processor count and average number of
 * mesh pieces per processor gives a total number of mesh pieces that comprise the entire mesh. MACSio then
 * finds an N dimensional arrangement (N=[1,2,3]) of the pieces that is as close to equal dimension as possible.
 * If mesh piece size or total count of pieces wind up being prime numbers, MACSio will only be able to factor
 * these into long, narrow shapes where 2 (or 3) of the dimensions are of size 1. That will make examination of
 * the resulting data using visualization tools like VisIt a little less convenient but is otherwise harmless
 * from the perspective of driving and assessing I/O performance.
 *
 * Once the global whole mesh shape is determined as a count of total pieces and as counts of pieces in each
 * of the logical dimensions, MACSio uses a very simple algorithm to assign mesh pieces to MPI ranks.
 * The global list of mesh pieces is numbered starting from 0. First, the number
 * of pieces to assign to rank 0 is chosen. When the average piece count is non-integral, it is a value
 * between K and K+1. So, MACSio randomly chooses either K or K+1 pieces but being carful to weight the
 * randomness so that once all pieces are assigned to all ranks, the average piece count per rank target
 * is achieved. MACSio then assigns the next K or K+1 numbered pieces to the next MPI rank. It continues
 * assigning pieces to MPI ranks, in piece number order, until all MPI ranks have been assigned pieces.
 * The algorithm runs indentically on all ranks. When the algorithm reaches the part assignment for the
 * rank on which its executing, it then generates the K or K+1 mesh pieces for that rank. Although the
 * algorithm is essentially a sequential algorithm with asymptotic behavior O(#total pieces), it is primarily
 * a simple book-keeping loop which completes in a fraction of a second even for more than one million
 * pieces.
 *
 * Each piece of the mesh is a simple rectangular region of space. The spatial bounds of that region are
 * easily determined. Any variables to be placed on the mesh can be easily handled as long as the variable's
 * spatial variation can be described in the global goemetric space.
 *
 */

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
    int i, n, *ids=0;;
    FILE *outFILE = (isatty(2) ? stderr : stdout);

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
    int i, n, *ids = 0;
    FILE *outFILE = (isatty(2) ? stderr : stdout);
    char names_buf[1024];

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
    MACSIO_CLARGS_ArgvFlags_t const argFlags = {MACSIO_CLARGS_WARN, MACSIO_CLARGS_TOJSON};
    json_object *mainJargs = 0;
    int plugin_args_start = -1;
    int cl_result;

#warning SUBGROUP OPTIONS INTO READ AND WRITE OPTIONS
#warning MAYBE MAKE IT EASIER TO SPECIFY STRONG OR WEAK SCALING CASE

    cl_result = MACSIO_CLARGS_ProcessCmdline((void**)&mainJargs, argFlags, 1, argc, argv,
        "--units_prefix_system %s", "binary",
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
        "--part_size %d", "80000",
            "Mesh part size in bytes. This becomes the nominal I/O request size\n"
            "used by each MPI rank when marshalling data. A following B|K|M|G\n"
            "character indicates 'B'ytes, 'K'ilo-, 'M'ega- or 'G'iga- bytes\n"
            "representing powers of either 1000 or 1024 according to the selected\n"
            "units prefix system. With no size modifier character, 'B' is assumed.\n"
            "Mesh and variable data is then sized by MACSio to hit this target byte\n"
            "count. However, due to contraints involved in creating valid mesh\n"
            "topology and variable data with realistic variation in features (e.g.\n"
            "zone- and node-centering), this target byte count is hit exactly for\n"
            "only the most frequently dumped objects and approximately for other objects.",
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
            "2^number of topological dimensions. [50]",
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
#warning MAY WANT SOME PORTIONS OF METADATA TO SCALE WITH MESH PIECE COUNT
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
            "files on each dump. The default setting is unlimited meaning that MACSio\n"
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
        "--debug_level %d", "0",
            "Set debugging level (1, 2 or 3) of log files. Higher numbers mean\n"
            "more frequent and detailed output. A value of zero, the default,\n"
            "turns all debugging output off. A value of 1 should not adversely\n"
            "effect performance. A value of 2 may effect performance and a value\n"
            "of 3 will almost certainly effect performance. For debug level 3,\n"
            "MACSio will generate ascii json files from each processor for the main\n"
            "dump object prior to starting dumps.",
        "--log_file_name %s", "macsio-log.log",
            "The name of the log file.",
        "--log_line_cnt %d", "64",
            "Set number of lines per rank in the log file.",
        "--log_line_length %d", "128",
            "Set log file line length.",
        "--alignment %d", MACSIO_CLARGS_NODEFAULT,
            "Not currently documented",
        "--filebase %s", "macsio",
            "Basename of generated file(s).",
        "--fileext %s", "dat",
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
        "--time_randomize_seeds", "",
            "Make randomness in MACSio vary from dump to dump and run to run by\n"
            "time-modulating all random number seeding.",
#if 0
        MACSIO_CLARGS_LAST_ARG_SEPERATOR(plugin_args)
#endif
        "--plugin_args %n", MACSIO_CLARGS_NODEFAULT,
            "All arguments after this sentinel are passed to the I/O plugin\n"
            "plugin. The '%n' is a special designator for the builtin 'argi'\n"
            "value.",
    MACSIO_CLARGS_END_OF_ARGS);

    plugin_args_start = json_object_path_get_int(mainJargs, "argi");

    /* if we discovered help was requested, then print each plugin's help too */
    if (cl_result == MACSIO_CLARGS_HELP)
        handle_help_request_and_exit(plugin_args_start+1, argc, argv);

    if (!strcmp(json_object_path_get_string(mainJargs, "interface"), "list"))
        handle_list_request_and_exit();

    /* sanity check some values */
    if (!strcmp(json_object_path_get_string(mainJargs, "interface"), ""))
        MACSIO_LOG_MSG(Die, ("no io-interface specified"));

    if (plugin_argi)
        *plugin_argi = plugin_args_start>-1?plugin_args_start+1:argc;

    return mainJargs;
}

static int
main_write(int argi, int argc, char **argv, json_object *main_obj)
{
    int rank = 0, dumpNum = 0, dumpCount = 0;
    unsigned long long problem_nbytes, dumpBytes = 0, summedBytes = 0;
    char nbytes_str[32], seconds_str[32], bandwidth_str[32], seconds_str2[32];
    double dumpTime = 0;
    double bandwidth, summedBandwidth;
    MACSIO_TIMING_GroupMask_t main_wr_grp = MACSIO_TIMING_GroupMask("main_write");
    double dump_loop_start, dump_loop_end;
    double min_dump_loop_start, max_dump_loop_end;
    int exercise_scr = JsonGetInt(main_obj, "clargs/exercise_scr");

    /* Sanity check args */

    /* Generate a static problem object to dump on each dump */
    json_object *problem_obj = MACSIO_DATA_GenerateTimeZeroDumpObject(main_obj,0);
    problem_nbytes = (unsigned long long) json_object_object_nbytes(problem_obj);

#warning MAKE JSON OBJECT KEY CASE CONSISTENT
    json_object_object_add(main_obj, "problem", problem_obj);

    /* Just here for debugging for the moment */
    if (MACSIO_LOG_DebugLevel >= 3)
    {
        char outfName[256];
        FILE *outf;

#warning ADD JSON PRINTING OPTIONS: sort extarrs at end, dont dump large data, html output, dump large data at end
#warning LEVEL 1 AND LEVEL 2 DEBUGGING SHOULD GENERATE JSON FILES BUT WITHOUT RAW DATA
        snprintf(outfName, sizeof(outfName), "main_obj_write_%03d.json", MACSIO_MAIN_Rank);
        outf = fopen(outfName, "w");
        fprintf(outf, "\"%s\"\n", json_object_to_json_string_ext(main_obj, JSON_C_TO_STRING_PRETTY));
        fclose(outf);
    }

#warning WERE NOT GENERATING OR WRITING ANY METADATA STUFF

    dump_loop_start = MT_Time();
    dumpTime = 0.0;
    for (dumpNum = 0; dumpNum < json_object_path_get_int(main_obj, "clargs/num_dumps"); dumpNum++)
    {
        double dt;
        int scr_need_checkpoint_flag = 1;
        MACSIO_TIMING_TimerId_t heavy_dump_tid;

#warning ADD OPTION TO UNLINK OLD FILE SETS

#ifdef HAVE_SCR
        if (exercise_scr)
            SCR_Need_checkpoint(&scr_need_checkpoint_flag);
#endif

        const MACSIO_IFACE_Handle_t *iface = MACSIO_IFACE_GetByName(
            json_object_path_get_string(main_obj, "clargs/interface"));

        /* log dump start */

        if (!exercise_scr || scr_need_checkpoint_flag)
        {
            int scr_valid = 0;

#ifdef HAVE_SCR
            if (exercise_scr)
                SCR_Start_checkpoint();
#endif

            /* Start dump timer */
            heavy_dump_tid = MT_StartTimer("heavy dump", main_wr_grp, dumpNum);

#warning REPLACE DUMPN AND DUMPT WITH A STATE TUPLE
#warning SHOULD HAVE PLUGIN RETURN FILENAMES SO MACSIO CAN STAT FOR TOTAL BYTES ON DISK
            /* do the dump */
            (*(iface->dumpFunc))(argi, argc, argv, main_obj, dumpNum, dumpTime);

            dt = MT_StopTimer(heavy_dump_tid);

#ifdef HAVE_SCR
            if (exercise_scr)
                SCR_Complete_checkpoint(scr_valid);
#endif
        }

        /* stop timer */
        dumpTime += dt;
        dumpBytes += problem_nbytes;
        dumpCount += 1;

        /* log dump timing */
        MACSIO_LOG_MSG(Info, ("Dump %02d BW: %s/%s = %s", dumpNum,
            MU_PrByts(problem_nbytes, 0, nbytes_str, sizeof(nbytes_str)),
            MU_PrSecs(dt, 0, seconds_str, sizeof(seconds_str)),
            MU_PrBW(problem_nbytes, dt, 0, bandwidth_str, sizeof(bandwidth_str))));
    }

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
}

#warning DO WE REALLY CALL IT THE MAIN_OBJ HERE
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
}

int
main(int argc, char *argv[])
{
    json_object *main_obj = json_object_new_object();
    json_object *parallel_obj = json_object_new_object();
    json_object *problem_obj = 0;
    json_object *clargs_obj = 0;
    MACSIO_TIMING_GroupMask_t main_grp;
    MACSIO_TIMING_TimerId_t main_tid;
    int i, argi, exercise_scr = 0;
    int size = 1, rank = 0;

    /* quick pre-scan for scr cl flag */
    for (i = 0; i < argc && !exercise_scr; i++)
        exercise_scr = !strcmp("exercise_scr", argv[i]);

#warning SHOULD WE BE USING MPI-3 API
#ifdef HAVE_MPI
    MPI_Init(&argc, &argv);
#ifdef HAVE_SCR
#warning SANITY CHECK WITH MIFFPP
    if (exercise_scr)
        SCR_Init();
#endif
    MPI_Comm_dup(MPI_COMM_WORLD, &MACSIO_MAIN_Comm);
    MPI_Errhandler_set(MACSIO_MAIN_Comm, MPI_ERRORS_RETURN);
    MPI_Comm_size(MACSIO_MAIN_Comm, &MACSIO_MAIN_Size);
    MPI_Comm_rank(MACSIO_MAIN_Comm, &MACSIO_MAIN_Rank);
    mpi_errno = MPI_SUCCESS;
#endif
    errno = 0;

    main_grp = MACSIO_TIMING_GroupMask("MACSIO main()");
    main_tid = MT_StartTimer("main", main_grp, MACSIO_TIMING_ITER_AUTO);

    MACSIO_LOG_StdErr = MACSIO_LOG_LogInit(MACSIO_MAIN_Comm, 0, 0, 0);

    /* Process the command line and put the results in the problem */
    clargs_obj = ProcessCommandLine(argc, argv, &argi);
    json_object_object_add(main_obj, "clargs", clargs_obj);

    strncpy(MACSIO_UTILS_UnitsPrefixSystem, JsonGetStr(clargs_obj, "units_prefix_system"),
        sizeof(MACSIO_UTILS_UnitsPrefixSystem));

    MACSIO_LOG_MainLog = MACSIO_LOG_LogInit(MACSIO_MAIN_Comm,
        JsonGetStr(clargs_obj, "log_file_name"),
        JsonGetInt(clargs_obj, "log_line_length"),
        JsonGetInt(clargs_obj, "log_line_cnt"));

#warning THESE INITIALIZATIONS SHOULD BE IN MACSIO_LOG
    MACSIO_LOG_DebugLevel = JsonGetInt(clargs_obj, "debug_level");

    /* Setup parallel information */
    json_object_object_add(parallel_obj, "mpi_size", json_object_new_int(MACSIO_MAIN_Size));
    json_object_object_add(parallel_obj, "mpi_rank", json_object_new_int(MACSIO_MAIN_Rank));
    json_object_object_add(main_obj, "parallel", parallel_obj);

#warning SHOULD WE INCLUDE TOP-LEVEL INFO ON VAR NAMES AND WHETHER THEYRE RESTRICTED
#warning CREATE AN IO CONTEXT OBJECT
    /* Acquire an I/O context handle from the plugin */

    /* Do a read or write test */
    if (strcmp(JsonGetStr(clargs_obj, "read_path"),"null"))
        main_read(argi, argc, argv, main_obj);
    else
        main_write(argi, argc, argv, main_obj);

    /* stop total timer */
    MT_StopTimer(main_tid);

    MACSIO_TIMING_ReduceTimers(MACSIO_MAIN_Comm, 0);

    if (!rank)
    {
        char **timer_strs;
        int i, ntimers, maxlen;

        MACSIO_TIMING_DumpReducedTimersToStrings(MACSIO_TIMING_ALL_GROUPS, &timer_strs, &ntimers, &maxlen);
        for (i = 0; i < ntimers; i++)
        {
            /* For now, just log to stderr */
#warning NEED A LOG FILE FOR SPECIFIC SET OF PROCESSORS, OR JUST ONE
            MACSIO_LOG_MSGL(MACSIO_LOG_StdErr, Info, (timer_strs[i]));
            free(timer_strs[i]);
        }
        free(timer_strs);
    }

    MACSIO_TIMING_ClearTimers(MACSIO_TIMING_ALL_GROUPS);

#warning ATEXIT THESE
    MACSIO_LOG_LogFinalize(MACSIO_LOG_MainLog);
    MACSIO_LOG_LogFinalize(MACSIO_LOG_StdErr);

#ifdef HAVE_SCR
    if (exercise_scr)
        SCR_Finalize();
#endif
#ifdef HAVE_MPI
    MPI_Finalize();
#endif

    return (0);
}
