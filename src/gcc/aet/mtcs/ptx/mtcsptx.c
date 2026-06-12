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

#define IN_TARGET_CODE 1 //不加这句在machmode.h中的GET_MODE_SIZE编译到poly_uint16(poly_int) 因为poly_int没有重载>号，所以编译报错
//insn-modes.h由nvptx生成，但i386生成的类型全覆盖nvptx的insn-modes.h,不需要平台的？？？
#include "config.h"
#define INCLUDE_ALGORITHM /* reverse */
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "tree-pass.h"
#include "cfghooks.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "insn-config.h"
#include "regs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "cgraph.h"
#include "tree-pretty-print.h" /* for dump_function_header */
#include "varasm.h"
#include "insn-attr.h"
#include "conditions.h"
#include "flags.h"
#include "output.h"
#include "except.h"
#include "rtl-error.h"
#include "toplev.h" /* exact_log2, floor_log2 */
#include "reload.h"
#include "intl.h"
#include "cfgrtl.h"
#include "debug.h"
#include "tree-ssa.h"
#include "cfgloop.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "rtl-iter.h"
#include "print-rtl.h"
#include "function-abi.h"
#include "common/common-target.h"
#include "diagnostic.h"
#include "context.h"
#include "options.h"
#include "machmode.h"
#include "poly-int-types.h"
#include "opts.h"
#include "targhooks.h"
#include "calls.h"
#include "langhooks.h"
#include "dwarf2out.h"
#include "symbol-summary.h"
#include "sreal.h"
#include "value-range.h"
#include "ipa-cp.h"
#include "ipa-prop.h"
#include "ipa-fnsummary.h"

#include "gimple.h"
#include "alloc-pool.h"
#include "ssa.h"
#include "coverage.h"
#include "gimple-pretty-print.h"
#include "data-streamer.h"
#include "tree-streamer.h"
#include "fold-const.h"
#include "gimple-iterator.h"
#include "tree-cfg.h"
#include "print-tree.h"
#include "ipa-utils.h"
#include "tree-ssa-alias-compare.h"
#include "ipa-icf-gimple.h"
#include "fibonacci_heap.h"
#include "stor-layout.h"
#include "dbgcnt.h"
#include "tree-vector-builder.h"
#include "symtab-thunks.h"
#include "alias.h"
#include "ipa-modref-tree.h"
#include "ipa-modref.h"
#include "target-def.h"
#include "gomp-constants.h"
#include "omp-general.h"


#include "aet/aetprinttree.h"
#include "../mtcstool.h"
#include "../mtcsargs.h"
#include "../rtl/mtcsrtlpassmgr.h"
#include "../rtl/mtcsloopinit.h"
#include "../rtl/mtcsweb.h"
#include "../rtl/mtcscombine.h"
#include "../rtl/mtcsifcvt.h"
#include "../rtl/mtcspostreload.h"
#include "../rtl/mtcsloopinvariant.h"
#include "../rtl/mtcsloopunroll.h"
#include "../rtl/mtcsextdce.h"

#include "gen/ptx-insn-modes.h"
#include "gen/ptx-insn-flags.h"
#include "gen/ptx-optionsitem.h"
#include "gen/ptx-insn-opinit.h"
#include "gen/ptx-insn-codes.h"
#include "ptx-common.h"
#include "ptxtool.h"
#include "mtcsptxmode.h"
#include "mtcsptxreg.h"
#include "mtcsptxoptions.h"
#include "mtcsptxrecog.h"
#include "mtcsptxpreds.h"
#include "mtcsptxalign.h"
#include "mtcsptxfunc.h"
#include "mtcsptxopinit.h"
#include "mtcsptxemit.h"
#include "mtcsptxreal.h"
#include "mtcsptxoutput.h"
#include "mtcsptxargs.h"
#include "mtcsptxcodes.h"
#include "mtcsptxconfig.h"
#include "mtcsptxbuiltins.h"
#include "mtcsptxtree.h"
#include "mtcsptxattribs.h"
#include "mtcsptxunspec.h"
#include "mtcsptxinsnattr.h"
#include "mtcsptxrtl.h"
#include "mtcsptxasm.h"
#include "mtcsptxinternalfn.h"
#include "targetptxvectorize.h"
#include "targetptxaddrspace.h"
#include "targetptxoption.h"
#include "targetptxcommon.h"
#include "targetptxasmout.h"
#include "targetptxcalls.h"
#include "targetptxrtx.h"

#include "mtcsptx.h"
#include "../mtcsprintrtl.h"
#include "gen/ptx-insn-unspec.h"
#include "../mtcsdfcore.h"
#include "../mtcsdfproblems.h"
#include "../mtcsmicro.h"
#include "../../mtcsinfo.h"


int test_abort =0;//用来测试的


//记录疑问
//mtcs_mode_get_type/*!nvptx_ptx_type_from_mode*/ machine mode 与nptx值不一样，因为一个是主机的machine mode，另外一个是nvptx的

struct declared_libfunc_hasher : ggc_cache_ptr_hash<rtx_def>
{
  static hashval_t hash (rtx x) { return htab_hash_pointer (x); }
  static bool equal (rtx a, rtx b) { return a == b; }
};

static GTY((cache))  hash_table<declared_libfunc_hasher> *declared_libfuncs_htab;


#define PTX_ASM_APP_ON "\t// #APP \n"
#define PTX_ASM_APP_OFF "\t// #NO_APP \n"

#define PTX_CTA_SIZE 1024

/* If MODE should be treated as two registers of an inner mode, return
   that inner mode.  Otherwise return VOIDmode.  */
nuint  mtcs_ptx_maybe_split_mode ( MtcsPtx *self,mtcs_mode mode)
{
    MtcsMode *mtcsMode=((MtcsTarget*)self)->mtcsMode;
    if (mtcs_mode_is_complex_p (mtcsMode,mode))
      return mtcs_mode_get_inner/*GET_MODE_INNER */(mtcsMode,mode);
    if (mode == mtcsMode->modes.M_TImode)
      return mtcsMode->modes.M_DImode;
    return mtcsMode->modes.M_VOIDmode;
}

/* Return true if mode should be treated as two registers.  */
bool mtcs_ptx_split_mode_p (MtcsPtx *self,machine_mode mode)
{
  return mtcs_ptx_maybe_split_mode (self,mode) != VOIDmode;
}

/* Determine whether MODE and TYPE (possibly NULL) should be passed or
   returned in memory.  Integer and floating types supported by the
   machine are passed in registers, everything else is passed in
   memory.  Complex types are split.  */
bool mtcs_ptx_pass_in_memory (MtcsPtx *self,mtcs_mode  mode, const_tree type, bool for_return)
{
   MtcsMode *mtcsMode=((MtcsTarget *)self)->mtcsMode;
   if (type){
      n_debug("mtcsptx.c nvptx.cc pass_in_memory 00 %d %p %d\n",mode,type,for_return);
      if (AGGREGATE_TYPE_P (type))
         return true;
      n_debug("mtcsptx.c nvptx.cc pass_in_memory 11 %d %p %d\n",mode,type,for_return);
      if (VECTOR_TYPE_P (type))
         return true;
   }

   if (!for_return && mtcs_mode_is_complex_p/*COMPLEX_MODE_P*/ (mtcsMode,mode)){
      /* Complex types are passed as two underlying args.  */
      n_debug("mtcsptx.c nvptx.cc pass_in_memory 22 COMPLEX_MODE_P (mode) %d %p %d\n",mode,type,for_return);
      mode = mtcs_mode_get_inner(mtcsMode,mode)/*GET_MODE_INNER (mode)*/;
   }

   if (mtcs_mode_get_class/*GET_MODE_CLASS*/ (mtcsMode,mode) != MODE_INT
   && mtcs_mode_get_class/*GET_MODE_CLASS*/ (mtcsMode,mode) != MODE_FLOAT){
      n_debug("mtcsptx.c nvptx.cc pass_in_memory 33 GET_MODE_CLASS (mode) != MODE_INT %d %p %d\n",mode,type,for_return);
      return true;
   }

   if (mtcs_mode_get_size/*GET_MODE_SIZE */(mtcsMode,mode) > PTX_UNITS_PER_WORD){
      n_debug("mtcsptx.c nvptx.cc pass_in_memory 44 GET_MODE_CLASS (mode) != MODE_INT %d %p %d %d\n",
      mode,type,for_return,mtcs_mode_get_size/*GET_MODE_SIZE */(mtcsMode,mode));
      return true;
   }
   n_debug("mtcsptx.c nvptx.cc pass_in_memory 55 返回false %d %p %d\n",mode,type,for_return);
   return false;
}

/* Offloading function attributes.  */

struct offload_attrs
{
  unsigned mask;
  int num_gangs;
  int num_workers;
  int vector_length;
};

/* Define entries for cfun->machine->axis_dim.  */

#define MACH_VECTOR_LENGTH 0
#define MACH_MAX_WORKERS 1

static void populate_offload_attrs (offload_attrs *oa);

static void init_axis_dim (MtcsPtx *self)
{
  MtcsTarget *mtcsTarget=(MtcsTarget *)self;
  MtcsMode *mtcsMode=mtcs_target_get_mode(mtcsTarget);
  MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
 // struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)mtcsFunc->machine;
  struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)cfun->machine;

  offload_attrs oa;
  int max_workers;

  populate_offload_attrs (&oa);

  if (oa.num_workers == 0)
    max_workers = PTX_CTA_SIZE / oa.vector_length;
  else
    max_workers = oa.num_workers;

  nvptxMachine->axis_dim[MACH_VECTOR_LENGTH] = oa.vector_length;
  nvptxMachine->axis_dim[MACH_MAX_WORKERS] = max_workers;
  nvptxMachine->axis_dim_init_p = true;
}

static void populate_offload_attrs (offload_attrs *oa)
{
  tree attr = oacc_get_fn_attrib (current_function_decl);
  tree dims = TREE_VALUE (attr);
  unsigned ix;

  oa->mask = 0;

  for (ix = 0; ix != GOMP_DIM_MAX; ix++, dims = TREE_CHAIN (dims)){
      tree t = TREE_VALUE (dims);
      int size = (t == NULL_TREE) ? -1 : TREE_INT_CST_LOW (t);
      tree allowed = TREE_PURPOSE (dims);

      if (size != 1 && !(allowed && integer_zerop (allowed)))
    oa->mask |= GOMP_DIM_MASK (ix);

      switch (ix){
    case GOMP_DIM_GANG:
      oa->num_gangs = size;
      break;

    case GOMP_DIM_WORKER:
      oa->num_workers = size;
      break;

    case GOMP_DIM_VECTOR:
      oa->vector_length = size;
      break;
    }
    }
}


//原型 nvptx_mach_max_workers nvptx.cc
int   mtcs_ptx_get_mach_max_workers(MtcsPtx *self)
{
    MtcsTarget *mtcsTarget=(MtcsTarget *)self;
    MtcsMode *mtcsMode=mtcs_target_get_mode(mtcsTarget);
    MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
    //struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)mtcsFunc->machine;
    struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)cfun->machine;

    //  if (!cfun->machine->axis_dim_init_p)
    //    init_axis_dim ();
    //  return cfun->machine->axis_dim[MACH_MAX_WORKERS];

    if (!nvptxMachine->axis_dim_init_p)
        init_axis_dim (self);
    return nvptxMachine->axis_dim[MACH_MAX_WORKERS];
}

static int ATTRIBUTE_UNUSED nvptx_mach_vector_length (MtcsPtx *self)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)self;
   MtcsMode *mtcsMode=mtcs_target_get_mode(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)cfun->machine;
   //  if (!cfun->machine->axis_dim_init_p)
   //    init_axis_dim ();
   //  return cfun->machine->axis_dim[MACH_VECTOR_LENGTH];
   if (!nvptxMachine->axis_dim_init_p)
      init_axis_dim (self);
   return nvptxMachine->axis_dim[MACH_VECTOR_LENGTH];
}

#define MASK_ABI64 (1U << 0)
#define MASK_GOMP (1U << 1)
#define MASK_SOFT_STACK (1U << 2)
#define MASK_UNIFORM_SIMT (1U << 3)


//原型 #define FUNCTION_PROFILER(FILE, LABELNO)
static void functionProfiler_cb(MtcsTarget *target,int labelno)
{
    fatal_error (input_location,"profiling is not yet implemented for this architecture");
}


/* Return true if TYPE is a record type where the last field is an array without
   given dimension.  */
static bool flexible_array_member_type_p (const_tree type)
{
  if (TREE_CODE (type) != RECORD_TYPE)
    return false;

  const_tree last_field = NULL_TREE;
  for (const_tree f = TYPE_FIELDS (type); f; f = TREE_CHAIN (f))
    last_field = f;

  if (!last_field)
    return false;

  const_tree last_field_type = TREE_TYPE (last_field);
  if (TREE_CODE (last_field_type) != ARRAY_TYPE)
    return false;

  return (! TYPE_DOMAIN (last_field_type) || ! TYPE_MAX_VALUE (TYPE_DOMAIN (last_field_type)));
}

//原型 insnify insn-target-def.h
static inline rtx_insn *insnify (MtcsTarget *mtcsTarget,rtx x)
{
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
  MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
  if (!x)
    return NULL;
  if (rtx_insn *insn = dyn_cast <rtx_insn *> (x))
    return insn;
  mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);

  enum rtx_code code = classify_insn (x);
  n_debug("mtcsptx.c insnify ptx 00 ---- x:%p %d\n",x,code);
  if(code==CODE_LABEL){
     n_debug("mtcsptx.c insnify ptx 11 ---- x:%p %d CODE_LABEL\n",x,code);
  }else if(code==JUMP_INSN){
     n_debug("mtcsptx.c insnify ptx 22---- x:%p %d JUMP_INSN code:%d %s\n",x,code,GET_CODE(x),GET_RTX_NAME(GET_CODE(x)));
  }
  mtcs_emit_emit/*!emit*/(mtcsEmit,x, false);
  rtx_insn *res = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
  mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
  n_debug("mtcsptx.c insnify ptx 33 --ttt-- x:%p %p JUMP_INSN res uid:%d\n",x,res,INSN_UID(res));

  return res;
}


///* The various PTX memory areas an object might reside in.  */
//enum nvptx_data_area
//{
//  DATA_AREA_GENERIC,
//  DATA_AREA_GLOBAL,
//  DATA_AREA_SHARED,
//  DATA_AREA_LOCAL,
//  DATA_AREA_CONST,
//  DATA_AREA_PARAM,
//  DATA_AREA_MAX
//};
//
///*  We record the data area in the target symbol flags.  */
//#define SYMBOL_DATA_AREA(SYM) \
//  (nvptx_data_area)((SYMBOL_REF_FLAGS (SYM) >> SYMBOL_FLAG_MACH_DEP_SHIFT) & 7)
//
//#define SET_SYMBOL_DATA_AREA(SYM,AREA) \
//  (SYMBOL_REF_FLAGS (SYM) |= (AREA) << SYMBOL_FLAG_MACH_DEP_SHIFT)

/* Return the PTX name of the data area in which SYM should be
   placed.  The symbol must have already been processed by
   nvptx_encode_seciton_info, or equivalent.  */

static const char * section_for_sym (rtx sym)
{
  ptx_data_area area = PTX_SYMBOL_DATA_AREA (sym);
  /* Same order as nvptx_data_area enum.  */
  static char const *const areas[] =
    {"", ".global", ".shared", ".local", ".const", ".param"};

  return areas[area];
}

/* Similarly for a decl.  */
static const char * section_for_decl (MtcsPtx *self,const_tree decl)
{
   MtcsTarget *target=(MtcsTarget *)self;
   MtcsAsm *mtcsAsm=(MtcsAsm *)target->mtcsAsm;

   static char *managed = ".global .attribute(.managed)";
   const char *ret= section_for_sym (XEXP (mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,CONST_CAST (tree, decl)), 0));
   if(VAR_P(decl)){
      MtcsVarType varType = mtcs_info_get_var_type(decl);
      if(varType == MTCS_VAR_MANAGED && !strcmp(ret,".global"))
         return managed;
   }
   return ret;
}



//#define DATA_ALIGNMENT nvptx_data_alignment
static unsigned int dataAlignment_cb(MtcsTarget *target,const_tree type, unsigned int basic_align)
{
    MtcsPtx *self=(MtcsPtx *)target;
    MtcsMode *mtcsMode =target->mtcsMode;
    n_debug("mtcsptx.c -----nvptx.cc -----91-- nvptx_data_alignment\n");
    if (TREE_CODE (type) == INTEGER_TYPE){
        unsigned HOST_WIDE_INT size = tree_to_uhwi (TYPE_SIZE_UNIT (type));
        mtcs_mode timode=mtcsMode->modes.M_TImode;//mtcs_mode_host2device(mtcsMode,TImode);
        if (size ==mtcs_mode_get_size/*GET_MODE_SIZE*/ (mtcsMode,timode))
            return mtcs_mode_get_bitsize/*GET_MODE_BITSIZE*/ (mtcsMode,mtcs_ptx_maybe_split_mode (self,timode));
    }
    return basic_align;
}

#define STRICT_ALIGNMENT 1
//原型ttargetm.slow_unaligned_access (DECL_MODE (decl),DECL_ALIGN (decl)) #define TARGET_SLOW_UNALIGNED_ACCESS default_slow_unaligned_access
static bool slowUnalignedAccess_cb (MtcsTarget *self,machine_mode mode, unsigned int align)
{
  return STRICT_ALIGNMENT;
}

static void handle_ptx_version_option ( MtcsPtx *self)
{
    MtcsTarget *target=(MtcsTarget *)self;
    int     isa=mtcs_target_get_isa(target);
    int     version=mtcs_target_get_version(target);
    nboolean valid=ptx_tool_valid_isa_version ((PtxIsa)isa,(PtxVersion)version);
    if(!valid){
       PtxVersion first =ptx_tool_get_first_version_supporting_sm ((PtxIsa)isa);
       char *isaStr=ptx_tool_sm_version_to_string ((PtxIsa)isa);
       char *versionStr=ptx_tool_version_to_string ((PtxVersion)version);
       char *firstStr=ptx_tool_version_to_string ((PtxVersion)first);
       error("为了支持架构 sm_%s，PTX 版本至少需要%s，当前设置的是:%s",isaStr,firstStr,versionStr);
    }
}

static int callCount=0;
//原型 targetm.hard_regno_nregs (i, (machine_mode) j); #define TARGET_HARD_REGNO_NREGS nvptx_hard_regno_nregs
static unsigned int hardRegnoNregs_cb(MtcsTarget *self,unsigned int num, machine_mode mode)
{
    if(callCount==0)
       n_debug("mtcsptx.c  -----nvptx.cc -----72-- TARGET_HARD_REGNO_NREGS\
               unsigned int nvptx_hard_regno_nregs num:%d mode:%d callCount:%d\n",num,mode,callCount);
     callCount++;
     return 1;
}

//原型 targetm.hard_regno_mode_ok (regno, mode) #define TARGET_HARD_REGNO_MODE_OK hook_bool_uint_mode_true
static bool hardRegnoModeOk_cb (MtcsTarget *self,unsigned int reg, machine_mode mode)
{
  return true;
}

//原型 targetm.scalar_mode_supported_p (TImode)) #define TARGET_SCALAR_MODE_SUPPORTED_P nvptx_scalar_mode_supported_p
//函数体来自  default_scalar_mode_supported_p (mode);
static bool scalarModeSupportedP_cb (MtcsTarget *mtcsTarget,mtcs_mode mode)
{
    MtcsMode *mtcsMode=mtcsTarget->mtcsMode;
    MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
    PtxOptionsItem *ptxOptionsItem=(PtxOptionsItem *)mtcsOptions->global_options;

     n_debug("mtcsptx.c -----nvptx.cc -----68-- TARGET_SCALAR_MODE_SUPPORTED_P bool nvptx_scalar_mode_supported_p (scalar_mode mode) %d\n",mode);
     if (ptxOptionsItem->x_nvptx_experimental && mode ==PTX_HFmode && TARGET_SM53)
         return true;
     // int precision = GET_MODE_PRECISION (mode);
      int precision =mtcs_mode_get_precision(mtcsMode,mode);
      unsigned char modeClass=mtcs_mode_get_class(mtcsMode,mode);
      switch (modeClass){
        case MODE_PARTIAL_INT:
        case MODE_INT:
          if (precision == CHAR_TYPE_SIZE)
              return true;
          if (precision == PTX_SHORT_TYPE_SIZE)
              return true;
          if (precision == PTX_INT_TYPE_SIZE)
              return true;
          if (precision == PTX_LONG_TYPE_SIZE)
              return true;
          if (precision == PTX_LONG_LONG_TYPE_SIZE)
              return true;
          if (precision == 2 * PTX_BITS_PER_WORD)
              return true;
          return false;
        case MODE_FLOAT:
          if (precision == PTX_FLOAT_TYPE_SIZE)
              return true;
          if (precision == PTX_DOUBLE_TYPE_SIZE)
              return true;
          if (precision == PTX_LONG_DOUBLE_TYPE_SIZE)
              return true;
          return false;

        case MODE_DECIMAL_FLOAT:
        case MODE_FRACT:
        case MODE_UFRACT:
        case MODE_ACCUM:
        case MODE_UACCUM:
          return false;

        default:
          gcc_unreachable ();
    }
    return false;
}

//原型 targetm.libgcc_floating_mode_supported_p (mode) #define TARGET_LIBGCC_FLOATING_MODE_SUPPORTED_P
static bool libgccFloatingModeSupportedP_cb (MtcsTarget *mtcsTarget,scalar_mode mode)
{
    MtcsMode *mtcsMode=mtcsTarget->mtcsMode;
    MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
    PtxOptionsItem *ptxOptionsItem=(PtxOptionsItem *)mtcsOptions->global_options;
    n_debug("mtcsptx.c -----nvptx.cc -----79-- TARGET_LIBGCC_FLOATING_MODE_SUPPORTED_P bool nvptx_libgcc_floating_mode_supported_p %d %d %d %d\n",
          mode,PTX_HFmode,ptxOptionsItem->x_nvptx_experimental,TARGET_SM53);
    if (ptxOptionsItem->x_nvptx_experimental && mode ==PTX_HFmode && TARGET_SM53)
        return true;
    return mtcs_ptx_mode_libgcc_floating_mode_supported_p((MtcsPtxMode *)mtcsMode,mode);
}


/* Set up all builtin functions for this target.  */
//原型 targetm.init_builtins ();#define TARGET_INIT_BUILTINS nvptx_init_builtins
static void initBuiltins_cb (MtcsTarget *mtcsTarget)
{
    MtcsPtx *self=(MtcsPtx *)mtcsTarget;
    MtcsPtxBuiltins *mtcsPtxBuiltins=(MtcsPtxBuiltins *)mtcs_target_get_builtins(mtcsTarget);
    n_debug("mtcsptx.c -----nvptx.cc -----61-- 需要在编译c源代码时就创建TARGET_INIT_BUILTINS void nvptx_init_builtins (void)\n");
    mtcs_ptx_builtins_init_builtins(mtcsPtxBuiltins);
}

/* Implement TARGET_MODES_TIEABLE_P.  */
//原型targetm.modes_tieable_p  #define TARGET_MODES_TIEABLE_P hook_bool_mode_mode_true
static bool modesTieableP_cb (MtcsTarget *mtcsTarget,machine_mode m1, machine_mode m2)
{
  n_debug("mtcsptx.c -----nvptx.cc -----71-- TARGET_MODES_TIEABLE_P \
          bool nvptx_modes_tieable_p (machine_mode, machine_mode) m1:%d m2:%d\n",m1,m2);
  return false;
}

//原型 targetm.vector_alignment #define TARGET_VECTOR_ALIGNMENT nvptx_vector_alignment
static HOST_WIDE_INT vectorAlignment_cb(MtcsTarget *mtcsTarget,const_tree type)
{
   MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
   return mtcs_align_get_vector_alignment(mtcsAlign,type);
}


typedef hash_map<basic_block, rtx_insn *> bb_insn_map_t;
typedef std::pair<rtx_insn *, basic_block> insn_bb_t;
typedef auto_vec<insn_bb_t> insn_bb_vec_t;

/* Split basic blocks such that each forked and join unspecs are at
   the start of their basic blocks.  Thus afterwards each block will
   have a single partitioning mode.  We also do the same for return
   insns, as they are executed by every thread.  Return the
   partitioning mode of the function as a whole.  Populate MAP with
   head and tail blocks.  We also clear the BB visited flag, which is
   used when finding partitions.  */
/* See also 'gcc/omp-oacc-neuter-broadcast.cc:omp_sese_split_blocks'.  */
static void nvptx_split_blocks (MtcsPtx *self,bb_insn_map_t *map)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)self;
   MtcsMode *mtcsMode=mtcsTarget->mtcsMode;
   MtcsCfgContext *mtcsCfgContext=mtcs_target_get_cfg_context(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   n_debug("mtcsptx.c -----nvptx.cc -----167-- nvptx_split_blocks\n");

   insn_bb_vec_t worklist;
   basic_block block;
   rtx_insn *insn;

   /* Locate all the reorg instructions of interest.  */
   FOR_ALL_BB_FN (block, cfun){
      bool seen_insn = false;

      /* Clear visited flag, for use by parallel locator  */
      block->flags &= ~BB_VISITED;

      FOR_BB_INSNS (block, insn){
         if (!INSN_P (insn))
            continue;
         switch (mtcs_recog_recog_memoized/*!recog_memoized*/(mtcsRecog,insn)){
            default:
               seen_insn = true;
               continue;
            case PTX_CODE_FOR_nvptx_forked:
            case PTX_CODE_FOR_nvptx_join:
               break;

            case CODE_FOR_return:
               /* We also need to split just before return insns, as
               that insn needs executing by all threads, but the
               block it is in probably does not.  */
               break;
         }

         if (seen_insn)
            /* We've found an instruction that  must be at the start of
            a block, but isn't.  Add it to the worklist.  */
            worklist.safe_push (insn_bb_t (insn, block));
         else
            /* It was already the first instruction.  Just add it to
            the map.  */
            map->get_or_insert (block) = insn;
         seen_insn = true;
      }
   }

   /* Split blocks on the worklist.  */
   unsigned ix;
   insn_bb_t *elt;
   basic_block remap = 0;
   for (ix = 0; worklist.iterate (ix, &elt); ix++){
      if (remap != elt->second) {
         block = elt->second;
         remap = block;
      }

      /* Split block before insn. The insn is in the new block  */
      edge e = mtcs_cfg_context_split_block/*!split_block*/(mtcsCfgContext,block, PREV_INSN (elt->first));

      block = e->dest;
      map->get_or_insert (block) = elt->first;
   }
}

static rtx gen_comment (MtcsPtx *self,const char *s)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)self;
   MtcsMode *mtcsMode=mtcsTarget->mtcsMode;
   MtcsAsm   *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

   n_debug("mtcsptx.c -----nvptx.cc -----111--static rtx gen_comment (const char *s)\n");
   char *asmCommentStart = mtcs_asm_get_comment_start/*!ASM_COMMENT_START*/(mtcsAsm);
   const char *sep = " ";
   size_t len = strlen (asmCommentStart/*!ASM_COMMENT_START*/) + strlen (sep) + strlen (s) + 1;
   char *comment = (char *) alloca (len);
   snprintf (comment, len, "%s%s%s", asmCommentStart/*!ASM_COMMENT_START*/, sep, s);
   return gen_rtx_ASM_INPUT_loc (VOIDmode, ggc_strdup (comment),DECL_SOURCE_LOCATION (cfun->decl));
}

/* Initialize all declared regs at function entry.
   Advantage   : Fool-proof.
   Disadvantage: Potentially creates a lot of long live ranges and adds a lot
         of insns.  */
static void workaround_uninit_method_1 (MtcsPtx *self)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)self;
   MtcsMode *mtcsMode=mtcsTarget->mtcsMode;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   PtxOptionsItem *ptxOptionsItem=(PtxOptionsItem *)mtcsOptionsItem;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr *mtcsExpr = mtcs_target_get_expr(mtcsTarget);

   n_debug("mtcsptx.c -----nvptx.cc -----141-- workaround_uninit_method_1\n");

   rtx_insn *first =  mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
   rtx_insn *insert_here = NULL;

   for (int ix = mtcs_reg_get_last_virtual_regno/*!LAST_VIRTUAL_REGISTER*/(mtcsReg) + 1; ix < max_reg_num (); ix++){
      rtx reg = mtcsRtlData->regno_reg_rtx/*!regno_reg_rtx*/[ix];

      /* Skip undeclared registers.  */
      if (reg == const0_rtx)
         continue;

      gcc_assert (CONST0_RTX (GET_MODE (reg)));

      mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
      if (ptxOptionsItem->x_nvptx_comment && first != NULL)
         mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,gen_comment(self,"Start: Added by -minit-regs=1"));
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,reg, CONST0_RTX (GET_MODE (reg)));
      rtx_insn *inits = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);

      if (dump_file && (dump_flags & TDF_DETAILS))
         for (rtx_insn *init = inits; init != NULL; init = NEXT_INSN (init))
            fprintf (dump_file, "Default init of reg %u inserted: insn %u\n", ix, INSN_UID (init));

      if (first != NULL){
         insert_here = mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,inits, first);
         first = NULL;
      }else
         insert_here =mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,inits, insert_here);
   }

   if (ptxOptionsItem->x_nvptx_comment && insert_here != NULL)
      mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,gen_comment(self,"End: Added by -minit-regs=1"), insert_here);
}

/* Find uses of regs that are not defined on all incoming paths, and insert a
   corresponding def at function entry.
   Advantage   : Simple.
   Disadvantage: Potentially creates long live ranges.
         May not catch all cases.  F.i. a clobber cuts a live range in
         the compiler and may prevent entry_lr_in from being set for a
         reg, but the clobber does not translate to a ptx insn, so in
         ptx there still may be an uninitialized ptx reg.  See f.i.
         gcc.c-torture/compile/20020926-1.c.  */
static void workaround_uninit_method_2 (MtcsPtx *self)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)self;
   MtcsMode *mtcsMode=mtcsTarget->mtcsMode;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   PtxOptionsItem *ptxOptionsItem=(PtxOptionsItem *)mtcsOptionsItem;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr *mtcsExpr = mtcs_target_get_expr(mtcsTarget);

   n_debug("mtcsptx.c -----nvptx.cc -----140-- workaround_uninit_method_2\n");
   auto_bitmap entry_pseudo_uninit;
   {
      auto_bitmap not_pseudo;
      bitmap_set_range (not_pseudo, 0, mtcs_reg_get_last_virtual_regno/*!LAST_VIRTUAL_REGISTER*/(mtcsReg));

      bitmap entry_lr_in = DF_LR_IN (ENTRY_BLOCK_PTR_FOR_FN (cfun));
      bitmap_and_compl (entry_pseudo_uninit, entry_lr_in, not_pseudo);
   }

   rtx_insn *first =  mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
   rtx_insn *insert_here = NULL;

   bitmap_iterator iterator;
   unsigned ix;
   EXECUTE_IF_SET_IN_BITMAP (entry_pseudo_uninit, 0, ix, iterator){
      rtx reg = mtcsRtlData->regno_reg_rtx/*!regno_reg_rtx*/[ix];
      gcc_assert (CONST0_RTX (GET_MODE (reg)));

      mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
      if (ptxOptionsItem->x_nvptx_comment && first != NULL)
         mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,gen_comment(self,"Start: Added by -minit-regs=2:"));
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,reg, CONST0_RTX (GET_MODE (reg)));
      rtx_insn *inits =  mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);

      if (dump_file && (dump_flags & TDF_DETAILS))
         for (rtx_insn *init = inits; init != NULL; init = NEXT_INSN (init))
            fprintf (dump_file, "Missing init of reg %u inserted: insn %u\n", ix, INSN_UID (init));

      if (first != NULL){
         insert_here = mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,inits, first);
         first = NULL;
      }else
         insert_here = mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,inits, insert_here);
   }

   if (ptxOptionsItem->x_nvptx_comment && insert_here != NULL)
      mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,gen_comment(self,"End: Added by -minit-regs=2"), insert_here);
}

/* Find uses of regs that are not defined on all incoming paths, and insert a
   corresponding def on those.
   Advantage   : Doesn't create long live ranges.
   Disadvantage: More complex, and potentially also more defs.  */

static void workaround_uninit_method_3 (MtcsPtx *self)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)self;
   MtcsMode *mtcsMode=mtcsTarget->mtcsMode;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   PtxOptionsItem *ptxOptionsItem=(PtxOptionsItem *)mtcsOptionsItem;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExpr *mtcsExpr = mtcs_target_get_expr(mtcsTarget);

   n_debug("mtcsptx.c -----nvptx.cc -----139-- workaround_uninit_method_3\n");

   auto_bitmap not_pseudo;
   bitmap_set_range (not_pseudo, 0, mtcs_reg_get_last_virtual_regno/*!LAST_VIRTUAL_REGISTER*/(mtcsReg));

   basic_block bb;
   FOR_EACH_BB_FN (bb, cfun){
      if (single_pred_p (bb))
         continue;

      auto_bitmap bb_pseudo_uninit;
      bitmap_and_compl (bb_pseudo_uninit, DF_LIVE_IN (bb), DF_MIR_IN (bb));
      bitmap_and_compl_into (bb_pseudo_uninit, not_pseudo);

      bitmap_iterator iterator;
      unsigned ix;
      EXECUTE_IF_SET_IN_BITMAP (bb_pseudo_uninit, 0, ix, iterator){
         bool have_false = false;
         bool have_true = false;

         edge e;
         edge_iterator ei;
         FOR_EACH_EDGE (e, ei, bb->preds) {
            if (bitmap_bit_p (DF_LIVE_OUT (e->src), ix))
               have_true = true;
            else
               have_false = true;
         }
         if (have_false ^ have_true)
            continue;

         FOR_EACH_EDGE (e, ei, bb->preds){
            if (bitmap_bit_p (DF_LIVE_OUT (e->src), ix))
               continue;

            rtx reg = mtcsRtlData->regno_reg_rtx/*!regno_reg_rtx*/[ix];
            gcc_assert (CONST0_RTX (GET_MODE (reg)));
            n_debug("mtcsptx.c -----nvptx.cc -----139aa-- workaround_uninit_method_3\n");

            mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,reg, CONST0_RTX (GET_MODE (reg)));
            rtx_insn *inits =  mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
            mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
            n_debug("mtcsptx.c -----nvptx.cc -----139bb-- workaround_uninit_method_3\n");

            if (dump_file && (dump_flags & TDF_DETAILS))
               for (rtx_insn *init = inits; init != NULL; init = NEXT_INSN (init))
                  fprintf (dump_file, "Missing init of reg %u inserted on edge: %d -> %d: insn %u\n",
                     ix, e->src->index, e->dest->index,INSN_UID (init));

            mtcs_cfg_rtl_insert_insn_on_edge/*!insert_insn_on_edge*/(mtcsCfgRtl,inits, e);
            n_debug("mtcsptx.c -----nvptx.cc -----139cc-- workaround_uninit_method_3\n");

         }
      }
   }

   if (ptxOptionsItem->x_nvptx_comment)
      FOR_EACH_BB_FN (bb, cfun){
         if (single_pred_p (bb))
            continue;

         edge e;
         edge_iterator ei;
         FOR_EACH_EDGE (e, ei, bb->preds){
            if (e->insns.r == NULL_RTX)
               continue;
            n_debug("mtcsptx.c -----nvptx.cc -----139dd-- workaround_uninit_method_3\n");

            mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
            mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,gen_comment(self,"Start: Added by -minit-regs=3:"));
            mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,e->insns.r);
            mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,gen_comment(self,"End: Added by -minit-regs=3:"));
            e->insns.r =  mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
            mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
         }
      }
   n_debug("mtcsptx.c -----nvptx.cc -----139ee-- workaround_uninit_method_3\n");

   mtcs_cfg_rtl_commit_edge_insertions/*!commit_edge_insertions*/(mtcsCfgRtl);
}

static void workaround_uninit (MtcsPtx *self)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)self;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   PtxOptionsItem *ptxOptionsItem=(PtxOptionsItem *)mtcsOptionsItem;

   n_debug("mtcsptx.c -----nvptx.cc -----138-- workaround_uninit x_nvptx_init_regs:%d\n",ptxOptionsItem->x_nvptx_init_regs);

   switch (ptxOptionsItem->x_nvptx_init_regs){
      case 0:
         /* Skip.  */
         break;
      case 1:
         workaround_uninit_method_1(self);
         break;
      case 2:
         workaround_uninit_method_2(self);
         break;
      case 3:
         workaround_uninit_method_3(self);
         break;
      default:
         gcc_unreachable ();
   }
}

/* Record replacement regs used to deal with subreg operands.  */
struct reg_replace
{
  rtx replacement[MAX_MAX_RECOG_OPERANDS/*!MAX_RECOG_OPERANDS*/];
  machine_mode mode;
  int n_allocated;
  int n_in_use;
};

/* Allocate or reuse a replacement in R and return the rtx.  */
static rtx get_replacement (MtcsPtx *self,struct reg_replace *r)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)self;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   n_debug("mtcsptx.c -----nvptx.cc -----100-- static rtx get_replacement (struct reg_replace *r)\n");
   if (r->n_allocated == r->n_in_use)
      r->replacement[r->n_allocated++] = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,r->mode);
   return r->replacement[r->n_in_use++];
}

/* Return true if given call insn references one of the functions provided by
   the CUDA runtime: malloc, free, vprintf.  */
static bool nvptx_call_insn_is_syscall_p (rtx_insn *insn)
{
   n_debug("mtcsptx.c -----nvptx.cc -----171-- nvptx_call_insn_is_syscall_p\n");
   rtx pat = PATTERN (insn);
   gcc_checking_assert (GET_CODE (pat) == PARALLEL);
   pat = XVECEXP (pat, 0, 0);
   if (GET_CODE (pat) == SET)
      pat = SET_SRC (pat);
   gcc_checking_assert (GET_CODE (pat) == CALL && GET_CODE (XEXP (pat, 0)) == MEM);
   rtx addr = XEXP (XEXP (pat, 0), 0);
   if (GET_CODE (addr) != SYMBOL_REF)
      return false;
   const char *name = XSTR (addr, 0);
   /* Ordinary malloc/free are redirected to __nvptx_{malloc,free), so only the
   references with forced assembler name refer to PTX syscalls.  For vprintf,
   accept both normal and forced-assembler-name references.  */
   return (!strcmp (name, "vprintf") || !strcmp (name, "*vprintf")
         || !strcmp (name, "*malloc") || !strcmp (name, "*free"));
}

/* If SET subexpression of INSN sets a register, emit a shuffle instruction to
   propagate its value from lane MASTER to current lane.  */
static bool nvptx_unisimt_handle_set (MtcsPtx *self,rtx set, rtx_insn *insn, rtx master)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)self;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsPtxEmit *mtcsPtxEmit=(MtcsPtxEmit *)mtcsEmit;

   n_debug("mtcsptx.c -----nvptx.cc -----170-- nvptx_unisimt_handle_set\n");
   rtx reg;
   if (GET_CODE (set) == SET  && REG_P (reg = SET_DEST (set))
   && find_reg_note (insn, REG_UNUSED, reg) == NULL_RTX){
      mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,
            mtcs_ptx_emit_gen_shuffle/*!nvptx_gen_shuffle*/(mtcsPtxEmit,reg, reg, master, PTX_SHUFFLE_IDX/*!SHUFFLE_IDX*/),insn);
      return true;
   }
   return false;
}

/* Return a SImode "master lane index" register for uniform-simt, allocating on
   first use.  */
static rtx nvptx_get_unisimt_master (MtcsPtx *self)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)self;
   MtcsMode *mtcsMode=mtcsTarget->mtcsMode;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   n_debug("mtcsptx.c -----nvptx.cc -----101-- static rtx nvptx_get_unisimt_master ()\n");
   struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)cfun->machine;
   rtx &master = nvptxMachine/*!cfun->machine*/->unisimt_master;
   return master ? master : master = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mtcsMode->modes.M_SImode);
}

/* Return a BImode "predicate" register for uniform-simt, similar to above.  */

static rtx nvptx_get_unisimt_predicate (MtcsPtx *self)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)self;
   MtcsMode *mtcsMode=mtcsTarget->mtcsMode;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

   n_debug("mtcsptx.c -----nvptx.cc -----102--static rtx nvptx_get_unisimt_predicate ()\n");
   struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)cfun->machine;
   rtx &pred = nvptxMachine/*!cfun->machine*/->unisimt_predicate;
   return pred ? pred : pred = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mtcsMode->modes.M_BImode);
}

static rtx nvptx_get_unisimt_outside_simt_predicate (MtcsPtx *self)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)self;
   MtcsMode *mtcsMode=mtcsTarget->mtcsMode;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   n_debug("mtcsptx.c -----nvptx.cc -----103--static rtx nvptx_get_unisimt_outside_simt_predicate ()\n");
   struct ptx_machine_function *nvptxMachine=(struct ptx_machine_function *)cfun->machine;

   rtx &pred = nvptxMachine/*!cfun->machine*/->unisimt_outside_simt_predicate;
   return pred ? pred : pred = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mtcsMode->modes.M_BImode);
}

static void predicate_insn (MtcsPtx *self,rtx_insn *insn, rtx pred)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)self;
   MtcsMode *mtcsMode=mtcsTarget->mtcsMode;
   MtcsRecog *mtcsRecog = mtcs_target_get_recog(mtcsTarget);

   n_debug("mtcsptx.c -----nvptx.cc -----169-- predicate_insn\n");

   rtx pat = PATTERN (insn);
   pred = gen_rtx_NE (mtcsMode->modes.M_BImode, pred, const0_rtx);
   pat = gen_rtx_COND_EXEC (VOIDmode, pred, pat);
   bool changed_p = mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,insn, &PATTERN (insn), pat, false);
   gcc_assert (changed_p);
}


/* Adjust code for uniform-simt code generation variant by making atomics and
   "syscalls" conditionally executed, and inserting shuffle-based propagation
   for registers being set.  */
static void nvptx_reorg_uniform_simt (MtcsPtx *self)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)self;
   MtcsMode *mtcsMode=mtcsTarget->mtcsMode;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsInsnAttr *mtcsInsnAttr = mtcs_target_get_insn_attr(mtcsTarget);
   MtcsRecog *mtcsRecog = mtcs_target_get_recog(mtcsTarget);

   n_debug("mtcsptx.c -----nvptx.cc -----168-- nvptx_reorg_uniform_simt\n");
   rtx_insn *insn, *next;
   for (insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); insn; insn = next){
      next = NEXT_INSN (insn);

      /* Skip NOTE, USE, etc.  */
      if (!INSN_P (insn) || mtcs_recog_recog_memoized/*!recog_memoized*/(mtcsRecog,insn) == -1)
         continue;

      if (CALL_P (insn) && nvptx_call_insn_is_syscall_p (insn)){
      /* Handle syscall.  */
      }else if (mtcs_insn_attr_get_attr_atomic/*!get_attr_atomic*/(mtcsInsnAttr,insn)){
      /* Handle atomic insn.  */
      }else
         continue;

      rtx pat = PATTERN (insn);
      rtx master = nvptx_get_unisimt_master(self);
      bool shuffle_p = false;
      switch (GET_CODE (pat)){
         case PARALLEL:
            for (int i = 0; i < XVECLEN (pat, 0); i++)
               shuffle_p |= nvptx_unisimt_handle_set(self,XVECEXP (pat, 0, i), insn, master);
            break;
         case SET:
            shuffle_p |= nvptx_unisimt_handle_set(self,pat, insn, master);
            break;
         default:
            gcc_unreachable ();
      }

      if (shuffle_p && TARGET_PTX_6_0){
      /* The shuffle is a sync, so uniformity is guaranteed.  */
      }else{
         if (TARGET_PTX_6_0){
            gcc_assert (!shuffle_p);
            /* Emit after the insn, to guarantee uniformity.  */
            mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,ptx_gen_nvptx_warpsync/*!gen_nvptx_warpsync*/(), insn);
         }else{
            /* Emit after the insn (and before the shuffle, if there are any)
            to check uniformity.  */
            mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,
                  ptx_gen_nvptx_uniform_warp_check/*!gen_nvptx_uniform_warp_check*/(), insn);
         }
      }

      rtx pred = nvptx_get_unisimt_predicate(self);
      predicate_insn(self,insn, pred);

      pred = NULL_RTX;
      for (rtx_insn *post = NEXT_INSN (insn); post != next; post = NEXT_INSN (post)){
         if (pred == NULL_RTX)
            pred = nvptx_get_unisimt_outside_simt_predicate(self);
         predicate_insn(self,post, pred);
      }
   }
}

/* Clean up subreg operands.  In ptx assembly, everything is typed, and
   the presence of subregs would break the rules for most instructions.
   Replace them with a suitable new register of the right size, plus
   conversion copyin/copyout instructions.  */

static void nvptx_reorg_subreg (MtcsPtx *self)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)self;
   MtcsMode *mtcsMode=mtcsTarget->mtcsMode;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsInsnAttr *mtcsInsnAttr = mtcs_target_get_insn_attr(mtcsTarget);
   MtcsRecog *mtcsRecog = mtcs_target_get_recog(mtcsTarget);

   n_debug("mtcsptx.c -----nvptx.cc -----172-- nvptx_reorg_subreg\n");

   struct reg_replace qiregs, hiregs, siregs, diregs;
   rtx_insn *insn, *next;

   qiregs.n_allocated = 0;
   hiregs.n_allocated = 0;
   siregs.n_allocated = 0;
   diregs.n_allocated = 0;
   qiregs.mode = mtcsMode->modes.M_QImode;
   hiregs.mode = mtcsMode->modes.M_HImode;
   siregs.mode = mtcsMode->modes.M_SImode;
   diregs.mode = mtcsMode->modes.M_DImode;

   for (insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); insn; insn = next){
      next = NEXT_INSN (insn);
      if (!NONDEBUG_INSN_P (insn)
      || asm_noperands (PATTERN (insn)) >= 0
      || GET_CODE (PATTERN (insn)) == USE
      || GET_CODE (PATTERN (insn)) == CLOBBER)
         continue;

      qiregs.n_in_use = 0;
      hiregs.n_in_use = 0;
      siregs.n_in_use = 0;
      diregs.n_in_use = 0;
      mtcs_recog_extract_insn/*!extract_insn*/(mtcsRecog,insn);
      int /*!enum attr_subregs_ok*/ s_ok = mtcs_insn_attr_get_attr_subregs_ok/*!get_attr_subregs_ok*/(mtcsInsnAttr,insn);

      for (int i = 0; i <mtcsRecog->recog_data.n_operands; i++){
         rtx op = mtcsRecog->recog_data.operand[i];
         if (GET_CODE (op) != SUBREG)
            continue;

         rtx inner = SUBREG_REG (op);

         machine_mode outer_mode = GET_MODE (op);
         machine_mode inner_mode = GET_MODE (inner);
         gcc_assert (s_ok);
         if (s_ok  && (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,inner_mode) >=
               mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,outer_mode)))
            continue;
         gcc_assert (mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,outer_mode));
         struct reg_replace *r = (outer_mode == mtcsMode->modes.M_QImode ? &qiregs
          : outer_mode == mtcsMode->modes.M_HImode ? &hiregs
          : outer_mode == mtcsMode->modes.M_SImode ? &siregs
          : &diregs);
         rtx new_reg = get_replacement(self,r);

         if (mtcsRecog->recog_data.operand_type[i] != OP_OUT){
            enum rtx_code code;
            if (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,inner_mode)
            < mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,outer_mode))
               code = ZERO_EXTEND;
            else
               code = TRUNCATE;

            rtx pat = gen_rtx_SET (new_reg,gen_rtx_fmt_e (code, outer_mode, inner));
            mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,pat, insn);
         }

         if (mtcsRecog->recog_data.operand_type[i] != OP_IN){
            enum rtx_code code;
            if (mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,inner_mode)
            < mtcs_mode_get_precision/*!GET_MODE_PRECISION*/(mtcsMode,outer_mode))
               code = TRUNCATE;
            else
               code = ZERO_EXTEND;

            rtx pat = gen_rtx_SET (inner,gen_rtx_fmt_e (code, inner_mode, new_reg));
            mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,pat, insn);
         }
         mtcs_recog_validate_change/*!validate_change*/(mtcsRecog,insn, mtcsRecog->recog_data.operand_loc[i], new_reg, false);
      }
   }
}

/* Variant of pc_set that only requires JUMP_P (INSN) if STRICT.  This variant
   is needed in the nvptx target because the branches generated for
   parititioning are NONJUMP_INSN_P, not JUMP_P.  */

static rtx nvptx_pc_set (const rtx_insn *insn, bool strict = true)
{
   n_debug("mtcsptx.c -----nvptx.cc -----109--static rtx nvptx_pc_set (const rtx_insn *insn, bool strict = true)\n");
   rtx pat;
   if ((strict && !JUMP_P (insn)) || (!strict && !INSN_P (insn)))
      return NULL_RTX;
   pat = PATTERN (insn);

   /* The set is allowed to appear either as the insn pattern or
   the first set in a PARALLEL.  */
   if (GET_CODE (pat) == PARALLEL)
      pat = XVECEXP (pat, 0, 0);
   if (GET_CODE (pat) == SET && GET_CODE (SET_DEST (pat)) == PC)
      return pat;

   return NULL_RTX;
}

/* Variant of condjump_label that only requires JUMP_P (INSN) if STRICT.  */
static rtx nvptx_condjump_label (const rtx_insn *insn, bool strict = true)
{
   n_debug("mtcsptx.c -----nvptx.cc -----110--static rtx nvptx_condjump_label (const rtx_insn *insn, bool strict = true)\n");
   rtx x = nvptx_pc_set (insn, strict);
   if (!x)
      return NULL_RTX;
   x = SET_SRC (x);
   if (GET_CODE (x) == LABEL_REF)
      return x;
   if (GET_CODE (x) != IF_THEN_ELSE)
      return NULL_RTX;
   if (XEXP (x, 2) == pc_rtx && GET_CODE (XEXP (x, 1)) == LABEL_REF)
      return XEXP (x, 1);
   if (XEXP (x, 1) == pc_rtx && GET_CODE (XEXP (x, 2)) == LABEL_REF)
      return XEXP (x, 2);
   return NULL_RTX;
}

/* Insert a dummy ptx insn when encountering a branch to a label with no ptx
   insn inbetween the branch and the label.  This works around a JIT bug
   observed at driver version 384.111, at -O0 for sm_50.  */
static void prevent_branch_around_nothing (MtcsPtx *self)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)self;
   MtcsMode *mtcsMode=mtcsTarget->mtcsMode;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRecog *mtcsRecog = mtcs_target_get_recog(mtcsTarget);

   n_debug("mtcsptx.c -----nvptx.cc -----143-- prevent_branch_around_nothing\n");
   rtx_insn *seen_label = NULL;
   for (rtx_insn *insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); insn; insn = NEXT_INSN (insn)){
      if (INSN_P (insn) && condjump_p (insn)){
         seen_label = label_ref_label (nvptx_condjump_label (insn, false));
         continue;
      }

      if (seen_label == NULL)
         continue;

      if (NOTE_P (insn) || DEBUG_INSN_P (insn))
         continue;

      if (INSN_P (insn))
         switch (mtcs_recog_recog_memoized/*!recog_memoized*/(mtcsRecog,insn)){
            case PTX_CODE_FOR_nvptx_fork:
            case PTX_CODE_FOR_nvptx_forked:
            case PTX_CODE_FOR_nvptx_joining:
            case PTX_CODE_FOR_nvptx_join:
            case CODE_FOR_nop:
               continue;
            case -1:
               /* Handle asm ("") and similar.  */
               if (GET_CODE (PATTERN (insn)) == ASM_INPUT
               || GET_CODE (PATTERN (insn)) == ASM_OPERANDS
               || (GET_CODE (PATTERN (insn)) == PARALLEL
               && asm_noperands (PATTERN (insn)) >= 0))
               continue;
            /* FALLTHROUGH.  */
            default:
               seen_label = NULL;
               continue;
         }

      if (LABEL_P (insn) && insn == seen_label)
         mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,ptx_gen_fake_nop (), insn);

      seen_label = NULL;
   }
}

/* Insert two membar.cta insns inbetween two subsequent bar.sync insns.  This
   works around a hang observed at driver version 390.48 for sm_50.  */
static void workaround_barsyncs (MtcsPtx *self)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)self;
   MtcsMode *mtcsMode=mtcsTarget->mtcsMode;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRecog *mtcsRecog = mtcs_target_get_recog(mtcsTarget);

   n_debug("mtcsptx.c -----nvptx.cc -----142-- workaround_barsyncs\n");

   bool seen_barsync = false;
   for (rtx_insn *insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); insn; insn = NEXT_INSN (insn)){
      if (INSN_P (insn) && recog_memoized (insn) == PTX_CODE_FOR_nvptx_barsync){
         if (seen_barsync){
            mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,ptx_gen_nvptx_membar_cta (), insn);
            mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,ptx_gen_nvptx_membar_cta (), insn);
         }

         seen_barsync = true;
         continue;
      }

      if (!seen_barsync)
         continue;

      if (NOTE_P (insn) || DEBUG_INSN_P (insn))
         continue;
      else if (INSN_P (insn))
         switch (mtcs_recog_recog_memoized/*!recog_memoized*/(mtcsRecog,insn)){
            case PTX_CODE_FOR_nvptx_fork:
            case PTX_CODE_FOR_nvptx_forked:
            case PTX_CODE_FOR_nvptx_joining:
            case PTX_CODE_FOR_nvptx_join:
               continue;
            default:
               break;
         }

      seen_barsync = false;
   }
}

/* PTX-specific reorganization
   - Split blocks at fork and join instructions
   - Compute live registers
   - Mark now-unused registers, so function begin doesn't declare
   unused registers.
   - Insert state propagation when entering partitioned mode
   - Insert neutering instructions when in single mode
   - Replace subregs with suitable sequences.
*/
//TARGET_MACHINE_DEPENDENT_REORG是在rtl pass的"mach"中被调用
//原型 targetm.machine_dependent_reorg () #define TARGET_MACHINE_DEPENDENT_REORG nvptx_reorg
static void  machineDependentReorg_cb(MtcsTarget *mtcsTarget)
{
   MtcsPtx *self=(MtcsPtx *)mtcsTarget;
   MtcsMode *mtcsMode=mtcsTarget->mtcsMode;
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   MtcsDfproblems *mtcsDfproblems=mtcs_target_get_dfproblems(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   PtxOptionsItem *ptxOptionsItem=(PtxOptionsItem *)mtcsOptionsItem;
   n_debug("mtcsptx.c -----nvptx.cc -----57-- TARGET_MACHINE_DEPENDENT_REORG void nvptx_reorg (void)\n");
   /* We are freeing block_for_insn in the toplev to keep compatibility
      with old MDEP_REORGS that are not CFG based.  Recompute it now.  */
   mtcs_cfg_rtl_compute_bb_for_insn/*!compute_bb_for_insn*/(mtcsCfgRtl);
   n_debug("mtcsptx.c -----nvptx.cc -----57aa11-- insn:%p\n",get_last_insn());

   mtcs_func_thread_prologue_and_epilogue_insns/*!thread_prologue_and_epilogue_insns*/(mtcsFunc);
   n_debug("mtcsptx.c -----nvptx.cc -----57aa22-- insn:%p\n",get_last_insn());

   /* Split blocks and record interesting unspecs.  */
   bb_insn_map_t bb_insn_map;

   nvptx_split_blocks(self,&bb_insn_map);
   n_debug("mtcsptx.c -----nvptx.cc -----57aa-- insn:%p\n",get_last_insn());

   /* Compute live regs */
   mtcs_dfcore_df_clear_flags/*!df_clear_flags*/(mtcsDfcore,DF_LR_RUN_DCE);
   mtcs_dfcore_df_set_flags/*!df_set_flags*/(mtcsDfcore,DF_NO_INSN_RESCAN | DF_NO_HARD_REGS);
   mtcs_dfproblems_df_live_add_problem/*!df_live_add_problem*/(mtcsDfproblems);
   mtcs_dfproblems_df_live_set_all_dirty/*!df_live_set_all_dirty*/(mtcsDfproblems);
   if (ptxOptionsItem->x_nvptx_init_regs == 3)
      mtcs_dfproblems_df_mir_add_problem/*!df_mir_add_problem*/(mtcsDfproblems);
   mtcs_dfcore_df_analyze/*!df_analyze*/(mtcsDfcore);
   n_debug("mtcsptx.c -----nvptx.cc -----57bb-- insn:%p\n",get_last_insn());

   mtcs_reg_regstat_init_n_sets_and_refs/*!regstat_init_n_sets_and_refs*/(mtcsReg);

   if (dump_file)
      mtcs_dfcore_df_dump/*!df_dump*/(mtcsDfcore,dump_file);
   n_debug("mtcsptx.c -----nvptx.cc -----57cc-- insn:%p\n",get_last_insn());

   /* Mark unused regs as unused.  */
   int max_regs = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);
   for (int i = mtcs_reg_get_last_virtual_regno/*!LAST_VIRTUAL_REGISTER*/(mtcsReg) + 1; i < max_regs; i++)
     if (REG_N_SETS (i) == 0 && REG_N_REFS (i) == 0)
        mtcsRtlData->regno_reg_rtx/*!regno_reg_rtx*/[i] = const0_rtx;

   workaround_uninit(self);
   n_debug("mtcsptx.c -----nvptx.cc -----57dd-- insn:%p\n",get_last_insn());

   /* Determine launch dimensions of the function.  If it is not an
      offloaded function  (i.e. this is a regular compiler), the
      function has no neutering.  */
   tree attr = oacc_get_fn_attrib (current_function_decl);
   if (attr)
     {
      n_debug("mtcsptx.c -----nvptx.cc -----57ee-- \n");

       /* If we determined this mask before RTL expansion, we could
      elide emission of some levels of forks and joins.  */
       offload_attrs oa;

       populate_offload_attrs (&oa);

       /* If there is worker neutering, there must be vector
      neutering.  Otherwise the hardware will fail.  */
       gcc_assert (!(oa.mask & GOMP_DIM_MASK (GOMP_DIM_WORKER))
           || (oa.mask & GOMP_DIM_MASK (GOMP_DIM_VECTOR)));

       /* Discover & process partitioned regions.  */
//       parallel *pars = nvptx_discover_pars (&bb_insn_map);
//       nvptx_process_pars (pars);
//       nvptx_neuter_pars (pars, oa.mask, 0);
//       delete pars;
     }
   n_debug("mtcsptx.c -----nvptx.cc -----57ff-- insn:%p\n",get_last_insn());

   /* Replace subregs.  */
   nvptx_reorg_subreg(self);
   n_debug("mtcsptx.c -----nvptx.cc -----57hh--TARGET_UNIFORM_SIMT:%d insn:%p \n",TARGET_UNIFORM_SIMT,get_last_insn());

   if (TARGET_UNIFORM_SIMT)
     nvptx_reorg_uniform_simt(self);
   n_debug("mtcsptx.c -----nvptx.cc -----57ii-- \n");

 //#if WORKAROUND_PTXJIT_BUG_2
   prevent_branch_around_nothing(self);
// #endif
   n_debug("mtcsptx.c -----nvptx.cc -----57jj-- \n");

// #ifdef WORKAROUND_PTXJIT_BUG_3
   workaround_barsyncs(self);
 //#endif
   n_debug("mtcsptx.c -----nvptx.cc -----57kk-- \n");

   regstat_free_n_sets_and_refs ();
   n_debug("mtcsptx.c -----nvptx.cc -----57ll-- \n");

   mtcs_dfcore_df_finish_pass/*!df_finish_pass*/(mtcsDfcore,true);
}


//原型 targetm.can_change_mode_class (GET_MODE (x_inner), mode, ALL_REGS)) #define TARGET_CAN_CHANGE_MODE_CLASS hook_bool_mode_mode_reg_class_t_true
static bool canChangeModeClass_cb(MtcsTarget *self,machine_mode from, machine_mode to, reg_class_t rclass)
{
   fprintf(stderr,"-----mtcsptx.c -----73-- TARGET_CAN_CHANGE_MODE_CLASS \
       bool nvptx_can_change_mode_class (machine_mode, machine_mode, reg_class_t) %d %d %d\n",from,to,rclass);
   return false;
}
//recog_memoized需要覆盖吗？
//原型 targetm.cannot_copy_insn_p #define TARGET_CANNOT_COPY_INSN_P nvptx_cannot_copy_insn_p
static bool cannotCopyInsnP_cb(MtcsTarget *mtcsTarget,rtx_insn *insn)
{
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   n_debug("mtcsptx.c -----nvptx.cc -----183-- TARGET_CANNOT_COPY_INSN_P void nvptx_cannot_copy_insn_p (rtx_insn *insn)\n");
   switch (mtcs_recog_recog_memoized/*!recog_memoized*/(mtcsRecog,insn)){
      case PTX_CODE_FOR_nvptx_shufflesi:
      case PTX_CODE_FOR_nvptx_shufflesf:
      case PTX_CODE_FOR_nvptx_barsync:
      case PTX_CODE_FOR_nvptx_fork:
      case PTX_CODE_FOR_nvptx_forked:
      case PTX_CODE_FOR_nvptx_joining:
      case PTX_CODE_FOR_nvptx_join:
         return true;
      default:
         return false;
   }
}

//原型 targetm.cannot_force_const_mem (mode, x) #define TARGET_CANNOT_FORCE_CONST_MEM nvptx_cannot_force_const_mem
static bool cannotForceConstMem_cb(MtcsTarget *self, machine_mode mode,rtx x)
{
   n_debug("mtcsptx.c -----nvptx.cc -----67-- TARGET_CANNOT_FORCE_CONST_MEM bool nvptx_cannot_force_const_mem (machine_mode mode ATTRIBUTE_UNUSED, rtx x ATTRIBUTE_UNUSED)\n");
   return true;
}

//原型 targetm.use_blocks_for_constant_p (mode, x)#define TARGET_USE_BLOCKS_FOR_CONSTANT_P hook_bool_mode_const_rtx_true
static bool useBlocksForConstantP_cb(MtcsTarget *self, machine_mode mode,const_rtx x)
{
   return true;
}

//原型 targetm.truly_noop_truncation (GET_MODE_PRECISION (MODE1),GET_MODE_PRECISION (MODE2)))#define TARGET_TRULY_NOOP_TRUNCATION nvptx_truly_noop_truncation
static bool trulyNoopTruncation_cb(MtcsTarget *self, poly_uint64 outprec,poly_uint64 inprec)
{
   n_debug("mtcsptx.c -----nvptx.cc -----74-- TARGET_TRULY_NOOP_TRUNCATION bool nvptx_truly_noop_truncation (poly_uint64, poly_uint64)\n");
   return false;
}


/* Emit to STREAM the assembler syntax for an insn operand whose memory
   address is X.  */
//原型targetm.asm_out.print_operand_address #define TARGET_PRINT_OPERAND_ADDRESS default_print_operand_address
static void printOperandAddress_cb(MtcsTarget *mtcsTarget,machine_mode mode,  rtx addr ATTRIBUTE_UNUSED)
{
   MtcsPtxOutput *mtcsPtxOutput=(MtcsPtxOutput *)mtcs_target_get_output(mtcsTarget);
   mtcs_ptx_output_print_address_operand (mtcsPtxOutput,addr, mode);
}

//原型targetm.asm_out.print_operand #define TARGET_PRINT_OPERAND default_print_operand
static void printOperand_cb(MtcsTarget *mtcsTarget,rtx x ATTRIBUTE_UNUSED,int code ATTRIBUTE_UNUSED)
{
   MtcsPtxOutput *mtcsPtxOutput=(MtcsPtxOutput *)mtcs_target_get_output(mtcsTarget);
   mtcs_ptx_output_print_operand(mtcsPtxOutput,x,code);
}

static bool useAnchorsForSymbolP_cb(MtcsTarget *self,const_rtx symbol)
{
   n_debug("mtcsptx.c -----nvptx.cc -----60-- TARGET_USE_ANCHORS_FOR_SYMBOL_P bool nvptx_use_anchors_for_symbol_p (const_rtx ARG_UNUSED (a))\n");
   return false;
}
//原型 targetm.set_current_function(tree fndecl); #define TARGET_SET_CURRENT_FUNCTION hook_void_tree
static void setCurrentFunction_cb(MtcsTarget *mtcsTarget,tree fndecl)
{
   MtcsPtx *self=(MtcsPtx *)mtcsTarget;
   n_debug("mtcsptx.c -----nvptx.cc -----77-- TARGET_SET_CURRENT_FUNCTION void nvptx_set_current_function (tree fndecl)\n");
   if (!fndecl || fndecl == self->nvptx_previous_fndecl)
      return;

   self->gang_private_shared_hmap.empty ();
   self->nvptx_previous_fndecl = fndecl;
   self->vector_red_partition = 0;
   // oacc_bcast_partition = 0;
}

//原型targetm.vector_mode_supported_p (result_mode)) #define TARGET_VECTOR_MODE_SUPPORTED_P nvptx_vector_mode_supported
static bool vectorModeSupportedP_cb (MtcsTarget *mtcsTarget,machine_mode mode)
{
    n_debug("mtcsptx.c -----nvptx.cc -----69-- TARGET_VECTOR_MODE_SUPPORTED_P bool nvptx_vector_mode_supported (machine_mode mode)\n");
    MtcsMode *mtcsMode=mtcs_target_get_mode(mtcsTarget);
   return (mode == mtcsMode->modes.M_V2SImode || mode ==  mtcsMode->modes.M_V2DImode);
}

//原型 targetm.stack_protect_runtime_enabled_p () #define TARGET_STACK_PROTECT_RUNTIME_ENABLED_P hook_bool_void_true
static bool stackProtectRuntimeEnabledP_cb (MtcsTarget *mtcsTarget)
{
    return true;
}

//原型 targetm.precompute_tls_p (args[i].mode, args[i].value) #define TARGET_PRECOMPUTE_TLS_P hook_bool_mode_rtx_false
static bool precomputeTlsP_cb(MtcsTarget *mtcsTarget,machine_mode, rtx)
{
  return false;
}

//原型  targetm.class_likely_spilled_p #define TARGET_CLASS_LIKELY_SPILLED_P default_class_likely_spilled_p
static bool classLikelySpilledP_cb(MtcsTarget *mtcsTarget,reg_class_t rclass)
{
    MtcsReg *mtcsReg=mtcsTarget->mtcsReg;
    return (mtcsReg->hardRegs.x_reg_class_size/*!reg_class_size*/[(int) rclass] == 1);
}

//原型 targetm.gen_restore_stack_nonlocal #define TARGET_GEN_RESTORE_STACK_NONLOCAL invalid_rtx_rtx
static rtx_insn *genRestoreStackNonlocal_cb(MtcsTarget *self,rtx x0, rtx x1)
{
    gcc_unreachable ();
}

 //原型 targetm.stack_protect_fail () #define TARGET_STACK_PROTECT_FAIL default_external_stack_protect_fail
static tree stackProtectFail_cb(MtcsTarget *self)
{
    return default_external_stack_protect_fail();
}

//原型 targetm.class_max_nregs ((reg_class_t) i, (machine_mode) m) #define TARGET_CLASS_MAX_NREGS default_class_max_nregs
static unsigned char classMaxNregs_cb(MtcsTarget *mtcsTarget,reg_class_t rclass ATTRIBUTE_UNUSED,machine_mode mode ATTRIBUTE_UNUSED)
{
    MtcsMode *mtcsMode=mtcsTarget->mtcsMode;
    //nvptx定义的宏 #define CLASS_MAX_NREGS(class, mode)  ((GET_MODE_SIZE (mode) + UNITS_PER_WORD - 1) / UNITS_PER_WORD)
    //与class无关 需要把mode转成MACRO_MODE (mode)
    mode=mtcs_mode_get_macro(mtcsMode,mode);
    unsigned int size = mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/ (mtcsMode,mode).to_constant ();
    return (size + UNITS_PER_WORD - 1) / UNITS_PER_WORD;;
}

//原型 targetm.expand_builtin (exp, target, subtarget, mode, ignore) #define TARGET_EXPAND_BUILTIN nvptx_expand_builtin
static rtx expandBuiltin_cb(MtcsTarget *mtcsTarget,tree exp, rtx target, rtx ARG_UNUSED (subtarget),
              machine_mode mode, int ignore)
{
  n_debug("mtcsptx.c -----nvptx.cc -----117-- TARGET_EXPAND_BUILTIN rtx nvptx_expand_builtin\n");
  MtcsPtxBuiltins *mtcsPtxBuiltins=(MtcsPtxBuiltins *)mtcs_target_get_builtins(mtcsTarget);
  return mtcs_ptx_builtins_expand_builtin(mtcsPtxBuiltins,exp,target,subtarget,mode,ignore);
}

//原型 targetm.libc_has_function #define TARGET_LIBC_HAS_FUNCTION nvptx_libc_has_function
static bool libcHasFunction_cb(MtcsTarget *mtcsTarget,enum function_class fn_class, tree type)
{
   n_debug("mtcsptx.c -----nvptx.cc -----78-- TARGET_LIBC_HAS_FUNCTION void nvptx_libc_has_function fn_class:%d\n",
         fn_class);
   if (fn_class == function_sincos){
      if (type != NULL_TREE)
         /* Currently, newlib does not support sincosl.  */
         return type == float_type_node || type == double_type_node;
      else
         return true;
   }
   return default_libc_has_function (fn_class, type);
}

//原型 targetm.floatn_mode (n, extended) #define TARGET_FLOATN_MODE default_floatn_mode
static opt_scalar_float_mode floatnMode_cb(MtcsTarget *mtcsTarget,int n, bool extended)
{
   MtcsMode *mtcsMode=mtcsTarget->mtcsMode;

   if (extended){
      opt_scalar_float_mode cand1, cand2;
      scalar_float_mode mode;
      switch (n){
         case 32:
         cand1 =mtcs_mode_as_a<scalar_float_mode>(mtcsMode, PTX_DFmode);
         break;
         case 64:
         //   #ifdef HAVE_XFmode
         //        cand1 = XFmode;
         //   #endif
         //   #ifdef HAVE_TFmode
         //        cand2 = TFmode;
         //   #endif
            break;

         case 128:
            break;
         default:
            /* Those are the only valid _FloatNx types.  */
            gcc_unreachable ();
      }
      n_debug("mtcsptx.c  floatnMode_cb 00 n:%d extends:%d exists:%d\n",n,extended,cand1.exists (&mode));
      n_debug("mtcsptx.c  floatnMode_cb 11 mode:%d HF:%d SF:%d DF:%d\n",mode,PTX_HFmode,PTX_SFmode,PTX_DFmode);
      if(cand1.exists (&mode)){
             n_debug("mtcsptx.c  floatnMode_cb 22 %d\n",mtcs_mode_get_real_format(mtcsMode,mode)->ieee_bits);
             n_debug("mtcsptx.c  floatnMode_cb 33 %d\n",mtcsTarget->scalar_mode_supported_p(mtcsTarget,mode));
             n_debug("mtcsptx.c  floatnMode_cb 44 %d\n", mtcsTarget->libgcc_floating_mode_supported_p(mtcsTarget,mode));
      }
      if (cand1.exists (&mode)
      && mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,mode)->ieee_bits > n
      && mtcsTarget->scalar_mode_supported_p/*!targetm.scalar_mode_supported_p*/(mtcsTarget,mode)
      && mtcsTarget->libgcc_floating_mode_supported_p/*!targetm.libgcc_floating_mode_supported_p*/(mtcsTarget,mode))
         return cand1;
      n_debug("mtcsptx.c  floatnMode_cb 55 n:%d extends:%d exists:%d\n",n,extended,cand2.exists (&mode));
      n_debug("mtcsptx.c  floatnMode_cb 66 mode:%d HF:%d SF:%d DF:%d\n",mode,PTX_HFmode,PTX_SFmode,PTX_DFmode);
      if(cand2.exists (&mode)){
                  n_debug("mtcsptx.c  floatnMode_cb 77 %d\n",mtcs_mode_get_real_format(mtcsMode,mode)->ieee_bits);
                  n_debug("mtcsptx.c  floatnMode_cb 88 %d\n",mtcsTarget->scalar_mode_supported_p(mtcsTarget,mode));
                  n_debug("mtcsptx.c  floatnMode_cb 99 %d\n", mtcsTarget->libgcc_floating_mode_supported_p(mtcsTarget,mode));
           }
      if (cand2.exists (&mode)
      && mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,mode)->ieee_bits > n
      && mtcsTarget->scalar_mode_supported_p/*!targetm.scalar_mode_supported_p*/(mtcsTarget,mode)
      && mtcsTarget->libgcc_floating_mode_supported_p/*!targetm.libgcc_floating_mode_supported_p*/(mtcsTarget,mode))
         return cand2;
   }else{
      opt_scalar_float_mode cand;
      scalar_float_mode mode;
      switch (n){
         case 16:
            /* Always enable _Float16 if we have basic support for the mode.
            Targets can control the range and precision of operations on
            the _Float16 type using TARGET_C_EXCESS_PRECISION.  */
            //#ifdef HAVE_HFmode
            cand = mtcs_mode_as_a<scalar_float_mode>(mtcsMode, PTX_HFmode);
            // #endif
            break;

         case 32:
            // #ifdef HAVE_SFmode
            cand = mtcs_mode_as_a<scalar_float_mode>(mtcsMode, PTX_SFmode);;
            //#endif
            break;

         case 64:
            //#ifdef HAVE_DFmode
            cand = mtcs_mode_as_a<scalar_float_mode>(mtcsMode, PTX_DFmode);
            // #endif
            break;

         case 128:
            // #ifdef HAVE_TFmode
            //     cand = TFmode;
            // #endif
            break;

         default:
            break;
      }
      n_debug("mtcsptx.c  floatnMode_cb 100 n:%d extends:%d exists:%d\n",n,extended,cand.exists (&mode));
      n_debug("mtcsptx.c  floatnMode_cb 101 mode:%d HF:%d SF:%d DF:%d\n",mode,PTX_HFmode,PTX_SFmode,PTX_DFmode);
      if(cand.exists (&mode)){
        n_debug("mtcsptx.c  floatnMode_cb 102 %d\n",mtcs_mode_get_real_format(mtcsMode,mode)->ieee_bits);
        n_debug("mtcsptx.c  floatnMode_cb 103 %d\n",mtcsTarget->scalar_mode_supported_p(mtcsTarget,mode));
        n_debug("mtcsptx.c  floatnMode_cb 104 %d\n", mtcsTarget->libgcc_floating_mode_supported_p(mtcsTarget,mode));
      }

      if (cand.exists (&mode)
      && mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,mode)->ieee_bits == n
      && mtcsTarget->scalar_mode_supported_p/*!targetm.scalar_mode_supported_p*/(mtcsTarget,mode)
      &&  mtcsTarget->libgcc_floating_mode_supported_p/*!targetm.libgcc_floating_mode_supported_p*/(mtcsTarget,mode))
         return cand;
   }
   return opt_scalar_float_mode ();
}

//原型 targetm.decimal_float_supported_p() #define TARGET_DECIMAL_FLOAT_SUPPORTED_P default_decimal_float_supported_p
static bool decimalFloatSupportedP_cb(MtcsTarget *mtcsTarget)
{
   return false;//ENABLE_DECIMAL_FLOAT;
}

//原型 targetm.preferred_reload_class(x, rclass);#define TARGET_PREFERRED_RELOAD_CLASS default_preferred_reload_class
static reg_class_t preferredReloadClass_cb(MtcsTarget *mtcsTarget,rtx x, reg_class_t regclass)
{
  return regclass;
}

//原型 targetm.secondary_reload (to_p, x, rclass, mode, &sri); #define TARGET_SECONDARY_RELOAD default_secondary_reload
//参照 default_secondary_reload targhooks.cc
static reg_class_t secondaryReload_cb(MtcsTarget *mtcsTarget,bool in_p ATTRIBUTE_UNUSED, rtx x ATTRIBUTE_UNUSED,
           reg_class_t reload_class_i ATTRIBUTE_UNUSED, machine_mode reload_mode ATTRIBUTE_UNUSED, secondary_reload_info *sri)
{
   MtcsMode *mtcsMode=mtcsTarget->mtcsMode;
   MtcsOpinit *mtcsOpinit =mtcs_target_get_opinit(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsPreds *mtcsPreds =mtcs_target_get_preds(mtcsTarget);
   MtcsReg *mtcsReg =mtcs_target_get_reg(mtcsTarget);

   enum reg_class rclass = NO_REGS;
   enum reg_class reload_class = (enum reg_class) reload_class_i;

   if (sri->prev_sri && sri->prev_sri->t_icode != CODE_FOR_nothing){
      sri->icode = sri->prev_sri->t_icode;
      return NO_REGS;
   }
   // #ifdef SECONDARY_INPUT_RELOAD_CLASS //host=0 nvptx=0
   //   if (in_p)
   //     rclass = SECONDARY_INPUT_RELOAD_CLASS (reload_class,
   //                   MACRO_MODE (reload_mode), x);
   // #endif
   // #ifdef SECONDARY_OUTPUT_RELOAD_CLASS
   //   if (! in_p)
   //     rclass = SECONDARY_OUTPUT_RELOAD_CLASS (reload_class,
   //                    MACRO_MODE (reload_mode), x);
   // #endif
   if (rclass != NO_REGS){
      enum insn_code icode = mtcs_opinit_direct_optab_handler/*!direct_optab_handler*/(mtcsOpinit,
            in_p ? reload_in_optab : reload_out_optab,reload_mode);

      if (icode != CODE_FOR_nothing
      && !mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(mtcsOptabs,icode, in_p, x))
         icode = CODE_FOR_nothing;
      else if (icode != CODE_FOR_nothing){
         const char *insn_constraint, *scratch_constraint;
         enum reg_class insn_class, scratch_class;

         gcc_assert (mtcsOutput->insn_data[(int) icode].n_operands == 3);
         insn_constraint = mtcsOutput->insn_data[(int) icode].operand[!in_p].constraint;
         if (!*insn_constraint)
            insn_class =mtcs_reg_get_all_regs/*!ALL_REGS*/(mtcsReg);
         else{
            if (in_p){
               gcc_assert (*insn_constraint == '=');
               insn_constraint++;
            }
            insn_class = (mtcs_preds_reg_class_for_constraint/*!reg_class_for_constraint*/(mtcsPreds,
                  mtcs_preds_lookup_constraint/*!lookup_constraint*/(mtcsPreds,insn_constraint)));
            gcc_assert (insn_class != NO_REGS);
         }

         scratch_constraint = mtcsOutput->insn_data[(int) icode].operand[2].constraint;
         /* The scratch register's constraint must start with "=&",
         except for an input reload, where only "=" is necessary,
         and where it might be beneficial to re-use registers from
         the input.  */
         gcc_assert (scratch_constraint[0] == '='  && (in_p || scratch_constraint[1] == '&'));
         scratch_constraint++;
         if (*scratch_constraint == '&')
            scratch_constraint++;
         scratch_class = (mtcs_preds_reg_class_for_constraint/*!reg_class_for_constraint*/(mtcsPreds,
               mtcs_preds_lookup_constraint/*!lookup_constraint*/(mtcsPreds,scratch_constraint)));

         if (mtcs_reg_reg_class_subset_p/*!reg_class_subset_p*/(mtcsReg,reload_class, insn_class)){
            gcc_assert (scratch_class == rclass);
            rclass = NO_REGS;
         }else
            rclass = insn_class;

      }
      if (rclass == NO_REGS)
         sri->icode = icode;
      else
         sri->t_icode = icode;
   }
   return rclass;
}

//原型  targetm.has_ifunc_p () #define TARGET_HAS_IFUNC_P default_has_ifunc_p
static bool hasIfuncP_cb(MtcsTarget *mtcsTarget)
{
   return false;
}

static void createComponent(MtcsPtx *self)
{
    MtcsTarget *mtcsTarget=(MtcsTarget *)self;
    MtcsMode *mtcsMode=mtcsTarget->mtcsMode;
   // mtcsTarget->mtcsReg=(MtcsReg *)mtcs_ptx_reg_new(mtcsMode);//平台 寄存器
    mtcsTarget->mtcsOptions=(MtcsOptions *)mtcs_ptx_options_new(mtcsMode);
    mtcsTarget->mtcsRecog=(MtcsRecog *)mtcs_ptx_recog_new(mtcsMode);
    mtcsTarget->mtcsPreds=(MtcsPreds *)mtcs_ptx_preds_new(mtcsMode);
    mtcsTarget->mtcsAlign=(MtcsAlign *)mtcs_ptx_align_new(mtcsMode);
    mtcsTarget->mtcsFunc=(MtcsFunc *)mtcs_ptx_func_new(mtcsMode);
    mtcsTarget->mtcsOpinit=(MtcsOpinit *)mtcs_ptx_opinit_new(mtcsMode);
    mtcsTarget->mtcsEmit=(MtcsEmit *)mtcs_ptx_emit_new(mtcsMode);
    mtcsTarget->mtcsReal=(MtcsReal *)mtcs_ptx_real_new(mtcsMode);
    mtcsTarget->mtcsOutput=(MtcsOutput *)mtcs_ptx_output_new(mtcsMode);
    mtcsTarget->mtcsArgs=(MtcsArgs *)mtcs_ptx_args_new();
    mtcsTarget->mtcsCodes=(MtcsCodes*)mtcs_ptx_codes_new(mtcsMode);
    mtcsTarget->mtcsConfig=(MtcsConfig*)mtcs_ptx_config_new();
    mtcsTarget->mtcsBuiltins=(MtcsBuiltins *)mtcs_ptx_builtins_new(mtcsMode);
    mtcsTarget->mtcsTree=(MtcsTree *)mtcs_ptx_tree_new(mtcsMode);
    mtcsTarget->mtcsAttribs=(MtcsAttribs *)mtcs_ptx_attribs_new(mtcsMode);
    mtcsTarget->mtcsUnspec=(MtcsUnspec *)mtcs_ptx_unspec_new(mtcsMode);
    mtcsTarget->mtcsInsnAttr=(MtcsInsnAttr *)mtcs_ptx_insn_attr_new(mtcsMode);
    mtcsTarget->mtcsRTL=(MtcsRTL *)mtcs_ptx_rtl_new(mtcsMode);
    mtcsTarget->mtcsAsm=(MtcsAsm *)mtcs_ptx_asm_new(mtcsMode);
    mtcsTarget->mtcsInternalFn=(MtcsInternalFn *)mtcs_ptx_internal_fn_new(mtcsMode);

}

static void mtcsPtxInit(MtcsPtx *self)
{
    MtcsTarget *mtcsTarget=(MtcsTarget *)self;
    //原型 #define FUNCTION_PROFILER(FILE, LABELNO)
    mtcsTarget->function_profiler=functionProfiler_cb;
    //#define DATA_ALIGNMENT nvptx_data_alignment
    mtcsTarget->data_alignment=dataAlignment_cb;
    //原型ttargetm.slow_unaligned_access (DECL_MODE (decl),DECL_ALIGN (decl)) #define TARGET_SLOW_UNALIGNED_ACCESS default_slow_unaligned_access
    mtcsTarget->slow_unaligned_access=slowUnalignedAccess_cb;
    //原型 targetm.hard_regno_nregs (i, (machine_mode) j); #define TARGET_HARD_REGNO_NREGS nvptx_hard_regno_nregs
    mtcsTarget->hard_regno_nregs=hardRegnoNregs_cb;
    //原型 targetm.hard_regno_mode_ok (regno, mode) #define TARGET_HARD_REGNO_MODE_OK hook_bool_uint_mode_true
    mtcsTarget->hard_regno_mode_ok=hardRegnoModeOk_cb;
    //原型 targetm.scalar_mode_supported_p (TImode)) #define TARGET_SCALAR_MODE_SUPPORTED_P nvptx_scalar_mode_supported_p
    mtcsTarget->scalar_mode_supported_p=scalarModeSupportedP_cb;
    //原型 targetm.libgcc_floating_mode_supported_p (mode) #define TARGET_LIBGCC_FLOATING_MODE_SUPPORTED_P
    mtcsTarget->libgcc_floating_mode_supported_p=libgccFloatingModeSupportedP_cb;
    //原型 targetm.init_builtins ();#define TARGET_INIT_BUILTINS nvptx_init_builtins
    mtcsTarget->init_builtins=initBuiltins_cb;
    //原型targetm.modes_tieable_p  #define TARGET_MODES_TIEABLE_P hook_bool_mode_mode_true
    mtcsTarget->modes_tieable_p=modesTieableP_cb;
    //原型 targetm.can_change_mode_class (GET_MODE (x_inner), mode, ALL_REGS)) #define TARGET_CAN_CHANGE_MODE_CLASS hook_bool_mode_mode_reg_class_t_true
    mtcsTarget->can_change_mode_class=canChangeModeClass_cb;
    //原型 targetm.cannot_copy_insn_p #define TARGET_CANNOT_COPY_INSN_P nvptx_cannot_copy_insn_p
    mtcsTarget->cannot_copy_insn_p=cannotCopyInsnP_cb;
    //原型 targetm.cannot_force_const_mem (mode, x) #define TARGET_CANNOT_FORCE_CONST_MEM nvptx_cannot_force_const_mem
    mtcsTarget->cannot_force_const_mem=cannotForceConstMem_cb;
    //原型 targetm.use_blocks_for_constant_p (mode, x)#define TARGET_USE_BLOCKS_FOR_CONSTANT_P hook_bool_mode_const_rtx_true
    mtcsTarget->use_blocks_for_constant_p=useBlocksForConstantP_cb;
    //原型 targetm.truly_noop_truncation (GET_MODE_PRECISION (MODE1),GET_MODE_PRECISION (MODE2)))#define TARGET_TRULY_NOOP_TRUNCATION nvptx_truly_noop_truncation
    mtcsTarget->truly_noop_truncation=trulyNoopTruncation_cb;
    //原型 targetm.use_anchors_for_symbol_p (base)) #define TARGET_USE_ANCHORS_FOR_SYMBOL_P default_use_anchors_for_symbol_p
    mtcsTarget->use_anchors_for_symbol_p=useAnchorsForSymbolP_cb;
    //原型 targetm.set_current_function(tree fndecl); #define TARGET_SET_CURRENT_FUNCTION hook_void_tree
    mtcsTarget->set_current_function=setCurrentFunction_cb;
    //原型targetm.vector_mode_supported_p (result_mode)) #define TARGET_VECTOR_MODE_SUPPORTED_P nvptx_vector_mode_supported
    mtcsTarget->vector_mode_supported_p=vectorModeSupportedP_cb;
    //原型 targetm.emit_epilogue_for_sibcall #define TARGET_EMIT_EPILOGUE_FOR_SIBCALL NULL
    mtcsTarget->emit_epilogue_for_sibcall=NULL;
    //原型 targetm.stack_protect_runtime_enabled_p () #define TARGET_STACK_PROTECT_RUNTIME_ENABLED_P hook_bool_void_true
    mtcsTarget->stack_protect_runtime_enabled_p=stackProtectRuntimeEnabledP_cb;
    //原型 targetm.precompute_tls_p (args[i].mode, args[i].value) #define TARGET_PRECOMPUTE_TLS_P hook_bool_mode_rtx_false
    mtcsTarget->precompute_tls_p=precomputeTlsP_cb;
    //原型  targetm.class_likely_spilled_p #define TARGET_CLASS_LIKELY_SPILLED_P default_class_likely_spilled_p
    mtcsTarget->class_likely_spilled_p=classLikelySpilledP_cb;
    //原型 targetm.gen_ccmp_first #define TARGET_GEN_CCMP_FIRST NULL
    mtcsTarget->gen_ccmp_first=NULL;
    //原型 targetm.gen_ccmp_next #define TARGET_GEN_CCMP_NEXT NULL
    mtcsTarget->gen_ccmp_next=NULL;
    //原型 targetm.stack_protect_fail () #define TARGET_STACK_PROTECT_FAIL default_external_stack_protect_fail
    mtcsTarget->stack_protect_fail=stackProtectFail_cb;
    //原型 targetm.class_max_nregs ((reg_class_t) i, (machine_mode) m) #define TARGET_CLASS_MAX_NREGS default_class_max_nregs
    mtcsTarget->class_max_nregs=classMaxNregs_cb;
    //原型 targetm.expand_builtin (exp, target, subtarget, mode, ignore) #define TARGET_EXPAND_BUILTIN nvptx_expand_builtin
    mtcsTarget->expand_builtin=expandBuiltin_cb;
    //原型 targetm.libc_has_function #define TARGET_LIBC_HAS_FUNCTION nvptx_libc_has_function
    mtcsTarget->libc_has_function=libcHasFunction_cb;
    //原型 targetm.floatn_mode (n, extended) #define TARGET_FLOATN_MODE default_floatn_mode
    mtcsTarget->floatn_mode=floatnMode_cb;
    //原型 targetm.decimal_float_supported_p() #define TARGET_DECIMAL_FLOAT_SUPPORTED_P default_decimal_float_supported_p
    mtcsTarget->decimal_float_supported_p=decimalFloatSupportedP_cb;
    //原型 targetm.preferred_reload_class(x, rclass);#define TARGET_PREFERRED_RELOAD_CLASS default_preferred_reload_class
    mtcsTarget->preferred_reload_class=preferredReloadClass_cb;
    //原型 targetm.secondary_reload (to_p, x, rclass, mode, &sri); #define TARGET_SECONDARY_RELOAD default_secondary_reload
    mtcsTarget->secondary_reload=secondaryReload_cb;
    //原型  targetm.has_ifunc_p () #define TARGET_HAS_IFUNC_P default_has_ifunc_p
    mtcsTarget->has_ifunc_p=hasIfuncP_cb;
    //原型 targetm.vector_alignment #define TARGET_VECTOR_ALIGNMENT nvptx_vector_alignment
    mtcsTarget->vector_alignment=vectorAlignment_cb;
    //原型 targetm.machine_dependent_reorg () #define TARGET_MACHINE_DEPENDENT_REORG nvptx_reorg
    mtcsTarget->machine_dependent_reorg=machineDependentReorg_cb;
    //原型  #define TARGET_NO_REGISTER_ALLOCATION true nvptx=true
    mtcsTarget->no_register_allocation = true;

    mtcs_machine_set_vectorize(mtcsTarget->mtcsMachine,(TargetVectorize *)target_ptx_vectorize_new(mtcsTarget->mtcsMode));
    mtcs_machine_set_addr_space(mtcsTarget->mtcsMachine,(TargetAddrSpace *)target_ptx_addr_space_new(mtcsTarget->mtcsMode));
    mtcs_machine_set_option(mtcsTarget->mtcsMachine,(TargetOption *)target_ptx_option_new(mtcsTarget->mtcsMode));
    mtcs_machine_set_common(mtcsTarget->mtcsMachine,(TargetCommon *)target_ptx_common_new(mtcsTarget->mtcsMode));
    mtcs_machine_set_asm_out(mtcsTarget->mtcsMachine,(TargetAsmOut *)target_ptx_asm_out_new(mtcsTarget->mtcsMode));
    mtcs_machine_set_calls(mtcsTarget->mtcsMachine,(TargetCalls *)target_ptx_calls_new(mtcsTarget->mtcsMode));
    mtcs_machine_set_tmrtx(mtcsTarget->mtcsMachine,(TargetRtx *)target_ptx_rtx_new(mtcsTarget->mtcsMode));

    ///----------------结束target域赋值------------------------
    self->func_decls=n_string_new("");//记录函数声明，在最后替换 REPLACE_FUNC_DECL_LOCATION
    createComponent(self);
    self->nvptxOptimize=-1;
    target_calls_set_custom_function_descriptors(mtcsTarget->mtcsMachine->calls,-1);
    mtcs_target_set_supports_aliases(mtcsTarget,((MtcsPtxOptions*)mtcsTarget->mtcsOptions)->x_nvptx_alias);
}

static void createAllPass(MtcsPtx *self)
{
   MtcsTarget *mtcsTarget=(MtcsTarget *)self;
   MtcsMode *mtcsMode=mtcsTarget->mtcsMode;
   MtcsPassMgr *mtcsPassMgr=mtcs_target_get_pass_mgr(mtcsTarget);


   //以上pass都是gimple,下面的pass全是rtl
   //176. expand 原型 expand cfgexpand.cc
   MtcsPass *pass=(MtcsPass*)mtcs_pass_expand_new(mtcsMode);
   mtcs_pass_mgr_add_all_pass(mtcsPassMgr,pass);
   //177. *rest_of_compilation 原型 *rest_of_compilation pass.cc
   MtcsPass *restOfcompilation=(MtcsPass*)mtcs_pass_rest_of_compilation_new(mtcsMode);
   mtcs_pass_mgr_add_all_pass(mtcsPassMgr,restOfcompilation);
   //第一级 INSERT restOfcompilation 3个
   //178. vregs 原型 vregs function.cc
   pass=(MtcsPass*)mtcs_pass_instantiate_virtual_regs_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //179 into_cfglayout 原型 into_cfglayout cfgrtl.cc
   pass=(MtcsPass*)mtcs_pass_into_cfg_layout_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);

   //180 jump 原型 jump cfgcleanup.cc
   pass=(MtcsPass*)mtcs_pass_jump_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);

   //181 subreg1 原型 subreg1 lower-subreg.cc
   pass=(MtcsPass*)mtcs_pass_lower_subreg_new(mtcsMode,1);
   mtcs_pass_add_pass(restOfcompilation,pass);

   //182 dfinit 原型 dfinit df-core.cc
   pass=(MtcsPass*)mtcs_pass_df_initialize_opt_new(mtcsMode,1);
   mtcs_pass_add_pass(restOfcompilation,pass);

   //183 cse1 原型 cse1  cse.cc
   pass=(MtcsPass*)mtcs_pass_cse1_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);

   //184 fwprop1 原型 fwprop1  fwprop.cc
   pass=(MtcsPass*)mtcs_pass_fwprop1_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);

   //185 cprop 原型 cprop  cprop.cc
   pass=(MtcsPass*)mtcs_pass_cprop_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);

   //186 rtl pre 原型 rtl pre  gcse.cc
   pass=(MtcsPass*)mtcs_pass_rtl_pre_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);

   //187 hoist 原型 hoist  gcse.cc
   pass=(MtcsPass*)mtcs_pass_hoist_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);

   //188 cprop 原型 cprop  cprop.cc 重复 cprop 第二次创建
   pass=(MtcsPass*)mtcs_pass_cprop_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //189  原型 "store_motion" store-motion.cc mtcs不需要
   //190 cse_local 原型 cse_local  cse.cc  nvptx pass_cse_after_global_opts
   pass=(MtcsPass*)mtcs_pass_cse_local_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //191 ce1 原型 ce1  ifcvt.cc
   pass=(MtcsPass*)mtcs_pass_ifcvt_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //192 reginfo 原型 reginfo  reginfo.cc
   pass=(MtcsPass*)mtcs_pass_reg_info_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);

   //193 loop2 原型 loop2  loop-init.cc
   MtcsPass *loop2=(MtcsPass*)mtcs_pass_loop2_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,loop2);
   //加入到 loop2中
   //193 loop2_init 原型 loop2_init  loop-init.cc
   pass=(MtcsPass*)mtcs_pass_loop_init_new(mtcsMode);
   mtcs_pass_add_pass(loop2,pass);
   //194 loop2_invariant 原型 loop2_invariant  loop-init.cc
   pass=(MtcsPass*)mtcs_loop_invariant_new(mtcsMode);
   mtcs_pass_add_pass(loop2,pass);
   //195 loop2_unroll 原型 loop2_unroll  loop-init.cc
   pass=(MtcsPass*)mtcs_loop_unroll_new(mtcsMode);
   mtcs_pass_add_pass(loop2,pass);
   //196 loop2_doloop 原型 loop2_doloop  loop-init.cc
   pass=(MtcsPass*)mtcs_pass_doloop_new(mtcsMode);
   mtcs_pass_add_pass(loop2,pass);
   //197 loop2_done 原型 loop2_done  loop-init.cc
   pass=(MtcsPass*)mtcs_pass_loop_done_new(mtcsMode);
   mtcs_pass_add_pass(loop2,pass);
   //从loop2出来，重新加入到restOfcompilation
   //198 subreg2 原型 subreg2 lower-subreg.cc
   pass=(MtcsPass*)mtcs_pass_lower_subreg_new(mtcsMode,2);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //199 web 原型 web web.cc
   pass=(MtcsPass*)mtcs_web_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //200 cprop 原型 cprop  cprop.cc 重复 cprop 第三次创建
   pass=(MtcsPass*)mtcs_pass_cprop_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //201 cse2  原型 cee2 cse.cc
   pass=(MtcsPass*)mtcs_pass_cse2_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //202 dse1  原型 dse1 dse.cc
   pass=(MtcsPass*)mtcs_pass_dse1_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //203 fwprop2  原型 pass_rtl_fwprop_addr fwprop2 fwprop.cc
   pass=(MtcsPass*)mtcs_pass_fwprop2_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //204 auto_inc_dec 原型 pass_inc_dec  auto_inc_dec auto-inc-dec.cc 不执行 跳过
   //205 init-regs  原型 pass_initialize_regs init-regs init-regs.cc
   pass=(MtcsPass*)mtcs_pass_init_regs_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //206 ud_dce  原型 pass_ud_rtl_dce ud_dce  dce.cc
   pass=(MtcsPass*)mtcs_pass_ud_dce_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //gcc15新加入的 ext_dce ext-dce.cc
   pass=(MtcsPass*)mtcs_ext_dce_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //207 combine  原型 pass_combine combine  combine.cc
   pass=(MtcsPass*)mtcs_pass_combine_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //208 ce2  原型 pass_if_after_combine ce2  ifcvt.cc
   pass=(MtcsPass*)mtcs_pass_if_after_combine_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //209 jump_after_combine  原型 pass_jump_after_combine jump_after_combine  cfgcleanup.cc
   pass=(MtcsPass*)mtcs_pass_jump_after_combine(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //210 bbpart  原型 pass_partition_blocks bbpart  bb-reorder.cc
   pass=(MtcsPass*)mtcs_pass_bb_part_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //211 outof_cfglayout  原型 pass_outof_cfg_layout_mode outof_cfglayout  cfgrtl.cc
   pass=(MtcsPass*)mtcs_pass_outof_cfg_layout_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //212 split1  原型 pass_split_all_insns split1  recog.cc
   pass=(MtcsPass*)mtcs_pass_split_all_insns_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //213 subreg3  原型 pass_lower_subreg3 subreg3  lower-subreg.cc
   pass=(MtcsPass*)mtcs_pass_lower_subreg_new(mtcsMode,3);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //214  no-opt dfinit  原型 pass_df_initialize_no_opt no-opt dfinit  df-core.cc
   pass=(MtcsPass*)mtcs_pass_df_initialize_no_opt_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //215  *stack_ptr_mod  原型 pass_stack_ptr_mod  *stack_ptr_mod stack-ptr-mod.cc
   pass=(MtcsPass*)mtcs_pass_stack_ptr_mod_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //216 mode_sw 原型 pass_mode_switching mode_sw  mode-switching.cc 依赖#ifdef OPTIMIZE_MODE_SWITCHING 不执行 跳过
   //217 asmcons  原型 pass_match_asm_constraints  asmcons function.cc
   pass=(MtcsPass*)mtcs_pass_match_asm_constraints_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //218 sms 原型 pass_sms sms modulo-sched.cc 依赖 (optimize > 0 && flag_modulo_sched); 不执行 跳过
   //219 lr_shrinkage 原型 pass_live_range_shrinkage lr_shrinkage sched-rgn.cc 依赖 #ifdef INSN_SCHEDULING; 不执行 跳过
   //220 sched1 原型 pass_sched sched1 sched-rgn.cc 依赖 #ifdef INSN_SCHEDULING; 不执行 跳过
   //221 early_remat 原型 pass_early_remat early_remat early-remat.cc 依赖 optimize > 1 && NUM_POLY_INT_COEFFS > 1; 不执行 跳过
   //222 ira 原型 pass_ira ira ira.cc 依赖 !targetm.no_register_allocation;; 不执行 跳过
   //223 reload 原型 pass_reload reload ira.cc 依赖 !targetm.no_register_allocation;; 不执行 跳过

   //224 *all-postreload 原型 pass_postreload *all-postreload passes.cc
   MtcsPass *allPostreloadPass=(MtcsPass*)mtcs_pass_post_reload_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,allPostreloadPass);
   //225 postreload  原型 pass_postreload_cse postreload postreload.cc
   pass=(MtcsPass*)mtcs_post_reload_new(mtcsMode);
   mtcs_pass_add_pass(allPostreloadPass,pass);
   /*暂时不需要 23个 pass
   NEXT_PASS (pass_gcse2, 1);
   NEXT_PASS (pass_split_after_reload, 1);
   NEXT_PASS (pass_ree, 1);
   NEXT_PASS (pass_compare_elim_after_reload, 1);
   NEXT_PASS (pass_thread_prologue_and_epilogue, 1);
   NEXT_PASS (pass_rtl_dse2, 1);
   NEXT_PASS (pass_stack_adjustments, 1);
   NEXT_PASS (pass_jump2, 1);
   NEXT_PASS (pass_duplicate_computed_gotos, 1);
   NEXT_PASS (pass_sched_fusion, 1);
   NEXT_PASS (pass_peephole2, 1);
   NEXT_PASS (pass_if_after_reload, 1);
   NEXT_PASS (pass_regrename, 1);
   NEXT_PASS (pass_fold_mem_offsets, 1);
   NEXT_PASS (pass_cprop_hardreg, 1);
   NEXT_PASS (pass_fast_rtl_dce, 1);
   NEXT_PASS (pass_reorder_blocks, 1);
   NEXT_PASS (pass_leaf_regs, 1);
   NEXT_PASS (pass_split_before_sched2, 1);
   NEXT_PASS (pass_sched2, 1);
   NEXT_PASS (pass_stack_regs, 1);
   PUSH_INSERT_PASSES_WITHIN (pass_stack_regs)
   NEXT_PASS (pass_split_before_regstack, 1);
   NEXT_PASS (pass_stack_regs_run, 1);
   POP_INSERT_PASSES ()
   POP_INSERT_PASSES ()
   */
   //重回到第一级
   //248 late_pro_and_epilogue  原型 pass_late_thread_prologue_and_epilogue late_pro_and_epilogue function.cc
   pass=(MtcsPass*)mtcs_pass_late_thread_prologue_and_epilogue_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //249 *all-late_compilation  原型 pass_late_compilation *all-late_compilation passes.cc
   MtcsPass *allLatecompilateion=(MtcsPass*)mtcs_pass_all_late_compilation_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,allLatecompilateion);
   //250 zero_call_used_regs  原型 pass_zero_call_used_regs zero_call_used_regs function.cc
   pass=(MtcsPass*)mtcs_pass_zero_call_use_regs_new(mtcsMode);
   mtcs_pass_add_pass(allLatecompilateion,pass);
   //251 alignments  原型 pass_compute_alignments alignments final.cc
   pass=(MtcsPass*)mtcs_pass_compute_alignments_new(mtcsMode);
   mtcs_pass_add_pass(allLatecompilateion,pass);
   //252 vartrack  原型 pass_variable_tracking vartrack var-tracking.cc 跳过
   //253 *free_cfg  原型 pass_free_cfg *free_cfg  cfgrtl.cc
   pass=(MtcsPass*)mtcs_pass_free_cfg_new(mtcsMode);
   mtcs_pass_add_pass(allLatecompilateion,pass);
   //254 mach  原型 pass_machine_reorg mach reorg.cc
   pass=(MtcsPass*)mtcs_pass_mach_new(mtcsMode);
   mtcs_pass_add_pass(allLatecompilateion,pass);
   //255 barriers  原型 pass_cleanup_barriers barriers jump.cc
   pass=(MtcsPass*)mtcs_pass_cleanup_barriers_new(mtcsMode);
   mtcs_pass_add_pass(allLatecompilateion,pass);
   //256 dbr  原型 pass_delay_slots dbr reorg.cc
   pass=(MtcsPass*)mtcs_pass_delay_slots_new(mtcsMode);
   mtcs_pass_add_pass(allLatecompilateion,pass);
   //257 split5  原型 pass_split_for_shorten_branches split5 recog.cc
   pass=(MtcsPass*)mtcs_pass_split_for_shorten_branches_new(mtcsMode);
   mtcs_pass_add_pass(allLatecompilateion,pass);
   //258 eh_ranges  原型 pass_convert_to_eh_region_ranges eh_ranges except.cc
   pass=(MtcsPass*)mtcs_pass_convert_to_eh_region_ranges_new(mtcsMode);
   mtcs_pass_add_pass(allLatecompilateion,pass);
   //259 shorten  原型 pass_shorten_branches shorten final.cc
   pass=(MtcsPass*)mtcs_pass_shorten_branches_new(mtcsMode);
   mtcs_pass_add_pass(allLatecompilateion,pass);
   //260 nothrow  原型 pass_set_nothrow_function_flags nothrow except.cc
   pass=(MtcsPass*)mtcs_pass_set_nothrow_function_flags_new(mtcsMode);
   mtcs_pass_add_pass(allLatecompilateion,pass);
   //261 dwarf2  原型 pass_dwarf2_frame dwarf2 dwarf2cfi.cc
   pass=(MtcsPass*)mtcs_pass_dwarf2_frame_new(mtcsMode);
   mtcs_pass_add_pass(allLatecompilateion,pass);
   //262 final  原型 pass_final final final.cc
   pass=(MtcsPass*)mtcs_pass_final_new(mtcsMode);
   mtcs_pass_add_pass(allLatecompilateion,pass);
   //263 dfinish  原型 pass_df_finish dfinish df-core.cc
   pass=(MtcsPass*)mtcs_pass_df_finish_new(mtcsMode);
   mtcs_pass_add_pass(restOfcompilation,pass);
   //264 *clean_state  原型 pass_clean_state *clean_state  final.cc
   pass=(MtcsPass*)mtcs_pass_clean_state_new(mtcsMode);
   mtcs_pass_mgr_add_all_pass(mtcsPassMgr,pass);
}

/* Check NAME for special function names and redirect them by returning a
   replacement.  This applies to malloc, free and realloc, for which we
   want to use libgcc wrappers, and call, which triggers a bug in
   ptxas.  We can't use TARGET_MANGLE_DECL_ASSEMBLER_NAME, as that's
   not active in an offload compiler -- the names are all set by the
   host-side compiler.  */
//获取需要替换的函数名
//原型 static const char * nvptx_name_replacement (const char *name) nvptx.cc
const char *mtcs_ptx_get_replace_function_name(MtcsPtx *self,const char *origName)
{
   n_debug("mtcsptx.c -----nvptx.cc -----4-- nvptx_name_replacement  替换函数名 name:%s\n",origName);
   if (strcmp (origName, "call") == 0)
      return "__nvptx_call";
   if (strcmp (origName, "malloc") == 0)
      return "__nvptx_malloc";
   if (strcmp (origName, "free") == 0)
      return "__nvptx_free";
   if (strcmp (origName, "realloc") == 0)
      return "__nvptx_realloc";
   if (strcmp (origName, "printf") == 0) //ptx没有printf系统调用，只有vprintf(fmt,va_list)//zclei
      return "vprintf";
   //if (strcmp (name, "puts") == 0) //ptx没有puts系统调用，只有vprintf(fmt,va_list)//zclei
   // return "vprintf";
   const char *replace=mtcs_ptx_math_get_replace_funcname(self->mtcsPtxMath,origName);
   if(replace)
      return replace;
   return origName;
}

/**
 * 当完成MTCS汇编后，调用平台的 mtcs_target_get_link_funcname 方法。
 */
static char *getLinkFuncName_cb(MtcsTarget *mtcsTarget)
{
   MtcsPtx *self =( MtcsPtx *)mtcsTarget;
   return  mtcs_ptx_math_get_link_funcname(self->mtcsPtxMath);
}

/**
 * 由于同一个平台有多个版本号，往往这些版本号是全局的，所以在编译前用每个target的版本号设为全局变量。
 */
static void publishVersion_cb(MtcsTarget *mtcsTarget)
{
   //声明在ptx-common.h 初始定义在mtcsptxoptions.c 重要影响 mtcs_ptx.md中的 TARGET_PTX_7.3 TARGET_SM52
   mtcs_ptx_isa_option=mtcsTarget->platformInfo.isa;
   mtcs_ptx_version_option=mtcsTarget->platformInfo.version;
}

MtcsPtx *mtcs_ptx_new_full(int isa,int ptxVersion)
{
   MtcsPtx *self = n_slice_alloc0 (sizeof(MtcsPtx));
   MtcsMode *mtcsMode=(MtcsMode *)mtcs_ptx_mode_new();//必须先创建 下一步创建component依赖mtcsMode;
   mtcs_mode_set_target(mtcsMode,(npointer)self);
   ((MtcsTarget*)self)->mtcsMode=mtcsMode;
   /*HardRegSet初始化需要 mtcsReg中的hardRegElement,所以先创建mtcsReg,
   *下面的组件可以从mtcsXXxInit方法中获取mtcsreg中的hardRegElement来初始化HardRegSet
   */
   ((MtcsTarget*)self)->mtcsReg=(MtcsReg *)mtcs_ptx_reg_new(mtcsMode);//平台 寄存器
   mtcs_reg_init_reg_sets(((MtcsTarget*)self)->mtcsReg);
   mtcs_target_init((MtcsTarget*)self);
   mtcsPtxInit(self);
   mtcs_libfuncs_set_normalib_def(((MtcsTarget*)self)->mtcsLibfuncs,(struct mtcs_optab_libcall_d*)ptx_get_optab_libcall());
   mtcs_libfuncs_set_convlib_def(((MtcsTarget*)self)->mtcsLibfuncs,(struct mtcs_convert_optab_libcall_d*)ptx_get_convert_optab_libcall());
   mtcs_target_set_platform_name((MtcsTarget*)self,"cuda");
   mtcs_target_set_isa((MtcsTarget*)self,isa);
   mtcs_target_set_version((MtcsTarget*)self,ptxVersion);
   //mtcs代码已经过 gimple regular ipa lateipa
   createAllPass(self);
   self->mtcsPtxMath=mtcs_ptx_math_new(mtcsMode);
   ((MtcsTarget*)self)->getLinkFuncName=getLinkFuncName_cb;
   ((MtcsTarget*)self)->publishVersion=publishVersion_cb;
   return self;
}

MtcsPtx *mtcs_ptx_new()
{
   return mtcs_ptx_new_full(MTCS_PTX_ISA_SM_75,MTCS_PTX_VERSION_7_8);
}


