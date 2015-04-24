#ifndef _MACSIO_CLAGS_H
#define _MACSIO_CLAGS_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MACSIO_CLARGS_TOMEM     0 
#define MACSIO_CLARGS_TOJSON    1

#define MACSIO_CLARGS_HELP  -1
#define MACSIO_CLARGS_ERROR 1
#define MACSIO_CLARGS_WARN 0
#define MACSIO_CLARGS_OK 0
#define MACSIO_CLARGS_SEPARATOR(SEPSTR) "macsio_args_sep_" #SEPSTR
#define MACSIO_CLARGS_END_OF_ARGS "macsio_end_of_args"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _MACSIO_CLARGS_ArgvFlags_t
{
    unsigned int error_mode : 1;
    unsigned int route_mode : 2;
} MACSIO_CLARGS_ArgvFlags_t;

#warning RE-THINK THESE NAMES
extern int MACSIO_CLARGS_ProcessCmdline(void **retobj, MACSIO_CLARGS_ArgvFlags_t flags, int argi, int argc, char **argv, ...);

#ifdef __cplusplus
}
#endif

#endif /* _UTIL_H */
