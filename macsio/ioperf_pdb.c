#include <ifacemap.h>
#include <macsio.h>
#include <options.h>
#include <util.h>

#ifdef USING_PDB_LITE
#include <lite_pdb.h> /* will re-map all PDB functions to lite variants */
#else
#include <pdb.h>
#endif

#include <stdlib.h>
#include <string.h>

#ifdef USING_PDB_LITE
#define FHNDL2(A) MACSIO_FileHandle_ ## A ## _t
#define FHNDL FHNDL2(pdblite)
#define FNAME2(FUNC,A) FUNC ## _ ## A
#define FNAME(FUNC) FNAME2(FUNC,pdblite)
#define INAME2(A) #A
#define INAME INAME2(pdblite)
#else
#define FHNDL2(A) MACSIO_FileHandle_ ## A ## _t
#define FHNDL FHNDL2(pdb)
#define FNAME2(FUNC,A) FUNC ## _ ## A
#define FNAME(FUNC) FNAME2(FUNC,pdb)
#define INAME2(A) #A
#define INAME INAME2(pdb)
#endif

/* The driver's file handle "inherits" from the public handle */
typedef struct FHNDL
{
    MACSIO_FileHandlePublic_t pub;
    PDBfile *f;
} FHNDL;

/* the name you want to assign to the interface */
static char const *iface_name = INAME;
static char const *iface_ext = INAME;

static int FNAME(close_file)(struct MACSIO_FileHandle_t *fh, MACSIO_optlist_t const *moreopts);

static MACSIO_FileHandle_t *make_file_handle(PDBfile *f)
{
    FHNDL *retval;
    retval = (FHNDL*) calloc(1,sizeof(FHNDL));
    retval->f = f;

    /* populate file, namespace and array methods here */
    retval->pub.closeFileFunc = FNAME(close_file);

    return (MACSIO_FileHandle_t*) retval;
}

static MACSIO_FileHandle_t *FNAME(create_file)(char const *_pathname, int flags, MACSIO_optlist_t const *opts)
{
    char mode[2] = "w";
    char pathname[1024];
    strcpy(pathname, _pathname);
    PDBfile *f = PD_open(pathname,mode);
    if (!f) return 0;
#warning FIX ERROR CHECKING
    return make_file_handle(f);
}

static MACSIO_FileHandle_t *FNAME(open_file)(char const *_pathname, int flags, MACSIO_optlist_t const *opts)
{
    char mode[2] = "a";
    char pathname[1024];
    strcpy(pathname, _pathname);
    PDBfile *f = PD_open(pathname, mode);
    if (!f) return 0;
#warning FIX ERROR CHECKING
    return make_file_handle(f);
}

static int FNAME(close_file)(struct MACSIO_FileHandle_t *_fh, MACSIO_optlist_t const *moreopts)
{
    int retval;
    FHNDL *fh = (FHNDL*) _fh;
    retval = PD_close(fh->f);
    free(fh);
    return retval;
}

static int register_this_interface()
{
    unsigned int id = bjhash((unsigned char*)iface_name, strlen(iface_name), 0) % MACSIO_MAX_IFACES;
    if (strlen(iface_name) >= MACSIO_MAX_IFACE_NAME)
        MACSIO_ERROR(("interface name \"%s\" too long",iface_name) , MACSIO_FATAL);
    if (iface_map[id].slotUsed!= 0)
        MACSIO_ERROR(("hash collision for interface name \"%s\"",iface_name) , MACSIO_FATAL);

    /* Take this slot in the map */
    iface_map[id].slotUsed = 1;
    strcpy(iface_map[id].name, iface_name);
    strcpy(iface_map[id].ext, iface_ext);

    /* Must define at least these two methods */
    iface_map[id].createFileFunc = FNAME(create_file);
    iface_map[id].openFileFunc = FNAME(open_file);

    return 0;
}

/* this one statement is the only statement requiring compilation by
   a C++ compiler. That is because it involves initialization and non
   constant expressions (a function call in this case). This function
   call is guaranteed to occur during *initialization* (that is before
   even 'main' is called) and so will have the effect of populating the
   iface_map array merely by virtue of the fact that this code is linked
   with a main. */
static int dummy = register_this_interface();









#if 0
static int Write_pdb(void *buf, size_t nbytes)
{
    int status;
    char dsname[256];
    static int n = 0;
    sprintf(dsname, "data_%07d(%d)", n++, nbytes/sizeof(double));
    status = PD_write(pdbfile, dsname, "double", buf);
    if (status) return nbytes;
    return 0;
}

static int Read_pdb(void *buf, size_t nbytes)
{
    return 0;
}
#endif
