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
 * base on genpreds.cc
 */
#include "bconfig.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "errors.h"
#include "obstack.h"
#include "read-md.h"
#include "gensupport.h"
#include "mtcsgen.h"
#include "../../nlib.h"

static char general_mem[] = { TARGET_MEM_CONSTRAINT, 0 };

/* Given a predicate expression EXP, from form NAME at location LOC,
   verify that it does not contain any RTL constructs which are not
   valid in predicate definitions.  Returns true if EXP is
   INvalid; issues error messages, caller need not.  */
static bool
validate_exp (rtx exp, const char *name, file_location loc)
{
  if (exp == 0)
    {
      message_at (loc, "%s: must give a predicate expression", name);
      return true;
    }

  switch (GET_CODE (exp))
    {
      /* Ternary, binary, unary expressions: recurse into subexpressions.  */
    case IF_THEN_ELSE:
      if (validate_exp (XEXP (exp, 2), name, loc))
	return true;
      /* fall through */
    case AND:
    case IOR:
      if (validate_exp (XEXP (exp, 1), name, loc))
	return true;
      /* fall through */
    case NOT:
      return validate_exp (XEXP (exp, 0), name, loc);

      /* MATCH_CODE might have a syntax error in its path expression.  */
    case MATCH_CODE:
      {
	const char *p;
	for (p = XSTR (exp, 1); *p; p++)
	  {
	    if (!ISDIGIT (*p) && !ISLOWER (*p))
	      {
		error_at (loc, "%s: invalid character in path "
			  "string '%s'", name, XSTR (exp, 1));
		return true;
	      }
	  }
      }
      gcc_fallthrough ();

      /* These need no special checking.  */
    case MATCH_OPERAND:
    case MATCH_TEST:
      return false;

    default:
      error_at (loc, "%s: cannot use '%s' in a predicate expression",
		name, GET_RTX_NAME (GET_CODE (exp)));
      return true;
    }
}

/* Predicates are defined with (define_predicate) or
   (define_special_predicate) expressions in the machine description.  */
static void
process_define_predicate (md_rtx_info *info)
{
  validate_exp (XEXP (info->def, 1), XSTR (info->def, 0), info->loc);
}

/* Given a predicate, if it has an embedded C block, write the block
   out as a static inline subroutine, and augment the RTL test with a
   match_test that calls that subroutine.  For instance,

       (define_predicate "basereg_operand"
         (match_operand 0 "register_operand")
       {
         if (GET_CODE (op) == SUBREG)
           op = SUBREG_REG (op);
         return REG_POINTER (op);
       })

   becomes

       static inline bool basereg_operand_1(rtx op, machine_mode mode)
       {
         if (GET_CODE (op) == SUBREG)
           op = SUBREG_REG (op);
         return REG_POINTER (op);
       }

       (define_predicate "basereg_operand"
         (and (match_operand 0 "register_operand")
	      (match_test "basereg_operand_1 (op, mode)")))

   The only wart is that there's no way to insist on a { } string in
   an RTL template, so we have to handle "" strings.  */


static void
write_predicate_subfunction (NString *outFile,struct pred_data *p)
{
  const char *match_test_str;
  rtx match_test_exp, and_exp;

  if (p->c_block[0] == '\0')
    return;

  /* Construct the function-call expression.  */
  obstack_grow (rtl_obstack, p->name, strlen (p->name));
  obstack_grow (rtl_obstack, "_1 (op, mode)",
		sizeof "_1 (op, mode)");
  match_test_str = XOBFINISH (rtl_obstack, const char *);

  /* Add the function-call expression to the complete expression to be
     evaluated.  */
  match_test_exp = rtx_alloc (MATCH_TEST);
  XSTR (match_test_exp, 0) = match_test_str;

  and_exp = rtx_alloc (AND);
  XEXP (and_exp, 0) = p->exp;
  XEXP (and_exp, 1) = match_test_exp;

  p->exp = and_exp;

  n_string_append_printf(outFile,"static inline bool\n"
	  "%s_1 (rtx op ATTRIBUTE_UNUSED, machine_mode mode ATTRIBUTE_UNUSED)\n",
	  p->name);
 // rtx_reader_ptr->print_md_ptr_loc (p->c_block);
  const struct md_reader::ptr_loc *loc = rtx_reader_ptr->get_md_ptr_loc (p->c_block);
  if (loc != 0)
     n_string_append_printf(outFile, "#line %d \"%s\"\n", loc->loc.lineno, loc->loc.filename);
  if (p->c_block[0] == '{')
     n_string_append(outFile,p->c_block);
  else
     n_string_append_printf(outFile,"{\n  %s\n}", p->c_block);
  n_string_append(outFile,"\n\n");
}

/* Given a predicate expression EXP, from form NAME, determine whether
   it refers to the variable given as VAR.  */
static bool
needs_variable (rtx exp, const char *var)
{
  switch (GET_CODE (exp))
    {
      /* Ternary, binary, unary expressions need a variable if
	 any of their subexpressions do.  */
    case IF_THEN_ELSE:
      if (needs_variable (XEXP (exp, 2), var))
	return true;
      /* fall through */
    case AND:
    case IOR:
      if (needs_variable (XEXP (exp, 1), var))
	return true;
      /* fall through */
    case NOT:
      return needs_variable (XEXP (exp, 0), var);

      /* MATCH_CODE uses "op", but nothing else.  */
    case MATCH_CODE:
      return !strcmp (var, "op");

      /* MATCH_OPERAND uses "op" and may use "mode".  */
    case MATCH_OPERAND:
      if (!strcmp (var, "op"))
	return true;
      if (!strcmp (var, "mode") && GET_MODE (exp) == VOIDmode)
	return true;
      return false;

      /* MATCH_TEST uses var if XSTR (exp, 0) =~ /\b${var}\b/o; */
    case MATCH_TEST:
      {
	const char *p = XSTR (exp, 0);
	const char *q = strstr (p, var);
	if (!q)
	  return false;
	if (q != p && (ISALNUM (q[-1]) || q[-1] == '_'))
	  return false;
	q += strlen (var);
	if (ISALNUM (q[0]) || q[0] == '_')
	  return false;
      }
      return true;

    default:
      gcc_unreachable ();
    }
}

/* Given an RTL expression EXP, find all subexpressions which we may
   assume to perform mode tests.  Normal MATCH_OPERAND does;
   MATCH_CODE doesn't as such (although certain codes always have
   VOIDmode); and we have to assume that MATCH_TEST does not.
   These combine in almost-boolean fashion - the only exception is
   that (not X) must be assumed not to perform a mode test, whether
   or not X does.

   The mark is the RTL /v flag, which is true for subexpressions which
   do *not* perform mode tests.
*/
#define NO_MODE_TEST(EXP) RTX_FLAG (EXP, volatil)
static void
mark_mode_tests (rtx exp)
{
  switch (GET_CODE (exp))
    {
    case MATCH_OPERAND:
      {
	struct pred_data *p = lookup_predicate (XSTR (exp, 1));
	if (!p)
	  error ("reference to undefined predicate '%s'", XSTR (exp, 1));
	else if (p->special || GET_MODE (exp) != VOIDmode)
	  NO_MODE_TEST (exp) = 1;
      }
      break;

    case MATCH_CODE:
      NO_MODE_TEST (exp) = 1;
      break;

    case MATCH_TEST:
    case NOT:
      NO_MODE_TEST (exp) = 1;
      break;

    case AND:
      mark_mode_tests (XEXP (exp, 0));
      mark_mode_tests (XEXP (exp, 1));

      NO_MODE_TEST (exp) = (NO_MODE_TEST (XEXP (exp, 0))
			    && NO_MODE_TEST (XEXP (exp, 1)));
      break;

    case IOR:
      mark_mode_tests (XEXP (exp, 0));
      mark_mode_tests (XEXP (exp, 1));

      NO_MODE_TEST (exp) = (NO_MODE_TEST (XEXP (exp, 0))
			    || NO_MODE_TEST (XEXP (exp, 1)));
      break;

    case IF_THEN_ELSE:
      /* A ? B : C does a mode test if (one of A and B) does a mode
	 test, and C does too.  */
      mark_mode_tests (XEXP (exp, 0));
      mark_mode_tests (XEXP (exp, 1));
      mark_mode_tests (XEXP (exp, 2));

      NO_MODE_TEST (exp) = ((NO_MODE_TEST (XEXP (exp, 0))
			     && NO_MODE_TEST (XEXP (exp, 1)))
			    || NO_MODE_TEST (XEXP (exp, 2)));
      break;

    default:
      gcc_unreachable ();
    }
}

/* Determine whether the expression EXP is a MATCH_CODE that should
   be written as a switch statement.  */
static bool
generate_switch_p (rtx exp)
{
  return GET_CODE (exp) == MATCH_CODE
	 && strchr (XSTR (exp, 0), ',');
}

/* Given a predicate, work out where in its RTL expression to add
   tests for proper modes.  Special predicates do not get any such
   tests.  We try to avoid adding tests when we don't have to; in
   particular, other normal predicates can be counted on to do it for
   us.  */

static void
add_mode_tests (struct pred_data *p)
{
  rtx match_test_exp, and_exp;
  rtx *pos;

  /* Don't touch special predicates.  */
  if (p->special)
    return;

  /* Check whether the predicate accepts const scalar ints (which always
     have a stored mode of VOIDmode, but logically have a real mode)
     and whether it matches anything besides const scalar ints.  */
  bool matches_const_scalar_int_p = false;
  bool matches_other_p = false;
  for (int i = 0; i < NUM_RTX_CODE; ++i)
    if (p->codes[i])
      switch (i)
	{
	case CONST_INT:
	case CONST_WIDE_INT:
	  /* Special handling for (VOIDmode) LABEL_REFs.  */
	case LABEL_REF:
	  matches_const_scalar_int_p = true;
	  break;

	case CONST_DOUBLE:
	  if (!TARGET_SUPPORTS_WIDE_INT)
	    matches_const_scalar_int_p = true;
	  matches_other_p = true;
	  break;

	default:
	  matches_other_p = true;
	  break;
	}

  /* There's no need for a mode check if the predicate only accepts
     constant integers.  The code checks in the predicate are enough
     to establish that the mode is VOIDmode.

     Note that the predicate itself should check whether a scalar
     integer is in range of the given mode.  */
  if (!matches_other_p)
    return;

  mark_mode_tests (p->exp);

  /* If the whole expression already tests the mode, we're done.  */
  if (!NO_MODE_TEST (p->exp))
    return;

  match_test_exp = rtx_alloc (MATCH_TEST);
  if (matches_const_scalar_int_p)
    XSTR (match_test_exp, 0) = ("mode == VOIDmode || GET_MODE (op) == mode"
				" || GET_MODE (op) == VOIDmode");
  else
    XSTR (match_test_exp, 0) = "mode == VOIDmode || GET_MODE (op) == mode";
  and_exp = rtx_alloc (AND);
  XEXP (and_exp, 1) = match_test_exp;

  /* It is always correct to rewrite p->exp as

        (and (...) (match_test "mode == VOIDmode || GET_MODE (op) == mode"))

     but there are a couple forms where we can do better.  If the
     top-level pattern is an IOR, and one of the two branches does test
     the mode, we can wrap just the branch that doesn't.  Likewise, if
     we have an IF_THEN_ELSE, and one side of it tests the mode, we can
     wrap just the side that doesn't.  And, of course, we can repeat this
     descent as many times as it works.  */

  pos = &p->exp;
  for (;;)
    {
      rtx subexp = *pos;

      switch (GET_CODE (subexp))
	{
	case AND:
	  /* The switch code generation in write_predicate_stmts prefers
	     rtx code tests to be at the top of the expression tree.  So
	     push this AND down into the second operand of an existing
	     AND expression.  */
	  if (generate_switch_p (XEXP (subexp, 0)))
	    pos = &XEXP (subexp, 1);
	  goto break_loop;

	case IOR:
	  {
	    int test0 = NO_MODE_TEST (XEXP (subexp, 0));
	    int test1 = NO_MODE_TEST (XEXP (subexp, 1));

	    gcc_assert (test0 || test1);

	    if (test0 && test1)
	      goto break_loop;
	    pos = test0 ? &XEXP (subexp, 0) : &XEXP (subexp, 1);
	  }
	  break;

	case IF_THEN_ELSE:
	  {
	    int test0 = NO_MODE_TEST (XEXP (subexp, 0));
	    int test1 = NO_MODE_TEST (XEXP (subexp, 1));
	    int test2 = NO_MODE_TEST (XEXP (subexp, 2));

	    gcc_assert ((test0 && test1) || test2);

	    if (test0 && test1 && test2)
	      goto break_loop;
	    if (test0 && test1)
	      /* Must put it on the dependent clause, not the
	      	 controlling expression, or we change the meaning of
	      	 the test.  */
	      pos = &XEXP (subexp, 1);
	    else
	      pos = &XEXP (subexp, 2);
	  }
	  break;

	default:
	  goto break_loop;
	}
    }
 break_loop:
  XEXP (and_exp, 0) = *pos;
  *pos = and_exp;
}

/* PATH is a string describing a path from the root of an RTL
   expression to an inner subexpression to be tested.  Output
   code which computes the subexpression from the variable
   holding the root of the expression.  */
static void
write_extract_subexp (NString *outFile,const char *path)
{
  int len = strlen (path);
  int i;

  /* We first write out the operations (XEXP or XVECEXP) in reverse
     order, then write "op", then the indices in forward order.  */
  for (i = len - 1; i >= 0; i--)
    {
      if (ISLOWER (path[i]))
         n_string_append(outFile,"XVECEXP (");
      else if (ISDIGIT (path[i]))
         n_string_append(outFile,"XEXP (");
      else
	gcc_unreachable ();
    }

  n_string_append(outFile,"op");

  for (i = 0; i < len; i++)
    {
      if (ISLOWER (path[i]))
         n_string_append_printf(outFile,", 0, %d)", path[i] - 'a');
      else if (ISDIGIT (path[i]))
         n_string_append_printf(outFile,", %d)", path[i] - '0');
      else
	gcc_unreachable ();
    }
}

/* CODES is a list of RTX codes.  Write out an expression which
   determines whether the operand has one of those codes.  */
static void
write_match_code (NString *outFile,const char *path, const char *codes)
{
  const char *code;

  while ((code = scan_comma_elt (&codes)) != 0)
    {
     n_string_append(outFile,"GET_CODE (");
      write_extract_subexp (outFile,path);
      n_string_append(outFile,") == ");
      while (code < codes)
	{
         n_string_append_printf(outFile,"%c",TOUPPER (*code));
	  code++;
	}

      if (*codes == ',')
         n_string_append(outFile," || ");
    }
}

//代替 rtx_reader_ptr->print_c_condition (f, exp);
static void print_c_condition(NString *outFile,char *exp)
{
   int pid=getpid();
   char fileName[256];
   sprintf(fileName,"/dev/shm/temp_-#$_%d",pid);
   FILE *f=fopen(fileName,"w");
   rtx_reader_ptr->print_c_condition (f, exp);
   fclose(f);
   char buffer[1024];
   FILE *op=fopen(fileName,"r");
   int rev=fread(buffer,1,1024,op);
   fclose(op);
   buffer[rev]='\0';
   remove(fileName);
   n_string_append(outFile,buffer);
}
/* EXP is an RTL (sub)expression for a predicate.  Recursively
   descend the expression and write out an equivalent C expression.  */
static void
write_predicate_expr (NString *outFile,rtx exp)
{
  switch (GET_CODE (exp))
    {
    case AND:
       n_string_append(outFile,"(");
      write_predicate_expr (outFile,XEXP (exp, 0));
      n_string_append(outFile,") && (");
      write_predicate_expr (outFile,XEXP (exp, 1));
      n_string_append(outFile,")");
      break;

    case IOR:
       n_string_append(outFile,"(");
      write_predicate_expr (outFile,XEXP (exp, 0));
      n_string_append(outFile,") || (");
      write_predicate_expr (outFile,XEXP (exp, 1));
      n_string_append(outFile,")");
      break;

    case NOT:
       n_string_append(outFile,"!(");
      write_predicate_expr (outFile,XEXP (exp, 0));
      n_string_append(outFile,")");
      break;

    case IF_THEN_ELSE:
       n_string_append(outFile,"(");
      write_predicate_expr (outFile,XEXP (exp, 0));
      n_string_append(outFile,") ? (");
      write_predicate_expr (outFile,XEXP (exp, 1));
      n_string_append(outFile,") : (");
      write_predicate_expr (outFile,XEXP (exp, 2));
      n_string_append(outFile,")");
      break;

    case MATCH_OPERAND:
      if (GET_MODE (exp) == VOIDmode)
         n_string_append_printf(outFile,"%s (op, mode)", XSTR (exp, 1));
      else
         n_string_append_printf(outFile,"%s (op, %smode)", XSTR (exp, 1),
               mtcs_gen_get_mode_name/*!mode_name[GET_MODE (exp)]*/(mtcs_gen_get(),GET_MODE (exp)));
      break;

    case MATCH_CODE:
      write_match_code(outFile,XSTR (exp, 1), XSTR (exp, 0));
      break;

    case MATCH_TEST:
      print_c_condition/*!rtx_reader_ptr->print_c_condition*/(outFile, XSTR (exp, 0));
      break;

    default:
      gcc_unreachable ();
    }
}

/* Write the MATCH_CODE expression EXP as a switch statement.  */

static void
write_match_code_switch (NString *outFile,rtx exp)
{
  const char *codes = XSTR (exp, 0);
  const char *path = XSTR (exp, 1);
  const char *code;

  n_string_append(outFile,"  switch (GET_CODE (");
  write_extract_subexp(outFile,path);
  n_string_append(outFile,"))\n    {\n");

  while ((code = scan_comma_elt (&codes)) != 0)
    {
     n_string_append(outFile,"    case ");
      while (code < codes)
	{
         n_string_append_printf(outFile,"%c",TOUPPER (*code));
	  code++;
	}
      n_string_append(outFile,":\n");
    }
}

/* Given a predicate expression EXP, write out a sequence of stmts
   to evaluate it.  This is similar to write_predicate_expr but can
   generate efficient switch statements.  */

static void
write_predicate_stmts (NString *outFile,rtx exp)
{
  switch (GET_CODE (exp))
    {
    case MATCH_CODE:
      if (generate_switch_p (exp))
	{
	  write_match_code_switch (outFile,exp);
	  n_string_append(outFile,"      return true;\n"
		"    default:\n"
		"      break;\n"
		"    }\n"
		"  return false;\n");
	  return;
	}
      break;

    case AND:
      if (generate_switch_p (XEXP (exp, 0)))
	{
	  write_match_code_switch(outFile,XEXP (exp, 0));
	  n_string_append(outFile,"      break;\n"
		"    default:\n"
		"      return false;\n"
		"    }\n");
	  exp = XEXP (exp, 1);
	}
      break;

    case IOR:
      if (generate_switch_p (XEXP (exp, 0)))
	{
	  write_match_code_switch (outFile,XEXP (exp, 0));
	  n_string_append(outFile,"      return true;\n"
		"    default:\n"
		"      break;\n"
		"    }\n");
	  exp = XEXP (exp, 1);
	}
      break;

    case NOT:
      if (generate_switch_p (XEXP (exp, 0)))
	{
	  write_match_code_switch (outFile,XEXP (exp, 0));
	  n_string_append(outFile,"      return false;\n"
		"    default:\n"
		"      break;\n"
		"    }\n"
		"  return true;\n");
	  return;
	}
      break;

    default:
      break;
    }

  n_string_append(outFile,"  return ");
  write_predicate_expr(outFile,exp);
  n_string_append(outFile,";\n");
}

/* Given a predicate, write out a complete C function to compute it.  */
static void
write_one_predicate_function (NString *outFile,struct pred_data *p)
{
  if (!p->exp)
    return;
  const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei
  write_predicate_subfunction (outFile,p);
  add_mode_tests (p);

  /* A normal predicate can legitimately not look at machine_mode
     if it accepts only CONST_INTs and/or CONST_WIDE_INT and/or CONST_DOUBLEs.  */
 // if(strcmp("aligned_register_operand",p->name)==0)
    // fprintf(outFile,"bool\n%s_%s (rtx op, machine_mode mode ATTRIBUTE_UNUSED)\n{\n",platName,p->name);
  //else
  n_string_append_printf(outFile,"bool\n%s (rtx op, machine_mode mode ATTRIBUTE_UNUSED)\n{\n",p->name);
  write_predicate_stmts (outFile,p->exp);
  n_string_append(outFile,"}\n\n");
}

/* Constraints fall into two categories: register constraints
   (define_register_constraint), and others (define_constraint,
   define_memory_constraint, define_special_memory_constraint,
   define_relaxed_memory_constraint, define_address_constraint).  We work out
   automatically which of the various old-style macros they correspond to, and
   produce appropriate code.  They all go in the same hash table so we can
   verify that there are no duplicate names.  */

/* All data from one constraint definition.  */
class constraint_data
{
public:
  class constraint_data *next_this_letter;
  class constraint_data *next_textual;
  const char *name;
  const char *c_name;    /* same as .name unless mangling is necessary */
  file_location loc;     /* location of definition */
  size_t namelen;
  const char *regclass;  /* for register constraints */
  rtx exp;               /* for other constraints */
  const char *filter;    /* the register filter condition, or null if none */
  unsigned int is_register	: 1;
  unsigned int is_const_int	: 1;
  unsigned int is_const_dbl	: 1;
  unsigned int is_extra		: 1;
  unsigned int is_memory	: 1;
  unsigned int is_special_memory: 1;
  unsigned int is_relaxed_memory: 1;
  unsigned int is_address	: 1;
  unsigned int maybe_allows_reg : 1;
  unsigned int maybe_allows_mem : 1;
};

/* Overview of all constraints beginning with a given letter.  */

static class constraint_data *
constraints_by_letter_table[1<<CHAR_BIT];

/* For looking up all the constraints in the order that they appeared
   in the machine description.  */
static class constraint_data *first_constraint;
static class constraint_data **last_constraint_ptr = &first_constraint;

#define FOR_ALL_CONSTRAINTS(iter_) \
  for (iter_ = first_constraint; iter_; iter_ = iter_->next_textual)

/* Contraint letters that have a special meaning and that cannot be used
   in define*_constraints.  */
static const char generic_constraint_letters[] = "g";

/* Machine-independent code expects that constraints with these
   (initial) letters will allow only (a subset of all) CONST_INTs.  */

static const char const_int_constraints[] = "IJKLMNOP";

/* Machine-independent code expects that constraints with these
   (initial) letters will allow only (a subset of all) CONST_DOUBLEs.  */

static const char const_dbl_constraints[] = "GH";

/* Summary data used to decide whether to output various functions and
   macro definitions.  */
static unsigned int constraint_max_namelen;
static bool have_register_constraints;
static bool have_memory_constraints;
static bool have_special_memory_constraints;
static bool have_relaxed_memory_constraints;
static bool have_address_constraints;
static bool have_extra_constraints;
static bool have_const_int_constraints;
static unsigned int num_constraints;

static const constraint_data **enum_order;
static unsigned int register_start, register_end;
static unsigned int satisfied_start;
static unsigned int const_int_start, const_int_end;
static unsigned int memory_start, memory_end;
static unsigned int special_memory_start, special_memory_end;
static unsigned int relaxed_memory_start, relaxed_memory_end;
static unsigned int address_start, address_end;
static unsigned int maybe_allows_none_start, maybe_allows_none_end;
static unsigned int maybe_allows_reg_start, maybe_allows_reg_end;
static unsigned int maybe_allows_mem_start, maybe_allows_mem_end;

/* Convert NAME, which contains angle brackets and/or underscores, to
   a string that can be used as part of a C identifier.  The string
   comes from the rtl_obstack.  */
static const char *
mangle (const char *name)
{
  for (; *name; name++)
    switch (*name)
      {
      case '_': obstack_grow (rtl_obstack, "__", 2); break;
      case '<':	obstack_grow (rtl_obstack, "_l", 2); break;
      case '>':	obstack_grow (rtl_obstack, "_g", 2); break;
      case ':': obstack_grow (rtl_obstack, "_c", 2); break;
      default: obstack_1grow (rtl_obstack, *name); break;
      }

  obstack_1grow (rtl_obstack, '\0');
  return XOBFINISH (rtl_obstack, const char *);
}

/* Add one constraint, of any sort, to the tables.  NAME is its name; REGCLASS
   is the register class, if any; EXP is the expression to test, if any;
   IS_MEMORY, IS_SPECIAL_MEMORY, IS_RELAXED_MEMORY and IS_ADDRESS indicate
   memory, special memory, and address constraints, respectively; LOC is the .md
   file location; FILTER is the filter condition for a register constraint,
   or null if none.

   Not all combinations of arguments are valid; most importantly, REGCLASS is
   mutually exclusive with EXP, and
   IS_MEMORY/IS_SPECIAL_MEMORY/IS_RELAXED_MEMORY/IS_ADDRESS are only meaningful
   for constraints with EXP.

   This function enforces all syntactic and semantic rules about what
   constraints can be defined.  */

static void
add_constraint (const char *name, const char *regclass,
		rtx exp, bool is_memory, bool is_special_memory,
		bool is_relaxed_memory, bool is_address, file_location loc,
		const char *filter = nullptr)
{
  class constraint_data *c, **iter, **slot;
  const char *p;
  bool need_mangled_name = false;
  bool is_const_int;
  bool is_const_dbl;
  size_t namelen;

  if (strcmp (name, "TARGET_MEM_CONSTRAINT") == 0)
    name = general_mem;

  if (exp && validate_exp (exp, name, loc))
    return;

  for (p = name; *p; p++)
    if (!ISALNUM (*p))
      {
	if (*p == '<' || *p == '>' || *p == '_' || *p == ':')
	  need_mangled_name = true;
	else
	  {
	    error_at (loc, "constraint name '%s' must be composed of letters,"
		      " digits, underscores, colon and angle brackets",
		      name);
	    return;
	  }
      }

  if (strchr (generic_constraint_letters, name[0]))
    {
      if (name[1] == '\0')
	error_at (loc, "constraint letter '%s' cannot be "
		  "redefined by the machine description", name);
      else
	error_at (loc, "constraint name '%s' cannot be defined by the machine"
		  " description, as it begins with '%c'", name, name[0]);
      return;
    }


  namelen = strlen (name);
  slot = &constraints_by_letter_table[(unsigned int)name[0]];
  for (iter = slot; *iter; iter = &(*iter)->next_this_letter)
    {
      /* This causes slot to end up pointing to the
	 next_this_letter field of the last constraint with a name
	 of equal or greater length than the new constraint; hence
	 the new constraint will be inserted after all previous
	 constraints with names of the same length.  */
      if ((*iter)->namelen >= namelen)
	slot = iter;

      if (!strcmp ((*iter)->name, name))
	{
	  error_at (loc, "redefinition of constraint '%s'", name);
	  message_at ((*iter)->loc, "previous definition is here");
	  return;
	}
      else if (!strncmp ((*iter)->name, name, (*iter)->namelen))
	{
	  error_at (loc, "defining constraint '%s' here", name);
	  message_at ((*iter)->loc, "renders constraint '%s' "
		      "(defined here) a prefix", (*iter)->name);
	  return;
	}
      else if (!strncmp ((*iter)->name, name, namelen))
	{
	  error_at (loc, "constraint '%s' is a prefix", name);
	  message_at ((*iter)->loc, "of constraint '%s' (defined here)",
		      (*iter)->name);
	  return;
	}
    }

  is_const_int = strchr (const_int_constraints, name[0]) != 0;
  is_const_dbl = strchr (const_dbl_constraints, name[0]) != 0;

  if (is_const_int || is_const_dbl)
    {
      enum rtx_code appropriate_code
	= is_const_int ? CONST_INT : CONST_DOUBLE;

      /* Consider relaxing this requirement in the future.  */
      if (regclass
	  || GET_CODE (exp) != AND
	  || GET_CODE (XEXP (exp, 0)) != MATCH_CODE
	  || strcmp (XSTR (XEXP (exp, 0), 0),
		     GET_RTX_NAME (appropriate_code)))
	{
	  if (name[1] == '\0')
	    error_at (loc, "constraint letter '%c' is reserved "
		      "for %s constraints", name[0],
		      GET_RTX_NAME (appropriate_code));
	  else
	    error_at (loc, "constraint names beginning with '%c' "
		      "(%s) are reserved for %s constraints",
		      name[0], name, GET_RTX_NAME (appropriate_code));
	  return;
	}

      if (is_memory || is_special_memory || is_relaxed_memory)
	{
	  if (name[1] == '\0')
	    error_at (loc, "constraint letter '%c' cannot be a "
		      "memory constraint", name[0]);
	  else
	    error_at (loc, "constraint name '%s' begins with '%c', "
		      "and therefore cannot be a memory constraint",
		      name, name[0]);
	  return;
	}
      else if (is_address)
	{
	  if (name[1] == '\0')
	    error_at (loc, "constraint letter '%c' cannot be an "
		      "address constraint", name[0]);
	  else
	    error_at (loc, "constraint name '%s' begins with '%c', "
		      "and therefore cannot be an address constraint",
		      name, name[0]);
	  return;
	}
    }


  c = XOBNEW (rtl_obstack, class constraint_data);
  c->name = name;
  c->c_name = need_mangled_name ? mangle (name) : name;
  c->loc = loc;
  c->namelen = namelen;
  c->regclass = regclass;
  c->exp = exp;
  c->filter = filter;
  c->is_register = regclass != 0;
  c->is_const_int = is_const_int;
  c->is_const_dbl = is_const_dbl;
  c->is_extra = !(regclass || is_const_int || is_const_dbl);
  c->is_memory = is_memory;
  c->is_special_memory = is_special_memory;
  c->is_relaxed_memory = is_relaxed_memory;
  c->is_address = is_address;
  c->maybe_allows_reg = true;
  c->maybe_allows_mem = true;
  if (exp)
    {
      char codes[NUM_RTX_CODE];
      compute_test_codes (exp, loc, codes);
      if (!codes[REG] && !codes[SUBREG])
	c->maybe_allows_reg = false;
      if (!codes[MEM])
	c->maybe_allows_mem = false;
    }
  c->next_this_letter = *slot;
  *slot = c;

  /* Insert this constraint in the list of all constraints in textual
     order.  */
  c->next_textual = 0;
  *last_constraint_ptr = c;
  last_constraint_ptr = &c->next_textual;

  constraint_max_namelen = MAX (constraint_max_namelen, strlen (name));
  have_register_constraints |= c->is_register;
  have_const_int_constraints |= c->is_const_int;
  have_extra_constraints |= c->is_extra;
  have_memory_constraints |= c->is_memory;
  have_special_memory_constraints |= c->is_special_memory;
  have_relaxed_memory_constraints |= c->is_relaxed_memory;
  have_address_constraints |= c->is_address;
  num_constraints += 1;
}

/* Process a DEFINE_CONSTRAINT, DEFINE_MEMORY_CONSTRAINT,
   DEFINE_SPECIAL_MEMORY_CONSTRAINT, DEFINE_RELAXED_MEMORY_CONSTRAINT, or
   DEFINE_ADDRESS_CONSTRAINT expression, C.  */
static void
process_define_constraint (md_rtx_info *info)
{
  add_constraint (XSTR (info->def, 0), 0, XEXP (info->def, 2),
		  GET_CODE (info->def) == DEFINE_MEMORY_CONSTRAINT,
		  GET_CODE (info->def) == DEFINE_SPECIAL_MEMORY_CONSTRAINT,
		  GET_CODE (info->def) == DEFINE_RELAXED_MEMORY_CONSTRAINT,
		  GET_CODE (info->def) == DEFINE_ADDRESS_CONSTRAINT,
		  info->loc);
}

/* Process a DEFINE_REGISTER_CONSTRAINT expression, C.  */
static void
process_define_register_constraint (md_rtx_info *info)
{
  add_constraint (XSTR (info->def, 0), XSTR (info->def, 1),
		  0, false, false, false, false, info->loc,
		  XSTR (info->def, 3));
}

/* Put the constraints into enum order.  We want to keep constraints
   of the same type together so that query functions can be simple
   range checks.  */
static void
choose_enum_order (void)
{
  class constraint_data *c;

  enum_order = XNEWVEC (const constraint_data *, num_constraints);
  unsigned int next = 0;

  register_start = next;
  FOR_ALL_CONSTRAINTS (c)
    if (c->is_register)
      enum_order[next++] = c;
  register_end = next;

  satisfied_start = next;

  const_int_start = next;
  FOR_ALL_CONSTRAINTS (c)
    if (c->is_const_int)
      enum_order[next++] = c;
  const_int_end = next;

  memory_start = next;
  FOR_ALL_CONSTRAINTS (c)
    if (c->is_memory)
      enum_order[next++] = c;
  memory_end = next;

  special_memory_start = next;
  FOR_ALL_CONSTRAINTS (c)
    if (c->is_special_memory)
      enum_order[next++] = c;
  special_memory_end = next;

  relaxed_memory_start = next;
  FOR_ALL_CONSTRAINTS (c)
    if (c->is_relaxed_memory)
      enum_order[next++] = c;
  relaxed_memory_end = next;

  address_start = next;
  FOR_ALL_CONSTRAINTS (c)
    if (c->is_address)
      enum_order[next++] = c;
  address_end = next;

  maybe_allows_none_start = next;
  FOR_ALL_CONSTRAINTS (c)
    if (!c->is_register && !c->is_const_int && !c->is_memory
	&& !c->is_special_memory && !c->is_relaxed_memory && !c->is_address
	&& !c->maybe_allows_reg && !c->maybe_allows_mem)
      enum_order[next++] = c;
  maybe_allows_none_end = next;

  maybe_allows_reg_start = next;
  FOR_ALL_CONSTRAINTS (c)
    if (!c->is_register && !c->is_const_int && !c->is_memory
	&& !c->is_special_memory && !c->is_relaxed_memory && !c->is_address
	&& c->maybe_allows_reg && !c->maybe_allows_mem)
      enum_order[next++] = c;
  maybe_allows_reg_end = next;

  maybe_allows_mem_start = next;
  FOR_ALL_CONSTRAINTS (c)
    if (!c->is_register && !c->is_const_int && !c->is_memory
	&& !c->is_special_memory && !c->is_relaxed_memory && !c->is_address
	&& !c->maybe_allows_reg && c->maybe_allows_mem)
      enum_order[next++] = c;
  maybe_allows_mem_end = next;

  FOR_ALL_CONSTRAINTS (c)
    if (!c->is_register && !c->is_const_int && !c->is_memory
	&& !c->is_special_memory && !c->is_relaxed_memory && !c->is_address
	&& c->maybe_allows_reg && c->maybe_allows_mem)
      enum_order[next++] = c;
  gcc_assert (next == num_constraints);
}

/* Write out an enumeration with one entry per machine-specific
   constraint.  */
static void
write_enum_constraint_num (NString *outFile)
{
   const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei
   const char *upperPlatName=mtcs_gen_get_platform_upper_name(mtcs_gen_get());

   n_string_append_printf(outFile,"#define %s_CONSTRAINT_NUM_DEFINED_P 1\n",upperPlatName);
   n_string_append_printf(outFile,"enum %s_constraint_num\n"
	 "{\n"
	 "  %s_CONSTRAINT__UNKNOWN = 0",platName,upperPlatName);
  for (unsigned int i = 0; i < num_constraints; ++i)
     n_string_append_printf(outFile,",\n  %s_CONSTRAINT_%s", upperPlatName,enum_order[i]->c_name);
  n_string_append_printf(outFile,",\n  %s_CONSTRAINT__LIMIT\n};\n\n",upperPlatName);
}

/* Write out a function which looks at a string and determines what
   constraint name, if any, it begins with.  */
static void
write_lookup_constraint_1 (NString *outFile)
{
   const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei
   const char *upperPlatName=mtcs_gen_get_platform_upper_name(mtcs_gen_get());
  unsigned int i;
  n_string_append_printf(outFile,"enum %s_constraint_num\n"
	"%s_lookup_constraint_1 (const char *str)\n"
	"{\n"
	"  switch (str[0])\n"
	"    {\n",platName,platName);

  for (i = 0; i < ARRAY_SIZE (constraints_by_letter_table); i++)
    {
      class constraint_data *c = constraints_by_letter_table[i];
      if (!c)
	continue;

      n_string_append_printf(outFile,"    case '%c':\n", i);
      if (c->namelen == 1)
         n_string_append_printf(outFile,"      return %s_CONSTRAINT_%s;\n", upperPlatName,c->c_name);
      else
	{
	  do
	    {
	      if (c->namelen > 2)
	         n_string_append_printf(outFile,"      if (!strncmp (str + 1, \"%s\", "
					     HOST_SIZE_T_PRINT_UNSIGNED "))\n"
			"        return %s_CONSTRAINT_%s;\n",
			c->name + 1, (fmt_size_t) (c->namelen - 1),upperPlatName,c->c_name);
	      else
	         n_string_append_printf(outFile,"      if (str[1] == '%c')\n"
			"        return %s_CONSTRAINT_%s;\n",	c->name[1], upperPlatName,c->c_name);
	      c = c->next_this_letter;
	    }
	  while (c);
	  n_string_append(outFile,"      break;\n");
	}
    }

  n_string_append_printf(outFile,"    default: break;\n"
	"    }\n"
	"  return %s_CONSTRAINT__UNKNOWN;\n"
	"}\n\n",upperPlatName);
}

/* Write out an array that maps single-letter characters to their
   constraints (if that fits in a character) or 255 if lookup_constraint_1
   must be called.  */
static void
write_lookup_constraint_array (NString *outFile)
{
   const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei
   const char *upperPlatName=mtcs_gen_get_platform_upper_name(mtcs_gen_get());
  unsigned int i;
  n_string_append_printf(outFile,"const unsigned char %s_lookup_constraint_array[] = {\n  ",platName);
  for (i = 0; i < ARRAY_SIZE (constraints_by_letter_table); i++)
    {
      if (i != 0)
         n_string_append(outFile,",\n  ");
      class constraint_data *c = constraints_by_letter_table[i];
      if (!c)
         n_string_append_printf(outFile,"%s_CONSTRAINT__UNKNOWN",upperPlatName);
      else if (c->namelen == 1)
         n_string_append_printf(outFile,"MIN ((int) %s_CONSTRAINT_%s, (int) UCHAR_MAX)", upperPlatName,c->c_name);
      else
         n_string_append(outFile,"UCHAR_MAX");
    }
  n_string_append(outFile,"\n};\n\n");
}

/* Write out a function which looks at a string and determines what
   the constraint name length is.  */
static void
write_insn_constraint_len (NString *outFile)
{
   const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei
  unsigned int i;

  n_string_append_printf(outFile,"static inline size_t\n"
	"%s_insn_constraint_len (char fc, const char *str ATTRIBUTE_UNUSED)\n"
	"{\n"
	"  switch (fc)\n"
	"    {\n",platName);

  for (i = 0; i < ARRAY_SIZE (constraints_by_letter_table); i++)
    {
      class constraint_data *c = constraints_by_letter_table[i];

      if (!c
      	  || c->namelen == 1)
	continue;

      /* Constraints with multiple characters should have the same
	 length.  */
      {
	class constraint_data *c2 = c->next_this_letter;
	size_t len = c->namelen;
	while (c2)
	  {
	    if (c2->namelen != len)
	      error ("Multi-letter constraints with first letter '%c' "
		     "should have same length", i);
	    c2 = c2->next_this_letter;
	  }
      }

      n_string_append_printf(outFile,"    case '%c': return " HOST_SIZE_T_PRINT_UNSIGNED ";\n",
	      i, (fmt_size_t) c->namelen);
    }

  n_string_append(outFile,"    default: break;\n"
	"    }\n"
	"  return 1;\n"
	"}\n\n");
}

/* Write out the function which computes the register class corresponding
   to a register constraint.  */
static void
write_reg_class_for_constraint_1 (NString *outFile)
{
   const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei
   const char *upperPlatName=mtcs_gen_get_platform_upper_name(mtcs_gen_get());

  class constraint_data *c;

  n_string_append_printf(outFile,"enum reg_class\n"
	"%s_reg_class_for_constraint_1 (enum %s_constraint_num c)\n"
	"{\n"
	"  switch (c)\n"
	"    {\n",platName,platName);

  FOR_ALL_CONSTRAINTS (c)
    if (c->is_register)
       n_string_append_printf(outFile,"    case %s_CONSTRAINT_%s: return %s;\n", upperPlatName,c->c_name, c->regclass);

  n_string_append(outFile,"    default: break;\n"
	"    }\n"
	"  return NO_REGS;\n"
	"}\n\n");
}

/* Write out the functions which compute whether a given value matches
   a given non-register constraint.  */
static void
write_tm_constrs_h (NString *outFile)
{
  class constraint_data *c;
  const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei
  const char *upperPlatName=mtcs_gen_get_platform_upper_name(mtcs_gen_get());

  n_string_append_printf(outFile,"\
/* Generated automatically by the program '%s'\n\
   from the machine description file '%s'.  */\n\n", progname,
	  md_reader_ptr->get_top_level_filename ());

  n_string_append_printf(outFile,"\
#ifndef %s_INSN_CONSTRS_H\n\
#define %s_INSN_CONSTRS_H\n\n",upperPlatName,upperPlatName);

  FOR_ALL_CONSTRAINTS (c)
    if (!c->is_register)
      {
	bool needs_ival = needs_variable (c->exp, "ival");
	bool needs_hval = needs_variable (c->exp, "hval");
	bool needs_lval = needs_variable (c->exp, "lval");
	bool needs_rval = needs_variable (c->exp, "rval");
	bool needs_mode = (needs_variable (c->exp, "mode")
			   || needs_hval || needs_lval || needs_rval);
	bool needs_op = (needs_variable (c->exp, "op")
			 || needs_ival || needs_mode);

	n_string_append_printf(outFile,"static inline bool\n"
		"%s_satisfies_constraint_%s (rtx %s)\n"
		"{\n",platName, c->c_name,
		needs_op ? "op" : "ARG_UNUSED (op)");
	n_string_append(outFile,"   MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());\n");
	if (needs_mode)
	   n_string_append(outFile,"  machine_mode mode = GET_MODE (op);\n");
	if (needs_ival)
	   n_string_append(outFile,"  HOST_WIDE_INT ival = 0;\n");
	if (needs_hval)
	   n_string_append(outFile,"  HOST_WIDE_INT hval = 0;\n");
	if (needs_lval)
	   n_string_append(outFile,"  unsigned HOST_WIDE_INT lval = 0;\n");
	if (needs_rval)
	   n_string_append(outFile,"  const REAL_VALUE_TYPE *rval = 0;\n");

	if (needs_ival)
	   n_string_append(outFile,"  if (CONST_INT_P (op))\n"
		"    ival = INTVAL (op);\n");
#if TARGET_SUPPORTS_WIDE_INT
	if (needs_lval || needs_hval)
	  error ("you can't use lval or hval");
#else
	if (needs_hval)
	   n_string_append(outFile,"  if (GET_CODE (op) == CONST_DOUBLE && mode == VOIDmode)"
		"    hval = CONST_DOUBLE_HIGH (op);\n");
	if (needs_lval)
	   n_string_append(outFile,"  if (GET_CODE (op) == CONST_DOUBLE && mode == VOIDmode)"
		"    lval = CONST_DOUBLE_LOW (op);\n");
#endif
	if (needs_rval)
	   n_string_append(outFile,"  if (GET_CODE (op) == CONST_DOUBLE && mode != VOIDmode)"
		"    rval = CONST_DOUBLE_REAL_VALUE (op);\n");

	write_predicate_stmts (outFile,c->exp);
	n_string_append(outFile,"}\n");
      }

  n_string_append_printf(outFile,"#endif /* %s-insn-constrs.h */\n",platName);
}

/* Write out the wrapper function, constraint_satisfied_p, that maps
   a CONSTRAINT_xxx constant to one of the predicate functions generated
   above.  */
static void
write_constraint_satisfied_p_array (NString *outFile)
{
   const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei
   const char *upperPlatName=mtcs_gen_get_platform_upper_name(mtcs_gen_get());
  if (satisfied_start == num_constraints)
    return;

  n_string_append_printf(outFile,"bool (*%s_constraint_satisfied_p_array[]) (rtx) = {\n  ",platName);
  for (unsigned int i = satisfied_start; i < num_constraints; ++i)
    {
      if (i != satisfied_start)
         n_string_append(outFile,",\n  ");
      n_string_append_printf(outFile,"%s_satisfies_constraint_%s", platName,enum_order[i]->c_name);
    }
  n_string_append(outFile,"\n};\n\n");
}

/* Write out the function which computes whether a given value matches
   a given CONST_INT constraint.  This doesn't just forward to
   constraint_satisfied_p because caller passes the INTVAL, not the RTX.  */
static void
write_insn_const_int_ok_for_constraint (NString *outFile)
{
   const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei
   const char *upperPlatName=mtcs_gen_get_platform_upper_name(mtcs_gen_get());

  class constraint_data *c;

  n_string_append_printf(outFile,"bool\n"
	"%s_insn_const_int_ok_for_constraint (HOST_WIDE_INT ival, "
	                                  "enum %s_constraint_num c)\n"
	"{\n"
	"  switch (c)\n"
	"    {\n",platName,platName);

  FOR_ALL_CONSTRAINTS (c)
    if (c->is_const_int)
      {
       n_string_append_printf(outFile,"    case %s_CONSTRAINT_%s:\n      return ", upperPlatName,c->c_name);
	/* c->exp is guaranteed to be (and (match_code "const_int") (...));
	   we know at this point that we have a const_int, so we need not
	   bother with that part of the test.  */
	write_predicate_expr (outFile,XEXP (c->exp, 1));
	n_string_append(outFile,";\n\n");
      }

  n_string_append(outFile,"    default: break;\n"
	"    }\n"
	"  return false;\n"
	"}\n\n");
}


/* Print the init_reg_class_start_regs function, which initializes
   this_target_constraints->register_filters from the C conditions
   in the define_register_constraints.  */
static void
write_init_reg_class_start_regs (NString *outFile)
{
   const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei
   n_string_append_printf(outFile,"\n"
	  "void\n"
	  "%s_init_reg_class_start_regs ()\n"
	  "{\n",platName);
  if (!register_filters.is_empty ())
    {
     n_string_append(outFile,"  for (unsigned int regno = 0; regno < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsTarget->mtcsReg);"
	      " ++regno)\n"
	      "    {\n");
      for (unsigned int i = 0; i < register_filters.length (); ++i)
	{
         n_string_append(outFile,"      if (");
         print_c_condition/*!rtx_reader_ptr->print_c_condition*/(outFile, register_filters[i]);
	  n_string_append_printf(outFile,")\n"
		  "        mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsTarget->mtcsReg,&%s[%d], regno);\n",
		  "%s_default_target_constraints.register_filters", i,platName);
	}
      n_string_append(outFile,"    }\n");
    }
  n_string_append(outFile,"}\n");
}

/* Write a definition for a function NAME that returns true if a given
   constraint_num is in the range [START, END).  */
static void
write_range_function (NString *outFile,const char *name, unsigned int start, unsigned int end)
{
   const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei
   const char *upperPlatName=mtcs_gen_get_platform_upper_name(mtcs_gen_get());
   n_string_append(outFile,"static inline bool\n");
  if (start != end)
     n_string_append_printf(outFile,"%s_%s (enum %s_constraint_num c)\n"
	    "{\n"
	    "  return c >= %s_CONSTRAINT_%s && c <= %s_CONSTRAINT_%s;\n"
	    "}\n\n",
	    platName,name, platName,upperPlatName,enum_order[start]->c_name, upperPlatName,enum_order[end - 1]->c_name);
  else
     n_string_append_printf(outFile,"%s_%s (enum %s_constraint_num)\n"
	    "{\n"
	    "  return false;\n"
	    "}\n\n", platName,name,platName);
}

/* Write a definition for insn_extra_constraint_allows_reg_mem function.  */
static void
write_allows_reg_mem_function (NString *outFile)
{
   const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei
   const char *upperPlatName=mtcs_gen_get_platform_upper_name(mtcs_gen_get());

   n_string_append_printf(outFile,"static inline void\n"
	  "%s_insn_extra_constraint_allows_reg_mem (enum %s_constraint_num c,\n"
	  "\t\t\t\t      bool *allows_reg, bool *allows_mem)\n"
	  "{\n",platName,platName);
  if (maybe_allows_none_start != maybe_allows_none_end)
     n_string_append_printf(outFile,"  if (c >= %s_CONSTRAINT_%s && c <= %s_CONSTRAINT_%s)\n"
	    "    return;\n",upperPlatName,
	    enum_order[maybe_allows_none_start]->c_name,upperPlatName,
	    enum_order[maybe_allows_none_end - 1]->c_name);
  if (maybe_allows_reg_start != maybe_allows_reg_end)
     n_string_append_printf(outFile,"  if (c >= %s_CONSTRAINT_%s && c <= %s_CONSTRAINT_%s)\n"
	    "    {\n"
	    "      *allows_reg = true;\n"
	    "      return;\n"
	    "    }\n",upperPlatName,
	    enum_order[maybe_allows_reg_start]->c_name,upperPlatName,
	    enum_order[maybe_allows_reg_end - 1]->c_name);
  if (maybe_allows_mem_start != maybe_allows_mem_end)
     n_string_append_printf(outFile,"  if (c >= %s_CONSTRAINT_%s && c <= %s_CONSTRAINT_%s)\n"
	    "    {\n"
	    "      *allows_mem = true;\n"
	    "      return;\n"
	    "    }\n",upperPlatName,
	    enum_order[maybe_allows_mem_start]->c_name,upperPlatName,
	    enum_order[maybe_allows_mem_end - 1]->c_name);
  n_string_append_printf(outFile,"  (void) c;\n"
	  "  *allows_reg = true;\n"
	  "  *allows_mem = true;\n"
	  "}\n\n");
}


/* VEC is a list of key/value pairs, with the keys being lower bounds
   of a range.  Output a decision tree that handles the keys covered by
   [VEC[START], VEC[END]), returning FALLBACK for keys lower then VEC[START]'s.
   INDENT is the number of spaces to indent the code.  */
static void
print_type_tree (NString *outFile,const vec <std::pair <unsigned int, const char *> > &vec,
		 unsigned int start, unsigned int end, const char *fallback,
		 unsigned int indent)
{
   const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei
   const char *upperPlatName=mtcs_gen_get_platform_upper_name(mtcs_gen_get());
  while (start < end)
    {
      unsigned int mid = (start + end) / 2;
      n_string_append_printf(outFile,"%*sif (c >= %s_CONSTRAINT_%s)\n",
	      indent, "", upperPlatName,enum_order[vec[mid].first]->c_name);
      if (mid + 1 == end)
	print_type_tree (outFile,vec, mid + 1, end, vec[mid].second, indent + 2);
      else
	{
         n_string_append_printf(outFile,"%*s{\n", indent + 2, "");
	  print_type_tree (outFile,vec, mid + 1, end, vec[mid].second, indent + 4);
	  n_string_append_printf(outFile,"%*s}\n", indent + 2, "");
	}
      end = mid;
    }
  n_string_append_printf(outFile,"%*sreturn %s_%s;\n", indent, "", upperPlatName,fallback);
}


/* Print the get_register_filter function, which returns a pointer
   to the start register filter for a given constraint, or null if none.  */
static void
write_get_register_filter (NString *outFile)
{
   const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei
   const char *upperPlatName=mtcs_gen_get_platform_upper_name(mtcs_gen_get());
  constraint_data *c;

  n_string_append_printf(outFile,"\n"
	  "static inline const HardRegSet *\n"
	  "%s_get_register_filter (%s_constraint_num%s)\n",platName,platName,
	  register_filters.is_empty () ? "" : " c");
  n_string_append(outFile,"{\n");
  FOR_ALL_CONSTRAINTS (c)
    if (c->is_register && c->filter)
      {
       n_string_append_printf(outFile,"  if (c == %s_CONSTRAINT_%s)\n",upperPlatName, c->c_name);
       n_string_append_printf(outFile,"    return &%s_default_target_constraints.register_filters[%d];\n",
             platName,get_register_filter_id (c->filter));
      }
  n_string_append(outFile,"  return nullptr;\n"
	  "}\n"
	  "\n");
}

/* Print the get_register_filter_id function, which returns the index
   of the given constraint's register filter in
   this_target_constraints->register_filters, or -1 if none.  */
static void
write_get_register_filter_id (NString *outFile)
{
   const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei
   const char *upperPlatName=mtcs_gen_get_platform_upper_name(mtcs_gen_get());
  constraint_data *c;

  n_string_append_printf(outFile,"\n"
	  "static inline int\n"
	  "%s_get_register_filter_id (%s_constraint_num%s)\n",platName,platName,
	  register_filters.is_empty () ? "" : " c");
  n_string_append(outFile,"{\n");
  FOR_ALL_CONSTRAINTS (c)
    if (c->is_register && c->filter)
      {
       n_string_append_printf(outFile,"  if (c == %s_CONSTRAINT_%s)\n", upperPlatName,c->c_name);
       n_string_append_printf(outFile,"    return %d;\n", get_register_filter_id (c->filter));
      }
  n_string_append(outFile,"  return -1;\n"
	  "}\n");
}

//在gensupport.cc中定义的保留谓词
static char *reservePreds[]={
        {"general_operand"},
        {"address_operand"},
        {"register_operand"},
        {"pmode_register_operand"},
        {"scratch_operand"},
        {"immediate_operand"},
        {"const_int_operand"},
      #if TARGET_SUPPORTS_WIDE_INT
        {"const_scalar_int_operand"},
        {"const_double_operand"},
      #else
        {"const_double_operand"},
      #endif
        {"nonimmediate_operand"},
        {"nonmemory_operand"},
        {"push_operand"},
        {"pop_operand"},
        {"memory_operand"},
        {"indirect_operand"},
        {"ordered_comparison_operator"},
        {"comparison_operator"},
        {NULL}
};

static int isReservePreds(char *predsName)
{
   int i=0;
   char *item;
  while((item=reservePreds[i++])!=NULL){
     if(strcmp(item,predsName)==0)
        return 1;
  }
  return 0;
}

/* Write tm-preds.h.  Unfortunately, it is impossible to forward-declare
   an enumeration in portable C, so we have to condition all these
   prototypes on HAVE_MACHINE_MODES.  */
static void write_tm_preds_h (NString *outFile)
{
  struct pred_data *p;
  const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei
  const char *upperPlatName=mtcs_gen_get_platform_upper_name(mtcs_gen_get());

  n_string_append_printf(outFile,"\
/* Generated automatically by the program '%s'\n\
   from the machine description file '%s'.  */\n\n", progname,
	  md_reader_ptr->get_top_level_filename ());

  n_string_append_printf(outFile,"\
#ifndef %s_INSN_PREDS_H\n\
#define %s_INSN_PREDS_H\n\
   \n\
   \n",upperPlatName,upperPlatName);

  //aligned_register_operand来自mtcs_common.md 加ptx_ 前缀与示与host的tm-preds.h声明的方法aligned_register_operand区别。
  FOR_ALL_PREDICATES (p){
     if(!isReservePreds(p->name)){
        n_string_append_printf(outFile,"extern bool %s (rtx, machine_mode);\n",p->name);
     }
  }


  /* Print the definition of the target_constraints structure.  */
  n_string_append_printf(outFile,
	  "\nstruct %s_target_constraints {\n"
	  "  HardRegSet/*!HARD_REG_SET*/ register_filters[%d];\n",platName,
	  MAX (register_filters.length (), 1));
  n_string_append_printf(outFile,"};\n"
	  "\n"
	  "extern struct %s_target_constraints %s_default_target_constraints;\n",platName,platName);



  /* Print TEST_REGISTER_FILTER_BIT, which tests whether register REGNO
     is a valid start register for register filter ID.  */
  n_string_append_printf(outFile,"\n"
	  "#define %s_TEST_REGISTER_FILTER_BIT(ID, REGNO) \\\n",upperPlatName);
  if (register_filters.is_empty ())
     n_string_append(outFile,"  ((void) (ID), (void) (REGNO), false)\n");
  else
     n_string_append_printf(outFile,"  mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/ ("
	    "&%s_default_target_constraints.register_filters[ID], REGNO)\n",platName);

  /* Print test_register_filters, which tests whether register REGNO
     is a valid start register for the mask of register filters in MASK.  */
  n_string_append_printf(outFile,"\n"
	  "inline bool\n"
	  "%s_test_register_filters (unsigned int%s, unsigned int%s)\n",platName,
	  register_filters.is_empty () ? "" : " mask",
	  register_filters.is_empty () ? "" : " regno");
  n_string_append(outFile,"{\n");
  if (register_filters.is_empty ())
     n_string_append(outFile,"  return true;\n");
  else
    {
     n_string_append_printf(outFile,"  for (unsigned int id = 0; id < %d; ++id)\n",
	      register_filters.length ());
     n_string_append_printf(outFile,"    if ((mask & (1U << id))\n"
	      "\t&& !%s_TEST_REGISTER_FILTER_BIT (id, regno))\n"
	      "      return false;\n"
	      "  return true;\n",upperPlatName);
    }
  n_string_append(outFile,"}\n"
	  "\n");

  if (constraint_max_namelen > 0)
    {
      write_enum_constraint_num (outFile);
      n_string_append_printf(outFile,"extern enum %s_constraint_num %s_lookup_constraint_1 (const char *);\n"
	    "extern const unsigned char %s_lookup_constraint_array[];\n"
	    "\n"
	    "/* Return the constraint at the beginning of P, or"
	    " CONSTRAINT__UNKNOWN if it\n"
	    "   isn't recognized.  */\n"
	    "\n"
	    "static inline enum %s_constraint_num\n"
	    "%s_lookup_constraint (const char *p)\n"
	    "{\n"
	    "  unsigned int index = %s_lookup_constraint_array"
	    "[(unsigned char) *p];\n"
	    "  return (index == UCHAR_MAX\n"
	    "          ? %s_lookup_constraint_1 (p)\n"
	    "          : (enum %s_constraint_num) index);\n"
	    "}\n\n",platName,platName,platName,platName,platName,platName,platName,platName);
      if (satisfied_start == num_constraints)
         n_string_append_printf(outFile,"/* Return true if X satisfies constraint C.  */\n"
	      "\n"
	      "static inline bool\n"
	      "%s_constraint_satisfied_p (rtx, enum %s_constraint_num)\n"
	      "{\n"
	      "  return false;\n"
	      "}\n\n",platName,platName);
      else
         n_string_append_printf(outFile,"extern bool (*%s_constraint_satisfied_p_array[]) (rtx);\n"
		"\n"
		"/* Return true if X satisfies constraint C.  */\n"
		"\n"
		"static inline bool\n"
		"%s_constraint_satisfied_p (rtx x, enum %s_constraint_num c)\n"
		"{\n"
		"  int i = (int) c - (int) %s_CONSTRAINT_%s;\n"
		"  return i >= 0 && %s_constraint_satisfied_p_array[i] (x);\n"
		"}\n"
		"\n",platName,platName,platName,upperPlatName,
		enum_order[satisfied_start]->name,platName);

      write_range_function (outFile,"insn_extra_register_constraint",
			    register_start, register_end);
      write_range_function (outFile,"insn_extra_memory_constraint",
			    memory_start, memory_end);
      write_range_function (outFile,"insn_extra_special_memory_constraint",
			    special_memory_start, special_memory_end);
      write_range_function (outFile,"insn_extra_relaxed_memory_constraint",
			    relaxed_memory_start, relaxed_memory_end);
      write_range_function (outFile,"insn_extra_address_constraint",
			    address_start, address_end);
      write_allows_reg_mem_function (outFile);

      if (constraint_max_namelen > 1)
        {
	  write_insn_constraint_len (outFile);
	  n_string_append_printf(outFile,"#define %s_CONSTRAINT_LEN(c_,s_) "
		"%s_insn_constraint_len (c_,s_)\n\n",upperPlatName,platName);
	}
      else
         n_string_append_printf(outFile,"#define %s_CONSTRAINT_LEN(c_,s_) 1\n\n",upperPlatName);
      if (have_register_constraints)
         n_string_append_printf(outFile,"extern enum reg_class %s_reg_class_for_constraint_1 "
	      "(enum %s_constraint_num);\n"
	      "\n"
	      "static inline enum reg_class\n"
	      "%s_reg_class_for_constraint (enum %s_constraint_num c)\n"
	      "{\n"
	      "  if (%s_insn_extra_register_constraint (c))\n"
	      "    return %s_reg_class_for_constraint_1 (c);\n"
	      "  return NO_REGS;\n"
	      "}\n\n",platName,platName,platName,platName,platName,platName);
      else
         n_string_append_printf(outFile,"static inline enum reg_class\n"
	      "%s_reg_class_for_constraint (enum %s_constraint_num)\n"
	      "{\n"
	      "  return NO_REGS;\n"
	      "}\n\n");
      if (have_const_int_constraints)
         n_string_append_printf(outFile,"extern bool %s_insn_const_int_ok_for_constraint "
	      "(HOST_WIDE_INT, enum %s_constraint_num);\n"
	      "#define CONST_OK_FOR_CONSTRAINT_P(v_,c_,s_) \\\n"
	      "    %s_insn_const_int_ok_for_constraint (v_, "
	      "%s_lookup_constraint (s_))\n\n",platName,platName,platName,platName);
      else
         n_string_append_printf(outFile,"static inline bool\n"
	      "%s_insn_const_int_ok_for_constraint (HOST_WIDE_INT,"
	      " enum %s_constraint_num)\n"
	      "{\n"
	      "  return false;\n"
	      "}\n\n",platName,platName);

      n_string_append_printf(outFile,"enum %s_constraint_type\n"
	    "{\n"
	    "  %s_CT_REGISTER,\n"
	    "  %s_CT_CONST_INT,\n"
	    "  %s_CT_MEMORY,\n"
	    "  %s_CT_SPECIAL_MEMORY,\n"
	    "  %s_CT_RELAXED_MEMORY,\n"
	    "  %s_CT_ADDRESS,\n"
	    "  %s_CT_FIXED_FORM\n"
	    "};\n"
	    "\n"
	    "static inline enum %s_constraint_type\n"
	    "%s_get_constraint_type (enum %s_constraint_num c)\n"
	    "{\n",platName,upperPlatName,upperPlatName,upperPlatName,upperPlatName,upperPlatName,upperPlatName,upperPlatName,
	    platName,platName,platName);
      auto_vec <std::pair <unsigned int, const char *>, 4> values;
      if (const_int_start != const_int_end)
	values.safe_push (std::make_pair (const_int_start, "CT_CONST_INT"));
      if (memory_start != memory_end)
	values.safe_push (std::make_pair (memory_start, "CT_MEMORY"));
      if (special_memory_start != special_memory_end)
	values.safe_push (std::make_pair (special_memory_start,
					  "CT_SPECIAL_MEMORY"));
      if (relaxed_memory_start != relaxed_memory_end)
	values.safe_push (std::make_pair (relaxed_memory_start,
					  "CT_RELAXED_MEMORY"));
      if (address_start != address_end)
	values.safe_push (std::make_pair (address_start, "CT_ADDRESS"));
      if (address_end != num_constraints)
	values.safe_push (std::make_pair (address_end, "CT_FIXED_FORM"));
      print_type_tree (outFile,values, 0, values.length (), "CT_REGISTER", 2);
      n_string_append(outFile,"}\n");

      write_get_register_filter (outFile);
      write_get_register_filter_id (outFile);
    }
  n_string_append_printf(outFile,"\n void %s_init_reg_class_start_regs ();\n",platName);
  n_string_append_printf(outFile,"\nvoid %s_preds_set_target(void *target);\n\n",platName);
  n_string_append_printf(outFile,"#endif /* %s-insn-preds.h */\n",platName);
}

/* Write insn-preds.cc.
   N.B. the list of headers to include was copied from genrecog; it
   may not be ideal.

   FUTURE: Write #line markers referring back to the machine
   description.  (Can't practically do this now since we don't know
   the line number of the C block - just the line number of the enclosing
   expression.)  */
static void
write_insn_preds_c (NString *outFile)
{
  struct pred_data *p;

  n_string_append_printf(outFile,"\
/* Generated automatically by the program '%s'\n\
   from the machine description file '%s'.  */\n\n", progname,
	  md_reader_ptr->get_top_level_filename ());

  n_string_append(outFile,"\
#define IN_TARGET_CODE 1\n\
#include \"config.h\"\n\
#include \"system.h\"\n\
#include \"coretypes.h\"\n\
#include \"backend.h\"\n\
#include \"predict.h\"\n\
#include \"tree.h\"\n\
#include \"rtl.h\"\n\
#include \"alias.h\"\n\
#include \"varasm.h\"\n\
#include \"stor-layout.h\"\n\
#include \"calls.h\"\n\
#include \"memmodel.h\"\n\
#include \"tm_p.h\"\n\
#include \"insn-config.h\"\n\
#include \"recog.h\"\n\
#include \"output.h\"\n\
#include \"flags.h\"\n\
#include \"df.h\"\n\
#include \"resource.h\"\n\
#include \"diagnostic-core.h\"\n\
#include \"reload.h\"\n\
#include \"regs.h\"\n\
#include \"emit-rtl.h\"\n\
#include \"target.h\"\n");



  const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei
  const char *upperPlatName=mtcs_gen_get_platform_upper_name(mtcs_gen_get());
  n_string_append_printf(outFile,"\
#include \"%s-insn-flags.h\"\n\
#include \"../../mtcscompile.h\"\n\
#include \"../../mtcstarget.h\"\n\
#include \"%s-insn-preds.h\"\n\
#include \"%s-insn-constrs.h\"\n\n",platName,platName,platName);

  n_string_append(outFile,"static MtcsTarget *mtcsTarget;\n");
  n_string_append_printf(outFile,"void %s_preds_set_target(void *target)\n",platName);
  n_string_append(outFile,"{\n");
  n_string_append(outFile,"    mtcsTarget=(MtcsTarget *)target;\n");
  n_string_append(outFile,"}\n\n");

  n_string_append_printf(outFile,"\n"
	  "struct %s_target_constraints %s_default_target_constraints;\n",platName,platName);


  FOR_ALL_PREDICATES (p)
    write_one_predicate_function (outFile,p);

  if (constraint_max_namelen > 0)
    {
      write_lookup_constraint_1 (outFile);
      write_lookup_constraint_array (outFile);
      if (have_register_constraints)
	write_reg_class_for_constraint_1(outFile);
      write_constraint_satisfied_p_array(outFile);

      if (have_const_int_constraints)
	write_insn_const_int_ok_for_constraint(outFile);
    }

  write_init_reg_class_start_regs (outFile);
}


static void replacePredsFunc(NString *src)
{
   const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei
   struct pred_data *p;
   FOR_ALL_PREDICATES (p){
      if(!isReservePreds(p->name)){
         char replace[512];
         sprintf(replace,"%s_%s",platName,p->name);
         n_string_replace(src,p->name,replace,-1);
      }
   }
}

/*
在common.md中有9处需要替换,这9处生成的代码在 xxx-insn-constrs.h xxx-insn-preds.c中
mtcs_common.md  GET_MODE_SIZE  HARD_REGISTER_P  in_hard_reg_set_p GENERAL_REGS 仅用在 xxx-insn-preds.c中
memory_address_addr_space_p  MEM_ADDR_SPACE offsettable_nonstrict_memref_p LEGITIMATE_PIC_OPERAND_P GET_MODE_CLASS 仅用在 xxx-insn-constrs.h中
*/
static void replace_misc(NString *src)
{
   if(mtcs_gen_is_ptx(mtcs_gen_get())){
      char *patterns[9][2]={
         {"GET_MODE_SIZE (","mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsTarget->mtcsMode,"},
         {"HARD_REGISTER_P (","mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsTarget->mtcsReg,"},
         {"operand_reg_set (","&mtcsTarget->mtcsReg->hardRegs.x_operand_reg_set/*!operand_reg_set*/"},
         {"memory_address_addr_space_p (","mtcs_recog_memory_address_addr_space_p/*!memory_address_addr_space_p*/(mtcsTarget->mtcsRecog,"},
         {"MEM_ADDR_SPACE (","mtcs_rtl_get_mem_addr_space/*!MEM_ADDR_SPACE*/(mtcsTarget->mtcsRTL,"},
         {"LEGITIMATE_PIC_OPERAND_P (","mtcs_recog_is_legitimate_pic_operand_p/*!LEGITIMATE_PIC_OPERAND_P*/(mtcsTarget->mtcsRecog,"},
         {"GET_MODE_CLASS (","mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsTarget->mtcsMode,"},
         {"GENERAL_REGS","mtcs_reg_get_general_regs(mtcsTarget->mtcsReg)"},
         {"offsettable_nonstrict_memref_p (","mtcs_recog_offsettable_nonstrict_memref_p/*!offsettable_nonstrict_memref_p*/(mtcsTarget->mtcsRecog,"},
      };
      int i;
      for(i=0;i<9;i++)
         n_string_replace(src,patterns[i][0],patterns[i][1],-1);
   }
}
/* Master control.  */
int
main (int argc, const char **argv)
{
   progname = argv[0];

   if (argc <= 1)
      fatal ("no input file name");

   if (!init_rtx_reader_args_cb (argc, argv, mtcs_gen_handle_arg/*!parse_option*/))
      return FATAL_EXIT_CODE;

   const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());//zclei
   const char *saveRootPath = mtcs_gen_get_save_root_path(mtcs_gen_get());
   if(saveRootPath==NULL){
      error("在mtcsgenpreds的参数中没有根路径.");
      return FATAL_EXIT_CODE;
   }
   //ptx-insn-preds.h
   char preds_header_name[255];
   FILE *predsHeader = NULL;
   //ptx-insn-constrs.h
   char constrs_header_name[255];
   FILE *constrsHeader = NULL;
   //ptx-insn-preds.c
   FILE *predsFile = NULL;

   snprintf (preds_header_name, 255, "%s-insn-preds.h", platName);
   char predsHeaderFileName[255];
   sprintf(predsHeaderFileName,"%s/%s/gen/%s",saveRootPath,platName,preds_header_name);
   predsHeader = fopen (predsHeaderFileName, "w");

   snprintf (constrs_header_name, 255, "%s-insn-constrs.h", platName);
   char constrsHeaderFileName[255];
   sprintf(constrsHeaderFileName,"%s/%s/gen/%s",saveRootPath,platName,constrs_header_name);
   constrsHeader = fopen (constrsHeaderFileName, "w");

   predsFile=stdout;

   md_rtx_info info;
   while (read_md_rtx (&info))
      switch (GET_CODE (info.def)){
         case DEFINE_PREDICATE:
         case DEFINE_SPECIAL_PREDICATE:
            process_define_predicate (&info);
            break;

         case DEFINE_CONSTRAINT:
         case DEFINE_MEMORY_CONSTRAINT:
         case DEFINE_SPECIAL_MEMORY_CONSTRAINT:
         case DEFINE_RELAXED_MEMORY_CONSTRAINT:
         case DEFINE_ADDRESS_CONSTRAINT:
            process_define_constraint (&info);
            break;

         case DEFINE_REGISTER_CONSTRAINT:
            process_define_register_constraint (&info);
            break;

         default:
            break;
      }

   choose_enum_order ();
   NString *headContent=n_string_sized_new(512*1024);
   write_tm_preds_h (headContent);
   mtcs_gen_replace_plat_preds(mtcs_gen_get(),headContent);
   fprintf(predsHeader,"%s",headContent->str);
   NString *constrsContent=n_string_sized_new(256*1024);
   write_tm_constrs_h (constrsContent);
   replace_misc(constrsContent);
   fprintf(constrsHeader,"%s",constrsContent->str);

   NString *predsC=n_string_sized_new(1024*1024);
   write_insn_preds_c (predsC);
   mtcs_gen_replace_plat_preds(mtcs_gen_get(),predsC);
   mtcs_gen_replace_common_preds(mtcs_gen_get(),predsC);
   replace_misc(predsC);
   fprintf(stdout,"%s",predsC->str);


   fprintf(stderr,"mtcsgenpreds---- 55 %s\n",constrs_header_name);

   if (have_error || ferror (predsHeader) || fflush (predsHeader) || fclose (predsHeader))
      return FATAL_EXIT_CODE;
   if (have_error || ferror (constrsHeader) || fflush (constrsHeader) || fclose (constrsHeader))
      return FATAL_EXIT_CODE;
   if (have_error || ferror (stdout) || fflush (stdout) || fclose (stdout))
      return FATAL_EXIT_CODE;

   return SUCCESS_EXIT_CODE;
}
