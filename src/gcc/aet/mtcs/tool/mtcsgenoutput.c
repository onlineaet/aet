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
 * AET was originally developed  by the onlineaet@163.com
 * base on genoutput.cc
 */

/* This program reads the machine description for the compiler target machine
   and produces a file containing these things:

   1. An array of `struct insn_data_d', which is indexed by insn code number,
   which contains:

     a. `name' is the name for that pattern.  Nameless patterns are
     given a name.

     b. `output' hold either the output template, an array of output
     templates, or an output function.

     c. `genfun' is the function to generate a body for that pattern,
     given operands as arguments.

     d. `n_operands' is the number of distinct operands in the pattern
     for that insn,

     e. `n_dups' is the number of match_dup's that appear in the insn's
     pattern.  This says how many elements of `recog_data.dup_loc' are
     significant after an insn has been recognized.

     f. `n_alternatives' is the number of alternatives in the constraints
     of each pattern.

     g. `output_format' tells what type of thing `output' is.

     h. `operand' is the base of an array of operand data for the insn.

   2. An array of `struct insn_operand data', used by `operand' above.

     a. `predicate', an int-valued function, is the match_operand predicate
     for this operand.

     b. `constraint' is the constraint for this operand.

     c. `address_p' indicates that the operand appears within ADDRESS
     rtx's.

     d. `mode' is the machine mode that that operand is supposed to have.

     e. `strict_low', is nonzero for operands contained in a STRICT_LOW_PART.

     f. `eliminable', is nonzero for operands that are matched normally by
     MATCH_OPERAND; it is zero for operands that should not be changed during
     register elimination such as MATCH_OPERATORs.

     g. `allows_mem', is true for operands that accept MEM rtxes.

  The code number of an insn is simply its position in the machine
  description; code numbers are assigned sequentially to entries in
  the description, starting with code number 0.

  Thus, the following entry in the machine description

    (define_insn "clrdf"
      [(set (match_operand:DF 0 "general_operand" "")
	    (const_int 0))]
      ""
      "clrd %0")

  assuming it is the 25th entry present, would cause
  insn_data[24].template to be "clrd %0", and
  insn_data[24].n_operands to be 1.  */


#include "bconfig.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "errors.h"
#include "read-md.h"
#include "gensupport.h"
#include "../../nlib.h"
#include "mtcsgen.h"

/* No instruction can have more operands than this.  Sorry for this
   arbitrary limit, but what machine will have an instruction with
   this many operands?  */

#define MAX_MAX_OPERANDS 40

static char general_mem[] = { TARGET_MEM_CONSTRAINT, 0 };

static int n_occurrences		(int, const char *);
static const char *strip_whitespace	(const char *);


/* This counts all operands used in the md file.  The first is null.  */
static int next_operand_number = 1;
/* Record in this chain all information about the operands we will output.  */

struct operand_data
{
  struct operand_data *next;
  int index;
  const char *predicate;
  const char *constraint;
  machine_mode mode;
  unsigned char n_alternatives;
  char address_p;
  char strict_low;
  char eliminable;
  char seen;
};

/* Begin with a null operand at index 0.  */

static struct operand_data null_operand =
{
  0, 0, "", "", E_VOIDmode, 0, 0, 0, 0, 0
};

static struct operand_data *odata = &null_operand;
static struct operand_data **odata_end = &null_operand.next;

/* Must match the constants in recog.h.  */

#define INSN_OUTPUT_FORMAT_NONE         0       /* abort */
#define INSN_OUTPUT_FORMAT_SINGLE       1       /* const char * */
#define INSN_OUTPUT_FORMAT_MULTI        2       /* const char * const * */
#define INSN_OUTPUT_FORMAT_FUNCTION     3       /* const char * (*)(...) */

/* Record in this chain all information that we will output,
   associated with the code number of the insn.  */

class data
{
public:
  class data *next;
  const char *name;
  const char *template_code;
  file_location loc;
  int code_number;
  int n_generator_args;		/* Number of arguments passed to generator */
  int n_operands;		/* Number of operands this insn recognizes */
  int n_dups;			/* Number times match_dup appears in pattern */
  int n_alternatives;		/* Number of alternatives in each constraint */
  int operand_number;		/* Operand index in the big array.  */
  int output_format;		/* INSN_OUTPUT_FORMAT_*.  */
  bool compact_syntax_p;
  struct operand_data operand[MAX_MAX_OPERANDS];
};

/* This variable points to the first link in the insn chain.  */
static class data *idata;

/* This variable points to the end of the insn chain.  This is where
   everything relevant from the machien description is appended to.  */
static class data **idata_end;


static void output_prologue (NString *outFile);
static void output_operand_data (NString *outFile);
static void output_insn_data (NString *outFile);
static void output_get_insn_name (void);
static void scan_operands (class data *, rtx, int, int);
static int compare_operands (struct operand_data *,
			     struct operand_data *);
static void place_operands (class data *);
static void process_template (NString *outFile,class data *, const char *);
static void validate_insn_alternatives (class data *);
static void validate_insn_operands (class data *);

class constraint_data
{
public:
  class constraint_data *next_this_letter;
  file_location loc;
  unsigned int namelen;
  char name[1];
};

/* All machine-independent constraint characters (except digits) that
   are handled outside the define*_constraint mechanism.  */
static const char indep_constraints[] = ",=+%*?!^$#&g";

static class constraint_data *
constraints_by_letter_table[1 << CHAR_BIT];

static int mdep_constraint_len (const char *, file_location, int);
static void note_constraint (md_rtx_info *);

static void output_prologue (NString *outFile)
{
  n_string_append(outFile,"/* Generated automatically by the program `mtcsgenoutput'\n\
   from the machine description file `md'.  */\n\n");

  n_string_append(outFile,"#define IN_TARGET_CODE 1\n");
  n_string_append(outFile,"#include \"config.h\"\n");
  n_string_append(outFile,"#include \"system.h\"\n");
  n_string_append(outFile,"#include \"coretypes.h\"\n");
  n_string_append(outFile,"#include \"backend.h\"\n");
  n_string_append(outFile,"#include \"predict.h\"\n");
  n_string_append(outFile,"#include \"tree.h\"\n");
  n_string_append(outFile,"#include \"rtl.h\"\n");
  n_string_append(outFile,"#include \"flags.h\"\n");
  n_string_append(outFile,"#include \"alias.h\"\n");
  n_string_append(outFile,"#include \"varasm.h\"\n");
  n_string_append(outFile,"#include \"stor-layout.h\"\n");
  n_string_append(outFile,"#include \"calls.h\"\n");
  n_string_append(outFile,"#include \"insn-config.h\"\n");
  n_string_append(outFile,"#include \"expmed.h\"\n");
  n_string_append(outFile,"#include \"dojump.h\"\n");
  n_string_append(outFile,"#include \"explow.h\"\n");
  n_string_append(outFile,"#include \"memmodel.h\"\n");
  n_string_append(outFile,"#include \"emit-rtl.h\"\n");
  n_string_append(outFile,"#include \"stmt.h\"\n");
  n_string_append(outFile,"#include \"expr.h\"\n");
  n_string_append(outFile,"#include \"insn-codes.h\"\n");
  n_string_append(outFile,"#include \"tm_p.h\"\n");
  n_string_append(outFile,"#include \"regs.h\"\n");
  n_string_append(outFile,"#include \"conditions.h\"\n");
  n_string_append(outFile,"#include \"insn-attr.h\"\n");
  n_string_append(outFile,"#include \"recog.h\"\n");
  n_string_append(outFile,"#include \"diagnostic-core.h\"\n");
  n_string_append(outFile,"#include \"output.h\"\n");
  n_string_append(outFile,"#include \"target.h\"\n");
  n_string_append(outFile,"\n");
  n_string_append(outFile,"#include \"../../mtcsmicro.h\"\n");
  if(mtcs_gen_is_ptx(mtcs_gen_get())){
      n_string_append(outFile,"#include \"ptx-insn-modes.h\"\n");
      n_string_append(outFile,"#include \"ptx-insn-flags.h\"\n");
      n_string_append(outFile,"#include \"ptx-insn-preds.h\"\n");
      n_string_append(outFile,"#include \"ptx-optionsitem.h\"\n");
      n_string_append(outFile,"#include \"../mtcsptxoutput.h\"\n");
      n_string_append(outFile,"#include \"../ptx-common.h\"\n");
      n_string_append(outFile,"#include \"../mtcsptxpreds.h\"\n");
      n_string_append(outFile,"#include \"../mtcsptxemit.h\"\n");
      n_string_append(outFile,"#include \"../mtcsptxmode.h\"\n");
      n_string_append(outFile,"#include \"../mtcsptxbuiltins.h\"\n");
  }
  n_string_append(outFile,"#include \"../../mtcstarget.h\"\n");
  n_string_append(outFile,"#include \"../../mtcspreds.h\"\n");
  n_string_append(outFile,"#include \"../../mtcsoutput.h\"\n");
  n_string_append(outFile,"#include \"../../mtcsoptions.h\"\n");
  n_string_append(outFile,"#include \"../../mtcsoptabs.h\"\n");
}


static const char *get_mode_name( struct operand_data *d)
{
    return mtcs_gen_get_mode_name(mtcs_gen_get(),d->mode);
}

/**
 * operand_data变成 ptx_operand_data;
 */
static void output_operand_data (NString *outFile)
{
  struct operand_data *d;
  n_string_append(outFile,"\nstatic const struct insn_operand_data operand_data[] = \n{\n");
  const char *platNameUpper=mtcs_gen_get_platform_upper_name(mtcs_gen_get());
  for (d = odata; d; d = d->next){
      struct pred_data *pred;
      const char *name=get_mode_name(d);
      //fprintf(stderr,"output_operand_data xxxx------%s\n",name);
      n_string_append(outFile,"  {\n");

      n_string_append_printf(outFile,"    %s,\n",
	      d->predicate && d->predicate[0] ? d->predicate : "0");

      n_string_append_printf(outFile,"    \"%s\",\n", d->constraint ? d->constraint : "");

      n_string_append_printf(outFile,"    (machine_mode)%s_%smode,\n", platNameUpper,name/*GET_MODE_NAME (d->mode)*/);//zclei %s_%smode 原来是 E_%smode

      n_string_append_printf(outFile,"    %d,\n", d->strict_low);

      n_string_append_printf(outFile,"    %d,\n", d->constraint == NULL ? 1 : 0);

      n_string_append_printf(outFile,"    %d,\n", d->eliminable);

      pred = NULL;
      if (d->predicate)
          pred = lookup_predicate (d->predicate);
      n_string_append_printf(outFile,"    %d\n", pred && pred->codes[MEM]);

      n_string_append(outFile,"  },\n");
  }
  n_string_append(outFile,"};\n\n\n");
}

static void output_insn_data (NString *outFile)
{
  class data *d;
  int name_offset = 0;
  int next_name_offset;
  const char * last_name = 0;
  const char * next_name = 0;
  class data *n;
  const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei

  for (n = idata, next_name_offset = 1; n; n = n->next, next_name_offset++)
    if (n->name){
        next_name = n->name;
        break;
    }

  n_string_append(outFile,"#if GCC_VERSION >= 2007\n__extension__\n#endif\n");
  n_string_append_printf(outFile,"\nconst struct insn_data_d %s_insn_data[] = \n{\n",platName);//zclei

  for (d = idata; d; d = d->next){
     n_string_append_printf(outFile,"  /* %s:%d */\n", d->loc.filename, d->loc.lineno);
     n_string_append(outFile,"  {\n");

      if (d->name){
         n_string_append_printf(outFile,"    \"%s\",\n", d->name);
	  name_offset = 0;
	  last_name = d->name;
	  next_name = 0;
	  for (n = d->next, next_name_offset = 1; n;
	       n = n->next, next_name_offset++)
	    {
	      if (n->name)
		{
		  next_name = n->name;
		  break;
		}
	    }
	}
      else
	{
	  name_offset++;
	  if (next_name && (last_name == 0
			    || name_offset > next_name_offset / 2))
	     n_string_append_printf(outFile,"    \"%s-%d\",\n", next_name,
		    next_name_offset - name_offset);
	  else
	     n_string_append_printf(outFile,"    \"%s+%d\",\n", last_name, name_offset);
	}

      switch (d->output_format)
	{
	case INSN_OUTPUT_FORMAT_NONE:
	   n_string_append(outFile,"#if HAVE_DESIGNATED_UNION_INITIALIZERS\n");
	   n_string_append(outFile,"    { 0 },\n");
	   n_string_append(outFile,"#else\n");
	   n_string_append(outFile,"    { 0, 0, 0 },\n");
	   n_string_append(outFile,"#endif\n");
	  break;
	case INSN_OUTPUT_FORMAT_SINGLE:
	  {
	    const char *p = d->template_code;
	    char prev = 0;

	    n_string_append(outFile,"#if HAVE_DESIGNATED_UNION_INITIALIZERS\n");
	     n_string_append(outFile,"    { .single =\n");
	     n_string_append(outFile,"#else\n");
	     n_string_append(outFile,"    {\n");
	     n_string_append(outFile,"#endif\n");
	     n_string_append(outFile,"    \"");
	    while (*p)
	      {
		if (IS_VSPACE (*p) && prev != '\\')
		  {
		    /* Preserve two consecutive \n's or \r's, but treat \r\n
		       as a single newline.  */
		    if (*p == '\n' && prev != '\r')
		       n_string_append(outFile,"\\n\\\n");
		  }
		else
		   n_string_append_c(outFile,*p);
		prev = *p;
		++p;
	      }
	     n_string_append(outFile,"\",\n");
	     n_string_append(outFile,"#if HAVE_DESIGNATED_UNION_INITIALIZERS\n");
	     n_string_append(outFile,"    },\n");
	     n_string_append(outFile,"#else\n");
	     n_string_append(outFile,"    0, 0 },\n");
	     n_string_append(outFile,"#endif\n");
	  }
	  break;
	case INSN_OUTPUT_FORMAT_MULTI:
	   n_string_append(outFile,"#if HAVE_DESIGNATED_UNION_INITIALIZERS\n");
	   n_string_append_printf(outFile,"    { .multi = output_%d },\n", d->code_number);
	   n_string_append(outFile,"#else\n");
	  n_string_append_printf(outFile,"    { 0, output_%d, 0 },\n", d->code_number);
	   n_string_append(outFile,"#endif\n");
	  break;
	case INSN_OUTPUT_FORMAT_FUNCTION:
	   n_string_append(outFile,"#if HAVE_DESIGNATED_UNION_INITIALIZERS\n");
	  n_string_append_printf(outFile,"    { .function = output_%d },\n", d->code_number);
	   n_string_append(outFile,"#else\n");
	  n_string_append_printf(outFile,"    { 0, 0, output_%d },\n", d->code_number);
	   n_string_append(outFile,"#endif\n");
	  break;
	default:
	  gcc_unreachable ();
	}

      if (d->name && d->name[0] != '*')
          n_string_append_printf(outFile,"    { (insn_gen_fn::stored_funcptr) %s_gen_%s },\n", platName,d->name);//zclei 加入ptx_
      else
           n_string_append(outFile,"    { 0 },\n");

      n_string_append_printf(outFile,"    &operand_data[%d],\n", d->operand_number);
      n_string_append_printf(outFile,"    %d,\n", d->n_generator_args);
      n_string_append_printf(outFile,"    %d,\n", d->n_operands);
      n_string_append_printf(outFile,"    %d,\n", d->n_dups);
      n_string_append_printf(outFile,"    %d,\n", d->n_alternatives);
      n_string_append_printf(outFile,"    %d\n", d->output_format);

      n_string_append(outFile,"  },\n");
  }
  n_string_append(outFile,"};\n\n\n");
}

static void output_get_insn_name (NString *outFile)
{
   const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei

  n_string_append(outFile,"const char *\n");
  n_string_append_printf(outFile,"%s_get_insn_name (int code)\n",platName);
  n_string_append(outFile,"{\n");
  n_string_append(outFile,"  if (code == NOOP_MOVE_INSN_CODE)\n");
  n_string_append(outFile,"    return \"NOOP_MOVE\";\n");
  n_string_append(outFile,"  else\n");
  n_string_append_printf(outFile,"    return %s_insn_data[code].name;\n",platName);
  n_string_append(outFile,"}\n");
}


/* Stores the operand data into `d->operand[i]'.

   THIS_ADDRESS_P is nonzero if the containing rtx was an ADDRESS.
   THIS_STRICT_LOW is nonzero if the containing rtx was a STRICT_LOW_PART.  */

static void scan_operands (class data *d, rtx part, int this_address_p,int this_strict_low)
{
  int i, j;
  const char *format_ptr;
  int opno;

  if (part == 0)
    return;

  switch (GET_CODE (part)){
    case MATCH_OPERAND:
      opno = XINT (part, 0);
      if (opno >= MAX_MAX_OPERANDS){
          error_at (d->loc, "maximum number of operands exceeded");
          return;
	  }
      if (d->operand[opno].seen)
          error_at (d->loc, "repeated operand number %d\n", opno);

      d->operand[opno].seen = 1;
     // fprintf(stderr,"名字是----- %s\n",GET_MODE_NAME(GET_MODE (part)));
      d->operand[opno].mode = GET_MODE (part);
      //d->operand[opno].mode = mtcs_gen_hostmode_to_devicemode(mtcs_gen_get(),GET_MODE (part));//GET_MODE (part);
      PUT_MODE_RAW(part,d->operand[opno].mode);
      d->operand[opno].strict_low = this_strict_low;
      d->operand[opno].predicate = XSTR (part, 1);
      d->operand[opno].constraint = strip_whitespace (XSTR (part, 2));
      d->operand[opno].n_alternatives= n_occurrences (',', d->operand[opno].constraint) + 1;
      d->operand[opno].address_p = this_address_p;
      d->operand[opno].eliminable = 1;
      return;

    case MATCH_SCRATCH:
      opno = XINT (part, 0);
      if (opno >= MAX_MAX_OPERANDS){
          error_at (d->loc, "maximum number of operands exceeded");
          return;
	  }
      if (d->operand[opno].seen)
          error_at (d->loc, "repeated operand number %d\n", opno);

      d->operand[opno].seen = 1;
      d->operand[opno].mode = GET_MODE (part);
     // d->operand[opno].mode = mtcs_gen_hostmode_to_devicemode(mtcs_gen_get(),GET_MODE (part));//GET_MODE (part);
      PUT_MODE_RAW(part,d->operand[opno].mode);
      d->operand[opno].strict_low = 0;
      d->operand[opno].predicate = "scratch_operand";
      d->operand[opno].constraint = strip_whitespace (XSTR (part, 1));
      d->operand[opno].n_alternatives= n_occurrences (',', d->operand[opno].constraint) + 1;
      d->operand[opno].address_p = 0;
      d->operand[opno].eliminable = 0;
      return;

    case MATCH_OPERATOR:
    case MATCH_PARALLEL:
      opno = XINT (part, 0);
      if (opno >= MAX_MAX_OPERANDS){
          error_at (d->loc, "maximum number of operands exceeded");
          return;
	  }
      if (d->operand[opno].seen)
          error_at (d->loc, "repeated operand number %d\n", opno);

      d->operand[opno].seen = 1;
      d->operand[opno].mode = GET_MODE (part);
     // d->operand[opno].mode = mtcs_gen_hostmode_to_devicemode(mtcs_gen_get(),GET_MODE (part));//GET_MODE (part);
      PUT_MODE_RAW(part,d->operand[opno].mode);
      d->operand[opno].strict_low = 0;
      d->operand[opno].predicate = XSTR (part, 1);
      d->operand[opno].constraint = 0;
      d->operand[opno].address_p = 0;
      d->operand[opno].eliminable = 0;
      for (i = 0; i < XVECLEN (part, 2); i++)
          scan_operands (d, XVECEXP (part, 2, i), 0, 0);
      return;

    case STRICT_LOW_PART:
      scan_operands (d, XEXP (part, 0), 0, 1);
      return;

    default:
      break;
  }

  format_ptr = GET_RTX_FORMAT (GET_CODE (part));

  for (i = 0; i < GET_RTX_LENGTH (GET_CODE (part)); i++)
    switch (*format_ptr++){
      case 'e':
      case 'u':
        scan_operands (d, XEXP (part, i), 0, 0);
        break;
      case 'E':
        if (XVEC (part, i) != NULL)
          for (j = 0; j < XVECLEN (part, i); j++)
            scan_operands (d, XVECEXP (part, i, j), 0, 0);
        break;
    }
}

/* Compare two operands for content equality.  */

static int compare_operands (struct operand_data *d0, struct operand_data *d1)
{
  const char *p0, *p1;

  p0 = d0->predicate;
  if (!p0)
    p0 = "";
  p1 = d1->predicate;
  if (!p1)
    p1 = "";
  if (strcmp (p0, p1) != 0)
    return 0;

  p0 = d0->constraint;
  if (!p0)
    p0 = "";
  p1 = d1->constraint;
  if (!p1)
    p1 = "";
  if (strcmp (p0, p1) != 0)
    return 0;

  if (d0->mode != d1->mode)
    return 0;

  if (d0->strict_low != d1->strict_low)
    return 0;

  if (d0->eliminable != d1->eliminable)
    return 0;

  return 1;
}

/* Scan the list of operands we've already committed to output and either
   find a subsequence that is the same, or allocate a new one at the end.  */

static void place_operands (class data *d)
{
  struct operand_data *od, *od2;
  int i;
  if (d->n_operands == 0){
      d->operand_number = 0;
      return;
  }

  /* Brute force substring search.  */
  for (od = odata, i = 0; od; od = od->next, i = 0)
     if (compare_operands (od, &d->operand[0])){
         od2 = od->next;
         i = 1;
         while (1){
            if (i == d->n_operands)
              goto full_match;
            if (od2 == NULL)
              goto partial_match;
            if (! compare_operands (od2, &d->operand[i]))
              break;
            ++i, od2 = od2->next;
         }
      }

  /* Either partial match at the end of the list, or no match.  In either
     case, we tack on what operands are remaining to the end of the list.  */
 partial_match:
  d->operand_number = next_operand_number - i;
  for (; i < d->n_operands; ++i){
      od2 = &d->operand[i];
      *odata_end = od2;
      odata_end = &od2->next;
      od2->index = next_operand_number++;
  }
  *odata_end = NULL;
  return;

 full_match:
  d->operand_number = od->index;
  return;
}

/* Process an assembler template from a define_insn or a define_peephole.
   It is either the assembler code template, a list of assembler code
   templates, or C code to generate the assembler code template.  */

static void
process_template (NString *outFile,class data *d, const char *template_code)
{
  const char *cp;
  int i;

  /* Templates starting with * contain straight code to be run.  */
  if (template_code[0] == '*'){
      d->template_code = 0;
      d->output_format = INSN_OUTPUT_FORMAT_FUNCTION;

      n_string_append(outFile,"\nstatic const char *\n");
      n_string_append_printf(outFile,"output_%d (rtx *operands ATTRIBUTE_UNUSED, rtx_insn *insn ATTRIBUTE_UNUSED)\n",
	      d->code_number);
      n_string_append(outFile,"{\n");
      //rtx_reader_ptr->print_md_ptr_loc (template_code);
      const struct md_reader::ptr_loc *loc = rtx_reader_ptr->get_md_ptr_loc (template_code);
      if (loc != 0)
         n_string_append_printf(outFile, "#line %d \"%s\"\n", loc->loc.lineno, loc->loc.filename);
      n_string_append_printf(outFile,"%s\n",template_code + 1);
      n_string_append(outFile,"}\n");
  }

  /* If the assembler code template starts with a @ it is a newline-separated
     list of assembler code templates, one for each alternative.  */
  else if (template_code[0] == '@'){
      int found_star = 0;
      for (cp = &template_code[1]; *cp; ){
          while (ISSPACE (*cp))
            cp++;
          if (*cp == '*')
            found_star = 1;
          while (!IS_VSPACE (*cp) && *cp != '\0')
            ++cp;
	  }
      d->template_code = 0;
      if (found_star){
          d->output_format = INSN_OUTPUT_FORMAT_FUNCTION;
          n_string_append(outFile,"\nstatic const char *\n");
          n_string_append_printf(outFile,"output_%d (rtx *operands ATTRIBUTE_UNUSED, "
              "rtx_insn *insn ATTRIBUTE_UNUSED)\n", d->code_number);
          n_string_append(outFile,"{\n");
          n_string_append(outFile,"  switch (which_alternative)\n    {\n");
	  }else{
          d->output_format = INSN_OUTPUT_FORMAT_MULTI;
          n_string_append_printf(outFile,"\nstatic const char * const output_%d[] = {\n",d->code_number);
	  }

      for (i = 0, cp = &template_code[1]; *cp; ){
	  const char *ep, *sp, *bp;

	  while (ISSPACE (*cp))
	    cp++;

	  bp = cp;
	  if (found_star)
	    {
	     n_string_append_printf(outFile,"    case %d:", i);
	      if (*cp == '*')
		{
	         n_string_append(outFile,"\n      ");
		  cp++;
		}
	      else
	         n_string_append(outFile," return \"");
	    }
	  else
	     n_string_append(outFile,"  \"");

	  for (ep = sp = cp; !IS_VSPACE (*ep) && *ep != '\0'; ++ep)
	    if (!ISSPACE (*ep))
	      sp = ep + 1;

	  if (sp != ep)
	    message_at (d->loc, "trailing whitespace in output template");

	  /* Check for any unexpanded iterators.  */
	  if (bp[0] != '*' && d->compact_syntax_p)
	    {
	      const char *p = cp;
	      const char *last_bracket = nullptr;
	      while (p < sp)
		{
		  if (*p == '\\' && p + 1 < sp)
		    {
		      n_string_append_c(outFile,*p);
		      n_string_append_c(outFile,*(p+1));
		      p += 2;
		      continue;
		    }

		  if (*p == '>' && last_bracket && *last_bracket == '<')
		    {
		      int len = p - last_bracket;
		      fatal_at (d->loc, "unresolved iterator '%.*s' in '%s'",
				len - 1, last_bracket + 1, cp);
		    }
		  else if (*p == '<' || *p == '>')
		    last_bracket = p;

		  n_string_append_c(outFile,*p);
		  p += 1;
		}

	      if (last_bracket)
		{
		  char *nl = strchr (const_cast<char*> (cp), '\n');
		  if (nl)
		    *nl = '\0';
		  fatal_at (d->loc, "unmatched angle brackets, likely an "
			    "error in iterator syntax in %s", cp);
		}
	    }
	  else
	    {
	      while (cp < sp)
	         n_string_append_c(outFile,*(cp++));
	    }

	  cp = sp;

	  if (!found_star)
	     n_string_append(outFile,"\",\n");
	  else if (*bp != '*')
	     n_string_append(outFile,"\";\n");
	  else
	    {
	      /* The usual action will end with a return.
		 If there is neither break or return at the end, this is
		 assumed to be intentional; this allows to have multiple
		 consecutive alternatives share some code.  */
	     n_string_append(outFile,"\n");
	    }
	  i++;
	}
      if (i == 1)
	message_at (d->loc, "'@' is redundant for output template with"
		    " single alternative");
      if (i != d->n_alternatives)
	error_at (d->loc, "wrong number of alternatives in the output"
		  " template");

      if (found_star)
         n_string_append(outFile,"      default: gcc_unreachable ();\n    }\n}\n");
      else
         n_string_append(outFile,"};\n");
    }
  else
    {
      d->template_code = template_code;
      d->output_format = INSN_OUTPUT_FORMAT_SINGLE;
    }
}

/* Check insn D for consistency in number of constraint alternatives.  */

static void validate_insn_alternatives (class data *d)
{
  int n = 0, start;

  /* Make sure all the operands have the same number of alternatives
     in their constraints.  Let N be that number.  */
  for (start = 0; start < d->n_operands; start++)
    if (d->operand[start].n_alternatives > 0){
        int len, i;
        const char *p;
        char c;
        int which_alternative = 0;
        int alternative_count_unsure = 0;
        bool seen_write = false;
        bool alt_mismatch = false;

        for (p = d->operand[start].constraint; (c = *p); p += len){
            if ((c == '%' || c == '=' || c == '+') && p != d->operand[start].constraint)
              error_at (d->loc, "character '%c' can only be used at the beginning of a constraint string", c);

            if (c == '=' || c == '+')
              seen_write = true;

            /* Earlyclobber operands must always be marked write-only
               or read/write.  */
            if (!seen_write && c == '&')
              error_at (d->loc, "earlyclobber operands may not be read-only in alternative %d", which_alternative);

            if (ISSPACE (c) || strchr (indep_constraints, c))
              len = 1;
            else if (ISDIGIT (c)){
                const char *q = p;
                do
                  q++;
                while (ISDIGIT (*q));
                len = q - p;
            }else{
              len = mdep_constraint_len (p, d->loc, start);
            }

            if (c == ','){
                which_alternative++;
                continue;
            }

            for (i = 1; i < len; i++)
              if (p[i] == '\0'){
                  error_at (d->loc, "NUL in alternative %d of operand %d",which_alternative, start);
                  alternative_count_unsure = 1;
                  break;
              }else if (strchr (",#*", p[i])){
                  error_at (d->loc, "'%c' in alternative %d of operand %d",p[i], which_alternative, start);
                  alternative_count_unsure = 1;
              }
        }

        if (!alternative_count_unsure){
            if (n == 0)
              n = d->operand[start].n_alternatives;
            else if (n != d->operand[start].n_alternatives){
            if (!alt_mismatch){
                alt_mismatch = true;
                error_at (d->loc,"alternative number mismatch: operand %d has %d, operand %d has %d",
                      0, n, start, d->operand[start].n_alternatives);
            }else
              error_at (d->loc, "operand %d has %d alternatives",start, d->operand[start].n_alternatives);
            }
        }
     }

  /* Record the insn's overall number of alternatives.  */
  d->n_alternatives = n;
}

/* Verify that there are no gaps in operand numbers for INSNs.  */

static void
validate_insn_operands (class data *d)
{
  int i;

  for (i = 0; i < d->n_operands; ++i)
    if (d->operand[i].seen == 0)
      error_at (d->loc, "missing operand %d", i);
}

static void
validate_optab_operands (class data *d)
{
  if (!d->name || d->name[0] == '\0' || d->name[0] == '*')
    return;

  /* Miscellaneous tests.  */
  if (startswith (d->name, "cstore")
      && d->name[strlen (d->name) - 1] == '4'
      && d->operand[0].mode == VOIDmode)
    {
      message_at (d->loc, "missing mode for operand 0 of cstore");
      have_error = 1;
    }
}

/* Look at a define_insn just read.  Assign its code number.  Record
   on idata the template and the number of arguments.  If the insn has
   a hairy output action, output a function for now.  */

static void
gen_insn (NString *outFile,md_rtx_info *info)
{
  struct pattern_stats stats;
  rtx insn = info->def;
  data *d = new data;
  int i;

  d->code_number = info->index;
  d->loc = info->loc;
  if (XSTR (insn, 0)[0])
    d->name = XSTR (insn, 0);
  else
    d->name = 0;

  d->compact_syntax_p = compact_syntax.contains (insn);
  /* Build up the list in the same order as the insns are seen
     in the machine description.  */
  d->next = 0;
  *idata_end = d;
  idata_end = &d->next;

  memset (d->operand, 0, sizeof (d->operand));
  //fprintf(stderr,"mtcsgenoutput.c gen_insn ---- len:%d %s\n",XVECLEN (insn, 1),XTMPL (insn, 3));
  for (i = 0; i < XVECLEN (insn, 1); i++)
    scan_operands (d, XVECEXP (insn, 1, i), 0, 0);
  get_pattern_stats (&stats, XVEC (insn, 1));
  d->n_generator_args = stats.num_generator_args;
  d->n_operands = stats.num_insn_operands;
  d->n_dups = stats.num_dups;
  validate_insn_operands (d);
  validate_insn_alternatives (d);
  validate_optab_operands (d);
  place_operands (d);
  process_template (outFile,d, XTMPL (insn, 3));
}

/* Look at a define_peephole just read.  Assign its code number.
   Record on idata the template and the number of arguments.
   If the insn has a hairy output action, output it now.  */

static void
gen_peephole (NString *outFile,md_rtx_info *info)
{
  struct pattern_stats stats;
  data *d = new data;
  int i;

  d->code_number = info->index;
  d->loc = info->loc;
  d->name = 0;

  /* Build up the list in the same order as the insns are seen
     in the machine description.  */
  d->next = 0;
  *idata_end = d;
  idata_end = &d->next;

  memset (d->operand, 0, sizeof (d->operand));

  /* Get the number of operands by scanning all the patterns of the
     peephole optimizer.  But ignore all the rest of the information
     thus obtained.  */
  rtx peep = info->def;
  for (i = 0; i < XVECLEN (peep, 0); i++)
    scan_operands (d, XVECEXP (peep, 0, i), 0, 0);

  get_pattern_stats (&stats, XVEC (peep, 0));
  d->n_generator_args = 0;
  d->n_operands = stats.num_insn_operands;
  d->n_dups = 0;

  validate_insn_alternatives (d);
  place_operands (d);
  process_template (outFile,d, XTMPL (peep, 2));
}

/* Process a define_expand just read.  Assign its code number,
   only for the purposes of `insn_gen_function'.  */

static void
gen_expand (md_rtx_info *info)
{
  struct pattern_stats stats;
  rtx insn = info->def;
  data *d = new data;
  int i;

  d->code_number = info->index;
  d->loc = info->loc;
  if (XSTR (insn, 0)[0])
    d->name = XSTR (insn, 0);
  else
    d->name = 0;

  /* Build up the list in the same order as the insns are seen
     in the machine description.  */
  d->next = 0;
  *idata_end = d;
  idata_end = &d->next;

  memset (d->operand, 0, sizeof (d->operand));

  /* Scan the operands to get the specified predicates and modes,
     since expand_binop needs to know them.  */

  if (XVEC (insn, 1))
    for (i = 0; i < XVECLEN (insn, 1); i++)
      scan_operands (d, XVECEXP (insn, 1, i), 0, 0);

  get_pattern_stats (&stats, XVEC (insn, 1));
  d->n_generator_args = stats.num_generator_args;
  d->n_operands = stats.num_insn_operands;
  d->n_dups = stats.num_dups;
  d->template_code = 0;
  d->output_format = INSN_OUTPUT_FORMAT_NONE;

  validate_insn_alternatives (d);
  validate_optab_operands (d);
  place_operands (d);
}

static void
init_insn_for_nothing (void)
{
  idata = XCNEW (class data);
  new (idata) data ();
  idata->name = "*placeholder_for_nothing";
  idata->loc = file_location ("<internal>", 0, 0);
  idata_end = &idata->next;
}

extern int main (int, const char **);


/* Return the number of occurrences of character C in string S or
   -1 if S is the null string.  */

static int
n_occurrences (int c, const char *s)
{
  int n = 0;

  if (s == 0 || *s == '\0')
    return -1;

  while (*s)
    n += (*s++ == c);

  return n;
}

/* Remove whitespace in `s' by moving up characters until the end.
   Return a new string.  */

static const char *
strip_whitespace (const char *s)
{
  char *p, *q;
  char ch;

  if (s == 0)
    return 0;

  p = q = XNEWVEC (char, strlen (s) + 1);
  while ((ch = *s++) != '\0')
    if (! ISSPACE (ch))
      *p++ = ch;

  *p = '\0';
  return q;
}

/* Record just enough information about the constraint in *INFO to allow
   checking of operand constraint strings above, in validate_insn_alternatives.
   Does not validate most properties of the constraint itself; does enforce
   no duplicate names, no overlap with MI constraints, and no prefixes.  */
static void note_constraint (md_rtx_info *info)
{
  rtx exp = info->def;
  const char *name = XSTR (exp, 0);
  class constraint_data **iter, **slot, *new_cdata;

  if (strcmp (name, "TARGET_MEM_CONSTRAINT") == 0)
    name = general_mem;
  unsigned int namelen = strlen (name);
  fprintf(stderr,"note_constraint ----00 name:%s\n",name);

  if (strchr (indep_constraints, name[0])){
      if (name[1] == '\0')
          error_at (info->loc, "constraint letter '%s' cannot be redefined by the machine description", name);
      else
          error_at (info->loc, "constraint name '%s' cannot be defined by the machine description, as it begins with '%c'",
		  name, name[0]);
      return;
  }

  slot = &constraints_by_letter_table[(unsigned int)name[0]];
  for (iter = slot; *iter; iter = &(*iter)->next_this_letter){
      /* This causes slot to end up pointing to the
	 next_this_letter field of the last constraint with a name
	 of equal or greater length than the new constraint; hence
	 the new constraint will be inserted after all previous
	 constraints with names of the same length.  */
      if ((*iter)->namelen >= namelen)
          slot = iter;
      fprintf(stderr,"note_constraint ----11 name:%s\n",name);

      if (!strcmp ((*iter)->name, name)){
          error_at (info->loc, "redefinition of constraint '%s'", name);
          message_at ((*iter)->loc, "previous definition is here");
          return;
	  } else if (!strncmp ((*iter)->name, name, (*iter)->namelen)){
          error_at (info->loc, "defining constraint '%s' here", name);
          message_at ((*iter)->loc, "renders constraint '%s' (defined here) a prefix", (*iter)->name);
          return;
	  } else if (!strncmp ((*iter)->name, name, namelen)){
          error_at (info->loc, "constraint '%s' is a prefix", name);
          message_at ((*iter)->loc, "of constraint '%s' (defined here)", (*iter)->name);
          return;
	  }
  }
  new_cdata = XNEWVAR (class constraint_data,sizeof (class constraint_data) + namelen);
  new (new_cdata) constraint_data ();
  strcpy (CONST_CAST (char *, new_cdata->name), name);
  new_cdata->namelen = namelen;
  new_cdata->loc = info->loc;
  new_cdata->next_this_letter = *slot;
  *slot = new_cdata;
}

/* Return the length of the constraint name beginning at position S
   of an operand constraint string, or issue an error message if there
   is no such constraint.  Does not expect to be called for generic
   constraints.  */
static int mdep_constraint_len (const char *s, file_location loc, int opno)
{
  class constraint_data *p;

  p = constraints_by_letter_table[(unsigned int)s[0]];
  if (p){
    for (; p; p = p->next_this_letter){
      if (!strncmp (s, p->name, p->namelen)){
          return p->namelen;
      }
    }
  }

  error_at (loc, "error: undefined machine-specific constraint at this point: \"%s\"", s);
  message_at (loc, "note:  in operand %d", opno);
  return 1; /* safe */
}

static void replacePredsFunc(NString *src)
{
   const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei
   struct pred_data *p;
   char replace[512];
   char find[512];

   FOR_ALL_PREDICATES (p){
      if(!mtcs_gen_is_reserve_preds(mtcs_gen_get(),p->name)){
           sprintf(find," %s",p->name);
           sprintf(replace," %s_%s",platName,p->name);
           n_string_replace(src,find,replace,-1);
      }else{
           sprintf(find," %s",p->name);
           sprintf(replace," mtcs_preds_%s",p->name);
           n_string_replace(src,find,replace,-1);
      }
   }
}

/**
 * nvptx-protols.h 中声明的方法，用在 xxx-insn-emit.c中方法见 char *patterns[8][2]
 * 其中 nvptx_mem_local_p insn-emit.c insn-output.c都引用。
 * extern void nvptx_expand_oacc_fork (unsigned);
extern void nvptx_expand_oacc_join (unsigned);
extern void nvptx_expand_call (rtx, rtx);
extern rtx nvptx_gen_shuffle (rtx, rtx, rtx, nvptx_shuffle_kind);
extern rtx nvptx_expand_compare (rtx);
extern const char *nvptx_ptx_type_from_mode (machine_mode, bool);//ptx-insn-emit.c insn-ouput.c 没引用
extern const char *nvptx_output_mov_insn (rtx, rtx);//insn-output.cc中引用 其它没有
extern const char *nvptx_output_call_insn (rtx_insn *, rtx, rtx);
extern const char *nvptx_output_fake_ptx_alloca (void);
extern const char *nvptx_output_return (void);
extern const char *nvptx_output_set_softstack (unsigned);
extern const char *nvptx_output_simt_enter (rtx, rtx, rtx);
extern const char *nvptx_output_simt_exit (rtx);
extern const char *nvptx_output_red_partition (rtx, rtx);
extern const char *nvptx_output_atomic_insn (const char *, rtx *, int, int);
extern bool nvptx_mem_local_p (rtx); //output emit 两个引用
extern bool nvptx_mem_maybe_shared_p (const_rtx); //emit引用
extern void output_asm_insn (const char *, rtx *); //output.h final.cc 在md文件中引用，生成的output也引用。

 */
static void replace_protols(NString *src)
{
   if(mtcs_gen_is_ptx(mtcs_gen_get())){
      char *patterns[11][2]={
         {"nvptx_output_mov_insn (","mtcs_ptx_output_mov_insn/*!nvptx_output_mov_insn*/((MtcsPtxOutput*)mtcsTarget->mtcsOutput,"},
         {"nvptx_output_call_insn (","mtcs_ptx_output_call_insn/*!nvptx_output_call_insn*/((MtcsPtxOutput*)mtcsTarget->mtcsOutput,"},
         {"nvptx_output_fake_ptx_alloca (","mtcs_ptx_output_fake_ptx_alloca((MtcsPtxOutput*)mtcsTarget->mtcsOutput"},
         {"nvptx_output_return (","mtcs_ptx_output_return/*!nvptx_output_return*/((MtcsPtxOutput*)mtcsTarget->mtcsOutput"},
         {"nvptx_output_set_softstack (","mtcs_ptx_output_set_softstack/*!nvptx_output_set_softstack*/((MtcsPtxOutput*)mtcsTarget->mtcsOutput,"},
         {"nvptx_output_simt_enter (","mtcs_ptx_output_simt_enter/*!nvptx_output_simt_enter*/((MtcsPtxOutput*)mtcsTarget->mtcsOutput,"},
         {"nvptx_output_simt_exit (","mtcs_ptx_output_simt_exit/*!nvptx_output_simt_exit*/((MtcsPtxOutput*)mtcsTarget->mtcsOutput,"},
         {"nvptx_output_red_partition (","mtcs_ptx_output_red_partition/*!nvptx_output_red_partition*/((MtcsPtxOutput*)mtcsTarget->mtcsOutput,"},
         {"nvptx_output_atomic_insn (","mtcs_ptx_output_atomic_insn/*!nvptx_output_atomic_insn*/((MtcsPtxOutput*)mtcsTarget->mtcsOutput,"},
         {"nvptx_mem_local_p (","mtcs_ptx_output_mem_local_p/*!nvptx_mem_local_p*/((MtcsPtxOutput*)mtcsTarget->mtcsOutput,"},
         {"output_asm_insn (","mtcs_output_asm_insn/*!output_asm_insn*/(mtcsTarget->mtcsOutput,"},
      };
      int i;
      for(i=0;i<11;i++)
         n_string_replace(src,patterns[i][0],patterns[i][1],-1);
   }
}

static void replace_misc(NString *src)
{
   static char *fake="nvptx_fake_ptx_alloca";
   static char *replace="((PtxOptionsItem *)mtcsTarget->mtcsOptions->global_options)->x_nvptx_fake_ptx_alloca";
   if(mtcs_gen_is_ptx(mtcs_gen_get())){
      n_string_replace(src,fake,replace,-1);
   }
}

int main (int argc, const char **argv)
{
  progname = "mtcsgenoutput";
  init_insn_for_nothing ();
  if (!init_rtx_reader_args_cb(argc, argv,mtcs_gen_handle_arg))
    return (FATAL_EXIT_CODE);
  NString *outStr=n_string_sized_new(1024*1024);

  output_prologue (outStr);
  n_string_append(outStr,mtcs_gen_create_target_code(mtcs_gen_get(),"output"));//zclei
  mtcs_gen_append_preds(mtcs_gen_get());
  fprintf(stderr,"mtcsgenotput -xxx--file:%s %s\n",argv[1],argv[2]);
  /* Read the machine description.  */
   int count=0;
  md_rtx_info info;
  while (read_md_rtx (&info))
    switch (GET_CODE (info.def)){
      case DEFINE_INSN:
       //   fprintf(stderr,"mtcsgenotput ---处理DEFINE_PEEPHOLE :\n",count++);
        gen_insn (outStr,&info);
        break;
      case DEFINE_PEEPHOLE:
        //fprintf(stderr,"mtcsgenotput ---处理DEFINE_PEEPHOLE :\n",count++);
        gen_peephole (outStr,&info);
        break;
      case DEFINE_EXPAND:
         // fprintf(stderr,"mtcsgenotput ---处理DEFINE_EXPAND :\n",count++);
        gen_expand (&info);
        break;
      case DEFINE_CONSTRAINT:
      case DEFINE_REGISTER_CONSTRAINT:
      case DEFINE_ADDRESS_CONSTRAINT:
      case DEFINE_MEMORY_CONSTRAINT:
      case DEFINE_SPECIAL_MEMORY_CONSTRAINT:
      case DEFINE_RELAXED_MEMORY_CONSTRAINT:
        //fprintf(stderr,"mtcsgenotput ---处理note_constraint :\n",count++ );
        note_constraint (&info);
        break;
      default:
        break;
  }

  n_string_append(outStr,"\n\n");
  output_operand_data (outStr);
  output_insn_data (outStr);
  //不需要生成获取名字的源代码 mtcsptxoutput.c实现
  output_get_insn_name (outStr);
  replacePredsFunc(outStr);
  replace_protols(outStr);
  replace_misc(outStr);
  fprintf(stdout,"%s",outStr->str);
  fflush (stdout);
  int err=ferror (stdout) != 0 || have_error  ? FATAL_EXIT_CODE : SUCCESS_EXIT_CODE;
  return (ferror (stdout) != 0 || have_error
    ? FATAL_EXIT_CODE : SUCCESS_EXIT_CODE);
}

