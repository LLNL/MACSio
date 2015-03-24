#include <string.h>

#include <macsio_log.h>
#include <options.h>

#define TEST_INT_OPT_VAL 5
#define TEST_DBL_OPT_VAL 1.235 
#define TEST_STR_OPT_VAL "mark"

#define INTARR_SIZE sizeof(intarr)/sizeof(intarr[0])
#define DBLARR_SIZE sizeof(dblarr)/sizeof(dblarr[0])

int main(int argc, char **argv)
{
    MACSIO_optlist_t *opts;
    int intarr[7] = {1, 2, 3, 4, 5, 6, 7};
    double dblarr[3] = {1.2, 5.8, 7.1};
    int const *pi;
    double const *pd;
    int i,cnt;

    opts = MACSIO_MakeOptlist();

    /* add some scalar options */
    MACSIO_AddIntOption(opts, TEST_INT_OPTID, TEST_INT_OPT_VAL);
    MACSIO_AddDblOption(opts, TEST_DBL_OPTID, TEST_DBL_OPT_VAL);
    MACSIO_AddStrOption(opts, TEST_STR_OPTID, TEST_STR_OPT_VAL);

    /* add some array valued options */
    MACSIO_AddIntArrOption(opts, TEST_INTARR, intarr, INTARR_SIZE);
    MACSIO_AddDblArrOption(opts, TEST_DBLARR, dblarr, DBLARR_SIZE);

    /* check the scalar options */
    if (MACSIO_GetIntOption(opts, TEST_INT_OPTID) != TEST_INT_OPT_VAL)
        MACSIO_LOG_MSG(Die, ("incorrect option value"));
    if (MACSIO_GetDblOption(opts, TEST_DBL_OPTID) != TEST_DBL_OPT_VAL)
        MACSIO_LOG_MSG(Die, ("incorrect option value"));
    if (strcmp(MACSIO_GetStrOption(opts, TEST_STR_OPTID), TEST_STR_OPT_VAL))
        MACSIO_LOG_MSG(Die, ("incorrect option value"));

    /* check the int array option */
    if ((cnt = MACSIO_GetIntOption(opts, TEST_INTARR_SIZE)) != INTARR_SIZE)
        MACSIO_LOG_MSG(Die, ("incorrect option value"));
    pi = MACSIO_GetIntArrOption(opts, TEST_INTARR);
    if (!pi)
        MACSIO_LOG_MSG(Die, ("incorrect option value"));
    for (i = 0; i < cnt; i++)
    {
        if (pi[i] != intarr[i])
            MACSIO_LOG_MSG(Die, ("incorrect option value"));
    }

    /* check the dbl array option */
    if ((cnt = MACSIO_GetIntOption(opts, TEST_DBLARR_SIZE)) != DBLARR_SIZE)
        MACSIO_LOG_MSG(Die, ("incorrect option value"));
    pd = MACSIO_GetDblArrOption(opts, TEST_DBLARR);
    if (!pd)
        MACSIO_LOG_MSG(Die, ("incorrect option value"));
    for (i = 0; i < cnt; i++)
    {
        if (pd[i] != dblarr[i])
            MACSIO_LOG_MSG(Die, ("incorrect option value"));
    }

    /* clear some options */
    MACSIO_ClearOption(opts, TEST_DBL_OPTID);
    MACSIO_ClearArrOption(opts, TEST_INTARR);
    if (MACSIO_GetOption(opts, TEST_DBL_OPTID))
        MACSIO_LOG_MSG(Die, ("option was not cleared"));
    if (MACSIO_GetOption(opts, TEST_INTARR_SIZE))
        MACSIO_LOG_MSG(Die, ("option was not cleared"));
    if (MACSIO_GetOption(opts, TEST_INTARR))
        MACSIO_LOG_MSG(Die, ("option was not cleared"));

    /* Try to write new data via backdoor */
    {
        char *ps = (char *) MACSIO_GetMutableOption(opts, TEST_STR_OPTID);
        int *p = (int *) MACSIO_GetMutableOption(opts, TEST_INT_OPTID);

        *p = -5;
        if (MACSIO_GetIntOption(opts, TEST_INT_OPTID) != -5)
            MACSIO_LOG_MSG(Die, ("backdoor write failed"));

        strcpy(ps, "kram");
        if (strcmp(MACSIO_GetStrOption(opts, TEST_STR_OPTID), "kram"))
            MACSIO_LOG_MSG(Die, ("backdoor write failed"));
    }

    MACSIO_FreeOptlist(opts);

    printf("All optlist tests passed\n");
    return 0;
}
