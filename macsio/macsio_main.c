#include <errno.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <ifacemap.h>
#include <macsio.h>
#include <macsio_params.h>
#include <util.h>

#include <json-c/json.h>

#ifdef PARALLEL
#include <mpi.h>
#endif

static void handle_help_request_and_exit(int argi, int argc, char **argv)
{
    int i, n, *ids=0;;
    FILE *outFILE = (isatty(2) ? stderr : stdout);

    MACSIO_GetInterfaceIds(&n, &ids);
    for (i = 0; i < n; i++)
    {
        const MACSIO_IFaceHandle_t *iface = MACSIO_GetInterfaceById(ids[i]);
        if (iface->processArgsFunc)
        {
            fprintf(outFILE, "\nOptions specific to the \"%s\" I/O driver\n", iface->name);
            (*(iface->processArgsFunc))(argi, argc, argv);
        }
    }
#ifdef PARALLEL
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
    MACSIO_GetInterfaceIds(&n, &ids);
    for (i = 0; i < n; i++)
    {
        char const *nm = MACSIO_GetInterfaceName(ids[i]);
        strcat(names_buf, "\"");
        strcat(names_buf, nm);
        strcat(names_buf, "\", ");
        if (!((i+1) % 10)) strcat(names_buf, "\n");
    }
    fprintf(outFILE, "List of available I/O-library drivers...\n");
    fprintf(outFILE, "%s\n", names_buf);
#ifdef PARALLEL
    {   int result;
        if ((MPI_Initialized(&result) == MPI_SUCCESS) && result)
            MPI_Finalize();
    }
#endif
    exit(0);
}

static void ProcessCommandLine(int argc, char *argv[], int *plugin_argi, json_object **clJargs)
{
    MACSIO_ArgvFlags_t const argFlags = {MACSIO_WARN, MACSIO_ARGV_TOJSON};
    int plugin_args_start = -1;
    int cl_result;
    json_object *mainJargs = 0;

    cl_result = MACSIO_ProcessCommandLine((void**)&mainJargs, argFlags, 1, argc, argv,
        MACSIO_ARGV_DEF(interface, %s),
        MACSIO_ARGV_DEF(parallel_file_mode, %s %d),
        MACSIO_ARGV_DEF(part_size, %d),
        "--avg-num-parts %f",
            "The average number of mesh parts per MPI rank. Non-integral values are acceptable. For example, "
            "a value that is half-way between two integers, K and K+1, means that half the ranks have K "
            "mesh parts and half have K+1 mesh parts. As another example, a value of 2.75 here would mean "
            "that 75%% of the ranks get 3 parts and 25%% of the ranks get 2 parts. Note that the total "
            "number of parts is the this number multiplied by the MPI communicator size. If the result of "
            "that product is non-integral, it will be rounded and a warning message will be generated. [1]",
        "--part-distribution %s",
            "Specify how parts are distributed to MPI tasks. (currently ignored)",
        "--vars-per-part %d",
            "Number of mesh variable objects in each part. The smallest this can be depends on the mesh "
            "type. For rectilinear mesh it is 1. For curvilinear mesh it is the number of spatial "
            "dimensions and for unstructured mesh it is the number of spatial dimensions plus 2 * "
            "number of topological dimensions. [50]",
        "--num-dumps %d",
            "Total number of dump requests to write or read [10]",
        "--alignment %d",
            "Align all I/O requests on boundaries that are a integral multiple "
            "of this size",
        "--filename %s",
            "Specify sprintf-style string to indicate how file(s) should be named. If the "
            "the file(s) already exists, they will be used in a read test. If the file(s) "
            "do not exist, it/they will be generated in a write test. "
            "Default is 'macsio-<iface>-<group#>.<ext>' where <group#> is the group number "
            "(MIF modes) or absent (SIF mode), <iface> is the name of the interface and <ext> "
            "is the canonical file extension for the given interface.",
        "--print-details",
            "Print detailed I/O performance data",
        "--driver-args %n",
            "All arguments after this sentinel are passed to the I/O driver plugin (ignore the %n)",
    MACSIO_END_OF_ARGS);

    /* if we discovered help was requested, then print each plugin's help too */
    if (cl_result == MACSIO_ARGV_HELP)
        handle_help_request_and_exit(plugin_args_start+1, argc, argv);

    if (!strcmp(json_object_path_get_string(mainJargs, MACSIO_ARGV_KEY(interface)), "list"))
        handle_list_request_and_exit();

    /* sanity check some values */
    if (!strcmp(json_object_path_get_string(mainJargs, MACSIO_ARGV_KEY(interface)), ""))
        MACSIO_ERROR(("no io-interface specified"), MACSIO_FATAL);

    if (plugin_argi)
        *plugin_argi = plugin_args_start>-1?plugin_args_start+1:argc;

    *clJargs = mainJargs;

}

/* Debugging support method */
#define OUTDBLOPT(A) fprintf(f?f:stdout, "%-40s = %f\n", #A, MACSIO_GetDblOption(opts,A));
#define OUTINTOPT(A) fprintf(f?f:stdout, "%-40s = %d\n", #A, MACSIO_GetIntOption(opts,A));
#define OUTSTROPT(A) fprintf(f?f:stdout, "%-40s = \"%s\"\n", #A, MACSIO_GetStrOption(opts,A));
static void DumpOptions(MACSIO_optlist_t const *opts, FILE *f)
{
    OUTSTROPT(IOINTERFACE_NAME);
    OUTSTROPT(PARALLEL_FILE_MODE);
    OUTINTOPT(PARALLEL_FILE_COUNT);
    OUTINTOPT(PART_SIZE);
    OUTDBLOPT(AVG_NUM_PARTS);
    OUTSTROPT(PART_DISTRIBUTION);
    OUTINTOPT(VARS_PER_PART);
    OUTINTOPT(NUM_DUMPS);
    OUTINTOPT(ALIGNMENT);
    OUTSTROPT(FILENAME_SPRINTF);
    OUTINTOPT(PRINT_TIMING_DETAILS);
    OUTINTOPT(MPI_SIZE);
    OUTINTOPT(MPI_RANK);
}

static MACSIO_IFaceHandle_t const *GetIOInterface(int argi, int argc, char *argv[], MACSIO_optlist_t const *opts)
{
    int i;
    char testfilename[256];
    char ifacename[256];
    const MACSIO_IFaceHandle_t *retval=0;

    /* First, get rid of the old data file */
    strcpy(ifacename, MACSIO_GetStrOption(opts, IOINTERFACE_NAME));

#warning FIX FILENAME GENERATION
    sprintf(testfilename, "macsio_test_%s.dat", ifacename);
    unlink(testfilename);

    /* search for and instantiate the desired interface */
    retval = MACSIO_GetInterfaceByName(ifacename);
    if (!retval)
        MACSIO_ERROR(("unable to instantiate IO interface \"%s\"",ifacename), MACSIO_FATAL);

    return retval;
}

static MACSIO_optlist_t *SetupDefaultOptions()
{
    MACSIO_optlist_t *ol = MACSIO_MakeOptlist();

    MACSIO_AddLiteralStrOptionFromHeap(ol, IOINTERFACE_NAME, "hdf5");
    MACSIO_AddLiteralStrOptionFromHeap(ol, PARALLEL_FILE_MODE, "MIF");
    MACSIO_AddIntOption(ol, PARALLEL_FILE_COUNT, 2);
    MACSIO_AddIntOption(ol, PART_SIZE, (1<<20));
    MACSIO_AddDblOption(ol, AVG_NUM_PARTS, 1.0);
    MACSIO_AddIntOption(ol, VARS_PER_PART, 50);
    MACSIO_AddLiteralStrOptionFromHeap(ol, PART_DISTRIBUTION, "ingored");
    MACSIO_AddIntOption(ol, NUM_DUMPS, 10);
    MACSIO_AddIntOption(ol, ALIGNMENT, 0);
    MACSIO_AddLiteralStrOptionFromHeap(ol, FILENAME_SPRINTF, "macsio-hdf5-%06d.h5");
    MACSIO_AddIntOption(ol, PRINT_TIMING_DETAILS, 1);
    MACSIO_AddIntOption(ol, MPI_SIZE, 1);
    MACSIO_AddIntOption(ol, MPI_RANK, 0);

    return ol;
}

int
main(int argc, char *argv[])
{
    MACSIO_FileHandle_t *fh;
    json_object *main_obj = json_object_new_object();
    json_object *parallel_obj = json_object_new_object();
    json_object *jargs;
    MACSIO_optlist_t *opts;
    const MACSIO_IFaceHandle_t *ioiface;
    double         t0,t1;
    int            argi;
    int size = 1, rank = 0;

#ifdef PARALLEL
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

    opts = SetupDefaultOptions();

    /* Process the command line and put the results in the problem */
    ProcessCommandLine(argc, argv, &argi, &jargs);
    json_object_object_add(main_obj, MACSIO_MAIN_KEY(clargs), jargs);


    json_object_object_add(parallel_obj, MACSIO_PARALLEL_KEY(mpi_size), json_object_new_int(size));
    json_object_object_add(parallel_obj, MACSIO_PARALLEL_KEY(mpi_rank), json_object_new_int(rank));
    json_object_object_add(main_obj, MACSIO_MAIN_KEY(parallel), parallel_obj);

    printf("\"%s\"\n", json_object_to_json_string(main_obj));

#if 0
    /* Generate a static problem object to dump on each dump */
    macsio_problem_object = MACSIO_GenerateStaticDumpObject();
    
    /* Start total timer */

    for (dumpNum = 0; dumpNum < MACSIO_GetIntOption(opts, NUM_DUMPS); dumpNum++)
    {
        /* Use 'Fill' for read operation */
        const MACSIO_IFaceHandle_t *iface = MACSIO_GetInterfaceByName(MACSIO_GetStrOption(opts, IOINTERFACE_NAME));

        /* log dump start */

        /* Start dump timer */

        /* do the dump */
        (*(iface->Dump))(argc, argi, argv, opts, macsio_problem_object, timer_object, dumpNum);

        /* stop timer */

        /* log dump completion */
    }

    /* stop total timer */
#endif

#ifdef PARALLEL
    MPI_Finalize();
#endif

    return (0);
}
