/*
Copyright (c) 2015, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.
Written by Mark C. Miller

LLNL-CODE-676051. All rights reserved.

This file is part of MACSio

Please also read the LICENSE file at the top of the source code directory or
folder hierarchy.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License (as published by the Free Software
Foundation) version 2, dated June 1991.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <json-cwx/json.h>

#include <macsio_data.h>
#include <macsio_utils.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/*!
\brief Support functions for Perlin noise
@{
*/
static double fade(double t) { return t * t * t * (t * (t * 6 - 15) + 10); }
static double lerp(double t, double a, double b) { return a + t * (b - a); }
static double grad(int hash, double x, double y, double z) {
    int h = hash & 15;
    double u = h<8 ? x : y;
    double v = h<4 ? y : h==12||h==14 ? x : z;
    return ((h&1) == 0 ? u : -u) + ((h&2) == 0 ? v : -v);
}
/*@}*/

/*!
\brief Ken Perlin's Improved Noise

Copyright 2002, Ken Perlin.

Modified by Mark Miller for C and for arbitrary sized spatial domains
*/
static double noise(
    double _x, /**< x spatial coordinate */
    double _y, /**< y spatial coordinate */
    double _z, /**< z spatial coordinate */
    double const *bounds /**< total spatial bounds to be mapped to unit cube */
)
{
    static int p[512], permutation[256] = {151,160,137,91,90,15,
        131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
        190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
        88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
        77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
        102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
        135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
        5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
        223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
        129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
        251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
        49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
        138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180};
    static int p_initialized = 0;

    int X, Y, Z, A, AA, AB, B, BA, BB;
    double u, v, w;
    double x = 0, y = 0, z = 0;

    if (!p_initialized)
    {
        int i;
        for (i=0; i < 256 ; i++)
            p[256+i] = p[i] = permutation[i];
        p_initialized = 1;
    }

    /* Map point in bounds to point in unit cube */
    x = _x / (bounds[3] - bounds[0]);
    if (bounds[4] != bounds[1])
        y = _y / (bounds[4] - bounds[1]);
    if (bounds[5] != bounds[2])
        z = _z / (bounds[5] - bounds[2]);

    /* Unit cube that contains the point */
    X = (int)floor(x) & 255;
    Y = (int)floor(y) & 255;
    Z = (int)floor(z) & 255;

    /* Find relative X, Y, Z of point in cube */
    x -= floor(x);
    y -= floor(y);
    z -= floor(z);

    /* Compute fade curves */
    u = fade(x);
    v = fade(y);
    w = fade(z);

    /* Hash coords of 8 cube corners */
    A = p[X  ]+Y; AA = p[A]+Z; AB = p[A+1]+Z;
    B = p[X+1]+Y; BA = p[B]+Z; BB = p[B+1]+Z;

    return lerp(w, lerp(v, lerp(u, grad(p[AA  ], x  , y  , z   ),
                                   grad(p[BA  ], x-1, y  , z   )),
                           lerp(u, grad(p[AB  ], x  , y-1, z   ),
                                   grad(p[BB  ], x-1, y-1, z   ))),
                   lerp(v, lerp(u, grad(p[AA+1], x  , y  , z-1 ),
                                   grad(p[BA+1], x-1, y  , z-1 )),
                           lerp(u, grad(p[AB+1], x  , y-1, z-1 ),
                                   grad(p[BB+1], x-1, y-1, z-1 ))));
}

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

    return json_object_new_extarr(data, dtype, ndims, dims, 0);
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

json_object *
MACSIO_DATA_MakeRandomObject(int nbytes)
{
    json_object *first_attempt = make_random_object_recurse(nbytes, 0);
    /*nbytes = first_attempt->*/
}

json_object *
MACSIO_DATA_MakeRandomTable(int nbytes)
{
    int divisor = random() % 10 + 1;
    int nbytes_per_entry = random() % (nbytes / divisor) + 4;
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
    json_object_object_add(coords, "XAxisCoords", json_object_new_extarr(vals, json_extarr_type_flt64, 1, &dims[0], 0));

    if (ndims > 1)
    {
        /* build Y coordinate array */
        delta = MACSIO_UTILS_YDelta(dims, bounds);
        vals = (double *) malloc(MACSIO_UTILS_YDim(dims) * sizeof(double));
        for (i = 0; i < MACSIO_UTILS_YDim(dims); i++)
            vals[i] = MACSIO_UTILS_YMin(bounds) + i * delta;
        json_object_object_add(coords, "YAxisCoords", json_object_new_extarr(vals, json_extarr_type_flt64, 1, &dims[1], 0));
    }

    if (ndims > 2)
    {
        /* build Z coordinate array */
        delta = MACSIO_UTILS_ZDelta(dims, bounds);
        vals = (double *) malloc(MACSIO_UTILS_ZDim(dims) * sizeof(double));
        for (i = 0; i < MACSIO_UTILS_ZDim(dims); i++)
            vals[i] = MACSIO_UTILS_ZMin(bounds) + i * delta;
        json_object_object_add(coords, "ZAxisCoords", json_object_new_extarr(vals, json_extarr_type_flt64, 1, &dims[2], 0));
    }

#warning ADD GLOBAL IDS

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
    int nx = MACSIO_UTILS_XDim(dims), ny = MU_MAX(MACSIO_UTILS_YDim(dims),1), nz = MU_MAX(MACSIO_UTILS_ZDim(dims),1);
    int i, j, k;

    json_object_object_add(coords, "CoordBasis", json_object_new_string("X,Y,Z")); /* "R,Theta,Phi" */

    x = (double *) malloc(nx * ny * nz * sizeof(double));
    if (ndims > 1)
        y = (double *) malloc(nx * ny * nz * sizeof(double));
    if (ndims > 2)
        z = (double *) malloc(nx * ny * nz * sizeof(double));
#warning X IS VARYING SLOWEST HERE. SEEMS BACKWARDS FROM NORMAL
    for (k = 0; k < nz; k++)
    {
        for (j = 0; j < ny; j++)
        {
            for (i = 0; i < nx; i++)
            {
                int idx = k * ny * nx  + j * nx + i;
                       x[idx] = MACSIO_UTILS_XMin(bounds) + i * dx;
                if (y) y[idx] = MACSIO_UTILS_YMin(bounds) + j * dy;
                if (z) z[idx] = MACSIO_UTILS_ZMin(bounds) + k * dz;
            }
        }
    }
    json_object_object_add(coords, "XCoords", json_object_new_extarr(x, json_extarr_type_flt64, ndims, dims, 0));
    if (ndims > 1)
        json_object_object_add(coords, "YCoords", json_object_new_extarr(y, json_extarr_type_flt64, ndims, dims, 0));
    if (ndims > 2)
        json_object_object_add(coords, "ZCoords", json_object_new_extarr(z, json_extarr_type_flt64, ndims, dims, 0));

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

        /* For domain entity (zone i), here are the nodal offsets in
           linear address space, left node followed by right node */
        json_object_array_add(topo_template, json_object_new_int(MU_SeqIdx1(0))); /* i+0 */
        json_object_array_add(topo_template, json_object_new_int(MU_SeqIdx1(1))); /* i+1 */

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
    /* these are #ZONES (not #NODES) in each dimension */
    int nx = MACSIO_UTILS_XDim(dims)-1;
    int ny = MU_MAX(MACSIO_UTILS_YDim(dims)-1,1);
    int nz = MU_MAX(MACSIO_UTILS_ZDim(dims)-1,1);
    int i,j,k,n=0;
    int ncells = nx * ny * nz;
    int cellsize = 1 << ndims;
    int *nodelist = (int *) malloc(ncells * cellsize * sizeof(int));
    int nl_dims[2] = {ncells, cellsize};
    json_object *topology = json_object_new_object();

    json_object_object_add(topology, "Type", json_object_new_string("Explicit"));

    /* Specifies the topological dimensionality of the entities in the
       domain and range of the map (nodelist). The domain is 'over all mesh zones',
       so the domain entities are the mesh zones and they have topological dimension
       equal to ndims. Each zone references all its nodes which are zero-dimensional
       entities. So, the range dimension of the map is zero. */
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
        for (j = 0; j < ny; j++)
        {
            for (i = 0; i < nx; i++)
            {
                nodelist[n++] = MU_SeqIdx2(i+0,j+0,nx+1);
                nodelist[n++] = MU_SeqIdx2(i+1,j+0,nx+1);
                nodelist[n++] = MU_SeqIdx2(i+1,j+1,nx+1);
                nodelist[n++] = MU_SeqIdx2(i+0,j+1,nx+1);
            }
        }
        json_object_object_add(topology, "ElemType", json_object_new_string("Quad4"));
    }
    else if (ndims == 3)
    {
        for (k = 0; k < nz; k++)
        {
            for (j = 0; j < ny; j++)
            {
                for (i = 0; i < nx; i++)
                {
                    nodelist[n++] = MU_SeqIdx3(i+0,j+0,k+0,nx+1,ny+1);
                    nodelist[n++] = MU_SeqIdx3(i+1,j+0,k+0,nx+1,ny+1);
                    nodelist[n++] = MU_SeqIdx3(i+1,j+1,k+0,nx+1,ny+1);
                    nodelist[n++] = MU_SeqIdx3(i+0,j+1,k+0,nx+1,ny+1);
                    nodelist[n++] = MU_SeqIdx3(i+0,j+0,k+1,nx+1,ny+1);
                    nodelist[n++] = MU_SeqIdx3(i+1,j+0,k+1,nx+1,ny+1);
                    nodelist[n++] = MU_SeqIdx3(i+1,j+1,k+1,nx+1,ny+1);
                    nodelist[n++] = MU_SeqIdx3(i+0,j+1,k+1,nx+1,ny+1);
                }
            }
        }
        json_object_object_add(topology, "ElemType", json_object_new_string("Hex8"));
    }
    json_object_object_add(topology, "ElemSize", json_object_new_int(cellsize));
    json_object_object_add(topology, "Nodelist", json_object_new_extarr(nodelist, json_extarr_type_int32, 2, nl_dims, 0));

    return topology;
}

static json_object *
make_arb_mesh_topology(int ndims, int const *dims)
{
    /* these are #ZONES (not #NODES) in each dimension */
    int i,j,k,n,f,z;
    int nx =        MACSIO_UTILS_XDim(dims)-1;
    int ny = MU_MAX(MACSIO_UTILS_YDim(dims)-1,1);
    int nz = MU_MAX(MACSIO_UTILS_ZDim(dims)-1,1);
    int ncells = nx * ny * nz;
    int cellsize = 2 * ndims;
    int *facelist = (int *) malloc(ncells * cellsize * sizeof(int));
    int *facecnts = (int *) malloc(ncells * sizeof(int));
    int fl_dims[2] = {ncells, cellsize};

    /* Here, we represent the facelist as a union of 3 logical arrays.
       The x-perpendicular faces, followed by the y-perp. faces
       followed by the z-perp. faces. */
    int xfaces_offset, yfaces_offset, zfaces_offset;
    int facesize = 1 << (ndims-1);
    int nxfaces = (nx+1) *     ny * nz;
    int nyfaces =     nx * (ny+1) * nz;
    int nzfaces =     nx *     ny * (nz+1);
    int nfaces = nxfaces + (ndims>1?nyfaces:0) + (ndims>2?nzfaces:0);
    int *nodelist = (int *) malloc(nfaces * facesize * sizeof(int));
    int *nodecnts = (int *) malloc(nfaces * sizeof(int));
    int nl_dims[2] = {nfaces, facesize};

    json_object *topology = json_object_new_object();

#warning WE NEED TO DOCUMENT THE MACSIO DATA MODEL
#warning WE REALLY HAVE MULTIPLE MAPS HERE ONE FROM ZONES TO FACES AND ONE FROM FACES TO NODES
#warning EACH MAP HAS A DOMAIN AND RANGE TYPE
#warning WE COULD EVEN DO FULL DIMENSIONAL CASCADE TOO
    json_object_object_add(topology, "Type", json_object_new_string("Explicit"));
    json_object_object_add(topology, "DomainDim", json_object_new_int(ndims));
    json_object_object_add(topology, "RangeDim", json_object_new_int(0)); /* node refs */
    json_object_object_add(topology, "ElemType", json_object_new_string("Arbitrary"));

    n = f = z = 0;
    if (ndims == 1)
    {
        /* same as ucdzoo case but no elem type */
        for (i = 0; i < nx; i++) 
        {
            nodelist[n++] = MU_SeqIdx1(i+0);
            nodelist[n++] = MU_SeqIdx1(i+1);
        }
        free(facelist);
    }
    else if (ndims == 2) /* here a 'face' is really an edge */
    {

        /* x-perp faces go in first */
        xfaces_offset = 0;
        for (j = 0; j < ny; j++)
        {
            for (i = 0; i < nx+1; i++)
            {
                nodelist[n++] = MU_SeqIdx2(i+0,j+0,nx+1);
                nodelist[n++] = MU_SeqIdx2(i+0,j+1,nx+1);
                nodecnts[f++] = 2;
            }
        }

        /* Now, y-perp faces */
        yfaces_offset = f;
        for (j = 0; j < ny+1; j++)
        {
            for (i = 0; i < nx; i++)
            {
                nodelist[n++] = MU_SeqIdx2(i+0,j+0,nx+1);
                nodelist[n++] = MU_SeqIdx2(i+1,j+0,nx+1);
                nodecnts[f++] = 2;
            }
        }

        /* Now add the zones as reference to 4 edges */
        f = 0;
        for (j = 0; j < ny; j++)
        {
            for (i = 0; i < nx; i++)
            {
                    /* x-perp faces */
                    facelist[z++] = xfaces_offset + MU_SeqIdx2(i+0,j+0,nx+1);
                    facelist[z++] = xfaces_offset + MU_SeqIdx2(i+1,j+0,nx+1);

                    /* y-perp faces */
                    facelist[z++] = yfaces_offset + MU_SeqIdx2(i+0,j+0,nx);
                    facelist[z++] = yfaces_offset + MU_SeqIdx2(i+0,j+1,nx);

                    facecnts[f++] = 4;
            }
        }
    }
    else if (ndims == 3)
    {
        /* x-perp faces go in first */
        xfaces_offset = 0;
        for (k = 0; k < nz; k++)
        {
            for (j = 0; j < ny; j++)
            {
                for (i = 0; i < nx+1; i++)
                {
                    nodelist[n++] = MU_SeqIdx3(i+0,j+0,k+0,nx+1,ny+1);
                    nodelist[n++] = MU_SeqIdx3(i+0,j+1,k+0,nx+1,ny+1);
                    nodelist[n++] = MU_SeqIdx3(i+0,j+1,k+1,nx+1,ny+1);
                    nodelist[n++] = MU_SeqIdx3(i+0,j+0,k+1,nx+1,ny+1);
                    nodecnts[f++] = 4;
                }
            }
        }

        /* Now, y-perp faces */
        yfaces_offset = f;
        for (k = 0; k < nz; k++)
        {
            for (j = 0; j < ny+1; j++)
            {
                for (i = 0; i < nx; i++)
                {
                    nodelist[n++] = MU_SeqIdx3(i+0,j+0,k+0,nx+1,ny+1);
                    nodelist[n++] = MU_SeqIdx3(i+1,j+0,k+0,nx+1,ny+1);
                    nodelist[n++] = MU_SeqIdx3(i+1,j+0,k+1,nx+1,ny+1);
                    nodelist[n++] = MU_SeqIdx3(i+0,j+0,k+1,nx+1,ny+1);
                    nodecnts[f++] = 4;
                }
            }
        }

        /* Now, z-perp faces */
        zfaces_offset = f;
        for (k = 0; k < nz+1; k++)
        {
            for (j = 0; j < ny; j++)
            {
                for (i = 0; i < nx; i++)
                {
                    nodelist[n++] = MU_SeqIdx3(i+0,j+0,k+0,nx+1,ny+1);
                    nodelist[n++] = MU_SeqIdx3(i+1,j+0,k+0,nx+1,ny+1);
                    nodelist[n++] = MU_SeqIdx3(i+1,j+1,k+0,nx+1,ny+1);
                    nodelist[n++] = MU_SeqIdx3(i+0,j+1,k+0,nx+1,ny+1);
                    nodecnts[f++] = 4;
                }
            }
        }

        /* Now add the zones as reference to 6 faces */
        f = 0;
        for (k = 0; k < nz; k++)
        {
            for (j = 0; j < ny; j++)
            {
                for (i = 0; i < nx; i++)
                {
                    /* x-perp faces */
                    facelist[f++] = ~(xfaces_offset + MU_SeqIdx3(i+0,j+0,k+0,nx+1,ny));
                    facelist[f++] =  (xfaces_offset + MU_SeqIdx3(i+1,j+0,k+0,nx+1,ny));

                    /* y-perp faces */
                    facelist[f++] = ~(yfaces_offset + MU_SeqIdx3(i+0,j+0,k+0,nx,ny+1));
                    facelist[f++] =   yfaces_offset + MU_SeqIdx3(i+0,j+1,k+0,nx,ny+1);

                    /* z-perp faces */
                    facelist[f++] =   zfaces_offset + MU_SeqIdx3(i+0,j+0,k+0,nx,ny);
                    facelist[f++] = ~(zfaces_offset + MU_SeqIdx3(i+0,j+0,k+1,nx,ny));

                    facecnts[z++] = 6;
                }
            }
        }
    }

    json_object_object_add(topology, "Nodelist", json_object_new_extarr(nodelist, json_extarr_type_int32, 2, nl_dims, 0));
    json_object_object_add(topology, "NodeCounts", json_object_new_extarr(nodecnts, json_extarr_type_int32, 1, &nfaces, 0));
    if (ndims > 1)
    {
        json_object_object_add(topology, "Facelist", json_object_new_extarr(facelist, json_extarr_type_int32, 2, fl_dims, 0));
        json_object_object_add(topology, "FaceCounts", json_object_new_extarr(facecnts, json_extarr_type_int32, 1, &ncells, 0));
    }

    return topology;
}

#warning WE SHOULD ENABLE ABILITY TO CHANGE TOPOLOGY WITH TIME

#warning REPLACE STRINGS FOR CENTERING AND DTYPE WITH ENUMS
#warning WE NEED TO GENERALIZE THIS VAR METHOD TO ALLOW FOR NON-RECT NODE/ZONE CONFIGURATIONS
#warning SUPPORT FACE AND EDGE CENTERINGS TOO
static json_object *
make_scalar_var(int ndims, int const *dims, double const *bounds,
    char const *centering, char const *dtype, char const *kind)
{
    json_object *var_obj = json_object_new_object();
    int i,j,k,n;
    int dims2[3] = {1,1,1};
    double dims_diameter2 = 1;
    int minus_one = strcmp(centering, "zone")?0:-1;
    json_object *data_obj;
    double *valdp;
    int    *valip;

    for (i = 0; i < ndims; i++)
    { 
        dims2[i] = dims[i] + minus_one;
        dims_diameter2 += dims[i]*dims[i];
    }

#warning NEED EXPLICIT NAME FOR VARIABLE
    json_object_object_add(var_obj, "name", json_object_new_string(kind));
    json_object_object_add(var_obj, "centering", json_object_new_string(centering));
    if (!strcmp(dtype, "double"))
        data_obj = json_object_new_extarr_alloc(json_extarr_type_flt64, ndims, dims2, 0);
    else if (!strcmp(dtype, "int"))
        data_obj = json_object_new_extarr_alloc(json_extarr_type_int32, ndims, dims2, 0);
    json_object_object_add(var_obj, "data", data_obj);
    valdp = (double *) json_object_extarr_data(data_obj);
    valip = (int *) json_object_extarr_data(data_obj);

    n = 0;
#warning PASS RANK OR RANDOM SEED IN HERE TO ENSURE DIFF PROCESSORS HAVE DIFF RANDOM DATA
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
                else if (!strcmp(kind, "noise"))
                {
                    double x = bounds[0] + i * MACSIO_UTILS_XDelta(dims, bounds);
                    double y = bounds[1] + j * MACSIO_UTILS_YDelta(dims, bounds);
                    double z = bounds[2] + k * MACSIO_UTILS_ZDelta(dims, bounds);
                    valdp[n++] = noise(x,y,z,bounds);
                }
                else if (!strcmp(kind, "noise_sum"))
                {
#warning SHOULD USE GLOBAL DIMS DIAMETER HERE
                    int q, nlevels = (int) log2(sqrt(dims_diameter2))+1;
                    double x = bounds[0] + i * MACSIO_UTILS_XDelta(dims, bounds);
                    double y = bounds[1] + j * MACSIO_UTILS_YDelta(dims, bounds);
                    double z = bounds[2] + k * MACSIO_UTILS_ZDelta(dims, bounds);
                    double mult = 1;
                    valdp[n++] = 0;
                    for (q = 0; q < nlevels; q++)
                    {
                        valdp[n-1] += 1/mult * fabs(noise(mult*x,mult*y,mult*z,bounds));
                        mult *= 2;
                    }
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
make_mesh_vars(int ndims, int const *dims, double const *bounds, int nvars)
{
    json_object *vars_array = json_object_new_array();
    char const *centering_names[2] = {"zone", "node"};
    char const *type_names[2] = {"double", "int"};
    int const centerings[] = {0,0,0,1,1,1,1,0};
    int const types[] = {0,0,0,0,0,0,0,1};
    char const *var_names[] = {"constant","random","spherical","xramp","ysin","noise","noise_sum","xlayers"};
    int i;

    /* for now, just hack and cycle through possible combinations */
    for (i = 0; i < nvars; i++)
    {
        int mod8 = i % 8;
        char const *centering = centering_names[centerings[mod8]];
        char const *type = type_names[types[mod8]];
        char const *name = var_names[mod8];
        char tmpname[32];

        if (i < 8)
            snprintf(tmpname, sizeof(tmpname), "%s", name);
        else
            snprintf(tmpname, sizeof(tmpname), "%s_%03d", name, (i-8)/8);

        json_object_array_add(vars_array, make_scalar_var(ndims, dims, bounds, centering, type, tmpname));
    }
    return vars_array;
}

#warning UNIFY PART CHUNK TERMINOLOGY THEY ARE THE SAME
#warning SHOULD NAME CHUNK/PART NUMBER HERE TO INDICATE IT IS A GLOBAL NUMBER
static json_object *make_uniform_mesh_chunk(int chunkId, int ndims, int const *dims, double const *bounds, int nvars)
{
    json_object *chunk_obj = json_object_new_object();
    json_object *mesh_obj = json_object_new_object();
    json_object_object_add(mesh_obj, "MeshType", json_object_new_string("uniform"));
    json_object_object_add(mesh_obj, "ChunkID", json_object_new_int(chunkId));
    json_object_object_add(mesh_obj, "GeomDim", json_object_new_int(ndims));
    json_object_object_add(mesh_obj, "TopoDim", json_object_new_int(ndims));
    json_object_object_add(mesh_obj, "LogDims", MACSIO_UTILS_MakeDimsJsonArray(ndims, dims));
    json_object_object_add(mesh_obj, "Bounds", MACSIO_UTILS_MakeBoundsJsonArray(bounds));
    json_object_object_add(mesh_obj, "Coords", make_uniform_mesh_coords(ndims, dims, bounds));
    json_object_object_add(mesh_obj, "Topology", make_uniform_mesh_topology(ndims, dims));
    json_object_object_add(chunk_obj, "Mesh", mesh_obj);
    json_object_object_add(chunk_obj, "Vars", make_mesh_vars(ndims, dims, bounds, nvars));
    return chunk_obj;
}

#warning ADD CALLS TO VARGEN FOR OTHER MESH TYPES
static json_object *make_rect_mesh_chunk(int chunkId, int ndims, int const *dims, double const *bounds, int nvars)
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
    json_object_object_add(chunk_obj, "Vars", make_mesh_vars(ndims, dims, bounds, nvars));
    return chunk_obj;
}

static json_object *make_curv_mesh_chunk(int chunkId, int ndims, int const *dims, double const *bounds, int nvars)
{
    json_object *chunk_obj = json_object_new_object();
    json_object *mesh_obj = json_object_new_object();
    json_object_object_add(mesh_obj, "MeshType", json_object_new_string("curvilinear"));
    json_object_object_add(mesh_obj, "ChunkID", json_object_new_int(chunkId));
    json_object_object_add(mesh_obj, "GeomDim", json_object_new_int(ndims));
    json_object_object_add(mesh_obj, "TopoDim", json_object_new_int(ndims));
    json_object_object_add(mesh_obj, "LogDims", MACSIO_UTILS_MakeDimsJsonArray(ndims, dims));
    json_object_object_add(mesh_obj, "Bounds", MACSIO_UTILS_MakeBoundsJsonArray(bounds));
    json_object_object_add(mesh_obj, "Coords", make_curv_mesh_coords(ndims, dims, bounds));
    json_object_object_add(mesh_obj, "Topology", make_curv_mesh_topology(ndims, dims));
    json_object_object_add(chunk_obj, "Mesh", mesh_obj);
#warning ADD NVARS AND VARMAPS ARGS HERE
    json_object_object_add(chunk_obj, "Vars", make_mesh_vars(ndims, dims, bounds, nvars));
    return chunk_obj;
}

static json_object *make_ucdzoo_mesh_chunk(int chunkId, int ndims, int const *dims, double const *bounds, int nvars)
{
    json_object *chunk_obj = json_object_new_object();
    json_object *mesh_obj = json_object_new_object();
    json_object_object_add(mesh_obj, "MeshType", json_object_new_string("ucdzoo"));
    json_object_object_add(mesh_obj, "ChunkID", json_object_new_int(chunkId));
    json_object_object_add(mesh_obj, "GeomDim", json_object_new_int(ndims));
    json_object_object_add(mesh_obj, "TopoDim", json_object_new_int(ndims));
    json_object_object_add(mesh_obj, "LogDims", MACSIO_UTILS_MakeDimsJsonArray(ndims, dims));
    json_object_object_add(mesh_obj, "Bounds", MACSIO_UTILS_MakeBoundsJsonArray(bounds));
    json_object_object_add(mesh_obj, "Coords", make_ucdzoo_mesh_coords(ndims, dims, bounds));
    json_object_object_add(mesh_obj, "Topology", make_ucdzoo_mesh_topology(ndims, dims));
    json_object_object_add(chunk_obj, "Mesh", mesh_obj);
    json_object_object_add(chunk_obj, "Vars", make_mesh_vars(ndims, dims, bounds, nvars));
    return chunk_obj;
}

static json_object *make_arb_mesh_chunk(int chunkId, int ndims, int const *dims, double const *bounds, int nvars)
{
    json_object *chunk_obj = json_object_new_object();
    json_object *mesh_obj = json_object_new_object();
    json_object_object_add(mesh_obj, "MeshType", json_object_new_string("arbitrary"));
    json_object_object_add(mesh_obj, "ChunkID", json_object_new_int(chunkId));
    json_object_object_add(mesh_obj, "GeomDim", json_object_new_int(ndims));
    json_object_object_add(mesh_obj, "TopoDim", json_object_new_int(ndims));
    json_object_object_add(mesh_obj, "LogDims", MACSIO_UTILS_MakeDimsJsonArray(ndims, dims));
    json_object_object_add(mesh_obj, "Bounds", MACSIO_UTILS_MakeBoundsJsonArray(bounds));
    json_object_object_add(mesh_obj, "Coords", make_arb_mesh_coords(ndims, dims, bounds));
    json_object_object_add(mesh_obj, "Topology", make_arb_mesh_topology(ndims, dims));
    json_object_object_add(chunk_obj, "Mesh", mesh_obj);
    json_object_object_add(chunk_obj, "Vars", make_mesh_vars(ndims, dims, bounds, nvars));
    return chunk_obj;
}

/* dims are # nodes in x, y and z,
   bounds are xmin,ymin,zmin,xmax,ymax,zmax */
static json_object *
make_mesh_chunk(int chunkId, int ndims, int const *dims, double const *bounds, char const *type, int nvars)
{
         if (!strncasecmp(type, "uniform", sizeof("uniform")))
        return make_uniform_mesh_chunk(chunkId, ndims, dims, bounds, nvars);
    else if (!strncasecmp(type, "rectilinear", sizeof("rectilinear")))
        return make_rect_mesh_chunk(chunkId, ndims, dims, bounds, nvars);
    else if (!strncasecmp(type, "curvilinear", sizeof("curvilinear")))
        return make_curv_mesh_chunk(chunkId, ndims, dims, bounds, nvars);
    else if (!strncasecmp(type, "unstructured", sizeof("unstructured")))
        return make_ucdzoo_mesh_chunk(chunkId, ndims, dims, bounds, nvars);
    else if (!strncasecmp(type, "arbitrary", sizeof("arbitrary")))
        return make_arb_mesh_chunk(chunkId, ndims, dims, bounds, nvars);
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

#warning ADD ABILITY TO VARY MESH TOPOLOGY WITH TIME AND ADD KEY TO INDICATE IF ITS BEEN CHANGED
#warning GET FUNTION NAMING CONSISTENT THROUGHOUT SOURCE FILES
#warning MAYBE PASS IN SEED HERE OR ADD TO MAIN_OBJ
#warning COULD IMPROVE DESIGN A BIT BY SEPARATING ALGORITHM FOR GEN WITH A CALLBACK
/* Just a very simple spatial partitioning. We use the same exact algorithm
   to determine which rank owns a chunk. So, we overload this method and
   for that purpose as well even though in that case, it doesn generate
   anything. */
json_object *
MACSIO_DATA_GenerateTimeZeroDumpObject(json_object *main_obj, int *rank_owning_chunkId)
{
#warning FIX LEAK OF OBJECTS FOR QUERY CASE
    json_object *mesh_obj = rank_owning_chunkId?0:json_object_new_object();
    json_object *global_obj = rank_owning_chunkId?0:json_object_new_object();
    json_object *part_array = rank_owning_chunkId?0:json_object_new_array();
    int size = json_object_path_get_int(main_obj, "parallel/mpi_size");
    int part_size = json_object_path_get_int(main_obj, "clargs/part_size") / sizeof(double);
    double avg_num_parts = json_object_path_get_double(main_obj, "clargs/avg_num_parts");
    int dim = json_object_path_get_int(main_obj, "clargs/part_dim");
    int vars_per_part = json_object_path_get_int(main_obj, "clargs/vars_per_part");
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
        jpart_width = 1;
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
                        json_object_path_get_string(main_obj, "clargs/part_type"), vars_per_part);
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

int MACSIO_DATA_GetRankOwningPart(json_object *main_obj, int chunkId)
{
    int tmp = chunkId;
    /* doesn't really generate anything; just goes through the motions
       necessary to compute which ranks own which parts. */
    MACSIO_DATA_GenerateTimeZeroDumpObject(main_obj, &tmp);
    return tmp;
}

int MACSIO_DATA_ValidateDataRead(json_object *main_obj)
{
#warning IMPLEMENT THE DATA READ VALIDATION 
    return 0;
}

int MACSIO_DATA_SimpleAssignKPartsToNProcs(int k, int n, int my_rank, int *my_part_cnt, int **my_part_ids)
{
    return 0;
}
