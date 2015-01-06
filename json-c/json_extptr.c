#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <json_extpr.h>
#include <json/json.h>
#include <json/json_object_private.h>
#include <json/printbuf.h>

#define EXTPTR_HDRSTR "\":{\"ptr\":\"0x"

static void indent(struct printbuf *pb, int level, int flags)
{
    if (flags & JSON_C_TO_STRING_PRETTY)
    {
        printbuf_memset(pb, -1, ' ', level * 2);
    }
}

static int json_object_object_length(struct json_object *o)
{
    struct lh_table *t = json_object_get_object(o);
    return t->count;
}

static int json_object_object_get_member_count(struct json_object *o)
{
    int n = 0;
    struct json_object_iter jiter;
    json_object_object_foreachC(o, jiter)
    {
        n++;
    }
    return n;
}

static void json_object_set_serializer(json_object *jso,
    json_object_to_json_string_fn to_string_func,
    void * userdata, json_object_delete_fn * user_delete)
{
    jso->_to_json_string = to_string_func;
}

static void json_object_set_deleter(json_object *jso,
    json_object_delete_fn delete_func)
{
    jso->_delete = delete_func;
}

void
json_object_extptr_delete(struct json_object *jso)
{
    void *extptr = json_object_get_extptr_ptr(jso);
    if (extptr) free(extptr);
    json_object_put(jso);
}

static int
json_object_extptr_to_json_string(struct json_object* jso,
    struct printbuf *pb, int level, int flags)
{
        int had_children = 0;
        int ii;
        char *p;
        int datatype, nvals, ndims, dims[32];
        struct json_object *darr, *subobj;
        int retval;

        sprintbuf(pb, "{\"ptr\":%s", json_object_to_json_string(json_object_object_get(jso, "ptr")));
        sprintbuf(pb, ",\"datatype\":%s", json_object_to_json_string(json_object_object_get(jso, "datatype")));
        sprintbuf(pb, ",\"ndims\":%s", json_object_to_json_string(json_object_object_get(jso, "ndims")));
        sprintbuf(pb, ",\"dims\":%s", json_object_to_json_string(json_object_object_get(jso, "dims")));
 
        p = (char*) json_object_get_strptr(json_object_object_get(jso, "ptr"));
        datatype = json_object_get_int(json_object_object_get(jso, "datatype"));
        ndims = json_object_get_int(json_object_object_get(jso, "ndims"));
        darr = json_object_object_get(jso, "dims");
        nvals = 1;
        for (ii = 0; ii < ndims; ii++)
        {
            dims[ii] = json_object_get_int(json_object_array_get_idx(darr, ii));
            nvals *= dims[ii];
        }

        if (flags & JSON_C_TO_STRING_EXTPTR_AS_BINARY)
        {
            sprintbuf(pb, ",\"nvals\":%d", nvals);
            sprintbuf(pb, ",\"data\":[");
            printbuf_memappend(pb, p, nvals*db_GetMachDataSize(datatype));
        }
        else if (flags & JSON_C_TO_STRING_EXTPTR_SKIP)
        {
            return sprintbuf(pb, "}");
        }
        else
        {

            sprintbuf(pb, ",\"data\":[");
            if (flags & JSON_C_TO_STRING_PRETTY)
                    sprintbuf(pb, "\n");

            for(ii=0; ii < nvals; ii++)
            {
                    struct json_object *val;
                    if (had_children)
                    {
                            sprintbuf(pb, ",");
                            if (flags & JSON_C_TO_STRING_PRETTY)
                                    sprintbuf(pb, "\n");
                    }
                    had_children = 1;
                    if (flags & JSON_C_TO_STRING_SPACED)
                            sprintbuf(pb, " ");
                    indent(pb, level + 1, flags);

                    switch (datatype)
                    {
                        case DB_CHAR:
                        {
                            sprintbuf(pb, "%c", *((char*)p));
                            p += sizeof(char);
                            break;
                        }
                        case DB_SHORT:
                        {
                            sprintbuf(pb, "%hd", *((short*)p));
                            p += sizeof(short);
                            break;
                        }
                        case DB_INT:
                        {
                            sprintbuf(pb, "%d", *((int*)p));
                            p += sizeof(int);
                            break;
                        }
                        case DB_LONG:
                        {
                            sprintbuf(pb, "%ld", *((long*)p));
                            p += sizeof(long);
                            break;
                        }
                        case DB_LONG_LONG:
                        {
                            sprintbuf(pb, "%lld", *((long long*)p));
                            p += sizeof(long long);
                            break;
                        }
                        case DB_FLOAT:
                        {
                            sprintbuf(pb, "%f", *((float*)p));
                            p += sizeof(float);
                            break;
                        }
                        case DB_DOUBLE:
                        {
                            sprintbuf(pb, "%f", *((double*)p));
                            p += sizeof(double);
                            break;
                        }
                    }
            }
        }
        if (flags & JSON_C_TO_STRING_PRETTY)
        {
                if (had_children)
                        sprintbuf(pb, "\n");
                indent(pb,level,flags);
        }

        if (flags & JSON_C_TO_STRING_SPACED)
                retval = sprintbuf(pb, " ]}");
        else
                retval = sprintbuf(pb, "]}");

        return retval;
}

struct json_object *
json_object_new_strptr(void *p)
{
    static char tmp[32];
    if (sizeof(p) == sizeof(unsigned))
        snprintf(tmp, sizeof(tmp), "0x%016x", (unsigned) p);
    else if (sizeof(p) == sizeof(unsigned long))
        snprintf(tmp, sizeof(tmp), "0x%016lx", (unsigned long) p);
    else if (sizeof(p) == sizeof(unsigned long long))
        snprintf(tmp, sizeof(tmp), "0x%016llx", (unsigned long long) p);

    return json_object_new_string(tmp);
}

void *
json_object_get_strptr(struct json_object *o)
{
    void *p;
    char const *strptr = json_object_get_string(o);
    if (sscanf(strptr, "%p", &p) == 1)
        return p;
    else
        return 0;
}

struct json_object *
json_object_new_extptr(void *p, int ndims, int const *dims, int datatype)
{
    int i;
    struct json_object *jobj = json_object_new_object();
    struct json_object *jarr = json_object_new_array();

    json_object_set_serializer(jobj, json_object_extptr_to_json_string, 0, 0);
    json_object_set_deleter(jobj, json_object_extptr_delete); 

    for (i = 0; i < ndims; i++)
        json_object_array_add(jarr, json_object_new_int(dims[i]));

    json_object_object_add(jobj, "ptr", json_object_new_strptr(p));
    json_object_object_add(jobj, "datatype", json_object_new_int(datatype));
    json_object_object_add(jobj, "ndims", json_object_new_int(ndims));
    json_object_object_add(jobj, "dims", jarr);

    return jobj;
}

int
json_object_is_extptr(struct json_object *obj)
{
    if (json_object_object_get_ex(obj, "ptr", 0) &&
        json_object_object_get_ex(obj, "datatype", 0) &&
        json_object_object_get_ex(obj, "ndims", 0) &&
        json_object_object_get_ex(obj, "dims", 0))
        return 1;
    return 0;
}

int
json_object_get_extptr_ndims(struct json_object *obj)
{
    struct json_object *sobj = 0;
    if (json_object_object_get_ex(obj, "ptr", 0) &&
        json_object_object_get_ex(obj, "datatype", 0) &&
        json_object_object_get_ex(obj, "ndims", &sobj) &&
        json_object_object_get_ex(obj, "dims", 0) && sobj)
        return json_object_get_int(sobj);
    return -1;
}

int
json_object_get_extptr_datatype(struct json_object *obj)
{
    struct json_object *sobj = 0;
    if (json_object_object_get_ex(obj, "ptr", 0) &&
        json_object_object_get_ex(obj, "datatype", &sobj) &&
        json_object_object_get_ex(obj, "ndims", 0) &&
        json_object_object_get_ex(obj, "dims", 0) && sobj)
        return json_object_get_int(sobj);
    return -1;
}

int
json_object_get_extptr_dims_idx(struct json_object *obj, int idx)
{
    struct json_object *sobj = 0;
    if (json_object_object_get_ex(obj, "ptr", 0) &&
        json_object_object_get_ex(obj, "datatype", 0) &&
        json_object_object_get_ex(obj, "ndims", 0) &&
        json_object_object_get_ex(obj, "dims", &sobj) && sobj)
        return json_object_get_int(json_object_array_get_idx(sobj, idx));
    return 0;
}

void *
json_object_get_extptr_ptr(struct json_object *obj)
{
    struct json_object *sobj = 0;
    if (json_object_object_get_ex(obj, "ptr", &sobj) &&
        json_object_object_get_ex(obj, "datatype", 0) &&
        json_object_object_get_ex(obj, "ndims", 0) &&
        json_object_object_get_ex(obj, "dims", 0) && sobj)
        return json_object_get_strptr(sobj);
    return 0;
}

int
json_object_to_binary_buf(struct json_object *obj, int flags, void **buf, int *len)
{
    char const *pjhdr;
    struct printbuf *pb = printbuf_new();

    /* first, stringify the json object as normal but skipping extptr data */
    char const *jhdr = json_object_to_json_string_ext(obj, flags|JSON_C_TO_STRING_EXTPTR_SKIP);
    sprintbuf(pb, "%s", jhdr);
    sprintbuf(pb, "%c", '\0'); /* so header is null-terminated */

    /* now, handle all extptr objects by appending their binary data onto the end,
       and over-writing the "ptr" value with offset within the buffer */
    pjhdr = jhdr;
    while (*pjhdr)
    {
        if (!strncmp(pjhdr, EXTPTR_HDRSTR, sizeof(EXTPTR_HDRSTR)-1))
        {
            char tmp[64];
            int pblen;
            struct json_object *extptr_obj = json_tokener_parse(pjhdr+2); /* walk past '":' */
            void *p = json_object_get_strptr(json_object_object_get(extptr_obj, "ptr"));
            int datatype = json_object_get_int(json_object_object_get(extptr_obj, "datatype"));
            int ndims = json_object_get_int(json_object_object_get(extptr_obj, "ndims"));
            struct json_object *darr = json_object_object_get(extptr_obj, "dims");
            int ii, nvals = 1;
            for (ii = 0; ii < ndims; ii++)
                nvals *= json_object_get_int(json_object_array_get_idx(darr, ii));
            pblen = printbuf_length(pb);
            printbuf_memappend_fast(pb, p, nvals*db_GetMachDataSize(datatype));
            /* Use of a hexadecimal value works ok here because a scanf can read it */
            sprintf(tmp,"%-.16x",pblen); /* overwrite ptr value w/buffer-offset */
            memcpy(pb->buf + (pjhdr+12-jhdr),tmp,strlen(tmp)); /* overwrite ptr value w/buffer-offset */
            pjhdr += sizeof(EXTPTR_HDRSTR);
            json_object_put(extptr_obj);
        }
        pjhdr++;
    }
    if (len) *len = printbuf_length(pb);
    if (buf) *buf = pb->buf;
    free(pb);
    return 0;
}

static void
json_object_from_binary_buf_recurse(struct json_object *jso, void *buf)
{
    /* first, reconstitute the header */
    struct json_object_iter iter;

    json_object_object_foreachC(jso, iter)
    {
        json_type jtype = json_object_get_type(iter.val);
        if (jtype == json_type_object)
        {
            if (json_object_object_get_ex(iter.val, "ptr", 0) &&
                json_object_object_get_ex(iter.val, "datatype", 0) &&
                json_object_object_get_ex(iter.val, "ndims", 0) &&
                json_object_object_get_ex(iter.val, "dims", 0))
            {
                char strptr[128];
                void *p;
                int i, offset, nvals=1;
                char const *offstr = json_object_get_string(json_object_object_get(iter.val, "ptr"));
                int datatype = json_object_get_int(json_object_object_get(iter.val, "datatype"));
                int ndims = json_object_get_int(json_object_object_get(iter.val, "ndims"));
                struct json_object *darr = json_object_object_get(iter.val, "dims");
                for (i = 0; i < ndims; i++)
                    nvals *= json_object_get_int(json_object_array_get_idx(darr, i));
                sscanf(offstr, "%x", &offset);
                p = malloc(nvals*db_GetMachDataSize(datatype));
                memcpy(p, buf+offset, nvals*db_GetMachDataSize(datatype));
                json_object_object_del(iter.val,"ptr");
                snprintf(strptr, sizeof(strptr), "%p", p);
                json_object_object_add(iter.val, "ptr", json_object_new_string(strptr));
            }
            else
            {
                json_object_from_binary_buf_recurse(iter.val, buf);
            }
        }
    }
}

struct json_object *
json_object_from_binary_buf(void *buf, int len)
{
    struct json_object *retval = json_tokener_parse((char*)buf);
    json_object_from_binary_buf_recurse(retval, buf);
    return retval;
}

struct json_object *
json_object_from_binary_file(char const *filename)
{
    struct json_object *retval;
    void *buf;
    int fd;

#ifndef SIZEOF_OFF64_T
#error missing definition for SIZEOF_OFF64_T in silo_private.h
#else
#if SIZEOF_OFF64_T > 4
    struct stat64 s;
#else
    struct stat s;
#endif
#endif

    errno = 0;
    memset(&s, 0, sizeof(s));

#if SIZEOF_OFF64_T > 4
    if (stat64(filename, &s) != 0 || errno != 0)
        return 0;
#else
    if (stat(filename, &s) != 0 || errno != 0)
        return 0;
#endif

    fd = open(filename, O_RDONLY);
    if (fd < 0)
        return 0;
    buf = malloc(s.st_size);
    if (read(fd, buf, (size_t) s.st_size) != (ssize_t) s.st_size)
    {
        free(buf);
        return 0;
    }
    close(fd);
    retval = json_object_from_binary_buf(buf, (int) s.st_size);
    free(buf);

    return retval;
}

int
json_object_to_binary_file(char const *filename, struct json_object *obj)
{
    void *buf; int len; int fd;

    json_object_to_binary_buf(obj, 0, &buf, &len);
    fd = open(filename, O_CREAT|O_TRUNC|O_WRONLY, S_IRUSR|S_IWUSR);
    write(fd, buf, len);
    close(fd);
    free(buf);
    return 0;
}

int
json_object_reconstitute_extptrs(struct json_object *obj)
{
    struct json_object_iter jiter;
    struct json_object *parent_jobjs_with_extptr_members[1000];
    char const *extptr_member_keys[1000];
    int k, num_to_remove = 0;

    json_object_object_foreachC(obj, jiter)
    {
        struct json_object *ptr_obj, *datatype_obj, *ndims_obj, *dims_obj, *data_obj;

        struct json_object *mobj = jiter.val;
        char const *mname = jiter.key;

        if (json_object_get_type(mobj) != json_type_object)
            continue;

        if (!(json_object_object_get_ex(mobj, "ptr", &ptr_obj ) &&
              json_object_object_get_ex(mobj, "datatype", &datatype_obj) &&
              json_object_object_get_ex(mobj, "ndims", &ndims_obj) &&
              json_object_object_get_ex(mobj, "dims", &dims_obj) &&
              json_object_object_get_ex(mobj, "data", &data_obj)))
        {
            json_object_reconstitute_extptrs(mobj);
            continue;
        }

        if (!(json_object_get_type(ptr_obj) == json_type_string &&
              json_object_get_type(datatype_obj) == json_type_int &&
              json_object_get_type(ndims_obj) == json_type_int &&
              json_object_get_type(dims_obj) == json_type_array &&
              json_object_get_type(data_obj) == json_type_array))
        {
            json_object_reconstitute_extptrs(mobj);
            continue;
        }

        if (json_object_object_get_member_count(mobj) != 5)
            continue;

        extptr_member_keys[num_to_remove] = mname;
        parent_jobjs_with_extptr_members[num_to_remove] = obj;
        num_to_remove++;
    }

    for (k = 0; k < num_to_remove; k++)
    {
        struct json_object *ptr_obj, *datatype_obj, *ndims_obj, *dims_obj, *data_obj;
        char *mname = strdup(extptr_member_keys[k]);
        struct json_object *pobj = parent_jobjs_with_extptr_members[k];
        struct json_object *extptr_obj;

        json_object_object_get_ex(pobj, mname, &extptr_obj);
        json_object_object_get_ex(extptr_obj, "ptr", &ptr_obj );
        json_object_object_get_ex(extptr_obj, "datatype", &datatype_obj);
        json_object_object_get_ex(extptr_obj, "ndims", &ndims_obj);
        json_object_object_get_ex(extptr_obj, "dims", &dims_obj);
        json_object_object_get_ex(extptr_obj, "data", &data_obj);

        /* We're at an extptr object that was serialized to a 'standard' json string.
         * So, lets reconstitute it. */
        {
            int i, n = 1;
            int datatype = json_object_get_int(datatype_obj);
            int ndims = json_object_get_int(ndims_obj);
            int *dims = (int *) malloc(ndims * sizeof(int));
            int jdtype = json_object_get_type(json_object_array_get_idx(data_obj, 0));
            char *p;
            void *pdata;

            /* get the array dimension sizes */
            for (i = 0; i < ndims; i++)
            {
                dims[i] = json_object_get_int(json_object_array_get_idx(dims_obj, i));
                n *= dims[i];
            }

            /* get the array data */
            pdata = (void *) malloc(n * db_GetMachDataSize(datatype));
            for (i = 0, p = pdata; i < n; i++, p += db_GetMachDataSize(datatype))
            {
                switch (jdtype)
                {
                    case json_type_int:
                    {
                        int ival = json_object_get_int(json_object_array_get_idx(data_obj, i));
                        if (datatype == DB_CHAR)
                            *((char*)p) = (unsigned char) ival;
                        else if (datatype == DB_SHORT)
                            *((short*)p) = (short) ival;
                        else if (datatype == DB_INT)
                            *((int*)p) = ival;
                        else if (datatype == DB_LONG)
                            *((long*)p) = (long) ival;
                        break;
                    }
                    case json_type_double:
                    {
                        double dval = json_object_get_double(json_object_array_get_idx(data_obj, i));
                        if (datatype == DB_DOUBLE)
                            *((double*)p) = dval;
                        else
                            *((float*)p) = (float) dval;
                        break;
                    }
                }
            }
                    
            /* delete the old object */
            json_object_object_del(pobj, mname);

            /* Add the reconstituted extptr object */
            json_object_object_add(pobj, mname,
                json_object_new_extptr(pdata, ndims, dims, datatype));

            free(mname);
        }
    }

    return 0;
}
