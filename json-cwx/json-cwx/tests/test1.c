#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#include "json.h"
#include "parse_flags.h"

static int sort_fn (const void *j1, const void *j2)
{
	json_object * const *jso1, * const *jso2;
	int i1, i2;

	jso1 = (json_object* const*)j1;
	jso2 = (json_object* const*)j2;
	if (!*jso1 && !*jso2)
		return 0;
	if (!*jso1)
		return -1;
	if (!*jso2)
		return 1;

	i1 = json_object_get_int(*jso1);
	i2 = json_object_get_int(*jso2);

	return i1 - i2;
}

#ifdef TEST_FORMATTED
#define json_object_to_json_string(obj) json_object_to_json_string_ext(obj,sflags)
#else
/* no special define */
#endif

int main(int argc, char **argv)
{
	json_object *my_string, *my_int, *my_object, *my_array;
	int i;
#ifdef TEST_FORMATTED
	int sflags = 0;
#endif

	MC_SET_DEBUG(1);

#ifdef TEST_FORMATTED
	sflags = parse_flags(argc, argv);
#endif

	my_string = json_object_new_string("\t");
	printf("my_string=%s\n", json_object_get_string(my_string));
	printf("my_string.to_string()=%s\n", json_object_to_json_string(my_string));
	json_object_put(my_string);

	my_string = json_object_new_string("\\");
	printf("my_string=%s\n", json_object_get_string(my_string));
	printf("my_string.to_string()=%s\n", json_object_to_json_string(my_string));
	json_object_put(my_string);

	my_string = json_object_new_string("foo");
	printf("my_string=%s\n", json_object_get_string(my_string));
	printf("my_string.to_string()=%s\n", json_object_to_json_string(my_string));

	my_int = json_object_new_int(9);
	printf("my_int=%d\n", json_object_get_int(my_int));
	printf("my_int.to_string()=%s\n", json_object_to_json_string(my_int));

	my_array = json_object_new_array();
	json_object_array_add(my_array, json_object_new_int(1));
	json_object_array_add(my_array, json_object_new_int(2));
	json_object_array_add(my_array, json_object_new_int(3));
	json_object_array_put_idx(my_array, 4, json_object_new_int(5));
	printf("my_array=\n");
	for(i=0; i < json_object_array_length(my_array); i++)
	{
		json_object *obj = json_object_array_get_idx(my_array, i);
		printf("\t[%d]=%s\n", i, json_object_to_json_string(obj));
	}
	printf("my_array.to_string()=%s\n", json_object_to_json_string(my_array));    

	json_object_put(my_array);

	my_array = json_object_new_array();
	json_object_array_add(my_array, json_object_new_int(3));
	json_object_array_add(my_array, json_object_new_int(1));
	json_object_array_add(my_array, json_object_new_int(2));
	json_object_array_put_idx(my_array, 4, json_object_new_int(0));
	printf("my_array=\n");
	for(i=0; i < json_object_array_length(my_array); i++)
	{
		json_object *obj = json_object_array_get_idx(my_array, i);
		printf("\t[%d]=%s\n", i, json_object_to_json_string(obj));
	}
	printf("my_array.to_string()=%s\n", json_object_to_json_string(my_array));    
	json_object_array_sort(my_array, sort_fn);
	printf("my_array=\n");
	for(i=0; i < json_object_array_length(my_array); i++)
	{
		json_object *obj = json_object_array_get_idx(my_array, i);
		printf("\t[%d]=%s\n", i, json_object_to_json_string(obj));
	}
	printf("my_array.to_string()=%s\n", json_object_to_json_string(my_array));    

	my_object = json_object_new_object();
	json_object_object_add(my_object, "abc", json_object_new_int(12));
	json_object_object_add(my_object, "foo", json_object_new_string("bar"));
	json_object_object_add(my_object, "bool0", json_object_new_boolean(0));
	json_object_object_add(my_object, "bool1", json_object_new_boolean(1));
	json_object_object_add(my_object, "baz", json_object_new_string("bang"));

	json_object *baz_obj = json_object_new_string("fark");
	json_object_get(baz_obj);
	json_object_object_add(my_object, "baz", baz_obj);
	json_object_object_del(my_object, "baz");

	/* baz_obj should still be valid */
	printf("baz_obj.to_string()=%s\n", json_object_to_json_string(baz_obj));
	json_object_put(baz_obj);

	/*json_object_object_add(my_object, "arr", my_array);*/
	printf("my_object=\n");
	json_object_object_foreach(my_object, key, val)
	{
		printf("\t%s: %s\n", key, json_object_to_json_string(val));
	}
	printf("my_object.to_string()=%s\n", json_object_to_json_string(my_object));

	json_object_put(my_string);
	json_object_put(my_int);
	json_object_put(my_object);
	json_object_put(my_array);

        {
          int i;
          int dims[3] = {2, 7, 3}; 
          int nvals = dims[0]*dims[1]*dims[2];
          struct json_object *extarr = json_object_new_extarr_alloc(json_extarr_type_flt64, 3, dims);
          float *fbuf = 0;
          int *ibuf;
          for (i = 0; i < nvals; i++)
          {
            double *p = (double*)json_object_extarr_data(extarr)+i;
            *p = (double) i/2.0;
          }
	  printf("extarr.to_string()=%s\n", json_object_to_json_string(extarr));
          printf("extarr_crc() = %lx\n", json_object_extarr_crc(extarr));
          {
            struct _bits {
                unsigned int bit00:1;
                unsigned int bit01:1;
                unsigned int bit02:1;
                unsigned int bit03:1;
                unsigned int bit04:1;
                unsigned int bit05:1;
                unsigned int bit06:1;
                unsigned int bit07:1;
                unsigned int bit08:1;
                unsigned int bit09:1;
                unsigned int bit10:1;
                unsigned int bit11:1;
                unsigned int bit12:1;
                unsigned int bit13:1;
                unsigned int bit14:1;
                unsigned int bit15:1;
                unsigned int bit16:1;
                unsigned int bit17:1;
                unsigned int bit18:1;
                unsigned int bit19:1;
                unsigned int bit20:1;
                unsigned int bit21:1;
                unsigned int bit22:1;
                unsigned int bit23:1;
                unsigned int bit24:1;
                unsigned int bit25:1;
                unsigned int bit26:1;
                unsigned int bit27:1;
                unsigned int bit28:1;
                unsigned int bit29:1;
                unsigned int bit30:1;
                unsigned int bit31:1;
            } *pbits = (struct _bits*) json_object_extarr_data(extarr)+17;
            pbits->bit16 = ~pbits->bit16;
          }
          printf("extarr_crc() = %lx\n", json_object_extarr_crc(extarr));

          json_object_extarr_data_as_float(extarr, &fbuf);
          for (i = 0; i < nvals; i++)
              printf("fbuf[%d]=%f\n",i,fbuf[i]);
          free(fbuf);

          ibuf = (int *) malloc(nvals * sizeof(int));
          json_object_extarr_data_as_int(extarr, &ibuf);
          for (i = 0; i < nvals; i++)
              printf("ibuf[%d]=%d\n",i,ibuf[i]);
          free(ibuf);
        
          json_object_put(extarr);
        }
        {
          const char *input = "( 6, 3, 2, 7, 3, 0, 0.5, 1, 1.5, 2, 2.5, 3, 3.5, 4, 4.5, 5, 5.5, 6, 6.5, 7, 7.5, 8, 8.5, 9, 9.5, 10, 10.5, 11, 11.5, 12, 12.5, 13, 13.5, 14, 14.5, 15, 15.5, 16, 16.5, 17, 17.5, 18, 18.5, 19, 19.5, 20, 20.5 )";
          struct json_object *parse_result = json_tokener_parse((char*)input);
          const char *unjson = json_object_get_string(parse_result);
	  printf("parse_result=%s\n", unjson);
        }

        {
            json_object *filobj = json_object_from_file("foo.json");
            json_object *tmpobj;
	    printf("filobj (without extarr vals) =%s\n",
                json_object_to_json_string_ext(filobj,
                   JSON_C_TO_STRING_NO_EXTARR_VALS|JSON_C_TO_STRING_SPACED));
	    printf("filobj=%s, nbytes=%d\n", json_object_to_json_string(filobj),
                json_object_object_nbytes(filobj,0));
            printf("\"steve/cameron/b\" = %d, \"abc\" = %d\n",
                json_object_path_get_int(filobj, "steve/cameron/b"),
                json_object_path_get_int(filobj, "abc"));
            if (!json_object_path_set_int(filobj, "steve/cameron/b", 37))
                printf("path_set_int failed\n");
            if (!json_object_path_set_int(filobj, "abc", 91))
                printf("path_set_int failed\n");
            printf("\"steve/cameron/b\" = %d, \"abc\" = %d\n",
                json_object_path_get_int(filobj, "steve/cameron/b"),
                json_object_path_get_int(filobj, "abc"));
            printf("\"steve/cameron/b\" = %d, \"abc\" = %d\n",
                json_object_apath_get_int(filobj, "steve/cameron/b"),
                json_object_apath_get_int(filobj, "abc"));
            printf("\"array/2\" = %d\n",
                json_object_apath_get_int(filobj, "array/2"));
            printf("\"array2/6/cameron/a\" = %d\n",
                JsonGetInt(filobj, "array2",6,"cameron/a"));
            printf("\"array2/6/cameron/a\" = %d\n",
                JsonGetInt(filobj, "array2/",6,"/cameron/a"));
            tmpobj = JsonGetObj(filobj, "array2");
            printf("\"6/cameron/a\" = %d\n",
                JsonGetInt(tmpobj, "",6,"/cameron/a"));
            printf("\"array2/2\" = \"%s\"\n",
                json_object_apath_get_string(filobj, "array2/2"));
            printf("Finding \"cameron/a\" from root = %d\n",
                json_object_get_int(JsonFindObj(filobj, "cameron/a")));
        }
        {
            struct json_object *eobj = json_object_new_enum();
            json_object_enum_add(eobj, "val31", 31, JSON_C_FALSE);
            json_object_enum_add(eobj, "val276", 276, JSON_C_FALSE);
            json_object_enum_add(eobj, "val17", 17, JSON_C_TRUE);
	    printf("eobj=%s\n", json_object_to_json_string(eobj));
        }

	return 0;
}
