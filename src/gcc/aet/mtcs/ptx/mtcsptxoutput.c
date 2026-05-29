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
 */

#define IN_TARGET_CODE 1
#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "predict.h"
#include "tree.h"
#include "rtl.h"
#include "flags.h"
#include "alias.h"
#include "varasm.h"
#include "stor-layout.h"
#include "calls.h"
#include "insn-config.h"
#include "expmed.h"
#include "dojump.h"
#include "explow.h"
#include "memmodel.h"
#include "emit-rtl.h"
#include "stmt.h"
#include "expr.h"
#include "insn-codes.h"
#include "tm_p.h"
#include "regs.h"
#include "conditions.h"
#include "insn-attr.h"
#include "recog.h"
#include "diagnostic-core.h"
#include "output.h"
#include "target.h"
#include "stringpool.h"
#include "attribs.h"

#include "aet/aetprinttree.h"
#include "../mtcstarget.h"
#include "ptx-common.h"
#include "mtcsptxoutput.h"
#include "mtcsptxmode.h"
#include "ptxtool.h"
#include "mtcsptx.h"
#include "../mtcsasm.h"
#include "mtcsptxfunc.h"
#include "../mtcscompile.h"
#include "gen/ptx-insn-modes.h"
#include "../mtcsprintrtl.h"
#include "../../mtcsinfo.h"

static const char *getInsnName_cb(MtcsOutput *mtcsOutput,int code);
//原型  targetm.encode_section_info (exp, rtl, true); #define TARGET_ENCODE_SECTION_INFO default_encode_section_info
//原型 mtcsoutput.h void (*encode_section_info) (MtcsOutput *self,tree decl, rtx rtl, int first ATTRIBUTE_UNUSED);
static void encodeSectionInfo_cb(MtcsOutput *mtcsOutput,tree decl, rtx rtl, int first ATTRIBUTE_UNUSED);
static void outputDebugFile_cb(MtcsOutput *self,int emitted_number,char *fileName);

static void mtcsPtxInsnOutputInit(MtcsPtxOutput *self)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsOutput *mtcsOutput=(MtcsOutput *)self;
    mtcsOutput->get_insn_name=getInsnName_cb;//原型 get_insn_name rtl.h insn-output.cc
    //替换 targetm.encode_section_info nvptx有实现
    //原型  targetm.encode_section_info (exp, rtl, true); #define TARGET_ENCODE_SECTION_INFO default_encode_section_info
    mtcsOutput->encode_section_info = encodeSectionInfo_cb;
    //解决bug 082
    mtcsOutput->output_debug_file=outputDebugFile_cb;

    mtcs_ptx_output_set_target(self,(void *)mtcsMode->target);//该方法在ptx-insns-output.c中实现
    mtcs_output_set_insn_data(mtcsOutput,ptx_insn_data,0);
}

/* Output a pre/post barrier for MEM_OPERAND according to MEMMODEL.  */
static void nvptx_output_barrier (MtcsPtxOutput *self,rtx *mem_operand, int memmodel, bool pre_p)
{
  bool post_p = !pre_p;

  switch (memmodel)
    {
    case MEMMODEL_RELAXED:
      return;
    case MEMMODEL_CONSUME:
    case MEMMODEL_ACQUIRE:
    case MEMMODEL_SYNC_ACQUIRE:
      if (post_p)
          break;
      return;
    case MEMMODEL_RELEASE:
    case MEMMODEL_SYNC_RELEASE:
      if (pre_p)
    break;
      return;
    case MEMMODEL_ACQ_REL:
    case MEMMODEL_SEQ_CST:
    case MEMMODEL_SYNC_SEQ_CST:
      if (pre_p || post_p)
    break;
      return;
    default:
      gcc_unreachable ();
    }

  mtcs_output_asm_insn/*output_asm_insn*/ ((MtcsOutput *)self,"%.\tmembar%B0;", mem_operand);
}

/* Output code for switching uniform-simt state.  ENTERING indicates whether
   we are entering or leaving non-uniform execution region.  */

static void nvptx_output_unisimt_switch (MtcsPtxOutput *self, bool entering)
{
    MtcsOutput *mtcsOutput=(MtcsOutput *)self;
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsPtx *mtcsPtx=(MtcsPtx *)mtcsTarget;
    MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
    MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
    MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
    MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
    FILE *file=mtcsAsm->asmFile;
    //struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)mtcsFunc->machine;
    struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)cfun->machine;

    n_debug("mtcsptxoutput.c -----nvptx.cc -----28-- void nvptx_output_unisimt_switch (FILE *file, bool entering)\n");

  if (mtcsRtlData/*!crtl*/->is_leaf && !nvptxMachine->unisimt_predicate)
    return;
  fprintf (file, "\t{\n");
  fprintf (file, "\t\t.reg.u32 %%ustmp2;\n");
  fprintf (file, "\t\tmov.u32 %%ustmp2, %d;\n", entering ? -1 : 0);
  if (nvptxMachine/*!cfun->machine*/->unisimt_outside_simt_predicate){
      int pred_outside_simt = REGNO (nvptxMachine->unisimt_outside_simt_predicate);
      fprintf (file, "\t\tmov.pred %%r%d, %d;\n", pred_outside_simt,entering ? 0 : 1);
  }
  if (!mtcsRtlData/*!crtl*/->is_leaf){
      int loc = REGNO (nvptxMachine->unisimt_location);
      fprintf (file, "\t\tst.shared.u32 [%%r%d], %%ustmp2;\n", loc);
  }
  if (nvptxMachine/*!cfun->machine*/->unisimt_predicate) {
      int master = REGNO (nvptxMachine/*!cfun->machine*/->unisimt_master);
      int pred = REGNO (nvptxMachine/*!cfun->machine*/->unisimt_predicate);
      fprintf (file, "\t\tmov.u32 %%ustmp2, %%laneid;\n");
      fprintf (file, "\t\tmov.u32 %%r%d, %s;\n",
           master, entering ? "%ustmp2" : "0");
      fprintf (file, "\t\tsetp.eq.u32 %%r%d, %%r%d, %%ustmp2;\n", pred, master);
  }
  fprintf (file, "\t}\n");
}

/* Output code for allocating per-lane storage and switching soft-stack pointer.
   ENTERING indicates whether we are entering or leaving non-uniform execution.
   PTR is the register pointing to allocated storage, it is assigned to on
   entering and used to restore state on leaving.  SIZE and ALIGN are used only
   on entering.  */

static void nvptx_output_softstack_switch (MtcsPtxOutput *self,bool entering,rtx ptr, rtx size, rtx align)
{
    MtcsOutput *mtcsOutput=(MtcsOutput *)self;
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsPtx *mtcsPtx=(MtcsPtx *)mtcsTarget;
    MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
    MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
    MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
    MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
    MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
    FILE *file=mtcsAsm->asmFile;
    //struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)mtcsFunc->machine;
    struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)cfun->machine;

    n_debug("mtcsptxoutput.c -----nvptx.cc -----29-- void nvptx_output_softstack_switch (FILE *file, bool entering,rtx ptr, rtx size, rtx align)\n");

  gcc_assert (REG_P (ptr) && !mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,ptr));
  if (mtcsRtlData/*!crtl*/->is_leaf && !nvptxMachine->simt_stack_size)
    return;
  int bits = POINTER_SIZE, regno = REGNO (ptr);
  fprintf (file, "\t{\n");
  if (entering){
      fprintf (file, "\t\tcvta.local.u%d %%r%d, %%simtstack_ar + "
           HOST_WIDE_INT_PRINT_DEC ";\n", bits, regno,nvptxMachine->simt_stack_size);
      fprintf (file, "\t\tsub.u%d %%r%d, %%r%d, ", bits, regno, regno);
      if (CONST_INT_P (size))
          fprintf (file, HOST_WIDE_INT_PRINT_DEC,ROUND_UP (UINTVAL (size), mtcs_mode_get_size(mtcsMode,mtcsMode->modes.M_DImode)));
      else
          mtcs_ptx_output_output_reg (self, REGNO (size), VOIDmode);
      fputs (";\n", file);
      if (!CONST_INT_P (size) || UINTVAL (align) > mtcs_mode_get_size(mtcsMode,mtcsMode->modes.M_DImode))
          fprintf (file, "\t\tand.b%d %%r%d, %%r%d, -" HOST_WIDE_INT_PRINT_DEC ";\n",bits, regno, regno, UINTVAL (align));
  }
  if (nvptxMachine->has_softstack){
      const char *reg_stack = mtcs_reg_get_reg_name(mtcsReg,PTX_STACK_POINTER_REGNUM)/*reg_names[STACK_POINTER_REGNUM]*/;
      if (entering){
          fprintf (file, "\t\tst.u%d [%%r%d + -%d], %s;\n",bits, regno, bits / 8, reg_stack);
          fprintf (file, "\t\tsub.u%d %s, %%r%d, %d;\n",bits, reg_stack, regno, bits / 8);
      }else{
          fprintf (file, "\t\tld.u%d %s, [%%r%d + -%d];\n",bits, reg_stack, regno, bits / 8);
      }
      mtcs_ptx_output_set_softstack (self,REGNO (mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL)));
  }
  fprintf (file, "\t}\n");
}


////////////////////////////////////----------output---------------------
/* Output a pattern for a move instruction.  */
/* SYM is a SYMBOL_REF.  If it refers to an external function, record
   it as needed.  */
const char *mtcs_ptx_output_mov_insn (MtcsPtxOutput *self,rtx dst, rtx src)
{
  MtcsOutput *mtcsOutput=(MtcsOutput *)self;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsPtxFunc *mtcsPtxFunc=(MtcsPtxFunc *)mtcs_target_get_func(mtcsTarget);

  MtcsPtx *mtcsPtx=(MtcsPtx *)mtcsTarget;
  machine_mode dst_mode = GET_MODE (dst);
  machine_mode src_mode = GET_MODE (src);
  machine_mode dst_inner = (GET_CODE (dst) == SUBREG
                ? GET_MODE (XEXP (dst, 0)) : dst_mode);
  machine_mode src_inner = (GET_CODE (src) == SUBREG
                ? GET_MODE (XEXP (src, 0)) : dst_mode);
  n_debug("mtcsptxoutput.c mtcs_ptx_output_mov_insn 00\n");
  rtx sym = src;
  if (GET_CODE (sym) == CONST)
    sym = XEXP (XEXP (sym, 0), 0);
  if (SYMBOL_REF_P (sym)){
      if (PTX_SYMBOL_DATA_AREA (sym) != PTX_DATA_AREA_GENERIC)
          return "%.\tcvta%D1%t0\t%0, %1;";
      mtcs_ptx_func_maybe_record_fnsym/*!nvptx_maybe_record_fnsym*/(mtcsPtxFunc,sym);
  }
  n_debug("mtcsptxoutput.c mtcs_ptx_output_mov_insn 11 dst_mode:%d src_mode:%d\n",dst_mode,src_mode);

  if (src_inner == dst_inner)
    return "%.\tmov%t0\t%0, %1;";

  if (CONSTANT_P (src))
    return (mtcs_mode_get_class(mtcsMode,dst_inner) == MODE_INT
        && mtcs_mode_get_class(mtcsMode,src_inner) != MODE_FLOAT
        ? "%.\tmov%t0\t%0, %1;" : "%.\tmov.b%T0\t%0, %1;");

  if (mtcs_mode_get_size(mtcsMode,dst_inner) == mtcs_mode_get_size (mtcsMode,src_inner)){
      if (mtcs_mode_get_bitsize (mtcsMode,dst_mode) == 128  && mtcs_mode_get_bitsize (mtcsMode,src_mode) == 128){
         n_debug("mtcsptxoutput.c mtcs_ptx_output_mov_insn 22 dst_mode:%d src_mode:%d\n",dst_mode,src_mode);

          /* mov.b128 is not supported.  */
          if (dst_inner == PTX_V2DImode && src_inner == PTX_TImode)
            return "%.\tmov.u64\t%0.x, %L1;\n\t%.\tmov.u64\t%0.y, %H1;";
          else if (dst_inner == PTX_TImode && src_inner == PTX_V2DImode)
            return "%.\tmov.u64\t%L0, %1.x;\n\t%.\tmov.u64\t%H0, %1.y;";

          gcc_unreachable ();
      }
      return "%.\tmov.b%T0\t%0, %1;";
  }
  n_debug("mtcsptxoutput.c mtcs_ptx_output_mov_insn 33 dst_mode:%d src_mode:%d\n",dst_mode,src_mode);

  if (mtcs_mode_get_bitsize (mtcsMode,src_inner) == 128 && mtcs_mode_get_bitsize (mtcsMode,src_mode) == 64)
    return "%.\tmov.b%T0\t%0, %1;";

  return "%.\tcvt%t0%t1\t%0, %1;";
}

const char *mtcs_ptx_output_return (MtcsPtxOutput *self)
{
  MtcsOutput *mtcsOutput=(MtcsOutput *)self;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsPtx *mtcsPtx=(MtcsPtx *)mtcsTarget;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  FILE *asmFile=mtcsAsm->asmFile;
  //struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)mtcsFunc->machine;
  struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)cfun->machine;

  machine_mode mode = nvptxMachine->return_mode;
  n_debug("mtcsptxoutput.c -----nvptx.cc -----33-- const char * nvptx_output_return (void)\n");
  if (mode != PTX_VOIDmode)
    fprintf (asmFile, "\tst.param%s\t[%s_out], %s;\n",
          mtcs_mode_get_type (mtcsMode,mode, false),
         mtcsReg->hardRegs.x_reg_names/*!reg_names*/[PTX_NVPTX_RETURN_REGNUM],
         mtcsReg->hardRegs.x_reg_names/*!reg_names*/[PTX_NVPTX_RETURN_REGNUM]);

  return "ret;";
}


/* Output INSN, which is a call to CALLEE with result RESULT.  For ptx, this
   involves writing .param declarations and in/out copies into them.  For
   indirect calls, also write the .callprototype.  */

const char *mtcs_ptx_output_call_insn (MtcsPtxOutput *self,rtx_insn *insn, rtx result, rtx callee)
{
   MtcsOutput *mtcsOutput=(MtcsOutput *)self;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsPtx *mtcsPtx=(MtcsPtx *)mtcsTarget;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsPtxFunc *mtcsPtxFunc=(MtcsPtxFunc *)mtcsFunc;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);

   FILE *asmFile=mtcsAsm->asmFile;
   struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)cfun->machine;
   n_debug("mtcsptxoutput.c -----nvptx.cc -----173-- nvptx_output_call_insn asmFile:%p\n",asmFile);
   char buf[16];
   static int labelno;
   bool needs_tgt = mtcs_preds_register_operand/*!register_operand*/(mtcsPreds,callee, mtcs_mode_get_Pmode(mtcsMode));
   n_debug("mtcsptxoutput.c -----nvptx.cc -----173aa-- needs_tgt:%d\n",needs_tgt);

   rtx pat = PATTERN (insn);
   if (GET_CODE (pat) == COND_EXEC)
      pat = COND_EXEC_CODE (pat);
   int arg_end = XVECLEN (pat, 0);
   tree decl = NULL_TREE;
   n_debug("mtcsptxoutput.c -----nvptx.cc -----173bb-- arg_end:%d\n",arg_end);

   fprintf (asmFile, "\t{\n");
   if (result != NULL){
      n_debug("mtcsptxoutput.c -----nvptx.cc -----173cc-- %s %s\n",mtcs_mode_get_type (mtcsMode,GET_MODE (result), false)
            ,mtcs_reg_get_reg_name(mtcsReg,PTX_NVPTX_RETURN_REGNUM));
      fprintf (asmFile, "\t\t.param%s %s_in;\n",mtcs_mode_get_type (mtcsMode,GET_MODE (result), false),
            mtcs_reg_get_reg_name(mtcsReg,PTX_NVPTX_RETURN_REGNUM)/*!reg_names[NVPTX_RETURN_REGNUM]*/);
   }
   /* Ensure we have a ptx declaration in the output if necessary.  */
   if (GET_CODE (callee) == SYMBOL_REF){
      decl = SYMBOL_REF_DECL (callee);
      if (!decl|| (DECL_EXTERNAL (decl) && !TYPE_ARG_TYPES (TREE_TYPE (decl)))){
         n_debug("mtcsptxoutput.c -----nvptx.cc -----173dd--GET_CODE (callee) == SYMBOL_REF\n");
         mtcs_ptx_func_write_record_libfunc/*!nvptx_record_libfunc*/ (mtcsPtxFunc,mtcsPtx->func_decls,callee, result, pat);
      }else if (DECL_EXTERNAL (decl)){
         n_debug("mtcsptxoutput.c -----nvptx.cc -----173ee--GET_CODE (callee) == SYMBOL_REF\n");
         mtcs_ptx_func_record_fndecl/*!nvptx_record_fndecl*/(mtcsPtxFunc,decl);
      }
   }

   if (needs_tgt){
      mtcs_asm_generate_internal_label/*!ASM_GENERATE_INTERNAL_LABEL*/(mtcsAsm,buf, "LCT", labelno);

      labelno++;
      //ASM_OUTPUT_LABEL (asm_out_file, buf);
      mtcsAsm->output_label/*ASM_OUTPUT_LABEL (file,*/(mtcsAsm, buf);
      NString *strs=n_string_new("");
      // std::stringstream s;
      mtcs_ptx_func_write_fn_proto_from_insn (mtcsPtxFunc, strs,NULL, result, pat);
      //fputs (s.str().c_str(), asm_out_file);
      fputs (strs->str,mtcsAsm->asmFile);
      n_string_free(strs,TRUE);
   }

   for (int argno = 1; argno < arg_end; argno++){
      rtx t = XEXP (XVECEXP (pat, 0, argno), 0);
      machine_mode mode = GET_MODE (t);
      const char *ptx_type = mtcs_mode_get_type/*!nvptx_ptx_type_from_mode*/ (mtcsMode,mode, false);
      n_debug("mtcsptxoutput.c -----nvptx.cc -----173ff--argno:%d mode:%d ptx_type:%s REGNO (t):%d\n",argno,mode,ptx_type,REGNO (t));
      /* Mode splitting has already been done.  */
      fprintf (mtcsAsm->asmFile, "\t\t.param%s %%out_arg%d;\n"
         "\t\tst.param%s [%%out_arg%d], ",
         ptx_type, argno, ptx_type, argno);
      mtcs_ptx_output_output_reg (self, REGNO (t), VOIDmode);
      fprintf (mtcsAsm->asmFile, ";\n");
   }
   /* The '.' stands for the call's predicate, if any.  */
   mtcs_ptx_output_print_operand (self, NULL_RTX, '.');
   fprintf (mtcsAsm->asmFile, "\t\tcall ");
   if (result != NULL_RTX)
      fprintf (mtcsAsm->asmFile, "(%s_in), ", mtcs_reg_get_reg_name(mtcsReg,PTX_NVPTX_RETURN_REGNUM)/*!reg_names[NVPTX_RETURN_REGNUM]*/);

   if (decl){
      char *replaced_dots = NULL;
      const char *name = get_fnname_from_decl (decl);
      const char *replacement = mtcs_ptx_get_replace_function_name(mtcsPtx,name);
      if (replacement != name)
         name = replacement;
      else{
         replaced_dots = ptx_tool_replace_dot (name);
         if (replaced_dots)
            name = replaced_dots;
      }
      mtcs_asm_assemble_name/*!assemble_name*/(mtcsAsm, name);
      if (replaced_dots)
         XDELETE (replaced_dots);
   }else
      mtcs_output_address/*!output_address*/ (mtcsOutput,VOIDmode, callee);

   const char *open = "(";
   for (int argno = 1; argno < arg_end; argno++){
      fprintf (mtcsAsm->asmFile, ", %s%%out_arg%d", open, argno);
      open = "";
   }
   if (decl && DECL_STATIC_CHAIN (decl)){
      fprintf (mtcsAsm->asmFile, ", %s%s", open,  mtcs_reg_get_reg_name(mtcsReg,PTX_STATIC_CHAIN_REGNUM)/*!reg_names [STATIC_CHAIN_REGNUM]*/);
      open = "";
   }
   if (!open[0])
      fprintf (mtcsAsm->asmFile, ")");

   if (needs_tgt){
      fprintf (mtcsAsm->asmFile, ", ");
      mtcs_asm_assemble_name (mtcsAsm, buf);
   }
   fprintf (mtcsAsm->asmFile, ";\n");

   if (find_reg_note (insn, REG_NORETURN, NULL)){
      /* No return functions confuse the PTX JIT, as it doesn't realize
      the flow control barrier they imply.  It can seg fault if it
      encounters what looks like an unexitable loop.  Emit a trailing
      trap and exit, which it does grok.  */
      fprintf (mtcsAsm->asmFile, "\t\ttrap; // (noreturn)\n");
      fprintf (mtcsAsm->asmFile, "\t\texit; // (noreturn)\n");
   }

   if (result){
      static char rval[sizeof ("\tld.param%%t0\t%%0, [%%%s_in];\n\t}") + 8];
      if (!rval[0])
         /* We must escape the '%' that starts RETURN_REGNUM.  */
         sprintf (rval, "\tld.param%%t0\t%%0, [%%%s_in];\n\t}",
               mtcs_reg_get_reg_name(mtcsReg,PTX_NVPTX_RETURN_REGNUM)/*!reg_names[NVPTX_RETURN_REGNUM]*/);
      return rval;
   }
   return "}";
}

//原型 nvptx_output_fake_ptx_alloca nvptx-protos.h nvptx.cc gcc15 新加的
const char *mtcs_ptx_output_fake_ptx_alloca (MtcsPtxOutput *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsPtxFunc *mtcsPtxFunc=(MtcsPtxFunc *)mtcs_target_get_func(mtcsTarget);
   MtcsTree *mtcsTree = mtcs_target_get_tree(mtcsTarget);

#define FAKE_PTX_ALLOCA_NAME "__GCC_nvptx__PTX_alloca_not_supported"
   static tree decl;
   if (!decl){
      tree alloca_type = TREE_TYPE (builtin_decl_explicit (BUILT_IN_ALLOCA));
      decl = mtcs_tree_build_decl/*!build_decl*/(mtcsTree,
            UNKNOWN_LOCATION, FUNCTION_DECL,get_identifier (FAKE_PTX_ALLOCA_NAME), alloca_type);
      DECL_EXTERNAL (decl) = 1;
      TREE_PUBLIC (decl) = 1;
      mtcs_ptx_func_record_needed_fndecl/*!nvptx_record_needed_fndecl*/(mtcsPtxFunc,decl);
   }
   return "\tcall\t(%0), " FAKE_PTX_ALLOCA_NAME ", (%1);";
#undef FAKE_PTX_ALLOCA_NAME
}

/* Output instruction that sets soft stack pointer in shared memory to the
   value in register given by SRC_REGNO.  */
//原型 nvptx_output_set_softstack nvptx-protos.h nvptx.cc gcc15 新加的
const char *mtcs_ptx_output_set_softstack (MtcsPtxOutput *self,unsigned src_regno)
{
  MtcsOutput *mtcsOutput=(MtcsOutput *)self;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsPtx *mtcsPtx=(MtcsPtx *)mtcsTarget;
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  //struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)mtcsFunc->machine;
  struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)cfun->machine;

  n_debug("mtcsptxoutput.c -----nvptx.cc -----32-- const char * nvptx_output_set_softstack (unsigned src_regno)\n");
  if (nvptxMachine->has_softstack && !mtcsRtlData/*!crtl*/->is_leaf){
      fprintf (mtcsAsm->asmFile, "\tst.shared.u%d\t[%s], ",
           POINTER_SIZE, mtcs_reg_get_reg_name(mtcsReg,PTX_SOFTSTACK_SLOT_REGNUM)/*!reg_names[SOFTSTACK_SLOT_REGNUM]*/);
      mtcs_ptx_output_output_reg (self, src_regno, VOIDmode);
      fprintf (mtcsAsm->asmFile, ";\n");
  }
  return "";
}

//原型 nvptx_output_simt_enter nvptx-protos.h nvptx.cc gcc15 新加的
const char *mtcs_ptx_output_simt_enter (MtcsPtxOutput *self,rtx dest, rtx size, rtx align)
{
  n_debug("mtcsptxoutput.c -----nvptx.cc -----30-- const char * nvptx_output_simt_enter (rtx dest, rtx size, rtx align)\n");
  nvptx_output_unisimt_switch (self, true);
  nvptx_output_softstack_switch (self, true, dest, size, align);
  return "";
}

//原型 nvptx_output_atomic_insn nvptx-protos.h nvptx.cc
const char *mtcs_ptx_output_atomic_insn (MtcsPtxOutput *self,const char *asm_template, rtx *operands, int mem_pos,int memmodel_pos)
{
  nvptx_output_barrier (self,&operands[mem_pos], INTVAL (operands[memmodel_pos]), true);
  mtcs_output_asm_insn((MtcsOutput*)self,asm_template, operands);
  nvptx_output_barrier (self,&operands[mem_pos], INTVAL (operands[memmodel_pos]),false);
  return "";
}

//原型 nvptx_mem_local_p nvptx-protos.h nvptx.cc
bool mtcs_ptx_output_mem_local_p (MtcsPtxOutput *self,rtx mem)
{
  MtcsOutput *mtcsOutput=(MtcsOutput *)self;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);

  gcc_assert (GET_CODE (mem) == MEM);
  struct address_info info;
  decompose_mem_address (&info, mem);
  if (info.base != NULL && REG_P (*info.base)  && mtcs_reg_regnum_ptr_frame_p/*!REGNO_PTR_FRAME_P*/ (mtcsReg,REGNO (*info.base))){
      if (mtcs_options_target_soft_stack/*!TARGET_SOFT_STACK*/(mtcsOptions)) {
      /* Frame-related doesn't mean local.  */
      }else
          return true;
  }
  return false;
}

//原型 nvptx_output_red_partition nvptx-protos.h nvptx.cc
const char *mtcs_ptx_output_red_partition (MtcsPtxOutput *self,rtx dst, rtx offset)
{
    MtcsOutput *mtcsOutput=(MtcsOutput *)self;
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsPtx *mtcsPtx=(MtcsPtx *)mtcsTarget;
    MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
    MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
    MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   // struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)mtcsFunc->machine;
    struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)cfun->machine;

  const char *zero_offset = "\t\tmov.u64\t%%r%d, %%r%d; // vred buffer\n";
  const char *with_offset = "\t\tadd.u64\t%%r%d, %%r%d, %d; // vred buffer\n";

  if (offset == const0_rtx)
    fprintf (mtcsAsm->asmFile, zero_offset, REGNO (dst),
         REGNO (nvptxMachine/*!cfun->machine*/->red_partition));
  else
    fprintf (mtcsAsm->asmFile, with_offset, REGNO (dst),
         REGNO (nvptxMachine->red_partition), UINTVAL (offset));

  return "";
}

//原型 nvptx_output_simt_exit nvptx-protos.h nvptx.cc
const char *mtcs_ptx_output_simt_exit (MtcsPtxOutput *self,rtx src)
{
   n_debug("mtcsptxoutput.c -----nvptx.cc -----31-- const char * nvptx_output_simt_exit (rtx src)\n");
   nvptx_output_unisimt_switch (self, false);
   nvptx_output_softstack_switch (self, false, src, NULL_RTX, NULL_RTX);
   return "";
}


/* Output a register, subreg, or register pair (with optional
   enclosing braces).  */
/* 输出一个寄存器、子寄存器或寄存器对（带有可选的括号）。*/
void mtcs_ptx_output_output_reg (MtcsPtxOutput *self,unsigned regno, mtcs_mode inner_mode,int subreg_offset = -1)
{
  MtcsOutput *mtcsOutput=(MtcsOutput *)self;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsPtx *mtcsPtx=(MtcsPtx *)mtcsTarget;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  n_debug("mtcsptxoutput.c-----nvptx.cc -----85-- output_reg regno:%d mode:%d %s\n",regno,inner_mode,mtcs_mode_get_name(mtcsMode,inner_mode));
  FILE *file=mtcsAsm->asmFile;
  if (inner_mode == mtcsMode->modes.M_VOIDmode){
      if (mtcs_reg_is_hard (mtcsReg,regno))
          fprintf (file, "%s", mtcs_reg_get_reg_name(mtcsReg,regno)/*reg_names[regno]*/);
      else
          fprintf (file, "%%r%d", regno);
  }else if (subreg_offset >= 0){
      mtcs_ptx_output_output_reg (self,regno, mtcsMode->modes.M_VOIDmode);
      fprintf (file, "$%d", subreg_offset);
  }else{
      if (subreg_offset == -1)
          fprintf (file, "{");
      mtcs_ptx_output_output_reg (self,regno, inner_mode,
            mtcs_mode_get_size(mtcsMode,inner_mode)/*GET_MODE_SIZE(inner_mode)*/);
      fprintf (file, ",");
      mtcs_ptx_output_output_reg (self,regno, inner_mode, 0);
      if (subreg_offset == -1)
          fprintf (file, "}");
  }
}

/* Subroutine of nvptx_print_operand; used to print a memory reference X to FILE.  */
//原型 nvptx_print_address_operand (FILE *file, rtx x, machine_mode) nvptx.cc
void mtcs_ptx_output_print_address_operand (MtcsPtxOutput *self,rtx x, machine_mode)
{
   MtcsOutput *mtcsOutput=(MtcsOutput *)self;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

   rtx off;
   if (GET_CODE (x) == CONST)
      x = XEXP (x, 0);
   switch (GET_CODE (x)){
      case PLUS:
         off = XEXP (x, 1);
         mtcs_output_address/*!output_address*/ (mtcsOutput,VOIDmode, XEXP (x, 0));
         fprintf (mtcsAsm->asmFile, "+");
         mtcs_output_address (mtcsOutput,VOIDmode, off);
         break;

      case SYMBOL_REF:
      case LABEL_REF:
         mtcs_output_addr_const (mtcsOutput, x);
         break;

      default:
         gcc_assert (GET_CODE (x) != MEM);
         mtcs_ptx_output_print_operand/*!nvptx_print_operand*/ (self, x, 0);
         break;
   }
}


/* Print an operand, X, to FILE, with an optional modifier in CODE.

   Meaning of CODE:
   . -- print the predicate for the instruction or an emptry string for an
        unconditional one.
   # -- print a rounding mode for the instruction

   A -- print a data area for a MEM
   c -- print an opcode suffix for a comparison operator, including a type code
   D -- print a data area for a MEM operand
   S -- print a shuffle kind specified by CONST_INT
   t -- print a type opcode suffix, promoting QImode to 32 bits
   T -- print a type size in bits
   u -- print a type opcode suffix without promotions.
   p -- print a '!' for constant 0.
   x -- print a destination operand that may also be a bit bucket.  */
//原型 static void nvptx_print_operand (FILE file, rtx x, int code) nvptx.cc
void mtcs_ptx_output_print_operand (MtcsPtxOutput *self, rtx x, int code)
{
   MtcsOutput *mtcsOutput=(MtcsOutput *)self;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsPtx *mtcsPtx=(MtcsPtx *)mtcsTarget;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsReal *mtcsReal=mtcs_target_get_real(mtcsTarget);
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);


   FILE *file=mtcsAsm->asmFile;
   n_debug("mtcsptxoutput.c -----nvptx.cc -----51-- TARGET_PRINT_OPERAND void nvptx_print_operand"
         "(FILE *file, rtx x, int code) code:%d %c\n",code,code);

   if (code == '.'){
      x = mtcsOutput->current_insn_predicate;
      if (x){
         fputs ("@", file);
         if (GET_CODE (x) == EQ)
            fputs ("!", file);
         mtcs_ptx_output_output_reg (self, REGNO (XEXP (x, 0)), VOIDmode);
      }
      return;
   }else if (code == '#'){
      fputs (".rn", file);
      return;
   }
   enum rtx_code x_code = GET_CODE (x);
   machine_mode mode = GET_MODE (x);
   switch (code){
      case 'x':
         if (mtcsOutput->current_output_insn != NULL
         && find_reg_note (mtcsOutput->current_output_insn, REG_UNUSED, x) != NULL_RTX){
            fputs ("_", file);
            return;
         }
         goto common;
      case 'B':
         if (SYMBOL_REF_P (XEXP (x, 0)))
            switch (PTX_SYMBOL_DATA_AREA (XEXP (x, 0))){
               case PTX_DATA_AREA_GENERIC:
                  /* Assume worst-case: global.  */
                  gcc_fallthrough (); /* FALLTHROUGH.  */
               case PTX_DATA_AREA_GLOBAL:
                  break;
               case PTX_DATA_AREA_SHARED:
                  fputs (".cta", file);
                  return;
               case PTX_DATA_AREA_LOCAL:
               case PTX_DATA_AREA_CONST:
               case PTX_DATA_AREA_PARAM:
               default:
                  gcc_unreachable ();
            }

         /* There are 2 cases where membar.sys differs from membar.gl:
         - host accesses global memory (f.i. systemwide atomics)
         - 2 or more devices are setup in peer-to-peer mode, and one
         peer can access global memory of other peer.
         Neither are currently supported by openMP/OpenACC on nvptx, but
         that could change, so we default to membar.sys.  We could support
         this more optimally by adding DATA_AREA_SYS and then emitting
         .gl for DATA_AREA_GLOBAL and .sys for DATA_AREA_SYS.  */
         fputs (".sys", file);
         return;

      case 'A':
         x = XEXP (x, 0);
         gcc_fallthrough (); /* FALLTHROUGH. */
      case 'D':
         if (GET_CODE (x) == CONST)
            x = XEXP (x, 0);
         if (GET_CODE (x) == PLUS)
            x = XEXP (x, 0);

         if (GET_CODE (x) == SYMBOL_REF)
            fputs (ptx_tool_section_for_sym(x), file);
         break;

      case 't':
      case 'u':
         if (x_code == SUBREG){
            machine_mode inner_mode = GET_MODE (SUBREG_REG (x));
            if (mtcs_mode_is_vector_p/*!VECTOR_MODE_P*/ (mtcsMode,inner_mode)
            && (mtcs_mode_get_size (mtcsMode,mode) <= mtcs_mode_get_size (mtcsMode,mtcs_mode_get_inner(mtcsMode,inner_mode))))
               mode = mtcs_mode_get_inner (mtcsMode,inner_mode);
            else if (mtcs_ptx_split_mode_p (mtcsPtx,inner_mode))
               mode = mtcs_ptx_maybe_split_mode (mtcsPtx,inner_mode);
            else
               mode = inner_mode;
         }
         fprintf (file, "%s", mtcs_mode_get_type (mtcsMode,mode, code == 't'));
         break;

      case 'H':
      case 'L':
      {
         rtx inner_x = SUBREG_REG (x);
         machine_mode inner_mode = GET_MODE (inner_x);
         machine_mode split = mtcs_ptx_maybe_split_mode (mtcsPtx,inner_mode);
         mtcs_ptx_output_output_reg (self, REGNO (inner_x), split,(code == 'H'? mtcs_mode_get_size (mtcsMode,inner_mode) / 2 : 0));
      }
         break;

      case 'S':
      {
         ptx_shuffle_kind kind = (ptx_shuffle_kind) UINTVAL (x);
         /* Same order as nvptx_shuffle_kind.  */
         static const char *const kinds[] = {".up", ".down", ".bfly", ".idx"};
         fputs (kinds[kind], file);
      }
      break;

      case 'T':
         fprintf (file, "%d", mtcs_mode_get_bitsize(mtcsMode,mode));
         break;

      case 'j':
         fprintf (file, "@");
         goto common;

      case 'J':
         fprintf (file, "@!");
         goto common;

      case 'p':
         if (INTVAL (x) == 0)
            fprintf (file, "!");
         break;

      case 'c':
         mode = GET_MODE (XEXP (x, 0));
         switch (x_code){
            case EQ:
               fputs (".eq", file);
               break;
            case NE:
               if (mtcs_mode_is_float_p/*!FLOAT_MODE_P*/(mtcsMode,mode))
                  fputs (".neu", file);
               else
                  fputs (".ne", file);
               break;
            case LE:
            case LEU:
               fputs (".le", file);
               break;
            case GE:
            case GEU:
               fputs (".ge", file);
               break;
            case LT:
            case LTU:
               fputs (".lt", file);
               break;
            case GT:
            case GTU:
               fputs (".gt", file);
               break;
            case LTGT:
               fputs (".ne", file);
               break;
            case UNEQ:
               fputs (".equ", file);
               break;
            case UNLE:
               fputs (".leu", file);
               break;
            case UNGE:
               fputs (".geu", file);
               break;
            case UNLT:
               fputs (".ltu", file);
               break;
            case UNGT:
               fputs (".gtu", file);
               break;
            case UNORDERED:
               fputs (".nan", file);
               break;
            case ORDERED:
               fputs (".num", file);
               break;
            default:
               gcc_unreachable ();
         }
         if (mtcs_mode_is_float_p/*!FLOAT_MODE_P*/(mtcsMode,mode)
         || x_code == EQ || x_code == NE
         || x_code == GEU || x_code == GTU
         || x_code == LEU || x_code == LTU)
            fputs (mtcs_mode_get_type (mtcsMode,mode, true), file);
         else
            fprintf (file, ".s%d", mtcs_mode_get_bitsize (mtcsMode,mode));
         break;
      default:
common:
         switch (x_code){
            case SUBREG:
            {
               n_debug("A3---------code:%d %c\n",code,code);

               rtx inner_x = SUBREG_REG (x);
               machine_mode inner_mode = GET_MODE (inner_x);
               machine_mode split = mtcs_ptx_maybe_split_mode (mtcsPtx,inner_mode);

               if (mtcs_mode_is_vector_p (mtcsMode,inner_mode)
               && (mtcs_mode_get_size (mtcsMode,mode) <= mtcs_mode_get_size (mtcsMode,mtcs_mode_get_inner (mtcsMode,inner_mode)))){
                  mtcs_ptx_output_output_reg (self, REGNO (inner_x), VOIDmode);
                  fprintf (file, ".%s", SUBREG_BYTE (x) == 0 ? "x" : "y");
               }else if (mtcs_ptx_split_mode_p (mtcsPtx,inner_mode)
               && (mtcs_mode_get_size (mtcsMode,inner_mode) == mtcs_mode_get_size (mtcsMode,mode)))
                  mtcs_ptx_output_output_reg (self, REGNO (inner_x), split);
               else
                  mtcs_ptx_output_output_reg (self, REGNO (inner_x), split, SUBREG_BYTE (x));
            }
               break;

            case REG:
               n_debug("A4---------code:%d %c\n",code,code);

               mtcs_ptx_output_output_reg (self, REGNO (x), mtcs_ptx_maybe_split_mode (mtcsPtx,mode));
               break;

            case MEM:
               n_debug("A5---------code:%d %c\n",code,code);

               fputc ('[', file);
               mtcs_ptx_output_print_address_operand (self, XEXP (x, 0), mode);
               fputc (']', file);
               break;

            case CONST_INT:
               n_debug("A6---------code:%d %c\n",code,code);

               mtcs_output_addr_const ((MtcsOutput*)self, x);
               break;

            case CONST:
            case SYMBOL_REF:
            case LABEL_REF:
               n_debug("A7---------code:%d %c\n",code,code);

               /* We could use output_addr_const, but that can print things like
               "x-8", which breaks ptxas.  Need to ensure it is output as
               "x+-8".  */
               mtcs_ptx_output_print_address_operand (self, x, VOIDmode);
               break;

            case CONST_DOUBLE:
               n_debug("A8---------code:%d %c\n",code,code);

               long vals[2];
               mtcs_real_real_to_target/*!real_to_target*/(mtcsReal,vals, CONST_DOUBLE_REAL_VALUE (x), mode);
               vals[0] &= 0xffffffff;
               vals[1] &= 0xffffffff;
               if (mode == mtcsMode->modes.M_SFmode)
                  fprintf (file, "0f%08lx", vals[0]);
               else
                  fprintf (file, "0d%08lx%08lx", vals[1], vals[0]);
               break;

            case CONST_VECTOR:
            {
               n_debug("A9---------code:%d %c\n",code,code);

               unsigned n = CONST_VECTOR_NUNITS (x);
               fprintf (file, "{ ");
               for (unsigned i = 0; i < n; ++i) {
                  if (i != 0)
                     fprintf (file, ", ");

                  rtx elem = CONST_VECTOR_ELT (x, i);
                  mtcs_output_addr_const ((MtcsOutput*)self, elem);
               }
               fprintf (file, " }");
            }
               break;

            default:
               fprintf(stderr,"mtcsptxoutput.c -----nvptx.cc -----51cc %d %c\n",code,code);

               mtcs_output_addr_const ((MtcsOutput*)self, x);
               fprintf(stderr,"mtcsptxoutput.c -----nvptx.cc -----51dd %d %c\n",code,code);

         }
   }
}

//原型 get_insn_name rtl.h insn-output.cc
static const char *getInsnName_cb(MtcsOutput *mtcsOutput,int code)
{
   return ptx_get_insn_name(code);
}
//原型 output_quoted_string output.h final.cc
static void outputQuotedString (NString *str, const char *string)
{
   char c;
   n_string_append_c(str,'\"');
   while ((c = *string++) != 0){
      if (ISPRINT (c)){
         if (c == '\"' || c == '\\')
            n_string_append_c(str,'\\');
         n_string_append_c(str,c);
      }else
         n_string_append_printf(str,"\\%03o", (unsigned char) c);
   }
   n_string_append_c(str,'\"');
}

/*
解决bug 082
.visible .entry _Z6TFirst7setdataEPN6TFirstEw (.param.u64 %in_ar0, .param.u32 %in_ar1)
{
...
.reg.u32 %r26;
.reg.u64 %r27;
.file 2 "/home/sns/workspace/ai/src/debug/ai0.c"
.loc 2 14 20
mov.u64  %r22, %ar0;
* .file 2 "/home/sns/workspace/ai/src/debug/ai0.c" 不能输出在函数内，必须在函数外
* 原型 mtcsdwarf2out.c maybe_emit_file
*/
static void outputDebugFile_cb(MtcsOutput *mtcsOutput,int emitted_number,char *fileName)
{
   MtcsPtxOutput *self = (MtcsPtxOutput *)mtcsOutput;
   if(!self->debugFileStr)
      self->debugFileStr= n_string_new("\n");
   NString *str=self->debugFileStr;
   n_string_append_printf(str,"\t.file %u ",emitted_number);
   outputQuotedString(str,fileName);
   n_string_append_c(str,'\n');
   //fprintf(stderr,"outputDebugFile_cb -- %s\n",str->str);
}

//解决 bug 082
char *mtcs_ptx_output_get_debug_file(MtcsPtxOutput *self)
{
   if(!self->debugFileStr)
      return NULL;
   return self->debugFileStr->str;
}
//原型  targetm.encode_section_info (exp, rtl, true); #define TARGET_ENCODE_SECTION_INFO default_encode_section_info
//原型 mtcsoutput.h void (*encode_section_info) (MtcsOutput *self,tree decl, rtx rtl, int first ATTRIBUTE_UNUSED);
static void encodeSectionInfo_cb(MtcsOutput *mtcsOutput,tree decl, rtx rtl, int first ATTRIBUTE_UNUSED)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsOutput);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;

   n_debug("mtcsptx.c -----nvptx.cc -----3-- TARGET_ENCODE_SECTION_INFO nvptx_encode_section_info first:%d\n",first);
   mtcs_print_rtl(stderr,rtl);
   mtcs_output_default_encode_section_info/*!default_encode_section_info*/(mtcsOutput,decl, rtl, first);//原型来自varasm.cc
   n_debug("mtcsptx.c -----nvptx.cc -----3aa-- default_encode_section_info\n");
   mtcs_print_rtl(stderr,rtl);
   if (first && MEM_P (rtl)) {
      ptx_data_area area = PTX_DATA_AREA_GENERIC;
      n_debug("mtcsptx.c -----nvptx.cc -----3bb--获取 data_area  PTX_DATA_AREA_GENERIC\n");

      if (VAR_P (decl)){
         if (lookup_attribute (MTCS_SHARED_STRING, DECL_ATTRIBUTES (decl))){
            area = PTX_DATA_AREA_SHARED;
            if (DECL_INITIAL (decl))
               error ("static initialization of variable %q+D in %<.shared%> memory is not supported", decl);
         }else if(lookup_attribute (MTCS_CONSTANT_STRING, DECL_ATTRIBUTES (decl))){
            area = PTX_DATA_AREA_CONST;
         }else{
            n_debug("mtcsptx.c -----nvptx.cc -----3dd-- DATA_AREA_GLOBAL\n");
            area = PTX_DATA_AREA_GLOBAL;
         }
      }
      PTX_SET_SYMBOL_DATA_AREA (XEXP (rtl, 0), area);
      n_debug("mtcsptx.c -----nvptx.cc -----3ee-- default_encode_section_info area:%d\n",area);
      mtcs_print_rtl(stderr,rtl);
   }
}


MtcsPtxOutput  *mtcs_ptx_output_new(MtcsMode *mtcsMode)
{
   MtcsPtxOutput *self = n_slice_alloc0 (sizeof(MtcsPtxOutput));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_output_init((MtcsOutput *)self);
   mtcsPtxInsnOutputInit(self);
   return self;
}

