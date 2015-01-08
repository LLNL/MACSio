#include <errno.h>
#include <limits.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <util.h>
#include <options.h>

IOPoptlist_t *IOPMakeOptlist()
{
    IOPoptlist_t *optlist = 0;
    const int maxopts = 100;

    if (0 == (optlist = ALLOC(IOPoptlist_t)))
        IOP_ERROR(("optlist alloc failed"), IOP_FATAL);
    if (0 == (optlist->options = ALLOC_N(IOPoptid_t, maxopts)))
        IOP_ERROR(("optlist alloc failed"), IOP_FATAL);
    if (0 == (optlist->values = ALLOC_N(void *, maxopts)))
        IOP_ERROR(("optlist alloc failed"), IOP_FATAL);

    optlist->numopts = 0;
    optlist->maxopts = maxopts;

    return optlist;
}

int IOPFreeOptlist(IOPoptlist_t *optlist)
{
    int i;
    if (!optlist)
    {
        IOP_ERROR(("bad IOPoptlist_t pointer"), IOP_WARN);
        return 0;
    }
    IOPClearOptlist(optlist);
    FREE(optlist->options);
    FREE(optlist->values);
    FREE(optlist);
    return 0;
}

int IOPClearOption(IOPoptlist_t *optlist, IOPoptid_t option)
{
    int i, j, foundit=0;

    if (!optlist)
    {
        IOP_ERROR(("bad IOPoptlist_t pointer"), IOP_WARN);
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
        IOP_ERROR(("non-existent option specified"), IOP_WARN);

    optlist->numopts--;
    optlist->options[optlist->numopts] = OPTID_NOT_SET;
    optlist->values[optlist->numopts]  = 0;

    return 0;
}

int IOPClearOptlist(IOPoptlist_t *optlist)
{
    int i;

    if (!optlist)
    {
        IOP_ERROR(("bad IOPoptlist_t pointer"), IOP_WARN);
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
int IOPAddOption(IOPoptlist_t *optlist, IOPoptid_t option, void *value)
{
    if (!optlist)
    {
        IOP_ERROR(("bad IOPoptlist_t pointer"), IOP_WARN);
        return 0;
    }

    if (optlist->numopts >= optlist->maxopts)
    {
        int maxopts = 2*optlist->maxopts;
        if (0 == (optlist->options = REALLOC_N(optlist->options,IOPoptid_t,maxopts)))
            IOP_ERROR(("optlist realloc failed"), IOP_FATAL);
        if (0 == (optlist->values = REALLOC_N(optlist->values, void *,maxopts)))
            IOP_ERROR(("optlist realloc failed"), IOP_FATAL);
        optlist->maxopts = maxopts;
    }

    optlist->options[optlist->numopts] = option;
    optlist->values[optlist->numopts] = value;
    optlist->numopts++;

    return 0;
}

int IOPAddIntOption(IOPoptlist_t *optlist, IOPoptid_t option, int value)
{
    int *p = ALLOC(int);
    *p = value;
    return IOPAddOption(optlist, option, p);
}

int IOPAddIntArrOption2(IOPoptlist_t *optlist, IOPoptid_t id, IOPoptid_t sid, int const * values, int nvalues)
{
    int *p = ALLOC_N(int,nvalues);
    IOPAddIntOption(optlist, sid, nvalues);
    memcpy(p, values, sizeof(int)*nvalues);
    return IOPAddOption(optlist, id, p);
}

int IOPGetIntOption(IOPoptlist_t const *optlist, IOPoptid_t option)
{
    void const *p = IOPGetOption(optlist, option);
    if (p) return *((int*)p);
    return -INT_MAX;
}

int const *IOPGetIntArrOption(IOPoptlist_t const *optlist, IOPoptid_t option)
{
    void const *p = IOPGetOption(optlist, option);
    return (int *) p;
}

int IOPAddDblOption(IOPoptlist_t *optlist, IOPoptid_t option, double value)
{
    double *p = ALLOC(double);
    *p = value;
    return IOPAddOption(optlist, option, p);
}

int IOPAddDblArrOption2(IOPoptlist_t *optlist, IOPoptid_t id, IOPoptid_t sid, double const * values, int nvalues)
{
    double *p = ALLOC_N(double,nvalues);
    IOPAddIntOption(optlist, sid, nvalues);
    memcpy(p, values, sizeof(double)*nvalues);
    return IOPAddOption(optlist, id, p);
}

double IOPGetDblOption(IOPoptlist_t const *optlist, IOPoptid_t option)
{
    void const *p = IOPGetOption(optlist, option);
    if (p) return *((double*)p);
    return -DBL_MAX;
}

double const *IOPGetDblArrOption(IOPoptlist_t const *optlist, IOPoptid_t option)
{
    void const *p = IOPGetOption(optlist, option);
    return (double *) p;
}

int IOPAddStrOption(IOPoptlist_t *optlist, IOPoptid_t option, char const *value)
{
    char *p = ALLOC_N(char,strlen(value)+1);
    strcpy(p, value);
    return IOPAddOption(optlist, option, p);
}

char const *IOPGetStrOption(IOPoptlist_t const *optlist, IOPoptid_t option)
{
    void const *p = IOPGetOption(optlist, option);
    return (char const *) p;
}

void const *IOPGetOption(const IOPoptlist_t *optlist, IOPoptid_t option)
{
    int i;

    if (!optlist) return 0;

    /* find the given option in the optlist and return its value */
    for (i = 0; i < optlist->numopts; i++)
        if (optlist->options[i] == option)
            return optlist->values[i];

    return 0;
}

void *IOPGetMutableOption(const IOPoptlist_t *optlist, IOPoptid_t option)
{
    int i;

    if (!optlist) return 0;

    /* find the given option in the optlist and return its value */
    for (i = 0; i < optlist->numopts; i++)
        if (optlist->options[i] == option)
            return optlist->values[i];

    return 0;
}

void *IOPGetMutableArrOption(const IOPoptlist_t *optlist, IOPoptid_t option)
{
    int i;

    if (!optlist) return 0;

    /* find the given option in the optlist and return its value */
    for (i = 0; i < optlist->numopts; i++)
        if (optlist->options[i] == option)
            return &(optlist->values[i]);

    return 0;
}
