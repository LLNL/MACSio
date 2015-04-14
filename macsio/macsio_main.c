#warning ADD COPYRIGHT INFORMATION TO ALL FILES
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#ifdef HAVE_SCR
#ifdef __cplusplus
extern "C" {
#endif
#include <scr.h>
#ifdef __cplusplus
}
#endif
#endif

#include <ifacemap.h>
#include <macsio_clargs.h>
#include <macsio_log.h>
#include <macsio_main.h>
#include <macsio_timing.h>
#include <macsio_utils.h>

#include <json-c/json.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

#define MAX(A,B) (((A)>(B))?(A):(B))

extern char **enviornp;

#ifdef HAVE_MPI
MPI_Comm MACSIO_MAIN_Comm = MPI_COMM_WORLD;
#else
int MACSIO_MAIN_Comm = ;
#endif

int MACSIO_MAIN_Size = 1;
int MACSIO_MAIN_Rank = 0;

static json_object *
make_random_int()
{
    return json_object_new_int(random() % 100000);
}

static json_object *
make_random_double()
{
     return json_object_new_double(
         (double) (random() % 100000) / (random() % 100000 + 1));
}

static json_object *
make_random_primitive()
{
    int rval = random() % 100;
    if (rval < 33) /* favor doubles over ints 2:1 */
        return make_random_int();
    else
        return make_random_double();
}

static json_object *
make_random_string(int nthings)
{
    int i;
    char *rval = (char *) malloc(nthings);
    json_object *retval;

    for (i = 0; i < nthings-1; i++)
        rval[i] = 'A' + (random() % 61);
    rval[nthings-1] = '\0';
    retval = json_object_new_string(rval);
    free(rval);

    return retval;
}

static json_object *
make_random_array(int nthings)
{
    int i, rval = random() % 100;
    json_object *retval = json_object_new_array();
    for (i = 0; i < nthings; i++)
    {
        if (rval < 33) /* favor double arrays over int arrays 2:1 */
            json_object_array_add(retval, make_random_int());
        else
            json_object_array_add(retval, make_random_double());
    }
    return retval;
}

static json_object *
make_random_extarr(int nthings)
{
    
    int dims[2], ndims = random() % 2 + 1;
    int rval = random() % 100;
    json_extarr_type dtype;
    void *data;

    dims[0] = nthings;
    if (ndims == 2)
        MACSIO_UTILS_Best2DFactors(nthings, &dims[0], &dims[1]);

    if (rval < 33) /* favor double arrays over int arrays 2:1 */
    {
        int i, *vals = (int *) malloc(nthings * sizeof(int));
        for (i = 0; i < nthings; i++)
            vals[i] = i % 11 ? i : random() % nthings;
        dtype = json_extarr_type_int32;
        data = vals;
    }
    else
    {
        int i;
        double *vals = (double *) malloc(nthings * sizeof(double));
        for (i = 0; i < nthings; i++)
            vals[i] = (double) (random() % 100000) / (random() % 100000 + 1);
        dtype = json_extarr_type_flt64;
        data = vals;
    }

    return json_object_new_extarr(data, dtype, ndims, dims);
}

static json_object *
make_random_object_recurse(int nthings, int depth)
{
    int rval = random() % 100;
    int prim_cutoff, string_cutoff, array_cutoff, extarr_cutoff;

    /* adjust cutoffs to affect odds of different kinds of objects depending on total size */
    if (depth == 0 && nthings > 1)
    {
        prim_cutoff = 0; string_cutoff = 0; array_cutoff = 0; extarr_cutoff = 0;
    }
    else if (nthings > 10000)
    {
        prim_cutoff = 0; string_cutoff = 5; array_cutoff = 10; extarr_cutoff = 30;
    }
    else if (nthings > 1000)
    {
        prim_cutoff = 0; string_cutoff = 10; array_cutoff = 20; extarr_cutoff = 60;
    }
    else if (nthings > 100)
    {
        prim_cutoff = 0; string_cutoff = 25; array_cutoff = 55; extarr_cutoff = 85;
    }
    else if (nthings > 10)
    {
        prim_cutoff = 0; string_cutoff = 40; array_cutoff = 80; extarr_cutoff = 92;
    }
    else if (nthings > 1)
    {
        prim_cutoff = 0; string_cutoff = 40; array_cutoff = 85; extarr_cutoff = 96;
    }
    else
    {
        prim_cutoff = 100;
    }

    if (rval < prim_cutoff)
        return make_random_primitive();
    else if (rval < string_cutoff)
        return make_random_string(nthings);
    else if (rval < array_cutoff)
        return make_random_array(nthings);
    else if (rval < extarr_cutoff)
        return make_random_extarr(nthings);
    else 
    {
        int i;
        int nmembers = random() % (nthings > 100000 ? 500:
                                  (nthings > 10000  ? 100:
                                  (nthings > 1000   ?  25:
                                  (nthings > 100    ?  10:
                                  (nthings > 10     ?   3:
                                   nthings)))));
        json_object *obj = json_object_new_object();

        nthings -= nmembers;
        depth++;
        for (i = 0; i < nmembers; i++)
        {
            char name[32];
            int nthings_member = random() % nthings;
            snprintf(name, sizeof(name), "member%04d", i++);
            json_object_object_add(obj, name, make_random_object_recurse(nthings_member, depth));
            nthings -= nthings_member;
            if (nthings <= 0) break;
        }
        return obj;
    }
}

static json_object *
make_random_object(int nthings)
{
    return make_random_object_recurse(nthings, 0);
}

#warning NEED TO REPLACE STRINGS WITH KEYS FOR MESH PARAMETERS
static json_object *
make_uniform_mesh_coords(int ndims, int const *dims, double const *bounds)
{
    json_object *coords = json_object_new_object();

    json_object_object_add(coords, "CoordBasis", json_object_new_string("X,Y,Z"));
    json_object_object_add(coords, "OriginX", json_object_new_double(MACSIO_UTILS_XMin(bounds)));
    json_object_object_add(coords, "OriginY", json_object_new_double(MACSIO_UTILS_YMin(bounds)));
    json_object_object_add(coords, "OriginZ", json_object_new_double(MACSIO_UTILS_ZMin(bounds)));
    json_object_object_add(coords, "DeltaX", json_object_new_double(MACSIO_UTILS_XDelta(dims, bounds)));
    json_object_object_add(coords, "DeltaY", json_object_new_double(MACSIO_UTILS_YDelta(dims, bounds)));
    json_object_object_add(coords, "DeltaZ", json_object_new_double(MACSIO_UTILS_ZDelta(dims, bounds)));
    json_object_object_add(coords, "NumX", json_object_new_int(MACSIO_UTILS_XDim(dims)));
    json_object_object_add(coords, "NumY", json_object_new_int(MACSIO_UTILS_YDim(dims)));
    json_object_object_add(coords, "NumZ", json_object_new_int(MACSIO_UTILS_ZDim(dims)));

    return coords;
}

#warning PACKAGE EXTARR METHOD WITH CHECKSUM STUFF

static json_object *
make_rect_mesh_coords(int ndims, int const *dims, double const *bounds)
{
    json_object *coords = json_object_new_object();
    double *vals, delta;
    int i;

#warning SUPPORT DIFFERENT DATATYPES HERE

    json_object_object_add(coords, "CoordBasis", json_object_new_string("X,Y,Z"));

    /* build X coordinate array */
    delta = MACSIO_UTILS_XDelta(dims, bounds);
    vals = (double *) malloc(MACSIO_UTILS_XDim(dims) * sizeof(double));
    for (i = 0; i < MACSIO_UTILS_XDim(dims); i++)
        vals[i] = MACSIO_UTILS_XMin(bounds) + i * delta;
    json_object_object_add(coords, "XAxisCoords", json_object_new_extarr(vals, json_extarr_type_flt64, 1, &dims[0]));

    if (ndims > 1)
    {
        /* build Y coordinate array */
        delta = MACSIO_UTILS_YDelta(dims, bounds);
        vals = (double *) malloc(MACSIO_UTILS_YDim(dims) * sizeof(double));
        for (i = 0; i < MACSIO_UTILS_YDim(dims); i++)
            vals[i] = MACSIO_UTILS_YMin(bounds) + i * delta;
        json_object_object_add(coords, "YAxisCoords", json_object_new_extarr(vals, json_extarr_type_flt64, 1, &dims[1]));
    }

    if (ndims > 2)
    {
        /* build Z coordinate array */
        delta = MACSIO_UTILS_ZDelta(dims, bounds);
        vals = (double *) malloc(MACSIO_UTILS_ZDim(dims) * sizeof(double));
        for (i = 0; i < MACSIO_UTILS_ZDim(dims); i++)
            vals[i] = MACSIO_UTILS_ZMin(bounds) + i * delta;
        json_object_object_add(coords, "ZAxisCoords", json_object_new_extarr(vals, json_extarr_type_flt64, 1, &dims[2]));
    }

    return coords;
}

static json_object *
make_curv_mesh_coords(int ndims, int const *dims, double const *bounds)
{
    json_object *coords = json_object_new_object();
    double *x = 0, *y = 0, *z = 0;
    double dx = MACSIO_UTILS_XDelta(dims, bounds);
    double dy = MACSIO_UTILS_YDelta(dims, bounds);
    double dz = MACSIO_UTILS_ZDelta(dims, bounds);
    int nx = MACSIO_UTILS_XDim(dims), ny = MAX(MACSIO_UTILS_YDim(dims),1), nz = MAX(MACSIO_UTILS_ZDim(dims),1);
    int i, j, k;

    json_object_object_add(coords, "CoordBasis", json_object_new_string("X,Y,Z")); /* "R,Theta,Phi" */

    x = (double *) malloc(nx * ny * nz * sizeof(double));
    if (ndims > 1)
        y = (double *) malloc(nx * ny * nz * sizeof(double));
    if (ndims > 2)
        z = (double *) malloc(nx * ny * nz * sizeof(double));
    for (i = 0; i < nx; i++)
    {
        for (j = 0; j < ny; j++)
        {
            for (k = 0; k < nz; k++)
            {
                int idx = k * ny * nx  + j * nx + i;
                       x[idx] = MACSIO_UTILS_XMin(bounds) + i * dx;
                if (y) y[idx] = MACSIO_UTILS_YMin(bounds) + j * dy;
                if (z) z[idx] = MACSIO_UTILS_ZMin(bounds) + k * dz;
            }
        }
    }
    json_object_object_add(coords, "XCoords", json_object_new_extarr(x, json_extarr_type_flt64, ndims, dims));
    if (ndims > 1)
        json_object_object_add(coords, "YCoords", json_object_new_extarr(y, json_extarr_type_flt64, ndims, dims));
    if (ndims > 2)
        json_object_object_add(coords, "ZCoords", json_object_new_extarr(z, json_extarr_type_flt64, ndims, dims));

    return coords;
}

static json_object *
make_ucdzoo_mesh_coords(int ndims, int const *dims, double const *bounds)
{
    /* same case as curvilinear mesh */
    return make_curv_mesh_coords(ndims, dims, bounds);
}

static json_object *
make_arb_mesh_coords(int ndims, int const *dims, double const *bounds)
{
    /* same case as curvilinear mesh */
    return make_curv_mesh_coords(ndims, dims, bounds);
}

static json_object *
make_structured_mesh_topology(int ndims, int const *dims)
{
    json_object *topology = json_object_new_object();
    int nx = MACSIO_UTILS_XDim(dims);
    int ny = MACSIO_UTILS_YDim(dims);
    int nz = MACSIO_UTILS_ZDim(dims);

    json_object_object_add(topology, "Type", json_object_new_string("Templated"));
    json_object_object_add(topology, "DomainDim", json_object_new_int(ndims));
    json_object_object_add(topology, "RangeDim", json_object_new_int(0)); /* node refs */

    /* would be better for elem types to be enum or int */
    /* We can use a bonified json array here because the template is small. But,
       should we? */

    if (ndims == 1)
    {
        json_object *topo_template = json_object_new_array();

        json_object_array_add(topo_template, json_object_new_int(MU_SeqIdx1(0)));
        json_object_array_add(topo_template, json_object_new_int(MU_SeqIdx1(1)));

        json_object_object_add(topology, "ElemType", json_object_new_string("Beam2"));
        json_object_object_add(topology, "Template", topo_template);
    }
    else if (ndims == 2)
    {
        json_object *topo_template = json_object_new_array();

        /* For domain entity (zone i,j), here are the nodal offsets in
           linear address space, right-hand-rule starting from lower-left corner */
        json_object_array_add(topo_template, json_object_new_int(MU_SeqIdx2(0,0,nx)));
        json_object_array_add(topo_template, json_object_new_int(MU_SeqIdx2(1,0,nx)));
        json_object_array_add(topo_template, json_object_new_int(MU_SeqIdx2(1,1,nx)));
        json_object_array_add(topo_template, json_object_new_int(MU_SeqIdx2(0,1,nx)));

        json_object_object_add(topology, "ElemType", json_object_new_string("Quad4"));
        json_object_object_add(topology, "Template", topo_template);
    }
    else if (ndims == 3)
    {
        json_object *topo_template = json_object_new_array();

        /* For domain entity (zone i,j,k), here are the nodal offsets
           in linear address space, starting from lower-left-back corner,
           back-face first with inward normal using rhr then front face with
           outward normal using rhr. */
        json_object_array_add(topo_template, json_object_new_int(MU_SeqIdx3(0,0,0,nx,ny)));
        json_object_array_add(topo_template, json_object_new_int(MU_SeqIdx3(1,0,0,nx,ny)));
        json_object_array_add(topo_template, json_object_new_int(MU_SeqIdx3(1,1,0,nx,ny)));
        json_object_array_add(topo_template, json_object_new_int(MU_SeqIdx3(0,1,0,nx,ny)));
        json_object_array_add(topo_template, json_object_new_int(MU_SeqIdx3(0,0,1,nx,ny)));
        json_object_array_add(topo_template, json_object_new_int(MU_SeqIdx3(1,0,1,nx,ny)));
        json_object_array_add(topo_template, json_object_new_int(MU_SeqIdx3(1,1,1,nx,ny)));
        json_object_array_add(topo_template, json_object_new_int(MU_SeqIdx3(0,1,1,nx,ny)));

        json_object_object_add(topology, "ElemType", json_object_new_string("Hex8"));
        json_object_object_add(topology, "Template", topo_template);
    }

    return topology;
}

static json_object *
make_uniform_mesh_topology(int ndims, int const *dims)
{
    return make_structured_mesh_topology(ndims, dims);
}

static json_object *
make_rect_mesh_topology(int ndims, int const *dims)
{
    return make_structured_mesh_topology(ndims, dims);
}

static json_object *
make_curv_mesh_topology(int ndims, int const *dims)
{
    return make_structured_mesh_topology(ndims, dims);
}

static json_object *
make_ucdzoo_mesh_topology(int ndims, int const *dims)
{
    json_object *topology = json_object_new_object();
    int i,j,k,n=0;
    int nx = MACSIO_UTILS_XDim(dims), ny = MAX(MACSIO_UTILS_YDim(dims),1), nz = MAX(MACSIO_UTILS_ZDim(dims),1);
    int ncells = nx * ny * nz;
    int cellsize = 2 * ndims;
    int *nodelist = (int *) malloc(ncells * cellsize * sizeof(int));
    int nl_dims[2] = {ncells, cellsize};

    json_object_object_add(topology, "Type", json_object_new_string("Explicit"));
    json_object_object_add(topology, "DomainDim", json_object_new_int(ndims));
    json_object_object_add(topology, "RangeDim", json_object_new_int(0)); /* node refs */

    if (ndims == 1)
    {
        for (i = 0; i < nx; i++) 
        {
            nodelist[n++] = MU_SeqIdx1(i+0);
            nodelist[n++] = MU_SeqIdx1(i+1);
        }
        json_object_object_add(topology, "ElemType", json_object_new_string("Beam2"));
    }
    else if (ndims == 2)
    {
        for (i = 0; i < nx; i++)
        {
            for (j = 0; j < ny; j++)
            {
                nodelist[n++] = MU_SeqIdx2(i+0,j+0,nx);
                nodelist[n++] = MU_SeqIdx2(i+1,j+0,nx);
                nodelist[n++] = MU_SeqIdx2(i+1,j+1,nx);
                nodelist[n++] = MU_SeqIdx2(i+0,j+1,nx);
            }
        }
        json_object_object_add(topology, "ElemType", json_object_new_string("Quad4"));
    }
    else if (ndims == 3)
    {
        for (i = 0; i < nx; i++)
        {
            for (j = 0; j < ny; j++)
            {
                for (k = 0; k < nz; k++)
                {
                    nodelist[n++] = MU_SeqIdx3(i+0,j+0,k+0,nx,ny);
                    nodelist[n++] = MU_SeqIdx3(i+1,j+0,k+0,nx,ny);
                    nodelist[n++] = MU_SeqIdx3(i+1,j+1,k+0,nx,ny);
                    nodelist[n++] = MU_SeqIdx3(i+0,j+1,k+0,nx,ny);
                    nodelist[n++] = MU_SeqIdx3(i+0,j+0,k+1,nx,ny);
                    nodelist[n++] = MU_SeqIdx3(i+1,j+0,k+1,nx,ny);
                    nodelist[n++] = MU_SeqIdx3(i+1,j+1,k+1,nx,ny);
                    nodelist[n++] = MU_SeqIdx3(i+0,j+1,k+1,nx,ny);
                }
            }
        }
        json_object_object_add(topology, "ElemType", json_object_new_string("Hex8"));
    }
    json_object_object_add(topology, "ElemSize", json_object_new_int(cellsize));
    json_object_object_add(topology, "Nodelist", json_object_new_extarr(nodelist, json_extarr_type_int32, 2, nl_dims));

    return topology;
}

#warning REPLACE STRINGS FOR CENTERING AND DTYPE WITH ENUMS
static json_object *
make_scalar_var(int ndims, int const *dims, double const *bounds,
    char const *centering, char const *dtype, char const *kind)
{
    json_object *var_obj = json_object_new_object();
    int i,j,k,n;
    int dims2[3] = {1,1,1};
    int minus_one = strcmp(centering, "zone")?0:-1;
    json_object *data_obj;
    double *valdp;
    int    *valip;

    for (i = 0; i < ndims; i++)
        dims2[i] = dims[i] + minus_one;

#warning NEED EXPLICIT NAME FOR VARIABLE
    json_object_object_add(var_obj, "name", json_object_new_string(kind));
    json_object_object_add(var_obj, "centering", json_object_new_string(centering));
    if (!strcmp(dtype, "double"))
        data_obj = json_object_new_extarr_alloc(json_extarr_type_flt64, ndims, dims2);
    else if (!strcmp(dtype, "int"))
        data_obj = json_object_new_extarr_alloc(json_extarr_type_int32, ndims, dims2);
    json_object_object_add(var_obj, "data", data_obj);
    valdp = (double *) json_object_extarr_data(data_obj);
    valip = (int *) json_object_extarr_data(data_obj);

    n = 0;
    srandom(0xBabeFace);
    for (k = 0; k < dims2[2]; k++)
    {
        for (j = 0; j < dims2[1]; j++)
        {
            for (i = 0; i < dims2[0]; i++)
            {
#warning PUT THESE INTO A GENERATOR FUNCTION
#warning ACCOUNT FOR HALF ZONE OFFSETS
                if (!strcmp(kind, "constant"))
                    valdp[n++] = 1.0;
                else if (!strcmp(kind, "random"))
                    valdp[n++] = (double) (random() % 1000) / 1000;
                else if (!strcmp(kind, "xramp"))
                    valdp[n++] = bounds[0] + i * MACSIO_UTILS_XDelta(dims, bounds);
                else if (!strcmp(kind, "spherical"))
                {
                    double x = bounds[0] + i * MACSIO_UTILS_XDelta(dims, bounds);
                    double y = bounds[1] + j * MACSIO_UTILS_YDelta(dims, bounds);
                    double z = bounds[2] + k * MACSIO_UTILS_ZDelta(dims, bounds);
                    valdp[n++] = sqrt(x*x+y*y+z*z);
                }
                else if (!strcmp(kind, "ysin"))
                {
                    double y = bounds[1] + j * MACSIO_UTILS_YDelta(dims, bounds);
                    valdp[n++] = sin(y*3.1415266);
                }
                else if (!strcmp(kind, "xlayers"))
                {
                    valip[n++] = (i / 20) % 3;
                }
            }
        }
    }
#warning ADD CHECKSUM TO JSON OBJECT

    return var_obj; 

}

static json_object *
make_vector_var(int ndims, int const *dims, double const *bounds)
{
    return 0;
}

static json_object *
make_tensor_var(int ndims, int const *dims, double const *bounds)
{
    return 0;
}

static json_object *
make_subset_var(int ndims, int const *dims, double const *bounds)
{
    return 0;
}

static json_object *
make_mesh_vars(int ndims, int const *dims, double const *bounds)
{
    json_object *vars_array = json_object_new_array();

    json_object_array_add(vars_array, make_scalar_var(ndims, dims, bounds, "zone", "double", "constant"));
    json_object_array_add(vars_array, make_scalar_var(ndims, dims, bounds, "zone", "double", "random"));
    json_object_array_add(vars_array, make_scalar_var(ndims, dims, bounds, "zone", "double", "spherical"));
    json_object_array_add(vars_array, make_scalar_var(ndims, dims, bounds, "node", "double", "xramp"));
    json_object_array_add(vars_array, make_scalar_var(ndims, dims, bounds, "node", "double", "ysin"));
    json_object_array_add(vars_array, make_scalar_var(ndims, dims, bounds, "zone", "int", "xlayers"));

    return vars_array;
}

static json_object *
make_arb_mesh_topology(int ndims, int const *dims)
{
    return 0;
}

#warning UNIFY PART CHUNK TERMINOLOGY THEY ARE THE SAME
#warning SHOULD NAME CHUNK/PART NUMBER HERE TO INDICATE IT IS A GLOBAL NUMBER
static json_object *make_uniform_mesh_chunk(int chunkId, int ndims, int const *dims, double const *bounds)
{
    json_object *mesh_chunk = json_object_new_object();
    json_object_object_add(mesh_chunk, "MeshType", json_object_new_string("uniform"));
    json_object_object_add(mesh_chunk, "ChunkID", json_object_new_int(chunkId));
    json_object_object_add(mesh_chunk, "GeomDim", json_object_new_int(ndims));
    json_object_object_add(mesh_chunk, "TopoDim", json_object_new_int(ndims));
    json_object_object_add(mesh_chunk, "LogDims", MACSIO_UTILS_MakeDimsJsonArray(ndims, dims));
    json_object_object_add(mesh_chunk, "Bounds", MACSIO_UTILS_MakeBoundsJsonArray(bounds));
    json_object_object_add(mesh_chunk, "Coords", make_uniform_mesh_coords(ndims, dims, bounds));
    json_object_object_add(mesh_chunk, "Topology", make_uniform_mesh_topology(ndims, dims));
    return mesh_chunk;
}

#warning ADD CALLS TO VARGEN FOR OTHER MESH TYPES
static json_object *make_rect_mesh_chunk(int chunkId, int ndims, int const *dims, double const *bounds)
{
    json_object *chunk_obj = json_object_new_object();
    json_object *mesh_obj = json_object_new_object();
    json_object_object_add(mesh_obj, "MeshType", json_object_new_string("rectilinear"));
    json_object_object_add(mesh_obj, "ChunkID", json_object_new_int(chunkId));
    json_object_object_add(mesh_obj, "GeomDim", json_object_new_int(ndims));
    json_object_object_add(mesh_obj, "TopoDim", json_object_new_int(ndims));
    json_object_object_add(mesh_obj, "LogDims", MACSIO_UTILS_MakeDimsJsonArray(ndims, dims));
    json_object_object_add(mesh_obj, "Bounds", MACSIO_UTILS_MakeBoundsJsonArray(bounds));
    json_object_object_add(mesh_obj, "Coords", make_rect_mesh_coords(ndims, dims, bounds));
    json_object_object_add(mesh_obj, "Topology", make_rect_mesh_topology(ndims, dims));
    json_object_object_add(chunk_obj, "Mesh", mesh_obj);
#warning ADD NVARS AND VARMAPS ARGS HERE
    json_object_object_add(chunk_obj, "Vars", make_mesh_vars(ndims, dims, bounds));
    return chunk_obj;
}

static json_object *make_curv_mesh_chunk(int chunkId, int ndims, int const *dims, double const *bounds)
{
    json_object *mesh_chunk = json_object_new_object();
    json_object_object_add(mesh_chunk, "MeshType", json_object_new_string("curvilinear"));
    json_object_object_add(mesh_chunk, "ChunkID", json_object_new_int(chunkId));
    json_object_object_add(mesh_chunk, "GeomDim", json_object_new_int(ndims));
    json_object_object_add(mesh_chunk, "TopoDim", json_object_new_int(ndims));
    json_object_object_add(mesh_chunk, "LogDims", MACSIO_UTILS_MakeDimsJsonArray(ndims, dims));
    json_object_object_add(mesh_chunk, "Bounds", MACSIO_UTILS_MakeBoundsJsonArray(bounds));
    json_object_object_add(mesh_chunk, "Coords", make_curv_mesh_coords(ndims, dims, bounds));
    json_object_object_add(mesh_chunk, "Topology", make_curv_mesh_topology(ndims, dims));
    return mesh_chunk;
}

static json_object *make_ucdzoo_mesh_chunk(int chunkId, int ndims, int const *dims, double const *bounds)
{
    json_object *mesh_chunk = json_object_new_object();
    json_object_object_add(mesh_chunk, "MeshType", json_object_new_string("ucdzoo"));
    json_object_object_add(mesh_chunk, "ChunkID", json_object_new_int(chunkId));
    json_object_object_add(mesh_chunk, "GeomDim", json_object_new_int(ndims));
    json_object_object_add(mesh_chunk, "TopoDim", json_object_new_int(ndims));
    json_object_object_add(mesh_chunk, "LogDims", MACSIO_UTILS_MakeDimsJsonArray(ndims, dims));
    json_object_object_add(mesh_chunk, "Bounds", MACSIO_UTILS_MakeBoundsJsonArray(bounds));
    json_object_object_add(mesh_chunk, "Coords", make_ucdzoo_mesh_coords(ndims, dims, bounds));
    json_object_object_add(mesh_chunk, "Topology", make_ucdzoo_mesh_topology(ndims, dims));
    return mesh_chunk;
}

/* dims are # nodes in x, y and z,
   bounds are xmin,ymin,zmin,xmax,ymax,zmax */
static json_object *
make_mesh_chunk(int chunkId, int ndims, int const *dims, double const *bounds, char const *type)
{
         if (!strncasecmp(type, "uniform", sizeof("uniform")))
        return make_uniform_mesh_chunk(chunkId, ndims, dims, bounds);
    else if (!strncasecmp(type, "rectilinear", sizeof("rectilinear")))
        return make_rect_mesh_chunk(chunkId, ndims, dims, bounds);
    else if (!strncasecmp(type, "curvilinear", sizeof("curvilinear")))
        return 0;
    else if (!strncasecmp(type, "unstructured", sizeof("unstructured")))
        return make_ucdzoo_mesh_chunk(chunkId, ndims, dims, bounds);
    else if (!strncasecmp(type, "arbitrary", sizeof("arbitrary")))
        return 0;
    return 0;
}

static int choose_part_count(int K, int mod, int *R, int *Q)
{
    /* We have either K or K+1 parts so randomly select that for each rank */
    int retval = K + random() % mod;
    if (retval == K)
    {
        if (*R > 0)
        {
            *R--;
        }
        else if (*Q > 0)
        {
            retval = K+1;
            *Q--;
        }
    }
    else
    {
        if (*Q > 0)
        {
            *Q--;
        }
        else if (*R > 0)
        {
            retval = K;
            *R--;
        }
    }
    return retval;
}

#warning GET FUNTION NAMING CONSISTENT THROUGHOUT SOURCE FILES
#warning MAYBE PASS IN SEED HERE OR ADD TO MAIN_OBJ
#warning COULD IMPROVE DESIGN A BIT BY SEPARATING ALGORITHM FOR GEN WITH A CALLBACK
/* Just a very simple spatial partitioning. We use the same exact algorithm
   to determine which rank owns a chunk. So, we overload this method and
   for that purpose as well even though in that case, it doesn generate
   anything. */
static json_object *
MACSIO_GenerateStaticDumpObject(json_object *main_obj, int *rank_owning_chunkId)
{
#warning FIX LEAK OF OBJECTS FOR QUERY CASE
    json_object *mesh_obj = rank_owning_chunkId?0:json_object_new_object();
    json_object *global_obj = rank_owning_chunkId?0:json_object_new_object();
    json_object *part_array = rank_owning_chunkId?0:json_object_new_array();
    int size = json_object_path_get_int(main_obj, "parallel/mpi_size");
    int part_size = json_object_path_get_int(main_obj, "clargs/part_size") / sizeof(double);
    double avg_num_parts = json_object_path_get_double(main_obj, "clargs/avg_num_parts");
    int dim = json_object_path_get_int(main_obj, "clargs/part_dim");
    double total_num_parts_d = size * avg_num_parts;
    int total_num_parts = (int) lround(total_num_parts_d);
    int myrank = json_object_path_get_int(main_obj, "parallel/mpi_rank");

    int K = floor(avg_num_parts); /* some ranks get K parts */
    int K1 = K+1;                 /* some ranks get K+1 parts */
    int Q = total_num_parts - size * K; /* # ranks with K+1 parts */
    int R = size - Q;                   /* # ranks with K parts */
    int mod = ((double)K == avg_num_parts)?1:2;
    int nx_parts = total_num_parts, ny_parts = 1, nz_parts = 1;
    int nx = part_size, ny = 1, nz = 1;
    int ipart_width = 1, jpart_width = 0, kpart_width = 0;
    int ipart, jpart, kpart, chunk, rank, parts_on_this_rank;
    int part_dims[3], part_block_dims[3], global_log_dims[3], global_indices[3];
    double part_bounds[6], global_bounds[6];

    /* Determine spatial size and arrangement of parts */
    if (dim == 1)
    {
        ; /* no-op */
    }
    else if (dim == 2)
    {
        MACSIO_UTILS_Best2DFactors(total_num_parts, &nx_parts, &ny_parts);
        MACSIO_UTILS_Best2DFactors(part_size, &nx, &ny);
        jpart_width = 1;
    }
    else if (dim == 3)
    {
        MACSIO_UTILS_Best3DFactors(total_num_parts, &nx_parts, &ny_parts, &nz_parts);
        MACSIO_UTILS_Best3DFactors(part_size, &nx, &ny, &nz);
        kpart_width = 1;
    }
    MACSIO_UTILS_SetDims(part_dims, nx, ny, nz);
    MACSIO_UTILS_SetDims(part_block_dims, nx_parts, ny_parts, nz_parts);
    MACSIO_UTILS_SetDims(global_log_dims, nx * nx_parts, ny * ny_parts, nz * nz_parts);
    MACSIO_UTILS_SetBounds(global_bounds, 0, 0, 0,
        nx_parts * ipart_width, ny_parts * jpart_width, nz_parts * kpart_width);
    if (!rank_owning_chunkId)
    {
        json_object_object_add(global_obj, "TotalParts", json_object_new_int(total_num_parts));
#warning NOT SURE PartsLogDims IS USEFUL IN GENERAL CASE
        json_object_object_add(global_obj, "PartsLogDims", MACSIO_UTILS_MakeDimsJsonArray(dim, part_block_dims));
        json_object_object_add(global_obj, "LogDims", MACSIO_UTILS_MakeDimsJsonArray(dim, global_log_dims));
        json_object_object_add(global_obj, "Bounds", MACSIO_UTILS_MakeBoundsJsonArray(global_bounds));
        json_object_object_add(mesh_obj, "global", global_obj);
    }

    rank = 0;
    chunk = 0;
    srandom(0xDeadBeef); /* initialize for choose_part_count */
    parts_on_this_rank = choose_part_count(K,mod,&R,&Q);
    for (ipart = 0; ipart < nx_parts; ipart++)
    {
        for (jpart = 0; jpart < ny_parts; jpart++)
        {
            for (kpart = 0; kpart < nz_parts; kpart++)
            {
                if (!rank_owning_chunkId && rank == myrank)
                {
                    int global_log_origin[3];

                    /* build mesh part on this rank */
                    MACSIO_UTILS_SetBounds(part_bounds, (double) ipart, (double) jpart, (double) kpart,
                        (double) ipart+ipart_width, (double) jpart+jpart_width, (double) kpart+kpart_width);
                    json_object *part_obj = make_mesh_chunk(chunk, dim, part_dims, part_bounds,
                        json_object_path_get_string(main_obj, "clargs/part_type"));
                    MACSIO_UTILS_SetDims(global_indices, ipart, jpart, kpart);
#warning MAYBE MOVE GLOBAL LOG INDICES TO make_mesh_chunk
#warning GlogalLogIndices MAY NOT BE NEEDED
                    json_object_object_add(part_obj, "GlobalLogIndices",
                        MACSIO_UTILS_MakeDimsJsonArray(dim, global_indices));
                    MACSIO_UTILS_SetDims(global_log_origin, ipart * nx, jpart * ny, kpart * nz);
                    json_object_object_add(part_obj, "GlobalLogOrigin",
                        MACSIO_UTILS_MakeDimsJsonArray(dim, global_log_origin));
                    json_object_array_add(part_array, part_obj);
                }
                else if (rank_owning_chunkId && *rank_owning_chunkId == chunk)
                {
                    *rank_owning_chunkId = rank;
                    return 0;
                }
                chunk++;
                parts_on_this_rank--;
                if (parts_on_this_rank == 0)
                {
                    rank++;
                    parts_on_this_rank = choose_part_count(K,mod,&R,&Q);
                }
            }
        }
    } 
    json_object_object_add(mesh_obj, "parts", part_array);

    return mesh_obj;

}

int MACSIO_MAIN_GetRankOwningPart(json_object *main_obj, int chunkId)
{
    int tmp = chunkId;
    MACSIO_GenerateStaticDumpObject(main_obj, &tmp);
    return tmp;
}

static void handle_help_request_and_exit(int argi, int argc, char **argv)
{
    int i, n, *ids=0;;
    FILE *outFILE = (isatty(2) ? stderr : stdout);

    MACSIO_GetInterfaceIds(&n, &ids);
    for (i = 0; i < n; i++)
    {
        const MACSIO_IFaceHandle_t *iface = MACSIO_GetInterfaceById(ids[i]);
        if (iface->processArgsFunc)
        {
            fprintf(outFILE, "\nOptions specific to the \"%s\" I/O plugin\n", iface->name);
            (*(iface->processArgsFunc))(argi, argc, argv);
        }
    }
#ifdef HAVE_MPI
    {   int result;
        if ((MPI_Initialized(&result) == MPI_SUCCESS) && result)
            MPI_Finalize();
    }
#endif
    exit(0);
}

static void handle_list_request_and_exit()
{
    int i, n, *ids = 0;
    FILE *outFILE = (isatty(2) ? stderr : stdout);
    char names_buf[1024];

    names_buf[0] = '\0';
    MACSIO_GetInterfaceIds(&n, &ids);
    for (i = 0; i < n; i++)
    {
        char const *nm = MACSIO_GetInterfaceName(ids[i]);
        strcat(names_buf, "\"");
        strcat(names_buf, nm);
        strcat(names_buf, "\", ");
        if (!((i+1) % 10)) strcat(names_buf, "\n");
    }
    fprintf(outFILE, "List of available I/O-library plugins...\n");
    fprintf(outFILE, "%s\n", names_buf);
#ifdef HAVE_MPI
    {   int result;
        if ((MPI_Initialized(&result) == MPI_SUCCESS) && result)
            MPI_Finalize();
    }
#endif
    exit(0);
}

static json_object *ProcessCommandLine(int argc, char *argv[], int *plugin_argi)
{
    MACSIO_CLARGS_ArgvFlags_t const argFlags = {MACSIO_CLARGS_WARN, MACSIO_CLARGS_TOJSON};
    json_object *mainJargs = 0;
    int plugin_args_start = -1;
    int cl_result;

    cl_result = MACSIO_CLARGS_ProcessCmdline((void**)&mainJargs, argFlags, 1, argc, argv,
        "--interface %s",
            "Specify the name of the interface to be tested. Use keyword 'list'\n"
            "to print a list of all known interface names and then exit.",
        "--parallel_file_mode %s %d",
            "Specify the parallel file mode. There are several choices.\n"
            "Use 'MIF' for Multiple Independent File (Poor Man's) mode and then\n"
            "also specify the number of files. Or, use 'MIFFPP' for MIF mode and\n"
            "one file per processor or 'MIFOPT' for MIF mode and let the test\n"
            "determine the optimum file count. Use 'SIF' for SIngle shared File\n"
            "(Rich Man's) mode. If you also give a file count for SIF mode, then\n"
            "MACSio will perform a sort of hybrid combination of MIF and SIF modes.\n"
            "It will produce the specified number of files by grouping ranks in the\n"
            "the same way MIF does, but I/O within each group will be to a single,\n"
            "shared file using SIF mode.",
        "--avg_num_parts %f",
            "The average number of mesh parts per MPI rank. Non-integral values\n"
            "are acceptable. For example, a value that is half-way between two\n"
            "integers, K and K+1, means that half the ranks have K mesh parts\n"
            "and half have K+1 mesh parts. As another example, a value of 2.75\n"
            "here would mean that 75% of the ranks get 3 parts and 25% of the\n"
            "ranks get 2 parts. Note that the total number of parts is this\n"
            "number multiplied by the MPI communicator size. If the result of that\n"
            "product is non-integral, it will be rounded and a warning message will\n"
            "be generated. [1]",
        "--part_size %d",
            "Mesh part size in bytes. This becomes the nominal I/O request size\n"
            "used by each MPI rank when marshalling data. A following B|K|M|G\n"
            "character indicates 'B'ytes (2^0), 'K'ilobytes (2^10), 'M'egabytes\n"
            "(2^20) or 'G'igabytes (2^30).",
        "--part_dim %d",
            "Spatial dimension of parts; 1, 2, or 3",
        "--part_type %s",
            "Options are 'uniform', 'rectilinear', 'curvilinear', 'unstructured'\n"
            "and 'arbitrary' (only rectinilear is currently implemented)",
        "--part_distribution %s",
            "Specify how parts are distributed to MPI tasks. (currently ignored)",
        "--vars_per_part %d",
            "Number of mesh variable objects in each part. The smallest this can\n"
            "be depends on the mesh type. For rectilinear mesh it is 1. For\n"
            "curvilinear mesh it is the number of spatial dimensions and for\n"
            "unstructured mesh it is the number of spatial dimensions plus\n"
            "2^number of topological dimensions. [50]",
        "--meta_type %s",
            "Specify the type of metadata objects to include in each main dump.\n"
            "Options are 'tabular', 'hierarchical', 'amorphous'. For tabular type\n"
            "data, MACSio will generate a random set of tables of somewhat random\n"
            "structure and content. For amorphous, MACSio will generate a\n"
            "random hierarchy random type and size objects.",
        "--meta_size %d %d",
            "Specify the size of the metadata objects on each processor and\n"
            "separately, the root (or master) processor (MPI rank 0). The size\n"
            "is specified in terms of the total number of bytes in the metadata\n"
            "objects MACSio creates. For example, a type of tabular and a size of\n"
            "10K bytes could result in 3 random tables; one table with 250 unnamed\n"
            "records where each record is an array of 3 doubles for a total of\n"
            "6000 bytes, another table of 200 records where each record is a\n"
            "named integer value where each name is length 8 chars for a total of\n"
            "2400 bytes and a 3rd table of 40 unnamed records where each record\n"
            "is a 40 byte struct comprised of ints and doubles for a total of 1600\n"
            "bytes.",
        "--num_dumps %d",
            "Total number of dumps to marshal [10]",
        "--max_dir_size %d",
            "The maximum number of filesystem objects (e.g. files or subdirectories)\n"
            "that MACSio will create in any one subdirectory. This is typically\n"
            "relevant only in MIF mode because MIF mode can wind up generating many\n"
            "files on each dump. The default setting is unlimited meaning that MACSio\n"
            "will continue to create output files in the same directory until it has\n"
            "completed all dumps. Use a value of zero to force MACSio to put each\n"
            "dump in a separate directory but where the number of top-level directories\n"
            "is still unlimited. The result will be a 2-level directory hierarchy\n"
            "with dump directories at the top and individual dump files in each\n"
            "directory. A value > 0 will cause MACSio to create a tree-like directory\n"
            "structure where the files are the leaves and encompassing dir tree is\n"
            "created such as to maintain the max_dir_size constraint specified here.\n"
            "For example, if the value is set to 32 and the MIF file count is 1024,\n"
            "then each dump will involve a 3-level dir-tree; the top dir containing\n"
            "32 sub-dirs and each sub-dir containing 32 of the 1024 files for the\n"
            "dump. If more than 32 dumps are performed, then the dir-tree will really\n"
            "be 4 or more levels with the first 32 dumps' dir-trees going into the\n"
            "first dir, etc.",
#ifdef HAVE_SCR
        "--exercise_scr",
            "Exercise the Scalable Checkpoint and Restart library to marshal files.\n"
            "Note that this works only in MIFFPP mode.",
#endif
        "--debug_level %d",
            "Set debugging level (1, 2 or 3) of log files. Higher numbers mean\n"
            "more detailed output [0].",
        "--log_line_cnt %d",
            "Set number of lines per rank in the log file [64].",
        "--log_line_length %d",
            "Set log file line length [128].",
        "--alignment %d",
            "Not currently documented",
        "--filebase %s",
            "Basename of generated file(s). ['macsio_']",
        "--fileext %s",
            "Extension of generated file(s). ['.dat']",
        "--plugin-args %n",
            "All arguments after this sentinel are passed to the I/O plugin\n"
            "plugin. The '%n' is a special designator for the builtin 'argi'\n"
            "value.",
    MACSIO_CLARGS_END_OF_ARGS);

#warning DETERMINE IF THIS IS A WRITE TEST OR A READ TEST

#warning FIXME
    plugin_args_start = json_object_path_get_int(mainJargs, "argi");
    plugin_args_start = argc;

    /* if we discovered help was requested, then print each plugin's help too */
    if (cl_result == MACSIO_CLARGS_HELP)
        handle_help_request_and_exit(plugin_args_start+1, argc, argv);

    if (!strcmp(json_object_path_get_string(mainJargs, "interface"), "list"))
        handle_list_request_and_exit();

    /* sanity check some values */
    if (!strcmp(json_object_path_get_string(mainJargs, "interface"), ""))
        MACSIO_LOG_MSG(Die, ("no io-interface specified"));

    if (plugin_argi)
        *plugin_argi = plugin_args_start>-1?plugin_args_start+1:argc;

    return mainJargs;
}

#if 0
static MACSIO_IFaceHandle_t const *GetIOInterface(int argi, int argc, char *argv[], MACSIO_optlist_t const *opts)
{
    int i;
    char testfilename[256];
    char ifacename[256];
    const MACSIO_IFaceHandle_t *retval=0;

    /* First, get rid of the old data file */
    strcpy(ifacename, MACSIO_GetStrOption(opts, IOINTERFACE_NAME));

#warning FIX FILENAME GENERATION
    sprintf(testfilename, "macsio_test_%s.dat", ifacename);
    unlink(testfilename);

    /* search for and instantiate the desired interface */
    retval = MACSIO_GetInterfaceByName(ifacename);
    if (!retval)
        MACSIO_LOG_MSG(Die, ("unable to instantiate IO interface \"%s\"",ifacename));

    return retval;
}
#endif

int
main(int argc, char *argv[])
{
    json_object *main_obj = json_object_new_object();
    json_object *parallel_obj = json_object_new_object();
    json_object *problem_obj = 0;
    json_object *clargs_obj = 0;
    MACSIO_TIMING_TimerId_t main_tid;
    MACSIO_TIMING_GroupMask_t main_grp;
    const MACSIO_IFaceHandle_t *ioiface;
    double t0,t1;
    int i, argi, exercise_scr = 0;
    int size = 1, rank = 0;
    double dumpTime = 0;
    char outfName[64];
    FILE *outf;

    /* quick pre-scan for scr cl flag */
    for (i = 0; i < argc && !exercise_scr; i++)
        exercise_scr = !strcmp("exercise_scr", argv[i]);

    main_grp = MACSIO_TIMING_GroupMask("MACSIO main()");
    main_tid = MT_StartTimer("main", main_grp, MACSIO_TIMING_ITER_AUTO);

#warning SHOULD WE BE USING MPI-3 API
#ifdef HAVE_MPI
    MPI_Init(&argc, &argv);
#ifdef HAVE_SCR
#warning SANITY CHECK WITH MIFFPP
    if (exercise_scr)
        SCR_Init();
#endif
    MPI_Comm_dup(MPI_COMM_WORLD, &MACSIO_MAIN_Comm);
    MPI_Errhandler_set(MACSIO_MAIN_Comm, MPI_ERRORS_RETURN);
    MPI_Comm_size(MACSIO_MAIN_Comm, &MACSIO_MAIN_Size);
    MPI_Comm_rank(MACSIO_MAIN_Comm, &MACSIO_MAIN_Rank);
    mpi_errno = MPI_SUCCESS;
#endif

#warning SET DEFAULT VALUES FOR CLARGS

    /* Process the command line and put the results in the problem */
    clargs_obj = ProcessCommandLine(argc, argv, &argi);
    json_object_object_add(main_obj, "clargs", clargs_obj);

    errno = 0;
    MACSIO_LOG_DebugLevel = JsonGetInt(clargs_obj, "debug_level");
    MACSIO_LOG_MainLog = MACSIO_LOG_LogInit(MACSIO_MAIN_Comm, "macsio-log.log", 128, 64);
    MACSIO_LOG_StdErr = MACSIO_LOG_LogInit(MACSIO_MAIN_Comm, 0, 0, 0);

    /* Setup parallel information */
    json_object_object_add(parallel_obj, "mpi_size", json_object_new_int(MACSIO_MAIN_Size));
    json_object_object_add(parallel_obj, "mpi_rank", json_object_new_int(MACSIO_MAIN_Rank));
    json_object_object_add(main_obj, "parallel", parallel_obj);

#warning SHOULD WE INCLUDE TOP-LEVEL INFO ON VAR NAMES AND WHETHER THEYRE RESTRICTED
#warning CREATE AN IO CONTEXT OBJECT
    /* Acquire an I/O context handle from the plugin */

    /* Sanity check args */

    /* Generate a static problem object to dump on each dump */
    problem_obj = MACSIO_GenerateStaticDumpObject(main_obj,0);
#warning MAKE JSON OBJECT KEY CASE CONSISTENT
    json_object_object_add(main_obj, "problem", problem_obj);

#warning ADD JSON PRINTING OPTIONS: sort extarrs at end, dont dump large data, html output, dump large data at end
    /* Just here for debugging for the moment */
#if 1
    if (MACSIO_MAIN_Size <= 64)
    {
        snprintf(outfName, sizeof(outfName), "main_obj_%03d.json", MACSIO_MAIN_Rank);
        outf = fopen(outfName, "w");
        fprintf(outf, "\"%s\"\n", json_object_to_json_string_ext(main_obj, JSON_C_TO_STRING_PRETTY));
        fclose(outf);
    }
#endif

#warning WERE NOT GENERATING OR WRITING ANY METADATA STUFF

    dumpTime = 0.0;
    for (int dumpNum = 0; dumpNum < json_object_path_get_int(main_obj, "clargs/num_dumps"); dumpNum++)
    {
        int scr_need_checkpoint_flag = 1;
        MACSIO_TIMING_TimerId_t heavy_dump_tid;

#ifdef HAVE_SCR
        if (exercise_scr)
            SCR_Need_checkpoint(&scr_need_checkpoint_flag);
#endif

        /* Use 'Fill' or 'Load' as name for read operation */
#warning MOVE PLUGINS TO SEPARATE SRC DIR
        const MACSIO_IFaceHandle_t *iface = MACSIO_GetInterfaceByName(
            json_object_path_get_string(main_obj, "clargs/interface"));

        /* log dump start */

        /* Start dump timer */
        heavy_dump_tid = MT_StartTimer("heavy dump", main_grp, dumpNum);

        if (scr_need_checkpoint_flag)
        {
            int scr_valid = 0;

#ifdef HAVE_SCR
            if (exercise_scr)
                SCR_Start_checkpoint();
#endif

            /* do the dump */
            (*(iface->dumpFunc))(argi, argc, argv, main_obj, dumpNum, dumpTime);

#ifdef HAVE_SCR
            if (exercise_scr)
                SCR_Complete_checkpoint(scr_valid);
#endif

        }

        /* stop timer */
        MT_StopTimer(heavy_dump_tid);

        /* log dump completion */
    }

    /* stop total timer */
    MT_StopTimer(main_tid);

    MACSIO_TIMING_ReduceTimers(MACSIO_MAIN_Comm, 0);

    if (!rank)
    {
        char **timer_strs;
        int i, ntimers, maxlen;

        MACSIO_TIMING_DumpReducedTimersToStrings(MACSIO_TIMING_ALL_GROUPS, &timer_strs, &ntimers, &maxlen);
        for (i = 0; i < ntimers; i++)
        {
            /* For now, just log to stderr */
#warning NEED A LOG FILE FOR SPECIFIC SET OF PROCESSORS, OR JUST ONE
            MACSIO_LOG_MSGL(MACSIO_LOG_StdErr, Dbg1, (timer_strs[i]));
            free(timer_strs[i]);
        }
        free(timer_strs);
    }

    MACSIO_TIMING_ClearTimers(MACSIO_TIMING_ALL_GROUPS);

#warning ATEXIT THESE
    MACSIO_LOG_LogFinalize(MACSIO_LOG_StdErr);
    MACSIO_LOG_LogFinalize(MACSIO_LOG_MainLog);

#ifdef HAVE_SCR
    if (exercise_scr)
        SCR_Finalize();
#endif
#ifdef HAVE_MPI
    MPI_Finalize();
#endif

    return (0);
}
