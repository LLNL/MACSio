#include <ifacemap.h>
#include <options.h>
#include <util.h>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* convenient name mapping macors */
#define FHNDL2(A) MACSIO_FileHandle_ ## A ## _t
#define FHNDL FHNDL2(sec2)
#define FNAME2(FUNC,A) FUNC ## _ ## A
#define FNAME(FUNC) FNAME2(FUNC,sec2)
#define INAME2(A) #A
#define INAME INAME2(sec2)

/* the name you want to assign to the interface */
static char const *iface_name = INAME;
static char const *iface_ext = "dat2";

/* The driver's file handle "inherits" from the public handle */
typedef struct FHNDL
{
    MACSIO_FileHandlePublic_t pub;
    int fd;
} FHNDL;

static int FNAME(close_file)(struct MACSIO_FileHandle_t *fh, MACSIO_optlist_t const *moreopts);

static MACSIO_FileHandle_t *make_file_handle(int fd)
{
    FHNDL *retval;
    retval = (FHNDL*) calloc(1,sizeof(FHNDL));
    retval->fd = fd;

    /* populate file, namespace and array methods here */
    retval->pub.closeFileFunc = FNAME(close_file);

    return (MACSIO_FileHandle_t*) retval;
}

static MACSIO_FileHandle_t *FNAME(create_file)(char const *pathname, int flags, MACSIO_optlist_t const *opts)
{
    int fd = creat(pathname, S_IRUSR|S_IWUSR); 
    if (fd < 0) return 0;
#warning USE MACSIO_ ERROR HERE
    return make_file_handle(fd);
}

static MACSIO_FileHandle_t *FNAME(open_file)(char const *pathname, int flags, MACSIO_optlist_t const *opts)
{
    int fd = open(pathname, O_APPEND);
    if (fd < 0) return 0;
#warning USE MACSIO_ ERROR HERE
    return make_file_handle(fd);
}

static int FNAME(close_file)(struct MACSIO_FileHandle_t *_fh, MACSIO_optlist_t const *moreopts)
{
    int retval;
    FHNDL *fh = (FHNDL *) _fh;
    retval = close(fh->fd);
#warning CHECK ERROR HERE
    free(fh);
    return retval;
}

static int register_this_interface()
{
    unsigned int id = bjhash((unsigned char*)iface_name, strlen(iface_name), 0) % MAX_IFACES;
    if (strlen(iface_name) >= MAX_IFACE_NAME)
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
