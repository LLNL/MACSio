#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#ifdef HAVE_MPI
#include <mpi.h>
#endif
#include <util.h>

#include <json-c/json.h>

typedef struct _knownArgInfo {
   char *helpStr;		/* the help string for this command line argument */
   char *fmtStr;		/* format string for this command line argument */
   int argNameLength;		/* number of characters in this command line argument name */
   int paramCount;		/* number of parameters associated with this argument */
   char *paramTypes;		/* the string of parameter conversion specification characters */
   void **paramPtrs;		/* an array of pointers to caller-supplied scalar variables to be assigned */
   struct _knownArgInfo *next;	/* pointer to the next comand line argument */
} MACSIO_KnownArgInfo_t;

static int GetSizeFromModifierChar(char c)
{
    int n=1;
    switch (c)
    {
        case 'b': case 'B': n=(1<< 0); break;
        case 'k': case 'K': n=(1<<10); break;
        case 'm': case 'M': n=(1<<20); break;
        case 'g': case 'G': n=(1<<30); break;
        default:  n=1; break;
    }
    return n;
}

/*-------------------------------------------------------------------------
  Function: bjhash 

  Purpose: Hash a variable length stream of bytes into a 32-bit value.

  Programmer: By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.

  You may use this code any way you wish, private, educational, or
  commercial.  It's free. However, do NOT use for cryptographic purposes.

  See http://burtleburtle.net/bob/hash/evahash.html
 *-------------------------------------------------------------------------*/

#define bjhash_mix(a,b,c) \
{ \
  a -= b; a -= c; a ^= (c>>13); \
  b -= c; b -= a; b ^= (a<<8); \
  c -= a; c -= b; c ^= (b>>13); \
  a -= b; a -= c; a ^= (c>>12);  \
  b -= c; b -= a; b ^= (a<<16); \
  c -= a; c -= b; c ^= (b>>5); \
  a -= b; a -= c; a ^= (c>>3);  \
  b -= c; b -= a; b ^= (a<<10); \
  c -= a; c -= b; c ^= (b>>15); \
}

unsigned int bjhash(const unsigned char *k, unsigned int length, unsigned int initval)
{
   unsigned int a,b,c,len;

   len = length;
   a = b = 0x9e3779b9;
   c = initval;

   while (len >= 12)
   {
      a += (k[0] +((unsigned int)k[1]<<8) +((unsigned int)k[2]<<16) +((unsigned int)k[3]<<24));
      b += (k[4] +((unsigned int)k[5]<<8) +((unsigned int)k[6]<<16) +((unsigned int)k[7]<<24));
      c += (k[8] +((unsigned int)k[9]<<8) +((unsigned int)k[10]<<16)+((unsigned int)k[11]<<24));
      bjhash_mix(a,b,c);
      k += 12; len -= 12;
   }

   c += length;

   switch(len)
   {
      case 11: c+=((unsigned int)k[10]<<24);
      case 10: c+=((unsigned int)k[9]<<16);
      case 9 : c+=((unsigned int)k[8]<<8);
      case 8 : b+=((unsigned int)k[7]<<24);
      case 7 : b+=((unsigned int)k[6]<<16);
      case 6 : b+=((unsigned int)k[5]<<8);
      case 5 : b+=k[4];
      case 4 : a+=((unsigned int)k[3]<<24);
      case 3 : a+=((unsigned int)k[2]<<16);
      case 2 : a+=((unsigned int)k[1]<<8);
      case 1 : a+=k[0];
   }

   bjhash_mix(a,b,c);

   return c;
}

/* really just an internal function called via MACSIO_ERROR macro */
char const *
_iop_errmsg(const char *format, /* A printf-like error message. */
            ...                 /* Variable length debugging info specified by
                                 * format to be printed out. */
            )
{
  static char error_buffer[1024];
  static int error_buffer_ptr = 0;
  size_t L,Lmax;
  char   tmp[sizeof(error_buffer)];
  va_list ptr;

  va_start(ptr, format);

  vsprintf(tmp, format, ptr);
  L    = strlen(tmp);
  Lmax = sizeof(error_buffer) - error_buffer_ptr - 1;
  if (Lmax < L)
     tmp[Lmax-1] = '\0';
  strcpy(error_buffer + error_buffer_ptr,tmp);

  va_end(ptr);

  return error_buffer;
}

/* Handles adding one or more params for a single key. If the key doesn't
   exist, its a normal object add. If it does exist and is not already an
   array object, delete it and make it into an array object. Otherwise
   add the new param to the existing array object. */
static void
add_param_to_json_retobj(json_object *retobj, char const *key, json_object *addobj)
{
    json_object *existing_member = 0;

    if (json_object_object_get_ex(retobj, key, &existing_member))
    {
        if (json_object_is_type(existing_member, json_type_array))
        {
            json_object_array_add(existing_member, addobj);
        }
        else
        {
            json_object *addarray = json_object_new_array();
            json_object_array_add(addarray, existing_member);
            json_object_array_add(addarray, addobj);
            json_object_get(existing_member);
            json_object_object_add(retobj, key, addarray); /* replaces */
        }
    }
    else
    {
        json_object_object_add(retobj, key, addobj);
    }
}

/*---------------------------------------------------------------------------------------------------------------------------------
 * Audience:	Private
 * Chapter:	Example and Test Utilities	
 * Purpose:	Parse and assign command-line arguments	
 *
 * Description:	This routine is designed to do parsing of simple command-line arguments and assign values associated with
 *		them to caller-supplied scalar variables. It is used in the following manner.
 *
 *		   MACSIO_ProccessCommandLine(argc, argv,
 *		      "-multifile",
 *		         "if specified, use a file-per-timestep",
 *		         &doMultifile,
 *		      "-numCycles %d",
 *		         "specify the number of cycles to run",
 *		         &numCycles,
 *		      "-dims %d %f %d %f",
 *		         "specify size (logical and geometric) of mesh",
 *		         &Ni, &Xi, &Nj, &Xj
 *		      MACSIO_END_OF_ARGS);
 *
 *		After the argc,argv arguments, the remaining arguments come in groups of 3. The first of the three is a
 *		argument format specifier much like a printf statement. It indicates the type of each parameter to the
 *		argument and the number of parameters. Presently, it understands only %d, %f and %s types. the second
 *		of the three is a help line for the argument. Note, you can make this string as long as the C-compiler
 *		will permit. You *need*not* embed any '\n' charcters as the print routine will do that for you.
 *
 *		Command line arguments for which only existence of the argument is tested assume a caller-supplied return
 *		value of int and will be assigned `1' if the argument exists and `0' otherwise.
 *
 *		Do not name any argument with a substring `help' as that is reserved for obtaining help. Also, do not
 *		name any argument with the string `end_of_args' as that is used to indicate the end of the list of
 *		arguments passed to the function.
 *
 *		If any argument on the command line has the substring `help', help will be printed by processor 0 and then
 *		this function calls MPI_Finalize() (in parallel) and exit().
 *
 * Parallel:    This function must be called collectively in MPI_COMM_WORLD. Parallel and serial behavior is identical except in
 *		the
 *
 * Return:	MACSIO_ARGV_OK, MACSIO_ARGV_ERROR or MACSIO_ARGV_HELP
 *
 * Programmer:	Mark Miller, LLNL, Thu Dec 19, 2001 
 *---------------------------------------------------------------------------------------------------------------------------------
 */
int
MACSIO_ProcessCommandLine(
   void **retobj,       /* returned object (for cases that need it) */
   MACSIO_ArgvFlags_t flags, /* flag to indicate what to do if encounter an unknown argument (FATAL|WARN) */
   int argi,            /* first arg index to start processing at */
   int argc,		/* argc from main */
   char **argv,		/* argv from main */
   ...			/* a comma-separated list of 1) argument name and format specifier,  2) help-line for argument,
			   3) caller-supplied scalar variables to set */
)
{
   char *argvStr = NULL;
   int i, rank = 0;
   int helpWasRequested = 0;
   int invalidArgTypeFound = 0;
   int firstArg;
   int terminalWidth = 80 - 10;
   MACSIO_KnownArgInfo_t *knownArgs;
   va_list ap;
   json_object *ret_json_obj = 0;

#ifdef HAVE_MPI
   {  int result;
      if ((MPI_Initialized(&result) != MPI_SUCCESS) || !result)
      { 
         MACSIO_ERROR("MPI is not initialized", flags.error_mode);
         return MACSIO_ARGV_ERROR;
      }
   }
   MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

   /* quick check for a help request */
   if (rank == 0)
   {
      for (i = 0; i < argc; i++)
      {
	 if (strstr(argv[i], "help") != NULL)
	 {
	    char *s;
	    helpWasRequested = 1;
            if ((s=getenv("COLUMNS")) && isdigit(*s))
	       terminalWidth = (int)strtol(s, NULL, 0) - 10;
	    break;
	 }
      }
   }

#ifdef HAVE_MPI
#warning DO JUST ONE BCAST HERE
   MPI_Bcast(&helpWasRequested, 1, MPI_INT, 0, MPI_COMM_WORLD);
#endif

   /* everyone builds the command line parameter list */
   va_start(ap, argv);

   knownArgs = NULL;
   firstArg = 1;
   while (1)
   {
      int n, paramCount, argNameLength;
      char *fmtStr, *helpStr, *p, *paramTypes;
      void **paramPtrs;
      MACSIO_KnownArgInfo_t *newArg, *oldArg;

      /* get this arg's format specifier string */
      fmtStr = va_arg(ap, char *);

      /* check to see if we're done */
      if (!strcmp(fmtStr, MACSIO_END_OF_ARGS))
	 break;

      /* get this arg's help string */
      helpStr = va_arg(ap, char *);

      /* print this arg's help string from proc 0 if help was requested */
      if (helpWasRequested && rank == 0)
      {
	 static int first = 1;
	 char helpFmtStr[32];
	 FILE *outFILE = (isatty(2) ? stderr : stdout);

	 if (first)
	 {
	    first = 0;
	    fprintf(outFILE, "usage and help for %s\n", argv[0]);
	 }

	 /* this arguments format string */
	 fprintf(outFILE, "   %-s\n", fmtStr);

	 /* this arguments help-line format string */
	 sprintf(helpFmtStr, "      %%-%d.%ds", terminalWidth, terminalWidth);

	 /* this arguments help string */
	 p = helpStr;
	 n = (int)strlen(helpStr);
	 i = 0;
	 while (i < n)
	 {
	    fprintf(outFILE, helpFmtStr, p);
	    p += terminalWidth;
	    i += terminalWidth;
	    if ((i < n) && (*p != ' '))
	       fprintf(outFILE, "-\n");
	    else
	       fprintf(outFILE, "\n"); 
	 }
      }

      /* count number of parameters for this argument */
      paramCount = 0;
      n = (int)strlen(fmtStr);
      argNameLength = 0;
      for (i = 0; i < n; i++)
      {
	 if (fmtStr[i] == '%' && fmtStr[i+1] != '%')
	 {
	    paramCount++;
	    if (argNameLength == 0)
	       argNameLength = i-1;
	 }
      }

      if (paramCount)
      {
	 int k;

         /* allocate a string for the conversion characters */
         paramTypes = (char *) malloc((unsigned int)paramCount+1);

         /* allocate a string for the pointers to caller's arguments to set */ 
         paramPtrs = (void **) calloc(paramCount, sizeof(void*));

         /* ok, make a second pass through the string and setup argument pointers and conversion characters */
	 k = 0;
         for (i = 0; i < n; i++)
	 {
	    if (fmtStr[i] == '%' && fmtStr[i+1] != '%')
	    {
	       paramTypes[k] = fmtStr[i+1];
               if (flags.route_mode == MACSIO_ARGV_TOMEM)
               {
	           switch (paramTypes[k])
	           {
	              case 'd': paramPtrs[k] = va_arg(ap, int *); break;
	              case 's': paramPtrs[k] = va_arg(ap, char **); break;
	              case 'f': paramPtrs[k] = va_arg(ap, double *); break;
	              case 'n': paramPtrs[k] = va_arg(ap, int *); break;
	              default:  invalidArgTypeFound = k+1; break;
	           }
               }
	       k++;
	    }
	 }
      }	 
      else
      {
	 /* in this case, we assume we just have a boolean (int) value */
	 argNameLength = n;
	 paramTypes = NULL;
	 paramPtrs = (void **) malloc (sizeof(void*));
         if (flags.route_mode == MACSIO_ARGV_TOMEM)
	     paramPtrs[0] = va_arg(ap, int *);
      }

      /* ok, we're done with this parameter, so plug it into the paramInfo array */
      newArg = (MACSIO_KnownArgInfo_t *) malloc(sizeof(MACSIO_KnownArgInfo_t));
      newArg->helpStr = helpStr;
      newArg->fmtStr = fmtStr;
      newArg->argNameLength = argNameLength;
      newArg->paramCount = paramCount;
      newArg->paramTypes = paramTypes;
      newArg->paramPtrs = paramPtrs;
      newArg->next = NULL; 
      if (firstArg)
      {
	 firstArg = 0;
	 knownArgs = newArg;
	 oldArg = newArg;
      }
      else
         oldArg->next = newArg;
      oldArg = newArg;
   }
   va_end(ap);

#ifdef HAVE_MPI
   MPI_Bcast(&invalidArgTypeFound, 1, MPI_INT, 0, MPI_COMM_WORLD);
#endif
   if (invalidArgTypeFound)
   {
      if (rank == 0)
          MACSIO_ERROR(("invalid argument type encountered at position %d",invalidArgTypeFound), flags.error_mode);
#warning FIX WARN FAILURE BEHAVIOR HERE
      return MACSIO_ARGV_ERROR;
   }

   /* exit if help was requested */
   if (helpWasRequested)
      return MACSIO_ARGV_HELP;

   /* ok, now broadcast the whole argc, argv data */ 
#ifdef HAVE_MPI
   {
      int argvLen;
      char *p;

      /* processor zero computes size of linearized argv and builds it */
      if (rank == 0)
      {
	 if (getenv("MACSIO_IGNORE_UNKNOWN_ARGS"))
	    flags.error_mode = MACSIO_WARN;

         /* compute size of argv */
         argvLen = 0;
         for (i = 0; i < argc; i++)
	    argvLen += (strlen(argv[i])) + 1;

         /* allocate an array of chars to broadcast */
         argvStr = (char *) malloc((unsigned int)argvLen);

         /* now, fill argvStr */
         p = argvStr;
         for (i = 0; i < argc; i++)
         {
	    strcpy(p, argv[i]);
	    p += (strlen(argv[i]) + 1);
	    *(p-1) = '\0';
         }
      }

      /* now broadcast the linearized argv */
      MPI_Bcast(&flags, 1, MPI_INT, 0, MPI_COMM_WORLD);
      MPI_Bcast(&argc, 1, MPI_INT, 0, MPI_COMM_WORLD);
      MPI_Bcast(&argvLen, 1, MPI_INT, 0, MPI_COMM_WORLD);
      if (rank != 0)
         argvStr = (char *) malloc((unsigned int)argvLen);
      MPI_Bcast(argvStr, argvLen, MPI_CHAR, 0, MPI_COMM_WORLD);

      /* Issue: We're relying upon embedded nulls in argvStr */
      /* now, put it back into the argv form */
      if (rank != 0)
      {
	 argv = (char **) malloc(argc * sizeof(char*));
	 p = argvStr;
	 for (i = 0; i < argc; i++)
	 {
	    argv[i] = p;
	    p += (strlen(p) + 1);
	 }
      }
   }
#endif

   /* And now, finally, we can process the arguments and assign them */
   if (flags.route_mode == MACSIO_ARGV_TOJSON)
       ret_json_obj = json_object_new_object();
   i = argi;
   while (i < argc)
   {
      int foundArg;
      MACSIO_KnownArgInfo_t *p;
      char argName[64] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
                          0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

      /* search known arguments for this command line argument */
      p = knownArgs;
      foundArg = 0;
      while (p && !foundArg)
      {
         if (!strncmp(argv[i], p->fmtStr, (unsigned int)(p->argNameLength) ))
         {
            strncpy(argName, argv[i], (unsigned int)(p->argNameLength));
	    foundArg = 1;
         }
	 else
	    p = p->next;
      }
      if (foundArg)
      {
	 int j;

	 /* assign command-line arguments to caller supplied parameters */
	 if (p->paramCount)
	 {
	    for (j = 0; j < p->paramCount; j++)
	    {
               if (i == argc - 1)
                   MACSIO_ERROR(("too few arguments for command-line options"),MACSIO_FATAL);
	       switch (p->paramTypes[j])
	       {
	          case 'd':
	          {
                     int n = strlen(argv[++i])-1;
		     int *pInt = (int *) (p->paramPtrs[j]);
                     int tmpInt;
                     double tmpDbl;
                     n = GetSizeFromModifierChar(argv[i][n]);
		     tmpInt = strtol(argv[i], (char **)NULL, 10);
                     tmpDbl = tmpInt * n;
                     if ((int)tmpDbl != tmpDbl)
                     {
                         MACSIO_ERROR(("integer overflow (%.0f) for arg \"%s\"",tmpDbl,argv[i-1]), flags.error_mode);
                     }
                     else
                     {
                         if (flags.route_mode == MACSIO_ARGV_TOMEM)
                             *pInt = (int) tmpDbl;
                         else if (flags.route_mode == MACSIO_ARGV_TOJSON)
                             add_param_to_json_retobj(ret_json_obj, argName, json_object_new_int((int)tmpDbl));
                     }
		     break;
	          }
	          case 's':
	          {
		     char **pChar = (char **) (p->paramPtrs[j]);
		     if (pChar && *pChar == NULL)
		     {
                         if (flags.route_mode == MACSIO_ARGV_TOMEM)
                         {
		             *pChar = (char*) malloc(strlen(argv[++i]+1));
		             strcpy(*pChar, argv[i]);
                         }
                         else if (flags.route_mode == MACSIO_ARGV_TOJSON)
                         {
                             add_param_to_json_retobj(ret_json_obj, argName, json_object_new_string(argv[i+1]));
                             i++;
                         }
		     }
		     else
                     {
                         if (flags.route_mode == MACSIO_ARGV_TOMEM)
		             strcpy(*pChar, argv[++i]);
                         else if (flags.route_mode == MACSIO_ARGV_TOJSON)
                             add_param_to_json_retobj(ret_json_obj, argName, json_object_new_string(argv[++i]));
                     }
		     break;
	          }
	          case 'f':
	          {
		     double *pDouble = (double *) (p->paramPtrs[j]);
                     if (flags.route_mode == MACSIO_ARGV_TOMEM)
		         *pDouble = atof(argv[++i]);
                     else if (flags.route_mode == MACSIO_ARGV_TOJSON)
                         add_param_to_json_retobj(ret_json_obj, argName, json_object_new_double(atof(argv[++i])));
		     break;
	          }
	          case 'n': /* special case to return arg index */
	          {
		     int *pInt = (int *) (p->paramPtrs[j]);
                     if (flags.route_mode == MACSIO_ARGV_TOMEM)
		         *pInt = i++;
                     else if (flags.route_mode == MACSIO_ARGV_TOJSON)
                         add_param_to_json_retobj(ret_json_obj, "argi", json_object_new_int(i++));
		     break;
	          }
               }
   	    }
	 }
	 else
	 {
            int *pInt = (int *) (p->paramPtrs[0]);
            if (flags.route_mode == MACSIO_ARGV_TOMEM)
                *pInt = 1; 
            else if (flags.route_mode == MACSIO_ARGV_TOJSON)
                add_param_to_json_retobj(ret_json_obj, argName, json_object_new_boolean((json_bool)1));
	 }
      }
      else
      {
	 char *p = strrchr(argv[0], '/');
	 FILE *outFILE = (isatty(2) ? stderr : stdout);
	 p = p ? p+1 : argv[0];
	 if (rank == 0)
             MACSIO_ERROR(("%s: unknown argument %s. Type %s --help for help",p,argv[i],p), flags.error_mode);
         return MACSIO_ARGV_ERROR; 
      }

      /* move to next argument */
      i++;
   }

   /* free argvStr */
   if (argvStr)
      free(argvStr);

   /* free the argv pointers we created, locally */
   if (rank != 0 && argv)
      free(argv);

   /* free the known args stuff */
   while (knownArgs)
   {
      MACSIO_KnownArgInfo_t *next = knownArgs->next;

      if (knownArgs->paramTypes)
         free(knownArgs->paramTypes);
      if (knownArgs->paramPtrs)
         free(knownArgs->paramPtrs);
      free(knownArgs);
      knownArgs = next;
   }

   if (flags.route_mode == MACSIO_ARGV_TOJSON)
       *retobj = ret_json_obj;

   return MACSIO_ARGV_OK;
}
