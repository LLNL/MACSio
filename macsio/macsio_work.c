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
#include <sys/time.h>
#include <math.h>
#include <stdlib.h>
# include <string.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

#include <macsio_data.h>
#include <macsio_log.h>
#include <macsio_main.h>
#include <macsio_utils.h>
#include <macsio_work.h>

char *getTimestamp()
{
    char *timestamp = (char *)malloc(sizeof(char) *26);
    char buffer[26];
    int millisec;
    struct tm* tm_info;
    struct timeval tv;

    gettimeofday(&tv,NULL);
    millisec = lrint(tv.tv_usec/1000.0);
    if (millisec >= 1000){
	millisec -=1000;
	tv.tv_sec++;
    }
    tm_info = localtime(&tv.tv_sec);

    strftime(buffer, 26, "%H:%M:%S", tm_info);
    sprintf(timestamp, "%s.%03d", buffer, millisec);
    return timestamp;
}

void MACSIO_WORK_DoComputeWork(double *currentT, double currentDt, int workIntensity) 
{
    time_t start,end;
    time(&start);
    char *start_c = getTimestamp();
    switch(workIntensity)
    {
	case 1:
	    MACSIO_WORK_LevelOne(currentDt);
	    break;
	case 2:
	    MACSIO_WORK_LevelTwo(currentDt);
	    break;
	case 3:
	    MACSIO_WORK_LevelThree(currentDt);
	    break;
	default:
	    return;
    }
    char *end_c = getTimestamp();
    time(&end);
    MACSIO_LOG_MSG(Info, ("Work phase: Level %d: Begin %s - End %s: Duration %.2f sec", workIntensity, start_c, end_c, difftime(end, start)));
    *currentT += currentDt;
}

/* Sleep */
void MACSIO_WORK_LevelOne(double currentDt)
{
    struct timespec tim, tim2;
    tim.tv_sec = currentDt;
    tim.tv_nsec = 0;
    nanosleep(&tim, &tim2);
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
    cell_t *mcell;
} mesh_t;

/* Spin CPU */
void MACSIO_WORK_LevelTwo(double currentDt)
{
    time_t start_t, end_t;

    mesh_t *mesh;
    double *xx,*yy,*zz;
    cell_t *cell;

    int nm = 8000;
    int np = 1000000;
    int nshuffle = np/2;
    int nloop = 10;
    int i1, i2, i3;
    double rnum;

    /* Setup data */
    mesh = (mesh_t*) malloc(sizeof(mesh_t));
    mesh->mcell = (cell_t*) malloc(nm*sizeof(cell_t));

    int *indx = (int*) malloc(np*sizeof(int));
    int *loc = (int*) malloc(np*sizeof(int));

    xx = (double*) malloc(np*sizeof(double));
    yy = (double*) malloc(np*sizeof(double));
    zz = (double*) malloc(np*sizeof(double));

    for (int ii=0; ii<nm; ii++){
	mesh->mcell[ii].ccs1 = ii+1.0;
	mesh->mcell[ii].ccs2 = ii+2.0;
	mesh->mcell[ii].ccs3 = ii+3.0;
	mesh->mcell[ii].ccs4 = ii+4.0;
	for (int j=0; j<3; j++){
	    mesh->mcell[ii].ccv1[j] = ii+5.0;
	    mesh->mcell[ii].ccv2[j] = ii+6.0;
	    mesh->mcell[ii].ccv3[j] = ii+7.0;
	    mesh->mcell[ii].ccv4[j] = ii+8.0;
	}
	for (int j=0; j<4; j++){
	    mesh->mcell[ii].ncs1[j] = ii+9.0;
	    mesh->mcell[ii].ncs2[j] = ii+10.0;
	}
	for (int j=0; j<4; j++){
	    for (int k=0; k<3; k++){
		mesh->mcell[ii].ncv1[j][k] = ii+11.0/nm;
		mesh->mcell[ii].ncv2[j][k] = ii+12.0/nm;
	    }
	}
    }
    for (int ii=0; ii<np; ii++){
	indx[ii] = ii;
    }

    /* we're using naive_rtv here for randomization meaning that each
       rank is guaranteed to have different behavior */
    for (int ii=0; ii<nshuffle; ii++){
	rnum = ((double)MD_random_naive_rtv()/(double)((1u<<31)-1));
	i1 = int(rnum*np);
	rnum = ((double)MD_random_naive_rtv()/(double)((1u<<31)-1));
	i2 = int(rnum*np);
	if (i1 < 1) i1 = 1;
	if (i1 > np) i1=np;
	if (i2 < 1) i2 = 1;
	if (i2 > np) i2 = np;
	i3 = indx[i1];
	indx[i1] = indx[i2];
	indx[i2] = i3;
    }
    
    
    for (int ii=0; ii<np; ii++){
	rnum = ((double)MD_random_naive_rtv()/(double)((1u<<31)-1));
	i1 = int(rnum*nm);
	if (i1 > nm) i1 = nm;
	if (i1 <= 0) i1 = 1;
	loc[ii] = i1;
    }

    /* Start timer */ 
    time(&start_t);

    cell = mesh->mcell;
    int ii=0;
    /* Start of work loop */
    while (true){
	i1 = indx[ii];

	xx[i1]=0.0;
	yy[i1]=0.0;
	zz[i1]=0.0;
	for (int jj=0; jj<nloop; jj++){
	    /* Do flops requiring cell data */
	    xx[i1] += cell[loc[i1]].ccs1;
	    yy[i1] += cell[loc[i1]].ccs2;
	    zz[i1] += cell[loc[i1]].ccs3;

	    xx[i1] += sqrt( square(cell[loc[i1]].ccv1[0]) + square(cell[loc[i1]].ccv2[0]) + square(cell[loc[i1]].ccv3[0]) );
	    yy[i1] += sqrt( square(cell[loc[i1]].ccv1[1]) + square(cell[loc[i1]].ccv2[1]) + square(cell[loc[i1]].ccv3[1]) );
	    zz[i1] += sqrt( square(cell[loc[i1]].ccv1[2]) + square(cell[loc[i1]].ccv2[2]) + square(cell[loc[i1]].ccv3[2]) );

	    xx[i1] += log(cell[loc[i1]].ccv4[0]);
	    yy[i1] += log(cell[loc[i1]].ccv4[1]);
	    zz[i1] += log(cell[loc[i1]].ccv4[2]);

	    xx[i1] += sqrt( cell[loc[i1]].ncs1[0] + cell[loc[i1]].ncs1[1] + cell[loc[i1]].ncs1[2] + cell[loc[i1]].ncs1[3]);
	    yy[i1] += sqrt( cell[loc[i1]].ncs2[0] + cell[loc[i1]].ncs2[1] + cell[loc[i1]].ncs2[2] + cell[loc[i1]].ncs2[3]);
	    zz[i1] += sqrt( cell[loc[i1]].ncs1[0] + cell[loc[i1]].ncs1[1] + cell[loc[i1]].ncs1[2] + cell[loc[i1]].ncs1[3]
			    + cell[loc[i1]].ncs2[0] + cell[loc[i1]].ncs2[1] + cell[loc[i1]].ncs2[2] + cell[loc[i1]].ncs2[3]);

	    xx[i1] += ( sin(cell[loc[i1]].ncv1[0][0])+ sin(cell[loc[i1]].ncv1[1][0])+ sin(cell[loc[i1]].ncv1[2][0])+ sin(cell[loc[i1]].ncv1[4][0]) 
			    + sin(cell[loc[i1]].ncv2[0][0])+ sin(cell[loc[i1]].ncv2[1][0])+ sin(cell[loc[i1]].ncv2[2][0])+ sin(cell[loc[i1]].ncv2[4][0]));
	    yy[i1] += ( sin(cell[loc[i1]].ncv1[0][1])+ sin(cell[loc[i1]].ncv1[1][1])+ sin(cell[loc[i1]].ncv1[2][1])+ sin(cell[loc[i1]].ncv1[4][1]) 
			    + sin(cell[loc[i1]].ncv2[0][1])+ sin(cell[loc[i1]].ncv2[1][1])+ sin(cell[loc[i1]].ncv2[2][1])+ sin(cell[loc[i1]].ncv2[4][1]));
	    zz[i1] += ( sin(cell[loc[i1]].ncv1[0][2])+ sin(cell[loc[i1]].ncv1[1][2])+ sin(cell[loc[i1]].ncv1[2][2])+ sin(cell[loc[i1]].ncv1[4][2]) 
			    + sin(cell[loc[i1]].ncv2[0][2])+ sin(cell[loc[i1]].ncv2[1][2])+ sin(cell[loc[i1]].ncv2[2][2])+ sin(cell[loc[i1]].ncv2[4][2]));
	}

	time(&end_t);
	/* If we've done enough work break out and return to main loop */
	if (difftime(end_t, start_t) >= currentDt) break;
	ii++;
	if (ii >= np){
	    ii = 0;
	}
    }
	free(xx);
	free(yy); 
	free(zz);
	free(indx);
	free(loc);
	free(mesh->mcell);
	free(mesh);
}

double square(double num)
{
    return num*num;
}

/* Level Three solves the 2D Poisson equation via the Jacobi iterative method
 * The domains are divided into strips and non blocking MPI routines are used
 */

/* macro to index into a 2-D (N+2)x(N+2) array */
#define INDEX(i,j) ((N+2)*(i)+(j))

void MACSIO_WORK_LevelThree(double currentDt)
{
    int N = 1024;			/* number of interior points per dim */
    double *f;
    int i;
    int j;
    double change;
    double my_change;
    int my_n;
    int n;
    int step;
    double *swap;
    double wall_time;
    double start, end;
    int *proc;			/* process indexed by vertex */

    double *u, *u_new;		/* linear arrays to hold solution */
    int *i_min, *i_max;		/* min, max vertex indices of processes */
    int *left_proc, *right_proc;	/* processes to left and right */

    /* allocate and zero u and u_new */
    int ndof = ( N + 2 ) * ( N + 2 );
    u = ( double * ) malloc ( ndof * sizeof ( double ) );
    for ( i = 0; i < ndof; i++){
	u[i] = 0.0;
    }

    u_new = ( double * ) malloc ( ndof * sizeof ( double ) );
    for ( i = 0; i < ndof; i++ ){
	u_new[i] = 0.0;
    }

    /* Set up source term for poisson equation */
    int k;
    double q;

    f = ( double * ) malloc ( ( N + 2 ) * ( N + 2 ) * sizeof ( double ) );

    for ( i = 0; i < ( N + 2 ) * ( N + 2 ); i++ ){
	f[i] = 0.0;
    }
    /* Make a dipole. */
    q = 10.0;

    i = 1 + N / 4;
    j = i;
    k = INDEX ( i, j );
    f[k] = q;

    i = 1 + 3 * N / 4;
    j = i;
    k = INDEX ( i, j );
    f[k] = -q;

    double d;
    double eps;
    int p;
    double x_max;
    double x_min;

    /* Allocate arrays for process information. */
    proc = ( int * ) malloc ( ( N + 2 ) * sizeof ( int ) );
    i_min = ( int * ) malloc ( MACSIO_MAIN_Size * sizeof ( int ) );
    i_max = ( int * ) malloc ( MACSIO_MAIN_Size * sizeof ( int ) );
    left_proc = ( int * ) malloc ( MACSIO_MAIN_Size * sizeof ( int ) );
    right_proc = ( int * ) malloc ( MACSIO_MAIN_Size * sizeof ( int ) );
    /* Divide the range [(1-eps)..(N+eps)] evenly among the processes. */
    eps = 0.0001;
    d = ( N - 1.0 + 2.0 * eps ) / ( double ) MACSIO_MAIN_Size;

    for ( p = 0; p < MACSIO_MAIN_Size; p++ ){
	/* The I indices assigned to domain P will satisfy X_MIN <= I <= X_MAX. */
	x_min = - eps + 1.0 + ( double ) ( p * d );
	x_max = x_min + d;
	/* For the node with index I, store in PROC[I] the process P it belongs to. */
	for ( i = 1; i <= N; i++ ){
	    if ( x_min <= i && i < x_max ){
		proc[i] = p;
	    }
	}
    }
    /* Now find the lowest index I associated with each process P. */
    for ( p = 0; p < MACSIO_MAIN_Size; p++ ){
	for ( i = 1; i <= N; i++ ){
	    if ( proc[i] == p ){
		break;
	    }
	}
	i_min[p] = i;
	/* Find the largest index associated with each process P. */
	for ( i = N; 1 <= i; i-- ){
	    if ( proc[i] == p ){
		break;
	    }
	}
	i_max[p] = i;
	/* Find the processes to left and right. */
	left_proc[p] = -1;
	right_proc[p] = -1;

	if ( proc[p] != -1 ) {
	    if ( 1 < i_min[p] && i_min[p] <= N ){
		left_proc[p] = proc[i_min[p] - 1];
	    }
	    if ( 0 < i_max[p] && i_max[p] < N ){
		right_proc[p] = proc[i_max[p] + 1];
	    }
	}
    }

    step = 0;
    /* Begin timing. */
#ifdef HAVE_MPI
    start = MPI_Wtime ( );
#endif
    /* Begin iteration. */
    do {
	jacobi (N, f, u, u_new, i_min, i_max, left_proc, right_proc);
	++step;
	/* Estimate the error */
	change = 0.0;
	n = 0;

	my_change = 0.0;
	my_n = 0;

	for ( i = i_min[MACSIO_MAIN_Rank]; i <= i_max[MACSIO_MAIN_Rank]; i++ ){
	    for ( j = 1; j <= N; j++ ){
		if ( u_new[INDEX(i,j)] != 0.0 ) {
		    my_change = my_change + fabs ( 1.0 - u[INDEX(i,j)] / u_new[INDEX(i,j)] );
		    my_n = my_n + 1;
		}
	    }
	}
#ifdef HAVE_MPI
	MPI_Allreduce ( &my_change, &change, 1, MPI_DOUBLE, MPI_SUM, MACSIO_MAIN_Comm );
	MPI_Allreduce ( &my_n, &n, 1, MPI_INT, MPI_SUM, MACSIO_MAIN_Comm );
#endif

	if ( n != 0 ){
	    change = change / n;
	}
	/*  Swap U and U_NEW. */
	swap = u;
	u = u_new;
	u_new = swap;

	/* Because we are aiming to loop for a set time, we need to let the root process
	 * measure the time elapsed to ensure everyone does the same number of steps */ 
#ifdef HAVE_MPI
	end = MPI_Wtime();
	wall_time = end-start;
	MPI_Bcast(&wall_time, 1, MPI_DOUBLE, 0, MACSIO_MAIN_Comm);
#endif
    } while (wall_time < currentDt);

    free ( f );
}

/* Carry out the Jacobi iteration */
void jacobi (int N, double *f, double *u, double *u_new, int *i_min, int *i_max, int *left_proc, int *right_proc)
{
    double h;
    int i;
    int j;
#ifdef HAVE_MPI
    MPI_Request request[4];
    MPI_Status status[4];
#endif
    int requests;
    /* H is the lattice spacing. */
    h = 1.0 / ( double ) ( N + 1 );
    /* Update ghost layers using non-blocking send/receive */
    requests = 0;

#ifdef HAVE_MPI
    if ( left_proc[MACSIO_MAIN_Rank] >= 0 && left_proc[MACSIO_MAIN_Rank] < MACSIO_MAIN_Size ) {
	MPI_Irecv ( u + INDEX(i_min[MACSIO_MAIN_Rank] - 1, 1), N, MPI_DOUBLE,
		left_proc[MACSIO_MAIN_Rank], 0, MACSIO_MAIN_Comm,
		request + requests++ );

	MPI_Isend ( u + INDEX(i_min[MACSIO_MAIN_Rank], 1), N, MPI_DOUBLE,
		left_proc[MACSIO_MAIN_Rank], 1, MACSIO_MAIN_Comm,
		request + requests++ );
    }

    if ( right_proc[MACSIO_MAIN_Rank] >= 0 && right_proc[MACSIO_MAIN_Rank] < MACSIO_MAIN_Size ) {
	MPI_Irecv ( u + INDEX(i_max[MACSIO_MAIN_Rank] + 1, 1), N, MPI_DOUBLE,
		right_proc[MACSIO_MAIN_Rank], 1, MACSIO_MAIN_Comm,
		request + requests++ );

	MPI_Isend ( u + INDEX(i_max[MACSIO_MAIN_Rank], 1), N, MPI_DOUBLE,
		right_proc[MACSIO_MAIN_Rank], 0, MACSIO_MAIN_Comm,
		request + requests++ );
    }
#endif

    /* Jacobi update for internal vertices in my domain */
    for ( i = i_min[MACSIO_MAIN_Rank] + 1; i <= i_max[MACSIO_MAIN_Rank] - 1; i++ ){
	for ( j = 1; j <= N; j++ ){
	    u_new[INDEX(i,j)] =
		0.25 * ( u[INDEX(i-1,j)] + u[INDEX(i+1,j)] +
			u[INDEX(i,j-1)] + u[INDEX(i,j+1)] +
			h * h * f[INDEX(i,j)] );
	}
    }
    /*  Wait for all non-blocking communications to complete. */
#ifdef HAVE_MPI
    MPI_Waitall ( requests, request, status );
#endif
    /* Jacobi update for boundary vertices in my domain. */
    i = i_min[MACSIO_MAIN_Rank];
    for ( j = 1; j <= N; j++ ){
	u_new[INDEX(i,j)] =
	    0.25 * ( u[INDEX(i-1,j)] + u[INDEX(i+1,j)] +
		    u[INDEX(i,j-1)] + u[INDEX(i,j+1)] +
		    h * h * f[INDEX(i,j)] );
    }

    i = i_max[MACSIO_MAIN_Rank];
    if (i != i_min[MACSIO_MAIN_Rank]){
	for (j = 1; j <= N; j++){
	    u_new[INDEX(i,j)] =
		0.25 * ( u[INDEX(i-1,j)] + u[INDEX(i+1,j)] +
			u[INDEX(i,j-1)] + u[INDEX(i,j+1)] +
			h * h * f[INDEX(i,j)] );
	}
    }
}
