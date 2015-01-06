#ifndef JSON_EXTPTR_H
#define JSON_EXTPTR_H

#include <json/json.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A 'strptr' json object is just a string representation
 * (e.g. '0xFFFE60A42') of a void pointer */
extern struct json_object * json_object_new_strptr(void *p);
extern void *               json_object_get_strptr(struct json_object *o);

/* An 'extptr' json object is an ensemble of 4 json objects, 
 * (int) datatype, (int) ndims, (array) dims, (strptr) void *,
 * that represent an array of data externally referenced from
 * the json object. */
extern struct json_object * json_object_new_extptr(void *p, int ndims, int const *dims, int datatype);
extern void                 json_object_extptr_delete(struct json_object *jso);

/* Inspect members of an extptr object */
extern int                         json_object_is_extptr(struct json_object *obj);
extern int                  json_object_get_extptr_datatype(struct json_object *obj);
extern int                  json_object_get_extptr_ndims(struct json_object *obj);
extern int                  json_object_get_extptr_dims_idx(struct json_object *obj, int idx);
extern void *               json_object_get_extptr_ptr(struct json_object *obj);
extern int                  json_object_reconstitute_extptrs(struct json_object *obj);

/* Methods to serialize a json object to a binary buffer. Note that the
 * json-c library itself can serialize Silo's json objects to a string using
 * json_object_to_json_string(). */
extern int                  json_object_to_binary_buf(struct json_object *obj, int flags, void **buf, int *len);
extern struct json_object * json_object_from_binary_buf(void *buf, int len);

/* Methods to read/write serial, json object to a file */
extern int                  json_object_to_binary_file(char const *filename, struct json_object *obj);
extern struct json_object * json_object_from_binary_file(char const *filename);

#ifdef __cplusplus
}
#endif

#endif
