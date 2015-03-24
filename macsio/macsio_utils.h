#ifndef MACSIO_UTILS_H
#define MACSIO_UTILS_H

#include <json-c/json.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Short-hand for commonly used functions */
#define MU_SeqIdx3(i,j,k,Ni,Nj) MACSIO_UTILS_LogicalIJKIndexToSequentialIndex(i,j,k,Ni,Nj)
#define MU_SeqIdx2(i,j,Ni)      MACSIO_UTILS_LogicalIJIndexToSequentialIndex (i,j,  Ni   )
#define MU_SeqIdx1(i)           MACSIO_UTILS_LogicalIIndexToSequentialIndex  (i          )
#define MU_LogIdx3(s,Ni,Nj,a,b,c)                                        \
{ int q0,q1,q2;                                                       \
  MACSIO_UTILS_SequentialIndexToLogicalIJKIndex(s,Ni,Nj,&q0,&q1,&q2); \
  a=q0;b=q1;c=q2;                                                     \
}
#define MU_LogIdx2(s,Ni,a,b)                                             \
{ int q0,q1;                                                          \
   MACSIO_UTILS_SequentialIndexToLogicalIJIndex(s,Ni,   &q0,&q1    ); \
  a=q0;b=q1;                                                          \
}
#define MU_LogIdx1(s,a)                                                  \
{ int q0;                                                             \
    MACSIO_UTILS_SequentialIndexToLogicalIIndex(s,      &q0        ); \
  a=q0;                                                               \
}

unsigned int MACSIO_UTILS_BJHash(const unsigned char *k, unsigned int length, unsigned int initval);
int MACSIO_UTILS_Best2DFactors(int val, int *x, int *y);
int MACSIO_UTILS_Best3DFactors(int val, int *x, int *y, int *z);
int MACSIO_UTILS_LogicalIJKIndexToSequentialIndex(int i,int j,int k,int Ni,int Nj);
int MACSIO_UTILS_LogicalIJIndexToSequentialIndex (int i,int j,      int Ni       );
int MACSIO_UTILS_LogicalIIndexToSequentialIndex  (int i                          );
void MACSIO_UTILS_SequentialIndexToLogicalIJKIndex(int s,int Ni,int Nj,int *i,int *j,int *k);
void MACSIO_UTILS_SequentialIndexToLogicalIJIndex(int s,int Ni,int *i,int *j);
void MACSIO_UTILS_SequentialIndexToLogicalIIndex(int s,int *i);

void MACSIO_UTILS_SetDims(int *dims, int nx, int ny, int nz);
int MACSIO_UTILS_XDim(int const *dims);
int MACSIO_UTILS_YDim(int const *dims);
int MACSIO_UTILS_ZDim(int const *dims);
void MACSIO_UTILS_SetBounds(double *bounds,
    double xmin, double ymin, double zmin,
    double xmax, double ymax, double zmax);
double MACSIO_UTILS_XRange(double const *bounds);
double MACSIO_UTILS_YRange(double const *bounds);
double MACSIO_UTILS_ZRange(double const *bounds);
double MACSIO_UTILS_XMin(double const *bounds);
double MACSIO_UTILS_YMin(double const *bounds);
double MACSIO_UTILS_ZMin(double const *bounds);
double MACSIO_UTILS_XMax(double const *bounds);
double MACSIO_UTILS_YMax(double const *bounds);
double MACSIO_UTILS_ZMax(double const *bounds);
double MACSIO_UTILS_XDelta(int const *dims, double const *bounds);
double MACSIO_UTILS_YDelta(int const *dims, double const *bounds);
double MACSIO_UTILS_ZDelta(int const *dims, double const *bounds);
json_object * MACSIO_UTILS_MakeDimsJsonArray(int ndims, const int *dims);
json_object * MACSIO_UTILS_MakeBoundsJsonArray(double const * bounds);

#ifdef __cplusplus
}
#endif

#endif
