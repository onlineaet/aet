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
 * base on output.cc
 */
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "predict.h"
#include "memmodel.h"
#include "tm_p.h"
#include "stringpool.h"
#include "regs.h"
#include "emit-rtl.h"
#include "cgraph.h"
#include "diagnostic-core.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "varasm.h"
#include "version.h"
#include "flags.h"
#include "stmt.h"
#include "expr.h"
#include "expmed.h"
#include "optabs.h"
#include "output.h"
#include "langhooks.h"
#include "debug.h"
#include "common/common-target.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "rtl-iter.h"
#include "file-prefix-map.h" /* remap_debug_filename()  */
#include "alloc-pool.h"
#include "toplev.h"
#include "opts.h"
#include "asan.h"
#include "recog.h"
#include "rtl-error.h"

#include "mtcsasm.h"
#include "mtcsoutput.h"
#include "mtcscompile.h"
#include "mtcsprintrtl.h"

static rtx walk_alter_subreg (MtcsOutput *self,rtx *xp, bool *changed);

/* Output of assembler code from a template, and its subroutines.  */

/* Annotate the assembly with a comment describing the pattern and
   alternative used.  */
static void output_asm_name (MtcsOutput *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsAsm  *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsInsnAttr *mtcsInsnAttr = mtcs_target_get_insn_attr(mtcsTarget);

  if (self->debug_insn){
      fprintf (mtcsAsm->asmFile, "\t%s %d\t",mtcs_asm_get_comment_start/*!ASM_COMMENT_START*/(mtcsAsm), INSN_UID (self->debug_insn));
      fprintf (mtcsAsm->asmFile, "[c=%d",insn_cost (self->debug_insn, optimize_insn_for_speed_p ()));
      if (mtcs_insn_attr_get_have_attr_length/*!HAVE_ATTR_length*/(mtcsInsnAttr))

          fprintf (mtcsAsm->asmFile, " l=%d",get_attr_length (self->debug_insn));
      fprintf (mtcsAsm->asmFile, "]  ");

      int num = INSN_CODE (self->debug_insn);
      fprintf (mtcsAsm->asmFile, "%s", self->insn_data[num].name);
      if (self->insn_data[num].n_alternatives > 1)
          fprintf (mtcsAsm->asmFile, "/%d", which_alternative);
          /* Clear this so only the first assembler insn
         of any rtl insn will get the special comment for -dp.  */
      self->debug_insn = 0;
  }
}

/* If OP is a REG or MEM and we can find a MEM_EXPR corresponding to it
   or its address, return that expr .  Set *PADDRESSP to 1 if the expr
   corresponds to the address of the object and 0 if to the object.  */
//mtcsasm.c也有，但没有用，应删除
static tree get_mem_expr_from_op (MtcsOutput *self,rtx op, int *paddressp)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  tree expr;
  int inner_addressp;
  *paddressp = 0;
  if (REG_P (op))
    return REG_EXPR (op);
  else if (!MEM_P (op))
    return 0;

  if (mtcs_rtl_get_mem_expr/*!MEM_EXPR*/(mtcsRTL,op) != 0)
    return mtcs_rtl_get_mem_expr/*!MEM_EXPR*/(mtcsRTL,op);

  /* Otherwise we have an address, so indicate it and look at the address.  */
  *paddressp = 1;
  op = XEXP (op, 0);

  /* First check if we have a decl for the address, then look at the right side
     if it is a PLUS.  Otherwise, strip off arithmetic and keep looking.
     But don't allow the address to itself be indirect.  */
  if ((expr = get_mem_expr_from_op(self,op, &inner_addressp)) && ! inner_addressp)
    return expr;
  else if (GET_CODE (op) == PLUS
       && (expr = get_mem_expr_from_op(self,XEXP (op, 1), &inner_addressp)))
    return expr;

  while (UNARY_P (op)
     || GET_RTX_CLASS (GET_CODE (op)) == RTX_BIN_ARITH)
    op = XEXP (op, 0);

  expr = get_mem_expr_from_op(self,op, &inner_addressp);
  return inner_addressp ? 0 : expr;
}


/* Output operand names for assembler instructions.  OPERANDS is the
   operand vector, OPORDER is the order to write the operands, and NOPS
   is the number of operands to write.  */

static void output_asm_operand_names (MtcsOutput *self,rtx *operands, int *oporder, int nops)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsAsm  *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

  int wrote = 0;
  int i;
  for (i = 0; i < nops; i++){
      int addressp;
      rtx op = operands[oporder[i]];
      tree expr = get_mem_expr_from_op(self,op, &addressp);
      fprintf (mtcsAsm->asmFile, "%c%s",wrote ? ',' : '\t', wrote ? "" : mtcs_asm_get_comment_start/*!ASM_COMMENT_START*/(mtcsAsm));
      wrote = 1;
      if (expr){
          fprintf (mtcsAsm->asmFile, "%s",addressp ? "*" : "");
          print_mem_expr (mtcsAsm->asmFile, expr);
          wrote = 1;
      }else if (REG_P (op) && ORIGINAL_REGNO (op) && ORIGINAL_REGNO (op) != REGNO (op))
          fprintf (mtcsAsm->asmFile, " tmp%i", ORIGINAL_REGNO (op));
  }
}

void mtcs_output_asm_label (MtcsOutput *self,rtx x)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsAsm  *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

  char buf[256];

  if (GET_CODE (x) == LABEL_REF)
    x = label_ref_label (x);
  if (LABEL_P (x)  || (NOTE_P (x)  && NOTE_KIND (x) == NOTE_INSN_DELETED_LABEL))
     mtcs_asm_generate_internal_label/*!ASM_GENERATE_INTERNAL_LABEL*/(mtcsAsm,buf, "L", CODE_LABEL_NUMBER (x));
  else
    mtcs_output_operand_lossage (self,"'%%l' operand isn't a label");
  mtcs_asm_assemble_name (mtcsAsm, buf);
}


/* Report inconsistency between the assembler template and the operands.
   In an `asm', it's the user's fault; otherwise, the compiler's fault.  */

void mtcs_output_operand_lossage (MtcsOutput *self,const char *cmsgid, ...)
{
  char *fmt_string;
  char *new_message;
  const char *pfx_str;
  va_list ap;

  va_start (ap, cmsgid);
  pfx_str = self->this_is_asm_operands ? "invalid 'asm': ": "output_operand: ";
  fmt_string = xasprintf ("%s%s", pfx_str, cmsgid);
  new_message = xvasprintf (fmt_string, ap);
  if (this_is_asm_operands)
    error_for_asm (self->this_is_asm_operands, "%s", new_message);
  else
    internal_error ("%s", new_message);

  free (fmt_string);
  free (new_message);
  va_end (ap);
}

/* Print a memory reference operand for address X using
   machine-dependent assembler syntax.  */
//原型 output_address output.h final.cc
void mtcs_output_address (MtcsOutput *self,machine_mode mode, rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   bool changed = false;
   walk_alter_subreg(self,&x, &changed);
   target_asm_out_print_operand_address/*targetm.asm_out.print_operand_address*/(mtcsMachine->asmOut, mode, x);
}

/* Output text from TEMPLATE to the assembler output file,
   obeying %-directions to substitute operands taken from
   the vector OPERANDS.

   %N (for N a digit) means print operand N in usual manner.
   %lN means require operand N to be a CODE_LABEL or LABEL_REF
      and print the label name with no punctuation.
   %cN means require operand N to be a constant
      and print the constant expression with no punctuation.
   %aN means expect operand N to be a memory address
      (not a memory reference!) and print a reference
      to that address.
   %nN means expect operand N to be a constant
      and print a constant expression for minus the value
      of the operand, with no other punctuation.  */
//原型 output_asm_insn oput.h final.cc
void mtcs_output_asm_insn (MtcsOutput *self,const char *templ, rtx *operands)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm  *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   const char *p;
   int c;
   int oporder[MAX_RECOG_OPERANDS];
   char opoutput[MAX_RECOG_OPERANDS];
   int ops = 0;
   /* An insn may return a null string template
   in a case where no assembler code is needed.  */
   if (*templ == 0)
      return;
   memset (opoutput, 0, sizeof opoutput);
   p = templ;
   putc ('\t', mtcsAsm->asmFile);
   while ((c = *p++))
      switch (c){
         case '\n':
            if (mtcsOptionsItem->x_flag_verbose_asm)
               output_asm_operand_names (self,operands, oporder, ops);
            if (mtcsOptionsItem->x_flag_print_asm_name)
               output_asm_name (self);
            ops = 0;
            memset (opoutput, 0, sizeof opoutput);
            putc (c, mtcsAsm->asmFile);
            break;
         case '%':
            /* %% outputs a single %.  %{, %} and %| print {, } and | respectively
            if ASSEMBLER_DIALECT defined and these characters have a special
            meaning as dialect delimiters.*/
            if (*p == '%'){
               n_debug("mtcsoutput.c mtcs_output_asm_insn 00 templ:%s 单个字符 %c\n",templ,*p);
               putc (*p, mtcsAsm->asmFile);
               p++;
            }
            /* %= outputs a number which is unique to each insn in the entire
            compilation.  This is useful for making local labels that are
            referred to more than once in a given insn.  */
            else if (*p == '='){
               p++;
               fprintf (mtcsAsm->asmFile, "%d", self->insn_counter);
            }
            /* % followed by a letter and some digits
            outputs an operand in a special way depending on the letter.
            Letters `acln' are implemented directly.
            Other letters are passed to `output_operand' so that
            the TARGET_PRINT_OPERAND hook can define them.  */
            else if (ISALPHA (*p)){
               int letter = *p++;
               unsigned long opnum;
               char *endptr;
               opnum = strtoul (p, &endptr, 10);
               if (endptr == p)
                  mtcs_output_operand_lossage (self,"operand number missing after %%-letter");
               else if (self->this_is_asm_operands && opnum >= self->insn_noperands)
                  mtcs_output_operand_lossage (self,"operand number out of range");
               else if (letter == 'l')
                  mtcs_output_asm_label (self,operands[opnum]);
               else if (letter == 'a')
                  mtcs_output_address (self,VOIDmode, operands[opnum]);
               else if (letter == 'c'){
                  if (mtcs_rtl_constant_address_p/*!CONSTANT_ADDRESS_P*/(mtcsRTL,operands[opnum]))
                     mtcs_output_addr_const (self, operands[opnum]);
                  else
                     mtcs_output_output_operand/*!output_operand*/(self,operands[opnum], 'c');
               }else if (letter == 'n'){
                  if (CONST_INT_P (operands[opnum]))
                     fprintf (mtcsAsm->asmFile, HOST_WIDE_INT_PRINT_DEC, - INTVAL (operands[opnum]));
                  else{
                     putc ('-', mtcsAsm->asmFile);
                     mtcs_output_addr_const (self, operands[opnum]);
                  }
               }else{
                  mtcs_output_output_operand/*!output_operand*/(self,operands[opnum], letter);
               }

               if (!opoutput[opnum])
                  oporder[ops++] = opnum;
               opoutput[opnum] = 1;

               p = endptr;
               c = *p;
            }
            /* % followed by a digit outputs an operand the default way.  */
            else if (ISDIGIT (*p)){
               unsigned long opnum;
               char *endptr;
               opnum = strtoul (p, &endptr, 10);
               n_debug("mtcsoutput.c mtcs_output_asm_insn 11 templ:%s 单个字符 %c\n",templ,*p);
               if (this_is_asm_operands && opnum >= self->insn_noperands)
                  mtcs_output_operand_lossage (self,"operand number out of range");
               else
                  mtcs_output_output_operand/*!output_operand*/(self,operands[opnum], 0);

               if (!opoutput[opnum])
                  oporder[ops++] = opnum;
               opoutput[opnum] = 1;

               p = endptr;
               c = *p;
            }
            /* % followed by punctuation: output something for that
            punctuation character alone, with no operand.  The
            TARGET_PRINT_OPERAND hook decides what is actually done.  */
            else if (target_asm_out_print_operand_punct_valid_p/*!targetm.asm_out.print_operand_punct_valid_p*/
                  (mtcsMachine->asmOut,(unsigned char) *p))
               mtcs_output_output_operand/*!output_operand*/(self,NULL_RTX, *p++);
            else
               mtcs_output_operand_lossage (self,"invalid %%-code");
            break;

         default:
            putc (c, mtcsAsm->asmFile);
      }

   /* Try to keep the asm a bit more readable.  */
   if ((mtcsOptionsItem->x_flag_verbose_asm || mtcsOptionsItem->x_flag_print_asm_name) && strlen (templ) < 9)
      putc ('\t', mtcsAsm->asmFile);
   /* Write out the variable names for operands, if we know them.  */
   if (mtcsOptionsItem->x_flag_verbose_asm)
      output_asm_operand_names (self,operands, oporder, ops);
   if (mtcsOptionsItem->x_flag_print_asm_name)
      output_asm_name (self);
   putc ('\n', mtcsAsm->asmFile);
}

/* Print an integer constant expression in assembler syntax.
   Addition and subtraction are the only arithmetic
   that may appear in these expressions.  */
//原型 output_addr_const output.h final.cc
void mtcs_output_addr_const (MtcsOutput *self, rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm  *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   FILE *file=mtcsAsm->asmFile;
   char buf[256];
restart:
   switch (GET_CODE (x)){
      case PC:
         putc ('.', file);
         break;

      case SYMBOL_REF:
         if (SYMBOL_REF_DECL (x))
            mtcs_asm_assemble_external (mtcsAsm,SYMBOL_REF_DECL (x));
         mtcs_asm_assemble_name (mtcsAsm, XSTR (x, 0));
         break;

      case LABEL_REF:
         x = label_ref_label (x);
      /* Fall through.  */
      case CODE_LABEL:
         mtcs_asm_generate_internal_label/*!ASM_GENERATE_INTERNAL_LABEL*/(mtcsAsm,buf, "L", CODE_LABEL_NUMBER (x));
         mtcs_asm_assemble_name (mtcsAsm, buf);
         break;

      case CONST_INT:
         fprintf (file, HOST_WIDE_INT_PRINT_DEC, INTVAL (x));
         break;

      case CONST:
         /* This used to output parentheses around the expression,
         but that does not work on the 386 (either ATT or BSD assembler).  */
         mtcs_output_addr_const (self, XEXP (x, 0));
         break;

      case CONST_WIDE_INT:
      /* We do not know the mode here so we have to use a round about
      way to build a wide-int to get it printed properly.  */
      {
         wide_int w = wide_int::from_array (&CONST_WIDE_INT_ELT (x, 0),CONST_WIDE_INT_NUNITS (x),
               CONST_WIDE_INT_NUNITS (x)* HOST_BITS_PER_WIDE_INT,false);
         print_decs (w, file);
      }
         break;

      case CONST_DOUBLE:
         if (CONST_DOUBLE_AS_INT_P (x)){
            /* We can use %d if the number is one word and positive.  */
            if (CONST_DOUBLE_HIGH (x))
               fprintf (file, HOST_WIDE_INT_PRINT_DOUBLE_HEX,
                     (unsigned HOST_WIDE_INT) CONST_DOUBLE_HIGH (x),(unsigned HOST_WIDE_INT) CONST_DOUBLE_LOW (x));
            else if (CONST_DOUBLE_LOW (x) < 0)
               fprintf (file, HOST_WIDE_INT_PRINT_HEX,(unsigned HOST_WIDE_INT) CONST_DOUBLE_LOW (x));
            else
               fprintf (file, HOST_WIDE_INT_PRINT_DEC, CONST_DOUBLE_LOW (x));
         }else
            /* We can't handle floating point constants;
            PRINT_OPERAND must handle them.  */
            mtcs_output_operand_lossage (self,"floating constant misused");
         break;

      case CONST_FIXED:
         fprintf (file, HOST_WIDE_INT_PRINT_DEC, CONST_FIXED_VALUE_LOW (x));
         break;

      case PLUS:
         /* Some assemblers need integer constants to appear last (eg masm).  */
         if (CONST_INT_P (XEXP (x, 0))){
            mtcs_output_addr_const (self, XEXP (x, 1));
            if (INTVAL (XEXP (x, 0)) >= 0)
               fprintf (file, "+");
            mtcs_output_addr_const (self, XEXP (x, 0));
         }else{
            mtcs_output_addr_const (self, XEXP (x, 0));
            if (!CONST_INT_P (XEXP (x, 1)) || INTVAL (XEXP (x, 1)) >= 0)
               fprintf (file, "+");
            mtcs_output_addr_const (self, XEXP (x, 1));
         }
         break;

      case MINUS:
         /* Avoid outputting things like x-x or x+5-x,
         since some assemblers can't handle that.  */
         x = simplify_subtraction (x);
         if (GET_CODE (x) != MINUS)
            goto restart;

         mtcs_output_addr_const (self, XEXP (x, 0));
         fprintf (file, "-");
         if ((CONST_INT_P (XEXP (x, 1)) && INTVAL (XEXP (x, 1)) >= 0)
               || GET_CODE (XEXP (x, 1)) == PC || GET_CODE (XEXP (x, 1)) == SYMBOL_REF)
            mtcs_output_addr_const (self, XEXP (x, 1));
         else{
            fputs (mtcsTarget->open_paren/*targetm.asm_out.open_paren*/, file);
            mtcs_output_addr_const (self, XEXP (x, 1));
            fputs (mtcsTarget->close_paren/*targetm.asm_out.close_paren*/, file);
         }
         break;

      case ZERO_EXTEND:
      case SIGN_EXTEND:
      case SUBREG:
      case TRUNCATE:
         mtcs_output_addr_const (self, XEXP (x, 0));
         break;

      default:
         if (target_asm_out_output_addr_const_extra/*targetm.asm_out.output_addr_const_extra*/(mtcsMachine->asmOut, x))
            break;
         mtcs_output_operand_lossage (self,"invalid expression as operand");
   }
}

void     mtcs_output_add_insn_count(MtcsOutput *self,int count)
{
   self->insn_counter+=count;
}

void     mtcs_output_init(MtcsOutput *self)
{
   self->insn_data=NULL;
   self->count=0;
   self->user_label_prefix=NULL;
   //尝试替换targetm.section_type_flags #define TARGET_SECTION_TYPE_FLAGS default_section_type_flags
   self->section_type_flags = mtcs_output_default_section_type_flags;
   self->encode_section_info =  mtcs_output_default_encode_section_info;
}

void   mtcs_output_set_insn_data(MtcsOutput *self,struct insn_data_d *insn_data,int count)
{
   self->insn_data=insn_data;
   self->count=count;
}

//原型 get_insn_name rtl.h insn-output.cc
const char * mtcs_output_get_insn_name(MtcsOutput *self,int code)
{
   return self->get_insn_name(self,code);
}

char mtcs_output_get_n_generator_args(MtcsOutput *self,int code)
{
   return self->insn_data[code].n_generator_args;
}

char mtcs_output_get_n_operands(MtcsOutput *self,int code)
{
   return self->insn_data[code].n_operands;
}

char mtcs_output_get_n_dups(MtcsOutput *self,int code)
{
   return self->insn_data[code].n_dups;
}

char mtcs_output_get_n_alternatives(MtcsOutput *self,int code)
{
   return self->insn_data[code].n_alternatives;
}

char mtcs_output_get_output_format(MtcsOutput *self,int code)
{
   return self->insn_data[code].output_format;
}

//以下是md中需要的方法
/*insn_data 还调用output_asm_insn 定义在final.cc
 * output_asm_insn 如果加mtcs_ 与mtcsoutput.h中声明的方法mtcs_ouput_asm_insn冲突。改为加nvptx_
 * 原型是output_asm_insn
 */
void md_output_asm_insn (const char *templ, rtx *operands)
{
   MtcsTarget *mtcsTarget=mtcs_compile_get_current_target(mtcs_compile_get());
   MtcsOutput *self=mtcs_target_get_output(mtcsTarget);
   mtcs_output_asm_insn(self,templ,operands);
}

void mtcs_output_set_user_label_prefix(MtcsOutput *self,char *label)
{
   if(self->user_label_prefix){
      free(self->user_label_prefix);
      self->user_label_prefix=NULL;
   }
   self->user_label_prefix=xstrdup(label);
}

/* Given NAME, a putative register name, discard any customary prefixes.  */
//原型 strip_reg_name varasm.cc
static const char *strip_reg_name (MtcsOutput *self,const char *name)
{
#ifdef REGISTER_PREFIX
  if (!strncmp (name, REGISTER_PREFIX, strlen (REGISTER_PREFIX)))
    name += strlen (REGISTER_PREFIX);
#endif
  if (name[0] == '%' || name[0] == '#')
    name++;
  return name;
}

/* Decode an `asm' spec for a declaration as a register name.
   Return the register number, or -1 if nothing specified,
   or -2 if the ASMSPEC is not `cc' or `memory' and is not recognized,
   or -3 if ASMSPEC is `cc' and is not recognized,
   or -4 if ASMSPEC is `memory' and is not recognized.
   Accept an exact spelling or a decimal number.
   Prefixes such as % are optional.  */
//原型 decode_reg_name_and_count output.h varasm.cc
int mtcs_output_decode_reg_name_and_count (MtcsOutput *self,const char *asmspec, int *pnregs)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

  int   firstPseudoRegister= mtcs_reg_get_first_pseudo_register(mtcsReg);
  /* Presume just one register is clobbered.  */
  *pnregs = 1;
  if (asmspec != 0){
      int i;
      /* Get rid of confusing prefixes.  */
      asmspec = strip_reg_name(self,asmspec);
      /* Allow a decimal number as a "register name".  */
      for (i = strlen (asmspec) - 1; i >= 0; i--)
          if (! ISDIGIT (asmspec[i]))
              break;
      if (asmspec[0] != 0 && i < 0){
          i = atoi (asmspec);
          if (i < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/ && i >= 0
                  && mtcs_reg_get_reg_name/*!reg_names[i][0]*/(mtcsReg,i)[0])
            return i;
          else
            return -2;
      }

      for (i = 0; i < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; i++)
          if (mtcs_reg_get_reg_name/*!reg_names[i]*/(mtcsReg,i)[0]   && ! strcmp (asmspec,
                  strip_reg_name(self,mtcs_reg_get_reg_name/*!reg_names[i]*/(mtcsReg,i))))
              return i;

#ifdef OVERLAPPING_REGISTER_NAMES
      {
        static const struct
        {
          const char *const name;
          const int number;
          const int nregs;
        } table[] = OVERLAPPING_REGISTER_NAMES;

        for (i = 0; i < (int) ARRAY_SIZE (table); i++)
          if (table[i].name[0]  && ! strcmp (asmspec, table[i].name)){
              *pnregs = table[i].nregs;
              return table[i].number;
          }
    }
#endif /* OVERLAPPING_REGISTER_NAMES */

#ifdef ADDITIONAL_REGISTER_NAMES
      {
        static const struct { const char *const name; const int number; } table[]
          = ADDITIONAL_REGISTER_NAMES;

        for (i = 0; i < (int) ARRAY_SIZE (table); i++)
           if (table[i].name[0] && ! strcmp (asmspec, table[i].name)
              && mtcs_reg_get_reg_name/*!reg_names[table[i].number]*/(mtcsReg,table[i].number)[0])
            return table[i].number;
      }
#endif /* ADDITIONAL_REGISTER_NAMES */

      if (!strcmp (asmspec, "memory"))
          return -4;

      if (!strcmp (asmspec, "cc"))
          return -3;

      return -2;
  }

  return -1;
}

//原型 decode_reg_name output.h varasm.cc
int mtcs_output_decode_reg_name (MtcsOutput *self,const char *name)
{
  int count;
  return decode_reg_name_and_count (name, &count);
}

//原型 get_insn_template output.h final.cc
const char * mtcs_output_get_insn_template (MtcsOutput *self,int code, rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog  *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   n_debug("mtcsoutput.c mtcs_output_get_insn_template :code:%d insn:%p outputformat:%d\n",
         code,insn,self->insn_data[code].output_format);
   mtcs_print_rtl_single(stderr,insn);

   switch (self->insn_data[code].output_format){
      case INSN_OUTPUT_FORMAT_SINGLE:
         return self->insn_data[code].output.single;
      case INSN_OUTPUT_FORMAT_MULTI:
         return self->insn_data[code].output.multi[which_alternative];
      case INSN_OUTPUT_FORMAT_FUNCTION:
         gcc_assert (insn);
         return (*self->insn_data[code].output.function) (mtcsRecog->recog_data.operand, insn);
      default:
         gcc_unreachable ();
   }
}
/* Return true if this function has no function calls.  */
//原型 leaf_function_p output.h final.cc
bool mtcs_output_leaf_function_p (MtcsOutput *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   rtx_insn *insn;

   /* Ensure we walk the entire function body.  */
   gcc_assert (!mtcs_rtl_data_in_sequence_p/*!in_sequence_p*/(mtcsRtlData));

   /* Some back-ends (e.g. s390) want leaf functions to stay leaf
   functions even if they call mcount.  */
   if (mtcsRtlData/*!crtl*/->profile && !mtcsTarget/*!targetm.keep_leaf_when_profiled*/->keep_leaf_when_profiled(mtcsTarget))
      return false;

   for (insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); insn; insn = NEXT_INSN (insn)){
      if (CALL_P (insn) && ! SIBLING_CALL_P (insn) && ! FAKE_CALL_P (insn))
         return false;
      if (NONJUMP_INSN_P (insn)
      && GET_CODE (PATTERN (insn)) == SEQUENCE
      && CALL_P (XVECEXP (PATTERN (insn), 0, 0))
      && ! SIBLING_CALL_P (XVECEXP (PATTERN (insn), 0, 0)))
         return false;
   }
   return true;
}


/* If X is a SUBREG, try to replace it with a REG or a MEM, based on
   the thing it is a subreg of.  Do it anyway if FINAL_P.  */
//原型 alter_subreg output.h final.cc
rtx mtcs_output_alter_subreg (MtcsOutput *self,rtx *xp, bool final_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   rtx x = *xp;
   rtx y = SUBREG_REG (x);

   /* simplify_subreg does not remove subreg from volatile references.
   We are required to.  */
   if (MEM_P (y)){
      poly_int64 offset = SUBREG_BYTE (x);

      /* For paradoxical subregs on big-endian machines, SUBREG_BYTE
      contains 0 instead of the proper offset.  See simplify_subreg.  */
      if (mtcs_rtl_paradoxical_subreg_p/*!paradoxical_subreg_p*/(mtcsRTL,x))
         offset = mtcs_mode_byte_lowpart_offset/*!byte_lowpart_offset*/(mtcsMode,GET_MODE (x), GET_MODE (y));

      if (final_p)
         *xp = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,y, GET_MODE (x), offset);
      else
         *xp = mtcs_rtl_adjust_address_nv/*!adjust_address_nv*/(mtcsRTL,y, GET_MODE (x), offset);
   }else if (REG_P (y) && mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,y)){
      rtx new_rtx = mtcs_simplify_rtx_subreg/*!simplify_subreg*/(mtcsSimplifyRtx,GET_MODE (x), y, GET_MODE (y),
      SUBREG_BYTE (x));

      if (new_rtx != 0)
         *xp = new_rtx;
      else if (final_p && REG_P (y)){
         /* Simplify_subreg can't handle some REG cases, but we have to.  */
         unsigned int regno;
         poly_int64 offset;

         regno = mtcs_rtlanal_subreg_regno/*!subreg_regno*/(mtcsRtlanal,x);
         if (mtcs_rtl_subreg_lowpart_p/*!subreg_lowpart_p*/(mtcsRTL,x))
            offset =mtcs_mode_byte_lowpart_offset/*!byte_lowpart_offset*/(mtcsMode,GET_MODE (x), GET_MODE (y));
         else
            offset = SUBREG_BYTE (x);
         *xp = gen_rtx_REG_offset (y, GET_MODE (x), regno, offset);
      }
   }

   return *xp;
}

/* Do alter_subreg on all the SUBREGs contained in X.  */
static rtx walk_alter_subreg (MtcsOutput *self,rtx *xp, bool *changed)
{
   rtx x = *xp;
   switch (GET_CODE (x)){
      case PLUS:
      case MULT:
      case AND:
         XEXP (x, 0) = walk_alter_subreg(self,&XEXP (x, 0), changed);
         XEXP (x, 1) = walk_alter_subreg(self,&XEXP (x, 1), changed);
         break;

      case MEM:
      case ZERO_EXTEND:
         XEXP (x, 0) = walk_alter_subreg(self,&XEXP (x, 0), changed);
         break;

      case SUBREG:
         *changed = true;
         return mtcs_output_alter_subreg/*!alter_subreg*/(self,xp, true);

      default:
         break;
   }

   return *xp;
}

/* For each operand in INSN, simplify (subreg (reg)) so that it refers
   directly to the desired hard register.  */
//原型 cleanup_subreg_operands reload.h final.cc
void mtcs_output_cleanup_subreg_operands (MtcsOutput *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);

   int i;
   bool changed = false;
   mtcs_recog_extract_insn_cached/*!extract_insn_cached*/(mtcsRecog,insn);
   for (i = 0; i < mtcsRecog->recog_data.n_operands; i++){
      /* The following test cannot use recog_data.operand when testing
      for a SUBREG: the underlying object might have been changed
      already if we are inside a match_operator expression that
      matches the else clause.  Instead we test the underlying
      expression directly.  */
      if (GET_CODE (*mtcsRecog->recog_data.operand_loc[i]) == SUBREG){
         mtcsRecog->recog_data.operand[i] = mtcs_output_alter_subreg/*!alter_subreg*/(self,mtcsRecog->recog_data.operand_loc[i], true);
         changed = true;
      }else if (GET_CODE (mtcsRecog->recog_data.operand[i]) == PLUS
      || GET_CODE (mtcsRecog->recog_data.operand[i]) == MULT
      || MEM_P (mtcsRecog->recog_data.operand[i]))
         mtcsRecog->recog_data.operand[i] = walk_alter_subreg (self,mtcsRecog->recog_data.operand_loc[i], &changed);
   }

   for (i = 0; i < mtcsRecog->recog_data.n_dups; i++){
      if (GET_CODE (*mtcsRecog->recog_data.dup_loc[i]) == SUBREG){
         *mtcsRecog->recog_data.dup_loc[i] = mtcs_output_alter_subreg/*!alter_subreg*/(self,mtcsRecog->recog_data.dup_loc[i], true);
         changed = true;
      }else if (GET_CODE (*mtcsRecog->recog_data.dup_loc[i]) == PLUS
      || GET_CODE (*mtcsRecog->recog_data.dup_loc[i]) == MULT
      || MEM_P (*mtcsRecog->recog_data.dup_loc[i]))
         *mtcsRecog->recog_data.dup_loc[i] = walk_alter_subreg (self,mtcsRecog->recog_data.dup_loc[i], &changed);
   }
   if (changed)
      mtcs_dfscan_df_insn_rescan/*!df_insn_rescan*/(mtcsDfscan,insn);
}

/* Marks SYMBOL_REFs in x as referenced through use of assemble_external.  */
//原型 mark_symbol_refs_as_used output.h final.cc
void mtcs_output_mark_symbol_refs_as_used (MtcsOutput *self,rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm  *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

   subrtx_iterator::array_type array;
   FOR_EACH_SUBRTX (iter, array, x, ALL){
      const_rtx x = *iter;
      if (GET_CODE (x) == SYMBOL_REF)
         if (tree t = SYMBOL_REF_DECL (x))
            mtcs_asm_assemble_external/*!assemble_external*/(mtcsAsm,t);
   }
}


/* Print operand X using machine-dependent assembler syntax.
   CODE is a non-digit that preceded the operand-number in the % spec,
   such as 'z' if the spec was `%z3'.  CODE is 0 if there was no char
   between the % and the digits.
   When CODE is a non-letter, X is 0.

   The meanings of the letters are machine-dependent and controlled
   by TARGET_PRINT_OPERAND.  */
//原型 output_operand output.h final.cc
void mtcs_output_output_operand (MtcsOutput *self,rtx x, int code ATTRIBUTE_UNUSED)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsAsm  *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   int   firstPseudoRegister= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);

   if (x && GET_CODE (x) == SUBREG)
      x = alter_subreg (&x, true);

   /* X must not be a pseudo reg.  */
   if (!mtcsTarget/*!targetm.no_register_allocation*/->no_register_allocation)
      gcc_assert (!x || !REG_P (x) || REGNO (x) < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/);

   target_asm_out_print_operand/*!targetm.asm_out.print_operand*/(mtcsMachine->asmOut,x, code);

   if (x == NULL_RTX)
      return;

   mtcs_output_mark_symbol_refs_as_used/*!mark_symbol_refs_as_used*/(self,x);
}

/* Return the assembler directive for creating a given kind of integer
   object.  SIZE is the number of bytes in the object and ALIGNED_P
   indicates whether it is known to be aligned.  Return NULL if the
   assembly dialect has no such directive.

   The returned string should be printed at the start of a new line and
   be followed immediately by the object's initial value.  */
//原型 integer_asm_op output.h varasm.cc
const char *mtcs_output_integer_asm_op (MtcsOutput *self,int size, int aligned_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsAsm  *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   struct asm_int_op *ops;

   if (aligned_p)
      ops = &mtcsMachine->asmOut->aligned_op/*!targetm.asm_out.aligned_op*/;
   else
      ops = &mtcsMachine->asmOut->unaligned_op/*!targetm.asm_out.unaligned_op*/;

   switch (size){
      case 1:
         return mtcsMachine->asmOut->byte_op/*!targetm.asm_out.byte_op*/;
      case 2:
         return ops->hi;
      case 3:
         return ops->psi;
      case 4:
         return ops->si;
      case 5:
      case 6:
      case 7:
         return ops->pdi;
      case 8:
         return ops->di;
      case 9:
      case 10:
      case 11:
      case 12:
      case 13:
      case 14:
      case 15:
         return ops->pti;
      case 16:
         return ops->ti;
      default:
         return NULL;
   }
}

/* Return true if it is possible to put DECL in an object_block.  */
static bool useBlocksForDeclP (MtcsOutput *self,tree decl)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  struct symtab_node *snode;
  /* Don't create object blocks if each DECL is placed into a separate
     section because that will uselessly create a section anchor for
     each DECL.  */
  if (flag_data_sections)
    return false;
  /* Only data DECLs can be placed into object blocks.  */
  if (!VAR_P (decl) && TREE_CODE (decl) != CONST_DECL)
    return false;
  /* DECL_INITIAL (decl) set to decl is a hack used for some decls that
     are never used from code directly and we never want object block handling
     for those.  */
  if (DECL_INITIAL (decl) == decl)
    return false;
  /* If this decl is an alias, then we don't want to emit a
     definition.  */
  if (VAR_P (decl)  && (snode = symtab_node::get (decl)) != NULL  && snode->alias)
    return false;

  return mtcsTarget->use_blocks_for_decl_p (mtcsTarget,decl);
}

/* Return true if the current compilation mode benefits from having
   objects grouped into blocks.  */
static bool use_object_blocks_p (MtcsOutput *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   return mtcsOptionsItem->x_flag_section_anchors;
}

//enum section_category在output.h中已定义，声明在mtcsasm.h中冲突。改为静态方法
//原型 categorize_decl_for_section output.h varasm.cc
enum section_category mtcs_output_categorize_decl_for_section (MtcsOutput *self,const_tree decl, int reloc)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   enum section_category ret;
   if (TREE_CODE (decl) == FUNCTION_DECL)
      return SECCAT_TEXT;
   else if (TREE_CODE (decl) == STRING_CST){
      if ((flag_sanitize & SANITIZE_ADDRESS) && asan_protect_global (CONST_CAST_TREE (decl)))
         /* or !flag_merge_constants */
         return SECCAT_RODATA;
      else
         return SECCAT_RODATA_MERGE_STR;
   }else if (VAR_P (decl)){
      tree d = CONST_CAST_TREE (decl);
      if (bss_initializer_p (decl))
         ret = SECCAT_BSS;
      else if (! TREE_READONLY (decl) || (DECL_INITIAL (decl) && ! TREE_CONSTANT (DECL_INITIAL (decl)))){
         /* Here the reloc_rw_mask is not testing whether the section should
         be read-only or not, but whether the dynamic link will have to
         do something.  If so, we wish to segregate the data in order to
         minimize cache misses inside the dynamic linker.  */
         if (reloc & target_asm_out_reloc_rw_mask/*!targetm.asm_out.reloc_rw_mask*/(mtcsMachine->asmOut))
            ret = reloc == 1 ? SECCAT_DATA_REL_LOCAL : SECCAT_DATA_REL;
         else
            ret = SECCAT_DATA;
      }else if (reloc & target_asm_out_reloc_rw_mask/*!targetm.asm_out.reloc_rw_mask*/(mtcsMachine->asmOut))
         ret = reloc == 1 ? SECCAT_DATA_REL_RO_LOCAL : SECCAT_DATA_REL_RO;
      else if (reloc || (flag_merge_constants < 2 && !DECL_MERGEABLE (decl)) || ((flag_sanitize & SANITIZE_ADDRESS)
      /* PR 81697: for architectures that use section anchors we
      need to ignore DECL_RTL_SET_P (decl) for string constants
      inside this asan_protect_global call because otherwise
      we'll wrongly put them into SECCAT_RODATA_MERGE_CONST
      section, set DECL_RTL (decl) later on and add DECL to
      protected globals via successive asan_protect_global
      calls.  In this scenario we'll end up with wrong
      alignment of these strings at runtime and possible ASan
      false positives.  */
      && asan_protect_global (d, use_object_blocks_p(self) && useBlocksForDeclP (self,d))))
         /* C and C++ don't allow different variables to share the same
         location.  -fmerge-all-constants allows even that (at the
         expense of not conforming).  */
         ret = SECCAT_RODATA;
      else if (DECL_INITIAL (decl) && TREE_CODE (DECL_INITIAL (decl)) == STRING_CST)
         ret = SECCAT_RODATA_MERGE_STR_INIT;
      else
         ret = SECCAT_RODATA_MERGE_CONST;
   }else if (TREE_CODE (decl) == CONSTRUCTOR){
      if ((reloc & target_asm_out_reloc_rw_mask/*!targetm.asm_out.reloc_rw_mask*/(mtcsMachine->asmOut))
      || ! TREE_CONSTANT (decl))
         ret = SECCAT_DATA;
      else
         ret = SECCAT_RODATA;
   }else
      ret = SECCAT_RODATA;

   /* There are no read-only thread-local sections.  */
   if (VAR_P (decl) && DECL_THREAD_LOCAL_P (decl)){
      /* Note that this would be *just* SECCAT_BSS, except that there's
      no concept of a read-only thread-local-data section.  */
      if (ret == SECCAT_BSS || DECL_INITIAL (decl) == NULL || (flag_zero_initialized_in_bss && initializer_zerop (DECL_INITIAL (decl))))
         ret = SECCAT_TBSS;
      else
         ret = SECCAT_TDATA;
   }
   /* If the target uses small data sections, select it.  */
   else if (mtcsTarget->in_small_data_p (mtcsTarget,decl)){
      if (ret == SECCAT_BSS)
         ret = SECCAT_SBSS;
      else if (mtcsTarget->have_srodata_section && ret == SECCAT_RODATA)
         ret = SECCAT_SRODATA;
      else
         ret = SECCAT_SDATA;
   }
   return ret;
}

static bool decl_readonly_section_1 (enum section_category category)
{
   switch (category){
      case SECCAT_RODATA:
      case SECCAT_RODATA_MERGE_STR:
      case SECCAT_RODATA_MERGE_STR_INIT:
      case SECCAT_RODATA_MERGE_CONST:
      case SECCAT_SRODATA:
         return true;
      default:
         return false;
   }
}

//原型 default_section_type_flags output.h varasm.cc
unsigned int mtcs_output_default_section_type_flags (MtcsOutput *self,tree decl, const char *name, int reloc)
{
   unsigned int flags;
   if (decl && TREE_CODE (decl) == FUNCTION_DECL)
      flags = SECTION_CODE;
   else if (decl){
      enum section_category category = mtcs_output_categorize_decl_for_section/*categorize_decl_for_section*/(self,decl, reloc);
      if (decl_readonly_section_1 (category))
         flags = 0;
      else if (category == SECCAT_DATA_REL_RO  || category == SECCAT_DATA_REL_RO_LOCAL)
         flags = SECTION_WRITE | SECTION_RELRO;
      else
         flags = SECTION_WRITE;
   }else{
      flags = SECTION_WRITE;
      if (strcmp (name, ".data.rel.ro") == 0|| strcmp (name, ".data.rel.ro.local") == 0)
         flags |= SECTION_RELRO;
   }

   if (decl && DECL_P (decl) && DECL_COMDAT_GROUP (decl))
      flags |= SECTION_LINKONCE;

   if (strcmp (name, ".vtable_map_vars") == 0)
      flags |= SECTION_LINKONCE;

   if (decl && VAR_P (decl) && DECL_THREAD_LOCAL_P (decl))
      flags |= SECTION_TLS | SECTION_WRITE;

   if (strcmp (name, ".bss") == 0
   || startswith (name, ".bss.")
   || startswith (name, ".gnu.linkonce.b.")
   || strcmp (name, ".persistent.bss") == 0
   || strcmp (name, ".sbss") == 0
   || startswith (name, ".sbss.")
   || startswith (name, ".gnu.linkonce.sb."))
      flags |= SECTION_BSS;

   if (strcmp (name, ".tdata") == 0
   || startswith (name, ".tdata.")
   || startswith (name, ".gnu.linkonce.td."))
      flags |= SECTION_TLS;

   if (strcmp (name, ".tbss") == 0
   || startswith (name, ".tbss.")
   || startswith (name, ".gnu.linkonce.tb."))
      flags |= SECTION_TLS | SECTION_BSS;

   if (strcmp (name, ".noinit") == 0)
      flags |= SECTION_WRITE | SECTION_BSS | SECTION_NOTYPE;

   if (strcmp (name, ".persistent") == 0)
      flags |= SECTION_WRITE | SECTION_NOTYPE;

   /* Various sections have special ELF types that the assembler will
   assign by default based on the name.  They are neither SHT_PROGBITS
   nor SHT_NOBITS, so when changing sections we don't want to print a
   section type (@progbits or @nobits).  Rather than duplicating the
   assembler's knowledge of what those special name patterns are, just
   let the assembler choose the type if we don't know a specific
   reason to set it to something other than the default.  SHT_PROGBITS
   is the default for sections whose name is not specially known to
   the assembler, so it does no harm to leave the choice to the
   assembler when @progbits is the best thing we know to use.  If
   someone is silly enough to emit code or TLS variables to one of
   these sections, then don't handle them specially.

   default_elf_asm_named_section (below) handles the BSS, TLS, ENTSIZE, and
   LINKONCE cases when NOTYPE is not set, so leave those to its logic.  */
   if (!(flags & (SECTION_CODE | SECTION_BSS | SECTION_TLS | SECTION_ENTSIZE))
   && !(HAVE_COMDAT_GROUP && (flags & SECTION_LINKONCE)))
      flags |= SECTION_NOTYPE;

   return flags;
}

//原型 targetm.section_type_flags  #define TARGET_SECTION_TYPE_FLAGS default_section_type_flags
unsigned int mtcs_output_section_type_flags (MtcsOutput *self,tree decl, const char *name, int reloc)
{
   return self->section_type_flags(self,decl,name,reloc);
}



//原型 default_encode_section_info output.h varasm.cc
void mtcs_output_default_encode_section_info(MtcsOutput *self,tree decl, rtx rtl, int first ATTRIBUTE_UNUSED)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   rtx symbol;
   int flags;

   /* Careful not to prod global register variables.  */
   if (!MEM_P (rtl)){
      n_debug("mtcsoutput.c default_encode_section_info 00 !MEM_P (rtl)\n");
      return;
   }
   symbol = XEXP (rtl, 0);
   if (GET_CODE (symbol) != SYMBOL_REF){
      n_debug("mtcsoutput.c default_encode_section_info 11 GET_CODE (symbol) != SYMBOL_REF\n");
      return;
   }

   flags = SYMBOL_REF_FLAGS (symbol) & SYMBOL_FLAG_HAS_BLOCK_INFO;
   if (TREE_CODE (decl) == FUNCTION_DECL){
      n_debug("mtcsoutput.c default_encode_section_info 22 TREE_CODE (decl) == FUNCTION_DECL\n");
      flags |= SYMBOL_FLAG_FUNCTION;
   }
   if (mtcsTarget->binds_local_p/*!targetm.binds_local_p*/(mtcsTarget,decl)){
      n_debug("mtcsoutput.c default_encode_section_info 33 targetm.binds_local_p (decl)\n");
      flags |= SYMBOL_FLAG_LOCAL;
   }
   if (VAR_P (decl) && DECL_THREAD_LOCAL_P (decl)){
      n_debug("mtcsoutput.c default_encode_section_info 44 VAR_P (decl) && DECL_THREAD_LOCAL_P (decl)\n");
      flags |= DECL_TLS_MODEL (decl) << SYMBOL_FLAG_TLS_SHIFT;
   }else if (mtcsTarget->in_small_data_p/*!targetm.in_small_data_p*/(mtcsTarget,decl)){
      n_debug("mtcsoutput.c default_encode_section_info 55 targetm.in_small_data_p (decl)\n");
      flags |= SYMBOL_FLAG_SMALL;
   }
   /* ??? Why is DECL_EXTERNAL ever set for non-PUBLIC names?  Without
   being PUBLIC, the thing *must* be defined in this translation unit.
   Prevent this buglet from being propagated into rtl code as well.  */
   if (DECL_P (decl) && DECL_EXTERNAL (decl) && TREE_PUBLIC (decl)){
      n_debug("mtcsoutput.c default_encode_section_info 66 DECL_P (decl) && DECL_EXTERNAL (decl) && TREE_PUBLIC (decl)\n");
      flags |= SYMBOL_FLAG_EXTERNAL;
   }

   SYMBOL_REF_FLAGS (symbol) = flags;
}

//替换 targetm.encode_section_info nvptx有实现
//原型  targetm.encode_section_info (exp, rtl, true); #define TARGET_ENCODE_SECTION_INFO default_encode_section_info
void mtcs_output_encode_section_info (MtcsOutput *self,tree decl, rtx rtl, int first ATTRIBUTE_UNUSED)
{
    self->encode_section_info(self,decl,rtl,first);
}

void mtcs_output_debug_file(MtcsOutput *self,int emitted_number,char *fileName)
{
    self->output_debug_file(self,emitted_number,fileName);
}
