#ifndef _MACSIO_MIF_H
#define _MACSIO_MIF_H

#include <stdlib.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

/*!
\defgroup MACSIO_MIF MACSIO_MIF
\brief Utilities supporting Multiple Indpendent File (MIF) Parallel I/O

In the multiple, independent file (MIF) paradigm, parallelism is achieved through
simultaneous access to multiple files. The application divides itself into file groups.
For each file group, the application manages exclusive access among all the tasks of
the group. I/O is serial within groups but parallel across groups. The number of files
(groups) is wholly independent from the number of processors or mesh-parts. It is often
chosen to match the number of independent I/O pathways available in the hardware between
the compute nodes and the filesystem. The MIF paradigm is sometimes also called N->M
because it is N tasks writing to M files (M<N).

In this paradigm I/O requests are almost exclusively independent. However, there are
scenarios where collective I/O requests can be made to work and might even make sense
in the MIF paradigm.  MIF is often referred to as Poor Man’s parallel I/O because the
onus is on the application to manage the distribution of data across potentially many
files. In truth, this illuminates the only salient distinction between Single Shared
File (SSF) and MIF. In either paradigm, if you dig deep enough into the I/O stack, you
soon discover that data is always being distributed across multiple files. The only
difference is where in the stack the distribution into files is handled. In the MIF
paradigm, it is handled explicitly by the application itself. In a typical SSF paradigm,
it is handled by the underling filesystem.

In MIF, MPI tasks are divided into N groups and each group is responsible for creating
one of the N files. At any one moment, only one MPI task from each group has exclusive
access to the file. Hence, I/O is serial within a group. However, because one task in
each group is writing to its group's own file, simultaneously, I/O is parallel across
groups.

\image html pmpio_diagram.png

A call to MACSIO_MIF_Init() establishes this mapping of MPI tasks to file groups.

Within a group, access to the group's file is handled in a round-robin fashion. The first
MPI task in the group creates the file and then iterates over all mesh pieces it has.
For each mesh piece, it creates a sub-directory within the file (e.g., a separate namespace for 
the objects). It repeats this process for each mesh piece it has. Then, the first
MPI task closes the file and hands off exclusive access to the next task in the group.
That MPI task opens the file and iterates over all domains in the same way. Exclusive
access to the file is then handed off to the next task. This process continues until
all processors in the group have written their domains to unique sub-directories in the file.

Calls to MACSIO_MIF_WaitForBaton() and MACSIO_MIF_HandOffBaton() handle this handshaking
of file creations and/or opens and bracket blocks of code that are performing MIF I/O.

After all groups have finished with their files, an optional final step may involve creating
a master file which contains special metadata objects that point at all the pieces of
mesh (domains) scattered about in the N files.

A call to MACSIO_MIF_Finalize() frees up resources associated with handling MIF mappings.

The basic coding structure for a MIF I/O operation is as follows. . .

\code
    MACSIO_MIF_baton_t bat = MACSIO_MIF_Init(. . .);
    SomeFileType *fhndl = (SomeFileType *) MACSIO_MIF_WaitForBaton(bat, "GroupName", "ProcName");
        .
        . 
        . 
        < processor's work on the file >
        .
        . 
        . 
    MACSIO_MIF_HandOffBaton(bat, fhndl);
    MACSIO_MIF_Finalize(bat);
\endcode

Setting N to be equal to the number of MPI tasks, results in a file-per-process configuration,
which is typically not recommended. However, some applications do indeed choose to run this
way with good results. Alternatively, setting N equal to 1 results in effectively serializing
the I/O and is also certainly not recommended. For large, parallel runs, there is typicall a
sweet spot in the selection of N which results in peak I/O performance rates. If N is too large,
the I/O subsystem will likely be overwhelmed; setting it too small will likely underutilize
the system resources. This is illustrated
of files and MPI task counts.

\image html Ale3d_io_perf2.png

This approach to scalable, parallel I/O was originally developed in the late 1990s by Rob Neely,
a lead software architect on ALE3D at the time. It and variations thereof have since been adopted
by many codes and used productively through several transitions in orders of magnitude of MPI task
counts from hundreds then to hundreds of thousands today.

There are a large number of advantages to MIF-IO over SSF-IO.
  - MIF-IO enables the use of serial (e.g. non-parallel) I/O libraries. One caveat is that such\n
    libraries do need to provide namespace features (e.g. sub-directories).
  - MIF-IO is a much simpler programming model because it frees developers from having to think\n
    in terms of collective I/O operations. The code to write data from one MPI task doesn't depend\n
    on or involve other tasks. For large multi-physics applications where the size, shape and even\n
    existence of data can vary dramatically among MPI tasks, this is invaluable in simplifying\n
    I/O code development.
  - MIF-IO alleviates any need for global-to-local and local-to-global remapping upon every\n
    exchange of data between the application and its file.
  - Some Data-in-transit (DIT) services (e.g. those that may change size or shape such as\n
    compression) are easier to apply in a MIF-IO setting because processors are freed from\n
    having to coordinate with each other on changes in data size/shape as it is moved to the file.
  - Good performance demands very little in the way of extra/advanced features from the underlying\n
    I/O hardware and filesystem. A relatively dumb filesysem can get it right and perform well.
  - Application controlled throttling of I/O is easily supported in a MIF-IO setting because the\n
    number of concurrent operations is explicitly controlled. This can help to avoid overloading the\n
    underlying I/O subsystems.
  - MIF-IO is consistent with the way leading-edge commercial <em>big data</em> I/O in\n
    map-reduce operations is handled. Data sets are broken into pieces and stored in the\n
    filesystem as a collection of shards and different numbers of parallel tasks can process\n
    different numbers of shards.
@{
*/

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MACSIO_MIF_READ=0x1,
    MACSIO_MIF_WRITE=0x3
} MACSIO_MIF_iomode_t;

typedef struct _MACSIO_MIF_baton_t MACSIO_MIF_baton_t;
typedef void *(*MACSIO_MIF_CreateCB)(const char *fname, const char *nsname, void *udata);
typedef void *(*MACSIO_MIF_OpenCB)  (const char *fname, const char *nsname, MACSIO_MIF_iomode_t iomode, void *udata);
typedef void  (*MACSIO_MIF_CloseCB) (void *file, void *udata);

#ifdef HAVE_MPI
extern MACSIO_MIF_baton_t *MACSIO_MIF_Init(int numFiles, MACSIO_MIF_iomode_t ioMode, MPI_Comm mpiComm, int mpiTag,
    MACSIO_MIF_CreateCB createCb, MACSIO_MIF_OpenCB openCb, MACSIO_MIF_CloseCB closeCb, void *userData);
#endif
extern void   MACSIO_MIF_Finish(MACSIO_MIF_baton_t *bat);
extern void * MACSIO_MIF_WaitForBaton(MACSIO_MIF_baton_t *Bat, const char *fname, const char *nsname);
extern void   MACSIO_MIF_HandOffBaton(const MACSIO_MIF_baton_t *Bat, void *file);
extern int    MACSIO_MIF_RankOfGroup(const MACSIO_MIF_baton_t *Bat, int rankInComm);
extern int    MACSIO_MIF_RankInGroup(const MACSIO_MIF_baton_t *Bat, int rankInComm);

#ifdef __cplusplus
}
#endif

/*!@}*/

#endif /* #ifndef _MACSIO_MIF_H */
