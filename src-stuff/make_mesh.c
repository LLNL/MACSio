#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

#include <json-c/json.h>

#define MAX(A,B) (((A)>(B))?(A):(B))

#if 0
typedef MACSio_DjParCtx_t MPI_Comm;
typedef MACSio_ThParCtx_t 

typedef struct _MACSio_DomInfo_t {
    int64_t dpcglo_dom_id; /**< DPC-global domain id */
    int64_t dpc_rank;      /**< Rank of owning processor in the DPC */
} MACSio_DomNbor_t;

typedef struct _MACSio_DomDecomp_t {
    int64_t num_doms_total;           /**< Total number of domains over entire parallel context */
    MACSio_DisjParCtx_t disj_par_ctx; /**< Disjoint parallel context (DPC) */

    std::vector<MACSio_DomInfo_t>     /**< DPC-local domain information */
    int dpcloc_dom_cnt;               /**< DPC-local domain count. */
    MACSio_DomInfo_t const *

    int64_t const *dpcglo_dom_ids;    /**< Array of DBC-global domain ids */
    int64_t const *dpcloc_dom_nb_cnt; /**< Array of domain neighbor counts */
    MACSio_DomNbor_t const *dom_
    int64_t const * const *dpcglo_dom_nb_ids;    /**< Array of DBC-global domain neighbor ids */


    /* for each domain's neighbor, which MPI rank owns it (could be this one), local id in that rank */
    /* for each domain's neighbor, list of entities shared with that domain and local ids in that rank */
} MACSio_DomDecomp_t;
#endif

static int best_2d_factors(int val, int *x, int *y)
{
    int root = (int) sqrt((double)val);
    while (root)
    {
        if (!(val % root))
        {
            *x = root;
            *y = val / root;
            assert(*x * *y == val);
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
                assert(*x * *y * *z == val);
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
                assert(*x * *y * *z == val);
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

static int LogicalIJKIndexToSequentialIndex(i,j,k,Ni,Nj) { return k*Ni*Nj + j*Ni + i; }
static int  LogicalIJIndexToSequentialIndex(i,j,  Ni   ) { return           j*Ni + i; }
static int   LogicalIIndexToSequentialIndex(i          ) { return                  i; }
#define SeqIdx3(i,j,k,Ni,Nj) LogicalIJKIndexToSequentialIndex(i,j,k,Ni,Nj)
#define SeqIdx2(i,j,Ni)      LogicalIJKIndexToSequentialIndex(i,jNi)
#define SeqIdx1(i)           LogicalIKIndexToSequentialIndex(i)
static void SequentialIndexToLogicalIJKIndex(s,Ni,Nj,int*i,int*j,int*k)
{
    *k = (s / (Ni*Nj));
    *j = (s % (Ni*Nj)) / Ni;
    *i = (s % (Ni*Nh)) % Ni;
}
static void SequentialIndexToLogicalIJIndex(s,Ni,int*i,int*j)
{
    *j = (s / Ni);
    *i = (s % Ni);
}
static void SequentialIndexToLogicalIIndex(s,int*i)
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
make_dims_array(int ndims, int const *dims)
{
    json_object *dims_array = json_object_new_array();
    json_object_array_add(dims_array, json_object_new_int(ndims));
    json_object_array_add(dims_array, json_object_new_int(dims[0]));
    if (ndims > 1)
        json_object_array_add(dims_array, json_object_new_int(dims[1]));
    if (ndims > 2)
        json_object_array_add(dims_array, json_object_new_int(dims[2]));
    return dims_array;
}

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

static json_object *
make_rect_mesh_coords(int ndims, int const *dims, double const *bounds)
{
    json_object *coords = json_object_new_object();
    double *vals, delta;
    int i;

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
        json_object_object_add(coords, "YAxisCoords", json_object_new_extarr(vals, json_extarr_type_flt64, 1, &dims[0]));
    }

    if (ndims > 2)
    {
        /* build Z coordinate array */
        delta = z_delta(dims, bounds);
        vals = (double *) malloc(z_dim(dims) * sizeof(double));
        for (i = 0; i < z_dim(dims); i++)
            vals[i] = z_min(bounds) + i * delta;
        json_object_object_add(coords, "ZAxisCoords", json_object_new_extarr(vals, json_extarr_type_flt64, 1, &dims[0]));
    }

    return coords;
}

static json_object *
make_curv_mesh_coords(int ndims, int const *dims, double const *bounds)
{
    json_object *coords = json_object_new_object();
    double *x = 0, *y = 0, *z = 0;
    double dx = x_delta(bounds), dy = y_delta(bounds), dz = z_delta(bounds);
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

static json_object *
make_arb_mesh_topology(int ndims, int const *dims)
{
    return 0;
}

static json_object *make_uniform_mesh_chunk(int chunkId, int ndims, int const *dims, double const *bounds)
{
    json_object *mesh_chunk = json_object_object_new();
    json_object_object_add(mesh_chunk, "ChunkID", json_object_new_int(chunkId));
    json_object_object_add(mesh_chunk, "GeomDim", json_object_new_int(ndims));
    json_object_object_add(mesh_chunk, "TopoDim", json_object_new_int(ndims));
    json_object_object_add(mesh_chunk, "LogDims", make_dims_array(dims));
    json_object_object_add(mesh_chunk, "Bounds", make_bounds_array(bounds));
    json_object_object_add(mesh_chunk, "Coords", make_uniform_mesh_coords(ndims, dims, bounds));
    json_object_object_add(mesh_chunk, "Topology", make_uniform_mesh_topology(ndims, dims));
    return mesh_chunk;
}

static json_object *make_rect_mesh_chunk(int chunkId, int ndims, int const *dims, double const *bounds)
{
    json_object *mesh_chunk = json_object_object_new();
    json_object_object_add(mesh_chunk, "ChunkID", json_object_new_int(chunkId));
    json_object_object_add(mesh_chunk, "GeomDim", json_object_new_int(ndims));
    json_object_object_add(mesh_chunk, "TopoDim", json_object_new_int(ndims));
    json_object_object_add(mesh_chunk, "LogDims", make_dims_array(dims));
    json_object_object_add(mesh_chunk, "Bounds", make_bounds_array(bounds));
    json_object_object_add(mesh_chunk, "Coords", make_rect_mesh_coords(ndims, dims, bounds));
    json_object_object_add(mesh_chunk, "Topology", make_rect_mesh_topology(ndims, dims));
    return mesh_chunk;
}

static json_object *make_curv_mesh_chunk(int chunkId, int ndims, int const *dims, double const *bounds)
{
    json_object *mesh_chunk = json_object_object_new();
    json_object_object_add(mesh_chunk, "ChunkID", json_object_new_int(chunkId));
    json_object_object_add(mesh_chunk, "GeomDim", json_object_new_int(ndims));
    json_object_object_add(mesh_chunk, "TopoDim", json_object_new_int(ndims));
    json_object_object_add(mesh_chunk, "LogDims", make_dims_array(dims));
    json_object_object_add(mesh_chunk, "Bounds", make_bounds_array(bounds));
    json_object_object_add(mesh_chunk, "Coords", make_curv_mesh_coords(ndims, dims, bounds));
    json_object_object_add(mesh_chunk, "Topology", make_curv_mesh_topology(ndims, dims));
    return mesh_chunk;
}

static json_object *make_ucdzoo_mesh_chunk(int chunkId, int ndims, int const *dims, double const *bounds)
{
    json_object *mesh_chunk = json_object_object_new();
    json_object_object_add(mesh_chunk, "ChunkID", json_object_new_int(chunkId));
    json_object_object_add(mesh_chunk, "GeomDim", json_object_new_int(ndims));
    json_object_object_add(mesh_chunk, "TopoDim", json_object_new_int(ndims));
    json_object_object_add(mesh_chunk, "LogDims", make_dims_array(dims));
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
         if (!strncasecmp(type, "uniform", sizeof("uniform"));
        return make_uniform_mesh_chunk(chunkId, ndims, dims, bounds);
    else if (!strncasecmp(type, "rectilinear", sizeof("rectilinear"));
        return make_rect_mesh_chunk(chunkId, ndims, dims, bounds);
    else if (!strncasecmp(type, "curvilinear", sizeof("curvilinear"));
        return 0;
    else if (!strncasecmp(type, "unstructured", sizeof("unstructured"));
        return make_ucdzoo_mesh_chunk(chunkId, ndims, dims, bounds);
    else if (!strncasecmp(type, "arbitrary", sizeof("arbitrary"));
        return 0;
    return 0;
}


extern char **enviornp;

int main(int argc, char **argv)
{
    int mpi_rank = 0;
    int mpi_size = 1;
    int argi = 0;
    int req_size = 80 * 1000;
    int num_doms = -1;
    int num_doms_x, num_doms_y;
    domain_iterator_t *dom_it;
    json_object *macsio_problem_object = json_object_new_object();
#ifdef HAVE_MPI
    MPI_Comm macsio_mpi_comm = MPI_COMM_WORLD;
#endif

#ifdef HAVE_MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_dup(MPI_COMM_WORLD, &macsio_mpi_comm);
    MPI_Comm_size(macsio_mpi_comm, &mpi_size);
    MPI_Comm_rank(macsio_mpi_comm, &mpi_rank);
#endif 

    macsio_clargs = process_command_line_args(macsio_mpi_comm, argc, argv, enviornp);

#warning NAME PROGRAM VARIABLES TO DISTINGUISH BETWEEN LOCAL/GLOBAL VALUES

    /* package all CL args into a JSON array and broadcast it */
#warning NEED CL ARGS PACKAGE

    if (num_doms == -1)
        num_doms = mpi_size;

    /* Compute a suitable (ideally square) 2D decomposition. If num_doms is 
       a prime, then you get long y-strips. */
    best_2d_factors(num_doms, &num_doms_x, &num_doms_y);

    /* Get an iterator over domains on this rank */
    /* assign domains to MPI ranks (round robin for now) */
    /* create mesh boxes (1 per domain) */
    dom_it = get_iterator_for_domains_on_this_rank(mpi_size, mpi_rank,
                 num_doms_x, num_doms_y);

    macsio_problem_domains = json_object_mkdir(macsio_problem_object, "domains");

    while (dom_it->have_domains())
    {
        json_object_t *dom_json_obj;

        dom_it->get_domain_params();
  
        dom_json_obj = build_domain();

        json_object_object_add(macsio_problem_domains, dom_json_obj);
        
        dom_it->next_domain();
    }

    free_domain_iterator(dom_it);

    /* write the JSON-thingy */ 
    plugin_dump("Silo", macsio_problem_object);

#ifdef HAVE_MPI
    MPI_Finalize();
#endif

    return 0;
}
