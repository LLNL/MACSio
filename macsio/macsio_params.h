#ifndef _MACSIO_PARAMS_H
#define _MACSIO_PARAMS_H

#ifdef __cplusplus
extern "C" {
#endif

/* All string-oriented keys used in json objects origina here as members of 
   enum statements to provide compile time type checking of keys. */

/* top level keys for MACSIO */
typedef enum _MACSIO_MainParamKeys_t {
    _MainParamKeys_min = 0,
    clargs,
    interface_options,
    parallel,
    problem,
    _MainParamKeys_max
} MACSIO_MainParamKeys_t;
static MACSIO_MainParamKeys_t main_param_keychk;
#define MACSIO_MAIN_KEY(KEY) (main_param_keychk = KEY, #KEY)

/* Parameter keys for MACSio's main command-line arguments */
typedef enum _MACSIO_ArgvParamKeys_t {
    _ArgvParamKeys_min = 0,
    interface,
    parallel_file_mode,
    part_size,
    part_dim,
    part_type,
    avg_num_parts,
    part_distribution,
    vars_per_part,
    num_dumps,
    alignment,
    filebase,
    fileext,
    _ArgvParamKeys_max
} MACSIO_ArgvParamKeys_t;
static MACSIO_ArgvParamKeys_t argv_param_keychk;
/* These macros enforce type checking of argv keys */
#define MACSIO_ARGV_FMT(KEY,OPTS) (argv_param_keychk = KEY, "--" #KEY " " #OPTS)
#define MACSIO_ARGV_KEY(KEY) (argv_param_keychk = KEY, "--" #KEY)
#warning GET RID OF THE 2 IN THIS MACRO NAME
#define MACSIO_ARGV_HELP2(KEY) (argv_param_keychk = KEY, MACSIO_ArgvHelp[KEY].helpStr) 
#define MACSIO_ARGV_DEF(KEY,OPTS) MACSIO_ARGV_FMT(KEY,OPTS), MACSIO_ARGV_HELP2(KEY)

typedef enum _MACSIO_ParallelParamKeys_t {
    _ParallelParamKeys_min = 0,
    mpi_size,
    mpi_rank,
    _ParallelParamKeys_max
} MACSIO_ParallelParamKeys_t;
static MACSIO_ParallelParamKeys_t parallel_param_keychk;
#define MACSIO_PARALLEL_KEY(KEY) (parallel_param_keychk = KEY, #KEY)

/* Parameter keys for MACSio's mesh synthesis */
typedef enum _MACSIO_MeshParamKeys_t {
    _MeshParamKeys_min = 0,
    parts,
    _MeshParamKeys_max
} MACSIO_MeshParamKeys_t;
static MACSIO_MeshParamKeys_t mesh_param_keychk;
#define MACSIO_MESH_KEY(KEY) (mesh_param_keychk = KEY, #KEY)

typedef struct _MACSIO_ArgvHelpStruct_t
{
    MACSIO_ArgvParamKeys_t key;
    char const *helpStr;
} MACSIO_ArgvHelpStruct_t;

extern MACSIO_ArgvHelpStruct_t const MACSIO_ArgvHelp[];

#ifdef __cplusplus
}
#endif

#endif
