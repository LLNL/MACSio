#include <math.h>

#include <macsio_utils.h>

/*-------------------------------------------------------------------------
  Function: bjhash 

  Purpose: Hash a variable length stream of bytes into a 32-bit value.

  Programmer: By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.

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

#warning WOULD BE MORE CONSISTENT TO STORE AS MIN/MAX PAIRS RATHER THAN ALL MINS FOLLOWED BY ALL MAXS
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
