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

#include <stdio.h>
#include <time.h>

#include <macsio_work.h>


int MACSIO_WORK_DoComputeWork(double *currentDt, double targetDelta, int workIntensity) 
{
    int ret = 0;
    switch(workIntensity)
    {
	case 1:
	    MACSIO_WORK_LevelOne(currentDt, targetDelta);
	    break;
	case 2:
	    MACSIO_WORK_LevelTwo(currentDt, targetDelta);
	    break;
	case 3:
	    MACSIO_WORK_LevelThree(currentDt, targetDelta);
	    break;
	default:
	    return 0;
    }

    return ret;
}

/* Sleep */
int MACSIO_WORK_LevelOne(double *currentDt, double targetDelta)
{
    int ret = 0;
    struct timespec tim, tim2;
    tim.tv_sec = targetDelta;
    tim.tv_nsec = 0;
    nanosleep(&tim, &tim2);
    return ret;
}

typedef struct _cell {
    // Cell centered scalars
    double ccs1, ccs2, ccs3, ccs4;
    // Cell centered vectors
    double ccv1[3], ccv2[3], ccv3[3], ccv4[3];
    // Node centered scalars
    double ncs1[4], ncs2[4];
    // Node centered vectors
    double ncv1[4][3], ncv2[4][3];
} cell_t;

typedef struct _mesh {
    cell_t *mcell, *gcell;
} mesh_t;

/* Spin CPU */
int MACSIO_WORK_LevelTwo(double *currentDt, double targetDelta)
{
    int ret = 0;
    
    mesh_t *mesh;
    double *xx,*yy,*zz;

    int nm = 8000;
    int ng = 150;
    int np = 100000;
    int nshuffle = np/2;
    int nloop = 10;

    /* Setup data */
    mesh = (mesh_t*) malloc(sizeof(mesh_t));
    mesh->mcell = (cell_t*) malloc(nm*sizeof(cell_t));
    mesh->gcell = (cell_t*) malloc(ng*sizeof(cell_t));

    int *indx = (int*) malloc(np*sizeof(int));
    int *loc = (int*) malloc(np*sizeof(int));

    xx = (double*) malloc(np*sizeof(double));
    yy = (double*) malloc(np*sizeof(double));
    zz = (double*) malloc(np*sizeof(double));

    for (int ii=0; ii<nm; ii++){
	mesh->mcell[ii].ccs1 = ii+1.0;
    }

    /*Do some steps till we get close to the targetDt*/

    return ret;
}

/* Computation based proxy code */
int MACSIO_WORK_LevelThree(double *currentDt, double targetDelta)
{
    int ret = 0;

    return ret;
}
