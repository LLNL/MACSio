#ifndef _MACSIO_PARAMS_H
#define _MACSIO_PARAMS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Parameter keys for MACSio's main command-line arguments */
typedef enum _MACSIO_ArgvParamKeys_t {
    _ArgvParamKeys_min = 0,
    interface,
    parallel_file_mode,
    part_size,
    avg_num_parts,
    part_distribution,
    vars_per_part,
    num_dumps,
    _ArgvParamKeys_max
} MACSIO_ArgvParamKeys_t;
static MACSIO_ArgvParamKeys_t argv_param_keychk;

/* These macros enforce type checking of argv keys */
#define MACSIO_ARGV_FMT(KEY,OPTS) (argv_param_keychk = KEY, "--" #KEY " " #OPTS)
#define MACSIO_ARGV_KEY(KEY) (argv_param_keychk = KEY, "--" #KEY)
#warning GET RID OF THE 2 IN THIS MACRO NAME
#define MACSIO_ARGV_HELP2(KEY) (argv_param_keychk = KEY, MACSIO_ArgvHelp[KEY].helpStr) 
#define MACSIO_ARGV_DEF(KEY,OPTS) MACSIO_ARGV_FMT(KEY,OPTS), MACSIO_ARGV_HELP2(KEY)

/* Parameter keys for MACSio's mesh synthesis */
typedef enum _MACSIO_MeshParamKeys_t {
    _MeshParamKeys_min = 0,
    _MeshParamKeys_max
} MACSIO_MeshParamKeys_t;
static MACSIO_MeshParamKeys_t mesh_param_keychk;

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
