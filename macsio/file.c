/*

    GENARRIA - GENeric ARRay Interface Adaptor

    This is the top level interface that benchmarking clients call
    to create/open files, create namespaces and arrays and read and
    write arrays. This interface turns around and dispatches the
    equivalent requests to the underlying I/O drivers.

    This layer also handles some minimal 'services' that not all 
    low-level driver interfaces support such as Poor Man's parallel 
    file open logistics, symbol tables, etc.

    The file create/open methods are the ones responsible for
    instantiating the underlying I/O driver and populating the IOPFileHandle_t
    with appropriate function pointers for that driver.

*/

/* Include Silo's convenience macros for handling MIF mode */
#include <pmpio.h>

static void *IOP_PMPIOCreateFileCallback(char const *fname, char const *nsname, void *udata)
{
    /* call the io-interface's create file method */
    /* call the io-interface's create namespace method */
    /* set the current namespace */
    /* return the file handle */
}

static void *IOP_PMPIOOpenFileCallback(char const *fname, char const *nsname, PMPIO_iomode_t, void *udata)
{
    /* call the io-interface's open file method */

    /* if write, call the io-interface's create namespace method */

    /* call the io-interface's set current namespace method */

    /* return the file handle */
}

static void IOP_PMPIOCloseFileCallback(void *file, void *udata)
{
    /* call the interface's close file method */

    /* any other memory cleanup */
}


static IOPFileHandle_t *IOPCreateFileAllMIF(IOPoptlist_t const *opts)
{
    const int writeTest = 1;
    int numGroups = IOPGetIntOption(opts, PARALLEL_FILE_COUNT);
    char const *patt = IOPGetStrOption(opts, FILENAME_SPRINTF);
    char fileName[1024];
    char nsName[IOP_MAX_NAME];

    PMPIO_baton_t *bat = PMPIO_Init(numGroups, PMPIO_WRITE, MPI_COMM_WORLD, writeTest,
        IOP_PMPIOCreateFileCallback, IOP_PMPIOOpenFileCallback, IOP_PMPIOCloseFileCallback, &userData);

    sprintf(fileName, patt, bat->groupRank);
#warning MAKE NAMESPACE NAME OPTION
    sprintf(nsName, "domain_%06d", bat->rankInComm);

    return (IOPFileHandle_t *) PMPIO_WaitForBaton(bat, fileName, nsName);
}

static IOPFileHandle_t *IOPCreateFileAllSIF(IOPoptlist_t const *opts)
{
}

IOPFileHandle_t *IOPCreateFileAll(IOPoptlist_t const *opts)
{

    if (parallel_mode == MIF)
        return IOPCreateFileAllMIF(opts);
    else
        return IOPCreateFileAllSIF(opts);

}

IOPFileHandle_t *IOPOpenFileAll(char const *pathname, int driver_id,
   int flags, IOPoptlist_t const *moreopts)
{
}

int IOPCloseFileAll(IOPFileHandle_t *fh, IOPoptlist_t const *moreopts)
{
    if (parallel_mode == MIF)
        return IOPCloseFileMIF();
    else
        return IOPCloseFileSIF();
}
