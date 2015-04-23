#include <macsio_clargs.h>
#include <macsio_iface.h>
#include <macsio_log.h>
#include <macsio_main.h>
#include <macsio_mif.h>
#include <macsio_utils.h>

#include <stdio.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

#warning NEED DOXYGEN FILE-SCOPE DELIMITER
/*!
\addtogroup MACSIO_MIFTMPL

This is intended to serve as a template for how to create a MIF-mode plugin for MACSio.
This template code does indeed actually work as a MACSio plugin by writing JSON objects
to the individual files. Each processor in a group that baton-passes a MIF file around,
writes all its json objects at the end of the current file and maintains knowledge of
the offset within the file at which each piece is written. The filenames, offsets and
mesh part numbers are then written out to a root or master file. Currently, there is
no plugin in VisIt to read these files and display them. But, this example code does
help to outline the work to write a MIF plugin.

In practice, this plugin could have simply written the entire json object pass to it
in main_dump to the file. However, in doing that, the resulting file would not "know"
either how many mesh parts there are or where they are located in the file set. So,
we wind up writing json objects for each part individually so that we can keep track
of their offsets.

Be sure to declare all your symbols static. Each plugin is being linked into MACSio's main
and any symbols that are not static file scope will wind up appearing in and therefore being
vulnerable too global namespace collisions.
@{
*/

/* the name you want to assign to the interface */
static char const *iface_name = "miftmpl";
static char const *iface_ext = "json";

/* Examples of static, file scop variables to control plugin behavior */
static int my_opt_one;            /**< Example of a static scope, plugin-specific variable to be set in process_args 
                                       to control plugin behavior */
static int my_opt_two;            /**< Another example variable to control plugin behavior */
static char *my_opt_three_string; /**< Another example variable to control plugin behavior */
static float my_opt_three_float;  /**< Another example variable to control plugin behavior */

/*!
\brief Process command-line arguments specific to this plugin

Uses MACSIO_CLARGS_ProcessCmdline to do its work.

This plugin is implemented to route command line arguments to memory locations
here in the plugin (e.g. the static scope variables). Alternatively, a plugin
can choose to route the results of MACSIO_CLARGS_ProcessCmdline to a json object.
*/
static int process_args(
    int argi,      /**< [in] Argument index of first argument that is specific to this plugin */
    int argc,      /**< [in] argc as passed into main */
    char *argv[]   /**< [in] argv as passed into main */
)
{
    /* Can use MACSIO_CLARGS_TOJSON here instead in which case pass the pointer to
       a json_object* as first arg and eliminate all the pointers to specific
       variables. The args will be returned as a json-c object. */
    const MACSIO_CLARGS_ArgvFlags_t argFlags = {MACSIO_CLARGS_WARN, MACSIO_CLARGS_TOMEM};

    MACSIO_CLARGS_ProcessCmdline(0, argFlags, argi, argc, argv,
        "--my_opt_one",
            "Help message for my_opt_one which has no arguments. If present, local\n"
            "var my_opt_one will be assigned a value of 1 and a value of zero otherwise.",
            &my_opt_one,
        "--my_opt_two %d",
            "Help message for my_opt_two which has a single integer argument",
            &my_opt_two,
        "--my_opt_three %s %f",
            "Help message for my_opt_three which has a string argument and a float argument",
            &my_opt_three_string, &my_opt_three_float,
           MACSIO_CLARGS_END_OF_ARGS);

    return 0;
}

/*!
\brief Example of user data for MIF callbacks

*/
typedef struct _user_data {
    unsigned long long dirId; /**< Just example member for user-data for MIF callbacks */
    char someString[64];      /**< Just example member for user-data for MIF callbacks */
} user_data_t;

/*!
\brief CreateFile MIF Callback

\return A void pointer to the plugin-specific file handle
*/
static void *CreateMyFile( 
    const char *fname,     /**< [in] Name of the MIF file to create */
    const char *nsname,    /**< [in] Name of the namespace within the file for caller should use. */
    void *userData         /**< [in] Optional plugin-specific user-defined data */
)
{
    FILE *file = fopen(fname, "w");
    return (void *) file;
}

/*!
\brief OpenFile MIF Callback

\return A void pointer to the plugin-specific file handle
*/
static void *OpenMyFile(
    const char *fname,            /**< [in] Name of the MIF file to open */
    const char *nsname,           /**< [in] Name of the namespace within the file caller should use */
    MACSIO_MIF_ioFlags_t ioFlags, /**< [in] Various flags indicating behavior/options */
    void *userData                /**< [in] Optional plugin-specific user-defined data */
)
{
    FILE *file = fopen(fname, "a+");
    return (void *) file;
}

/*!
\brief CloseFile MIF Callback
*/
static void CloseMyFile(
    void *file,      /**< [in] A void pointer to the plugin specific file handle */
    void *userData   /**< [in] Optional plugin specific user-defined data */
)
{
    fclose((FILE*) file);
}

/*!
\brief Write a single mesh part to a MIF file
*/
static json_object *write_mesh_part(
    FILE *myFile,          /**< [in] The a file handle being used in a MIF dump */
    char const *fileName,  /**< [in] Name of the MIF file */
    json_object *part_obj  /**< [in] The json object representing this mesh part */
)
{
    json_object *part_info = json_object_new_object();

    /* Write the json mesh part object as an ascii string */
    fprintf(myFile, "%s\n", json_object_to_json_string_ext(part_obj, JSON_C_TO_STRING_PRETTY));
    json_object_free_printbuf(part_obj);

    json_object_object_add(part_info, "partid",
        json_object_new_int(json_object_path_get_int(part_obj, "Mesh/ChunkID")));
    json_object_object_add(part_info, "file",
        json_object_new_string(fileName));
    json_object_object_add(part_info, "offset",
        json_object_new_double((double) ftello(myFile)));

    return part_info;
}

/*!
\brief Main MIF dump implementation for this plugin
*/
static void main_dump(
    int argi,               /**< [in] Command-line argument index at which first plugin-specific arg appears */
    int argc,               /**< [in] argc from main */
    char **argv,            /**< [in] argv from main */
    json_object *main_obj,  /**< [in] The main json object representing all data to be dumped */
    int dumpn,              /**< [in] The number/index of this dump. Each dump in a sequence gets a unique,
                                      monotone increasing index starting from 0 */
    double dumpt            /**< [in] The time to be associated with this dump (like a simulation's time) */
)
{
    int i, rank, numFiles;
    char fileName[256];
    FILE *myFile;
    MACSIO_MIF_ioFlags_t ioFlags = {MACSIO_MIF_WRITE, JsonGetInt(main_obj, "clargs/exercise_scr")&0x1};
    MACSIO_MIF_baton_t *bat;
    json_object *parts;
    json_object *part_infos = json_object_new_array();

    /* process cl args */
    process_args(argi, argc, argv);

    /* ensure we're in MIF mode and determine the file count */
#warning SIMPLIFY THIS LOGIC USING NEW JSON INTERFACE
    json_object *parfmode_obj = json_object_path_get_array(main_obj, "clargs/parallel_file_mode");
    if (parfmode_obj)
    {
        json_object *modestr = json_object_array_get_idx(parfmode_obj, 0);
        json_object *filecnt = json_object_array_get_idx(parfmode_obj, 1);
        if (!strcmp(json_object_get_string(modestr), "SIF"))
        {
            MACSIO_LOG_MSG(Die, ("miftmpl plugin cannot currently handle SIF mode"));
        }
        else
        {
            numFiles = json_object_get_int(filecnt);
        }
    }
    else
    {
        char const * modestr = json_object_path_get_string(main_obj, "clargs/parallel_file_mode");
        if (!strcmp(modestr, "SIF"))
        {
            MACSIO_LOG_MSG(Die, ("miftmpl plugin cannot currently handle SIF mode"));
        }
        else if (!strcmp(modestr, "MIFMAX"))
            numFiles = json_object_path_get_int(main_obj, "parallel/mpi_size");
        else if (!strcmp(modestr, "MIFAUTO"))
        {
            /* Call MACSio utility to determine optimal file count */
        }
    }

    bat = MACSIO_MIF_Init(numFiles, ioFlags, MACSIO_MAIN_Comm, 3,
        CreateMyFile, OpenMyFile, CloseMyFile, 0);

    rank = json_object_path_get_int(main_obj, "parallel/mpi_rank");

    /* Construct name for the silo file */
    sprintf(fileName, "%s_json_%05d_%03d.%s",
        json_object_path_get_string(main_obj, "clargs/filebase"),
        MACSIO_MIF_RankOfGroup(bat, rank),
        dumpn,
        json_object_path_get_string(main_obj, "clargs/fileext"));

    myFile = (FILE *) MACSIO_MIF_WaitForBaton(bat, fileName, 0);

    parts = json_object_path_get_array(main_obj, "problem/parts");
    for (int i = 0; i < json_object_array_length(parts); i++)
    {
        json_object *this_part = json_object_array_get_idx(parts, i);
        json_object_array_add(part_infos, write_mesh_part(myFile, fileName, this_part));
    }

    /* Hand off the baton to the next processor. This winds up closing
     * the file so that the next processor that opens it can be assured
     * of getting a consistent and up to date view of the file's contents. */
    MACSIO_MIF_HandOffBaton(bat, myFile);

    /* We're done using MACSIO_MIF for these files, so finish it off */
    MACSIO_MIF_Finish(bat);

    /* Use MACSIO_MIF a second time to manage writing of the master/root
       file contents. This winds up being serial I/O but also means we
       never collect all info on all parts to any single processor. */
#warning THERE IS A BETTER WAY TO DO USING LOOP OF NON-BLOCKING RECIEVES
    bat = MACSIO_MIF_Init(1, ioFlags, MACSIO_MAIN_Comm, 5,
        CreateMyFile, OpenMyFile, CloseMyFile, 0);

    /* Construct name for the silo file */
    sprintf(fileName, "%s_json_root_%03d.%s",
        json_object_path_get_string(main_obj, "clargs/filebase"),
        dumpn,
        json_object_path_get_string(main_obj, "clargs/fileext"));

    myFile = (FILE *) MACSIO_MIF_WaitForBaton(bat, fileName, 0);

    fprintf(myFile, "%s\n", json_object_to_json_string_ext(part_infos, JSON_C_TO_STRING_PRETTY));

    MACSIO_MIF_HandOffBaton(bat, myFile);

    MACSIO_MIF_Finish(bat);

    /* decriment ref-count (and free) part_infos */
    json_object_put(part_infos);
}

/*!
\brief Method to register this plugin with MACSio main

Due to its use to initialize a file-scope, static const variable, this
function winds up being called at load time (e.g. before main is even called).

Its purpose is to add key information about this plugin to MACSio's interface
table.
*/
static int register_this_interface()
{
    MACSIO_IFACE_Handle_t iface;

    if (strlen(iface_name) >= MACSIO_IFACE_MAX_NAME)
        MACSIO_LOG_MSG(Die, ("Interface name \"%s\" too long", iface_name));

    /* Populate information about this plugin */
    strcpy(iface.name, iface_name);
    strcpy(iface.ext, iface_ext);
    iface.dumpFunc = main_dump;
    iface.processArgsFunc = process_args;

    /* Register this plugin */
    if (!MACSIO_IFACE_Register(&iface))
        MACSIO_LOG_MSG(Die, ("Failed to register interface \"%s\"", iface_name));

    return 0;
}

/*!
\brief Dummy initializer to trigger register_this_interface by the loader

This one statement is the only statement requiring compilation by
a C++ compiler. That is because it involves initialization and non
constant expressions (a function call in this case). This function
call is guaranteed to occur during *initialization* (that is before
even 'main' is called) and so will have the effect of populating the
iface_map array merely by virtue of the fact that this code is linked
with a main.
*/
static int const dummy = register_this_interface();

/*!@}*/
