#include <string.h>

#include <util.h>
#include <options.h>

#define TEST_INT_OPT_VAL 5
#define TEST_DBL_OPT_VAL 1.235 
#define TEST_STR_OPT_VAL "mark"

#define INTARR_SIZE sizeof(intarr)/sizeof(intarr[0])
#define DBLARR_SIZE sizeof(dblarr)/sizeof(dblarr[0])

int main(int argc, char **argv)
{
    IOPoptlist_t *opts;
    int intarr[7] = {1, 2, 3, 4, 5, 6, 7};
    double dblarr[3] = {1.2, 5.8, 7.1};
    int const *pi;
    double const *pd;
    int i,cnt;

    opts = IOPMakeOptlist();

    /* add some scalar options */
    IOPAddIntOption(opts, TEST_INT_OPTID, TEST_INT_OPT_VAL);
    IOPAddDblOption(opts, TEST_DBL_OPTID, TEST_DBL_OPT_VAL);
    IOPAddStrOption(opts, TEST_STR_OPTID, TEST_STR_OPT_VAL);

    /* add some array valued options */
    IOPAddIntArrOption(opts, TEST_INTARR, intarr, INTARR_SIZE);
    IOPAddDblArrOption(opts, TEST_DBLARR, dblarr, DBLARR_SIZE);

    /* check the scalar options */
    if (IOPGetIntOption(opts, TEST_INT_OPTID) != TEST_INT_OPT_VAL)
        IOP_ERROR(("incorrect option value"), IOP_FATAL);
    if (IOPGetDblOption(opts, TEST_DBL_OPTID) != TEST_DBL_OPT_VAL)
        IOP_ERROR(("incorrect option value"), IOP_FATAL);
    if (strcmp(IOPGetStrOption(opts, TEST_STR_OPTID), TEST_STR_OPT_VAL))
        IOP_ERROR(("incorrect option value"), IOP_FATAL);

    /* check the int array option */
    if ((cnt = IOPGetIntOption(opts, TEST_INTARR_SIZE)) != INTARR_SIZE)
        IOP_ERROR(("incorrect option value"), IOP_FATAL);
    pi = IOPGetIntArrOption(opts, TEST_INTARR);
    if (!pi)
        IOP_ERROR(("incorrect option value"), IOP_FATAL);
    for (i = 0; i < cnt; i++)
    {
        if (pi[i] != intarr[i])
            IOP_ERROR(("incorrect option value"), IOP_FATAL);
    }

    /* check the dbl array option */
    if ((cnt = IOPGetIntOption(opts, TEST_DBLARR_SIZE)) != DBLARR_SIZE)
        IOP_ERROR(("incorrect option value"), IOP_FATAL);
    pd = IOPGetDblArrOption(opts, TEST_DBLARR);
    if (!pd)
        IOP_ERROR(("incorrect option value"), IOP_FATAL);
    for (i = 0; i < cnt; i++)
    {
        if (pd[i] != dblarr[i])
            IOP_ERROR(("incorrect option value"), IOP_FATAL);
    }

    /* clear some options */
    IOPClearOption(opts, TEST_DBL_OPTID);
    IOPClearArrOption(opts, TEST_INTARR);
    if (IOPGetOption(opts, TEST_DBL_OPTID))
        IOP_ERROR(("option was not cleared"), IOP_FATAL);
    if (IOPGetOption(opts, TEST_INTARR_SIZE))
        IOP_ERROR(("option was not cleared"), IOP_FATAL);
    if (IOPGetOption(opts, TEST_INTARR))
        IOP_ERROR(("option was not cleared"), IOP_FATAL);

    /* Try to write new data via backdoor */
    {
        char *ps = (char *) IOPGetMutableOption(opts, TEST_STR_OPTID);
        int *p = (int *) IOPGetMutableOption(opts, TEST_INT_OPTID);

        *p = -5;
        if (IOPGetIntOption(opts, TEST_INT_OPTID) != -5)
            IOP_ERROR(("backdoor write failed"), IOP_FATAL);

        strcpy(ps, "kram");
        if (strcmp(IOPGetStrOption(opts, TEST_STR_OPTID), "kram"))
            IOP_ERROR(("backdoor write failed"), IOP_FATAL);
    }

    IOPFreeOptlist(opts);

    printf("All optlist tests passed\n");
    return 0;
}
