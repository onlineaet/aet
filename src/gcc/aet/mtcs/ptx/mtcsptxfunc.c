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
#include "dwarf2out.h"
#include "dwarf2.h"
#include "dwarf2asm.h"
#include "except.h"

#include "mtcsptxfunc.h"
#include "ptx-common.h"
#include "mtcsptx.h"
#include "ptxtool.h"
#include "aet/aetprinttree.h"
#include "../../mtcsinfo.h"
#include "mtcsptxasm.h"

//原型 init_machine_status function.h
static void *initMachineStatus_cb(MtcsFunc *mtcsFunc);

static nuint treeHash_cb(nconstpointer v)
{
  tree t=(tree )v;
  return htab_hash_pointer (t);
}

//原型 bool libfunc_hasher::equal (libfunc_entry *e1, libfunc_entry *e2) optabs-libfuncs.cc
static nboolean treeHashEqual_cb(nconstpointer v1, nconstpointer v2)
{
    tree a=(tree )v1;
    tree b=(tree )v2;
    return a==b;
}

static nuint libfuncHash_cb(nconstpointer v)
{
    rtx x=(rtx )v;
    return htab_hash_pointer (x);
}

//原型 bool libfunc_hasher::equal (libfunc_entry *e1, libfunc_entry *e2) optabs-libfuncs.cc
static nboolean libfuncHashEqual_cb(nconstpointer v1, nconstpointer v2)
{
    rtx a=(rtx )v1;
    rtx b=(rtx )v2;
    return a==b;
}

//来自aetutils.h
static inline nboolean validTree(tree value)
{
   return (value!=NULL && value!=NULL_TREE && value!=error_mark_node
         && (unsigned long)value!=0xffffffffffffffff);
}

/**
 * 原型 #define SUPPORTS_STACK_ALIGNMENT (MAX_STACK_ALIGNMENT > STACK_BOUNDARY)
 */
static nboolean isSupportStackAlignment_cb(MtcsFunc *self)
{
   return PTX_MAX_STACK_ALIGNMENT > PTX_STACK_BOUNDARY;
}

/**
 * 原型 MAX_SUPPORTED_STACK_ALIGNMENT
 */
static nuint getMaxSupportStackAlignment_cb(MtcsFunc *self)
{
   return PTX_MAX_SUPPORTED_STACK_ALIGNMENT;
}

//原型 #ifndef STACK_CHECK_PROTECT default.h
//(!global_options.x_flag_exceptions                   \
//    ? 4 * 1024                                \
//    : targetm_common.except_unwind_info (&global_options) == UI_SJLJ  \
//      ? 8 * 1024                              \
//      : 12 * 1024)
static int getStackCheckProtect_cb(MtcsFunc *mtcsFunc)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsFunc);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
    MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
    MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

    if(!mtcsOptionsItem->x_flag_exceptions){
        return 4*1024;
    }else{
        if(target_common_except_unwind_info/*!targetm.targetm_common.except_unwind_info*/(mtcsMachine->common,
              mtcsOptions->global_options)){
            return 8*1024;
        }else{
            return 12*1024;
        }
    }
}
//原型 #ifndef STACK_OLD_CHECK_PROTECT default.h
static int getStackOldCheckProtect_cb(MtcsFunc *mtcsFunc)
{
   return getStackCheckProtect_cb(mtcsFunc);
}

//原型 #ifndef STACK_CHECK_MOVING_SP
static int getStackCheckMovingSp_cb(MtcsFunc *mtcsFunc)
{
   return 0;
}
//原型 STACK_DYNAMIC_OFFSET (FNDECL) function.cc
//INCOMING_REG_PARM_STACK_SPACE 在平台定义依赖 REG_PARM_STACK_SPACE nvptx没有定义
//所以不需要实现在INCOMING_REG_PARM_STACK_SPACE下的 STACK_DYNAMIC_OFFSET
static poly_int64 getStackDynamicOffset_cb(MtcsFunc *mtcsFunc, tree fndecl)
{
    int stack_pointer_offset = mtcs_func_get_stack_pointer_offset(mtcsFunc);
    nboolean accumulateOutgoingArgs=mtcs_func_is_accumulate_outgoing_args(mtcsFunc);
    nboolean outgoingRegParmStackSpace=mtcs_func_is_outgoint_reg_parm_stack_space(mtcsFunc,!fndecl? NULL_TREE : TREE_TYPE (fndecl));
    return accumulateOutgoingArgs?(mtcsFunc->mtcsRtlData->outgoing_args_size
          +stack_pointer_offset/*!STACK_POINTER_OFFSET*/):(poly_int64 (0)+stack_pointer_offset/*!STACK_POINTER_OFFSET*/);
}

//原型 init_machine_status function.h
static void *initMachineStatus_cb(MtcsFunc *mtcsFunc)
{
    n_debug("mtcsptxfunc.c-----nvptx.cc -----98-- nvptx_init_machine_status\n");
    struct ptx_machine_function *nvptxMachine = ggc_cleared_alloc<ptx_machine_function> ();
    nvptxMachine->return_mode = VOIDmode;//mtcsMode->modes.M_VOIDmode=0 相等的
    return (void*)nvptxMachine;
}

//原型 #define FUNCTION_ARG_REGNO_P(r) 0
static bool isFunctionArgRegno_cb(MtcsFunc *mtcsFunc,int regno)
{
   return false;
}

//原型 FIRST_PARM_OFFSET host=0 nvptx=0
static int getFirstParmOffset_cb(MtcsFunc *mtcsFunc,tree fndecl)
{
   return ((void)(fndecl), 0);
}

//原型 #define FRAME_POINTER_CFA_OFFSET(FNDECL) ((void)(FNDECL), 0)
static int getFramePointerCfaOffset_cb(MtcsFunc *mtcsFunc,tree fndecl)
{
   return ((void)(fndecl), 0);
}

//原型 #define EPILOGUE_USES(REG) false defaults.h
static bool epilogueUses_cb(MtcsFunc *self,nuint regno)
{
   return false;
}

//原型 #define INITIAL_ELIMINATION_OFFSET(FROM, TO, OFFSET)
//ptx的定义是 #define INITIAL_ELIMINATION_OFFSET(FROM, TO, OFFSET) \((OFFSET) = 0)
static HOST_WIDE_INT initialEliminationOffset_cb(MtcsFunc *self,int from, int to)
{
   return 0;
}

static void mtcsPtxFuncInit(MtcsPtxFunc *self)
{
    MtcsFunc *mtcsFunc=(MtcsFunc *)self;
    MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
    mtcs_func_set_frame_grows_downward(mtcsFunc,PTX_FRAME_GROWS_DOWNWARD);//原型 FRAME_GROWS_DOWNWARD
    mtcs_func_set_stack_grows_downward(mtcsFunc,PTX_STACK_GROWS_DOWNWARD); //原型 STACK_GROWS_DOWNWARD
    mtcs_func_set_args_grows_downward(mtcsFunc,0);//原型 ARGS_GROWS_DOWNWARD default.h gcn.h
    mtcs_func_set_stack_boundary(mtcsFunc,PTX_STACK_BOUNDARY);// //原型 STACK_BOUNDARY
    mtcs_func_set_preferred_stack_boundary(mtcsFunc,PTX_STACK_BOUNDARY);//原型 #define PREFERRED_STACK_BOUNDARY STACK_BOUNDARY
    mtcs_func_set_incoming_stack_boundary(mtcsFunc,PTX_STACK_BOUNDARY);//原型 #define INCOMING_STACK_BOUNDARY PREFERRED_STACK_BOUNDARY
    mtcs_func_set_function_boundary(mtcsFunc,PTX_FUNCTION_BOUNDARY);//原型 #define FUNCTION_BOUNDARY 32
    mtcs_func_set_stack_pointer_offset(mtcsFunc,0);///原型 STACK_POINTER_OFFSET #define STACK_POINTER_OFFSET    0 defaults.h
    mtcsFunc->is_support_stack_alignment=isSupportStackAlignment_cb;
    mtcsFunc->get_max_support_stack_alignment=getMaxSupportStackAlignment_cb;
    //原型 STACK_DYNAMIC_OFFSET (FNDECL) function.cc
    //INCOMING_REG_PARM_STACK_SPACE 在平台定义依赖 REG_PARM_STACK_SPACE nvptx没有定义 所以不需要实现在INCOMING_REG_PARM_STACK_SPACE下的 STACK_DYNAMIC_OFFSET
    mtcsFunc->get_stack_dynamic_offset=getStackDynamicOffset_cb;
    //原型 #ifndef STACK_CHECK_PROTECT
    mtcsFunc->get_stack_check_protect=getStackCheckProtect_cb;
    //原型 #ifndef STACK_OLD_CHECK_PROTECT
    mtcsFunc->get_stack_old_check_protect=getStackOldCheckProtect_cb;
    //原型 #ifndef STACK_CHECK_MOVING_SP
    mtcsFunc->get_stack_check_moving_sp=getStackCheckMovingSp_cb;
    //原型 init_machine_status function.h
    mtcsFunc->init_machine_status=initMachineStatus_cb;
    //原型 #define FUNCTION_ARG_REGNO_P(r) 0
    mtcsFunc->is_function_arg_regno=isFunctionArgRegno_cb;
    //原型 FIRST_PARM_OFFSET host=0 nvptx=0
    mtcsFunc->get_first_parm_offset=getFirstParmOffset_cb;
    //原型 #define FRAME_POINTER_CFA_OFFSET(FNDECL) ((void)(FNDECL), 0)
    mtcsFunc->get_frame_pointer_cfa_offset=getFramePointerCfaOffset_cb;
    //原型 #define EPILOGUE_USES(REG) false defaults.h
    mtcsFunc->epilogue_uses=epilogueUses_cb;
    //原型 #define INITIAL_ELIMINATION_OFFSET(FROM, TO, OFFSET)
    mtcsFunc->initial_elimination_offset=initialEliminationOffset_cb;

    mtcs_func_set_accumulate_outgoing_args(mtcsFunc,PTX_ACCUMULATE_OUTGOING_ARGS);
    mtcs_func_set_parm_boundary(mtcsFunc,PTX_PARM_BOUNDARY);
    mtcs_func_set_stack_push_code(mtcsFunc,PRE_DEC);//原型 STACK_PUSH_CODE default.h
    mtcs_func_set_stack_check_probe_interval_exp(mtcsFunc,12);//原型 #define STACK_CHECK_PROBE_INTERVAL_EXP 12 defaults.h
    mtcs_func_set_exit_ignore_stack(mtcsFunc,0); //原型 #define EXIT_IGNORE_STACK 0 defaults.h host=1
    self->declared_fndecls_htab = n_hash_table_new_full(treeHash_cb,treeHashEqual_cb,NULL, NULL);
    self->needed_fndecls_htab = n_hash_table_new_full (treeHash_cb, treeHashEqual_cb,NULL, NULL);
    self->declared_libfuncs_htab = n_hash_table_new_full (libfuncHash_cb, libfuncHashEqual_cb,NULL, NULL);
}



/* Emit a PTX return as a prototype or function prologue declaration
   for MODE.  */

void mtcs_ptx_func_write_return_mode (MtcsPtxFunc *self,NString *str, bool for_proto, mtcs_mode mode)
{
   MtcsFunc *mtcsFunc=(MtcsFunc *)self;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   n_debug("mtcsptxfunc.c -----nvptx.cc -----1000-- write_return_mode  mode:%d for_proto:%d\n",mode,for_proto);

   const char *ptx_type = mtcs_mode_get_type (mtcsMode,mode, false);
   const char *pfx = "\t.reg";
   const char *sfx = ";\n";

   if (for_proto)
      pfx = "(.param", sfx = "_out) ";
   char *regName=mtcs_reg_get_reg_name(mtcsReg,PTX_NVPTX_RETURN_REGNUM);
   // s << pfx << ptx_type << " " << reg_names[NVPTX_RETURN_REGNUM] << sfx;
   n_string_append_printf(str,"%s%s %s%s",pfx,ptx_type,regName,sfx);
}

/* Construct a function declaration from a call insn.  This can be
   necessary for two reasons - either we have an indirect call which
   requires a .callprototype declaration, or we have a libcall
   generated by emit_library_call for which no decl exists.  */

void mtcs_ptx_func_write_fn_proto_from_insn (MtcsPtxFunc *self,NString *strs, const char *name,rtx result, rtx pat)
{
   MtcsFunc *mtcsFunc=(MtcsFunc *)self;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsPtx *mtcsPtx=(MtcsPtx*)mtcsTarget;

   char *replaced_dots = NULL;
   if (!name){
      // s << "\t.callprototype ";
      n_string_append(strs,"\t.callprototype ");
      name = "_";
   }else{
      const char *replacement = mtcs_ptx_get_replace_function_name(mtcsPtx,name);
      if (replacement != name)
         name = replacement;
      else{
         replaced_dots = ptx_tool_replace_dot (name);
         if (replaced_dots)
            name = replaced_dots;
      }
      fprintf(stderr,"nvptx.cc write_fn_proto_from_insn --- 生成 // BEGIN FUNCTION DECL: main$_omp_fn$0字符串 name:%s\n",name);
      mtcs_ptx_func_write_fn_marker (self,strs, false, true, name);
      //      s << "\t.extern .func ";
      n_string_append(strs,"\t.extern .func ");
   }

   if (result != NULL_RTX)
      mtcs_ptx_func_write_return_mode (self,strs, true, GET_MODE (result));
   //  s << name;
   n_string_append(strs,name);

   if (replaced_dots)
      XDELETE (replaced_dots);

   int arg_end = XVECLEN (pat, 0);
   for (int i = 1; i < arg_end; i++){
      /* We don't have to deal with mode splitting & promotion here,
      as that was already done when generating the call
      sequence.  */
      machine_mode mode = GET_MODE (XEXP (XVECEXP (pat, 0, i), 0));
      mtcs_ptx_func_write_arg_mode (self,strs, -1, i - 1, mode);
   }
   if (arg_end != 1)
      n_string_append(strs,")");//    s << ")";
   n_string_append(strs,";\n");//  s << ";\n";
}

/* Record a libcall or unprototyped external function. CALLEE is the
   SYMBOL_REF.  Insert into the libfunc hash table and emit a ptx
   declaration for it.  */
//原型 static void nvptx_record_libfunc (rtx callee, rtx retval, rtx pat) nvptx.cc
void  mtcs_ptx_func_write_record_libfunc (MtcsPtxFunc *self,NString *strs,rtx callee, rtx retval, rtx pat)
{
  fprintf(stderr,"-----nvptx.cc -----20-- void nvptx_record_libfunc (rtx callee, rtx retval, rtx pat)\n");
  rtx slot=n_hash_table_lookup(self->declared_libfuncs_htab,callee);

  if (slot == NULL){
      slot = callee;
      const char *name = XSTR (callee, 0);
      n_hash_table_insert(self->declared_libfuncs_htab,callee,callee);
      mtcs_ptx_func_write_fn_proto_from_insn (self,strs, name, retval, pat);
   }
}

/* Emit a linker marker for a function decl or defn.  */
//生成 "// BEGIN GLOBAL FUNCTION DECL: _Z8TestMtcs7setDataEPN8TestMtcsEPf$cuda$0"
void mtcs_ptx_func_write_fn_marker (MtcsPtxFunc *self,NString *str, bool is_defn, bool globalize, const char *name)
{
   n_string_append(str,"\n// BEGIN");
   if (globalize)
      n_string_append(str," GLOBAL");
   n_string_append(str," FUNCTION ");
   n_string_append(str,(is_defn ? "DEF: " : "DECL: "));
   n_string_append_printf(str,"%s\n",name);
}

/* Helper for write_arg.  Emit a single PTX argument of MODE, either
   in a prototype, or as copy in a function prologue.  ARGNO is the
   index of this argument in the PTX function.  FOR_REG is negative,
   if we're emitting the PTX prototype.  It is zero if we're copying
   to an argument register and it is greater than zero if we're
   copying to a specific hard register.  */
/* write_arg 的辅助函数。发出一个 MODE 类型的 PTX 参数，可以在原型中发出，也可以在函数序言中作为副本发出。
 * ARGNO 是此参数在 PTX 函数中的索引。如果我们发出的是 PTX 原型，则 FOR_REG 为负数。如果我们要复制到参数寄存器，
 * 则为零；如果我们要复制到特定的硬件寄存器，则为大于零的数值。
 * 生成的ptx像这样
 * .reg.u64 %ar0; //声明寄存器 0 是在函数中的位置
 *  ld.param.u64 %ar0, [%in_ar0]; in_ar0中0在函数中的位置
 */
//生成“.visible .entry _Z8TestMtcs7setDataEPN8TestMtcsEPf$cuda$0 (.param.u64 %in_ar0, .param.u64 %in_ar1);”
//中的参数部分 (.param.u64 %in_ar0, .param.u64 %in_ar1)
int mtcs_ptx_func_write_arg_mode (MtcsPtxFunc *self,NString *str, int for_reg, int argno,mtcs_mode mode)
{
  MtcsFunc *mtcsFunc=(MtcsFunc *)self;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsPtx *mtcsPtx=(MtcsPtx *)mtcsTarget;
  const char *ptx_type = mtcs_mode_get_type (mtcsMode,mode, false);
  n_debug("mtcsptxfunc.c -----nvptx.cc -----84-- write_arg_mode: for_reg:%d argno:%d  mode:%d ptx_type:%s E_DImode:%d\n",
        for_reg,argno,mode,ptx_type,E_DImode);

  if (for_reg < 0){
      /* Writing PTX prototype.  */
      //s << (argno ? ", " : " (");
      n_string_append(str,argno ? ", " : " (");
      //s << ".param" << ptx_type << " %in_ar" << argno;
      //%的转义是%%
      n_string_append_printf(str,".param%s %%in_ar%d",ptx_type,argno);
  }else{
     // s << "\t.reg" << ptx_type << " ";
      n_string_append_printf(str,"\t.reg%s ",ptx_type);
      if (for_reg)
          //s << reg_names[for_reg];
          n_string_append(str,mtcs_reg_get_reg_name(mtcsReg,for_reg)/*reg_names[for_reg]*/);
      else
          //s << "%ar" << argno;
          n_string_append_printf(str,"%%ar%d",argno);

      n_string_append(str,";\n");
      if (argno >= 0){
          // s << "\tld.param" << ptx_type << " ";
          n_string_append_printf(str,"\tld.param%s ",ptx_type);
          if (for_reg)
            //s << reg_names[for_reg];
              n_string_append(str,mtcs_reg_get_reg_name(mtcsReg,for_reg)/*reg_names[for_reg]*/);
          else
            //s << "%ar" << argno;
              n_string_append_printf(str,"%%ar%d",argno);

          //s << ", [%in_ar" << argno << "];\n";
          n_string_append_printf(str,", [%%in_ar%d];\n",argno);
      }
  }
  return argno + 1;
}


/* DECL is an external FUNCTION_DECL, make sure its in the fndecl hash
   table and write a ptx prototype.  These are emitted at end of
   compilation.  */
//原型 static nvptx_record_fndecl (tree decl) nvptx.cc
void mtcs_ptx_func_record_fndecl (MtcsPtxFunc *self,tree decl)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsPtx *mtcsPtx=(MtcsPtx *)mtcsTarget;
   NString *strs=mtcsPtx->func_decls;
   n_debug("mtcsptxfunc.c -----nvptx.cc -----19-- void nvptx_record_fndecl (tree decl)\n");
   aet_print_tree(decl);
   /*!
   tree *slot = self->declared_fndecls_htab->find_slot (decl, INSERT);
   if (*slot == NULL){
      *slot = decl;
      const char *name = get_fnname_from_decl (decl);
      mtcs_ptx_func_write_fn_proto (self,strs, false, name, decl);
   }
   */
   tree slot=n_hash_table_lookup(self->declared_fndecls_htab,decl);
   if(!validTree(slot)){
      n_hash_table_insert(self->declared_fndecls_htab,decl,decl);
      const char *name = get_fnname_from_decl (decl);
      n_debug("mtcsptxfunc.c -----nvptx.cc -----19aa-- void nvptx_record_fndecl (tree decl) %s\n",name);
      mtcs_ptx_func_write_fn_proto (self,name, decl);
   }
}

/* Write a .func or .kernel declaration or definition along with
   a helper comment for use by ld.  S is the stream to write to, DECL
   the decl for the function with name NAME.  For definitions, emit
   a declaration too.  */
//原型 static void write_fn_proto
char *mtcs_ptx_func_write_fn_proto (MtcsPtxFunc *self,const char *name, const_tree decl, bool force_public=false)
{
   MtcsFunc *mtcsFunc=(MtcsFunc *)self;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsPtx *mtcsPtx=(MtcsPtx*)mtcsTarget;

   n_debug("mtcsptxfunc.c -----nvptx.cc -----81-- write_fn_proto name:%s force_public:%d\n",name,force_public);
   const char *replacement = mtcs_ptx_get_replace_function_name(mtcsPtx,name);
   char *replaced_dots = NULL;
   if (replacement != name)
      name = replacement;
   else{
      replaced_dots = ptx_tool_replace_dot (name);
      n_debug("mtcsptxfunc.c -----nvptx.cc -----81aa-- write_fn_proto replaced_dots:%s\n",replaced_dots);
      if (replaced_dots)
         name = replaced_dots;
   }
   if (name[0] == '*')
      name++;

  // if (is_defn)
   /* Emit a declaration.  The PTX assembler gets upset without it. 发出声明。如果没有它，PTX 汇编器就会出错 */
    //  mtcs_ptx_func_write_fn_proto_1 (self,strs, false,   name, decl, force_public);
   char *ret= mtcs_ptx_func_write_fn_proto_1 (self,name, decl, force_public);
   if (replaced_dots)
      XDELETE (replaced_dots);
   return ret;
}

/* A non-memory argument of mode MODE is being passed, determine the mode it
   should be promoted to.  This is also used for determining return
   type promotion.
*/
static mtcs_mode promote_arg (MtcsPtxFunc *self,mtcs_mode mode, bool prototyped)
{
   MtcsFunc *mtcsFunc=(MtcsFunc *)self;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   if (!prototyped && mode == mtcsMode->modes.M_SFmode)
      /* K&R float promotion for unprototyped functions.  */
      mode = mtcsMode->modes.M_DFmode;
   else if (mtcs_mode_get_size/*GET_MODE_SIZE*/ (mtcsMode,mode) < mtcs_mode_get_size/*GET_MODE_SIZE*/ (mtcsMode,mtcsMode->modes.M_SImode))
      mode = mtcsMode->modes.M_SImode;
   return mode;
}

/* A non-memory return type of MODE is being returned.  Determine the
   mode it should be promoted to.  */

static mtcs_mode promote_return (MtcsPtxFunc *self,mtcs_mode mode)
{
   return promote_arg (self,mode, true);
}

/* Look for attributes in ATTRS that would indicate we must write a function
   as a .entry kernel rather than a .func.  Return true if one is found.  */
//来自mtcsinfo.h mtcs_info_get_func_type
static bool write_as_kernel (tree decl)
{
   MtcsFuncType type = mtcs_info_get_func_type(decl);
   return type == MTCS_FUNC_KERNEL;
}


/* Process a function return TYPE to emit a PTX return as a prototype
   or function prologue declaration.  Returns true if return is via an
   additional pointer parameter.  The promotion behavior here must
   match the regular GCC function return mashalling.
 */
//原型 static bool write_return_type nvptx.cc
bool mtcs_ptx_func_write_return_type (MtcsPtxFunc *self,NString *strs, bool for_proto, tree type)
{
   MtcsFunc *mtcsFunc=(MtcsFunc *)self;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsPtx *mtcsPtx=(MtcsPtx *)mtcsTarget;

   struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)cfun->machine;
   mtcs_mode mode=mtcs_mode_host2device_by_tree(mtcsMode,type,TYPE_MODE (type));
   n_debug("mtcsptxfunc.c -----nvptx.cc -----82-- write_return_type mode:%d for_proto:%d\n",mode,for_proto);
   if (mode == mtcsMode->modes.M_VOIDmode)
      return false;

   bool return_in_mem = mtcs_ptx_pass_in_memory (mtcsPtx,mode, type, true);
   n_debug("mtcsptxfunc.c -----nvptx.cc -----82aa  return_in_mem:%d\n",return_in_mem);

   if (return_in_mem){
      if (for_proto)
         return return_in_mem;

      /* Named return values can cause us to return a pointer as well
      as expect an argument for the return location.  This is
      optimization-level specific, so no caller can make use of
      this data, but more importantly for us, we must ensure it
      doesn't change the PTX prototype.  */
      //mode = (machine_mode) cfun->machine->return_mode;//mtcs replace machine
      mode = (mtcs_mode) nvptxMachine->return_mode;
      if (mode == mtcsMode->modes.M_VOIDmode)
         return return_in_mem;

      /* Clear return_mode to inhibit copy of retval to non-existent
      retval parameter.  */
      // cfun->machine->return_mode = VOIDmode;//mtcs replace machine
      nvptxMachine->return_mode=mtcsMode->modes.M_VOIDmode;
   }else
      mode = promote_return (self,mode);

   mtcs_ptx_func_write_return_mode (self,strs, for_proto, mode);

   return return_in_mem;
}

static bool mtcs_stdarg_p (const_tree fntype)
{
   function_args_iterator args_iter;
   tree n = NULL_TREE, t;
   n_debug("mtcsptxfunc.c mtcs_stdarg_p 00 %p\n",fntype);
   if (!fntype)
      return false;
   n_debug("mtcsptxfunc.c mtcs_stdarg_p 11 %p\n",fntype);

   if (TYPE_NO_NAMED_ARGS_STDARG_P (fntype))
      return true;
   n_debug("mtcsptxfunc.c mtcs_stdarg_p 22 %p\n",fntype);
   int count=0;
   FOREACH_FUNCTION_ARGS (fntype, t, args_iter){
      n = t;
      count++;
   }
   n_debug("mtcsptxfunc.c mtcs_stdarg_p 33 count:%d %p n:%p void_type_node:%p\n",count,fntype,n,void_type_node);
   aet_print_tree(n);
   return n != NULL_TREE && n != void_type_node;
}

/*
 * Helper function for write_fn_proto.
 * */
//原型 static void write_fn_proto_1 nvptx.cc
//生成 "// BEGIN GLOBAL FUNCTION DEF: _Z8TestMtcs7setDataEPN8TestMtcsEPf$cuda$0"
//    ".visible .entry _Z8TestMtcs7setDataEPN8TestMtcsEPf$cuda$0 (.param.u64 %in_ar0, .param.u64 %in_ar1);"
char *mtcs_ptx_func_write_fn_proto_1 (MtcsPtxFunc *self, const char *name, const_tree decl, bool force_public)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsPtx *mtcsPtx = (MtcsPtx *)mtcsTarget;
   n_debug("mtcsptxfunc.c -----nvptx.cc -----52-- void write_fn_proto_1 force_public:%d\n",force_public);
   MtcsPtxAsm *mtcsPtxAsm=(MtcsPtxAsm *)mtcs_target_get_asm(mtcsTarget);
   char *ret=mtcs_ptx_asm_get_func_decl_asm(mtcsPtxAsm,name);
   if(ret)
      return ret;
   NString *str=n_string_new("");

   /* PTX declaration.  */
   if (DECL_EXTERNAL (decl))
      // s << ".extern ";
      n_string_append(str,".extern ");
   else if (TREE_PUBLIC (decl) || force_public)
      // s << (DECL_WEAK (decl) ? ".weak " : ".visible ");
      n_string_append(str,(DECL_WEAK (decl) ? ".weak " : ".visible "));

   //s << (write_as_kernel (DECL_ATTRIBUTES (decl)) ? ".entry " : ".func ");
   n_string_append(str,write_as_kernel(decl) ? ".entry " : ".func ");
   n_debug("mtcsptxfunc.c -----nvptx.cc -----52bb-- DECL_EXTERNAL (decl):%d (TREE_PUBLIC (decl) || force_public):%d %s\n",
         DECL_EXTERNAL (decl),(TREE_PUBLIC (decl) || force_public),str->str);
   tree fntype = TREE_TYPE (decl);
   tree result_type = TREE_TYPE (fntype);

   /* atomic_compare_exchange_$n builtins have an exceptional calling
   convention.  */
   int not_atomic_weak_arg = -1;
   if (DECL_BUILT_IN_CLASS (decl) == BUILT_IN_NORMAL)
      switch (DECL_FUNCTION_CODE (decl)){
         case BUILT_IN_ATOMIC_COMPARE_EXCHANGE_1:
         case BUILT_IN_ATOMIC_COMPARE_EXCHANGE_2:
         case BUILT_IN_ATOMIC_COMPARE_EXCHANGE_4:
         case BUILT_IN_ATOMIC_COMPARE_EXCHANGE_8:
         case BUILT_IN_ATOMIC_COMPARE_EXCHANGE_16:
            /* These atomics skip the 'weak' parm in an actual library
            call.  We must skip it in the prototype too.  */
            not_atomic_weak_arg = 3;
            break;

         default:
            break;
      }

   /* Declare the result.  */
   bool return_in_mem = mtcs_ptx_func_write_return_type (self,str, true, result_type);
   n_debug("mtcsptxfunc.c -----nvptx.cc -----52cc-- return_in_mem:%d not_atomic_weak_arg:%d %s\n",
         return_in_mem,not_atomic_weak_arg,str->str);
   n_string_append(str,name);//写入函数名
   int argno = 0;

   /* Emit argument list.  */
   if (return_in_mem)
      argno = mtcs_ptx_func_write_arg_type (self,str, -1, argno, mtcs_ptr_type_node, true);

   /* We get:
   NULL in TYPE_ARG_TYPES, for old-style functions
   NULL in DECL_ARGUMENTS, for builtin functions without another
   declaration.
   So we have to pick the best one we have.  */
   tree args = TYPE_ARG_TYPES (fntype);
   bool prototyped = true;
   if (!args){
      args = DECL_ARGUMENTS (decl);
      prototyped = false;
   }
   n_debug("mtcsptxfunc.c -----nvptx.cc -----52dd-- 写入参数 %s fntype:%p result_type:%p stdarg_p (fntype):%d %d\n",
         str->str,fntype,result_type,stdarg_p (fntype),mtcs_stdarg_p (fntype));

   for (; args; args = TREE_CHAIN (args), not_atomic_weak_arg--){
      tree type = prototyped ? TREE_VALUE (args) : TREE_TYPE (args);
      n_debug("mtcsptxfunc.c -----nvptx.cc -----52ee--not_atomic_weak_arg:%d 写入参数 %s\n",not_atomic_weak_arg,str->str);

      if (not_atomic_weak_arg)
         argno = mtcs_ptx_func_write_arg_type (self,str, -1, argno, type, prototyped);
      else
         gcc_assert (TREE_CODE (type) == BOOLEAN_TYPE);
   }

   if (stdarg_p (fntype)){
      n_debug("mtcsptxfunc.c -----nvptx.cc -----52ff--not_atomic_weak_arg:%d 写入参数 %s\n",not_atomic_weak_arg,str->str);
      argno = mtcs_ptx_func_write_arg_type (self,str, -1, argno, mtcs_ptr_type_node, true);
   }

   if (DECL_STATIC_CHAIN (decl)){
      n_debug("mtcsptxfunc.c -----nvptx.cc -----52gg--not_atomic_weak_arg:%d 写入参数 %s\n",not_atomic_weak_arg,str->str);
      argno = mtcs_ptx_func_write_arg_type (self,str, -1, argno, mtcs_ptr_type_node, true);
   }

   if (argno < 2 && strcmp (name, "main") == 0){
      n_debug("mtcsptxfunc.c -----nvptx.cc -----52hh--argno:%d 写入参数 %s\n",argno,str->str);
      if (argno == 0)
         argno = mtcs_ptx_func_write_arg_type (self,str, -1, argno, integer_type_node, true);
      if (argno == 1)
         argno = mtcs_ptx_func_write_arg_type (self,str, -1, argno, ptr_type_node, true);
   }

   if (argno)
      n_string_append(str,")");//发射参数结束
   n_debug("mtcsptxfunc.c -----nvptx.cc -----52ii-- 函数到asm结束 %s\n",str->str);
   mtcs_ptx_asm_add_func_decl_asm(mtcsPtxAsm,name,str,DECL_EXTERNAL (decl));
   return str->str;

}



/* Process function parameter TYPE to emit one or more PTX
   arguments. S, FOR_REG and ARGNO as for write_arg_mode.  PROTOTYPED
   is true, if this is a prototyped function, rather than an old-style
   C declaration.  Returns the next argument number to use.

   The promotion behavior here must match the regular GCC function
   parameter marshalling machinery.  */
//原型 static int write_arg_type nvptx.cc
int mtcs_ptx_func_write_arg_type (MtcsPtxFunc *self,NString *strs, int for_reg, int argno,tree type, bool prototyped)
{
   MtcsFunc *mtcsFunc=(MtcsFunc *)self;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsPtx *mtcsPtx=(MtcsPtx *)mtcsTarget;
   struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)cfun->machine;

  // machine_mode hostMode = TYPE_MODE (type);
   mtcs_mode mode=TYPE_MODE (type);//mtcs_mode_host2device_by_tree(mtcsMode,type,hostMode);
   n_debug("mtcsptxfunc.c -----nvptx.cc -----83-- vwrite_arg_type: for_reg:%d argno:%d prototyped:%d mode:%d \n",
            for_reg,argno,prototyped,mode);

   if (mode == mtcsMode->modes.M_VOIDmode)
      return argno;

   if (mtcs_ptx_pass_in_memory (mtcsPtx,mode, type, false)){
      mode = mtcs_mode_get_Pmode(mtcsMode);
      n_debug("mtcsptxfunc.c -----nvptx.cc -----83aa--mtcs_ptx_pass_in_memory mode:%d\n",mode);
   }else{
      bool split = TREE_CODE (type) == COMPLEX_TYPE;
      n_debug("mtcsptxfunc.c -----nvptx.cc -----83bb--是不是复数 split:%d\n",split);
      if (split){
         /* Complex types are sent as two separate args.  */
         type = TREE_TYPE (type);
        // hostMode = TYPE_MODE (type);
         mode=TYPE_MODE (type);//mtcs_mode_host2device_by_tree(mtcsMode,type,hostMode);
         prototyped = true;
      }

      mode = promote_arg (self,mode, prototyped);
      n_debug("mtcsptxfunc.c -----nvptx.cc -----83cc-- mode:%d\n",mode);
      if (split)
         argno = mtcs_ptx_func_write_arg_mode (self,strs, for_reg, argno, mode);
   }
   return mtcs_ptx_func_write_arg_mode (self,strs, for_reg, argno, mode);
}

/* DECL is an external FUNCTION_DECL, that we're referencing.  If it
   is prototyped, record it now.  Otherwise record it as needed at end
   of compilation, when we might have more information about it.  */
//原型 nvptx_record_needed_fndecl nvptx.cc
void mtcs_ptx_func_record_needed_fndecl (MtcsPtxFunc *self,tree decl)
{
  n_debug("mtcsptxfunc.c -----nvptx.cc -----21-- void nvptx_record_needed_fndecl (tree decl)\n");
  MtcsFunc *mtcsFunc=(MtcsFunc *)self;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsPtx *mtcsPtx=(MtcsPtx *)mtcsTarget;

  MtcsPtxFunc *mtcsPtxFunc=(MtcsPtxFunc *)mtcs_target_get_func(mtcsTarget);
  if (TYPE_ARG_TYPES (TREE_TYPE (decl)) == NULL_TREE){
      /*
      tree *slot = self->needed_fndecls_htab->find_slot (decl, INSERT);
      if (*slot == NULL)
          *slot = decl;
      */
      tree slot=n_hash_table_lookup(self->needed_fndecls_htab,decl);
      if(!validTree(slot)){
          n_hash_table_insert(self->needed_fndecls_htab,decl,decl);
      }
  }else
     mtcs_ptx_func_record_fndecl/*!nvptx_record_fndecl*/(self,decl);
}

/* SYM is a SYMBOL_REF.  If it refers to an external function, record
   it as needed.  */
//原型static void nvptx_maybe_record_fnsym
void mtcs_ptx_func_maybe_record_fnsym  (MtcsPtxFunc *self,rtx sym)
{
  n_debug("mtcsptxfunc.c -----nvptx.cc -----22-- void nvptx_maybe_record_fnsym (rtx sym)\n");
  tree decl = SYMBOL_REF_DECL (sym);
  if (decl && TREE_CODE (decl) == FUNCTION_DECL && DECL_EXTERNAL (decl))
     mtcs_ptx_func_record_needed_fndecl/*!nvptx_record_needed_fndecl*/(self,decl);
}

/**
 * 当完成汇编文件时
 * 原型 static void nvptx_file_end (void) 开始部分
 * hash_table<tree_hasher>::iterator iter;
    tree decl;
    FOR_EACH_HASH_TABLE_ELEMENT (*needed_fndecls_htab, decl, tree, iter)
       nvptx_record_fndecl (decl);
 */
void mtcs_ptx_func_file_end(MtcsPtxFunc *self)
{
     NHashTableIter iter;
     npointer key, value;
     n_hash_table_iter_init(&iter, self->needed_fndecls_htab);
     while (n_hash_table_iter_next(&iter, &key, &value)) {
        tree decl = (tree)value;
        mtcs_ptx_func_record_fndecl(self,decl);
    }
}


MtcsPtxFunc *mtcs_ptx_func_new(MtcsMode *mtcsMode)
{
     MtcsPtxFunc *self = n_slice_alloc0 (sizeof(MtcsPtxFunc));
     mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
     mtcs_func_init((MtcsFunc *)self);
     mtcsPtxFuncInit(self);
     return self;
}
