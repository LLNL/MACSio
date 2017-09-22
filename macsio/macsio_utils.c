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

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>

#include <macsio_utils.h>

char MACSIO_UTILS_UnitsPrefixSystem[32];

/*-------------------------------------------------------------------------
  Function: bjhash 

  Purpose: Hash a variable length stream of bytes into a 32-bit value.

  Programmer: By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.

  This is the hash that Bob Jenkins described in the 1997 Dr. Dobb's
  article, http://www.burtleburtle.net/bob/hash/doobs.html

  You may use this code any way you wish, private, educational, or
  commercial.  It's free. However, do NOT use for cryptographic purposes.

  See http://burtleburtle.net/bob/hash/evahash.html
 *-------------------------------------------------------------------------*/

#define bjhash_mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

unsigned int MACSIO_UTILS_BJHash(const unsigned char *k, unsigned int length, unsigned int initval)
{
   unsigned int a,b,c,len;

   len = length;
   a = b = 0x9e3779b9;
   c = initval;

   while (len >= 12)
   {
      a += (k[0] +((unsigned int)k[1]<<8) +((unsigned int)k[2]<<16) +((unsigned int)k[3]<<24));
      b += (k[4] +((unsigned int)k[5]<<8) +((unsigned int)k[6]<<16) +((unsigned int)k[7]<<24));
      c += (k[8] +((unsigned int)k[9]<<8) +((unsigned int)k[10]<<16)+((unsigned int)k[11]<<24));
      bjhash_mix(a,b,c);
      k += 12; len -= 12;
   }

   c += length;

   switch(len)
   {
      case 11: c+=((unsigned int)k[10]<<24);
      case 10: c+=((unsigned int)k[9]<<16);
      case 9 : c+=((unsigned int)k[8]<<8);
      case 8 : b+=((unsigned int)k[7]<<24);
      case 7 : b+=((unsigned int)k[6]<<16);
      case 6 : b+=((unsigned int)k[5]<<8);
      case 5 : b+=k[4];
      case 4 : a+=((unsigned int)k[3]<<24);
      case 3 : a+=((unsigned int)k[2]<<16);
      case 2 : a+=((unsigned int)k[1]<<8);
      case 1 : a+=k[0];
   }

   bjhash_mix(a,b,c);

   return c;
}

int MACSIO_UTILS_Best2DFactors(
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

int MACSIO_UTILS_Best3DFactors(int val, int *x, int *y, int *z)
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
            if (!MACSIO_UTILS_Best2DFactors(val2d, x, y))
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
            if (!MACSIO_UTILS_Best2DFactors(val2d, x, y))
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

int MACSIO_UTILS_LogicalIJKIndexToSequentialIndex(int i,int j,int k,int Ni,int Nj) { return k*Ni*Nj + j*Ni + i; }
int MACSIO_UTILS_LogicalIJIndexToSequentialIndex (int i,int j,      int Ni       ) { return           j*Ni + i; }
int MACSIO_UTILS_LogicalIIndexToSequentialIndex  (int i                          ) { return                  i; }

void MACSIO_UTILS_SequentialIndexToLogicalIJKIndex(int s,int Ni,int Nj,int *i,int *j,int *k)
{
    *k = (s / (Ni*Nj));
    *j = (s % (Ni*Nj)) / Ni;
    *i = (s % (Ni*Nj)) % Ni;
}
void MACSIO_UTILS_SequentialIndexToLogicalIJIndex(int s,int Ni,int *i,int *j)
{
    *j = (s / Ni);
    *i = (s % Ni);
}
void MACSIO_UTILS_SequentialIndexToLogicalIIndex(int s,int *i)
{
    *i = s;
}

void MACSIO_UTILS_SetDims(int *dims, int nx, int ny, int nz)
{
    dims[0] = nx;
    dims[1] = ny;
    dims[2] = nz;
}
int MACSIO_UTILS_XDim(int const *dims) { return dims[0]; }
int MACSIO_UTILS_YDim(int const *dims) { return dims[1]; }
int MACSIO_UTILS_ZDim(int const *dims) { return dims[2]; }

void MACSIO_UTILS_SetBounds(double *bounds,
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

double MACSIO_UTILS_XRange(double const *bounds) { return bounds[3] - bounds[0]; }
double MACSIO_UTILS_YRange(double const *bounds) { return bounds[4] - bounds[1]; }
double MACSIO_UTILS_ZRange(double const *bounds) { return bounds[5] - bounds[2]; }
double MACSIO_UTILS_XMin(double const *bounds) { return bounds[0]; }
double MACSIO_UTILS_YMin(double const *bounds) { return bounds[1]; }
double MACSIO_UTILS_ZMin(double const *bounds) { return bounds[2]; }
double MACSIO_UTILS_XMax(double const *bounds) { return bounds[3]; }
double MACSIO_UTILS_YMax(double const *bounds) { return bounds[4]; }
double MACSIO_UTILS_ZMax(double const *bounds) { return bounds[5]; }
double MACSIO_UTILS_XDelta(int const *dims, double const *bounds)
{
    if (MACSIO_UTILS_XDim(dims) < 2) return -1;
    return MACSIO_UTILS_XRange(bounds) / (MACSIO_UTILS_XDim(dims) - 1);
}
double MACSIO_UTILS_YDelta(int const *dims, double const *bounds)
{
    if (MACSIO_UTILS_YDim(dims) < 2) return -1;
    return MACSIO_UTILS_YRange(bounds) / (MACSIO_UTILS_YDim(dims) - 1);
}
double MACSIO_UTILS_ZDelta(int const *dims, double const *bounds)
{
    if (MACSIO_UTILS_ZDim(dims) < 2) return -1;
    return MACSIO_UTILS_ZRange(bounds) / (MACSIO_UTILS_ZDim(dims) - 1);
}

json_object *
MACSIO_UTILS_MakeDimsJsonArray(int ndims, const int *dims)
{
    json_object *dims_array = json_object_new_array();
    json_object_array_add(dims_array, json_object_new_int(dims[0]));
    if (ndims > 1)
        json_object_array_add(dims_array, json_object_new_int(dims[1]));
    if (ndims > 2)
        json_object_array_add(dims_array, json_object_new_int(dims[2]));
    return dims_array;
}

//#warning WOULD BE MORE CONSISTENT TO STORE AS MIN/MAX PAIRS RATHER THAN ALL MINS FOLLOWED BY ALL MAXS
json_object *
MACSIO_UTILS_MakeBoundsJsonArray(double const * bounds)
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

static char const *print_bytes(double val, char const *_fmt, char *str, int n, char const *_persec)
{
    char const *persec = _persec ? _persec : "";
    char const *fmt = _fmt ? _fmt : "%8.4f";

    static double const ki = 1024.0;
    static double const mi = 1024.0 * 1024.0;
    static double const gi = 1024.0 * 1024.0 * 1024.0;
    static double const ti = 1024.0 * 1024.0 * 1024.0 * 1024.0;
    static double const pi = 1024.0 * 1024.0 * 1024.0 * 1024.0 * 1024.0;

    static char const *kistr = "Ki";
    static char const *mistr = "Mi";
    static char const *gistr = "Gi";
    static char const *tistr = "Ti";
    static char const *pistr = "Pi";

    static double const kb = 1000.0;
    static double const mb = 1000.0 * 1000.0;
    static double const gb = 1000.0 * 1000.0 * 1000.0;
    static double const tb = 1000.0 * 1000.0 * 1000.0 * 1000.0;
    static double const pb = 1000.0 * 1000.0 * 1000.0 * 1000.0 * 1000.0;

    static char const *kbstr = "Kb";
    static char const *mbstr = "Mb";
    static char const *gbstr = "Gb";
    static char const *tbstr = "Tb";
    static char const *pbstr = "Pb";

    char fmt2[32];

    double kv = ki;
    double mv = mi;
    double gv = gi;
    double tv = ti;
    double pv = pi;

    char const *kvstr = kistr;
    char const *mvstr = mistr;
    char const *gvstr = gistr;
    char const *tvstr = tistr;
    char const *pvstr = pistr;

    if (!strncasecmp(MACSIO_UTILS_UnitsPrefixSystem, "decimal",
        sizeof(MACSIO_UTILS_UnitsPrefixSystem)))
    {
        kv = kb;
        mv = mb;
        gv = gb;
        tv = tb;
        pv = pb;
        kvstr = kbstr;
        mvstr = mbstr;
        gvstr = gbstr;
        tvstr = tbstr;
        pvstr = pbstr;
    }

    if      (val >= pv)
    {
        snprintf(fmt2, sizeof(fmt2), "%s %s%s", fmt, pvstr, persec);
        snprintf(str, n, fmt2, val / pv);
    }
    else if (val >= tv)
    {
        snprintf(fmt2, sizeof(fmt2), "%s %s%s", fmt, tvstr, persec);
        snprintf(str, n, fmt2, val / tv);
    }
    else if (val >= gv)
    {
        snprintf(fmt2, sizeof(fmt2), "%s %s%s", fmt, gvstr, persec);
        snprintf(str, n, fmt2, val / gv);
    }
    else if (val >= mv)
    {
        snprintf(fmt2, sizeof(fmt2), "%s %s%s", fmt, mvstr, persec);
        snprintf(str, n, fmt2, val / mv);
    }
    else if (val >= kv)
    {
        snprintf(fmt2, sizeof(fmt2), "%s %s%s", fmt, kvstr, persec);
        snprintf(str, n, fmt2, val / kv);
    }
    else
    {
        snprintf(fmt2, sizeof(fmt2), "%s b%s", fmt, persec);
        snprintf(str, n, fmt2, val);
    }

    return str;
}

char const *MACSIO_UTILS_PrintBytes(unsigned long long bytes, char const *fmt, char *str, int n)
{
    return print_bytes((double)bytes, fmt, str, n, 0);
}

char const *MACSIO_UTILS_PrintBandwidth(unsigned long long bytes, double seconds,
    char const *fmt, char *str, int n)
{
    return print_bytes((double)bytes/seconds, fmt, str, n, "/sec");
}

char const *MACSIO_UTILS_PrintSeconds(double seconds, char const *fmt, char *str, int n)
{
    static double const min  = 60.0;
    static double const hour = 60.0 * 60.0;
    static double const day  = 60.0 * 60.0 * 24.0;
    static double const week = 60.0 * 60.0 * 24.0 * 7.0;
    char fmt2[32];

    if (seconds >= week)
    {
        snprintf(fmt2, sizeof(fmt2), "%s wks", fmt?fmt:"%8.4f");
        snprintf(str, n, fmt2, seconds/week);
    }
    else if (seconds >= day)
    {
        snprintf(fmt2, sizeof(fmt2), "%s days", fmt?fmt:"%8.4f");
        snprintf(str, n, fmt2, seconds/day);
    }
    else if (seconds >= hour)
    {
        snprintf(fmt2, sizeof(fmt2), "%s hrs", fmt?fmt:"%8.4f");
        snprintf(str, n, fmt2, seconds/hour);
    }
    else if (seconds >= min)  
    {
        snprintf(fmt2, sizeof(fmt2), "%s mins", fmt?fmt:"%8.4f");
        snprintf(str, n, fmt2, seconds/min);
    }
    else if (seconds >= 1)
    {
        snprintf(fmt2, sizeof(fmt2), "%s secs", fmt?fmt:"%8.4f");
        snprintf(str, n, fmt2, seconds);
    }
    else if (seconds >= 1e-3)
    {
        snprintf(fmt2, sizeof(fmt2), "%s msecs", fmt?fmt:"%8.4f");
        snprintf(str, n, fmt2, seconds/1e-3);
    }
    else if (seconds >= 1e-6)
    {
        snprintf(fmt2, sizeof(fmt2), "%s usecs", fmt?fmt:"%8.4f");
        snprintf(str, n, fmt2, seconds/1e-6);
    }
    else if (seconds >= 1e-9)
    {
        snprintf(fmt2, sizeof(fmt2), "%s nsecs", fmt?fmt:"%8.4f");
        snprintf(str, n, fmt2, seconds/1e-9);
    }

    return str;
}

typedef struct s_filegroup {
    int size;
    int total;
    char **names;
} filegroup;

filegroup* files;
int filegroup_count = 0;

void MACSIO_UTILS_CreateFileStore(int num_dumps, int files_per_dump)
{
    filegroup_count = num_dumps;
    files = (filegroup*)malloc(num_dumps * sizeof(filegroup));
    for (int i=0; i<num_dumps; i++){
        files[i].size = 0;
        files[i].total = files_per_dump;
        files[i].names = (char**)malloc(files_per_dump*sizeof(char*));
    }
}

void MACSIO_UTILS_RecordOutputFiles(int dump_num, char *filename)
{
    if (dump_num > filegroup_count) return;

    int count = files[dump_num].size;

    char *name = (char*) malloc(sizeof(char)*strlen(filename)+1);
    strcpy(name, filename);

    if (files[dump_num].size == files[dump_num].total){
        files[dump_num].names = (char**)realloc(files[dump_num].names, 1.5*files[dump_num].total*sizeof(char*));
        files[dump_num].total *= 1.5;
    }

    files[dump_num].names[count] = name;
    files[dump_num].size++;
}

void MACSIO_UTILS_CleanupFileStore()
{   
    for (int i=0; i<filegroup_count; i++){
        for ( int j=0; j<files[i].size; j++){
            free(files[i].names[j]);
        }
        free(files[i].names);
    }
    free(files);
}

unsigned long long MACSIO_UTILS_StatFiles(int dump_num)
{
    if (dump_num > filegroup_count) return 0;

    unsigned long long dump_bytes = 0;

    struct stat *buf;
    buf = (struct stat*)malloc(sizeof(struct stat));

    for (int i=0; i<files[dump_num].size; i++){
        stat(files[dump_num].names[i], buf);
        int size = buf->st_size;
        dump_bytes += size;
        //printf("%s: %d\n", files[dump_num].names[i], size);

    }
    free(buf);
    return dump_bytes;
}
