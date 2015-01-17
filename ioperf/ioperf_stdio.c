#include <ifacemap.h>
#include <options.h>
#include <util.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* convenient name mapping macors */
#define FHNDL2(A) IOPFileHandle_ ## A ## _t
#define FHNDL FHNDL2(stdio)
#define FNAME2(FUNC,A) FUNC ## _ ## A
#define FNAME(FUNC) FNAME2(FUNC,stdio)
#define INAME2(A) #A
#define INAME INAME2(stdio)

/* the name you want to assign to the interface */
static char const *iface_name = INAME;
static char const *iface_ext = "dat";

/* The driver's file handle "inherits" from the public handle */
typedef struct FHNDL
{
    IOPFileHandlePublic_t pub;
    FILE *f;
} FHNDL;

static int FNAME(close_file)(struct IOPFileHandle_t *fh, IOPoptlist_t const *moreopts);

static IOPFileHandle_t *make_file_handle(FILE* f)
{
    FHNDL *retval;
    retval = (FHNDL*) calloc(1,sizeof(FHNDL));
    retval->f = f;

    /* populate file, namespace and array methods here */
    retval->pub.closeFileFunc = FNAME(close_file);
#if 0
    retval->pub.syncDataFunc = FNAME(sync_data);
    retval->pub.createNamespaceFunc FNAME(create_namespace);
    retval->pub.setNamespaceFunc FNAME(set_namespace);
    retval->pub.getNamespaceFunc FNAME(get_namespace);
#endif

    return (IOPFileHandle_t*) retval;
}

static IOPFileHandle_t *FNAME(create_file)(char const *pathname, int flags, IOPoptlist_t const *opts)
{
    FILE *file = fopen(pathname, "w+");
    if (!file) return 0;
    return make_file_handle(file);
}

static IOPFileHandle_t *FNAME(open_file)(char const *pathname, int flags, IOPoptlist_t const *opts)
{
    FILE *file = fopen(pathname, "a+");
    if (!file) return 0;
    return make_file_handle(file);
}

static int FNAME(close_file)(struct IOPFileHandle_t *_fh, IOPoptlist_t const *moreopts)
{
    int retval;
    FHNDL *fh = (FHNDL*) _fh;
    retval = fclose(fh->f);
    free(fh);
    return retval;
}

static int register_this_interface()
{
    unsigned int id = bjhash((unsigned char*)iface_name, strlen(iface_name), 0) % MAX_IFACES;
    if (strlen(iface_name) >= MAX_IFACE_NAME)
        IOP_ERROR(("interface name \"%s\" too long",iface_name) , IOP_FATAL);
    if (iface_map[id].slotUsed!= 0)
        IOP_ERROR(("hash collision for interface name \"%s\"",iface_name) , IOP_FATAL);

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