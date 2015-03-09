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

#include <ifacemap.h>
#include <macsio_main.h>
#include <macsio_params.h>
#include <util.h>

#include <json-c/json.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

#define MAX(A,B) (((A)>(B))?(A):(B))

extern char **enviornp;

static int
best_2d_factors(
    int val,
    int *x,
    int *y
)
{
    int root = (int) sqrt((double)val);
    while (root)
    {
        if (!(val % root))
        {
            *x = root;
            *y = val / root;
            return 0;
        }
        root--;
    }
    return 1;
}

static int best_3d_factors(int val, int *x, int *y, int *z)
{
    int root = (int) cbrt((double)val);
    int root2;
    int xb, yb, zb, mb=-1, mf=-1;

    /* first, walk backwards from the cube root */
    while (root)
    {
        if (!(val % root))
        {
            int val2d = val / root;
            if (!best_2d_factors(val2d, x, y))
            {
                *z = val / val2d;
                xb = *x;
                yb = *y;
                zb = *z;
                mb = xb<yb?xb:yb;
                mb = mb<zb?mb:zb;
                break;
            }
        }
        root--;
    }

    /* next, walk forwards from the cube root to the square root */
    root = (int) cbrt((double)val);
    root2 = (int) sqrt((double)val);
    while (root < root2)
    {
        if (!(val % root))
        {
            int val2d = val / root;
            if (!best_2d_factors(val2d, x, y))
            {
                *z = val / val2d;
                mf = *x<*y?*x:*y;
                mf = mf<*z?mf:*z;
                break;
            }
        }
        root++;
    }

    if (mb != -1)
    {
        if (mb > mf)
        {
            *x = xb;
            *y = yb;
            *z = zb;
        }
        return 0;
    }

    return 1;
}

static int LogicalIJKIndexToSequentialIndex(int i,int j,int k,int Ni,int Nj) { return k*Ni*Nj + j*Ni + i; }
static int  LogicalIJIndexToSequentialIndex(int i,int j,      int Ni       ) { return           j*Ni + i; }
static int   LogicalIIndexToSequentialIndex(int i                          ) { return                  i; }
#define SeqIdx3(i,j,k,Ni,Nj) LogicalIJKIndexToSequentialIndex(i,j,k,Ni,Nj)
#define SeqIdx2(i,j,Ni)       LogicalIJIndexToSequentialIndex(i,j,  Ni   )
#define SeqIdx1(i)             LogicalIIndexToSequentialIndex(i          )
static void SequentialIndexToLogicalIJKIndex(int s,int Ni,int Nj,int *i,int *j,int *k)
{
    *k = (s / (Ni*Nj));
    *j = (s % (Ni*Nj)) / Ni;
    *i = (s % (Ni*Nj)) % Ni;
}
static void SequentialIndexToLogicalIJIndex(int s,int Ni,int *i,int *j)
{
    *j = (s / Ni);
    *i = (s % Ni);
}
static void SequentialIndexToLogicalIIndex(int s,int *i)
{
    *i = s;
}
#define LogIdx3(s,Ni,Nj,a,b,c) { int q0,q1,q2; SequentialIndexToLogicalIJKIndex(s,Ni,Nj,&q0,&q1,&q2);a=q0;b=q1;c=q2; }
#define LogIdx2(s,Ni,a,b)      { int q0,q1   ;  SequentialIndexToLogicalIJIndex(s,Ni,   &q0,&q1    );a=q0;b=q1;      }
#define LogIdx1(s,a)           { int q0      ;   SequentialIndexToLogicalIIndex(s,      &q0        );a=q0;           }

static void set_dims(int *dims, int nx, int ny, int nz)
{
    dims[0] = nx;
    dims[1] = ny;
    dims[2] = nz;
}
static int x_dim(int const *dims) { return dims[0]; }
static int y_dim(int const *dims) { return dims[1]; }
static int z_dim(int const *dims) { return dims[2]; }

static void set_bounds(double *bounds,
    double xmin, double ymin, double zmin,
    double xmax, double ymax, double zmax)
{
    bounds[0] = xmin;
    bounds[1] = ymin;
    bounds[2] = zmin;
    bounds[3] = xmax;
    bounds[4] = ymax;
    bounds[5] = zmax;
}
static double x_range(double const *bounds) { return bounds[3] - bounds[0]; }
static double y_range(double const *bounds) { return bounds[4] - bounds[1]; }
static double z_range(double const *bounds) { return bounds[5] - bounds[2]; }
static double x_min(double const *bounds) { return bounds[0]; }
static double y_min(double const *bounds) { return bounds[1]; }
static double z_min(double const *bounds) { return bounds[2]; }
static double x_max(double const *bounds) { return bounds[3]; }
static double y_max(double const *bounds) { return bounds[4]; }
static double z_max(double const *bounds) { return bounds[5]; }
static double x_delta(int const *dims, double const *bounds)
{
    if (x_dim(dims) < 2) return -1;
    return x_range(bounds) / (x_dim(dims) - 1);
}
static double y_delta(int const *dims, double const *bounds)
{
    if (y_dim(dims) < 2) return -1;
    return y_range(bounds) / (y_dim(dims) - 1);
}
static double z_delta(int const *dims, double const *bounds)
{
    if (z_dim(dims) < 2) return -1;
    return z_range(bounds) / (z_dim(dims) - 1);
}

static json_object *
make_dims_array(int ndims, const int *dims)
{
    json_object *dims_array = json_object_new_array();
    json_object_array_add(dims_array, json_object_new_int(dims[0]));
    if (ndims > 1)
        json_object_array_add(dims_array, json_object_new_int(dims[1]));
    if (ndims > 2)
        json_object_array_add(dims_array, json_object_new_int(dims[2]));
    return dims_array;
}

#warning WOULD BE MORE CONSISTENT TO STORE AS MIN/MAX PAIRS
static json_object *
make_bounds_array(double const * bounds)
{
    json_object *bounds_array = json_object_new_array();
    json_object_array_add(bounds_array, json_object_new_double(bounds[0]));
    json_object_array_add(bounds_array, json_object_new_double(bounds[1]));
    json_object_array_add(bounds_array, json_object_new_double(bounds[2]));
    json_object_array_add(bounds_array, json_object_new_double(bounds[3]));
    json_object_array_add(bounds_array, json_object_new_double(bounds[4]));
    json_object_array_add(bounds_array, json_object_new_double(bounds[5]));
    return bounds_array;
}

#warning NEED TO REPLACE STRINGS WITH KEYS FOR MESH PARAMETERS
static json_object *
make_uniform_mesh_coords(int ndims, int const *dims, double const *bounds)
{
    json_object *coords = json_object_new_object();

    json_object_object_add(coords, "CoordBasis", json_object_new_string("X,Y,Z"));
    json_object_object_add(coords, "OriginX", json_object_new_double(x_min(bounds)));
    json_object_object_add(coords, "OriginY", json_object_new_double(y_min(bounds)));
    json_object_object_add(coords, "OriginZ", json_object_new_double(z_min(bounds)));
    json_object_object_add(coords, "DeltaX", json_object_new_double(x_delta(dims, bounds)));
    json_object_object_add(coords, "DeltaY", json_object_new_double(y_delta(dims, bounds)));
    json_object_object_add(coords, "DeltaZ", json_object_new_double(z_delta(dims, bounds)));
    json_object_object_add(coords, "NumX", json_object_new_int(x_dim(dims)));
    json_object_object_add(coords, "NumY", json_object_new_int(y_dim(dims)));
    json_object_object_add(coords, "NumZ", json_object_new_int(z_dim(dims)));

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
    delta = x_delta(dims, bounds);
    vals = (double *) malloc(x_dim(dims) * sizeof(double));
    for (i = 0; i < x_dim(dims); i++)
        vals[i] = x_min(bounds) + i * delta;
    json_object_object_add(coords, "XAxisCoords", json_object_new_extarr(vals, json_extarr_type_flt64, 1, &dims[0]));

    if (ndims > 1)
    {
        /* build Y coordinate array */
        delta = y_delta(dims, bounds);
        vals = (double *) malloc(y_dim(dims) * sizeof(double));
        for (i = 0; i < y_dim(dims); i++)
            vals[i] = y_min(bounds) + i * delta;
        json_object_object_add(coords, "YAxisCoords", json_object_new_extarr(vals, json_extarr_type_flt64, 1, &dims[1]));
    }

    if (ndims > 2)
    {
        /* build Z coordinate array */
        delta = z_delta(dims, bounds);
        vals = (double *) malloc(z_dim(dims) * sizeof(double));
        for (i = 0; i < z_dim(dims); i++)
            vals[i] = z_min(bounds) + i * delta;
        json_object_object_add(coords, "ZAxisCoords", json_object_new_extarr(vals, json_extarr_type_flt64, 1, &dims[2]));
    }

    return coords;
}

static json_object *
make_curv_mesh_coords(int ndims, int const *dims, double const *bounds)
{
    json_object *coords = json_object_new_object();
    double *x = 0, *y = 0, *z = 0;
    double dx = x_delta(dims, bounds), dy = y_delta(dims, bounds), dz = z_delta(dims, bounds);
    int nx = x_dim(dims), ny = MAX(y_dim(dims),1), nz = MAX(z_dim(dims),1);
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
                x[idx] = x_min(bounds) + i * dx;
                if (y) y[idx] = y_min(bounds) + j * dy;
                if (z) z[idx] = z_min(bounds) + k * dz;
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
    int nx = x_dim(dims);
    int ny = y_dim(dims);
    int nz = z_dim(dims);

    json_object_object_add(topology, "Type", json_object_new_string("Templated"));
    json_object_object_add(topology, "DomainDim", json_object_new_int(ndims));
    json_object_object_add(topology, "RangeDim", json_object_new_int(0)); /* node refs */

    /* would be better for elem types to be enum or int */
    /* We can use a bonified json array here because the template is small. But,
       should we? */

    if (ndims == 1)
    {
        json_object *topo_template = json_object_new_array();

        json_object_array_add(topo_template, json_object_new_int(SeqIdx1(0)));
        json_object_array_add(topo_template, json_object_new_int(SeqIdx1(1)));

        json_object_object_add(topology, "ElemType", json_object_new_string("Beam2"));
        json_object_object_add(topology, "Template", topo_template);
    }
    else if (ndims == 2)
    {
        json_object *topo_template = json_object_new_array();

        /* For domain entity (zone i,j), here are the nodal offsets in
           linear address space, right-hand-rule starting from lower-left corner */
        json_object_array_add(topo_template, json_object_new_int(SeqIdx2(0,0,nx)));
        json_object_array_add(topo_template, json_object_new_int(SeqIdx2(1,0,nx)));
        json_object_array_add(topo_template, json_object_new_int(SeqIdx2(1,1,nx)));
        json_object_array_add(topo_template, json_object_new_int(SeqIdx2(0,1,nx)));

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
        json_object_array_add(topo_template, json_object_new_int(SeqIdx3(0,0,0,nx,ny)));
        json_object_array_add(topo_template, json_object_new_int(SeqIdx3(1,0,0,nx,ny)));
        json_object_array_add(topo_template, json_object_new_int(SeqIdx3(1,1,0,nx,ny)));
        json_object_array_add(topo_template, json_object_new_int(SeqIdx3(0,1,0,nx,ny)));
        json_object_array_add(topo_template, json_object_new_int(SeqIdx3(0,0,1,nx,ny)));
        json_object_array_add(topo_template, json_object_new_int(SeqIdx3(1,0,1,nx,ny)));
        json_object_array_add(topo_template, json_object_new_int(SeqIdx3(1,1,1,nx,ny)));
        json_object_array_add(topo_template, json_object_new_int(SeqIdx3(0,1,1,nx,ny)));

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
    int nx = x_dim(dims), ny = MAX(y_dim(dims),1), nz = MAX(z_dim(dims),1);
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
            nodelist[n++] = SeqIdx1(i+0);
            nodelist[n++] = SeqIdx1(i+1);
        }
        json_object_object_add(topology, "ElemType", json_object_new_string("Beam2"));
    }
    else if (ndims == 2)
    {
        for (i = 0; i < nx; i++)
        {
            for (j = 0; j < ny; j++)
            {
                nodelist[n++] = SeqIdx2(i+0,j+0,nx);
                nodelist[n++] = SeqIdx2(i+1,j+0,nx);
                nodelist[n++] = SeqIdx2(i+1,j+1,nx);
                nodelist[n++] = SeqIdx2(i+0,j+1,nx);
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
                    nodelist[n++] = SeqIdx3(i+0,j+0,k+0,nx,ny);
                    nodelist[n++] = SeqIdx3(i+1,j+0,k+0,nx,ny);
                    nodelist[n++] = SeqIdx3(i+1,j+1,k+0,nx,ny);
                    nodelist[n++] = SeqIdx3(i+0,j+1,k+0,nx,ny);
                    nodelist[n++] = SeqIdx3(i+0,j+0,k+1,nx,ny);
                    nodelist[n++] = SeqIdx3(i+1,j+0,k+1,nx,ny);
                    nodelist[n++] = SeqIdx3(i+1,j+1,k+1,nx,ny);
                    nodelist[n++] = SeqIdx3(i+0,j+1,k+1,nx,ny);
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
    for (i = 0; i < dims2[0]; i++)
    {
        for (j = 0; j < dims2[1]; j++)
        {
            for (k = 0; k < dims2[2]; k++)
            {
#warning PUT THESE INTO A GENERATOR FUNCTION
#warning ACCOUNT FOR HALF ZONE OFFSETS
                if (!strcmp(kind, "constant"))
                    valdp[n++] = 1.0;
                else if (!strcmp(kind, "random"))
                    valdp[n++] = (double) (random() % 1000) / 1000;
                else if (!strcmp(kind, "xramp"))
                    valdp[n++] = i * x_delta(dims, bounds);
                else if (!strcmp(kind, "spherical"))
                {
                    double x = i * x_delta(dims, bounds);
                    double y = j * y_delta(dims, bounds);
                    double z = k * z_delta(dims, bounds);
                    valdp[n++] = sqrt(x*x+y*y+z*z);
                }
                else if (!strcmp(kind, "ysin"))
                {
                    double y = j * y_delta(dims, bounds);
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
}

static json_object *
make_tensor_var(int ndims, int const *dims, double const *bounds)
{
}

static json_object *
make_subset_var(int ndims, int const *dims, double const *bounds)
{
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
    json_object_object_add(mesh_chunk, "LogDims", make_dims_array(ndims, dims));
    json_object_object_add(mesh_chunk, "Bounds", make_bounds_array(bounds));
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
    json_object_object_add(mesh_obj, "LogDims", make_dims_array(ndims, dims));
    json_object_object_add(mesh_obj, "Bounds", make_bounds_array(bounds));
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
    json_object_object_add(mesh_chunk, "LogDims", make_dims_array(ndims, dims));
    json_object_object_add(mesh_chunk, "Bounds", make_bounds_array(bounds));
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
    json_object_object_add(mesh_chunk, "LogDims", make_dims_array(ndims, dims));
    json_object_object_add(mesh_chunk, "Bounds", make_bounds_array(bounds));
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
#warning COULD IMPROVE DESIGN A BIT BY SEPARATING ALGORITHM FOR GEN WITH A CALLBACK
/* Just a very simple spatial partitioning. We use the same exact algorithm
   to determine which rank owns a chunk. So, we overload this method and
   for that purpose as well even though in that case, it doesn generate
   anything. */
static json_object *
MACSIO_GenerateStaticDumpObject(json_object *main_obj, int *rank_owning_chunkId)
{
#warning FIX READ ACCESS TO KEYS MAKE IT TYPE SAFE
#warning FIX LEAK OF OBJECTS FOR QUERY CASE
    json_object *mesh_obj = rank_owning_chunkId?0:json_object_new_object();
    json_object *global_obj = rank_owning_chunkId?0:json_object_new_object();
    json_object *part_array = rank_owning_chunkId?0:json_object_new_array();
    int size = json_object_path_get_int(main_obj, "parallel/mpi_size");
    int part_size = json_object_path_get_int(main_obj, "clargs/--part_size") / sizeof(double);
    double avg_num_parts = json_object_path_get_double(main_obj, "clargs/--avg_num_parts");
    int dim = json_object_path_get_int(main_obj, "clargs/--part_dim");
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
        best_2d_factors(total_num_parts, &nx_parts, &ny_parts);
        best_2d_factors(part_size, &nx, &ny);
        jpart_width = 1;
    }
    else if (dim == 3)
    {
        best_3d_factors(total_num_parts, &nx_parts, &ny_parts, &nz_parts);
        best_3d_factors(part_size, &nx, &ny, &nz);
        kpart_width = 1;
    }
    set_dims(part_dims, nx, ny, nz);
    set_dims(part_block_dims, nx_parts, ny_parts, nz_parts);
    set_dims(global_log_dims, nx * nx_parts, ny * ny_parts, nz * nz_parts);
    set_bounds(global_bounds, 0, 0, 0,
        nx_parts * ipart_width, ny_parts * jpart_width, nz_parts * kpart_width);
    if (!rank_owning_chunkId)
    {
        json_object_object_add(global_obj, "TotalParts", json_object_new_int(total_num_parts));
#warning NOT SURE PartsLogDims IS USEFUL IN GENERAL CASE
        json_object_object_add(global_obj, "PartsLogDims", make_dims_array(dim, part_block_dims));
        json_object_object_add(global_obj, "LogDims", make_dims_array(dim, global_log_dims));
        json_object_object_add(global_obj, "Bounds", make_bounds_array(global_bounds));
        json_object_object_add(mesh_obj, MACSIO_MESH_KEY(global), global_obj);
    }

#warning SHOULD MAIN OBJECT KNOW WHERE ALL CHUNKS ARE OR ONLY CHUNKS ON THIS RANK
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
                    set_bounds(part_bounds, (double) ipart, (double) jpart, (double) kpart,
                        (double) ipart+ipart_width, (double) jpart+jpart_width, (double) kpart+kpart_width);
                    json_object *part_obj = make_mesh_chunk(chunk, dim, part_dims, part_bounds,
                        json_object_path_get_string(main_obj, "clargs/--part_type"));
                    set_dims(global_indices, ipart, jpart, kpart);
#warning MAYBE MOVE GLOBAL LOG INDICES TO make_mesh_chunk
#warning GlogalLogIndices MAY NOT BE NEEDED
                    json_object_object_add(part_obj, "GlobalLogIndices",
                        make_dims_array(dim, global_indices));
                    set_dims(global_log_origin, ipart * nx, jpart * ny, kpart * nz);
                    json_object_object_add(part_obj, "GlobalLogOrigin",
                        make_dims_array(dim, global_log_origin));
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
    json_object_object_add(mesh_obj, MACSIO_MESH_KEY(parts), part_array);

    return mesh_obj;

}

int MACSIO_GetRankOwningPart(json_object *main_obj, int chunkId)
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
            fprintf(outFILE, "\nOptions specific to the \"%s\" I/O driver\n", iface->name);
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
    fprintf(outFILE, "List of available I/O-library drivers...\n");
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
    MACSIO_ArgvFlags_t const argFlags = {MACSIO_WARN, MACSIO_ARGV_TOJSON};
    json_object *mainJargs = 0;
    int plugin_args_start = -1;
    int cl_result;

#warning FIX PARTIAL CODE REVISION HERE
    cl_result = MACSIO_ProcessCommandLine((void**)&mainJargs, argFlags, 1, argc, argv,
        MACSIO_ARGV_DEF(interface, %s),
        MACSIO_ARGV_DEF(parallel_file_mode, %s %d),
        MACSIO_ARGV_DEF(part_size, %d),
        MACSIO_ARGV_DEF(part_dim, %d),
        MACSIO_ARGV_DEF(part_type, %s),
        MACSIO_ARGV_DEF(avg_num_parts, %f),
        MACSIO_ARGV_DEF(part_distribution, %s),
        MACSIO_ARGV_DEF(vars_per_part, %d),
        MACSIO_ARGV_DEF(num_dumps, %d),
        MACSIO_ARGV_DEF(alignment, %d),
        MACSIO_ARGV_DEF(filebase, %s),
        MACSIO_ARGV_DEF(fileext, %s),
        "--driver-args %n",
            "All arguments after this sentinel are passed to the I/O driver plugin (ignore the %n)",
    MACSIO_END_OF_ARGS);

#warning FIXME
    plugin_args_start = json_object_path_get_int(mainJargs, "argi");
    plugin_args_start = argc;

    /* if we discovered help was requested, then print each plugin's help too */
    if (cl_result == MACSIO_ARGV_HELP)
        handle_help_request_and_exit(plugin_args_start+1, argc, argv);

    if (!strcmp(json_object_path_get_string(mainJargs, MACSIO_ARGV_KEY(interface)), "list"))
        handle_list_request_and_exit();

    /* sanity check some values */
    if (!strcmp(json_object_path_get_string(mainJargs, MACSIO_ARGV_KEY(interface)), ""))
        MACSIO_ERROR(("no io-interface specified"), MACSIO_FATAL);

    if (plugin_argi)
        *plugin_argi = plugin_args_start>-1?plugin_args_start+1:argc;

    return mainJargs;
}

/* Debugging support method */
#define OUTDBLOPT(A) fprintf(f?f:stdout, "%-40s = %f\n", #A, MACSIO_GetDblOption(opts,A));
#define OUTINTOPT(A) fprintf(f?f:stdout, "%-40s = %d\n", #A, MACSIO_GetIntOption(opts,A));
#define OUTSTROPT(A) fprintf(f?f:stdout, "%-40s = \"%s\"\n", #A, MACSIO_GetStrOption(opts,A));
static void DumpOptions(MACSIO_optlist_t const *opts, FILE *f)
{
    OUTSTROPT(IOINTERFACE_NAME);
    OUTSTROPT(PARALLEL_FILE_MODE);
    OUTINTOPT(PARALLEL_FILE_COUNT);
    OUTINTOPT(PART_SIZE);
    OUTDBLOPT(AVG_NUM_PARTS);
    OUTSTROPT(PART_DISTRIBUTION);
    OUTINTOPT(VARS_PER_PART);
    OUTINTOPT(NUM_DUMPS);
    OUTINTOPT(ALIGNMENT);
    OUTSTROPT(FILENAME_SPRINTF);
    OUTINTOPT(PRINT_TIMING_DETAILS);
    OUTINTOPT(MPI_SIZE);
    OUTINTOPT(MPI_RANK);
}

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
        MACSIO_ERROR(("unable to instantiate IO interface \"%s\"",ifacename), MACSIO_FATAL);

    return retval;
}

static MACSIO_optlist_t *SetupDefaultOptions()
{
    MACSIO_optlist_t *ol = MACSIO_MakeOptlist();

    MACSIO_AddLiteralStrOptionFromHeap(ol, IOINTERFACE_NAME, "hdf5");
    MACSIO_AddLiteralStrOptionFromHeap(ol, PARALLEL_FILE_MODE, "MIF");
    MACSIO_AddIntOption(ol, PARALLEL_FILE_COUNT, 2);
    MACSIO_AddIntOption(ol, PART_SIZE, (1<<20));
    MACSIO_AddDblOption(ol, AVG_NUM_PARTS, 1.0);
    MACSIO_AddIntOption(ol, VARS_PER_PART, 50);
    MACSIO_AddLiteralStrOptionFromHeap(ol, PART_DISTRIBUTION, "ingored");
    MACSIO_AddIntOption(ol, NUM_DUMPS, 10);
    MACSIO_AddIntOption(ol, ALIGNMENT, 0);
    MACSIO_AddLiteralStrOptionFromHeap(ol, FILENAME_SPRINTF, "macsio-hdf5-%06d.h5");
    MACSIO_AddIntOption(ol, PRINT_TIMING_DETAILS, 1);
    MACSIO_AddIntOption(ol, MPI_SIZE, 1);
    MACSIO_AddIntOption(ol, MPI_RANK, 0);

    return ol;
}

int
main(int argc, char *argv[])
{
    MACSIO_FileHandle_t *fh;
    json_object *main_obj = json_object_new_object();
    json_object *parallel_obj = json_object_new_object();
    json_object *problem_obj = 0;
    json_object *clargs_obj = 0;
    MACSIO_optlist_t *opts;
    const MACSIO_IFaceHandle_t *ioiface;
    double         t0,t1;
    int            argi;
    int size = 1, rank = 0;
    double dumpTime = 0;
    char outfName[64];
    FILE *outf;

#ifdef HAVE_MPI
    MPI_Comm macsio_io_comm = MPI_COMM_WORLD;
    MPI_Init(&argc, &argv);
    MPI_Comm_dup(MPI_COMM_WORLD, &macsio_io_comm);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

    opts = SetupDefaultOptions();

#warning SET DEFAULT VALUES FOR CLARGS

    /* Process the command line and put the results in the problem */
    clargs_obj = ProcessCommandLine(argc, argv, &argi);
    json_object_object_add(main_obj, MACSIO_MAIN_KEY(clargs), clargs_obj);

    /* Setup parallel information */
    json_object_object_add(parallel_obj, MACSIO_PARALLEL_KEY(mpi_size), json_object_new_int(size));
    json_object_object_add(parallel_obj, MACSIO_PARALLEL_KEY(mpi_rank), json_object_new_int(rank));
    json_object_object_add(main_obj, MACSIO_MAIN_KEY(parallel), parallel_obj);

#warning SHOULD WE INCLUDE TOP-LEVEL INFO ON VAR NAMES AND WHETHER THEY'RE RESTRICTED
#warning CREATE AN IO CONTEXT OBJECT
    /* Acquire an I/O context handle from the plugin */

    /* Sanity check args */

    /* Generate a static problem object to dump on each dump */
    problem_obj = MACSIO_GenerateStaticDumpObject(main_obj,0);
    json_object_object_add(main_obj, MACSIO_MAIN_KEY(problem), problem_obj);

#warning ADD JSON PRINTING OPTIONS: sort extarrs at end, don't dump large data, html output, dump large data at end
    /* Just here for debugging for the moment */
#if 0
    snprintf(outfName, sizeof(outfName), "main_obj_%03d.json", rank);
    outf = fopen(outfName, "w");
    fprintf(outf, "\"%s\"\n", json_object_to_json_string_ext(main_obj, JSON_C_TO_STRING_PRETTY));
    fclose(outf);
#endif


#warning INITIALIZE LOG FILES. LOGS HANDLES SHOULD BE PASSED TO PLUGINS

#warning START TOTAL TIMER

#warning WE'RE NOT GENERATING OR WRITING ANY METADATA STUFF

    dumpTime = 0.0;
    for (int dumpNum = 0; dumpNum < json_object_path_get_int(main_obj, "clargs/--num_dumps"); dumpNum++)
    {
        /* Use 'Fill' for read operation */
        const MACSIO_IFaceHandle_t *iface = MACSIO_GetInterfaceByName(
            json_object_path_get_string(main_obj, "clargs/--interface"));

        /* log dump start */

        /* Start dump timer */

        /* do the dump */
        (*(iface->dumpFunc))(argi, argc, argv, main_obj, dumpNum, dumpTime);

        /* stop timer */

        /* log dump completion */
    }

    /* stop total timer */

#ifdef HAVE_MPI
    MPI_Finalize();
#endif

    return (0);
}
