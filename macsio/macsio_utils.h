#ifndef MACSIO_UTILS_H
#define MACSIO_UTILS_H
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

#ifdef __cplusplus
extern "C" {
#endif

#define MU_MAX(A,B) (((A)>(B))?(A):(B))

/* Short-hand for commonly used functions */
#define MU_SeqIdx3(i,j,k,Ni,Nj) MACSIO_UTILS_LogicalIJKIndexToSequentialIndex(i,j,k,Ni,Nj)
#define MU_SeqIdx2(i,j,Ni)      MACSIO_UTILS_LogicalIJIndexToSequentialIndex (i,j,  Ni   )
#define MU_SeqIdx1(i)           MACSIO_UTILS_LogicalIIndexToSequentialIndex  (i          )
#define MU_LogIdx3(s,Ni,Nj,a,b,c)                                     \
{ int q0,q1,q2;                                                       \
  MACSIO_UTILS_SequentialIndexToLogicalIJKIndex(s,Ni,Nj,&q0,&q1,&q2); \
  a=q0;b=q1;c=q2;                                                     \
}
#define MU_LogIdx2(s,Ni,a,b)                                          \
{ int q0,q1;                                                          \
   MACSIO_UTILS_SequentialIndexToLogicalIJIndex(s,Ni,   &q0,&q1    ); \
  a=q0;b=q1;                                                          \
}
#define MU_LogIdx1(s,a)                                               \
{ int q0;                                                             \
    MACSIO_UTILS_SequentialIndexToLogicalIIndex(s,      &q0        ); \
  a=q0;                                                               \
}

#define MU_PrByts(B,FMT,STR,N) MACSIO_UTILS_PrintBytes(B,FMT,STR,N)
#define MU_PrSecs(S,FMT,STR,N) MACSIO_UTILS_PrintSeconds(S,FMT,STR,N)
#define MU_PrBW(B,S,FMT,STR,N) MACSIO_UTILS_PrintBandwidth(B,S,FMT,STR,N)

extern char MACSIO_UTILS_UnitsPrefixSystem[32];

extern unsigned int MACSIO_UTILS_BJHash(const unsigned char *k, unsigned int length, unsigned int initval);
extern int MACSIO_UTILS_Best2DFactors(int val, int *x, int *y);
extern int MACSIO_UTILS_Best3DFactors(int val, int *x, int *y, int *z);
extern int MACSIO_UTILS_LogicalIJKIndexToSequentialIndex(int i,int j,int k,int Ni,int Nj);
extern int MACSIO_UTILS_LogicalIJIndexToSequentialIndex (int i,int j,      int Ni       );
extern int MACSIO_UTILS_LogicalIIndexToSequentialIndex  (int i                          );
extern void MACSIO_UTILS_SequentialIndexToLogicalIJKIndex(int s,int Ni,int Nj,int *i,int *j,int *k);
extern void MACSIO_UTILS_SequentialIndexToLogicalIJIndex(int s,int Ni,int *i,int *j);
extern void MACSIO_UTILS_SequentialIndexToLogicalIIndex(int s,int *i);

extern void MACSIO_UTILS_SetDims(int *dims, int nx, int ny, int nz);
extern int MACSIO_UTILS_XDim(int const *dims);
extern int MACSIO_UTILS_YDim(int const *dims);
extern int MACSIO_UTILS_ZDim(int const *dims);
extern void MACSIO_UTILS_SetBounds(double *bounds,
    double xmin, double ymin, double zmin,
    double xmax, double ymax, double zmax);
extern double MACSIO_UTILS_XRange(double const *bounds);
extern double MACSIO_UTILS_YRange(double const *bounds);
extern double MACSIO_UTILS_ZRange(double const *bounds);
extern double MACSIO_UTILS_XMin(double const *bounds);
extern double MACSIO_UTILS_YMin(double const *bounds);
extern double MACSIO_UTILS_ZMin(double const *bounds);
extern double MACSIO_UTILS_XMax(double const *bounds);
extern double MACSIO_UTILS_YMax(double const *bounds);
extern double MACSIO_UTILS_ZMax(double const *bounds);
extern double MACSIO_UTILS_XDelta(int const *dims, double const *bounds);
extern double MACSIO_UTILS_YDelta(int const *dims, double const *bounds);
extern double MACSIO_UTILS_ZDelta(int const *dims, double const *bounds);
extern json_object * MACSIO_UTILS_MakeDimsJsonArray(int ndims, const int *dims);
extern json_object * MACSIO_UTILS_MakeBoundsJsonArray(double const * bounds);

extern char const *MACSIO_UTILS_PrintBytes(unsigned long long bytes, char const *fmt, char *str, int n);
extern char const *MACSIO_UTILS_PrintSeconds(double seconds, char const *fmt, char *str, int n);
extern char const *MACSIO_UTILS_PrintBandwidth(unsigned long long bytes, double seconds,
    char const *fmt, char *str, int n);

extern void MACSIO_UTILS_CreateFileStore(int num_dumps, int files_per_dump);
extern void MACSIO_UTILS_RecordOutputFiles(int dump_num, char *filename);
extern void MACSIO_UTILS_CleanupFileStore();
extern unsigned long long MACSIO_UTILS_StatFiles(int dump_num);

#ifdef __cplusplus
}
#endif

#endif
