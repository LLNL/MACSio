#include <macsio_params.h>

MACSIO_ArgvHelpStruct_t const MACSIO_ArgvHelp[] = {
    {_ArgvParamKeys_min, ""},
    {interface, 
        "Specify the name of the interface to be tested. Use keyword 'list' "
        "to print a list of all known interfaces and then exit"},
    {parallel_file_mode,
        "Specify the parallel file mode. There are several choices. "
        "Use 'MIF' for Multiple Independent File (Poor Man's) mode and then "
        "also specify the number of files. Or, use 'MIFMAX' for MIF mode and "
        "one file per processor or 'MIFAUTO' for MIF mode and let the test "
        "determine the optimum file count. Use 'SIF' for SIngle shared File "
        "(Rich Man's) mode."},
    {part_size,
        "Per-MPI-rank mesh part size in bytes. A following B|K|M|G character indicates 'B'ytes (2^0), "
        "'K'ilobytes (2^10), 'M'egabytes (2^20) or 'G'igabytes (2^30). This is then the nominal I/O "
        "request size emitted from each MPI rank. [1M]"},
    {avg_num_parts,
        "The average number of mesh parts per MPI rank. Non-integral values are acceptable. For example, "
        "a value that is half-way between two integers, K and K+1, means that half the ranks have K "
        "mesh parts and half have K+1 mesh parts. As another example, a value of 2.75 here would mean "
        "that 75%% of the ranks get 3 parts and 25%% of the ranks get 2 parts. Note that the total "
        "number of parts is the this number multiplied by the MPI communicator size. If the result of "
        "that product is non-integral, it will be rounded and a warning message will be generated. [1]"},
    {part_distribution,
        "Specify how parts are distributed to MPI tasks. (currently ignored)"},
    {vars_per_part,
        "Number of mesh variable objects in each part. The smallest this can be depends on the mesh "
        "type. For rectilinear mesh it is 1. For curvilinear mesh it is the number of spatial "
        "dimensions and for unstructured mesh it is the number of spatial dimensions plus 2 * "
        "number of topological dimensions. [50]"},
    {num_dumps,
        "Total number of dump requests to write or read [10]"},
    {_ArgvParamKeys_max, ""}
};
