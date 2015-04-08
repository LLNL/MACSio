/*
 * $Id: json_object.c,v 1.17 2006/07/25 03:24:50 mclark Exp $
 *
 * Copyright (c) 2004, 2005 Metaparadigm Pte. Ltd.
 * Michael Clark <michael@metaparadigm.com>
 * Copyright (c) 2009 Hewlett-Packard Development Company, L.P.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the MIT license. See COPYING for details.
 *
 */

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "printbuf.h"
#include "linkhash.h"
#include "arraylist.h"
#include "json_inttypes.h"
#include "json_object.h"
#include "json_object_private.h"
#include "json_util.h"
#include "math_compat.h"

#if !defined(HAVE_STRDUP) && defined(_MSC_VER)
  /* MSC has the version as _strdup */
# define strdup _strdup
#elif !defined(HAVE_STRDUP)
# error You do not have strdup on your system.
#endif /* HAVE_STRDUP */

#if !defined(HAVE_SNPRINTF) && defined(_MSC_VER)
  /* MSC has the version as _snprintf */
# define snprintf _snprintf
#elif !defined(HAVE_SNPRINTF)
# error You do not have snprintf on your system.
#endif /* HAVE_SNPRINTF */

// Don't define this.  It's not thread-safe.
/* #define REFCOUNT_DEBUG 1 */

const char *json_number_chars = "0123456789.+-eE";
const char *json_hex_chars = "0123456789abcdefABCDEF";

static void json_object_generic_delete(struct json_object* jso);
static struct json_object* json_object_new(enum json_type o_type);

static json_object_to_json_string_fn json_object_object_to_json_string;
static json_object_to_json_string_fn json_object_boolean_to_json_string;
static json_object_to_json_string_fn json_object_int_to_json_string;
static json_object_to_json_string_fn json_object_double_to_json_string;
static json_object_to_json_string_fn json_object_string_to_json_string;
static json_object_to_json_string_fn json_object_array_to_json_string;
static json_object_to_json_string_fn json_object_extarr_to_json_string;
static json_object_to_json_string_fn json_object_enum_to_json_string;


/* ref count debugging */

#ifdef REFCOUNT_DEBUG

static struct lh_table *json_object_table;

static void json_object_init(void) __attribute__ ((constructor));
static void json_object_init(void) {
  MC_DEBUG("json_object_init: creating object table\n");
  json_object_table = lh_kptr_table_new(128, "json_object_table", NULL);
}

static void json_object_fini(void) __attribute__ ((destructor));
static void json_object_fini(void) {
  struct lh_entry *ent;
  if(MC_GET_DEBUG()) {
    if (json_object_table->count) {
      MC_DEBUG("json_object_fini: %d referenced objects at exit\n",
  	       json_object_table->count);
      lh_foreach(json_object_table, ent) {
        struct json_object* obj = (struct json_object*)ent->v;
        MC_DEBUG("\t%s:%p\n", json_type_to_name(obj->o_type), obj);
      }
    }
  }
  MC_DEBUG("json_object_fini: freeing object table\n");
  lh_table_free(json_object_table);
}
#endif /* REFCOUNT_DEBUG */


/* string escaping */

static int json_escape_str(struct printbuf *pb, char *str, int len)
{
  int pos = 0, start_offset = 0;
  unsigned char c;
  while (len--) {
    c = str[pos];
    switch(c) {
    case '\b':
    case '\n':
    case '\r':
    case '\t':
    case '\f':
    case '"':
    case '\\':
    case '/':
      if(pos - start_offset > 0)
	printbuf_memappend(pb, str + start_offset, pos - start_offset);
      if(c == '\b') printbuf_memappend(pb, "\\b", 2);
      else if(c == '\n') printbuf_memappend(pb, "\\n", 2);
      else if(c == '\r') printbuf_memappend(pb, "\\r", 2);
      else if(c == '\t') printbuf_memappend(pb, "\\t", 2);
      else if(c == '\f') printbuf_memappend(pb, "\\f", 2);
      else if(c == '"') printbuf_memappend(pb, "\\\"", 2);
      else if(c == '\\') printbuf_memappend(pb, "\\\\", 2);
      else if(c == '/') printbuf_memappend(pb, "\\/", 2);
      start_offset = ++pos;
      break;
    default:
      if(c < ' ') {
	if(pos - start_offset > 0)
	  printbuf_memappend(pb, str + start_offset, pos - start_offset);
	sprintbuf(pb, "\\u00%c%c",
		  json_hex_chars[c >> 4],
		  json_hex_chars[c & 0xf]);
	start_offset = ++pos;
      } else pos++;
    }
  }
  if(pos - start_offset > 0)
    printbuf_memappend(pb, str + start_offset, pos - start_offset);
  return 0;
}


/* reference counting */

extern struct json_object* json_object_get(struct json_object *jso)
{
  if(jso) {
    jso->_ref_count++;
  }
  return jso;
}

int json_object_put(struct json_object *jso)
{
	if(jso)
	{
		jso->_ref_count--;
		if(!jso->_ref_count)
		{
			if (jso->_user_delete)
				jso->_user_delete(jso, jso->_userdata);
			jso->_delete(jso);
			return 1;
		}
	}
	return 0;
}


/* generic object construction and destruction parts */

static void json_object_generic_delete(struct json_object* jso)
{
#ifdef REFCOUNT_DEBUG
  MC_DEBUG("json_object_delete_%s: %p\n",
	   json_type_to_name(jso->o_type), jso);
  lh_table_delete(json_object_table, jso);
#endif /* REFCOUNT_DEBUG */
  printbuf_free(jso->_pb);
  free(jso);
}

static struct json_object* json_object_new(enum json_type o_type)
{
  struct json_object *jso;

  jso = (struct json_object*)calloc(sizeof(struct json_object), 1);
  if(!jso) return NULL;
  jso->o_type = o_type;
  jso->_ref_count = 1;
  jso->_delete = &json_object_generic_delete;
#ifdef REFCOUNT_DEBUG
  lh_table_insert(json_object_table, jso, jso);
  MC_DEBUG("json_object_new_%s: %p\n", json_type_to_name(jso->o_type), jso);
#endif /* REFCOUNT_DEBUG */
  return jso;
}


/* type checking functions */

int json_object_is_type(struct json_object *jso, enum json_type type)
{
  if (!jso)
    return (type == json_type_null);
  return (jso->o_type == type);
}

enum json_type json_object_get_type(struct json_object *jso)
{
  if (!jso)
    return json_type_null;
  return jso->o_type;
}

/* set a custom conversion to string */

void json_object_set_serializer(json_object *jso,
	json_object_to_json_string_fn to_string_func,
	void *userdata,
	json_object_delete_fn *user_delete)
{
	// First, clean up any previously existing user info
	if (jso->_user_delete)
	{
		jso->_user_delete(jso, jso->_userdata);
	}
	jso->_userdata = NULL;
	jso->_user_delete = NULL;

	if (to_string_func == NULL)
	{
		// Reset to the standard serialization function
		switch(jso->o_type)
		{
		case json_type_null:
			jso->_to_json_string = NULL;
			break;
		case json_type_boolean:
			jso->_to_json_string = &json_object_boolean_to_json_string;
			break;
		case json_type_double:
			jso->_to_json_string = &json_object_double_to_json_string;
			break;
		case json_type_int:
			jso->_to_json_string = &json_object_int_to_json_string;
			break;
		case json_type_object:
			jso->_to_json_string = &json_object_object_to_json_string;
			break;
		case json_type_array:
			jso->_to_json_string = &json_object_array_to_json_string;
			break;
		case json_type_string:
			jso->_to_json_string = &json_object_string_to_json_string;
			break;
		case json_type_extarr:
			jso->_to_json_string = &json_object_extarr_to_json_string;
			break;
		case json_type_enum:
			jso->_to_json_string = &json_object_enum_to_json_string;
			break;
		}
		return;
	}

	jso->_to_json_string = to_string_func;
	jso->_userdata = userdata;
	jso->_user_delete = user_delete;
}


/* extended conversion to string */

const char* json_object_to_json_string_ext(struct json_object *jso, int flags)
{
	if (!jso)
		return "null";

	if ((!jso->_pb) && !(jso->_pb = printbuf_new()))
		return NULL;

	printbuf_reset(jso->_pb);

	if(jso->_to_json_string(jso, jso->_pb, 0, flags) < 0)
		return NULL;

	return jso->_pb->buf;
}

/* backwards-compatible conversion to string */

const char* json_object_to_json_string(struct json_object *jso)
{
	return json_object_to_json_string_ext(jso, JSON_C_TO_STRING_SPACED);
}

static void indent(struct printbuf *pb, int level, int flags)
{
	if (flags & JSON_C_TO_STRING_PRETTY)
	{
		printbuf_memset(pb, -1, ' ', level * 2);
	}
}

/* json_object_object */

static int json_object_object_to_json_string(struct json_object* jso,
					     struct printbuf *pb,
					     int level,
						 int flags)
{
	int had_children = 0;
	struct json_object_iter iter;

	sprintbuf(pb, "{" /*}*/);
	if (flags & JSON_C_TO_STRING_PRETTY)
		sprintbuf(pb, "\n");
	json_object_object_foreachC(jso, iter)
	{
		if (had_children)
		{
			sprintbuf(pb, ",");
			if (flags & JSON_C_TO_STRING_PRETTY)
				sprintbuf(pb, "\n");
		}
		had_children = 1;
		if (flags & JSON_C_TO_STRING_SPACED)
			sprintbuf(pb, " ");
		indent(pb, level+1, flags);
		sprintbuf(pb, "\"");
		json_escape_str(pb, iter.key, strlen(iter.key));
		if (flags & JSON_C_TO_STRING_SPACED)
			sprintbuf(pb, "\": ");
		else
			sprintbuf(pb, "\":");
		if(iter.val == NULL)
			sprintbuf(pb, "null");
		else
			iter.val->_to_json_string(iter.val, pb, level+1,flags);
	}
	if (flags & JSON_C_TO_STRING_PRETTY)
	{
		if (had_children)
			sprintbuf(pb, "\n");
		indent(pb,level,flags);
	}
	if (flags & JSON_C_TO_STRING_SPACED)
		return sprintbuf(pb, /*{*/ " }");
	else
		return sprintbuf(pb, /*{*/ "}");
}


static void json_object_lh_entry_free(struct lh_entry *ent)
{
  free(ent->k);
  json_object_put((struct json_object*)ent->v);
}

static void json_object_object_delete(struct json_object* jso)
{
  lh_table_free(jso->o.c_object);
  json_object_generic_delete(jso);
}

struct json_object* json_object_new_object(void)
{
  struct json_object *jso = json_object_new(json_type_object);
  if(!jso) return NULL;
  jso->_delete = &json_object_object_delete;
  jso->_to_json_string = &json_object_object_to_json_string;
  jso->o.c_object = lh_kchar_table_new(JSON_OBJECT_DEF_HASH_ENTRIES,
					NULL, &json_object_lh_entry_free);
  return jso;
}

struct lh_table* json_object_get_object(struct json_object *jso)
{
  if(!jso) return NULL;
  switch(jso->o_type) {
  case json_type_object:
    return jso->o.c_object;
  default:
    return NULL;
  }
}

void json_object_object_add(struct json_object* jso, const char *key,
			    struct json_object *val)
{
	// We lookup the entry and replace the value, rather than just deleting
	// and re-adding it, so the existing key remains valid.
	json_object *existing_value = NULL;
	struct lh_entry *existing_entry;
	existing_entry = lh_table_lookup_entry(jso->o.c_object, (void*)key);
	if (!existing_entry)
	{
		lh_table_insert(jso->o.c_object, strdup(key), val);
		return;
	}
	existing_value = (json_object *)existing_entry->v;
	if (existing_value)
		json_object_put(existing_value);
	existing_entry->v = val;
}

int json_object_object_length(struct json_object *jso)
{
	return lh_table_length(jso->o.c_object);
}

struct json_object* json_object_object_get(struct json_object* jso, const char *key)
{
	struct json_object *result = NULL;
	json_object_object_get_ex(jso, key, &result);
	return result;
}

json_bool json_object_object_get_ex(struct json_object* jso, const char *key, struct json_object **value)
{
	if (value != NULL)
		*value = NULL;

	if (NULL == jso)
		return JSON_C_FALSE;

	switch(jso->o_type)
	{
	case json_type_object:
		return lh_table_lookup_ex(jso->o.c_object, (void*)key, (void**)value);
	default:
		if (value != NULL)
			*value = NULL;
		return JSON_C_FALSE;
	}
}

void json_object_object_del(struct json_object* jso, const char *key)
{
	lh_table_delete(jso->o.c_object, key);
}


/* json_object_boolean */

static int json_object_boolean_to_json_string(struct json_object* jso,
					      struct printbuf *pb,
					      int level,
						  int flags)
{
  if(jso->o.c_boolean) return sprintbuf(pb, "true");
  else return sprintbuf(pb, "false");
}

struct json_object* json_object_new_boolean(json_bool b)
{
  struct json_object *jso = json_object_new(json_type_boolean);
  if(!jso) return NULL;
  jso->_to_json_string = &json_object_boolean_to_json_string;
  jso->o.c_boolean = b;
  return jso;
}

json_bool json_object_get_boolean(struct json_object *jso)
{
  if(!jso) return JSON_C_FALSE;
  switch(jso->o_type) {
  case json_type_boolean:
    return jso->o.c_boolean;
  case json_type_int:
    return (jso->o.c_int64 != 0);
  case json_type_double:
    return (jso->o.c_double != 0);
  case json_type_string:
    return (jso->o.c_string.len != 0);
  default:
    return JSON_C_FALSE;
  }
}


/* json_object_int */

static int json_object_int_to_json_string(struct json_object* jso,
					  struct printbuf *pb,
					  int level,
					  int flags)
{
  return sprintbuf(pb, "%"PRId64, jso->o.c_int64);
}

struct json_object* json_object_new_int(int32_t i)
{
  struct json_object *jso = json_object_new(json_type_int);
  if(!jso) return NULL;
  jso->_to_json_string = &json_object_int_to_json_string;
  jso->o.c_int64 = i;
  return jso;
}

int32_t json_object_get_int(struct json_object *jso)
{
  int64_t cint64;
  enum json_type o_type;

  if(!jso) return 0;

  o_type = jso->o_type;
  cint64 = jso->o.c_int64;

  if (o_type == json_type_string)
  {
	/*
	 * Parse strings into 64-bit numbers, then use the
	 * 64-to-32-bit number handling below.
	 */
	if (json_parse_int64(jso->o.c_string.str, &cint64) != 0)
		return 0; /* whoops, it didn't work. */
	o_type = json_type_int;
  }

  switch(o_type) {
  case json_type_int:
	/* Make sure we return the correct values for out of range numbers. */
#ifdef INT32_MIN
	if (cint64 <= INT32_MIN)
		return INT32_MIN;
	else if (cint64 >= INT32_MAX)
		return INT32_MAX;
#else
	if (cint64 <= INT_MIN)
		return INT_MIN;
	else if (cint64 >= INT_MAX)
		return INT_MAX;
#endif
	else
		return (int32_t)cint64;
  case json_type_double:
    return (int32_t)jso->o.c_double;
  case json_type_boolean:
    return jso->o.c_boolean;
  default:
    return 0;
  }
}

struct json_object* json_object_new_int64(int64_t i)
{
  struct json_object *jso = json_object_new(json_type_int);
  if(!jso) return NULL;
  jso->_to_json_string = &json_object_int_to_json_string;
  jso->o.c_int64 = i;
  return jso;
}

int64_t json_object_get_int64(struct json_object *jso)
{
   int64_t cint;

  if(!jso) return 0;
  switch(jso->o_type) {
  case json_type_int:
    return jso->o.c_int64;
  case json_type_double:
    return (int64_t)jso->o.c_double;
  case json_type_boolean:
    return jso->o.c_boolean;
  case json_type_string:
	if (json_parse_int64(jso->o.c_string.str, &cint) == 0) return cint;
  default:
    return 0;
  }
}


/* json_object_double */

static int json_object_double_to_json_string(struct json_object* jso,
					     struct printbuf *pb,
					     int level,
						 int flags)
{
  char buf[128], *p, *q;
  int size;
  /* Although JSON RFC does not support
     NaN or Infinity as numeric values
     ECMA 262 section 9.8.1 defines
     how to handle these cases as strings */
  if(isnan(jso->o.c_double))
    size = snprintf(buf, sizeof(buf), "NaN");
  else if(isinf(jso->o.c_double))
    if(jso->o.c_double > 0)
      size = snprintf(buf, sizeof(buf), "Infinity");
    else
      size = snprintf(buf, sizeof(buf), "-Infinity");
  else
    size = snprintf(buf, sizeof(buf), "%.17g", jso->o.c_double);

  p = strchr(buf, ',');
  if (p) {
    *p = '.';
  } else {
    p = strchr(buf, '.');
  }
  if (p && (flags & JSON_C_TO_STRING_NOZERO)) {
    /* last useful digit, always keep 1 zero */
    p++;
    for (q=p ; *q ; q++) {
      if (*q!='0') p=q;
    }
    /* drop trailing zeroes */
    *(++p) = 0;
    size = p-buf;
  }
  printbuf_memappend(pb, buf, size);
  return size;
}

struct json_object* json_object_new_double(double d)
{
	struct json_object *jso = json_object_new(json_type_double);
	if (!jso)
		return NULL;
	jso->_to_json_string = &json_object_double_to_json_string;
	jso->o.c_double = d;
	return jso;
}

struct json_object* json_object_new_double_s(double d, const char *ds)
{
	struct json_object *jso = json_object_new_double(d);
	if (!jso)
		return NULL;

	json_object_set_serializer(jso, json_object_userdata_to_json_string,
	    strdup(ds), json_object_free_userdata);
	return jso;
}

int json_object_userdata_to_json_string(struct json_object *jso,
	struct printbuf *pb, int level, int flags)
{
	int userdata_len = strlen((char*)jso->_userdata);
	printbuf_memappend(pb, (char*)jso->_userdata, userdata_len);
	return userdata_len;
}

void json_object_free_userdata(struct json_object *jso, void *userdata)
{
	free(userdata);
}

double json_object_get_double(struct json_object *jso)
{
  double cdouble;
  char *errPtr = NULL;

  if(!jso) return 0.0;
  switch(jso->o_type) {
  case json_type_double:
    return jso->o.c_double;
  case json_type_int:
    return jso->o.c_int64;
  case json_type_boolean:
    return jso->o.c_boolean;
  case json_type_string:
    errno = 0;
    cdouble = strtod(jso->o.c_string.str,&errPtr);

    /* if conversion stopped at the first character, return 0.0 */
    if (errPtr == jso->o.c_string.str)
        return 0.0;

    /*
     * Check that the conversion terminated on something sensible
     *
     * For example, { "pay" : 123AB } would parse as 123.
     */
    if (*errPtr != '\0')
        return 0.0;

    /*
     * If strtod encounters a string which would exceed the
     * capacity of a double, it returns +/- HUGE_VAL and sets
     * errno to ERANGE. But +/- HUGE_VAL is also a valid result
     * from a conversion, so we need to check errno.
     *
     * Underflow also sets errno to ERANGE, but it returns 0 in
     * that case, which is what we will return anyway.
     *
     * See CERT guideline ERR30-C
     */
    if ((HUGE_VAL == cdouble || -HUGE_VAL == cdouble) &&
        (ERANGE == errno))
            cdouble = 0.0;
    return cdouble;
  default:
    return 0.0;
  }
}


/* json_object_string */

static int json_object_string_to_json_string(struct json_object* jso,
					     struct printbuf *pb,
					     int level,
						 int flags)
{
  if (!(flags & JSON_C_TO_STRING_UNQUOTED))
      sprintbuf(pb, "\"");
  json_escape_str(pb, jso->o.c_string.str, jso->o.c_string.len);
  if (!(flags & JSON_C_TO_STRING_UNQUOTED))
      sprintbuf(pb, "\"");
  return 0;
}

static void json_object_string_delete(struct json_object* jso)
{
  free(jso->o.c_string.str);
  json_object_generic_delete(jso);
}

struct json_object* json_object_new_string(const char *s)
{
  struct json_object *jso = json_object_new(json_type_string);
  if(!jso) return NULL;
  jso->_delete = &json_object_string_delete;
  jso->_to_json_string = &json_object_string_to_json_string;
  jso->o.c_string.str = strdup(s);
  jso->o.c_string.len = strlen(s);
  return jso;
}

struct json_object* json_object_new_string_len(const char *s, int len)
{
  struct json_object *jso = json_object_new(json_type_string);
  if(!jso) return NULL;
  jso->_delete = &json_object_string_delete;
  jso->_to_json_string = &json_object_string_to_json_string;
  jso->o.c_string.str = (char*)malloc(len + 1);
  memcpy(jso->o.c_string.str, (void *)s, len);
  jso->o.c_string.str[len] = '\0';
  jso->o.c_string.len = len;
  return jso;
}

const char* json_object_get_string(struct json_object *jso)
{
  if(!jso) return NULL;
  switch(jso->o_type) {
  case json_type_string:
    return jso->o.c_string.str;
  default:
    return json_object_to_json_string(jso);
  }
}

int json_object_get_string_len(struct json_object *jso)  {
  if(!jso) return 0;
  switch(jso->o_type) {
  case json_type_string:
    return jso->o.c_string.len;
  default:
    return 0;
  }
}


/* json_object_array */

static int json_object_array_to_json_string(struct json_object* jso,
                                            struct printbuf *pb,
                                            int level,
                                            int flags)
{
	int had_children = 0;
	int ii;
	sprintbuf(pb, "[");
	if (flags & JSON_C_TO_STRING_PRETTY)
		sprintbuf(pb, "\n");
	for(ii=0; ii < json_object_array_length(jso); ii++)
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
		val = json_object_array_get_idx(jso, ii);
		if(val == NULL)
			sprintbuf(pb, "null");
		else
			val->_to_json_string(val, pb, level+1, flags);
	}
	if (flags & JSON_C_TO_STRING_PRETTY)
	{
		if (had_children)
			sprintbuf(pb, "\n");
		indent(pb,level,flags);
	}

	if (flags & JSON_C_TO_STRING_SPACED)
		return sprintbuf(pb, " ]");
	else
		return sprintbuf(pb, "]");
}

static void json_object_array_entry_free(void *data)
{
  json_object_put((struct json_object*)data);
}

static void json_object_array_delete(struct json_object* jso)
{
  array_list_free(jso->o.c_array);
  json_object_generic_delete(jso);
}

struct json_object* json_object_new_array(void)
{
  struct json_object *jso = json_object_new(json_type_array);
  if(!jso) return NULL;
  jso->_delete = &json_object_array_delete;
  jso->_to_json_string = &json_object_array_to_json_string;
  jso->o.c_array = array_list_new(&json_object_array_entry_free);
  return jso;
}

struct array_list* json_object_get_array(struct json_object *jso)
{
  if(!jso) return NULL;
  switch(jso->o_type) {
  case json_type_array:
    return jso->o.c_array;
  default:
    return NULL;
  }
}

void json_object_array_sort(struct json_object *jso, int(*sort_fn)(const void *, const void *))
{
  array_list_sort(jso->o.c_array, sort_fn);
}

int json_object_array_length(struct json_object *jso)
{
  return array_list_length(jso->o.c_array);
}

int json_object_array_add(struct json_object *jso,struct json_object *val)
{
  return array_list_add(jso->o.c_array, val);
}

int json_object_array_put_idx(struct json_object *jso, int idx,
			      struct json_object *val)
{
  return array_list_put_idx(jso->o.c_array, idx, val);
}

struct json_object* json_object_array_get_idx(struct json_object *jso,
					      int idx)
{
  return (struct json_object*)array_list_get_idx(jso->o.c_array, idx);
}

/* json_object_extarr */
enum json_extarr_type json_object_extarr_type(struct json_object* jso)
{
    if (!jso || !json_object_is_type(jso, json_type_extarr)) return json_extarr_type_null;
    return jso->o.c_extarr.type;
}

int json_object_extarr_nvals(struct json_object* jso)
{
    int i, nvals;
    if (!jso || !json_object_is_type(jso, json_type_extarr)) return 0;
    for (i = 0, nvals=1; i < json_object_extarr_ndims(jso); i++)
    {
        struct json_object* dimobj = (struct json_object*) array_list_get_idx(jso->o.c_extarr.dims, i);
        nvals *= json_object_get_int(dimobj);
    }
    return nvals;
}

int json_object_extarr_ndims(struct json_object* jso)
{
    if (!jso) return 0;
    return array_list_length(jso->o.c_extarr.dims);
}

int json_object_extarr_dim(struct json_object* jso, int dimidx)
{
    struct json_object *dimobj;
    if (!jso || !json_object_is_type(jso, json_type_extarr)) return 0;
    if (dimidx < 0) return 0;
    dimobj = (struct json_object *) array_list_get_idx(jso->o.c_extarr.dims, dimidx);
    if (!dimobj) return 0;
    return json_object_get_int(dimobj);
}

void const *json_object_extarr_data(struct json_object* jso)
{
    if (!jso || !json_object_is_type(jso, json_type_extarr)) return 0;
    return jso->o.c_extarr.data;
}

static int json_object_extarr_to_json_string(struct json_object* jso,
                                             struct printbuf *pb,
                                             int level,
                                             int flags)
{
	int had_children = 0;
	int ii;
	sprintbuf(pb, "( %d, %d, ",
            (int) json_object_extarr_type(jso), json_object_extarr_ndims(jso));
        for (ii = 0; ii < json_object_extarr_ndims(jso)-1; ii++)
	    sprintbuf(pb, "%d, ", json_object_extarr_dim(jso, ii));
	sprintbuf(pb, "%d,", json_object_extarr_dim(jso, ii));
	if (flags & JSON_C_TO_STRING_PRETTY)
		sprintbuf(pb, "\n");
	for(ii=0; ii < json_object_extarr_nvals(jso); ii++)
	{
		struct json_object *val = 0;
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
                switch (json_object_extarr_type(jso))
                {
                  case json_extarr_type_null:
                  case json_extarr_type_bit01:
                  {
                    val = NULL;
                    break;
                  }
                  case json_extarr_type_byt08:
                  {
                    unsigned char dval = *((unsigned char*)json_object_extarr_data(jso)+ii);
                    val = json_object_new_int(dval);
                    break;
                  }
                  case json_extarr_type_int32:
                  {
                    int dval = *((int*)json_object_extarr_data(jso)+ii);
                    val = json_object_new_int(dval);
                    break;
                  }
                  case json_extarr_type_int64:
                  {
                    int64_t dval = *((int64_t*)json_object_extarr_data(jso)+ii);
                    val = json_object_new_int64(dval);
                    break;
                  }
                  case json_extarr_type_flt32:
                  {
                    float dval = *((float*)json_object_extarr_data(jso)+ii);
                    val = json_object_new_double(dval);
                    break;
                  }
                  case json_extarr_type_flt64:
                  {
                    double dval = *((double*)json_object_extarr_data(jso)+ii);
                    val = json_object_new_double(dval);
                    break;
                  }
                }
		if(val == NULL)
			sprintbuf(pb, "null");
		else
			val->_to_json_string(val, pb, level+1, flags);
                json_object_put(val);
	}
	if (flags & JSON_C_TO_STRING_PRETTY)
	{
		if (had_children)
			sprintbuf(pb, "\n");
		indent(pb,level,flags);
	}

	if (flags & JSON_C_TO_STRING_SPACED)
		return sprintbuf(pb, " )");
	else
		return sprintbuf(pb, ")");
}

static void json_object_extarr_delete(struct json_object* jso)
{
  assert(jso && json_object_is_type(jso, json_type_extarr));
  array_list_free(jso->o.c_extarr.dims);
  json_object_generic_delete(jso);
}

struct json_object* json_object_new_extarr(void const *data, enum json_extarr_type type, int ndims, int const *dims)
{
  int i;
  struct json_object *jso = json_object_new(json_type_extarr);
  if(!jso) return NULL;
  jso->_delete = &json_object_extarr_delete;
  jso->_to_json_string = &json_object_extarr_to_json_string;
  jso->o.c_extarr.data = data;
  jso->o.c_extarr.type = type;
  jso->o.c_extarr.dims = array_list_new(&json_object_array_entry_free);
  for (i = 0; i < ndims; i++)
    array_list_put_idx(jso->o.c_extarr.dims, i, json_object_new_int(dims[i])); 
  return jso;
}

struct json_object* json_object_new_extarr_alloc(enum json_extarr_type etype, int ndims, int const *dims)
{
  int i, nvals;
  struct json_object *jso = json_object_new(json_type_extarr);
  if(!jso) return NULL;
  jso->_delete = &json_object_extarr_delete;
  jso->_to_json_string = &json_object_extarr_to_json_string;
  jso->o.c_extarr.type = etype;
  jso->o.c_extarr.dims = array_list_new(&json_object_array_entry_free);
  for (i = 0, nvals = 1; i < ndims; i++)
  {
    array_list_put_idx(jso->o.c_extarr.dims, i, json_object_new_int(dims[i])); 
    nvals *= dims[i];
  }
  jso->o.c_extarr.data = malloc(nvals * json_extarr_type_nbytes(etype));
  if (!jso->o.c_extarr.data)
  {
    json_object_extarr_delete(jso);
    return NULL;
  }
  return jso;
}

/* json_object_enum */
static void json_object_enum_delete(struct json_object* jso)
{
  assert(jso && json_object_is_type(jso, json_type_enum));
  lh_table_free(jso->o.c_enum.choices);
  json_object_generic_delete(jso);
}

struct json_object* json_object_new_enum(void)
{
  struct json_object *jso = json_object_new(json_type_enum);
  if(!jso) return NULL;
  jso->_delete = &json_object_enum_delete;
  jso->_to_json_string = &json_object_enum_to_json_string;
  jso->o.c_enum.choices = lh_kchar_table_new(JSON_OBJECT_DEF_HASH_ENTRIES,
                                        NULL, &json_object_lh_entry_free);
  jso->o.c_enum.choice = -1;
  return jso;
}

void json_object_enum_add(struct json_object* jso, char const *choice_name, int64_t choice_val, json_bool selected)
{
  if (!choice_name) return;
  if (choice_val < 0) return;
  if (!jso || !json_object_is_type(jso, json_type_enum)) return;

  if (selected)
      jso->o.c_enum.choice = choice_val;
  /* We lookup the entry and replace the value, rather than just deleting
     and re-adding it, so the existing key remains valid.  */
  json_object *existing_value = NULL;
  struct lh_entry *existing_entry;
  existing_entry = lh_table_lookup_entry(jso->o.c_enum.choices, (void*)choice_name);
  if (!existing_entry)
  {
    lh_table_insert(jso->o.c_enum.choices, strdup(choice_name), json_object_new_int(choice_val));
    return;
  }
  existing_value = (json_object *)existing_entry->v;
  if (existing_value)
    json_object_put(existing_value);
  existing_entry->v = json_object_new_int(choice_val);
}

int json_object_enum_length(struct json_object* jso)
{
  if (!jso || !json_object_is_type(jso, json_type_enum)) return -1;
  return lh_table_length(jso->o.c_enum.choices);
}

char const *json_object_enum_get_idx_name(struct json_object* jso, int idx)
{
  int i = 0;
  struct lh_entry *ent;
  if (!jso || !json_object_is_type(jso, json_type_enum)) return NULL;
  lh_foreach(jso->o.c_enum.choices, ent)
  {
    if (i == idx)
      return (char const *) ent->k;
    i++;
  }
  return NULL;
}

char const *json_object_enum_get_name_from_val(struct json_object* jso, int64_t val)
{
  struct lh_entry *ent;
  if (!jso || !json_object_is_type(jso, json_type_enum)) return NULL;
  lh_foreach(jso->o.c_enum.choices, ent)
  {
    if (json_object_get_int((struct json_object*)ent->v) == val)
      return (char const *) ent->k;
  }
  return NULL;
}

int64_t json_object_enum_get_idx_val(struct json_object* jso, int idx)
{
  int i = 0;
  struct lh_entry *ent;
  if (!jso || !json_object_is_type(jso, json_type_enum)) return -1;
  lh_foreach(jso->o.c_enum.choices, ent)
  {
    if (i == idx)
      return json_object_get_int((struct json_object*)ent->v);
    i++;
  }
  return -1;
}

int64_t json_object_enum_get_val_from_name(struct json_object *jso, char const *name)
{
  struct lh_entry *ent;
  if (!jso || !json_object_is_type(jso, json_type_enum)) return -1;
  lh_foreach(jso->o.c_enum.choices, ent)
  {
    if (!strcmp((char const *)ent->k, name))
      return json_object_get_int((struct json_object*)ent->v);
  }
  return -1;
}

int64_t json_object_enum_get_choice_val(struct json_object* jso)
{
  if (!jso || !json_object_is_type(jso, json_type_enum)) return -1;
  return jso->o.c_enum.choice;
}

char const *json_object_enum_get_choice_name(struct json_object* jso)
{
  if (!jso || !json_object_is_type(jso, json_type_enum)) return NULL;
  return json_object_enum_get_name_from_val(jso, jso->o.c_enum.choice);
}

static int json_object_enum_to_json_string(struct json_object* jso,
                                            struct printbuf *pb,
                                            int level,
                                            int flags)
{
	int had_children = 0;
	int ii;
	sprintbuf(pb, "<");
	if (flags & JSON_C_TO_STRING_PRETTY)
		sprintbuf(pb, "\n");
	for(ii=-1; ii < json_object_enum_length(jso); ii++)
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
                if (ii < 0)
		  val = json_object_new_string(json_object_enum_get_choice_name(jso));
                else
		  val = json_object_new_string(json_object_enum_get_idx_name(jso, ii));
		if(val == NULL)
                  sprintbuf(pb, "null");
		else
                  val->_to_json_string(val, pb, level+1, flags);
                json_object_put(val);
                sprintbuf(pb, ":");
                if (ii < 0)
                  val = json_object_new_int(json_object_enum_get_choice_val(jso));
                else
                  val = json_object_new_int(json_object_enum_get_idx_val(jso, ii));
		if(val == NULL)
                  sprintbuf(pb, "null");
		else
                  val->_to_json_string(val, pb, level+1, flags);
                json_object_put(val);

	}
	if (flags & JSON_C_TO_STRING_PRETTY)
	{
		if (had_children)
			sprintbuf(pb, "\n");
		indent(pb,level,flags);
	}

	if (flags & JSON_C_TO_STRING_SPACED)
		return sprintbuf(pb, " >");
	else
		return sprintbuf(pb, ">");
}

/* primitive (leaf) overwrite (put) methods */
json_bool json_object_set_boolean(struct json_object *bool_obj, json_bool val)
{
    if (!bool_obj || !json_object_is_type(bool_obj, json_type_boolean)) return JSON_C_FALSE;
    bool_obj->o.c_boolean = val;
    return JSON_C_TRUE;
}

json_bool json_object_set_enum_choice_name(struct json_object *enum_obj, char const *name)
{
    struct lh_entry *ent;
    if (!enum_obj || !json_object_is_type(enum_obj, json_type_enum)) return JSON_C_FALSE;
    lh_foreach(enum_obj->o.c_enum.choices, ent)
    {
        if (!strcmp((char const *)ent->k, name))
        {
            enum_obj->o.c_enum.choice = json_object_get_int((struct json_object*)ent->v);
            break;
        }
    }
    return JSON_C_TRUE;
}

json_bool json_object_set_enum_choice_val(struct json_object *enum_obj, int64_t val)
{
    if (!enum_obj || !json_object_is_type(enum_obj, json_type_enum)) return JSON_C_FALSE;
    enum_obj->o.c_enum.choice = val;
    return JSON_C_TRUE;
}

json_bool json_object_set_int(struct json_object *int_obj, int32_t val)
{
    if (!int_obj || !json_object_is_type(int_obj, json_type_int)) return JSON_C_FALSE;
    int_obj->o.c_int64 = (int64_t) val;
    return JSON_C_TRUE;
}

json_bool json_object_set_int64(struct json_object *int_obj, int64_t val)
{
    if (!int_obj || !json_object_is_type(int_obj, json_type_int)) return JSON_C_FALSE;
    int_obj->o.c_int64 = val;
    return JSON_C_TRUE;
}

json_bool json_object_set_double(struct json_object *double_obj, double val)
{
    if (!double_obj || !json_object_is_type(double_obj, json_type_double)) return JSON_C_FALSE;
    double_obj->o.c_double = val;
    return JSON_C_TRUE;
}

json_bool json_object_set_string(struct json_object *string_obj, char const *val)
{
    if (!string_obj || !json_object_is_type(string_obj, json_type_string)) return JSON_C_FALSE;
    if (string_obj->o.c_string.str) free(string_obj->o.c_string.str);
    string_obj->o.c_string.str = strdup(val);
    string_obj->o.c_string.len = strlen(val);
    return JSON_C_TRUE;
}

#if 0
static void *json_object_get_voidptr(struct json_object *obj)
{
    static char garbage[32];
    if (json_object_is_type(obj, json_type_boolean))
        return (void*) &obj->o.c_boolean;
    if (json_object_is_type(obj, json_type_int))
        return (void*) &obj->o.c_int64;
    if (json_object_is_type(obj, json_type_double))
        return (void*) &obj->o.c_double;
    if (json_object_is_type(obj, json_type_string))
        return (void*) &obj->o.c_string.str;
    return (void*) &garbage[0];
}

static void *json_object_path_get_voidptr_recurse(struct json_object *src, char *key_path, json_type jtype)
{
    struct json_object *sub_object = 0;
    char *slash_char_p = strchr(key_path,'/');
    /* we need to support . and .. notation here too */
    if (slash_char_p)
    {
        *slash_char_p = '\0';
        if (json_object_object_get_ex(src, key_path, &sub_object))
            return json_object_path_get_voidptr_recurse(sub_object, slash_char_p+1, jtype);
    }
    else
    {
        if (json_object_object_get_ex(src, key_path, &sub_object) &&
            json_object_is_type(sub_object, jtype))
            return json_object_get_voidptr(sub_object);
    }
    return 0;
}

static void *json_object_path_get_voidptr(struct json_object *src, char const *key_path, json_type jtype)
{
    char *kp = strdup(key_path);
    void *retval = json_object_path_get_voidptr_recurse(src, kp, jtype);
    free(kp);
    return retval;
}
#endif

/* "apath" methods handle paths with possible array indexes embedded within them
   (e.g. "/foo/bar/123/front/567/back") */
static void *json_object_apath_get_leafobj_recurse(struct json_object *src, char *key_path)
{
#warning we need to support . and .. notation here too
    int idx;
    struct json_object *sub_object = 0;
    char *slash_char_p, *next = 0;
    int add1 = key_path[0]=='/'?1:0;

    if (!src) return 0;
    if (!key_path) return src;

    slash_char_p = strchr(key_path+add1, '/');
    if (slash_char_p)
    {
        *slash_char_p = '\0';
        next = slash_char_p+1;
    }
    errno = 0;
    idx = (int) strtol(key_path+add1, 0, 10);
    
    if (json_object_is_type(src, json_type_array) && errno == 0 &&
        0 <= idx && idx < json_object_array_length(src))
    {
        if ((sub_object = json_object_array_get_idx(src, idx)) && sub_object)
        {
            if (next)
                return json_object_apath_get_leafobj_recurse(sub_object, next);
            else 
                return sub_object;
        }
    }

    if (json_object_object_get_ex(src, key_path, &sub_object) && sub_object)
    {
        if (next)
            return json_object_apath_get_leafobj_recurse(sub_object, next);
        else
            return sub_object;
    }

    return src;
}

static struct json_object* json_object_apath_get_leafobj(struct json_object *obj, char const *key_path)
{
    char *kp = strdup(key_path);
    struct json_object *retval = (struct json_object *) json_object_apath_get_leafobj_recurse(obj, kp);
    free(kp);
    return retval;
}

json_bool json_object_apath_get_boolean(struct json_object *obj, char const *key_path)
{
    struct json_object *leafobj = json_object_apath_get_leafobj(obj, key_path);
    if (leafobj)
    {
        switch (json_object_get_type(leafobj))
        {
            case json_type_null: return JSON_C_FALSE;
            case json_type_boolean: return json_object_get_boolean(leafobj);
            case json_type_int: return json_object_get_int(leafobj)!=0?JSON_C_TRUE:JSON_C_FALSE;
            case json_type_double: return json_object_get_double(leafobj)!=0?JSON_C_TRUE:JSON_C_FALSE;
            case json_type_array: return json_object_array_length(leafobj)!=0?JSON_C_TRUE:JSON_C_FALSE;
            case json_type_object: return json_object_object_length(leafobj)?JSON_C_TRUE:JSON_C_FALSE;
            case json_type_enum: return json_object_enum_get_choice_val(leafobj)?JSON_C_TRUE:JSON_C_FALSE; 
            case json_type_string:
            {
                char const *str = json_object_get_string(leafobj);
                return (!str || *str=='\0')?JSON_C_FALSE:JSON_C_TRUE;
            }
            case json_type_extarr:
            {
                int i, ndims = json_object_extarr_ndims(leafobj);
                if (!ndims) return JSON_C_FALSE;
                for (i = 0; i < ndims; i++)
                    if (!json_object_extarr_dim(leafobj, i)) return JSON_C_FALSE;
                return json_object_extarr_data(leafobj)?JSON_C_TRUE:JSON_C_FALSE;
            }
        }
    }
    return JSON_C_FALSE;
}

int64_t json_object_apath_get_int64(struct json_object *obj, char const *key_path)
{
    struct json_object *leafobj = json_object_apath_get_leafobj(obj, key_path);
    if (leafobj)
    {
        switch (json_object_get_type(leafobj))
        {
            case json_type_null: return 0;
            case json_type_boolean: return json_object_get_boolean(leafobj)==JSON_C_TRUE?1:0;
            case json_type_int: return json_object_get_int64(leafobj);
            case json_type_double: return (int64_t) json_object_get_double(leafobj);
            case json_type_array: return json_object_array_length(leafobj);
            case json_type_object: return json_object_object_length(leafobj);
            case json_type_enum: return json_object_enum_get_choice_val(leafobj);
            case json_type_string:
            {
                char const *str = json_object_get_string(leafobj);
                errno = 0;
                if (str && *str != '\0')
                {
                    char *ep = 0;
                    long long val = strtoll(json_object_get_string(leafobj), &ep, 10);
                    if (errno == 0 && ep != str)
                        return (int64_t) val;
                }
            }
            case json_type_extarr:
            {
                int i, ndims = json_object_extarr_ndims(leafobj), n = ndims?1:0;
                for (i = 0; i < ndims; i++)
                    n *= json_object_extarr_dim(leafobj, i);
                return n;
            }
        }
    }
    return 0;
}

int json_object_apath_get_int(struct json_object *obj, char const *key_path)
{
    int64_t val64 = json_object_apath_get_int64(obj, key_path);
    if (INT_MIN <= val64 && val64 <= INT_MAX)
        return (int) val64;
    else
        return 0;
}

double json_object_apath_get_double(struct json_object *obj, char const *key_path)
{
    struct json_object *leafobj = json_object_apath_get_leafobj(obj, key_path);
    if (leafobj)
    {
        switch (json_object_get_type(leafobj))
        {
            case json_type_null: return 0.0;
            case json_type_boolean: return json_object_get_boolean(leafobj)?1.0:0.0;
            case json_type_int: return (double) json_object_get_int(leafobj);
            case json_type_double: return json_object_get_double(leafobj);
            case json_type_array: return (double) json_object_array_length(leafobj);
            case json_type_object: return (double) json_object_object_length(leafobj);
            case json_type_enum: return (double) json_object_enum_get_choice_val(leafobj);
            case json_type_string:
            {
                char const *str = json_object_get_string(leafobj);
                errno = 0;
                if (str && *str != '\0')
                {
                    char *ep = 0;
                    double val = strtod(json_object_get_string(leafobj), &ep);
                    if (errno == 0 && ep != str)
                        return val;
                }
            }
            case json_type_extarr:
            {
                int i, ndims = json_object_extarr_ndims(leafobj), n = ndims?1:0;
                for (i = 0; i < ndims; i++)
                    n *= json_object_extarr_dim(leafobj, i);
                return (double) n;
            }
        }
    }
    return 0.0;
}

#define CIRCBUF_SIZE 1024 
#define CIRCBUF_RET(STR)                       \
{                                              \
    if (circbuf_retval[circbuf_idx] != 0)      \
        free(circbuf_retval[circbuf_idx]);     \
    circbuf_retval[circbuf_idx] = strdup(STR); \
    retval = circbuf_retval[circbuf_idx];      \
    circbuf_idx = (circbuf_idx+1) % CIRCBUF_SIZE;  \
    return retval;                             \
}

char const *json_object_apath_get_string(struct json_object *obj, char const *key_path)
{
    static int circbuf_idx = 0;
    static char *circbuf_retval[CIRCBUF_SIZE] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
    char *retval;

    struct json_object *leafobj = json_object_apath_get_leafobj(obj, key_path);
    if (leafobj)
    {
        CIRCBUF_RET(json_object_to_json_string_ext(leafobj, JSON_C_TO_STRING_UNQUOTED));
    }
    else
    {
        CIRCBUF_RET("null");
    }
}

struct json_object *json_object_apath_get_object(struct json_object *obj, char const *key_path)
{
    struct json_object *leafobj = json_object_apath_get_leafobj(obj, key_path);
    if (leafobj) return leafobj;
    return 0;
}

char const *json_paste_apath(char const *va_args_str, char const *first, ...)
{
    static char retbuf[4096];
    int nargs = 1, i = 0, n = sizeof(retbuf);
    static char tmpstr[32];
    int val;
    char *str;

    /* count the number of args using commas in va_args_str */
    while (va_args_str[i] != '\0')
    {
        if (va_args_str[i] == ',') nargs++;
        i++;
    }
    
    va_list ap;
    va_start(ap, first);
    nargs--;

    retbuf[0] = '\0';
    strncat(retbuf, first, n);
    n -= strlen(first);
    val = va_arg(ap, int);

    while (nargs > 0)
    {
        snprintf(tmpstr, sizeof(tmpstr), "%d", val);
        tmpstr[sizeof(tmpstr)-1] = '\0';
        if (retbuf[sizeof(retbuf)-n-1] != '/')
        {
            strncat(retbuf, "/", n);
            n -= 1;
        }
        strncat(retbuf, tmpstr, n);
        n -= strlen(tmpstr);
        str = va_arg(ap, char*);
        nargs--;
        if (nargs == 0) break;
        if (str[0] != '/')
        {
            strncat(retbuf, "/", n);
            n -= 1;
        }
        strncat(retbuf, str, n);
        n -= strlen(str);
        val = va_arg(ap, int);
        nargs--;
    }
    va_end(ap);

    return retbuf;
}

static void *json_object_path_get_leafobj_recurse(struct json_object *src, char *key_path, json_type jtype)
{
#warning we need to support . and .. notation here too

    struct json_object *sub_object = 0;
    char *slash_char_p;

    if (!src || !key_path) return 0;

    slash_char_p = strchr(key_path,'/');
    if (slash_char_p)
    {
        *slash_char_p = '\0';
        if (json_object_object_get_ex(src, key_path, &sub_object))
            return json_object_path_get_leafobj_recurse(sub_object, slash_char_p+1, jtype);
    }
    else
    {
        if (json_object_object_get_ex(src, key_path, &sub_object))
        {
            if (jtype == json_type_null)
                return sub_object;
            else if (json_object_is_type(sub_object, jtype))
                return sub_object;
        }
    }
    return 0;
}

static struct json_object* json_object_path_get_leafobj(struct json_object *obj, char const *key_path, json_type jtype)
{
    char *kp = strdup(key_path);
    struct json_object *retval = (struct json_object *) json_object_path_get_leafobj_recurse(obj, kp, jtype);
    free(kp);
    return retval;
}

json_bool json_object_path_set_boolean(struct json_object *obj, char const *key_path, json_bool val)
{
    struct json_object *leafobj = json_object_path_get_leafobj(obj, key_path, json_type_boolean);
    if (leafobj) return json_object_set_boolean(leafobj, val);
    return JSON_C_FALSE;
}

json_bool json_object_path_set_enum_choice_val(struct json_object *obj, char const *key_path, int64_t val)
{
    struct json_object *leafobj = json_object_path_get_leafobj(obj, key_path, json_type_enum);
    if (leafobj) return json_object_set_enum_choice_val(leafobj, val);
    return JSON_C_FALSE;
}

json_bool json_object_path_set_enum_choice_name(struct json_object *obj, char const *key_path, char const *val)
{
    struct json_object *leafobj = json_object_path_get_leafobj(obj, key_path, json_type_enum);
    if (leafobj) return json_object_set_enum_choice_name(leafobj, val);
    return JSON_C_FALSE;
}

json_bool json_object_path_set_int(struct json_object *obj, char const *key_path, int32_t val)
{
    struct json_object *leafobj = json_object_path_get_leafobj(obj, key_path, json_type_int);
    if (leafobj) return json_object_set_int(leafobj, val);
    return JSON_C_FALSE;
}

json_bool json_object_path_set_int64(struct json_object *obj, char const *key_path, int64_t val)
{
    struct json_object *leafobj = json_object_path_get_leafobj(obj, key_path, json_type_int);
    if (leafobj) return json_object_set_int64(leafobj, val);
    return JSON_C_FALSE;
}

json_bool json_object_path_set_double(struct json_object *obj, char const *key_path, double val)
{
    struct json_object *leafobj = json_object_path_get_leafobj(obj, key_path, json_type_double);
    if (leafobj) return json_object_set_double(leafobj, val);
    return JSON_C_FALSE;
}

json_bool json_object_path_set_string(struct json_object *obj, char const *key_path, char const *val)
{
    struct json_object *leafobj = json_object_path_get_leafobj(obj, key_path, json_type_string);
    if (leafobj) return json_object_set_string(leafobj, val);
    return JSON_C_FALSE;
}

int32_t json_object_path_get_int(struct json_object *src, char const *key_path)
{
    int32_t retval = 0;
    char *kp = strdup(key_path);
    struct json_object *leafobj = json_object_path_get_leafobj(src, key_path, json_type_int);
    free(kp);
    if (leafobj) retval = json_object_get_int(leafobj);
    return retval;
}

int64_t json_object_path_get_int64(struct json_object *src, char const *key_path)
{
    int64_t retval = 0;
    char *kp = strdup(key_path);
    struct json_object *leafobj = json_object_path_get_leafobj(src, key_path, json_type_int);
    free(kp);
    if (leafobj) retval = json_object_get_int64(leafobj);
    return retval;
}

double json_object_path_get_double(struct json_object *src, char const *key_path)
{
    double retval = 0;
    char *kp = strdup(key_path);
    struct json_object *leafobj = json_object_path_get_leafobj(src, key_path, json_type_double);
    free(kp);
    if (leafobj) retval = json_object_get_double(leafobj);
    return retval;
}

json_bool json_object_path_get_boolean(struct json_object *src, char const *key_path)
{
    json_bool retval = JSON_C_FALSE;
    char *kp = strdup(key_path);
    struct json_object *leafobj = json_object_path_get_leafobj(src, key_path, json_type_boolean);
    free(kp);
    if (leafobj) retval = json_object_get_boolean(leafobj);
    return retval;
}

char const *json_object_path_get_string(struct json_object *src, char const *key_path)
{
    char const *retval = "";
    char *kp = strdup(key_path);
    struct json_object *leafobj = json_object_path_get_leafobj(src, key_path, json_type_string);
    free(kp);
    if (leafobj) retval = json_object_get_string(leafobj);
    return retval;
}

struct json_object *json_object_path_get_array(struct json_object *src, char const *key_path)
{
    struct json_object *retval = 0;
    char *kp = strdup(key_path);
    struct json_object *leafobj = json_object_path_get_leafobj(src, key_path, json_type_array);
    free(kp);
    if (leafobj) retval = leafobj;
    return retval;
}

struct json_object *json_object_path_get_object(struct json_object *src, char const *key_path)
{
    struct json_object *retval = 0;
    char *kp = strdup(key_path);
    struct json_object *leafobj = json_object_path_get_leafobj(src, key_path, json_type_object);
    free(kp);
    if (leafobj) retval = leafobj;
    return retval;
}

struct json_object *json_object_path_get_extarr(struct json_object *src, char const *key_path)
{
    struct json_object *retval = 0;
    char *kp = strdup(key_path);
    struct json_object *leafobj = json_object_path_get_leafobj(src, key_path, json_type_extarr);
    free(kp);
    if (leafobj) retval = leafobj;
    return retval;
}

struct json_object *json_object_path_get_any(struct json_object *src, char const *key_path)
{
    struct json_object *retval = 0;
    char *kp = strdup(key_path);
    struct json_object *leafobj = json_object_path_get_leafobj(src, key_path, json_type_null);
    free(kp);
    if (leafobj) retval = leafobj;
    return retval;
}
