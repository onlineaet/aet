/*
 * Copyright (C) 2026  zclei
 * This file is part of AET.

 * AET is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3, or (at your option) any later
 * version.

 * AET is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.

 * You should have received a copy of the GNU General Public License
 * along with GCC Exception along with this program; see the file COPYING3.
 * If not see <http://www.gnu.org/licenses/>.
 * AET was originally developed  by the zclei@sina.com
 */
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "AAssert.h"
#include "AString.h"

impl$ AAssert{


	static void log(int type,const char *string1,const char *string2,int n_args,long double *largs){
	    a_print ("(ERROR: %s)\n", string1);
	}

	static void message (const char *file,int line,const char *func,const char *message){
          char *domain=NULL;
          char lstr[32];
          char *s;
          if (!message)
            message = "code should not be reached";
          a_snprintf (lstr, 32, "%d", line);
          s = a_strconcat (domain ? domain : "", domain && domain[0] ? ":" : "",
                           "ERROR:", file, ":", lstr, ":",func, func[0] ? ":" : ""," ", message, NULL);
          a_printerr ("**\n%s\n", s);
          log (0, s, NULL, 0, NULL);
          /* store assertion message in global variable, so that it can be found in a
           * core dump */
          if (assertMsg != NULL)
            free (assertMsg);
          assertMsg = (char*) malloc (strlen (s) + 1);
          strcpy (assertMsg, s);
          a_free (s);
          if (inSubprocess){
             a_log_print_stack();
            _exit (1);
          }else{
            a_log_print_stack();
            a_abort ();
          }
	}

	static void message(const char *file,int line,const char *func,const char *expr, const AError *error,auint domain,int  code){
	  AString *str = new$ AString("assertion failed ");
	  if (domain)
	      str->appendPrintf("(%s == (%d, %d)): ", expr,domain,code);
	  else
	      str->appendPrintf("(%s == NULL): ", expr);

	  if (error)
	      str->appendPrintf("%s (%d, %d)", error->message,error->domain, error->code);
	  else
	      str->appendPrintf("%s is NULL", expr);
	  message (file, line, func, str->getString());
	  str->unref();
	}

	static void message(const char *file,int line, const char *func,const char *expr,long double arg1,
	                            const char *cmp,long double arg2,char numtype){
	  char *s = NULL;

	  switch (numtype)
	    {
	    case 'i':
	        s = a_strdup_printf ("assertion failed (%s): (%" A_AINT64_MODIFIER "i %s %" A_AINT64_MODIFIER "i)", expr, (aint64) arg1, cmp, (aint64) arg2);
	        break;
	    case 'x':
	        s = a_strdup_printf ("assertion failed (%s): (0x%08" A_AINT64_MODIFIER "x %s 0x%08" A_AINT64_MODIFIER "x)", expr,
	            (auint64) arg1, cmp, (auint64) arg2);
	        break;
	    case 'f':
	        s = a_strdup_printf ("assertion failed (%s): (%.9g %s %.9g)", expr, (double) arg1, cmp, (double) arg2);
	        break;
	      /* ideally use: floats=%.7g double=%.17g */
	    }
	  message (file, line, func, s);
	  a_free (s);
	}

};
