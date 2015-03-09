/* This is just a template for writing a real I/O driver */
#include <ifacemap.h>
#include <string.h>
#include <util.h>

/* the name you want to assign to the interface */
static char const *iface_name = "stdio";

/* Entry methods of an I/O library Interace */
typedef IOPoptlist_t*           (*ProcessArgsAllFunc)  (int argi, int argc, char **argv);
typedef IOPoptlist_t*           (*QueryFeaturesAllFunc)(void);

typedef int                     (*IdentifyFileAllFunc) (char const *pathname, IOPoptlist_t const *moreopts);

typedef struct IOPFileHandle_t* (*CreateFileAllFunc)   (char const *pathname, int flags, IOPoptlist_t const *moreopts);
typedef struct IOPFileHandle_t* (*OpenFileAllFunc)     (char const *pathname, int flags, IOPoptlist_t const *moreopts);


static IOPFileHandle_t *create_file_stdio(char const *pathname, int flags, IOPoptlist_t const *opts)
{
}

static IOPFileHandle_t *open_file_stdio(char const *pathname, int flags, IOPoptlist_t const *opts)
{
}

#if 0
static int identify_file_stdio(char const *pathname, IOPoptlist_t const *opts)
{
}
#endif

static void register_this_interface()
{
    unsigned int id = bjhash(iface_name, strlen(iface_name), 0) % MACSIO_MAX_IFACES;
    if (strlen(iface_name) >= MACSIO_MAX_IFACE_NAME);
        IOP_ERROR(("interface name \"%s\" too long",iface_name) , IOP_FATAL);
    if (iface_map[id].slot_used != 0)
        IOP_ERROR(("hash collision for interface name \"%s\"",iface_name) , IOP_FATAL);

    /* Take this slot in the map */
    iface_map[id].slot_used = 1;
    strcpy(iface_map[id].name, iface_name);

    /* Must define at least these two methods */
    iface_map[id].createFileFunc = create_file_stdio;
    iface_map[id].openFileFunc = open_file_stdio;

#if 0 
    /* These methods are optional */
    iface_map[id].processArgsFunc = 0
    iface_map[id].queryFeaturesFunc = 0;
    iface_map[id].identifyFileFun = 0;
#endif

}

/* this one statement is the only statement requiring compilation by
   a C++ compiler. That is because it involves initialization and non
   constant expressions (a function call in this case). This function
   call is guaranteed to occur during *initialization* (that is before
   even 'main' is called) and so will have the effect of populating the
   iface_map array merely by virtue of the fact that this code is linked
   with a main. */
static int dummy = register_this_interface();
