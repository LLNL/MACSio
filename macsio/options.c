#include <errno.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <util.h>
#include <options.h>

/* helpful memory allocation wrappers */
#define ALLOC(T) ((T*)calloc((size_t)1,sizeof(T)))
#define ALLOC_N(T,N) ((T*)((N)>0?calloc((size_t)(N),sizeof(T)):0))
#define REALLOC(P,T,N) REALLOC_N((P),(T),(N))
#define REALLOC_N(P,T,N) ((T*)((N)>0?realloc((P),(size_t)((N)*sizeof(T))):0))
#define FREE(M) if(M){free(M);(M)=0;}

MACSIO_optlist_t *MACSIO_MakeOptlist()
{
    MACSIO_optlist_t *optlist = 0;
    const int maxopts = 100;

    if (0 == (optlist = ALLOC(MACSIO_optlist_t)))
        return 0;
    if (0 == (optlist->options = ALLOC_N(MACSIO_optid_t, maxopts)))
        return 0;
    if (0 == (optlist->values = ALLOC_N(void *, maxopts)))
        return 0;

    optlist->numopts = 0;
    optlist->maxopts = maxopts;

    return optlist;
}

int MACSIO_FreeOptlist(MACSIO_optlist_t *optlist)
{
    int i;
    if (!optlist)
    {
        return 0;
    }
    MACSIO_ClearOptlist(optlist);
    FREE(optlist->options);
    FREE(optlist->values);
    FREE(optlist);
    return 0;
}

int MACSIO_ClearOption(MACSIO_optlist_t *optlist, MACSIO_optid_t option)
{
    int i, j, foundit=0;

    if (!optlist)
    {
        return 0;
    }

    /* Shift values down in list by one entry */
    for (i = 0; i < optlist->numopts; i++)
    {
        if (optlist->options[i] == option)
        {
            foundit = 1;
            FREE(optlist->values[i]);
            for (j = i; j < optlist->numopts-1; j++)
            {
                optlist->options[j] = optlist->options[j+1];
                optlist->values[j]  = optlist->values[j+1];
            }
            break;
        }
    }

    if (!foundit)
        return 0;

    optlist->numopts--;
    optlist->options[optlist->numopts] = OPTID_NOT_SET;
    optlist->values[optlist->numopts]  = 0;

    return 0;
}

int MACSIO_ClearOptlist(MACSIO_optlist_t *optlist)
{
    int i;

    if (!optlist)
    {
        return 0;
    }

    for (i = 0; i < optlist->maxopts; i++)
    {
        optlist->options[i] = OPTID_NOT_SET;
        FREE(optlist->values[i]);
    }
    optlist->numopts = 0;

    return 0;
}

/* Option list owns memory after this call */
int MACSIO_AddOption(MACSIO_optlist_t *optlist, MACSIO_optid_t option, void *value)
{
    if (!optlist)
    {
        return 0;
    }

    if (optlist->numopts >= optlist->maxopts)
    {
        int maxopts = 2*optlist->maxopts;
        if (0 == (optlist->options = REALLOC_N(optlist->options,MACSIO_optid_t,maxopts)))
            return 0;
        if (0 == (optlist->values = REALLOC_N(optlist->values, void *,maxopts)))
            return 0;
        optlist->maxopts = maxopts;
    }

    optlist->options[optlist->numopts] = option;
    optlist->values[optlist->numopts] = value;
    optlist->numopts++;

    return 0;
}

int MACSIO_AddIntOption(MACSIO_optlist_t *optlist, MACSIO_optid_t option, int value)
{
    int *p = ALLOC(int);
    *p = value;
    return MACSIO_AddOption(optlist, option, p);
}

int MACSIO_AddIntArrOption2(MACSIO_optlist_t *optlist, MACSIO_optid_t id, MACSIO_optid_t sid, int const * values, int nvalues)
{
    int *p = ALLOC_N(int,nvalues);
    MACSIO_AddIntOption(optlist, sid, nvalues);
    memcpy(p, values, sizeof(int)*nvalues);
    return MACSIO_AddOption(optlist, id, p);
}

int MACSIO_GetIntOption(MACSIO_optlist_t const *optlist, MACSIO_optid_t option)
{
    void const *p = MACSIO_GetOption(optlist, option);
    if (p) return *((int*)p);
    return -INT_MAX;
}

int const *MACSIO_GetIntArrOption(MACSIO_optlist_t const *optlist, MACSIO_optid_t option)
{
    void const *p = MACSIO_GetOption(optlist, option);
    return (int *) p;
}

int MACSIO_AddDblOption(MACSIO_optlist_t *optlist, MACSIO_optid_t option, double value)
{
    double *p = ALLOC(double);
    *p = value;
    return MACSIO_AddOption(optlist, option, p);
}

int MACSIO_AddDblArrOption2(MACSIO_optlist_t *optlist, MACSIO_optid_t id, MACSIO_optid_t sid, double const * values, int nvalues)
{
    double *p = ALLOC_N(double,nvalues);
    MACSIO_AddIntOption(optlist, sid, nvalues);
    memcpy(p, values, sizeof(double)*nvalues);
    return MACSIO_AddOption(optlist, id, p);
}

double MACSIO_GetDblOption(MACSIO_optlist_t const *optlist, MACSIO_optid_t option)
{
    void const *p = MACSIO_GetOption(optlist, option);
    if (p) return *((double*)p);
    return -DBL_MAX;
}

double const *MACSIO_GetDblArrOption(MACSIO_optlist_t const *optlist, MACSIO_optid_t option)
{
    void const *p = MACSIO_GetOption(optlist, option);
    return (double *) p;
}

int MACSIO_AddStrOption(MACSIO_optlist_t *optlist, MACSIO_optid_t option, char const *value)
{
    char *p = ALLOC_N(char,strlen(value)+1);
    strcpy(p, value);
    return MACSIO_AddOption(optlist, option, p);
}

char const *MACSIO_GetStrOption(MACSIO_optlist_t const *optlist, MACSIO_optid_t option)
{
    void const *p = MACSIO_GetOption(optlist, option);
    return (char const *) p;
}

void const *MACSIO_GetOption(const MACSIO_optlist_t *optlist, MACSIO_optid_t option)
{
    int i;

    if (!optlist) return 0;

    /* find the given option in the optlist and return its value */
    for (i = 0; i < optlist->numopts; i++)
        if (optlist->options[i] == option)
            return optlist->values[i];

    return 0;
}

void *MACSIO_GetMutableOption(const MACSIO_optlist_t *optlist, MACSIO_optid_t option)
{
    int i;

    if (!optlist) return 0;

    /* find the given option in the optlist and return its value */
    for (i = 0; i < optlist->numopts; i++)
        if (optlist->options[i] == option)
            return optlist->values[i];

    return 0;
}

void *MACSIO_GetMutableArrOption(const MACSIO_optlist_t *optlist, MACSIO_optid_t option)
{
    int i;

    if (!optlist) return 0;

    /* find the given option in the optlist and return its value */
    for (i = 0; i < optlist->numopts; i++)
        if (optlist->options[i] == option)
            return &(optlist->values[i]);

    return 0;
}
