#include <assert.h>
#include <unistd.h>

#include <json-cwx/json.h>

#include <macsio_clargs.h>
#include <macsio_log.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

/* long help string */
static char const *lorem_ipsum = "Lorem ipsum dolor sit amet, consectetur adipiscing elit, "
    "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
    "veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo "
    "consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore "
    "eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa "
    "qui officia deserunt mollit anim id est laborum.";

/* with some (arbitrary) embedded newlines */
static char const *lorem_ipsum2 = "Lorem ipsum dolor sit amet, consectetur adipiscing elit,\n"
    "sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim\n"
    "veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo\n"
    "consequat. Duis aute irure dolor\nin reprehenderit in voluptate velit\nesse cillum dolore "
    "eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa "
    "qui officia deserunt mollit anim id est laborum.";

typedef enum _TestPasses_t {
    HELP,
    WARN,
    ERROR,
    CHECKVAL,
    ASSIGN_ON,
    ASSIGN_OFF,
    SUB_MODULE
} TestPasses_t;

#define CHECK_VAL_(WHEN, CONDITION, LINE)                        \
    if (!strcmp(argv[1], #WHEN) && !(CONDITION))                   \
    {                                                              \
        printf(#CONDITION " failed " #WHEN " at line %d\n", LINE); \
        abort();                                                   \
    }
#define CHECK_VAL(WHEN, CONDITION) CHECK_VAL_(WHEN, CONDITION, __LINE__)

/* The funky macro magic here is so that the same MACSIO_CLARGS_ProcessCmdline
   code block can be used in two different ways that change the argument list */
#define VVV /*void*/
#define COMMA ,
#define PCL(JSON, MYBOOL, MYINT, MYDOUBLE, MYSTRING, MULTI1, MULTI2,\
    DUMMYD, DUMMYF, ARGN, COMMA, TWO) \
    MACSIO_CLARGS_ProcessCmdline(JSON, argFlags, argi, argc, argv, \
        "--bool-example", "", \
            "An example of a bool argument", \
             MYBOOL COMMA \
        "--int-example %d", "10", \
            "An example of an int argument with default value of 10", \
             MYINT COMMA \
        "--double-example %f", "3.1415926", \
            "An example of a double argument with default value of PI", \
             MYDOUBLE COMMA \
        "--string-example %s", "foobar", \
            "An example of a string argument with default value of \"foobar\"", \
             MYSTRING COMMA \
        "--multiple-args %f %d", "2.5 5", \
            "Example of option with multiple arguments", \
            MULTI1 COMMA MULTI2 COMMA \
        "--long-help-example %d", MACSIO_CLARGS_NODEFAULT, \
            lorem_ipsum, \
            DUMMYD COMMA \
        "--long-help-example2 %d", "42", \
            lorem_ipsum2, \
            DUMMYD COMMA \
        MACSIO_CLARGS_ARG_GROUP_BEG##TWO (Option group 1, Top-level help for option group 1), \
            "--optgrp1 %f", "6.02E+23", \
                "Help for opt1 in group.", \
                DUMMYF COMMA \
            "--optgrp2 %d", "64", \
                "Help for opt2 in group.", \
                DUMMYD COMMA \
        MACSIO_CLARGS_ARG_GROUP_END##TWO(Option group 1), \
        "--one-more-arg %d", "42", \
            "not much help", \
            DUMMYD COMMA \
        "--arg-separator-example %n", MACSIO_CLARGS_NODEFAULT, \
            "argument separator for passing next group of CL arguments to sub-module", \
            ARGN COMMA \
    MACSIO_CLARGS_END_OF_ARGS)

static int ParseCommandLine(int argc, char **argv,
    MACSIO_CLARGS_ArgvFlags_t argFlags, int mode, TestPasses_t pass)
{
    int mybool = -1;
    int myint = 0, myint2 = 0;
    double mydouble = 0;
    double multi1 = 0;
    int multi2 = 0;
    char mystring[256] = {""};
    char *pmystring = &mystring[0];
    float dummyf;
    int dummyd;
    int argi, argn;
    json_object *jargs;
    argi = 1;
#ifndef HAVE_MPI
    /* Run error/die cases in separate executable and just gather return code */
    pid_t newpid = 0;

    if (pass == ERROR)
        newpid = fork();

    if (newpid != 0)
    {
        int status;
        wait(&status);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != EINVAL)
            abort();
        return 0;
    }
#endif

    if (mode == MACSIO_CLARGS_TOMEM)
    {
        PCL(0, &mybool, &myint, &mydouble, &pmystring, &multi1, &multi2, &dummyd, &dummyf, &argn, COMMA, 2);

        if (pass == HELP || pass == WARN || pass == ERROR) return 0;

        if (pass == CHECKVAL)
        {
            CHECK_VAL(--bool-example,   mybool == 1);
            CHECK_VAL(--int-example,    myint == 17);
            CHECK_VAL(--double-example, mydouble == 17.7);
            CHECK_VAL(--string-example, !strcmp(mystring, "gorfo"));
            CHECK_VAL(--multiple-args,  multi1 == 17.7 && multi2 == 17);
        }
        else if (pass == ASSIGN_ON)
        {
            CHECK_VAL(--bool-example, myint == 10 && mydouble == 3.1415926 && !strcmp(mystring, "foobar"));
        }
        else if (pass == ASSIGN_OFF)
        {
            CHECK_VAL(--bool-example, myint == 0 && mydouble == 0 && !strcmp(mystring, ""));
        }
        else if (pass == SUB_MODULE)
        {
            CHECK_VAL(--bool-example, argn == 4);
        }
    }
    else if (mode == MACSIO_CLARGS_TOJSON)
    {
        PCL((void**)&jargs, VVV, VVV, VVV, VVV, VVV, VVV, VVV, VVV, VVV, VVV, /*void*/);

        if (pass == HELP || pass == WARN || pass == ERROR) return 0;

        if (pass == CHECKVAL)
        {
            CHECK_VAL(--bool-example, JsonGetBool(jargs, "bool-example") == JSON_C_TRUE);
            CHECK_VAL(--int-example, JsonGetInt(jargs, "int-example") == 17);
            CHECK_VAL(--double-example, JsonGetDbl(jargs, "double-example") == 17.7);
            CHECK_VAL(--string-example, !strcmp(JsonGetStr(jargs, "string-example"), "gorfo"));
            CHECK_VAL(--multiple-args,  JsonGetDbl(jargs, "multiple-args/0") == 17.7 &&
                                        JsonGetInt(jargs, "multiple-args/1") == 17);
        }
        else if (pass == ASSIGN_ON)
        {
            CHECK_VAL(--bool-example, JsonGetInt(jargs, "int-example") == 10 &&
                                      JsonGetDbl(jargs, "double-example") == 3.1415926 &&
                                     !strcmp(JsonGetStr(jargs, "string-example"), "foobar"))
        }
        else if (pass == ASSIGN_OFF)
        {
            CHECK_VAL(--bool-example, JsonGetInt(jargs, "int-example") == 0 &&
                                      JsonGetDbl(jargs, "double-example") == 0 &&
                                      JsonGetObj(jargs, "string-example") == 0)
        }
        else if (pass == SUB_MODULE)
        {
            CHECK_VAL(--bool-example, JsonGetInt(jargs, "argi") == 4);
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    int i, j;
    int argc2;
    char **argv2;
    MACSIO_CLARGS_ArgvFlags_t argFlags;
#ifdef HAVE_MPI
    MPI_Comm comm = MPI_COMM_WORLD;
#else
    int comm = 0;
#endif

#ifdef HAVE_MPI
    MPI_Init(&argc, &argv);
#endif

    MACSIO_LOG_StdErr = MACSIO_LOG_LogInit(comm, 0, 0, 0, 0);

    /* two iterations; 0) for memory routing, 1) for json routing */
    for (j = 0; j < 2; j++)
    {
        argFlags.route_mode = j==0?MACSIO_CLARGS_TOMEM:MACSIO_CLARGS_TOJSON;
        argFlags.error_mode = MACSIO_CLARGS_WARN;
        argFlags.defaults_mode = MACSIO_CLARGS_ASSIGN_OFF;

        /* Test help output */
        argc2 = 2;
        argv2 = (char **) malloc(argc2 * sizeof(char*));
        argv2[0] = strdup(argv[0]);
        argv2[1] = strdup("--help");
        ParseCommandLine(argc2, argv2, argFlags, argFlags.route_mode, HELP);
        for (i = 0; i < argc2; free(argv2[i]), i++);
        free(argv2);

        /* Test warning */
        argc2 = 2;
        argv2 = (char **) malloc(argc2 * sizeof(char*));
        argv2[0] = strdup(argv[0]);
        argv2[1] = strdup("--foobar");
        ParseCommandLine(argc2, argv2, argFlags, argFlags.route_mode, WARN);
        for (i = 0; i < argc2; free(argv2[i]), i++);
        free(argv2);

#ifndef HAVE_MPI
        /* Test error */
        argFlags.error_mode = MACSIO_CLARGS_ERROR;
        argc2 = 2;
        argv2 = (char **) malloc(argc2 * sizeof(char*));
        argv2[0] = strdup(argv[0]);
        argv2[1] = strdup("--foobar");
        ParseCommandLine(argc2, argv2, argFlags, argFlags.route_mode, ERROR);
        for (i = 0; i < argc2; free(argv2[i]), i++);
        free(argv2);
#endif

        /* Test bool assignment */
        argc2 = 2;
        argv2 = (char **) malloc(argc2 * sizeof(char*));
        argv2[0] = strdup(argv[0]);
        argv2[1] = strdup("--bool-example");
        ParseCommandLine(argc2, argv2, argFlags, argFlags.route_mode, CHECKVAL);
        for (i = 0; i < argc2; free(argv2[i]), i++);
        free(argv2);

        /* Test int assignment */
        argc2 = 3;
        argv2 = (char **) malloc(argc2 * sizeof(char*));
        argv2[0] = strdup(argv[0]);
        argv2[1] = strdup("--int-example");
        argv2[2] = strdup("17");
        ParseCommandLine(argc2, argv2, argFlags, argFlags.route_mode, CHECKVAL);
        for (i = 0; i < argc2; free(argv2[i]), i++);
        free(argv2);

        /* Test double assignment */
        argc2 = 3;
        argv2 = (char **) malloc(argc2 * sizeof(char*));
        argv2[0] = strdup(argv[0]);
        argv2[1] = strdup("--double-example");
        argv2[2] = strdup("17.7");
        ParseCommandLine(argc2, argv2, argFlags, argFlags.route_mode, CHECKVAL);
        for (i = 0; i < argc2; free(argv2[i]), i++);
        free(argv2);

        /* Test string assignment */
        argc2 = 3;
        argv2 = (char **) malloc(argc2 * sizeof(char*));
        argv2[0] = strdup(argv[0]);
        argv2[1] = strdup("--string-example");
        argv2[2] = strdup("gorfo");
        ParseCommandLine(argc2, argv2, argFlags, argFlags.route_mode, CHECKVAL);
        for (i = 0; i < argc2; free(argv2[i]), i++);
        free(argv2);

        /* Test multiple assignments */
        argc2 = 4;
        argv2 = (char **) malloc(argc2 * sizeof(char*));
        argv2[0] = strdup(argv[0]);
        argv2[1] = strdup("--multiple-args");
        argv2[2] = strdup("17.7");
        argv2[3] = strdup("17");
        ParseCommandLine(argc2, argv2, argFlags, argFlags.route_mode, CHECKVAL);
        for (i = 0; i < argc2; free(argv2[i]), i++);
        free(argv2);

        /* Test default assignments */
        argFlags.defaults_mode = MACSIO_CLARGS_ASSIGN_ON;
        argc2 = 2;
        argv2 = (char **) malloc(argc2 * sizeof(char*));
        argv2[0] = strdup(argv[0]);
        argv2[1] = strdup("--bool-example");
        ParseCommandLine(argc2, argv2, argFlags, argFlags.route_mode, ASSIGN_ON);
        for (i = 0; i < argc2; free(argv2[i]), i++);
        free(argv2);

        /* Test default assignments OFF */
        argFlags.defaults_mode = MACSIO_CLARGS_ASSIGN_OFF;
        argc2 = 2;
        argv2 = (char **) malloc(argc2 * sizeof(char*));
        argv2[0] = strdup(argv[0]);
        argv2[1] = strdup("--bool-example");
        ParseCommandLine(argc2, argv2, argFlags, argFlags.route_mode, ASSIGN_OFF);
        for (i = 0; i < argc2; free(argv2[i]), i++);
        free(argv2);

        /* Test arg separator for sub-module */
        argFlags.defaults_mode = MACSIO_CLARGS_ASSIGN_ON;
        argc2 = 7;
        argv2 = (char **) malloc(argc2 * sizeof(char*));
        argv2[0] = strdup(argv[0]);
        argv2[1] = strdup("--bool-example");
        argv2[2] = strdup("--double-example");
        argv2[3] = strdup("17.7");
        argv2[4] = strdup("--arg-separator-example");
        argv2[5] = strdup("--sub-module-arg");
        argv2[6] = strdup("17");
        ParseCommandLine(argc2, argv2, argFlags, argFlags.route_mode, SUB_MODULE);
        for (i = 0; i < argc2; free(argv2[i]), i++);
        free(argv2);
    }

    return 0;
}

