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
#include <ioperf.h>
#include <util.h>

#ifdef PARALLEL
#include <mpi.h>
#endif

static void handle_help_request_and_exit(int argi, int argc, char **argv)
{
    int i, n, *ids=0;;
    FILE *outFILE = (isatty(2) ? stderr : stdout);

    IOPGetInterfaceIds(&n, &ids);
    for (i = 0; i < n; i++)
    {
        const IOPIFaceHandle_t *iface = IOPGetInterfaceById(ids[i]);
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
    IOPGetInterfaceIds(&n, &ids);
    for (i = 0; i < n; i++)
    {
        char const *nm = IOPGetInterfaceName(ids[i]);
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

static void ProcessCommandLine(int argc, char *argv[], int *plugin_argi, IOPoptlist_t *opts)
{
    const int unknownArgsFlag = IOP_WARN;
    int plugin_args_start = -1;
    int cl_result;

    cl_result = IOPProcessCommandLine(unknownArgsFlag, 1, argc, argv,
        "--interface %s",
            "Specify the name of the interface to be tested. Use keyword 'list' "
            "to print a list of all known interfaces and then exit",
            IOPGetMutableArrOption(opts, IOINTERFACE_NAME),
        "--parallel-file-mode %s %d",
            "Specify the parallel file mode. There are several choices. "
            "Use 'MIF' for Multiple Independent File (Poor Man's) mode and then "
            "also specify the number of files. Or, use 'MIFMAX' for MIF mode and "
            "one file per processor or 'MIFAUTO' for MIF mode and let the test "
            "determine the optimum file count. Use 'SIF' for SIngle shared File "
            "(Rich Man's) mode.",
            IOPGetMutableArrOption(opts, PARALLEL_FILE_MODE),
            IOPGetMutableOption(opts, PARALLEL_FILE_COUNT),
        "--request-size %d",
            "Per-processor request size. A following B|K|M|G character indicates 'B'ytes (2^0), "
            "'K'ilobytes (2^10), 'M'egabytes (2^20) or 'G'igabytes (2^30) [1M]",
            IOPGetMutableOption(opts, REQUEST_SIZE),
        "--num-requests %d",
            "Total number of I/O requests [10]",
            IOPGetMutableOption(opts, NUM_REQUESTS),
        "--size-noise %d",
            "Randomly insert tiny I/O requests of specified size between main "
            "requests [0]",
            IOPGetMutableOption(opts, SIZE_NOISE),
        "--initial-size %d",
            "Specify initial file size to start from [0]",
            IOPGetMutableOption(opts, INIT_FILE_SIZE),
        "--alignment %d",
            "Align all I/O requests on boundaries that are a integral multiple "
            "of this size",
            IOPGetMutableOption(opts, ALIGNMENT),
        "--filename %s",
            "Specify sprintf-style string to indicate how file(s) should be named. If the "
            "the file(s) already exists, they will be used in a read test. If the file(s) "
            "do not exist, it/they will be generated in a write test. "
            "Default is 'iop-<iface>-<group#>.<ext>' where <group#> is the group number "
            "(MIF modes) or absent (SIF mode), <iface> is the name of the interface and <ext> "
            "is the canonical file extension for the given interface.",
            IOPGetMutableArrOption(opts, FILENAME_SPRINTF),
        "--rand-file-name",
            "Generate random filenames (why?)",
            IOPGetMutableOption(opts, RAND_FILENAME),
        "--no-mpi",
            "Do not use MPI. Just write a file per processor using random file "
            "names",
            IOPGetMutableOption(opts, NO_MPI),
        "--print-details",
            "Print detailed I/O performance data",
            IOPGetMutableOption(opts, PRINT_TIMING_DETAILS),
        "--driver-args %n",
            "All arguments after this sentinel are passed to the I/O driver plugin (ignore the %n)",
            &plugin_args_start,
    IOP_END_OF_ARGS);

    /* if we discovered help was requested, then print each plugin's help too */
    if (cl_result == IOP_ARGV_HELP)
        handle_help_request_and_exit(plugin_args_start+1, argc, argv);

    if (!strcmp(IOPGetStrOption(opts, IOINTERFACE_NAME), "list"))
        handle_list_request_and_exit();

    /* sanity check some values */
    if (!IOPGetStrOption(opts, IOINTERFACE_NAME))
        IOP_ERROR(("no io-interface specified"), IOP_FATAL);

    if (plugin_argi)
        *plugin_argi = plugin_args_start>-1?plugin_args_start+1:argc;

}

/* Debugging support method */
#define OUTINTOPT(A) fprintf(f?f:stdout, "%-40s = %d\n", #A, IOPGetIntOption(opts,A));
#define OUTSTROPT(A) fprintf(f?f:stdout, "%-40s = \"%s\"\n", #A, IOPGetStrOption(opts,A));
static void DumpOptions(IOPoptlist_t const *opts, FILE *f)
{
    OUTSTROPT(IOINTERFACE_NAME);
    OUTSTROPT(PARALLEL_FILE_MODE);
    OUTINTOPT(PARALLEL_FILE_COUNT);
    OUTINTOPT(REQUEST_SIZE);
    OUTINTOPT(NUM_REQUESTS);
    OUTINTOPT(SIZE_NOISE);
    OUTINTOPT(INIT_FILE_SIZE);
    OUTINTOPT(ALIGNMENT);
    OUTSTROPT(FILENAME_SPRINTF);
    OUTINTOPT(RAND_FILENAME);
    OUTINTOPT(NO_MPI);
    OUTINTOPT(PRINT_TIMING_DETAILS);
    OUTINTOPT(MPI_SIZE);
    OUTINTOPT(MPI_RANK);
}

static IOPIFaceHandle_t const *GetIOInterface(int argi, int argc, char *argv[], IOPoptlist_t const *opts)
{
    int i;
    char testfilename[256];
    char ifacename[256];
    const IOPIFaceHandle_t *retval=0;

    /* First, get rid of the old data file */
    strcpy(ifacename, IOPGetStrOption(opts, IOINTERFACE_NAME));

#warning FIX FILENAME GENERATION
    sprintf(testfilename, "iop_test_%s.dat", ifacename);
    unlink(testfilename);

    /* search for and instantiate the desired interface */
    retval = IOPGetInterfaceByName(ifacename);
    if (!retval)
        IOP_ERROR(("unable to instantiate IO interface \"%s\"",ifacename), IOP_FATAL);

    return retval;
}

static void BasicReadTest(IOPoptlist_t const *opts)
{
    IOP_ERROR(("BasicReadTest not yet implemented"), IOP_FATAL);
}

static void BasicWriteTest(IOPoptlist_t const *opts)
{
#if 0
    /* create buffer of data to write */
    IOPFileHandle_t *iopfile;
    int req_size = IOPGetIntOption(opts, REQUEST_SIZE);
    int num_reqs = IOPGetIntOption(opts, NUM_REQUESTS);
    int rank = IOPGetIntOption(opts, MPI_RANK);
    int size = IOPGetIntOption(opts, MPI_SIZE);
    char *parmode = IOPGetStrOption(opts, PARALLEL_MODE);
    char *filename = IOPGetStrOption(opts, FILENAME_SPRINTF);
    int i, j;
    double **buf;

    /* loop to frabricate some array data */
    buf = (double **) malloc(num_reqs * sizeof(double*));
    for (i = 0; i < num_reqs; i++)
    {
        buf[i] = (double *) malloc(req_size * sizeof(double));
    
        for (j = 0; j < req_size; j++)
        {
            double val = (double) rank;
            val += (double) j / (double) req_size;
            buf[i][j] = val;
        }
    }

    /* initialize the interface with any globally controlled settings/behaviors */

    /* create the file(s) */
    iopfile = IOPCreateFileAll(filename, opts);

    /* Create the arrays in the file (one for each request) to write */
    /* In SIF mode, we create one array shared across procs. In MIF mode, we
       create individual arrays, either one per proc or one per group */
    for (i = 0; i < num_reqs; i++)
    {
        int dims[4] = {0, 0, 0, 0};
        char arrname[IOP_MAX_NAME];

#warning SET ARRAY NAME DIFFERENTLY IN MIF/SIF MODES
        sprintf(arrname, "arr%06d", i);

#warning MAKE PARMODE OPTION AN ENUM LATER
        dims[0] = req_size / sizeof(double);
        if (!strcmp(parmode, "SIF"))
            dims[1] = size;

        IOPCreateArray(iopfile, arrname, IOP_DOUBLE, dims, 0);
    }

    /* sync file metadata */
    IOPSyncMeta(iopfile, 0);

    /* loop to define each array part we'll be writing from this processor */
    for (i = 0; i < num_reqs; i++)
    {
        int starts[4] = {0, rank, 0, 0};
        int strides[4] = {1, 1, 1, 1};
        int counts[4] = {req_size, size, 0, 0};
        char arrname[IOP_MAX_NAME];

        sprintf(arrname, "arr%06d", i);

        /* define array part */
        IOPDefineArrayPart(iopfile, arrname, starts, strides, counts, &buf[i], 0);
    }

    /* Start array writes */
    StartPendingArrays(iopfile);

    /* could add some 'fake' compute work here */

    /* Wait for everything to finish */
    FinishPendingArrays(iopfile);

    /* close the file(s) */
    IOPCloseFile(iopfile);

    /* output timing information */
#endif
}

static IOPoptlist_t *SetupDefaultOptions()
{
    IOPoptlist_t *ol = IOPMakeOptlist();

    IOPAddLiteralStrOptionFromHeap(ol, IOINTERFACE_NAME, "hdf5");
    IOPAddLiteralStrOptionFromHeap(ol, PARALLEL_FILE_MODE, "MIF");
    IOPAddIntOption(ol, PARALLEL_FILE_COUNT, 2);
    IOPAddIntOption(ol, REQUEST_SIZE, (1<<20));
    IOPAddIntOption(ol, NUM_REQUESTS, 10);
    IOPAddIntOption(ol, SIZE_NOISE, 0);
    IOPAddIntOption(ol, INIT_FILE_SIZE, 0);
    IOPAddIntOption(ol, ALIGNMENT, 0);
    IOPAddLiteralStrOptionFromHeap(ol, FILENAME_SPRINTF, "iop-hdf5-%06d.h5");
    IOPAddIntOption(ol, RAND_FILENAME, 0);
    IOPAddIntOption(ol, NO_MPI, 0);
    IOPAddIntOption(ol, PRINT_TIMING_DETAILS, 1);
    IOPAddIntOption(ol, MPI_SIZE, 1);
    IOPAddIntOption(ol, MPI_RANK, 0);

    return ol;
}

static int ReadTest(IOPoptlist_t const *opts)
{
#if 0
    struct stat statbuf;
    char tmpname[256];
    char *patt = IOPGetStrOption(opts, FILENAME_SPRINTF);
    snprintf(tmpname, sizeof(tmpname), patt, 0);

    /* use file existence of the zeroth to indicate write or read test */
    if (stat(tmpname, &statbuf) == 0) return 1;
#endif
    return 0;
}

int
main(int argc, char *argv[])
{
    IOPFileHandle_t *fh;
    IOPoptlist_t *opts;
    const IOPIFaceHandle_t *ioiface;
    double         t0,t1;
    int            argi;

    opts = SetupDefaultOptions();

    /* Process the command line */
    ProcessCommandLine(argc, argv, &argi, opts);


#ifdef PARALLEL
    if (!IOPGetIntOption(opts, NO_MPI))
    {
        MPI_Init(&argc, &argv);
        MPI_Comm_size(MPI_COMM_WORLD, (int *) IOPGetMutableOption(opts, MPI_SIZE));
	MPI_Comm_rank(MPI_COMM_WORLD, (int *) IOPGetMutableOption(opts, MPI_RANK));
    }
#endif

    if (ReadTest(opts))
        BasicReadTest(opts);
    else
        BasicWriteTest(opts);

#ifdef PARALLEL
    if (!IOPGetIntOption(opts, NO_MPI))
        MPI_Finalize();
#endif

    return (0);
}
