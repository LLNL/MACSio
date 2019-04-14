/*
Copyright (c) 2015, Lawrence Livermore National Security, LLC.
Produced at the Lawrence Livermore National Laboratory.
Written by Mark C. Miller

LLNL-CODE-676051. All rights reserved.

This file is part of MACSio

Please also read the LICENSE file at the top of the source code directory or
folder hierarchy.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License (as published by the Free Software
Foundation) version 2, dated June 1991.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the IMPLIED WARRANTY OF MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the terms and conditions of the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA
*/

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#ifdef HAVE_MPI
#include <mpi.h>
#endif

#include <json-cwx/json.h>

#include <macsio_clargs.h>
#include <macsio_log.h>
#include <macsio_utils.h>

/* Flags for parameters */
#define PARAM_ASSIGNED 0x01

typedef struct _knownArgInfo {
   char *helpStr;		/**< the help string for this command line argument */
   char *fmtStr;		/**< format string for this command line argument */
   char *defStr;                /**< Strong holding default values for arg params */
   char *argName;               /**< To get populated with the argument's name without leading dashes */
   int argNameLength;		/**< number of characters in this command line argument name */
   int paramCount;		/**< number of parameters associated with this argument */
   char *paramTypes;		/**< the string of parameter conversion specification characters */
   void **paramPtrs;		/**< an array of pointers to caller-supplied scalar variables to be assigned */
   char *paramFlags;            /**< Flags for each param to indicate things such as if the param was
                                     actually assigned a value */
   struct _knownArgInfo *next;	/**< pointer to the next comand line argument */
} MACSIO_KnownArgInfo_t;

static double GetSizeFromModifierChar(char c)
{
    if (!strncasecmp(MACSIO_UTILS_UnitsPrefixSystem, "decimal",
        sizeof(MACSIO_UTILS_UnitsPrefixSystem)))
    {
        switch (c)
        {
            case 'k': case 'K': return 1000.0;
            case 'm': case 'M': return 1000.0*1000.0;
            case 'g': case 'G': return 1000.0*1000.0*1000.0;
            case 't': case 'T': return 1000.0*1000.0*1000.0*1000.0;
            case 'p': case 'P': return 1000.0*1000.0*1000.0*1000.0*1000.0;
        }
    }
    else
    {
        switch (c)
        {
            case 'k': case 'K': return 1024.0;
            case 'm': case 'M': return 1024.0*1024.0;
            case 'g': case 'G': return 1024.0*1024.0*1024.0;
            case 't': case 'T': return 1024.0*1024.0*1024.0*1024.0;
            case 'p': case 'P': return 1024.0*1024.0*1024.0*1024.0*1024.0;
        }
    }

    return 1;
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

int
MACSIO_CLARGS_ProcessCmdline(
   void **retobj,
   MACSIO_CLARGS_ArgvFlags_t flags,
   int argi,
   int argc,
   char **argv,
   ...
)
{
   FILE *outFILE;
   char *argvStr = NULL;
   int i, rank = 0;
   int helpWasRequested = 0;
   int strictIsDisabled = 0;
   int invalidArgTypeFound = 0;
   int firstArg;
   int terminalWidth = 120 - 10;
   int haveSeenSeparatorArg = 0;
   MACSIO_KnownArgInfo_t *knownArgs, *newArg, *oldArg;
   va_list ap;
   json_object *ret_json_obj = 0;
   int depth;
   MACSIO_LOG_MsgSeverity_t msgSeverity = flags.error_mode?MACSIO_LOG_MsgDie:MACSIO_LOG_MsgErr;

#ifdef HAVE_MPI
   {  int result;
      if ((MPI_Initialized(&result) != MPI_SUCCESS) || !result)
      { 
         MACSIO_LOG_MSGLV(MACSIO_LOG_StdErr, msgSeverity,
             ("MPI is not initialized"));
         return MACSIO_CLARGS_ERROR;
      }
   }
   MPI_Comm_rank(MPI_COMM_WORLD, &rank);
#endif

   /* quick check for special args (--help, --no-strict) */
   if (rank == 0)
   {
      for (i = 0; i < argc; i++)
      {
	 if (strcasestr(argv[i], "--no-strict") != NULL)
             msgSeverity = MACSIO_LOG_MsgWarn;
	 if (strcasestr(argv[i], "--help") != NULL)
	    helpWasRequested = 1;
      }
      if (helpWasRequested)
      {
         char cmd[64];
	 char *s;
         if ((s=getenv("COLUMNS")) && isdigit(*s))
	    terminalWidth = (int)strtol(s, NULL, 0) - 8;
         else
         {
            struct winsize ws;
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1 && errno == 0)
               terminalWidth = ws.ws_col;
            else if (ioctl(STDERR_FILENO, TIOCGWINSZ, &ws) != -1 && errno == 0)
               terminalWidth = ws.ws_col;
            else
               terminalWidth = 80; /* best we can do is assume */
         }
         snprintf(cmd, sizeof(cmd), "fmt -w %d", terminalWidth);
         outFILE = popen(cmd, "w");
         if (!outFILE)
	     outFILE = (isatty(2) ? stderr : stdout);
      }
   }

#ifdef HAVE_MPI
   /* Ensure all tasks agree on these flags */
   {
      int tmp[2] = {helpWasRequested, msgSeverity};
      MPI_Bcast(&tmp, sizeof(tmp)/sizeof(tmp[0]), MPI_INT, 0, MPI_COMM_WORLD);
      helpWasRequested = tmp[0];
      msgSeverity = (MACSIO_LOG_MsgSeverity_t) tmp[1];
   }
#endif

   /* everyone builds the command line parameter list */
   va_start(ap, argv);

   knownArgs = NULL;
   firstArg = 1;
   depth = 1;
   while (1)
   {
      int n, paramCount, argNameLength;
      char *fmtStr, *defStr, *helpStr, *p, *paramTypes, *paramFlags;
      void **paramPtrs;
      int isgroup_begin, isgroup_end;

      /* get this arg's format specifier string */
      fmtStr = va_arg(ap, char *);

      /* check to see if we're done */
      if (!strncmp(fmtStr, MACSIO_CLARGS_END_OF_ARGS, strlen(MACSIO_CLARGS_END_OF_ARGS)))
	 break;

      /* check to see if this is a grouping item */
      isgroup_begin = isgroup_end = 0;
      if (!strncmp(fmtStr, MACSIO_CLARGS_GRP_BEG, strlen(MACSIO_CLARGS_GRP_BEG)))
      {
          isgroup_begin = 1;
          depth++;
      }
      else if (!strncmp(fmtStr, MACSIO_CLARGS_GRP_END, strlen(MACSIO_CLARGS_GRP_END)))
      {
          isgroup_end = 1;
          depth--;
      }

      /* get this arg's default string */
      defStr = va_arg(ap, char *);

      /* get this arg's help string */
      helpStr = va_arg(ap, char *);

      /* print this arg's help string from proc 0 if help was requested */
      if (helpWasRequested && rank == 0)
      {
         static int first = 1;
	 char helpFmtStr[32];
         int has_embedded_newlines = strchr(helpStr, '\n') != 0;
         int help_str_len = strlen(helpStr);

	 if (first)
	 {
            first = 0;
	    fprintf(outFILE, "Usage and Help for \"%s\"\n", argv[0]);
	    fprintf(outFILE, "Defaults, if any, in square brackets after argument definition\n");
	 }

	 /* this arguments format string */
         if (isgroup_begin)
	     fprintf(outFILE, "\n%*s%s\n", 2*(depth-1), " ", &fmtStr[strlen(MACSIO_CLARGS_GRP_BEG)]);
         else if (isgroup_end)
	     fprintf(outFILE, "\n");
         else
	     fprintf(outFILE, "%*s%s [%s]\n", 2*depth, " ", fmtStr, defStr?defStr:"");

         if (has_embedded_newlines || help_str_len < terminalWidth)
         {
	     p = helpStr;
             while (p)
             {
                 char *pnext = strchr(p, '\n');
                 int len = pnext ? pnext - p : strlen(p);
	         fprintf(outFILE, "%*s%*.*s\n", 2*(depth+1)+2, " ", len, len, p);
                 p = pnext ? pnext+1 : 0;
             }
         }
         else
         {
	     /* this arguments help-line format string */
	     sprintf(helpFmtStr, "%%*s%%-%d.%ds", terminalWidth, terminalWidth);

	     /* this arguments help string */
	     p = helpStr;
	     n = (int)strlen(helpStr);
	     i = 0;
	     while (i < n)
	     {
	        fprintf(outFILE, helpFmtStr, 2*(depth+1)+2, " ", p);
	        p += terminalWidth;
	        i += terminalWidth;
	        if ((i < n) && (*p != ' '))
	           fprintf(outFILE, "-\n");
	        else
	           fprintf(outFILE, "\n"); 
	     }
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

         /* allocate flags for params */
         paramFlags = (char*) calloc(paramCount+1, sizeof(char));

         /* ok, make a second pass through the string and setup argument pointers and conversion characters */
	 k = 0;
         for (i = 0; i < n; i++)
	 {
	    if (fmtStr[i] == '%' && fmtStr[i+1] != '%')
	    {
	       paramTypes[k] = fmtStr[i+1];
               if (flags.route_mode == MACSIO_CLARGS_TOMEM)
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
	 paramPtrs = (void **) malloc(sizeof(void*));
         paramFlags = (char*) malloc(sizeof(char));
         if (flags.route_mode == MACSIO_CLARGS_TOMEM)
	     paramPtrs[0] = va_arg(ap, int *);
      }

      /* ok, we're done with this parameter, so plug it into the paramInfo array */
      newArg = (MACSIO_KnownArgInfo_t *) calloc(1, sizeof(MACSIO_KnownArgInfo_t));
      newArg->helpStr = helpStr;
      newArg->fmtStr = fmtStr;
      newArg->defStr = defStr;
      if (fmtStr[0] == '-')
      {
          if (fmtStr[1] == '-')
              newArg->argName = strndup(&fmtStr[2], argNameLength-2);
          else
              newArg->argName = strndup(&fmtStr[1], argNameLength-1);
      }
      else
      {
          newArg->argName = strndup(&fmtStr[0], argNameLength-0);
      }
      newArg->argNameLength = argNameLength;
      newArg->paramCount = paramCount;
      newArg->paramTypes = paramTypes;
      newArg->paramPtrs = paramPtrs;
      newArg->paramFlags = paramFlags;
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
          MACSIO_LOG_MSGLV(MACSIO_LOG_StdErr, msgSeverity,
              ("invalid argument type encountered at position %d",invalidArgTypeFound));
//#warning FIX WARN FAILURE BEHAVIOR HERE
      return MACSIO_CLARGS_ERROR;
   }

   /* exit if help was requested */
   if (helpWasRequested)
   {
      if (rank == 0 && outFILE != stderr && outFILE != stdout)
          pclose(outFILE);
      return MACSIO_CLARGS_HELP;
   }

   /* ok, now broadcast the whole argc, argv data */ 
#ifdef HAVE_MPI
   {
      int argvLen;
      char *p;

      /* processor zero computes size of linearized argv and builds it */
      if (rank == 0)
      {
//#warning MAKE THIS A CLARG
	 if (getenv("MACSIO_CLARGS_IGNORE_UNKNOWN_ARGS"))
	    flags.error_mode = MACSIO_CLARGS_WARN;

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
   if (flags.route_mode == MACSIO_CLARGS_TOJSON)
       ret_json_obj = json_object_new_object();
   i = argi;
   while (i < argc && !haveSeenSeparatorArg)
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
               {
                   MACSIO_LOG_MSGLV(MACSIO_LOG_StdErr, msgSeverity, ("too few arguments for command-line options"));
                   break;
               }
	       switch (p->paramTypes[j])
	       {
	          case 'd':
	          {
                     int n = strlen(argv[++i])-1;
                     int tmpInt;
                     double tmpDbl, ndbl;
                     ndbl = GetSizeFromModifierChar(argv[i][n]);
		     tmpInt = strtol(argv[i], (char **)NULL, 10);
                     tmpDbl = tmpInt * ndbl;
                     if ((int)tmpDbl != tmpDbl)
                     {
                         MACSIO_LOG_MSGLV(MACSIO_LOG_StdErr, msgSeverity,
                             ("integer overflow (%.0f) for arg \"%s\"",tmpDbl,argv[i-1]));
                     }
                     else
                     {
                         if (flags.route_mode == MACSIO_CLARGS_TOMEM)
                         {
		             int *pInt = (int *) (p->paramPtrs[j]);
                             *pInt = (int) tmpDbl;
                         }
                         else if (flags.route_mode == MACSIO_CLARGS_TOJSON)
                             add_param_to_json_retobj(ret_json_obj, &argName[2], json_object_new_int((int)tmpDbl));
                         p->paramFlags[j] |= PARAM_ASSIGNED;
                     }
		     break;
	          }
	          case 's':
	          {
                     i++;
                     if (flags.route_mode == MACSIO_CLARGS_TOMEM)
                     {
		         char **pChar = (char **) (p->paramPtrs[j]);
                         if (*pChar == NULL)
		             *pChar = (char*) malloc(strlen(argv[i])+1);
		         strcpy(*pChar, argv[i]);
                     }
                     else if (flags.route_mode == MACSIO_CLARGS_TOJSON)
                         add_param_to_json_retobj(ret_json_obj, &argName[2], json_object_new_string(argv[i]));
                     p->paramFlags[j] |= PARAM_ASSIGNED;
		     break;
	          }
	          case 'f':
	          {
                     if (flags.route_mode == MACSIO_CLARGS_TOMEM)
                     {
		         double *pDouble = (double *) (p->paramPtrs[j]);
		         *pDouble = atof(argv[++i]);
                     }
                     else if (flags.route_mode == MACSIO_CLARGS_TOJSON)
                         add_param_to_json_retobj(ret_json_obj, &argName[2], json_object_new_double(atof(argv[++i])));
                     p->paramFlags[j] |= PARAM_ASSIGNED;
		     break;
	          }
	          case 'n': /* special case to return arg index */
	          {
                     haveSeenSeparatorArg = 1;
                     if (flags.route_mode == MACSIO_CLARGS_TOMEM)
                     {
		         int *pInt = (int *) (p->paramPtrs[j]);
		         *pInt = i++;
                     }
                     else if (flags.route_mode == MACSIO_CLARGS_TOJSON)
                         add_param_to_json_retobj(ret_json_obj, "argi", json_object_new_int(i++));
                     p->paramFlags[j] |= PARAM_ASSIGNED;
		     break;
	          }
               }
   	    }
	 }
	 else
	 {
            if (flags.route_mode == MACSIO_CLARGS_TOMEM)
            {
                int *pInt = (int *) (p->paramPtrs[0]);
                *pInt = 1; 
                p->paramFlags[0] |= PARAM_ASSIGNED;
            }
            else if (flags.route_mode == MACSIO_CLARGS_TOJSON)
            {
                add_param_to_json_retobj(ret_json_obj, &argName[2], json_object_new_boolean((json_bool)1));
                p->paramFlags[0] |= PARAM_ASSIGNED;
            }
	 }
      }
      else
      {
	 char *p = strrchr(argv[0], '/');
	 p = p ? p+1 : argv[0];
         errno = EINVAL; /* if dieing, ensure executable returns useful error code */
#ifdef HAVE_MPI
         mpi_errno = MPI_SUCCESS;
#endif
	 if (rank == 0)
             MACSIO_LOG_MSGLV(MACSIO_LOG_StdErr, msgSeverity,
                 ("%s: unknown argument %s. Type %s --help for help",p,argv[i],p));
         return MACSIO_CLARGS_ERROR; 
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

   /* find any args for which defaults are specified but a value
      wasn't assigned from arc/argv and assign the default value(s). */
   if (flags.defaults_mode == MACSIO_CLARGS_ASSIGN_ON)
   {
       MACSIO_KnownArgInfo_t *pka = knownArgs;
       while (pka)
       {
          if (pka->defStr)
          {
              if (pka->paramCount)
              {
                  int j;
                  char *defStr, *defStrOrig;
                  defStr = defStrOrig = strdup(pka->defStr);
                  for (j = 0; j < pka->paramCount; j++)
                  {
                      char *defParam = strsep(&defStr, " ");

                      if (pka->paramFlags[j] & PARAM_ASSIGNED) continue;

                      MACSIO_LOG_MSGL(MACSIO_LOG_StdErr, Dbg2,
                          ("Default value of \"%s\" assigned for param %d of arg \"%s\"",defParam,j,pka->argName));

	              switch (pka->paramTypes[j])
	              {
	                 case 'd':
	                 {
                            int n = strlen(defParam)-1;
                            int tmpInt;
                            double tmpDbl, ndbl;
                            ndbl = GetSizeFromModifierChar(defParam[n]);
		            tmpInt = strtol(defParam, (char **)NULL, 10);
                            tmpDbl = tmpInt * ndbl;
                            if (flags.route_mode == MACSIO_CLARGS_TOMEM)
                            {
		                int *pInt = (int *) (pka->paramPtrs[j]);
                                *pInt = (int) tmpDbl;
                            }
                            else if (flags.route_mode == MACSIO_CLARGS_TOJSON)
                                add_param_to_json_retobj(ret_json_obj, pka->argName, json_object_new_int((int)tmpDbl));
		            break;
	                 }
	                 case 's':
	                 {
                            if (flags.route_mode == MACSIO_CLARGS_TOMEM)
                            {
		                char **pChar = (char **) (pka->paramPtrs[j]);
                                if (*pChar == NULL)
		                    *pChar = (char*) malloc(strlen(defParam)+1);
		                strcpy(*pChar, defParam);
                            }
                            else if (flags.route_mode == MACSIO_CLARGS_TOJSON)
                            {
                                add_param_to_json_retobj(ret_json_obj, pka->argName, json_object_new_string(defParam));
                            }
		            break;
                         }
	                 case 'f':
	                 {
                            if (flags.route_mode == MACSIO_CLARGS_TOMEM)
                            {
		                double *pDouble = (double *) (pka->paramPtrs[j]);
		                *pDouble = atof(defParam);
                            }
                            else if (flags.route_mode == MACSIO_CLARGS_TOJSON)
                                add_param_to_json_retobj(ret_json_obj, pka->argName, json_object_new_double(atof(defParam)));
		            break;
	                 }
                      }
                  }
                  free(defStrOrig);
              }
              else /* boolean arg case */
              {
                  if (!(pka->paramFlags[0] & PARAM_ASSIGNED))
                  {
                      if (flags.route_mode == MACSIO_CLARGS_TOMEM)
                      {
                          int *pInt = (int *) (pka->paramPtrs[0]);
                          *pInt = 0; 
                      }
                      else if (flags.route_mode == MACSIO_CLARGS_TOJSON)
                          add_param_to_json_retobj(ret_json_obj, pka->argName, json_object_new_boolean((json_bool)0));
                  }
              }
          }
          pka = pka->next;
       }
   }

   /* free the known args stuff */
   while (knownArgs)
   {
      MACSIO_KnownArgInfo_t *next = knownArgs->next;
      if (knownArgs->paramTypes)
         free(knownArgs->paramTypes);
      if (knownArgs->paramPtrs)
         free(knownArgs->paramPtrs);
      if (knownArgs->paramFlags)
         free(knownArgs->paramFlags);
      if (knownArgs->argName)
         free(knownArgs->argName);
      free(knownArgs);
      knownArgs = next;
   }

   if (flags.route_mode == MACSIO_CLARGS_TOJSON)
       *retobj = ret_json_obj;

   errno = 0;
   return MACSIO_CLARGS_OK;
}
