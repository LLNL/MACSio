/* Written by James Dickson */

#include <math.h>
#include <time.h>
#include <unistd.h>

#include <macsio_clargs.h>
#include <macsio_iface.h>
#include <macsio_log.h>
#include <macsio_main.h>
#include <macsio_mif.h>
#include <macsio_msf.h>
#include <macsio_utils.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

#include <hdf5.h>
#include <typhonio.h>

/*!
\addtogroup plugins
@{
*/

/*!
\addtogroup TyphonIO
@{
*/

static char const *iface_name = "typhonio";
static char const *iface_ext = "h5";
static char *filename;

static int no_collective = 0; /**< Controls whether collective I/O will be enabled in TyphonIO */
static const char *romio_cb_write;
static const char *romio_ds_write;
static const char *striping_factor;

char  errstr[TIO_STRLEN];
TIO_t errnum;


/*!
\brief Convenience macro for issuing calls to TyphonIO and interpreting error codes
*/
#define TIO_Call(r,s) if((errnum=r) != TIO_SUCCESS) {TIO_Get_Error(errnum, errstr); printf("%s\n%s\n",s,errstr); exit(EXIT_FAILURE);}


/*!
\brief Process command-line arguments specific to this plugin

Uses MACSIO_CLARGS_ProcessCmdline() to do its work.

This example plugin is implemented to route command line arguments to memory locations
(e.g. static variables) here in the plugin.  Alternatively, a plugin can choose to
route the results of MACSIO_CLARGS_ProcessCmdline() to a JSON object. MACSio's main is
implemented that way.
*/
static int process_args(
    int argi,      /**< [in] Argument index of first argument that is specific to this plugin */
    int argc,      /**< [in] argc as passed into main */
    char *argv[])  /**< [in] argv as passed into main */
{
    /* Can use MACSIO_CLARGS_TOJSON here instead in which case pass the pointer to
       a json_object* as first arg and eliminate all the pointers to specific
       variables. The args will be returned as a json-c object. */
    const MACSIO_CLARGS_ArgvFlags_t argFlags = {MACSIO_CLARGS_WARN, MACSIO_CLARGS_TOMEM};

    MACSIO_CLARGS_ProcessCmdline(0, argFlags, argi, argc, argv,
	"--romio_cb_write %s", MACSIO_CLARGS_NODEFAULT,
	    "Explicitly set the mpi file hint for cb_write.",
	    &romio_cb_write,
	"--romio_ds_write %s", MACSIO_CLARGS_NODEFAULT,
	    "Explicitly set the mpi file hint for ds write.",
	    &romio_ds_write,
	"--striping_factor %s", MACSIO_CLARGS_NODEFAULT,
	    "Explicitly set the mpi file hint for striping factor.",
	    &striping_factor,
        "--no_collective", "",
            "Use independent, not collective I/O calls in SIF mode.",
            &no_collective,
	MACSIO_CLARGS_END_OF_ARGS);

    return 0;
}

/*!
\brief Return the current date as a formatted string

This uses built in time/date functions to take the current date and format to a string
for use as metadata in the TyphonIO file.

\return A string in the format 'DayOFWeek dd-mm-yyyy hh:mm' 
*/
static char *getDate()
{
    time_t     now;
    char       date[TIO_STRLEN];
    struct tm  *ts;

    time(&now);
    ts = localtime(&now);
    strftime(date, sizeof(date), "%a %d-%m-%Y %H:%M", ts);
    char *ret = (char*) malloc(20*sizeof(char));
    strcpy(ret, date);
    return ret;
}

/*!
\brief CreateFile MIF Callback

This implments the MACSIO_MIF_CreateFile callback needed for a MIF mode plugin.

\return A void pointer to the plugin-specific file handle
*/

static void *CreateTyphonIOFile(
    const char *fname,     /**< [in] Name of the MIF file to create */
    const char *nsname,    /**< [in] Name of the namespace within the file for caller should use. */
    void *userData)        /**< [in] Optional plugin-specific user-defined data */
{
    TIO_File_t *retval = 0;
    TIO_File_t file_id;
    //MPI_Comm *groupComm = (MPI_Comm*)userData;
    char *date = getDate();
    TIO_Call( TIO_Create(fname, &file_id, TIO_ACC_REPLACE, "MACSio",
                         "0.9", date, (char*)fname, MPI_COMM_SELF, MPI_INFO_NULL, MACSIO_MAIN_Rank),
              "File Creation Failed\n");
    if (file_id >= 0)
    {
        retval = (TIO_File_t *) malloc(sizeof(TIO_File_t));
        *retval = file_id;
    }
    return (void *) retval;
}

/*!
\brief OpenFile MIF Callback

This implments the MACSIO_MIF_OpenFile callback needed for a MIF mode plugin.

\return A void pointer to the plugin-specific file handle
*/
static void *OpenTyphonIOFile(
    const char *fname,              /**< [in] Name of the MIF file to create */ 
    const char *nsname,             /**< [in] Name of the namespace within the file the caller should use */
    MACSIO_MIF_ioFlags_t ioFlags,   /**< [in] Various flags indicating behavior/options */
    void *userData)                 /**< [in] Optional plugin-specific user-defined data */
{
    TIO_File_t *retval = 0;
    TIO_File_t file_id;

    char *date = getDate();
    TIO_Call( TIO_Open(fname, &file_id, TIO_ACC_READWRITE, "MACSio",
                       "0.9", date, (char*)fname, MPI_COMM_SELF, MPI_INFO_NULL, MACSIO_MAIN_Rank),
              "File Open Failed\n");
    if (file_id >= 0)
    {
        retval = (TIO_File_t *) malloc(sizeof(TIO_File_t));
        *retval = file_id;
    }
    return (void *) retval;
}

/*!
\brief CloseFile MIF Callback

This implments the MACSIO_CloseFile callback needed for a MIF mode plugin.
*/
static int CloseTyphonIOFile(
    void *file,         /**< [in] A void pointer to the plugin specific file handle */
    void *userData)     /**< [in] Optional plugin specific user-defined data */
{
    TIO_Call( TIO_Close(*(TIO_File_t*)file),
              "File Close Failed\n");
    return 0;
}

/*!
\brief Write a single quad mesh part to a MIF file

This method serializes the JSON object for the mesh part and then
appends/writes to the end of the current file.

This method is used for both Rectilinear (Colinear) and Curvilinear (Non-Colinear)
Quad meshes.
*/ 
static void write_quad_mesh_part(
    TIO_File_t file_id,         /**< [in] The file id being used in MIF dump */ 
    TIO_Object_t state_id,      /**< [in] The state id of the current dump */
    json_object *part_obj,      /**< [in] JSON object representing this mesh part */
    TIO_Mesh_t mesh_type)       /**< [in] Type of mesh [TIO_MESH_QUAD_COLINEAR/TIO_MESH_QUAD_NONCOLINEAR] */
{
    TIO_Object_t mesh_id;
    json_object *coordobj;
    void const *coords[3];
    int ndims = JsonGetInt(part_obj, "Mesh/GeomDim");
    int dims[3] = {1,1,1};
    int dimsz[3] = {1,1,1};

    if (mesh_type == TIO_MESH_QUAD_COLINEAR){
        switch(ndims){
            case 3:
                dims[2] = JsonGetInt(part_obj, "Mesh/LogDims", 2);
                coordobj = JsonGetObj(part_obj, "Mesh/Coords/ZAxisCoords");
                coords[2] = json_object_extarr_data(coordobj);
            case 2:
                dims[1] = JsonGetInt(part_obj, "Mesh/LogDims", 1);
                coordobj = JsonGetObj(part_obj, "Mesh/Coords/YAxisCoords");
                coords[1] = json_object_extarr_data(coordobj);
            default:
                dims[0] = JsonGetInt(part_obj, "Mesh/LogDims", 0);
                coordobj = JsonGetObj(part_obj, "Mesh/Coords/XAxisCoords");
                coords[0] = json_object_extarr_data(coordobj);
        }
    } else {
        switch(ndims){
            case 3:
                dims[2] = JsonGetInt(part_obj, "Mesh/LogDims", 2);
                coordobj = JsonGetObj(part_obj, "Mesh/Coords/ZCoords");
                coords[2] = json_object_extarr_data(coordobj);
            case 2:
                dims[1] = JsonGetInt(part_obj, "Mesh/LogDims", 1);
                coordobj = JsonGetObj(part_obj, "Mesh/Coords/YCoords");
                coords[1] = json_object_extarr_data(coordobj);
            default:
                dims[0] = JsonGetInt(part_obj, "Mesh/LogDims", 0);
                coordobj = JsonGetObj(part_obj, "Mesh/Coords/XCoords");
                coords[0] = json_object_extarr_data(coordobj);
        }   
    }

    TIO_Call( TIO_Create_Mesh(file_id, state_id, "mesh", &mesh_id, mesh_type, 
                            TIO_COORD_CARTESIAN, TIO_FALSE, "mesh_group", (TIO_Size_t)1,
                            TIO_DATATYPE_NULL, TIO_DOUBLE, (TIO_Dims_t)ndims,
                            (TIO_Size_t)dims[0], (TIO_Size_t)dims[1], (TIO_Size_t)dims[2],
                            TIO_GHOSTS_NONE, (TIO_Size_t)1,
                            NULL, NULL, NULL,
                            NULL, NULL, NULL),
                        "Create Mesh Failed\n");

    if (mesh_type == TIO_MESH_QUAD_COLINEAR){
        TIO_Call( TIO_Set_Quad_Chunk(file_id, mesh_id, (TIO_Size_t)0, (TIO_Dims_t)ndims,
                            0, dims[0]-1, 0, dims[1]-1, 0, dims[2]-1,
                            0, 0),
            "Set Quad Mesh Chunk Failed");
        TIO_Call( TIO_Write_QuadMesh_All(file_id, mesh_id, TIO_DOUBLE, coords[0], coords[1], coords[2]),
                    "Write Mesh Coords failed\n");
    } 
    else 
    {
        TIO_Call( TIO_Set_Quad_Chunk(file_id, mesh_id, (TIO_Size_t)0, (TIO_Dims_t)ndims,
                            0, dims[0]-1, 0, dims[1]-1, 0, dims[2]-1,
                            0, 0),
            "Set Quad Mesh Chunk Failed");
        TIO_Call( TIO_Write_QuadMesh_Chunk(file_id, mesh_id, 0, TIO_XFER_INDEPENDENT, 
                                            TIO_DOUBLE, coords[0], coords[1], coords[2]),
                    "Write Non-Colinear Mesh Coords failed\n");
    }

    TIO_Object_t variable_id;
    json_object *vars_array = json_object_path_get_array(part_obj, "Vars");

    for (int i = 0; i < json_object_array_length(vars_array); i++)
    {
        int j;
        TIO_Size_t var_dims[3];
        TIO_Object_t var_id;
        json_object *var_obj = json_object_array_get_idx(vars_array, i);
        json_object *data_obj = json_object_path_get_extarr(var_obj, "data");
        char const *varname = json_object_path_get_string(var_obj, "name");
        char *centering = strdup(json_object_path_get_string(var_obj, "centering"));
        TIO_Centre_t tio_centering = strcmp(centering, "zone") ? TIO_CENTRE_NODE : TIO_CENTRE_CELL;
        int ndims = json_object_extarr_ndims(data_obj);
        void const *buf = json_object_extarr_data(data_obj);

        TIO_Dims_t ndims_tio = (TIO_Dims_t)ndims;

        TIO_Data_t dtype_id = json_object_extarr_type(data_obj) == json_extarr_type_flt64 ?
                              TIO_DOUBLE : TIO_INT;

        for (j = 0; j < ndims; j++)
            var_dims[j] = json_object_extarr_dim(data_obj, j);

        TIO_Call( TIO_Create_Quant(file_id, mesh_id, varname, &var_id, dtype_id, tio_centering,
                                TIO_GHOSTS_NONE, TIO_FALSE, "qunits"),
                    "Create Var Quant Failed\n");

        TIO_Call( TIO_Write_QuadQuant_Chunk(file_id, var_id, (TIO_Size_t)0, TIO_XFER_INDEPENDENT,
                                            dtype_id, buf, (void*)TIO_NULL),
                    "Write Quad Quant Var Failed\n");
        TIO_Call( TIO_Close_Quant(file_id, var_id),
                    "Close Quant Failed\n");

    }
    TIO_Call( TIO_Close_Mesh(file_id, mesh_id),
        "Close Mesh failed\n");

}

/*!
\brief Write a single unstructured mesh part to a MIF file

This method serializes the JSON object for the mesh part and then
appends/writes to the end of the current file.

This method is used for both Unstructured Zoo and Arbitraty 
Unstructured meshes.
*/ 
static void write_ucdzoo_mesh_part(
    TIO_File_t file_id,     /**< [in] The file id being used in MIF dump */
    TIO_Object_t state_id,  /**< [in] The state id of the current dump */
    json_object *part_obj,  /**< [in] JSON object representing this mesh part */
    char const *topo_name)  /**< [in] Topology of unstructured mesh part [UCDZOO/Arbitrary] */
{
    TIO_Object_t mesh_id;
    json_object *coordobj, *topoobj;
    char const *coordnames[] = {"X", "Y", "Z"};
    void const *coords[3];
    int ndims = JsonGetInt(part_obj, "Mesh/GeomDim");
    int nnodes = 1, nzones = 1;
    int dims[3] = {1,1,1};
    int dimsz[3] = {1,1,1};

    switch(ndims){
        case 3:
        coordobj = JsonGetObj(part_obj, "Mesh/Coords/ZCoords");
            coords[2] = json_object_extarr_data(coordobj);
            dims[2] = JsonGetInt(part_obj, "Mesh/LogDims", 2);
            dimsz[2] = dims[2]-1;
            nnodes *= dims[2];
            nzones *= dimsz[2];
        case 2:
            coordobj = JsonGetObj(part_obj, "Mesh/Coords/YCoords");
            coords[1] = json_object_extarr_data(coordobj);
            dims[1] = JsonGetInt(part_obj, "Mesh/LogDims", 1);
            dimsz[1] = dims[1]-1;
            nnodes *= dims[1];
            nzones *= dimsz[1];
        default:
            coordobj = JsonGetObj(part_obj, "Mesh/Coords/XCoords");
            coords[0] = json_object_extarr_data(coordobj);
            dims[0] = JsonGetInt(part_obj, "Mesh/LogDims", 0);
            dimsz[0] = dims[0]-1;
            nnodes *= dims[0];
            nzones *= dimsz[0];         
    }


    if (ndims == 1 || !strcmp(topo_name, "ucdzoo"))
    /* UCDZOO */
    {
        json_object *topoobj = JsonGetObj(part_obj, "Mesh/Topology");
        json_object *nlobj = JsonGetObj(topoobj, "Nodelist");
        void const *nodelist = (void const*) json_object_extarr_data(nlobj);
        int lnodelist = json_object_extarr_nvals(nlobj);
        TIO_Shape_t shapetype; 
        int shapesize;
        int shapecnt = nzones;
        int ncells = nzones;

        if (!strcmp(JsonGetStr(topoobj, "ElemType"), "Beam2"))
        {
            shapesize = 2;
            shapetype = TIO_SHAPE_BAR2;
        }
        else if (!strcmp(JsonGetStr(topoobj, "ElemType"), "Quad4"))
        {
            shapesize = 4;
            shapetype = TIO_SHAPE_QUAD4;
        }
        else if (!strcmp(JsonGetStr(topoobj, "ElemType"), "Hex8"))
        {
            shapesize = 8;
            shapetype = TIO_SHAPE_HEX8;
        }

        TIO_Size_t nshapes = 1;
        TIO_Size_t nconnectivity = 0;//ncells*shapesize;

         /* For unstructured: n1=nnodes, n2=ncells, n3=nshapes, n4=nconnectivity */

        TIO_Call( TIO_Create_Mesh(file_id, state_id, "mesh", &mesh_id, TIO_MESH_UNSTRUCT,
                                TIO_COORD_CARTESIAN, TIO_FALSE, "mesh_group", (TIO_Size_t)1,
                                TIO_INT, TIO_DOUBLE, (TIO_Dims_t)ndims, 
                                (TIO_Size_t)nnodes, (TIO_Size_t)ncells, nshapes,
                                nconnectivity, (TIO_Size_t)1,
                                NULL, NULL, NULL,
                                NULL, NULL, NULL),
                            "Create Mesh Failed\n");

        TIO_Call( TIO_Set_Unstr_Chunk(file_id, mesh_id, (TIO_Size_t)0, (TIO_Dims_t)ndims, (TIO_Size_t)nnodes,
                                ncells, nshapes, nconnectivity, 0, 0, 0, 0, 0, 0),
                            "Set UCDZOO Mesh Chunk Failed\n");

        TIO_Call( TIO_Write_UnstrMesh_Chunk(file_id, mesh_id, (TIO_Size_t)0, TIO_XFER_INDEPENDENT,
                                        TIO_INT, TIO_DOUBLE, nodelist, nodelist, &shapetype, 
                                        &ncells, (const void*)NULL, coords[0], coords[1], coords[2]),
                            "Write unstructured Mesh Failed\n");
    }
    else if (!strcmp(topo_name, "arbitrary"))
    /* ARBITRARY */
    {
        json_object *topoobj = JsonGetObj(part_obj, "Mesh/Topology");
        json_object *nlobj = JsonGetObj(topoobj, "Nodelist");
        void const *nodelist = (void const*) json_object_extarr_data(nlobj);
        json_object *ncobj = JsonGetObj(topoobj, "NodeCounts");
        int const *nodecnt = (int const *) json_object_extarr_data(ncobj);
        int nfaces = json_object_extarr_nvals(ncobj);
        json_object *flobj = JsonGetObj(topoobj, "Facelist");
        int const *facelist = (int const *) json_object_extarr_data(flobj);
        int lfacelist = json_object_extarr_nvals(flobj);
        json_object *fcobj = JsonGetObj(topoobj, "FaceCounts");
        int const *facecnt = (int const *) json_object_extarr_data(fcobj);

        int ncells = nzones;
        TIO_Size_t nshapes = MACSIO_MAIN_Size;
        TIO_Size_t nconnectivity = 0;//ncells*shapesize;
        TIO_Shape_t *shapetype = (TIO_Shape_t*)malloc(nshapes * sizeof(TIO_Shape_t));
        for (int s = 0; s<nshapes; s++){
            shapetype[s] = (TIO_Shape_t)4; // Abribtrary polygon with 4 nodes
        }  

        TIO_Call( TIO_Create_Mesh(file_id, state_id, "mesh", &mesh_id, TIO_MESH_UNSTRUCT,
                                TIO_COORD_CARTESIAN, TIO_FALSE, "mesh_group", (TIO_Size_t)1,
                                TIO_INT, TIO_DOUBLE, (TIO_Dims_t)ndims,
                                (TIO_Size_t)nnodes, (TIO_Size_t)ncells, nshapes,
                                nconnectivity, (TIO_Size_t)1,
                                NULL, NULL, NULL,
                                NULL, NULL, NULL),
                            "Create Arbitrary Unstructured Mesh Failed\n");

        TIO_Call( TIO_Set_Unstr_Chunk(file_id, mesh_id, (TIO_Size_t)0, (TIO_Dims_t)ndims, (TIO_Size_t)nnodes,
                                ncells, nshapes, nconnectivity, 0, 0, 0, 0, 0, 0),
                            "Set Arbitrary Mesh Chunk Failed");

         TIO_Call( TIO_Write_UnstrMesh_Chunk(file_id, mesh_id, (TIO_Size_t)0, TIO_XFER_INDEPENDENT,
                                        TIO_INT, TIO_DOUBLE, nodelist, nodelist, shapetype, 
                                        &ncells, (const void*)NULL, coords[0], coords[1], coords[2]),
                            "Write unstructured Mesh Failed\n");
         free(shapetype);
    }   

    TIO_Object_t variable_id;
    json_object *vars_array = json_object_path_get_array(part_obj, "Vars");

    for (int i = 0; i < json_object_array_length(vars_array); i++)
    {
        int j;
        TIO_Size_t var_dims[3];
        TIO_Object_t var_id;
        json_object *var_obj = json_object_array_get_idx(vars_array, i);
        json_object *data_obj = json_object_path_get_extarr(var_obj, "data");
        char const *varname = json_object_path_get_string(var_obj, "name");
        char *centering = strdup(json_object_path_get_string(var_obj, "centering"));
        TIO_Centre_t tio_centering = strcmp(centering, "zone") ? TIO_CENTRE_NODE : TIO_CENTRE_CELL;
        int ndims = json_object_extarr_ndims(data_obj);
        void const *buf = json_object_extarr_data(data_obj);

        TIO_Data_t dtype_id = json_object_extarr_type(data_obj) == json_extarr_type_flt64 ?
                              TIO_DOUBLE : TIO_INT;

        for (j = 0; j < ndims; j++)
            var_dims[j] = json_object_extarr_dim(data_obj, j);

        TIO_Call( TIO_Create_Quant(file_id, mesh_id, varname, &var_id, dtype_id, tio_centering,
                                TIO_GHOSTS_NONE, TIO_FALSE, "quints"),
                        "Unstructured Quant Create Failed\n");
        TIO_Call( TIO_Write_UnstrQuant_Chunk(file_id, var_id, (TIO_Size_t)0, TIO_XFER_INDEPENDENT,
                                        dtype_id, buf, (void*)TIO_NULL),
                        "Write Unstructured Quant Chunk Failed\n");
        TIO_Call( TIO_Close_Quant(file_id, var_id),
                        "Close Unstructured Quant Failed\n");
    }       
    TIO_Call( TIO_Close_Mesh(file_id, mesh_id),
                "Close Mesh Failed\n");
    
}

/*!
\brief Calls the relevant method for different mesh types writing a MIF dump

This method checks the JSON object for the type of mesh and passes writing to
the correct method with a corresponding mesh type flag
*/  
static void write_mesh_part(
    TIO_File_t file_id,             /**< [in] The file id being used in MIF dump */
    TIO_Object_t state_id,          /**< [in] The state id of the current dump */
    json_object *part_obj)  /**< [in] JSON object representing this mesh part */
{
    if (!strcmp(JsonGetStr(part_obj, "Mesh/MeshType"), "rectilinear"))
        write_quad_mesh_part(file_id, state_id, part_obj, TIO_MESH_QUAD_COLINEAR);
    else if (!strcmp(JsonGetStr(part_obj, "Mesh/MeshType"), "curvilinear"))
        write_quad_mesh_part(file_id, state_id, part_obj, TIO_MESH_QUAD_NONCOLINEAR);
    else if (!strcmp(JsonGetStr(part_obj, "Mesh/MeshType"), "ucdzoo"))
        write_ucdzoo_mesh_part(file_id, state_id, part_obj, "ucdzoo");
    else if (!strcmp(JsonGetStr(part_obj, "Mesh/MeshType"), "arbitrary"))
        write_ucdzoo_mesh_part(file_id, state_id, part_obj, "arbitrary");
}

/*!
\brief Struct container for id of group and communicator
*/
typedef struct _group_data {
    TIO_t groupId;
    MPI_Comm groupComm;
} group_data_t;

/*!
\brief Main MIF dump implementation for the plugin

This function is called to handle MIF file dumps.

It uses \ref MACSIO_MIF for the main dump, passing off to \ref write_mesh_part 
to choose the correct functions based on mesh type
*/
static void main_dump_mif(
    json_object *main_obj,  /**< [in] The main JSON object representing all data to be dumped */
    int numFiles,           /**< [in] Number of files in the output dump */         
    int dumpn,              /**< [in] The number/index of this dump */
    double dumpt)           /**< [in] The time the be associated with this dump */
{
    int size, rank;
    TIO_t *tioFile_ptr;
    TIO_File_t tioFile;
    TIO_Object_t tioGroup;
    char fileName[256];
    int i, len;
    int *theData;
    group_data_t userData;
    MACSIO_MIF_ioFlags_t ioFlags = {MACSIO_MIF_WRITE, JsonGetInt(main_obj, "clargs/exercise_scr") & 0x1};

//#warning SET FILE AND DATASET PROPERTIES
//#warning DIFFERENT MPI TAGS FOR DIFFERENT PLUGINS AND CONTEXTS
    MACSIO_MIF_baton_t *bat = MACSIO_MIF_Init(numFiles, ioFlags, MACSIO_MAIN_Comm, 1,
                              CreateTyphonIOFile, OpenTyphonIOFile, CloseTyphonIOFile, &userData);

    rank = json_object_path_get_int(main_obj, "parallel/mpi_rank");
    size = json_object_path_get_int(main_obj, "parallel/mpi_size");

    /* Construct name for the typhonio file */
    sprintf(fileName, "%s_typhonio_%05d_%03d.%s",
            json_object_path_get_string(main_obj, "clargs/filebase"),
            MACSIO_MIF_RankOfGroup(bat, rank),
            dumpn,
            "h5");//json_object_path_get_string(main_obj, "clargs/fileext"));

    MACSIO_UTILS_RecordOutputFiles(dumpn, fileName);

    tioFile_ptr = (TIO_t *) MACSIO_MIF_WaitForBaton(bat, fileName, 0);
    tioFile = *tioFile_ptr;
    tioGroup = userData.groupId;

    json_object *parts = json_object_path_get_array(main_obj, "problem/parts");

    for (int i = 0; i < json_object_array_length(parts); i++)
    {
        char domain_dir[256];
        json_object *this_part = json_object_array_get_idx(parts, i);
        TIO_Object_t domain_group_id;

        snprintf(domain_dir, sizeof(domain_dir), "domain_%07d",
                 json_object_path_get_int(this_part, "Mesh/ChunkID"));

        TIO_Call( TIO_Create_State(tioFile, domain_dir, &domain_group_id, 1, (TIO_Time_t)0.0, "us"),
                  "State Create Failed\n");

        write_mesh_part(tioFile, domain_group_id, this_part);

        TIO_Call( TIO_Close_State(tioFile, domain_group_id),
                  "State Close Failed\n");
    }

    /* Hand off the baton to the next processor. This winds up closing
     * the file so that the next processor that opens it can be assured
     * of getting a consistent and up to date view of the file's contents. */
    MACSIO_MIF_HandOffBaton(bat, tioFile_ptr);

    /* We're done using MACSIO_MIF, so finish it off */
    MACSIO_MIF_Finish(bat);
}

static void write_quad_mesh_whole(
    MACSIO_MSF_baton_t const *bat,
    TIO_File_t file_id,     /**< [in] The file id being used in SIF dump */
    TIO_Object_t state_id,  /**< [in] The state id of the current dump */
    json_object *main_obj,  /**< [in] The main JSON object containing mesh data */
    TIO_Mesh_t mesh_type)   /**< [in] Type of mesh [TIO_MESH_QUAD_COLINEAR/TIO_MESH_QUAD_NONCOLINEAR] */    
{
    TIO_Object_t mesh_id;
    TIO_Object_t object_id;
    int ndims;
    int i, v, p;
    int use_part_count;
    const TIO_Xfer_t TIO_XFER = no_collective ? TIO_XFER_INDEPENDENT : TIO_XFER_COLLECTIVE;

    TIO_Size_t dims[3];

    ndims = json_object_path_get_int(main_obj, "clargs/part_dim");

    /* Get the list of vars on the first part as a guide to loop over vars */
    json_object *part_array = json_object_path_get_array(main_obj, "problem/parts");
    json_object *first_part_obj = json_object_array_get_idx(part_array, 0);
    json_object *first_part_vars_array = json_object_path_get_array(first_part_obj, "Vars");

    json_object *part_log_dims_array = json_object_path_get_array(first_part_obj, "Mesh/LogDims");

    for (i = 0; i < ndims; i++)
    {      
        dims[i] = (TIO_Size_t) JsonGetInt(part_log_dims_array, "", i) * MACSIO_MSF_SizeOfGroup(bat);
    }

    /* Loop over vars and then over parts */
    /* currently assumes all vars exist on all ranks. but not all parts */
    for (v = -1; v < json_object_array_length(first_part_vars_array); v++) /* -1 start is for Mesh */
    {
        /* Inspect the first part's var object for name, datatype, etc. */
        json_object *var_obj = json_object_array_get_idx(first_part_vars_array, v);

        char *centering = (char*)malloc(sizeof(char)*5);
        TIO_Data_t dtype_id;

        //MESH
        TIO_Dims_t ndims_tio = (TIO_Dims_t)ndims;

        if (v == -1) { 
            TIO_Call( TIO_Create_Mesh(file_id, state_id, "mesh", &mesh_id, mesh_type,
                                      TIO_COORD_CARTESIAN, TIO_FALSE, "mesh_group", (TIO_Size_t)1,
                                      TIO_DATATYPE_NULL, TIO_DOUBLE, ndims_tio,
                                      dims[0], dims[1], dims[2],
                                      TIO_GHOSTS_NONE, (TIO_Size_t)MACSIO_MSF_SizeOfGroup(bat),
                                      NULL, NULL, NULL,
                                      NULL, NULL, NULL),

                      "Mesh Create Failed\n");
            //TIO_Call( TIO_Flush(file_id), "Flush Failed\n");
        } else{
            char const *varName = json_object_path_get_string(var_obj, "name");
            centering = strdup(json_object_path_get_string(var_obj, "centering"));
            json_object *dataobj = json_object_path_get_extarr(var_obj, "data");
            dtype_id = json_object_extarr_type(dataobj) == json_extarr_type_flt64 ? TIO_DOUBLE : TIO_INT;
            TIO_Centre_t tio_centering = strcmp(centering, "zone") ? TIO_CENTRE_NODE : TIO_CENTRE_CELL;

            TIO_Call( TIO_Create_Quant(file_id, mesh_id, varName, &object_id, dtype_id, tio_centering,
                                        TIO_GHOSTS_NONE, TIO_FALSE, "qunits"),
                    "Quant Create Failed\n");
            //TIO_Call( TIO_Flush(file_id), "Flush Failed\n");

            free(centering);
        }

        /* Loop to make write calls for this var for each part on this rank */
        use_part_count = (int) ceil(json_object_path_get_double(main_obj, "clargs/avg_num_parts"));
        for (p = 0; p < use_part_count; p++)
        {
            json_object *part_obj = json_object_array_get_idx(part_array, p);
            json_object *var_obj = 0;

            void const *buf = 0;
            void const *x_coord = 0;
            void const *y_coord = 0;
            void const *z_coord = 0;

            void *x_coord_root = 0;
            void *y_coord_root = 0;
            void *z_coord_root = 0;

            if (part_obj)
            {
                if (v == -1) {
                    //Mesh
                    json_object *mesh_obj = json_object_path_get_object(part_obj, "Mesh");
                    json_object *global_log_origin_array = json_object_path_get_array(part_obj, "GlobalLogOrigin");
                    json_object *global_log_indices_array = json_object_path_get_array(part_obj, "GlobalLogIndices");
                    json_object *mesh_dims_array = json_object_path_get_array(mesh_obj, "LogDims");
                    int local_mesh_dims[3];
                    TIO_Size_t local_chunk_indices[6] = {0,0,0,0,0,0};  /* local_chunk_indices [il, ih, jl, jh, kl, kh] */
                    
                    for (i = 0; i < ndims; i++)
                    {
                        local_mesh_dims[i] = json_object_get_int(json_object_array_get_idx(mesh_dims_array, i));
                        int start = json_object_get_int(json_object_array_get_idx(global_log_origin_array, i));
                        int count = json_object_get_int(json_object_array_get_idx(mesh_dims_array, i)) - 1;

                        local_chunk_indices[i*2] = start;
                        local_chunk_indices[(i*2)+1] = start + count;
                    }
                                    
                    TIO_Size_t chunk_indices[MACSIO_MSF_SizeOfGroup(bat)][6];
                    MPI_Allgather(local_chunk_indices, 6, MPI_UNSIGNED_LONG, chunk_indices, 6, MPI_UNSIGNED_LONG, MACSIO_MSF_CommOfGroup(bat));

                    for (int k=0; k<MACSIO_MSF_SizeOfGroup(bat); k++){
                        TIO_Call( TIO_Set_Quad_Chunk(file_id, mesh_id, k, ndims_tio,
                                                chunk_indices[k][0], chunk_indices[k][1],
                                                chunk_indices[k][2], chunk_indices[k][3],
                                                chunk_indices[k][4], chunk_indices[k][5],
                                                 (TIO_Size_t)0, (TIO_Size_t)0),
                             "Set Quad Mesh Chunk Failed\n");
                        //TIO_Call( TIO_Flush(file_id), "Flush Failed\n");
                    }

                    json_object *coords = json_object_path_get_object(mesh_obj, "Coords");

                    int coord_array_size[] = {0, 0, 0};
                    
                    if (mesh_type == TIO_MESH_QUAD_COLINEAR){
                        x_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "XAxisCoords"));
                        y_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "YAxisCoords"));
                        z_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "ZAxisCoords"));  
                        coord_array_size[0] = dims[0];
                        coord_array_size[1] = dims[1];
                        coord_array_size[2] = dims[2];  

                        int parts = json_object_path_get_int(main_obj, "problem/global/TotalParts");;

                        if (MACSIO_MAIN_Rank == MACSIO_MSF_RootOfGroup(bat)){
                            x_coord_root = malloc(coord_array_size[0] * sizeof(double));
                            if (ndims > 1) y_coord_root = malloc(coord_array_size[1] * sizeof(double));
                            if (ndims > 2) z_coord_root = malloc(coord_array_size[2] * sizeof(double));
                        }

                        json_object *bounds = json_object_path_get_array(mesh_obj, "Bounds");
                        int bound_array[] = {0,0,0,0,0,0};

                        for (int j=0; j<6; j++){
                            bound_array[j] = JsonGetInt(bounds,"",j);
                        }

                        MPI_Gather((void*)x_coord, local_mesh_dims[0], MPI_DOUBLE, x_coord_root, local_mesh_dims[0], MPI_DOUBLE, 0, MACSIO_MSF_CommOfGroup(bat));
                        if (ndims > 1){
                            MPI_Gather((void*)y_coord, local_mesh_dims[1], MPI_DOUBLE, y_coord_root, local_mesh_dims[1], MPI_DOUBLE, 0, MACSIO_MSF_CommOfGroup(bat));                  
                            if (ndims > 2){
                                MPI_Gather((void*)z_coord, local_mesh_dims[2], MPI_DOUBLE, z_coord_root, local_mesh_dims[2], MPI_DOUBLE, 0, MACSIO_MSF_CommOfGroup(bat));
                            }
                        }
                    } else {
                        x_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "XCoords"));
                        y_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "YCoords"));
                        z_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "ZCoords"));
                    }

                } else {
                    //Variable 
                    json_object *vars_array = json_object_path_get_array(part_obj, "Vars");
                    json_object *var_obj = json_object_array_get_idx(vars_array, v);
                    json_object *extarr_obj = json_object_path_get_extarr(var_obj, "data");

                    buf = json_object_extarr_data(extarr_obj);
                }
            }

            if (v == -1) { 

                if (mesh_type == TIO_MESH_QUAD_COLINEAR){
                    if (MACSIO_MAIN_Rank == MACSIO_MSF_RootOfGroup(bat)){                 
                        TIO_Call( TIO_Write_QuadMesh_All(file_id, mesh_id, TIO_DOUBLE, x_coord_root, y_coord_root, z_coord_root),
                            "Write Quad Mesh All Failed\n");
                        //TIO_Call( TIO_Flush(file_id), "Flush Failed\n");
                    }
                } else {
                    TIO_Call( TIO_Write_QuadMesh_Chunk(file_id, mesh_id, MACSIO_MSF_RankInGroup(bat, MACSIO_MAIN_Rank), TIO_XFER,
                                                        TIO_DOUBLE, x_coord, y_coord, z_coord),
                                "Write Non-Colinear Mesh Coords failed\n");
                    //TIO_Call( TIO_Flush(file_id), "Flush Failed\n");
                }
            } else {                
                TIO_Call( TIO_Write_QuadQuant_Chunk(file_id, object_id, MACSIO_MSF_RankInGroup(bat, MACSIO_MAIN_Rank), 
                                                TIO_XFER, dtype_id, buf, (void*)TIO_NULL),
                    "Write Quad Quant Chunk Failed\n");
                //TIO_Call( TIO_Flush(file_id), "Flush Failed\n");

                TIO_Call( TIO_Close_Quant(file_id, object_id),
                    "Close Quant Failed\n");
                //TIO_Call( TIO_Flush(file_id), "Flush Failed\n");
            }

            free(x_coord_root);
            free(y_coord_root);
            free(z_coord_root);
        }
    }

    TIO_Call( TIO_Close_Mesh(file_id, mesh_id),
          "Close Mesh Failed\n");

}

static void write_ucd_mesh_whole(
    MACSIO_MSF_baton_t const *bat,
    TIO_File_t file_id,     /**< [in] The file id being used in SIF dump */
    TIO_Object_t state_id,  /**< [in] The state id of the current dump */
    json_object *main_obj,  /**< [in] The main JSON object containing mesh data */
    char *mesh_type)        /**< [in] Type of mesh [TIO_MESH_QUAD_COLINEAR/TIO_MESH_QUAD_NONCOLINEAR] */ 
{
    TIO_Object_t mesh_id;
    TIO_Object_t object_id;
    int i, v, p;
    int use_part_count;
    const TIO_Xfer_t TIO_XFER = no_collective ? TIO_XFER_INDEPENDENT : TIO_XFER_COLLECTIVE;
    TIO_File_t dims[3];
    int nnodes = 1, nzones = 1;
    TIO_Size_t nconnectivity = 1;

    int ndims = json_object_path_get_int(main_obj, "clargs/part_dim");

    /* Get the list of vars on the first part as a guide to loop over vars */
    json_object *part_array = json_object_path_get_array(main_obj, "problem/parts");
    json_object *first_part_obj = json_object_array_get_idx(part_array, 0);
    json_object *first_part_vars_array = json_object_path_get_array(first_part_obj, "Vars");

    json_object *part_log_dims_array = json_object_path_get_array(first_part_obj, "Mesh/LogDims");

    for (i = 0; i < ndims; i++)
    {      
        dims[i] = (TIO_Size_t) JsonGetInt(part_log_dims_array, "", i) * MACSIO_MSF_SizeOfGroup(bat);
        nnodes *= dims[i];
        nzones *= dims[i] - MACSIO_MSF_SizeOfGroup(bat);
    }

    /* Loop over vars and then over parts */
    /* currently assumes all vars exist on all ranks. but not all parts */
    for (v = -1; v < json_object_array_length(first_part_vars_array); v++) /* -1 start is for Mesh */
    {
        /* Inspect the first part's var object for name, datatype, etc. */
        json_object *var_obj = json_object_array_get_idx(first_part_vars_array, v);

        char *centering = (char*)malloc(sizeof(char)*5);
        TIO_Data_t dtype_id;
        int nshapes =  strcmp(mesh_type, "ucdzoo") ? MACSIO_MAIN_Size : 1;

        if (v == -1){
 
            char const *shape = JsonGetStr(first_part_obj, "Mesh/Topology/ElemType");

            if (!strcmp(shape, "Beam2"))
            {
                nconnectivity = nzones * 2;
            }
            else if (!strcmp(shape, "Quad4"))
            {
                nconnectivity = nzones * 4;
            }
            else if (!strcmp(shape, "Hex8"))
            {
                nconnectivity = nzones * 8;
            } 
            else if (!strcmp(shape, "Arbitrary"))
            {
                nconnectivity = nzones * 4;
            }

            TIO_Call( TIO_Create_Mesh(file_id, state_id, "mesh", &mesh_id, TIO_MESH_UNSTRUCT,
                                    TIO_COORD_CARTESIAN, TIO_FALSE, "mesh_group", (TIO_Size_t)1,
                                    TIO_INT, TIO_DOUBLE, (TIO_Dims_t)ndims, 
                                    (TIO_Size_t)nnodes, (TIO_Size_t)nzones, nshapes,//nzones,
                                    nconnectivity, (TIO_Size_t)MACSIO_MSF_SizeOfGroup(bat),
                                    NULL, NULL, NULL,
                                    NULL, NULL, NULL),
                "Mesh Create Failed\n");
        } else {
            char const *varName = json_object_path_get_string(var_obj, "name");
            centering = strdup(json_object_path_get_string(var_obj, "centering"));
            TIO_Centre_t tio_centering = strcmp(centering, "zone") ? TIO_CENTRE_NODE : TIO_CENTRE_CELL;
            json_object *dataobj = json_object_path_get_extarr(var_obj, "data");
            dtype_id = json_object_extarr_type(dataobj) == json_extarr_type_flt64 ? TIO_DOUBLE : TIO_INT;
            
            TIO_Call( TIO_Create_Quant(file_id, mesh_id, varName, &object_id, dtype_id, tio_centering,
                                        TIO_GHOSTS_NONE, TIO_FALSE, "qunits"),
                    "Quant Create Failed\n");

            free(centering);
        }

        use_part_count = (int) ceil(json_object_path_get_double(main_obj, "clargs/avg_num_parts"));
        for (p = 0; p < use_part_count; p++)
        {
            json_object *part_obj = json_object_array_get_idx(part_array, p);
            json_object *var_obj = 0;

            void const *buf = 0;
            void const *x_coord = 0;
            void const *y_coord = 0;
            void const *z_coord = 0;

            if(part_obj)
            {
                if (v == -1){
                    json_object *mesh_obj = json_object_path_get_object(part_obj, "Mesh");
                    json_object *global_log_origin_array = json_object_path_get_array(part_obj, "GlobalLogOrigin");
                    json_object *global_log_indices_array = json_object_path_get_array(part_obj, "GlobalLogIndices");
                    json_object *mesh_dims_array = json_object_path_get_array(mesh_obj, "LogDims");
                    int local_mesh_dims[3] = {0, 0, 0};
                    int chunk_parameters[2] = {1, 1}; /* nnodes, ncells */

                    /* Unused currently */
                    TIO_Size_t nghost_nodes = TIO_GHOSTS_NONE;
                    TIO_Size_t nghost_cells = TIO_GHOSTS_NONE;
                    TIO_Size_t nghost_shapes = TIO_GHOSTS_NONE;
                    TIO_Size_t nghost_connectivity = TIO_GHOSTS_NONE;
                    TIO_Size_t nmixcell = 0;
                    TIO_Size_t nmixcomp = 0;

                    json_object *topoobj = JsonGetObj(part_obj, "Mesh/Topology");

                    int chunk_cells = 1;
                    int chunk_nodes = 1;

                    for (i = 0; i < ndims; i++)
                    {
                        local_mesh_dims[i] = json_object_get_int(json_object_array_get_idx(mesh_dims_array, i));
                        chunk_nodes *= local_mesh_dims[i]; // nnodes
                        chunk_cells *= (local_mesh_dims[i]-1); // nzones/ncells
                    }
                    
                    if (!strcmp(mesh_type, "ucdzoo"))
                    {
                        int shapesize;
                        TIO_Shape_t shapetype = TIO_SHAPE_NULL;
                        int ncells_per_shape = 9801;

                        if (ndims == 1 || !strcmp(mesh_type, "ucdzoo")){                                             
                             if (!strcmp(JsonGetStr(topoobj, "ElemType"), "Beam2"))
                            {
                                shapesize = 2;
                                shapetype = TIO_SHAPE_BAR2;                             
                            }
                            else if (!strcmp(JsonGetStr(topoobj, "ElemType"), "Quad4"))
                            {
                                shapesize = 4;
                                shapetype = TIO_SHAPE_QUAD4;
                                    
                            }
                            else if (!strcmp(JsonGetStr(topoobj, "ElemType"), "Hex8"))
                            {
                                shapesize = 8;
                                shapetype = TIO_SHAPE_HEX8;                         
                            }
                        }   

                        int *global_nodes = (int*)malloc(sizeof(int)*MACSIO_MSF_SizeOfGroup(bat));
                        int *global_cells = (int*)malloc(sizeof(int)*MACSIO_MSF_SizeOfGroup(bat));

                        MPI_Allgather(&chunk_nodes, 1, MPI_INT, global_nodes, 1, MPI_INT, MACSIO_MSF_CommOfGroup(bat));
                        MPI_Allgather(&chunk_cells, 1, MPI_INT, global_cells, 1, MPI_INT, MACSIO_MSF_CommOfGroup(bat));
                
                        for (i=0; i<MACSIO_MSF_SizeOfGroup(bat); i++)
                        {
                            nconnectivity = global_cells[i] * shapesize; 
                            TIO_Call( TIO_Set_Unstr_Chunk(file_id, mesh_id, i, (TIO_Dims_t)ndims,
                                                        global_nodes[i], global_cells[i],
                                                        nshapes, nconnectivity,
                                                        nghost_nodes, nghost_cells,
                                                        nghost_shapes, nghost_connectivity,
                                                        nmixcell, nmixcomp),
                                    "Set UCDZOO Mesh Chunk Failed\n");
                        }

                        json_object *coords = json_object_path_get_object(mesh_obj, "Coords");

                        x_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "XCoords"));
                        y_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "YCoords"));
                        z_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "ZCoords"));

                        int chunkID = MACSIO_MSF_RankInGroup(bat, MACSIO_MAIN_Rank); //JsonGetInt(mesh_obj, "ChunkID");
                        void const *node_connectivity = (void const*) json_object_extarr_data(JsonGetObj(topoobj, "Nodelist"));
                        int *nodeIDs = (int*) malloc(sizeof(int) * chunk_nodes);
                        int *cellIDs = (int*) malloc(sizeof(int) * chunk_cells);

                        for (int k = 0; k < chunk_nodes; k++){
                            nodeIDs[k] = k + ((int)global_nodes[chunkID] * chunkID);
                        }

                        for (int cell = 0; cell < chunk_cells; cell++){
                            cellIDs[cell] = cell + ((int)global_cells[chunkID] * chunkID);
                        }

                        void const *node_ptr = nodeIDs;
                        void const *cell_ptr = cellIDs;

                        TIO_Call( TIO_Write_UnstrMesh_Chunk(file_id, mesh_id, (TIO_Size_t)MACSIO_MSF_RankInGroup(bat, MACSIO_MAIN_Rank), TIO_XFER, 
                                                    TIO_INT, TIO_DOUBLE, node_ptr, cell_ptr, &shapetype, 
                                                    &ncells_per_shape, node_connectivity, x_coord, y_coord, z_coord),
                                    "Write Unstructured Mesh Failed\n");

                        free((void*)nodeIDs);
                        free((void*)cellIDs);
                    } else if (!strcmp(mesh_type, "arbitrary"))
                    {       
                        TIO_Size_t nshapes = MACSIO_MAIN_Size;
                        int shapesize;
                        TIO_Shape_t *shapetype = (TIO_Shape_t*)malloc(nshapes * sizeof(TIO_Shape_t));
                        int ncells_per_shape = 9801;

                        for (int s = 0; s<nshapes; s++){
                            shapetype[s] = (TIO_Shape_t)4;
                        }

                        int *global_nodes = (int*)malloc(sizeof(int)*MACSIO_MSF_SizeOfGroup(bat));
                        int *global_cells = (int*)malloc(sizeof(int)*MACSIO_MSF_SizeOfGroup(bat));

                        MPI_Allgather(&chunk_nodes, 1, MPI_INT, global_nodes, 1, MPI_INT, MACSIO_MSF_CommOfGroup(bat));
                        MPI_Allgather(&chunk_cells, 1, MPI_INT, global_cells, 1, MPI_INT, MACSIO_MSF_CommOfGroup(bat));
                
                        for (i=0; i<MACSIO_MSF_SizeOfGroup(bat); i++)
                        {
                            nconnectivity = global_cells[i] * shapesize; 
                            TIO_Call( TIO_Set_Unstr_Chunk(file_id, mesh_id, i, (TIO_Dims_t)ndims,
                                                        global_nodes[i], global_cells[i],
                                                        nshapes, nconnectivity,
                                                        nghost_nodes, nghost_cells,
                                                        nghost_shapes, nghost_connectivity,
                                                        nmixcell, nmixcomp),
                                    "Set UCDZOO Mesh Chunk Failed\n");
                        }

                        json_object *coords = json_object_path_get_object(mesh_obj, "Coords");

                        x_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "XCoords"));
                        y_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "YCoords"));
                        z_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "ZCoords"));

                        int chunkID = MACSIO_MSF_RankInGroup(bat, MACSIO_MAIN_Rank); //JsonGetInt(mesh_obj, "ChunkID");
                        void const *node_connectivity = (void const*) json_object_extarr_data(JsonGetObj(topoobj, "Nodelist"));
                        int *nodeIDs = (int*) malloc(sizeof(int) * chunk_nodes);
                        int *cellIDs = (int*) malloc(sizeof(int) * chunk_cells);

                        for (int k = 0; k < chunk_nodes; k++){
                            nodeIDs[k] = k + ((int)global_nodes[chunkID] * chunkID);
                        }

                        for (int cell = 0; cell < chunk_cells; cell++){
                            cellIDs[cell] = cell + ((int)global_cells[chunkID] * chunkID);
                        }

                        void const *node_ptr = nodeIDs;
                        void const *cell_ptr = cellIDs;

                        TIO_Call( TIO_Write_UnstrMesh_Chunk(file_id, mesh_id, (TIO_Size_t)chunkID, TIO_XFER, 
                                                    TIO_INT, TIO_DOUBLE, node_ptr, cell_ptr, shapetype, 
                                                    &ncells_per_shape, node_connectivity, x_coord, y_coord, z_coord),
                                    "Write Unstructured Mesh Failed\n");

                        free((void*)nodeIDs);
                        free((void*)cellIDs);
                    }

                } else {
                    json_object *vars_array = json_object_path_get_array(part_obj, "Vars");
                    json_object *var_obj = json_object_array_get_idx(vars_array, v);
                    json_object *extarr_obj = json_object_path_get_extarr(var_obj, "data");

                    buf = json_object_extarr_data(extarr_obj);

                    TIO_Call( TIO_Write_UnstrQuant_Chunk(file_id, object_id, MACSIO_MSF_RankInGroup(bat, MACSIO_MAIN_Rank), TIO_XFER,
                                                        dtype_id, buf, (void*)TIO_NULL),
                                        "Write UCDZOO Quant Chunk Failed\n")

                    TIO_Call(TIO_Close_Quant(file_id, object_id),
                            "Close Quant Failed");
                }
            }

        }

    }

    TIO_Call( TIO_Close_Mesh(file_id, mesh_id),
                "Close Mesh Failed");
}

static void write_mesh_hybrid(
    MACSIO_MSF_baton_t const *bat,
    TIO_File_t file_id,             /**< [in] The file id being used in MIF dump */
    TIO_Object_t state_id,          /**< [in] The state id of the current dump */
    json_object *main_obj)  /**< [in] JSON object representing this mesh part */
{
    json_object *parts = json_object_path_get_array(main_obj, "problem/parts");
    json_object *first_part = json_object_array_get_idx(parts, 0);

    if (!strcmp(JsonGetStr(first_part, "Mesh/MeshType"), "rectilinear"))
        write_quad_mesh_whole(bat, file_id, state_id, main_obj, TIO_MESH_QUAD_COLINEAR);
    else if (!strcmp(JsonGetStr(first_part, "Mesh/MeshType"), "curvilinear"))
        write_quad_mesh_whole(bat, file_id, state_id, main_obj, TIO_MESH_QUAD_NONCOLINEAR);
    else if (!strcmp(JsonGetStr(first_part, "Mesh/MeshType"), "ucdzoo"))
        write_ucd_mesh_whole(bat, file_id, state_id, main_obj, "ucdzoo");
    else if (!strcmp(JsonGetStr(first_part, "Mesh/MeshType"), "arbitrary"))
        write_ucd_mesh_whole(bat, file_id, state_id, main_obj, "arbitrary");
}

/*!
\brief Main MSF dump implementation for the plugin

This function is called to handle MSF file dumps.

The files and states are created before passing off to write_quad_mesh_part_shared
or write_ucd_mesh_part_shared to handle the dump of the data.
*/
static void main_dump_msf(
    json_object *main_obj,
    int numFiles,
    int dumpn,
    double dumpt)
{
    int size, rank;
    TIO_t *tioFile_ptr;
    TIO_File_t tioFile;
    char fileName[256];
    char stateName[256];
    group_data_t userData;
    MACSIO_MSF_ioFlags_t ioFlags = {MACSIO_MSF_WRITE, JsonGetInt(main_obj, "clargs/exercise_scr") & 0x1};

    MACSIO_MSF_baton_t *bat = MACSIO_MSF_Init(numFiles, ioFlags, MACSIO_MAIN_Comm, 1, &userData);

    rank = json_object_path_get_int(main_obj, "parallel/mpi_rank");
    size = json_object_path_get_int(main_obj, "parallel/mpi_size");

    /* Construct name for the typhonio file */
    sprintf(fileName, "%s_typhonio_%05d_%03d.%s",
            json_object_path_get_string(main_obj, "clargs/filebase"),
            MACSIO_MSF_RankOfGroup(bat, rank),
            dumpn,
            "h5");//json_object_path_get_string(main_obj, "clargs/fileext"));

    sprintf(stateName, "state0");

    MACSIO_UTILS_RecordOutputFiles(dumpn, fileName);

    /* Create */
    //MPI_Comm *groupComm = (MPI_Comm*)userData;
    char *date = getDate();
    TIO_Call( TIO_Create(fileName, &tioFile, TIO_ACC_REPLACE, "MACSio",
                         "1.0", date, (char*)fileName, MACSIO_MSF_CommOfGroup(bat), MPI_INFO_NULL, MACSIO_MSF_RankInGroup(bat, MACSIO_MAIN_Rank)),
              "File Creation Failed\n");
    /* Create */ 

    json_object *parts = json_object_path_get_array(main_obj, "problem/parts");

    for (int i = 0; i < json_object_array_length(parts); i++)
    {
        char domain_dir[256];
        json_object *this_part = json_object_array_get_idx(parts, i);
        TIO_Object_t domain_group_id;

        snprintf(domain_dir, sizeof(domain_dir), "domain_%07d",
                 json_object_path_get_int(this_part, "Mesh/ChunkID"));

        TIO_Call( TIO_Create_State(tioFile, domain_dir, &domain_group_id, 1, (TIO_Time_t)0.0, "us"),
                  "State Create Failed\n");

        write_mesh_hybrid(bat, tioFile, domain_group_id, main_obj);

        TIO_Call( TIO_Close_State(tioFile, domain_group_id),
                  "State Close Failed\n");
    }
    /* Close the checkpoint file */
    TIO_Call( TIO_Close(tioFile),
      "Close File failed\n");

    /* We're done using MACSIO_MIF, so finish it off */
    MACSIO_MSF_Finish(bat);


}

/*!
\brief Write all quad mesh parts to a SIF file

This method serializes the JSON object for all mesh parts and then
appends/writes to the end of the shared file.

This method is used for both Rectilinear (Colinear) and Curvilinear (Non-Colinear)
Quad meshes.
*/ 
static void write_quad_mesh_whole(
    TIO_File_t file_id,     /**< [in] The file id being used in SIF dump */
    TIO_Object_t state_id,  /**< [in] The state id of the current dump */
    json_object *main_obj,  /**< [in] The main JSON object containing mesh data */
    TIO_Mesh_t mesh_type)   /**< [in] Type of mesh [TIO_MESH_QUAD_COLINEAR/TIO_MESH_QUAD_NONCOLINEAR] */    
{
    TIO_Object_t mesh_id;
    TIO_Object_t object_id;
    int ndims;
    int i, v, p;
    int use_part_count;
    const TIO_Xfer_t TIO_XFER = no_collective ? TIO_XFER_INDEPENDENT : TIO_XFER_COLLECTIVE;

    TIO_Size_t dims[3];

    ndims = json_object_path_get_int(main_obj, "clargs/part_dim");
    json_object *global_log_dims_array = json_object_path_get_array(main_obj, "problem/global/LogDims");
    json_object *global_parts_log_dims_array = json_object_path_get_array(main_obj, "problem/global/PartsLogDims");
    

    for (i = 0; i < ndims; i++)
    {
        dims[i] = (TIO_Size_t) JsonGetInt(global_log_dims_array, "", i);
    }

    /* Get the list of vars on the first part as a guide to loop over vars */
    json_object *part_array = json_object_path_get_array(main_obj, "problem/parts");
    json_object *first_part_obj = json_object_array_get_idx(part_array, 0);
    json_object *first_part_vars_array = json_object_path_get_array(first_part_obj, "Vars");

    /* Loop over vars and then over parts */
    /* currently assumes all vars exist on all ranks. but not all parts */
    for (v = -1; v < json_object_array_length(first_part_vars_array); v++) /* -1 start is for Mesh */
    {
        /* Inspect the first part's var object for name, datatype, etc. */
        json_object *var_obj = json_object_array_get_idx(first_part_vars_array, v);

        char *centering = (char*)malloc(sizeof(char)*5);
        TIO_Data_t dtype_id;

        //MESH
        TIO_Dims_t ndims_tio = (TIO_Dims_t)ndims;

        if (v == -1) { 
            TIO_Call( TIO_Create_Mesh(file_id, state_id, "mesh", &mesh_id, mesh_type,
                                      TIO_COORD_CARTESIAN, TIO_FALSE, "mesh_group", (TIO_Size_t)1,
                                      TIO_DATATYPE_NULL, TIO_DOUBLE, ndims_tio,
                                      dims[0], dims[1], dims[2],
                                      TIO_GHOSTS_NONE, (TIO_Size_t)MACSIO_MAIN_Size,
                                      NULL, NULL, NULL,
                                      NULL, NULL, NULL),

                      "Mesh Create Failed\n");
        } else{
            char const *varName = json_object_path_get_string(var_obj, "name");
            centering = strdup(json_object_path_get_string(var_obj, "centering"));
            json_object *dataobj = json_object_path_get_extarr(var_obj, "data");
            dtype_id = json_object_extarr_type(dataobj) == json_extarr_type_flt64 ? TIO_DOUBLE : TIO_INT;
            TIO_Centre_t tio_centering = strcmp(centering, "zone") ? TIO_CENTRE_NODE : TIO_CENTRE_CELL;

            TIO_Call( TIO_Create_Quant(file_id, mesh_id, varName, &object_id, dtype_id, tio_centering,
                                        TIO_GHOSTS_NONE, TIO_FALSE, "qunits"),
                    "Quant Create Failed\n");

            free(centering);
        }

        /* Loop to make write calls for this var for each part on this rank */
        use_part_count = (int) ceil(json_object_path_get_double(main_obj, "clargs/avg_num_parts"));
        for (p = 0; p < use_part_count; p++)
        {
            json_object *part_obj = json_object_array_get_idx(part_array, p);
            json_object *var_obj = 0;

            void const *buf = 0;
            void const *x_coord = 0;
            void const *y_coord = 0;
            void const *z_coord = 0;

            void *x_coord_root = 0;
            void *y_coord_root = 0;
            void *z_coord_root = 0;

            if (part_obj)
            {
                if (v == -1) {
                    //Mesh
                    json_object *mesh_obj = json_object_path_get_object(part_obj, "Mesh");
                    json_object *global_log_origin_array = json_object_path_get_array(part_obj, "GlobalLogOrigin");
                    json_object *global_log_indices_array = json_object_path_get_array(part_obj, "GlobalLogIndices");
                    json_object *mesh_dims_array = json_object_path_get_array(mesh_obj, "LogDims");
                    int local_mesh_dims[3];
                    TIO_Size_t local_chunk_indices[6] = {0,0,0,0,0,0};  /* local_chunk_indices [il, ih, jl, jh, kl, kh] */
                    
                    for (i = 0; i < ndims; i++)
                    {
                        local_mesh_dims[i] = json_object_get_int(json_object_array_get_idx(mesh_dims_array, i));
                        int start = json_object_get_int(json_object_array_get_idx(global_log_origin_array, i));
                        int count = json_object_get_int(json_object_array_get_idx(mesh_dims_array, i)) - 1;

                        local_chunk_indices[i*2] = start;
                        local_chunk_indices[(i*2)+1] = start + count;
                    }
                                    
                    TIO_Size_t chunk_indices[MACSIO_MAIN_Size][6];
                    MPI_Allgather(local_chunk_indices, 6, MPI_DOUBLE, chunk_indices, 6, MPI_DOUBLE, MACSIO_MAIN_Comm);

                    for (int k=0; k<MACSIO_MAIN_Size; k++){
                        TIO_Call( TIO_Set_Quad_Chunk(file_id, mesh_id, k, ndims_tio,
                                                chunk_indices[k][0], chunk_indices[k][1],
                                                chunk_indices[k][2], chunk_indices[k][3],
                                                chunk_indices[k][4], chunk_indices[k][5],
                                                 (TIO_Size_t)0, (TIO_Size_t)0),
                             "Set Quad Mesh Chunk Failed\n");
                    }

                    json_object *coords = json_object_path_get_object(mesh_obj, "Coords");

                    int coord_array_size[] = {0, 0, 0};
                    
                    if (mesh_type == TIO_MESH_QUAD_COLINEAR){
                        x_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "XAxisCoords"));
                        y_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "YAxisCoords"));
                        z_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "ZAxisCoords"));  
                        coord_array_size[0] = dims[0];
                        coord_array_size[1] = dims[1];
                        coord_array_size[2] = dims[2];  

                        int parts = json_object_path_get_int(main_obj, "problem/global/TotalParts");;

                        if (MACSIO_MAIN_Rank == 0){
                            x_coord_root = malloc(coord_array_size[0] * sizeof(double));
                            if (ndims > 1) y_coord_root = malloc(coord_array_size[1] * sizeof(double));
                            if (ndims > 2) z_coord_root = malloc(coord_array_size[2] * sizeof(double));
                        }

                        json_object *bounds = json_object_path_get_array(mesh_obj, "Bounds");

                        int color = (JsonGetInt(bounds,"",1) == 0 && JsonGetInt(bounds,"",2) ==0) ? 1: MPI_UNDEFINED;
                        MPI_Comm comm;
                        MPI_Comm_split(MACSIO_MAIN_Comm, color, MACSIO_MAIN_Rank, &comm);
                        if (color == 1){
                        MPI_Gather((void*)x_coord, local_mesh_dims[0], MPI_DOUBLE, x_coord_root, local_mesh_dims[0], MPI_DOUBLE, 0, comm);
                        }
                        if (ndims > 1){
                            color = (JsonGetInt(bounds, "", 0)==0 && JsonGetInt(bounds,"",2)==0) ? 1: MPI_UNDEFINED;
                            MPI_Comm_split(MACSIO_MAIN_Comm, color, MACSIO_MAIN_Rank, &comm);
                            if (color == 1){
                            MPI_Gather((void*)y_coord, local_mesh_dims[1], MPI_DOUBLE, y_coord_root, local_mesh_dims[1], MPI_DOUBLE, 0, comm);
                            }                   
                        }
                        if (ndims > 2){
                            color = (JsonGetInt(bounds, "", 0)==0 && JsonGetInt(bounds,"",1)==0) ? 1: MPI_UNDEFINED;
                            MPI_Comm_split(MACSIO_MAIN_Comm, color, MACSIO_MAIN_Rank, &comm);
                            if (color == 1)
                            MPI_Gather((void*)z_coord, local_mesh_dims[2], MPI_DOUBLE, z_coord_root, local_mesh_dims[2], MPI_DOUBLE, 0, comm);

                        }
                    } else {
                        x_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "XCoords"));
                        y_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "YCoords"));
                        z_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "ZCoords"));
                    }

                } else {
                    //Variable 
                    json_object *vars_array = json_object_path_get_array(part_obj, "Vars");
                    json_object *var_obj = json_object_array_get_idx(vars_array, v);
                    json_object *extarr_obj = json_object_path_get_extarr(var_obj, "data");

                    buf = json_object_extarr_data(extarr_obj);
                }
            }

            if (v == -1) { 

                if (mesh_type == TIO_MESH_QUAD_COLINEAR){
                    if (MACSIO_MAIN_Rank == 0){                 
                        TIO_Call( TIO_Write_QuadMesh_All(file_id, mesh_id, TIO_DOUBLE, x_coord_root, y_coord_root, z_coord_root),
                            "Write Quad Mesh All Failed\n");
                    }
                } else {
                    TIO_Call( TIO_Write_QuadMesh_Chunk(file_id, mesh_id, MACSIO_MAIN_Rank, TIO_XFER,
                                                        TIO_DOUBLE, x_coord, y_coord, z_coord),
                                "Write Non-Colinear Mesh Coords failed\n");
                }
            } else {                
                TIO_Call( TIO_Write_QuadQuant_Chunk(file_id, object_id, MACSIO_MAIN_Rank, 
                                                TIO_XFER, dtype_id, buf, (void*)TIO_NULL),
                    "Write Quad Quant Chunk Failed\n");

                TIO_Call( TIO_Close_Quant(file_id, object_id),
                    "Close Quant Failed\n");
            }

            free(x_coord_root);
            free(y_coord_root);
            free(z_coord_root);
        }
    }

    TIO_Call( TIO_Close_Mesh(file_id, mesh_id),
          "Close Mesh Failed\n");

}

/*!
\brief Write all unstructured mesh parts to a SIF file

This method serializes the JSON object for all mesh parts and then
appends/writes to the end of the shared file.

This method is used for both Unstructured Zoo and Arbitraty 
Unstructured meshes.
*/ 
static void write_ucd_mesh_whole(
    TIO_File_t file_id,     /**< [in] The file id being used in SIF dump */
    TIO_Object_t state_id,  /**< [in] The state id of the current dump */
    json_object *main_obj,  /**< [in] The main JSON object containing mesh data */
    char *mesh_type)        /**< [in] Type of mesh [TIO_MESH_QUAD_COLINEAR/TIO_MESH_QUAD_NONCOLINEAR] */ 
{
    TIO_Object_t mesh_id;
    TIO_Object_t object_id;
    int i, v, p;
    int use_part_count;
    const TIO_Xfer_t TIO_XFER = no_collective ? TIO_XFER_INDEPENDENT : TIO_XFER_COLLECTIVE;
    TIO_File_t dims[3];
    int nnodes = 1, nzones = 1;
    TIO_Size_t nconnectivity = 1;

    int ndims = json_object_path_get_int(main_obj, "clargs/part_dim");
    json_object *global_log_dims_array = json_object_path_get_array(main_obj, "problem/global/LogDims");
    json_object *global_parts_log_dims_array = json_object_path_get_array(main_obj, "problem/global/PartsLogDims");

    for (i = 0; i < ndims; i++)
    {
        dims[i] = (TIO_Size_t) JsonGetInt(global_log_dims_array, "", i);
        nnodes *= dims[i];
        nzones *= dims[i]-JsonGetInt(global_parts_log_dims_array,"", i);
    }

    /* Get the list of vars on the first part as a guide to loop over vars */
    json_object *part_array = json_object_path_get_array(main_obj, "problem/parts");
    json_object *first_part_obj = json_object_array_get_idx(part_array, 0);
    json_object *first_part_vars_array = json_object_path_get_array(first_part_obj, "Vars");

    /* Loop over vars and then over parts */
    /* currently assumes all vars exist on all ranks. but not all parts */
    for (v = -1; v < json_object_array_length(first_part_vars_array); v++) /* -1 start is for Mesh */
    {
        /* Inspect the first part's var object for name, datatype, etc. */
        json_object *var_obj = json_object_array_get_idx(first_part_vars_array, v);

        char *centering = (char*)malloc(sizeof(char)*5);
        TIO_Data_t dtype_id;
        int nshapes =  strcmp(mesh_type, "ucdzoo") ? MACSIO_MAIN_Size : 1;

        if (v == -1){
 
            char const *shape = JsonGetStr(first_part_obj, "Mesh/Topology/ElemType");

            if (!strcmp(shape, "Beam2"))
            {
                nconnectivity = nzones * 2;
            }
            else if (!strcmp(shape, "Quad4"))
            {
                nconnectivity = nzones * 4;
            }
            else if (!strcmp(shape, "Hex8"))
            {
                nconnectivity = nzones * 8;
            } 
            else if (!strcmp(shape, "Arbitrary"))
            {
                nconnectivity = nzones * 4;
            }

            TIO_Call( TIO_Create_Mesh(file_id, state_id, "mesh", &mesh_id, TIO_MESH_UNSTRUCT,
                                    TIO_COORD_CARTESIAN, TIO_FALSE, "mesh_group", (TIO_Size_t)1,
                                    TIO_INT, TIO_DOUBLE, (TIO_Dims_t)ndims, 
                                    (TIO_Size_t)nnodes, (TIO_Size_t)nzones, nshapes,//nzones,
                                    nconnectivity, (TIO_Size_t)MACSIO_MAIN_Size,
                                    NULL, NULL, NULL,
                                    NULL, NULL, NULL),
                "Mesh Create Failed\n");
        } else {
            char const *varName = json_object_path_get_string(var_obj, "name");
            centering = strdup(json_object_path_get_string(var_obj, "centering"));
            TIO_Centre_t tio_centering = strcmp(centering, "zone") ? TIO_CENTRE_NODE : TIO_CENTRE_CELL;
            json_object *dataobj = json_object_path_get_extarr(var_obj, "data");
            dtype_id = json_object_extarr_type(dataobj) == json_extarr_type_flt64 ? TIO_DOUBLE : TIO_INT;
            
            TIO_Call( TIO_Create_Quant(file_id, mesh_id, varName, &object_id, dtype_id, tio_centering,
                                        TIO_GHOSTS_NONE, TIO_FALSE, "qunits"),
                    "Quant Create Failed\n");

            free(centering);
        }

        use_part_count = (int) ceil(json_object_path_get_double(main_obj, "clargs/avg_num_parts"));
        for (p = 0; p < use_part_count; p++)
        {
            json_object *part_obj = json_object_array_get_idx(part_array, p);
            json_object *var_obj = 0;

            void const *buf = 0;
            void const *x_coord = 0;
            void const *y_coord = 0;
            void const *z_coord = 0;

            if(part_obj)
            {
                if (v == -1){
                    json_object *mesh_obj = json_object_path_get_object(part_obj, "Mesh");
                    json_object *global_log_origin_array = json_object_path_get_array(part_obj, "GlobalLogOrigin");
                    json_object *global_log_indices_array = json_object_path_get_array(part_obj, "GlobalLogIndices");
                    json_object *mesh_dims_array = json_object_path_get_array(mesh_obj, "LogDims");
                    int local_mesh_dims[3] = {0, 0, 0};
                    int chunk_parameters[2] = {1, 1}; /* nnodes, ncells */

                    /* Unused currently */
                    TIO_Size_t nghost_nodes = TIO_GHOSTS_NONE;
                    TIO_Size_t nghost_cells = TIO_GHOSTS_NONE;
                    TIO_Size_t nghost_shapes = TIO_GHOSTS_NONE;
                    TIO_Size_t nghost_connectivity = TIO_GHOSTS_NONE;
                    TIO_Size_t nmixcell = 0;
                    TIO_Size_t nmixcomp = 0;

                    json_object *topoobj = JsonGetObj(part_obj, "Mesh/Topology");

                    int chunk_cells = 1;
                    int chunk_nodes = 1;

                    for (i = 0; i < ndims; i++)
                    {
                        local_mesh_dims[i] = json_object_get_int(json_object_array_get_idx(mesh_dims_array, i));
                        chunk_nodes *= local_mesh_dims[i]; // nnodes
                        chunk_cells *= (local_mesh_dims[i]-1); // nzones/ncells
                    }
                    
                    if (!strcmp(mesh_type, "ucdzoo"))
                    {
                        int shapesize;
                        TIO_Shape_t shapetype = TIO_SHAPE_NULL;
                        int ncells_per_shape = 9801;

                        if (ndims == 1 || !strcmp(mesh_type, "ucdzoo")){                                             
                             if (!strcmp(JsonGetStr(topoobj, "ElemType"), "Beam2"))
                            {
                                shapesize = 2;
                                shapetype = TIO_SHAPE_BAR2;                             
                            }
                            else if (!strcmp(JsonGetStr(topoobj, "ElemType"), "Quad4"))
                            {
                                shapesize = 4;
                                shapetype = TIO_SHAPE_QUAD4;
                                    
                            }
                            else if (!strcmp(JsonGetStr(topoobj, "ElemType"), "Hex8"))
                            {
                                shapesize = 8;
                                shapetype = TIO_SHAPE_HEX8;                         
                            }
                        }   

                        int *global_nodes = (int*)malloc(sizeof(int)*MACSIO_MAIN_Size);
                        int *global_cells = (int*)malloc(sizeof(int)*MACSIO_MAIN_Size);

                        MPI_Allgather(&chunk_nodes, 1, MPI_INT, global_nodes, 1, MPI_INT, MACSIO_MAIN_Comm);
                        MPI_Allgather(&chunk_cells, 1, MPI_INT, global_cells, 1, MPI_INT, MACSIO_MAIN_Comm);
                
                        for (i=0; i<MACSIO_MAIN_Size; i++)
                        {
                            nconnectivity = global_cells[i] * shapesize; 
                            TIO_Call( TIO_Set_Unstr_Chunk(file_id, mesh_id, i, (TIO_Dims_t)ndims,
                                                        global_nodes[i], global_cells[i],
                                                        nshapes, nconnectivity,
                                                        nghost_nodes, nghost_cells,
                                                        nghost_shapes, nghost_connectivity,
                                                        nmixcell, nmixcomp),
                                    "Set UCDZOO Mesh Chunk Failed\n");
                        }

                        json_object *coords = json_object_path_get_object(mesh_obj, "Coords");

                        x_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "XCoords"));
                        y_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "YCoords"));
                        z_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "ZCoords"));

                        int chunkID = JsonGetInt(mesh_obj, "ChunkID");
                        void const *node_connectivity = (void const*) json_object_extarr_data(JsonGetObj(topoobj, "Nodelist"));
                        int *nodeIDs = (int*) malloc(sizeof(int) * chunk_nodes);
                        int *cellIDs = (int*) malloc(sizeof(int) * chunk_cells);

                        for (int k = 0; k < chunk_nodes; k++){
                            nodeIDs[k] = k + ((int)global_nodes[chunkID] * chunkID);
                        }

                        for (int cell = 0; cell < chunk_cells; cell++){
                            cellIDs[cell] = cell + ((int)global_cells[chunkID] * chunkID);
                        }

                        void const *node_ptr = nodeIDs;
                        void const *cell_ptr = cellIDs;

                        TIO_Call( TIO_Write_UnstrMesh_Chunk(file_id, mesh_id, (TIO_Size_t)MACSIO_MAIN_Rank, TIO_XFER, 
                                                    TIO_INT, TIO_DOUBLE, node_ptr, cell_ptr, &shapetype, 
                                                    &ncells_per_shape, node_connectivity, x_coord, y_coord, z_coord),
                                    "Write Unstructured Mesh Failed\n");

                        free((void*)nodeIDs);
                        free((void*)cellIDs);
                    } else if (!strcmp(mesh_type, "arbitrary"))
                    {       
                        TIO_Size_t nshapes = MACSIO_MAIN_Size;
                        int shapesize;
                        TIO_Shape_t *shapetype = (TIO_Shape_t*)malloc(nshapes * sizeof(TIO_Shape_t));
                        int ncells_per_shape = 9801;

                        for (int s = 0; s<nshapes; s++){
                            shapetype[s] = (TIO_Shape_t)4;
                        }

                        int *global_nodes = (int*)malloc(sizeof(int)*MACSIO_MAIN_Size);
                        int *global_cells = (int*)malloc(sizeof(int)*MACSIO_MAIN_Size);

                        MPI_Allgather(&chunk_nodes, 1, MPI_INT, global_nodes, 1, MPI_INT, MACSIO_MAIN_Comm);
                        MPI_Allgather(&chunk_cells, 1, MPI_INT, global_cells, 1, MPI_INT, MACSIO_MAIN_Comm);
                
                        for (i=0; i<MACSIO_MAIN_Size; i++)
                        {
                            nconnectivity = global_cells[i] * shapesize; 
                            TIO_Call( TIO_Set_Unstr_Chunk(file_id, mesh_id, i, (TIO_Dims_t)ndims,
                                                        global_nodes[i], global_cells[i],
                                                        nshapes, nconnectivity,
                                                        nghost_nodes, nghost_cells,
                                                        nghost_shapes, nghost_connectivity,
                                                        nmixcell, nmixcomp),
                                    "Set UCDZOO Mesh Chunk Failed\n");
                        }

                        json_object *coords = json_object_path_get_object(mesh_obj, "Coords");

                        x_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "XCoords"));
                        y_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "YCoords"));
                        z_coord = json_object_extarr_data(json_object_path_get_extarr(coords, "ZCoords"));

                        int chunkID = JsonGetInt(mesh_obj, "ChunkID");
                        void const *node_connectivity = (void const*) json_object_extarr_data(JsonGetObj(topoobj, "Nodelist"));
                        int *nodeIDs = (int*) malloc(sizeof(int) * chunk_nodes);
                        int *cellIDs = (int*) malloc(sizeof(int) * chunk_cells);

                        for (int k = 0; k < chunk_nodes; k++){
                            nodeIDs[k] = k + ((int)global_nodes[chunkID] * chunkID);
                        }

                        for (int cell = 0; cell < chunk_cells; cell++){
                            cellIDs[cell] = cell + ((int)global_cells[chunkID] * chunkID);
                        }

                        void const *node_ptr = nodeIDs;
                        void const *cell_ptr = cellIDs;

                        TIO_Call( TIO_Write_UnstrMesh_Chunk(file_id, mesh_id, (TIO_Size_t)MACSIO_MAIN_Rank, TIO_XFER, 
                                                    TIO_INT, TIO_DOUBLE, node_ptr, cell_ptr, shapetype, 
                                                    &ncells_per_shape, node_connectivity, x_coord, y_coord, z_coord),
                                    "Write Unstructured Mesh Failed\n");

                        free((void*)nodeIDs);
                        free((void*)cellIDs);
                    }

                } else {
                    json_object *vars_array = json_object_path_get_array(part_obj, "Vars");
                    json_object *var_obj = json_object_array_get_idx(vars_array, v);
                    json_object *extarr_obj = json_object_path_get_extarr(var_obj, "data");

                    buf = json_object_extarr_data(extarr_obj);

                    TIO_Call( TIO_Write_UnstrQuant_Chunk(file_id, object_id, MACSIO_MAIN_Rank, TIO_XFER,
                                                        dtype_id, buf, (void*)TIO_NULL),
                                        "Write UCDZOO Quant Chunk Failed\n")

                    TIO_Call(TIO_Close_Quant(file_id, object_id),
                            "Close Quant Failed");
                }
            }

        }

    }

    TIO_Call( TIO_Close_Mesh(file_id, mesh_id),
                "Close Mesh Failed");
}

/*!
\brief Main SIF dump implementation for the plugin

This function is called to handle SIF file dumps.

The file and state is created before passing off to \ref write_quad_mesh_whole or
\ref write_ucd_mesh_whole to handle the dump of data
*/
static void main_dump_sif(
    json_object *main_obj,  /**< [in] The main JSON object containing mesh data */
    int dumpn,              /**< [in] The number/index of this dump */ 
    double dumpt)           /**< [in] The time the be associated with this dump */
{
    char const *mesh_type = json_object_path_get_string(main_obj, "clargs/part_type");
    char fileName[256];
    TIO_File_t tiofile_id = NULL;
    TIO_Object_t state_id, variable_id;
    char state_name[16];
    char *date = (char*)getDate();
    MPI_Info mpiInfo = MPI_INFO_NULL;

    if (romio_cb_write || romio_ds_write || striping_factor){
	MPI_Info_create(&mpiInfo);
	if (strcmp(romio_cb_write,"enable") || strcmp(romio_cb_write,"disable")){
	    MPI_Info_set(mpiInfo, "romio_cb_write", romio_cb_write);
	}
	if (strcmp(romio_ds_write,"enable") || strcmp(romio_ds_write,"disable")){
	    MPI_Info_set(mpiInfo, "romio_ds_write", romio_ds_write);
	}
	if (striping_factor){
	    MPI_Info_set(mpiInfo, "striping_factor", striping_factor);
	}
    }
    int file_suffix = dumpn;
    sprintf(state_name, "state0");

    /* Construct name for the HDF5 file */
    sprintf(fileName, "%s_typhonio_%03d.%s",
            json_object_path_get_string(main_obj, "clargs/filebase"),
            file_suffix,
            "h5"); //json_object_path_get_string(main_obj, "clargs/fileext"));

    MACSIO_UTILS_RecordOutputFiles(dumpn, fileName);

    TIO_Call( TIO_Create(fileName, &tiofile_id, TIO_ACC_REPLACE, "MACSio",
        "1.0", date, fileName, MACSIO_MAIN_Comm, mpiInfo, MACSIO_MAIN_Rank),
    "File Creation Failed\n");
    if (romio_cb_write || romio_ds_write || striping_factor){
	MPI_Info_free(&mpiInfo);
    }
    TIO_Call( TIO_Create_State(tiofile_id, state_name, &state_id, 1, (TIO_Time_t)0.0, "us"),
        "State Create Failed\n");

    json_object *part_array = json_object_path_get_array(main_obj, "problem/parts");
    json_object *part_obj = json_object_array_get_idx(part_array, 0);
    json_object *mesh_obj = json_object_path_get_object(part_obj, "Mesh");

    if (!strcmp(JsonGetStr(mesh_obj, "MeshType"), "rectilinear"))
        write_quad_mesh_whole(tiofile_id, state_id, main_obj, TIO_MESH_QUAD_COLINEAR);
    else if (!strcmp(JsonGetStr(mesh_obj, "MeshType"), "curvilinear"))
        write_quad_mesh_whole(tiofile_id, state_id, main_obj, TIO_MESH_QUAD_NONCOLINEAR);
    else if (!strcmp(JsonGetStr(mesh_obj, "MeshType"), "ucdzoo"))
        write_ucd_mesh_whole(tiofile_id, state_id, main_obj, "ucdzoo");
    else if (!strcmp(JsonGetStr(mesh_obj, "MeshType"), "arbitrary"))
        write_ucd_mesh_whole(tiofile_id, state_id, main_obj, "arbitrary");

    TIO_Call( TIO_Close_State(tiofile_id, state_id),
        "State Close Failed\n");

    /* Close the checkpoint file */
    TIO_Call( TIO_Close(tiofile_id),
      "Close File failed\n");
}

/*!
\brief Main dump implementation for the TyphonIO plugin

This is the function MACSio main calls to orchestrate the dump of data with the plugin.

The command line arguments are processed to set up the application behaviour before 
I/O control is passed to either \ref main_dump_mif or \ref main_dump_sif 
*/
static void main_dump(
    int argi,               /**< [in] Command-line argument index at which first plugin-specific arg appears */
    int argc,               /**< [in] argc from main */
    char **argv,            /**< [in] argv from main */
    json_object * main_obj, /**< [in] The main json object representing all data to be dumped */
    int dumpn,              /**< [in] The number/index of this dump */
    double dumpt)           /**< [in] The time to be associated with this dump (like a simulation's time) */
{
    int rank, size, numFiles;
    /* Without this barrier, I get strange behavior with Silo's MACSIO_MIF interface */
    //mpi_errno = MPI_Barrier(MACSIO_MAIN_Comm);

    /* process cl args */
    process_args(argi, argc, argv);

    rank = json_object_path_get_int(main_obj, "parallel/mpi_rank");
    size = json_object_path_get_int(main_obj, "parallel/mpi_size");

    /* Decide which method to pass control to for MIF or SIF mode */
    json_object *parfmode_obj = json_object_path_get_array(main_obj, "clargs/parallel_file_mode");
    if (parfmode_obj) {
        json_object *modestr = json_object_array_get_idx(parfmode_obj, 0);
        json_object *filecnt = json_object_array_get_idx(parfmode_obj, 1);

        if (!strcmp(json_object_get_string(modestr), "SIF")) {
            float avg_num_parts = json_object_path_get_double(main_obj, "clargs/avg_num_parts");
            if (avg_num_parts == (float ((int) avg_num_parts))){
                if (json_object_get_int(filecnt) > 1){
                    main_dump_msf(main_obj, json_object_get_int(filecnt), dumpn, dumpt);
                } else {
                    main_dump_sif(main_obj, dumpn, dumpt);
                }
            } else {
                // CURRENTLY, SIF CAN WORK ONLY ON WHOLE PART COUNTS
                MACSIO_LOG_MSG(Die, ("TyphonIO plugin cannot currently handle SIF mode where "
                                     "there are different numbers of parts on each MPI rank. "
                                     "Set --avg_num_parts to an integral value." ));
            }
        }
        else {
            numFiles = json_object_get_int(filecnt);
            main_dump_mif(main_obj, numFiles, dumpn, dumpt);
        }
    } else {
        char const * modestr = json_object_path_get_string(main_obj, "clargs/parallel_file_mode");
        if (!strcmp(modestr, "SIF")) {
            float avg_num_parts = json_object_path_get_double(main_obj, "clargs/avg_num_parts");
            if (avg_num_parts == (float ((int) avg_num_parts)))
                main_dump_sif(main_obj, dumpn, dumpt);
            else {
                // CURRENTLY, SIF CAN WORK ONLY ON WHOLE PART COUNTS
                MACSIO_LOG_MSG(Die, ("TyphonIO plugin cannot currently handle SIF mode where "
                                     "there are different numbers of parts on each MPI rank. "
                                     "Set --avg_num_parts to an integral value." ));
            }
        }
        else if (!strcmp(modestr, "MIFMAX"))
            numFiles = json_object_path_get_int(main_obj, "parallel/mpi_size");
        else if (!strcmp(modestr, "MIFAUTO")) {
            /* Call utility to determine optimal file count */
            // ADD UTILIT TO DETERMINE OPTIMAL FILE COUNT
        }
        main_dump_mif(main_obj, numFiles, dumpn, dumpt);
    }
}

/*!
\brief Method to register this plugin with MACSio main

Due to its use to initialize a file-scope, static const variable, this
function winds up being called at load time (e.g. before main is even called).

Its purpose is to add key information about this plugin to MACSio's global
interface table.
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

/*!@}*/
