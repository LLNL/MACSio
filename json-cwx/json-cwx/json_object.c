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
#include "json_crc.h"
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

static int json_object_extarr_to_json_string(struct json_object* jso,
                                             struct printbuf *pb,
                                             int level,
                                             int flags)
{
        int do_vals = !(flags & JSON_C_TO_STRING_NO_EXTARR_VALS);
	int had_children = 0;
	int ii;
        if (do_vals)
	    sprintbuf(pb, "( %d, %d, ",
                (int) json_object_extarr_type(jso), json_object_extarr_ndims(jso));
        else
	    sprintbuf(pb, "[ %d, %d, ",
                (int) json_object_extarr_type(jso), json_object_extarr_ndims(jso));
        for (ii = 0; ii < json_object_extarr_ndims(jso)-1; ii++)
	    sprintbuf(pb, "%d, ", json_object_extarr_dim(jso, ii));
        if (do_vals)
	    sprintbuf(pb, "%d,", json_object_extarr_dim(jso, ii));
        else
	    sprintbuf(pb, "%d", json_object_extarr_dim(jso, ii));
	if (flags & JSON_C_TO_STRING_PRETTY)
		sprintbuf(pb, "\n");
	for(ii=0; ii < json_object_extarr_nvals(jso) && do_vals; ii++)
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

        if (do_vals)
        {
	    if (flags & JSON_C_TO_STRING_SPACED)
		return sprintbuf(pb, " )");
	    else
		return sprintbuf(pb, ")");
        }
        else
        {
	    if (flags & JSON_C_TO_STRING_SPACED)
		return sprintbuf(pb, " ]");
	    else
		return sprintbuf(pb, "]");
        }
}

static void json_object_extarr_delete(struct json_object* jso)
{
  assert(jso && json_object_is_type(jso, json_type_extarr));
  array_list_free(jso->o.c_extarr.dims);
  json_object_generic_delete(jso);
}

#warning MOVE ADDTOGROUP TO TOP OF NON-STATIC FUNCTIONS

/**
 * \addtogroup jsonclib JSON-C Library
 * \brief JSON-C is a library used within MACSio to manage data
 *
 * All of the data marshalled by MACSio and its plugins is stored in an uber
 * JSON object. In a write test, MACSio creates and updates this uber JSON
 * object and passes it to plugins. Plugins traverse, interrogate and write
 * out what MACSio passes to them. On the other hand, during read tests, it
 * is the plugins that are responsible for creating the same JSON object from
 * file data that gets passed back to MACSio to validate.
 *
 * As a consequence, it is important for plugin developers to understand the
 * structure of the JSON data object used in MACSio to represent mesh and
 * variable data.
 *
 * The easiest way to understand it is to \em browse an example of MACSio's
 * uber JSON object in a web browser. However, it is very useful for your
 * browser to have the extension necessary to \em render and manipulate
 * raw JSON strings nicely. Here are some options...
 *   - For Safari: https://github.com/rfletcher/safari-json-formatter
 *   - For Chrome: https://github.com/tulios/json-viewer
 *   - For Firefox: https://addons.mozilla.org/en-Us/firefox/addon/jsonview
 *
 * Here is an <A HREF="../../../macsio/macsio_json_object.json">example</A> of
 * a JSON object on a given MPI task.
 *
 * This is a modified version of the JSON-C library.
 *
 * The initial version was obtained from here, https://github.com/json-c/json-c/releases,
 * numbered 0.12-20140410.
 *
 * It has been modified to support HPC applications in some key ways.
 *  - Optimized, homogenously typed, multi-dimensional arrays.
 *    That is, arrays whose members are NOT json objects but
 *    the individual array elements in a larger buffer. These
 *    are 'extarr' object types.
 *  - Enumerations.
 *  - Path-oriented object get/set methods that support
 *    intermediate arrays so that in the object path, "/a/gorfo/5/foo/7/8", the
 *    '5', '7' and '8' can be interpreted as indexes into arrays preceding them
 *    in the path. In other words, when this path is successfully applied to an
 *    object, the object has a member 'a', which is in turn an object with a member,
 *    'gorfo', which is an array. Then, '5' picks the member of that array at index 5.
 *    That object has a member, 'foo', which is an array. The '7' indexes that array
 *    which returns another array which is in turn indexed by the '8'.
 *  - A find method that is a lot like Unix' find except that it can
 *    find a matching sub-path from the specified root.
 *
 * These modifications have not been and most likely will not ever be pushed
 * back to JSON-C implementors. This is primarily because many of these
 * enhancements kinda sorta fall outside the original design scope (IMHO)
 * of JSON in general and JSON-C in particular. In fact, the extarr and enum
 * types break the JSON-C ascii string syntax.
 *
 @{
 */

/**
 * \addtogroup aggregate Aggregate Types
 @{
 */

/**
 * \addtogroup extarr External Arrays
 * \brief Object for maintaining pointer to large, bulk data
 *
 * External array (or extarr) objects are an extension to JSON-C not
 * available in nor likely to be appropriate for the original JSON-C
 * implementation. HPC applications require external array objects
 * defined here because the arrays HPC applications use are very, very
 * large. In ordinary JSON arrays, each member of an array is a fully
 * self-contained JSON object. For large arrays, that means a lot of
 * unnecessary overhead. In addition, ordinary JSON arrays are single
 * dimensional whereas HPC applications often require arrays that are
 * multi-dimensional.
 *
 * Here, extarr objects are implemented as an extension to JSON-C. Changes
 * were made to JSON-C internals, parser and serialization methods to support
 * extarr objects. This means that any serialized string of a JSON-C object
 * that includes extarr objects within it, will be incompatible with any
 * standard JSON implementation. Another route is to implement extarr
 * objects <em>on top of</em> JSON-C and this may be more appropriate in
 * a future implementation.
 @{
*/

/** Create new external array object using existing buffer 
 *
 * Buffer data is \em not copied here. JSON-C does not take ownership
 * of the buffer and will not free the buffer when the extarr object
 * is deleted with json_object_put(). Caller is responsible for not
 * freeing buffers out from underneath any respective JSON-C extarr 
 * objects but is responsible for freeing the buffer after it is no
 * longer needed either by JSON-C or by caller. At any point before
 * deleting the extarr object with json_object_put(), caller can
 * obtain the buffer pointer with json_object_extarr_data().
 */
struct json_object*
json_object_new_extarr(
    void const *data,           /**< [in] The array buffer pointer */
    enum json_extarr_type type, /**< [in] The type of data in the array */
    int ndims,                  /**< [in] The number of dimensions in the array */
    int const *dims             /**< [in] Array of length \c ndims of integer values holding the size
                                     in each dimension */
)
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

/** Create new external array object and allocate associated buffer
 *
 * Although the JSON-C library allocates the buffer, this is just a convenience.
 * The caller is still responsible for freeing the buffer by getting its pointer
 * using json_object_extarr_data(). Caller must do this before calling json_object_put()
 * to delete the extarr object. The buffer is allocated but not initialized.
 */
struct json_object*
json_object_new_extarr_alloc(
    enum json_extarr_type etype, /**< [in] The type of data to be put into the buffer */
    int ndims,                   /**< [in] The number of dimensions in the array */
    int const *dims              /**< [in] Array of length \c ndims of integer values holding the
                                      [in] in each dimension */
)
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

int json_object_extarr_valsize(struct json_object* obj)
{
    if (!obj || !json_object_is_type(obj, json_type_extarr)) return 0;

    switch (json_object_extarr_type(obj))
    {
        case json_extarr_type_null:  return(0);
        case json_extarr_type_bit01: return(1);
        case json_extarr_type_byt08: return(1);
        case json_extarr_type_int32: return(4);
        case json_extarr_type_int64: return(8);
        case json_extarr_type_flt32: return(4);
        case json_extarr_type_flt64: return(8);
    }

    return 0;
}

int64_t json_object_extarr_nbytes(struct json_object* obj)
{
    if (!obj || !json_object_is_type(obj, json_type_extarr)) return 0;

    return json_object_extarr_nvals(obj) *
           json_object_extarr_valsize(obj);
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

#define COPYNCAST_EXTARR_DATA(SRCT, SRCP, DSTT, DSTP, NVALS) \
{                                                            \
    if (!strcmp(#SRCT, #DSTT))                               \
        memcpy(DSTP, SRCP, NVALS * sizeof(SRCT));            \
    else                                                     \
    {                                                        \
        int i;                                               \
        SRCT *psrc = (SRCT*) json_object_extarr_data(jso);   \
        DSTT *pdst = (DSTT*) DSTP;                           \
        for (i = 0; i < NVALS; i++)                          \
            pdst[i] = (DSTT) psrc[i];                        \
    }                                                        \
}

#define JSON_OBJECT_EXTARR_DATA_AS(DSTT,DSTN)                                                               \
int json_object_extarr_data_as_ ## DSTN(struct json_object* jso, DSTT **buf)                                \
{                                                                                                           \
    json_extarr_type etype;                                                                                 \
    int nvals;                                                                                              \
    void const *srcp;                                                                                       \
                                                                                                            \
    if (buf == 0) return 0;                                                                                 \
    if (!jso || !json_object_is_type(jso, json_type_extarr)) return 0;                                      \
                                                                                                            \
    etype = json_object_extarr_type(jso);                                                                   \
    if (etype == json_extarr_type_null) return 0;                                                           \
    nvals = json_object_extarr_nvals(jso);                                                                  \
                                                                                                            \
    if (*buf == 0)                                                                                          \
        *buf = (DSTT*) malloc(nvals * sizeof(DSTT));                                                        \
    srcp = json_object_extarr_data(jso);                                                                    \
                                                                                                            \
    switch (etype)                                                                                          \
    {                                                                                                       \
        case json_extarr_type_byt08: COPYNCAST_EXTARR_DATA(unsigned char, srcp, DSTT, *buf, nvals); break;  \
        case json_extarr_type_int32: COPYNCAST_EXTARR_DATA(int,           srcp, DSTT, *buf, nvals); break;  \
        case json_extarr_type_int64: COPYNCAST_EXTARR_DATA(int64_t,       srcp, DSTT, *buf, nvals); break;  \
        case json_extarr_type_flt32: COPYNCAST_EXTARR_DATA(float,         srcp, DSTT, *buf, nvals); break;  \
        case json_extarr_type_flt64: COPYNCAST_EXTARR_DATA(double,        srcp, DSTT, *buf, nvals); break;  \
        default: return 0;                                                                                  \
    }                                                                                                       \
                                                                                                            \
    return 1;                                                                                               \
}

JSON_OBJECT_EXTARR_DATA_AS(unsigned char,unsigned_char)
JSON_OBJECT_EXTARR_DATA_AS(int,int)
JSON_OBJECT_EXTARR_DATA_AS(int64_t,int64_t)
JSON_OBJECT_EXTARR_DATA_AS(float,float)
JSON_OBJECT_EXTARR_DATA_AS(double,double)

int64_t json_object_extarr_crc(struct json_object* obj)
{
    if (!obj || !json_object_is_type(obj, json_type_extarr)) return 0;

    return (int64_t) json_crcFast(json_object_extarr_data(obj),
                                  json_object_extarr_nbytes(obj));
}

/**@} External Arrays */

/**@} Aggregate Types */

/* json_object_enum */
static void json_object_enum_delete(struct json_object* jso)
{
  assert(jso && json_object_is_type(jso, json_type_enum));
  lh_table_free(jso->o.c_enum.choices);
  json_object_generic_delete(jso);
}

/**
  * \addtogroup prim Primitive Types
  *
  * The primitive types in JSON-C library are boolean, int, int64_t, enum, double and string.
  @{
  */

/**
 * \addtogroup enums Enumerations
 * \brief Support for enumerated types
 *
 * Enumerations are an extension of JSON-C. The are not really relevant
 * to MACSio but were implemented as part of a hack-a-thon.
 *
 * A challenge with enumerations is that they kinda sorta involve two
 * different kinds of information.  One is a list of possible values
 * (e.g. the available enumerations). The other is an instance of a JSON
 * object of that enumeration type and its associated value (e.g. one of
 * the available enumerations). Here, these two different sources of
 * information wind up being combined into a single JSON object. This
 * single object maintains both a list of the available enumeration values
 * and the currently selected choice.
 @{
 */

/**
  * \brief Create a new, empty enumeration object
  *
  * The enumeration's choice (e.g. selected value) is uninitialized.
  */
struct json_object* json_object_new_enum(void)
{
  struct json_object *jso = json_object_new(json_type_enum);
  if(!jso) return NULL;
  jso->_delete = &json_object_enum_delete;
  jso->_to_json_string = &json_object_enum_to_json_string;
  jso->o.c_enum.choices = lh_kchar_table_new(JSON_OBJECT_DEF_HASH_ENTRIES,
                                        NULL, &json_object_lh_entry_free);
  jso->o.c_enum.choice = 0;
  return jso;
}

/**
  * \brief Add a name/value pair to an enumeration
  *
  * Caller can also choose to indicate that the given name/value pair should
  * be the currently selected value for the enumeration by passing \c JSON_C_TRUE
  * for \c selected argument. Alternatively, caller can use either 
  * json_object_set_enum_choice_val() or json_object_set_enum_choice_name() to
  * adjust the current value for an enumeration.
  */
void 
json_object_enum_add(
    struct json_object* jso, /**< [in] The JSON enumeration object to update */
    char const *name,        /**< [in] The symbolic name of the new enumeration choice */
    int64_t val,             /**< [in] The numerical value of the new enumeration choice. */
    json_bool selected       /**< [in] Make this value is the currently selected value of the enumeration */
)
{
  if (!name) return;
  if (!jso || !json_object_is_type(jso, json_type_enum)) return;

  if (selected)
      jso->o.c_enum.choice = val;
  /* We lookup the entry and replace the value, rather than just deleting
     and re-adding it, so the existing key remains valid.  */
  json_object *existing_value = NULL;
  struct lh_entry *existing_entry;
  existing_entry = lh_table_lookup_entry(jso->o.c_enum.choices, (void*)name);
  if (!existing_entry)
  {
    lh_table_insert(jso->o.c_enum.choices, strdup(name), json_object_new_int(val));
    return;
  }
  existing_value = (json_object *)existing_entry->v;
  if (existing_value)
    json_object_put(existing_value);
  existing_entry->v = json_object_new_int(val);
}

/**
  * \brief Get the length (or size) of an enumeration
  *
  * \return The number of available name/value pairs in the enumeration.
  * If \c jso is either null or does not reference a JSON-C object
  * which is an enumeration, -1 is returned.
  */
int json_object_enum_length(struct json_object* jso)
{
  if (!jso || !json_object_is_type(jso, json_type_enum)) return -1;
  return lh_table_length(jso->o.c_enum.choices);
}

int64_t json_object_enum_nbytes(struct json_object *jso)
{
  int i;
  int64_t retval = 0;
  if (!jso || !json_object_is_type(jso, json_type_enum)) return 0;
  for (i = 0; i < json_object_enum_length(jso); i++)
      retval += strlen(json_object_enum_get_idx_name(jso, i)) + sizeof(int64_t);
  return retval;
}

/**
  * \brief Get the name of a specific name/value pair in the enumeration
  *
  * Enumeration name/value pairs are indexed starting from zero.
  *
  * \return The name of the ith name/value pair in the enumeration.
  * If \c jso is either null or does not reference a JSON-C object
  * which is an enumeration, null is returned.
  */
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

/**
  * \brief Get name of value in an enumeration
  *
  * \return The name of the name/value pair whose value matches the given value.
  * If \c jso is either null or does not reference a JSON-C object
  * which is an enumeration, null is returned.
  */
char const *json_object_enum_get_name(struct json_object* jso, int64_t val)
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

#warning MAY WANT TO USE -INT_MAX FOR BAD ENUM VALUE
/**
  * \brief Get the value of a specific name/value pair in the enumeration
  *
  * Enumeration name/value pairs are indexed starting from zero.
  *
  * \return The value of the ith name/value pair in the enumeration.
  * If \c jso is either null or does not reference a JSON-C object
  * which is an enumeration, -1 is returned.
  */
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

/**
  * \brief Get value of name in an enumeration
  *
  * \return The value of the name/value pair whose name matches the given name.
  * If \c jso is either null or does not reference a JSON-C object
  * which is an enumeration, -1 is returned.
  */
int64_t json_object_enum_get_val(struct json_object *jso, char const *name)
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

/**
  * \brief Get chosen value of an enumeration
  *
  * \return The value of the selected choice of the enumeration 
  * If \c jso is either null or does not reference a JSON-C object
  * which is an enumeration, -1 is returned.
  */
int64_t json_object_enum_get_choice_val(struct json_object* jso)
{
  if (!jso || !json_object_is_type(jso, json_type_enum)) return -1;
  return jso->o.c_enum.choice;
}

/**
  * \brief Get chosen name of an enumeration
  *
  * \return The name of the selected choice of the enumeration 
  * If \c jso is either null or does not reference a JSON-C object
  * which is an enumeration, null is returned.
  */
char const *json_object_enum_get_choice_name(struct json_object* jso)
{
  if (!jso || !json_object_is_type(jso, json_type_enum)) return NULL;
  return json_object_enum_get_name(jso, jso->o.c_enum.choice);
}

/**@} Enumerations */
/**@} Primitive Types */

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

/** \addtogroup prim Primitive Values
  @{ */

/**
 * \addtogroup setprim Set Primitive Object Values
 * \brief Set (overwrite) value associated with a primitive object
 *
 * In all these methods, if the object whose value is being set does not match
 * the type for the given method, the object is unchanged and the return value
 * is \c JSON_C_FALSE. Otherwise, the value is changed and the return value is
 * \c JSON_C_TRUE.
 @{
 */
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
/**@} Set Primitive Object Value */

/**@} Primitive Types */

/* "apath" methods handle paths with possible array indexes embedded within them
   (e.g. "/foo/bar/123/front/567/back") */
static void *json_object_apath_get_leafobj_recurse(struct json_object *src, char *key_path)
{
#warning we need to support . and .. notation here too
    int idx;
    struct json_object *sub_object = 0;
    char *slash_char_p, *next = 0;
    int add1;

    if (!src) return 0;
    if (!key_path) return src;

    add1 = key_path[0]=='/'?1:0;
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

    return 0;
}

static struct json_object* json_object_apath_get_leafobj(struct json_object *obj, char const *key_path)
{
    char *kp = strdup(key_path);
    struct json_object *retval = (struct json_object *) json_object_apath_get_leafobj_recurse(obj, kp);
    free(kp);
    return retval;
}

/**
  * \addtogroup objquery Object Introspection and Query
  @{
  */

/**
 * \addtogroup altpathkeys Alternative Path Queries 
 * \brief JSON object hierarchy path query methods
 *
 * These methods allow querying of a large, JSON object hiearchy using a unix-style
 * path for the keys (e.g. "/foo/bar/gorfo"). These alternative path (or apath) methods
 * support object hierarchies with intermediate arrays. For example,
 * the path "/foo/bar/16/gorfo" where 'bar' is an array object and '16' is the index
 * of the 'bar' array we want to query is allowed. Thus, "/foo/bar/16/gorfo" is
 * conceptually equivalent to "/foo/bar[16]/gorfo". However, if the object at the
 * path "/foo/bar" is not an array and is instead a normal object, then if it contains
 * a member whose key is the string "16", then that path will be queried. Likewise,
 * the path "/gorfo/32/11/dims" where "gorfo" is an array object whose members in turn
 * are also array objects works by first finding the array at index 32 in the 
 * "gorfo" array and then finding the object at index 11 in that array and then
 * finally the "dims" member of that object. In short, whenever possible, the apath
 * methods try to prefer treating path components consisting entirely of numbers
 * as indices into arrays. When this fails at any point in the query process,
 * then the numbers are treated as literal strings to form the key strings to query.
 * These methods also do sane casting (e.g. type coercion) to caller's return type
 * whenever needed.
 @{
 */

/**
 * \brief Query boolean value at specified path
 *
 * For return and type corecion, see json_object_path_get_boolean().
 */
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

/**
 * \brief Query int64_t value at specified path
 *
 * For return and type corecion, see json_object_path_get_int64().
 */
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

/**
 * \brief Query int value at specified path
 *
 * For return and type corecion, see json_object_path_get_int().
 */
int json_object_apath_get_int(struct json_object *obj, char const *key_path)
{
    int64_t val64 = json_object_apath_get_int64(obj, key_path);
    if (INT_MIN <= val64 && val64 <= INT_MAX)
        return (int) val64;
    else
        return 0;
}

/**
 * \brief Query double value at specified path
 *
 * For return and type corecion, see json_object_path_get_double().
 */
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
static int circbuf_idx = 0;
static char *circbuf_retval[CIRCBUF_SIZE];
#define CIRCBUF_RET(STR)                       \
{                                              \
    if (circbuf_retval[circbuf_idx] != 0)      \
        free(circbuf_retval[circbuf_idx]);     \
    circbuf_retval[circbuf_idx] = strdup(STR); \
    retval = circbuf_retval[circbuf_idx];      \
    circbuf_idx = (circbuf_idx+1) % CIRCBUF_SIZE;  \
    return retval;                             \
}

/**
 * \brief Query string value at specified path
 *
 * For return and type corecion, see json_object_path_get_string().
 */
char const *json_object_apath_get_string(struct json_object *obj, char const *key_path)
{
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

/**
 * \brief Query any object at specified path
 *
 * For return and type corecion, see json_object_path_get_any().
 */
struct json_object *json_object_apath_get_object(struct json_object *obj, char const *key_path)
{
    struct json_object *leafobj = json_object_apath_get_leafobj(obj, key_path);
    if (leafobj) return leafobj;
    return 0;
}

struct json_object *json_object_apath_find_object(struct json_object *root, char const *key_path)
{
    struct json_object *foundobj = json_object_apath_get_object(root, key_path);

    /* if we found a matching key_path from root, we're done */
    if (foundobj) return foundobj;

    if (json_object_is_type(root, json_type_object))
    {
        struct json_object_iter iter;

        /* Ok, we need to recurse on the members of root */
        json_object_object_foreachC(root, iter)
        {
            foundobj = json_object_apath_find_object(iter.val, key_path);
            if (foundobj) return foundobj;
        }
    }
    else if (json_object_is_type(root, json_type_array))
    {
        int i;
        for (i = 0; i < json_object_array_length(root); i++)
        {
            struct json_object *arrmember = json_object_array_get_idx(root, i);
            foundobj = json_object_apath_find_object(arrmember, key_path);
            if (foundobj) return foundobj;
        }
    }

    return 0;
}

/**
 * \brief Helper method to automatically construct paths from values
 *
 * This is an internal method called indirectly from JsonGetXXX convenience
 * macros to automatically construct the path string from arguments.
 *
 * Currently, this method works only for cases of strings intermingled with
 * integer variables being used for array indices.
 */
char const *json_paste_apath(
    char const *va_args_str, /**< [in] The string representation of all the arguments */
    char const *first,       /**< [in] The first argument must always be a string */
    ...                      /**< [in] Any remaining arguments */
)
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
/**@} Alternative Path Queries */

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

/**
 * \addtogroup pathsetprim Set Primitive Object Values at Path
 * \brief Set (overwrite) primitive value at given path
 *
 * In all these methods, if an object does not exist at the given path or
 * if its type does not match the type for the given method, the object is
 * unchanged and the return value is \c JSON_C_FALSE. Otherwise, the value
 * is changed and the return value is \c JSON_C_TRUE.
 @{
 */
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
/**@} Set Primitive Object Values at Path */

/**
 * \addtogroup oldpathkeys Obsolete Path Queries 
 * \brief Query JSON object hierarchy using key paths (obsolete)
 *
 * These methods support querying values from a hierarchy of JSON objects
 * using unix-style pathnames for sequences of keys. In addition, for
 * those methods that are intended to return a primitive \em value
 * (e.g. not an object), sane type casting (coercion) is performed if
 * the requested type and matching object's type are different.
 *
 * Due to the need to construct key path strings used in these calls,
 * it is often more convenient to use the JsonGetXXX macros instead.
 * See, for example, JsonGetInt().
 @{
 */

/**
 * \brief Get integer value for object at specified path
 *
 * \return If the object being queried does not exist, zero is returned.
 * If the object being queried exists and is type
 *   - json_bool, \c JSON_C_TRUE returns 1 and \c JSON_C_FALSE returns 0.
 *   - number, the value of the number cast to an int type is returned.
 *   - string
 *     -# holding a number, the value after casting to int the result of strtod on the string is returned.
 *     -# holding anything else, the length of the string is returned.
 *   - an object, the size of the object is returned.
 */
int32_t json_object_path_get_int(struct json_object *src, char const *key_path)
{
    int32_t retval = 0;
    char *kp = strdup(key_path);
    struct json_object *leafobj = json_object_path_get_leafobj(src, key_path, json_type_int);
    free(kp);
    if (leafobj) retval = json_object_get_int(leafobj);
    return retval;
}

/**
  * \brief Get int64 value for object at specified path
  *
  * Same as json_object_path_get_int() except that an \c int64_t type value is returned.
  */
int64_t json_object_path_get_int64(struct json_object *src, char const *key_path)
{
    int64_t retval = 0;
    char *kp = strdup(key_path);
    struct json_object *leafobj = json_object_path_get_leafobj(src, key_path, json_type_int);
    free(kp);
    if (leafobj) retval = json_object_get_int64(leafobj);
    return retval;
}

/**
 * \brief Get a double value for the object at specified path
 *
 * \return If the object being queried does not exist, 0.0 is returned.
 * If the object being queried exists and is type
 *   - json_bool, \c JSON_C_TRUE returns 1.0 and \c JSON_C_FALSE returns 0.0.
 *   - a number, the result of casting the number to type double is returned
 *   - a string
 *     -# holding a number, the value returned from strtod() on the string is returned.
 *     -# holding anything else, the length of the string is returned.
 *   - an object, the size of the object is returned.
 */
double json_object_path_get_double(struct json_object *src, char const *key_path)
{
    double retval = 0;
    char *kp = strdup(key_path);
    struct json_object *leafobj = json_object_path_get_leafobj(src, key_path, json_type_double);
    free(kp);
    if (leafobj) retval = json_object_get_double(leafobj);
    return retval;
}

/**
 * \brief Get boolean value for the object at specified path
 *
 * \return If the object being queried does not exist, \c JSON_C_FALSE is returned.
 * If the object being queried exists and its type is a
 *   - json_bool, its value is returned. 
 *   - number, if the number's value is zero, \c JSON_C_FALSE is returned.
 *   - string, if the string is empty, \c JSON_C_FALSE is returned.
 *   - object, if the length of the object is zero, \c JSON_C_FALSE is returned.
 *   - In all other cases, a value of \c JSON_C_TRUE is returned.
 */
json_bool json_object_path_get_boolean(struct json_object *src, char const *key_path)
{
    json_bool retval = JSON_C_FALSE;
    char *kp = strdup(key_path);
    struct json_object *leafobj = json_object_path_get_leafobj(src, key_path, json_type_boolean);
    free(kp);
    if (leafobj) retval = json_object_get_boolean(leafobj);
    return retval;
}

/**
 * \brief Get a string value for the object at specified path
 *
 * Other path get methods return a fixed size value whether they
 * succeed or fail. Queries for non-existent or mis-matching types are
 * for the most part harmless. Here, however, as long as the object identified
 * by path exists, this method will return a variable length ascii string
 * representation. Therefore, callers should beware of the need to exercise
 * more caution in ensuring the queried strings are actually needed. Also,
 * the resultant strings are cached internally in their respective objects.
 * The caller may wish to free printbuf memory associated with the string.
 * That memory is associated with the queried object. So, the caller must
 * first obtain a handle (pointer) to the queried object. 
 *
 * \code
 * char const *some_string = json_object_path_get_string(obj, "/foo/bar");
 * .
 * . do some work with the string 
 * .
 * json_object *some_obj = json_object_path_get_object(obj, "/foo/bar");
 * json_object_free_printbuf(some_obj);
 * \endcode 
 *
 * \return If the object being queried does not exist, the string \c "null" is returned.
 * If the object being queried exists, the result of json_object_to_json_string_ext() is returned.
 *
 */
char const *json_object_path_get_string(struct json_object *src, char const *key_path)
{
    char const *retval = "";
    char *kp = strdup(key_path);
    struct json_object *leafobj = json_object_path_get_leafobj(src, key_path, json_type_string);
    free(kp);
    if (leafobj) retval = json_object_get_string(leafobj);
    return retval;
}

/**
 * \brief Get the array object at specified path
 *
 * \return If the object being queried is does not exist, <code>(json_object *)0</code> is returned.
 * If the object being queried is not an ordinary array, <code>(json_object *)0</code> is returned.
 * Otherwise, the json_object* for the array object is returned.
 */
struct json_object *json_object_path_get_array(struct json_object *src, char const *key_path)
{
    struct json_object *retval = 0;
    char *kp = strdup(key_path);
    struct json_object *leafobj = json_object_path_get_leafobj(src, key_path, json_type_array);
    free(kp);
    if (leafobj) retval = leafobj;
    return retval;
}

/**
 * \brief Get the object at specified path
 *
 * \return If the object being queried is does not exist, <code>(json_object *)0</code> is returned.
 * If the object being queried is not an ordinary object, <code>(json_object *)0</code> is returned.
 * Otherwise, the json_object* for the object is returned.
 */
struct json_object *json_object_path_get_object(struct json_object *src, char const *key_path)
{
    struct json_object *retval = 0;
    char *kp = strdup(key_path);
    struct json_object *leafobj = json_object_path_get_leafobj(src, key_path, json_type_object);
    free(kp);
    if (leafobj) retval = leafobj;
    return retval;
}

/**
 * \brief Get the extarr object at specified path
 *
 * \return If the object being queried is does not exist, <code>(json_object *)0</code> is returned.
 * If the object being queried is not an extarr object, <code>(json_object *)0</code> is returned.
 * Otherwise, the json_object* for the extarr object is returned.
 */
struct json_object *json_object_path_get_extarr(struct json_object *src, char const *key_path)
{
    struct json_object *retval = 0;
    char *kp = strdup(key_path);
    struct json_object *leafobj = json_object_path_get_leafobj(src, key_path, json_type_extarr);
    free(kp);
    if (leafobj) retval = leafobj;
    return retval;
}

/**
 * \brief Get object of any type at specified path
 *
 * \return If the object being queried is does not exist, <code>(json_object *)0</code> is returned.
 * Otherwise, the json_object* for the object is returned.
 */
struct json_object *json_object_path_get_any(struct json_object *src, char const *key_path)
{
    struct json_object *retval = 0;
    char *kp = strdup(key_path);
    struct json_object *leafobj = json_object_path_get_leafobj(src, key_path, json_type_null);
    free(kp);
    if (leafobj) retval = leafobj;
    return retval;
}
/**@} Obsolete Path Queries */

/**@} Object Introspection and Query */

/**
 * \addtogroup serialize Serialization
 *
 * Ordinarily, JSON-C library keeps the strings resulting from any objects it has serialized
 * cached with the objects and only frees this memory when the object itself is garbage
 * collected (e.g. reference count goes to zero). This is not always convenient. So, this
 * method is provided to free any printbuf string associated with a given object. If the
 * object has no cached printbuf string, the call is harmless.
 @{
 */
void json_object_free_printbuf(struct json_object* jso)
{
  printbuf_free(jso->_pb);
  jso->_pb = 0;
}
/**@} Serialization */

int64_t json_object_object_nbytes(struct json_object* obj, json_bool mode)
{
    static int const objhdr = (int) sizeof(struct json_object);
    int addhdr = mode ? objhdr : 0;

    if (!obj) return 0;
    switch (json_object_get_type(obj))
    {
        case json_type_null:    return 0 + addhdr;
        case json_type_boolean: return sizeof(json_bool) + addhdr;
        case json_type_int:     return sizeof(int64_t) + addhdr;
        case json_type_double:  return sizeof(double) + addhdr;
        case json_type_string:  return json_object_get_string_len(obj) + addhdr;
        case json_type_extarr:
        {
            int64_t retval = json_object_extarr_nbytes(obj) + addhdr;
            if (mode) /* include the space for extarr header */
                retval += (json_object_extarr_ndims(obj)+2) * sizeof(int);
            return retval;
        }
        case json_type_enum:
        {
            if (mode) return json_object_enum_nbytes(obj) + addhdr;
            return sizeof(int64_t);
        }
        case json_type_array:
        {
            int i;
            int64_t retval = 0;
            for (i = 0; i < json_object_array_length(obj); i++)
                retval += json_object_object_nbytes(json_object_array_get_idx(obj, i), mode);
            return retval + addhdr;
        }
        case json_type_object:
        {
            int64_t retval = 0;
            struct json_object_iter iter;
            json_object_object_foreachC(obj, iter)
            {
                retval += json_object_object_nbytes(iter.val, mode);
                if (mode) retval += strlen(iter.key);
            }
            return retval + addhdr;
        }
    }
    return 0;
}

/**@} JSON-C Library */
