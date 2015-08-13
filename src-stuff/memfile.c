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

/*
This example demonstrates the use of mmap to MPI send/recv whole files
between MPI ranks all of which are aggregated at processor 0 into a real
file on disk. If the sender files are handled via a memory-based filesystem
such as tmpfs or ramfs, then no disk I/O is actually involved at the senders.

In addition, we use a trick in MPI (which I was unaware of until developing
this code) where an MPI_recv need not know, apriori, the size of the sent 
message. As long as it knows a maximum size, it can post a recv for a message
of the maximum size. The send will match with it and then an additional MPI
call, MPI_Get_count is used to determine the true size of the matching send.

At the aggregator, we mmap a single file/buffer (also best if the file is in
a memory-based filesystem) and then iterate over senders receiving their whole
HDF5 files as messages (as bufs of chars) into this buffer. Each time, we then
H5Fopen the mmapped buffer and H5Ocopy its contents in the aggregator file,
which is the only real file on disk.

For this test to work, it is non-essential that the sender's or reciever's
mmapped files actually be in a memory-based filesystem. The code works either
way. However, then a memory-based filesystem is used, then no actual disk I/O
is generated for the associated "files".

Finally, to be fair in comparison with a real two-phase I/O strategy, we are
not yet demonstrating here the aggregation of individual objects into larger
objects (and subsequent larger I/O requests) in the aggregator file. But that
is easily added by simply taking individual dataset contents, catentating them
into a larger buffer and then writing out that larger buffer to the aggregator
file as an aggregated dataset.

To apply this as a general mechanism within MACSio's MIF module, a little care
is needed to ensure the mesh parts that are indeed next to each other arrive
at the same aggregator so that a larger datasets involving multiple mesh parts
still forms a coherent part of the whole.
*/

#include <hdf5.h>
#include <mpi.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define ARG_INT 1
#define ARG_DBL 2
#define ARG_STR 3

#define FILE_TAG 54

#if 0
            case ARG_DBL:                                 \
                ARGNM = (TYPE) strtod(&argv[i][n],&tmp);  \
                had_error = (ARGNM==0 && errno != 0);     \
                break;                                    
#endif

#define HANDLE_ARG(ARGNM,TYPE,HELP)                       \
{                                                         \
    int n = strlen(#ARGNM)+1;                             \
    if (help)                                             \
    {                                                     \
        fprintf(stderr, "%s=<%s>: %s\n",                  \
            #ARGNM, #TYPE, #HELP);                        \
    }                                                     \
    else if (!strncasecmp(argv[i], #ARGNM"=", n))         \
    {                                                     \
        int had_error = 0;                                \
        char *tmp = 0;                                    \
        errno = 0;                                        \
        switch(typecode(#TYPE))                           \
        {                                                 \
            case ARG_INT:                                 \
                ARGNM = (TYPE) strtol(&argv[i][n],0,10);  \
                had_error = (ARGNM==0 && errno != 0);     \
                break;                                    \
            case ARG_STR:                                 \
                ARGNM = (TYPE) strdup(&argv[i][n]);       \
                had_error = errno!=0;                     \
                break;                                    \
            default: had_error = 1;                       \
        }                                                 \
        if (had_error)                                    \
            fprintf(stderr, "error processing argument \"%s\"" \
                "with input \"%s\"\n", #ARGNM, argv[i]);  \
    }                                                     \
}

static int typecode(char const *typestr)
{
    if (!strncasecmp(typestr,"int",3)) return ARG_INT;
    if (!strncasecmp(typestr,"double",6)) return ARG_DBL;
    if (!strncasecmp(typestr,"char*",5)) return ARG_STR;
    return 0;
}

int main(int argc, char **argv)
{
    int i;
    int rank = 0;
    int size = 1;
    char *memfs_path = 0;
    int help = (argc == 1 || (argc == 2 && strcasestr(argv[1], "help")));

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    H5open();
    H5Eset_auto1((H5E_auto1_t) H5Eprint1, stderr);

    for (i = 0; i < argc; i++)
    {
        HANDLE_ARG(help, int, Print help);
        HANDLE_ARG(memfs_path, char*, Path to a writeable dir
            in a mem-based filesystem (e.g. tmpfs or ramfs));
        if (help) exit(1);
    }

    /* rank 0 will serve as an "aggregator" recieving messages (whole files) from
       other ranks to 'append' to a single file */
    if (rank == 0)
    {
        int fd;
        int max_len = 2*3*size*sizeof(double)+10*1024;
        char filename[256];
        void *recvbuf;
        void *buf = malloc(max_len);
        hid_t fid;

        /* create the 'aggregated' results file */
        fid = H5Fcreate("aggregated.h5", H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

        /* create a temp file in memfs space to serve as our recv buffer */
        snprintf(filename, sizeof(filename), "%s/memfile-tmpbuf-000.h5", memfs_path, rank);
        fd = open(filename, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);
        write(fd, buf, max_len); /* write garbage for now */

        /* map the file into memory which will serve as our recv buffer */
        errno = 0;
        recvbuf = mmap(0, (size_t) max_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (errno != 0)
        {
            fprintf(stderr, "Encountered error \"%s\"\n", strerror(errno));
            abort();
        }
        assert(recvbuf != 0);

        for (i = 1; i < size; i++)
        {
            int real_len;
            hid_t tmpfid, src_grpid, src_dsid, dsid, sid, tid;
            hsize_t dims[3];
            MPI_Status status;
            char grpname[256];

            /* blocking recieve from any of whole file contents */
            /* note that the recv will complete possibly with fewer than max_len bytes */
            MPI_Recv(recvbuf, max_len, MPI_CHAR, MPI_ANY_SOURCE, FILE_TAG, MPI_COMM_WORLD, &status);
            MPI_Get_count(&status, MPI_CHAR, &real_len);
            msync(recvbuf, real_len, MS_SYNC);
            sync();

            /* setup name of group in aggregator file for this processor's contributions */
            snprintf(grpname, sizeof(grpname), "group-%03d", (int) status.MPI_SOURCE);

            /* ok, should be safe to 'open' the file as an HDF5 file and copy contents
               to aggregator file */
            tmpfid = H5Fopen(filename, H5F_ACC_RDONLY, H5P_DEFAULT);
            H5Ocopy(tmpfid, "/", fid, grpname, H5P_DEFAULT, H5P_DEFAULT);
            H5Fclose(tmpfid);
        }

        assert(munmap(recvbuf, (size_t) max_len)==0);
        close(fd);
        unlink(filename);

        /* close aggregator file */
        H5Fclose(fid);

    }
    else
    {
        char filename[256];
        hid_t fid, sid, dsid;
        int ndims=3, n=2*3*rank;
        hsize_t dims[3] = {2,3,rank};
        hid_t tid = rank%2?H5T_NATIVE_DOUBLE:H5T_NATIVE_INT;
        int *ibuf;
        double *dbuf;
        void *buf, *sendbuf;
        int fd, nbytes;
        struct stat stat_buf;

        /* Other ranks create a file in memfs, mmap it and send it to rank 0 to aggregate */

        /* This is essentially the MIF work block on each processor */

        /* Ordinarily, these would be on different nodes of a parallel machine
           and so in different (e.g. local) filesystems and no need to fiddle with filenames
           to make sure they don't collide with other processor's filenames */
        snprintf(filename, sizeof(filename), "%s/memfile-test-%03d.h5", memfs_path, rank);
        dbuf = (double*) malloc(n * sizeof(double));
        ibuf = (int*) malloc(n * sizeof(int));
        buf = rank%2?(void*)dbuf:(void*)ibuf;
        for (i = 0; i < n; i++)
        {
            dbuf[i] = ((double) 2 * i) / n;
            ibuf[i] = i;
        }

        fid = H5Fcreate(filename, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
        sid = H5Screate_simple(ndims, dims, 0);
        dsid = H5Dcreate1(fid, "foo", tid, sid, H5P_DEFAULT);
        H5Dwrite(dsid, tid, H5S_ALL, H5S_ALL, H5P_DEFAULT, buf);
        H5Sclose(sid);
        H5Dclose(dsid);
        H5Fclose(fid);
        sync();

        fd = open(filename, O_RDONLY);
        fstat(fd, &stat_buf); 
        nbytes = (int) stat_buf.st_size;

        /* mmap the file so we can MPI_Send it */
        errno = 0;
        sendbuf = mmap(0, (size_t) nbytes, PROT_READ, MAP_PRIVATE, fd, 0);
        if (errno != 0)
        {
            fprintf(stderr, "Encountered error \"%s\"\n", strerror(errno));
            abort();
        }
        assert(sendbuf != 0);

        MPI_Send(sendbuf, nbytes, MPI_CHAR, 0, FILE_TAG, MPI_COMM_WORLD);

        /* remove the mmap mapping */
        assert(munmap(sendbuf, (size_t) nbytes)==0);
        close(fd);
        unlink(filename);
    }

    MPI_Finalize();
}
