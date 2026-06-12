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
#include "tree-pass.h"
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
#include "expmed.h"
#include "expr.h"
#include "fold-const.h"
#include "common/common-targhooks.h"

#include "symbol-summary.h"
#include "tree-vrp.h"
#include "sreal.h"
#include "gimple.h"
#include "gimple-ssa.h"
#include "gimple-iterator.h"
#include "gimple-fold.h"
#include "gimplify.h"
#include "gimplify-me.h"
#include "tree-ssanames.h"

#include "aet/aetprinttree.h"
#include "../mtcstarget.h"
#include "mtcsptxmath.h"
#include "ptx-common.h"
#include "mtcsptxpreds.h"
#include "mtcsptxfunc.h"
#include "gen/ptx-insn-modes.h"

//temp/ptx/中 nvvm_compile_libdevice.c 用 libdevice.10.bc 生成 libdevice.ptx 注意有版本compute_80
//用scan_ptx_libdevice2.c从libdevice.ptx生成 kMathMap表。
// math_map.h (generated from PTX)
typedef struct
{
   const char* mathh;
   const char* libdevice;
} MathMap;
static const MathMap kMathMap[] = {
    {"clz", "__nv_clz"},
    {"clzll", "__nv_clzll"},
    {"popc", "__nv_popc"},
    {"popcll", "__nv_popcll"},
    {"byte_perm", "__nv_byte_perm"},
    {"min", "__nv_min"},
    {"umin", "__nv_umin"},
    {"llmin", "__nv_llmin"},
    {"ullmin", "__nv_ullmin"},
    {"max", "__nv_max"},
    {"umax", "__nv_umax"},
    {"llmax", "__nv_llmax"},
    {"ullmax", "__nv_ullmax"},
    {"mulhi", "__nv_mulhi"},
    {"umulhi", "__nv_umulhi"},
    {"mul64hi", "__nv_mul64hi"},
    {"umul64hi", "__nv_umul64hi"},
    {"mul24", "__nv_mul24"},
    {"umul24", "__nv_umul24"},
    {"brev", "__nv_brev"},
    {"brevll", "__nv_brevll"},
    {"sad", "__nv_sad"},
    {"usad", "__nv_usad"},
    {"abs", "__nv_abs"},
    {"llabs", "__nv_llabs"},
    {"floorf", "__nv_floorf"},
    {"floor", "__nv_floor"},
    {"fabsf", "__nv_fabsf"},
    {"fabs", "__nv_fabs"},
    {"rcp64h", "__nv_rcp64h"},
    {"fminf", "__nv_fminf"},
    {"fmaxf", "__nv_fmaxf"},
    {"rsqrtf", "__nv_rsqrtf"},
    {"fmin", "__nv_fmin"},
    {"fmax", "__nv_fmax"},
    {"rsqrt", "__nv_rsqrt"},
    {"ceil", "__nv_ceil"},
    {"trunc", "__nv_trunc"},
    {"exp2f", "__nv_exp2f"},
    {"truncf", "__nv_truncf"},
    {"ceilf", "__nv_ceilf"},
    {"saturatef", "__nv_saturatef"},
    {"fmaf_rn", "__nv_fmaf_rn"},
    {"fmaf_rz", "__nv_fmaf_rz"},
    {"fmaf_rd", "__nv_fmaf_rd"},
    {"fmaf_ru", "__nv_fmaf_ru"},
    {"fmaf_ieee_rn", "__nv_fmaf_ieee_rn"},
    {"fmaf_ieee_rz", "__nv_fmaf_ieee_rz"},
    {"fmaf_ieee_rd", "__nv_fmaf_ieee_rd"},
    {"fmaf_ieee_ru", "__nv_fmaf_ieee_ru"},
    {"fma_rn", "__nv_fma_rn"},
    {"fma_rz", "__nv_fma_rz"},
    {"fma_rd", "__nv_fma_rd"},
    {"fma_ru", "__nv_fma_ru"},
    {"fast_fdividef", "__nv_fast_fdividef"},
    {"fdiv_rn", "__nv_fdiv_rn"},
    {"fdiv_rz", "__nv_fdiv_rz"},
    {"fdiv_rd", "__nv_fdiv_rd"},
    {"fdiv_ru", "__nv_fdiv_ru"},
    {"frcp_rn", "__nv_frcp_rn"},
    {"frcp_rz", "__nv_frcp_rz"},
    {"frcp_rd", "__nv_frcp_rd"},
    {"frcp_ru", "__nv_frcp_ru"},
    {"fsqrt_rn", "__nv_fsqrt_rn"},
    {"fsqrt_rz", "__nv_fsqrt_rz"},
    {"fsqrt_rd", "__nv_fsqrt_rd"},
    {"fsqrt_ru", "__nv_fsqrt_ru"},
    {"ddiv_rn", "__nv_ddiv_rn"},
    {"ddiv_rz", "__nv_ddiv_rz"},
    {"ddiv_rd", "__nv_ddiv_rd"},
    {"ddiv_ru", "__nv_ddiv_ru"},
    {"drcp_rn", "__nv_drcp_rn"},
    {"drcp_rz", "__nv_drcp_rz"},
    {"drcp_rd", "__nv_drcp_rd"},
    {"drcp_ru", "__nv_drcp_ru"},
    {"dsqrt_rn", "__nv_dsqrt_rn"},
    {"dsqrt_rz", "__nv_dsqrt_rz"},
    {"dsqrt_rd", "__nv_dsqrt_rd"},
    {"dsqrt_ru", "__nv_dsqrt_ru"},
    {"sqrtf", "__nv_sqrtf"},
    {"sqrt", "__nv_sqrt"},
    {"dadd_rn", "__nv_dadd_rn"},
    {"dadd_rz", "__nv_dadd_rz"},
    {"dadd_rd", "__nv_dadd_rd"},
    {"dadd_ru", "__nv_dadd_ru"},
    {"dmul_rn", "__nv_dmul_rn"},
    {"dmul_rz", "__nv_dmul_rz"},
    {"dmul_rd", "__nv_dmul_rd"},
    {"dmul_ru", "__nv_dmul_ru"},
    {"fadd_rd", "__nv_fadd_rd"},
    {"fadd_ru", "__nv_fadd_ru"},
    {"fmul_rd", "__nv_fmul_rd"},
    {"fmul_ru", "__nv_fmul_ru"},
    {"fadd_rn", "__nv_fadd_rn"},
    {"fadd_rz", "__nv_fadd_rz"},
    {"fmul_rn", "__nv_fmul_rn"},
    {"fmul_rz", "__nv_fmul_rz"},
    {"double2float_rn", "__nv_double2float_rn"},
    {"double2float_rz", "__nv_double2float_rz"},
    {"double2float_rd", "__nv_double2float_rd"},
    {"double2float_ru", "__nv_double2float_ru"},
    {"double2int_rn", "__nv_double2int_rn"},
    {"double2int_rz", "__nv_double2int_rz"},
    {"double2int_rd", "__nv_double2int_rd"},
    {"double2int_ru", "__nv_double2int_ru"},
    {"double2uint_rn", "__nv_double2uint_rn"},
    {"double2uint_rz", "__nv_double2uint_rz"},
    {"double2uint_rd", "__nv_double2uint_rd"},
    {"double2uint_ru", "__nv_double2uint_ru"},
    {"int2double_rn", "__nv_int2double_rn"},
    {"uint2double_rn", "__nv_uint2double_rn"},
    {"float2int_rn", "__nv_float2int_rn"},
    {"float2int_rz", "__nv_float2int_rz"},
    {"float2int_rd", "__nv_float2int_rd"},
    {"float2int_ru", "__nv_float2int_ru"},
    {"float2uint_rn", "__nv_float2uint_rn"},
    {"float2uint_rz", "__nv_float2uint_rz"},
    {"float2uint_rd", "__nv_float2uint_rd"},
    {"float2uint_ru", "__nv_float2uint_ru"},
    {"int2float_rn", "__nv_int2float_rn"},
    {"int2float_rz", "__nv_int2float_rz"},
    {"int2float_rd", "__nv_int2float_rd"},
    {"int2float_ru", "__nv_int2float_ru"},
    {"uint2float_rn", "__nv_uint2float_rn"},
    {"uint2float_rz", "__nv_uint2float_rz"},
    {"uint2float_rd", "__nv_uint2float_rd"},
    {"uint2float_ru", "__nv_uint2float_ru"},
    {"hiloint2double", "__nv_hiloint2double"},
    {"double2loint", "__nv_double2loint"},
    {"double2hiint", "__nv_double2hiint"},
    {"float2ll_rn", "__nv_float2ll_rn"},
    {"float2ll_rz", "__nv_float2ll_rz"},
    {"float2ll_rd", "__nv_float2ll_rd"},
    {"float2ll_ru", "__nv_float2ll_ru"},
    {"float2ull_rn", "__nv_float2ull_rn"},
    {"float2ull_rz", "__nv_float2ull_rz"},
    {"float2ull_rd", "__nv_float2ull_rd"},
    {"float2ull_ru", "__nv_float2ull_ru"},
    {"double2ll_rn", "__nv_double2ll_rn"},
    {"double2ll_rz", "__nv_double2ll_rz"},
    {"double2ll_rd", "__nv_double2ll_rd"},
    {"double2ll_ru", "__nv_double2ll_ru"},
    {"double2ull_rn", "__nv_double2ull_rn"},
    {"double2ull_rz", "__nv_double2ull_rz"},
    {"double2ull_rd", "__nv_double2ull_rd"},
    {"double2ull_ru", "__nv_double2ull_ru"},
    {"ll2float_rn", "__nv_ll2float_rn"},
    {"ll2float_rz", "__nv_ll2float_rz"},
    {"ll2float_rd", "__nv_ll2float_rd"},
    {"ll2float_ru", "__nv_ll2float_ru"},
    {"ull2float_rn", "__nv_ull2float_rn"},
    {"ull2float_rz", "__nv_ull2float_rz"},
    {"ull2float_rd", "__nv_ull2float_rd"},
    {"ull2float_ru", "__nv_ull2float_ru"},
    {"ll2double_rn", "__nv_ll2double_rn"},
    {"ll2double_rz", "__nv_ll2double_rz"},
    {"ll2double_rd", "__nv_ll2double_rd"},
    {"ll2double_ru", "__nv_ll2double_ru"},
    {"ull2double_rn", "__nv_ull2double_rn"},
    {"ull2double_rz", "__nv_ull2double_rz"},
    {"ull2double_rd", "__nv_ull2double_rd"},
    {"ull2double_ru", "__nv_ull2double_ru"},
    {"float2half_rn", "__nv_float2half_rn"},
    {"half2float", "__nv_half2float"},
    {"int_as_float", "__nv_int_as_float"},
    {"float_as_int", "__nv_float_as_int"},
    {"uint_as_float", "__nv_uint_as_float"},
    {"float_as_uint", "__nv_float_as_uint"},
    {"longlong_as_double", "__nv_longlong_as_double"},
    {"double_as_longlong", "__nv_double_as_longlong"},
    {"fast_sinf", "__nv_fast_sinf"},
    {"fast_cosf", "__nv_fast_cosf"},
    {"fast_log2f", "__nv_fast_log2f"},
    {"fast_logf", "__nv_fast_logf"},
    {"fast_expf", "__nv_fast_expf"},
    {"fast_tanhf", "__nv_fast_tanhf"},
    {"tanhf", "__nv_tanhf"},
    {"fast_tanf", "__nv_fast_tanf"},
    {"fast_sincosf", "__nv_fast_sincosf"},
    {"fast_exp10f", "__nv_fast_exp10f"},
    {"fast_log10f", "__nv_fast_log10f"},
    {"fast_powf", "__nv_fast_powf"},
    {"hadd", "__nv_hadd"},
    {"rhadd", "__nv_rhadd"},
    {"uhadd", "__nv_uhadd"},
    {"urhadd", "__nv_urhadd"},
    {"fsub_rn", "__nv_fsub_rn"},
    {"fsub_rz", "__nv_fsub_rz"},
    {"fsub_rd", "__nv_fsub_rd"},
    {"fsub_ru", "__nv_fsub_ru"},
    {"frsqrt_rn", "__nv_frsqrt_rn"},
    {"ffs", "__nv_ffs"},
    {"ffsll", "__nv_ffsll"},
    {"rintf", "__nv_rintf"},
    {"llrintf", "__nv_llrintf"},
    {"nearbyintf", "__nv_nearbyintf"},
    {"isnanf", "__nv_isnanf"},
    {"signbitf", "__nv_signbitf"},
    {"copysignf", "__nv_copysignf"},
    {"finitef", "__nv_finitef"},
    {"isinff", "__nv_isinff"},
    {"nextafterf", "__nv_nextafterf"},
    {"nanf", "__nv_nanf"},
    {"sinf", "__nv_sinf"},
    {"cosf", "__nv_cosf"},
    {"sincosf", "__nv_sincosf"},
    {"sinpif", "__nv_sinpif"},
    {"cospif", "__nv_cospif"},
    {"sincospif", "__nv_sincospif"},
    {"tanf", "__nv_tanf"},
    {"log2f", "__nv_log2f"},
    {"expf", "__nv_expf"},
    {"exp10f", "__nv_exp10f"},
    {"coshf", "__nv_coshf"},
    {"sinhf", "__nv_sinhf"},
    {"atan2f", "__nv_atan2f"},
    {"atanf", "__nv_atanf"},
    {"asinf", "__nv_asinf"},
    {"acosf", "__nv_acosf"},
    {"logf", "__nv_logf"},
    {"log10f", "__nv_log10f"},
    {"log1pf", "__nv_log1pf"},
    {"acoshf", "__nv_acoshf"},
    {"asinhf", "__nv_asinhf"},
    {"atanhf", "__nv_atanhf"},
    {"expm1f", "__nv_expm1f"},
    {"hypotf", "__nv_hypotf"},
    {"rhypotf", "__nv_rhypotf"},
    {"norm3df", "__nv_norm3df"},
    {"rnorm3df", "__nv_rnorm3df"},
    {"norm4df", "__nv_norm4df"},
    {"rnorm4df", "__nv_rnorm4df"},
    {"normf", "__nv_normf"},
    {"rnormf", "__nv_rnormf"},
    {"cbrtf", "__nv_cbrtf"},
    {"rcbrtf", "__nv_rcbrtf"},
    {"j0f", "__nv_j0f"},
    {"j1f", "__nv_j1f"},
    {"y0f", "__nv_y0f"},
    {"y1f", "__nv_y1f"},
    {"ynf", "__nv_ynf"},
    {"jnf", "__nv_jnf"},
    {"cyl_bessel_i0f", "__nv_cyl_bessel_i0f"},
    {"cyl_bessel_i1f", "__nv_cyl_bessel_i1f"},
    {"erff", "__nv_erff"},
    {"erfinvf", "__nv_erfinvf"},
    {"erfcf", "__nv_erfcf"},
    {"erfcxf", "__nv_erfcxf"},
    {"erfcinvf", "__nv_erfcinvf"},
    {"normcdfinvf", "__nv_normcdfinvf"},
    {"normcdff", "__nv_normcdff"},
    {"lgammaf", "__nv_lgammaf"},
    {"ldexpf", "__nv_ldexpf"},
    {"scalbnf", "__nv_scalbnf"},
    {"frexpf", "__nv_frexpf"},
    {"modff", "__nv_modff"},
    {"fmodf", "__nv_fmodf"},
    {"remainderf", "__nv_remainderf"},
    {"remquof", "__nv_remquof"},
    {"fmaf", "__nv_fmaf"},
    {"powif", "__nv_powif"},
    {"powi", "__nv_powi"},
    {"powf", "__nv_powf"},
    {"tgammaf", "__nv_tgammaf"},
    {"roundf", "__nv_roundf"},
    {"llroundf", "__nv_llroundf"},
    {"fdimf", "__nv_fdimf"},
    {"ilogbf", "__nv_ilogbf"},
    {"logbf", "__nv_logbf"},
    {"rint", "__nv_rint"},
    {"llrint", "__nv_llrint"},
    {"nearbyint", "__nv_nearbyint"},
    {"signbitd", "__nv_signbitd"},
    {"isfinited", "__nv_isfinited"},
    {"isinfd", "__nv_isinfd"},
    {"isnand", "__nv_isnand"},
    {"copysign", "__nv_copysign"},
    {"sincos", "__nv_sincos"},
    {"sincospi", "__nv_sincospi"},
    {"sin", "__nv_sin"},
    {"cos", "__nv_cos"},
    {"sinpi", "__nv_sinpi"},
    {"cospi", "__nv_cospi"},
    {"tan", "__nv_tan"},
    {"log", "__nv_log"},
    {"log2", "__nv_log2"},
    {"log10", "__nv_log10"},
    {"log1p", "__nv_log1p"},
    {"exp", "__nv_exp"},
    {"exp2", "__nv_exp2"},
    {"exp10", "__nv_exp10"},
    {"expm1", "__nv_expm1"},
    {"cosh", "__nv_cosh"},
    {"sinh", "__nv_sinh"},
    {"tanh", "__nv_tanh"},
    {"atan2", "__nv_atan2"},
    {"atan", "__nv_atan"},
    {"asin", "__nv_asin"},
    {"acos", "__nv_acos"},
    {"acosh", "__nv_acosh"},
    {"asinh", "__nv_asinh"},
    {"atanh", "__nv_atanh"},
    {"hypot", "__nv_hypot"},
    {"rhypot", "__nv_rhypot"},
    {"norm3d", "__nv_norm3d"},
    {"rnorm3d", "__nv_rnorm3d"},
    {"norm4d", "__nv_norm4d"},
    {"rnorm4d", "__nv_rnorm4d"},
    {"norm", "__nv_norm"},
    {"rnorm", "__nv_rnorm"},
    {"cbrt", "__nv_cbrt"},
    {"rcbrt", "__nv_rcbrt"},
    {"pow", "__nv_pow"},
    {"j0", "__nv_j0"},
    {"j1", "__nv_j1"},
    {"y0", "__nv_y0"},
    {"y1", "__nv_y1"},
    {"yn", "__nv_yn"},
    {"jn", "__nv_jn"},
    {"cyl_bessel_i0", "__nv_cyl_bessel_i0"},
    {"cyl_bessel_i1", "__nv_cyl_bessel_i1"},
    {"erf", "__nv_erf"},
    {"erfinv", "__nv_erfinv"},
    {"erfcinv", "__nv_erfcinv"},
    {"normcdfinv", "__nv_normcdfinv"},
    {"erfc", "__nv_erfc"},
    {"erfcx", "__nv_erfcx"},
    {"normcdf", "__nv_normcdf"},
    {"tgamma", "__nv_tgamma"},
    {"lgamma", "__nv_lgamma"},
    {"ldexp", "__nv_ldexp"},
    {"scalbn", "__nv_scalbn"},
    {"frexp", "__nv_frexp"},
    {"modf", "__nv_modf"},
    {"fmod", "__nv_fmod"},
    {"remainder", "__nv_remainder"},
    {"remquo", "__nv_remquo"},
    {"nextafter", "__nv_nextafter"},
    {"nan", "__nv_nan"},
    {"round", "__nv_round"},
    {"llround", "__nv_llround"},
    {"ilogb", "__nv_ilogb"},
    {"logb", "__nv_logb"},
    {"fma", "__nv_fma"},
    {"dsub_rn", "__nv_dsub_rn"},
    {"dsub_rz", "__nv_dsub_rz"},
    {"dsub_ru", "__nv_dsub_ru"},
    {"dsub_rd", "__nv_dsub_rd"},
    {"fdim", "__nv_fdim"},
};

/**
 * 加入需要链接到libdevice.ptx的函数名。
 */
static void addNeedLinkFunc(MtcsPtxMath *self,char *funcName)
{
   int i;
   for(i=0;i<self->needLinkFuncNameArray->len;i++){
      char *name=n_ptr_array_index(self->needLinkFuncNameArray,i);
      if(strcmp(name,funcName)==0)
         return;
   }
   n_ptr_array_add(self->needLinkFuncNameArray,funcName);
}
/**
 * 返回origName对应的新函数名
 * 例如:源代码调用<math.h>中的expf,在这里返回的名字是: __nv_expf
 */
const char *mtcs_ptx_math_get_replace_funcname(MtcsPtxMath *self,const char *origName)
{
   if(self->funcNameHash==NULL){
      self->funcNameHash = n_hash_table_new_full (n_str_hash, n_str_equal,NULL, NULL);
      int len= ARRAY_SIZE (kMathMap);
      int i;
      for (i=0;i<len;i++){
         n_hash_table_insert( self->funcNameHash,kMathMap[i].mathh,kMathMap[i].libdevice);
      }
   }
   char *newName= n_hash_table_lookup(self->funcNameHash,origName);
   //保存调用过的函数名为动态jit准备。
   if(newName){
      addNeedLinkFunc(self,newName);
   }
   return newName;
}

static void mtcsPtxMathInit(MtcsPtxMath *self)
{
   self->funcNameHash=NULL;
   self->needLinkFuncNameArray=n_ptr_array_new();
}

/**
 * 返回引用的函数名
 */
char *mtcs_ptx_math_get_link_funcname(MtcsPtxMath *self)
{
   NString *n=n_string_new("");
   int i;
   for(i=0;i<self->needLinkFuncNameArray->len;i++){
      char *name=n_ptr_array_index(self->needLinkFuncNameArray,i);
      n_string_append(n,name);
      n_string_append(n,"\n");
   }
   n_debug("mtcsptxmath.c mtcs_ptx_math_get_link_funcname ---%s %d\n",n->str,n->len);
   if(n->len==0){
      n_string_free(n,TRUE);
      return NULL;
   }
   return n_string_free(n,FALSE);
}

static nboolean optimize_EXPF(MtcsPtxMath *self,gcall *stmt)
{
   if(!flag_unsafe_math_optimizations)
      return FALSE;
   if (gimple_call_num_args (stmt) != 1)
     return FALSE;

   tree arg = gimple_call_arg (stmt, 0);
   tree type = TREE_TYPE (arg);
   // 确保是单精度
   if (TYPE_PRECISION (type) != 32)  // 确保是 float
     return FALSE;

   // 检查 exp2f 是否可用
   if (builtin_decl_explicit (BUILT_IN_EXP2F) == NULL_TREE)
     return FALSE;

   gimple_stmt_iterator gsi = gsi_for_stmt (stmt);
   // 构建常量
   REAL_VALUE_TYPE log2e;
   real_from_string (&log2e, "1.44269504088896340736");
   real_convert (&log2e, TYPE_MODE (type), &log2e);
   tree const_val = build_real (type, log2e);

   // 构建乘法
   location_t loc = gimple_location (stmt);
   tree mult = fold_build2_loc (loc, MULT_EXPR, type, arg, const_val);

   // 构建新调用
   tree new_fndecl = builtin_decl_explicit (BUILT_IN_EXP2F);
   gimple *new_stmt = gimple_build_call (new_fndecl, 1, mult);
   gimple_set_location (new_stmt, loc);

   tree lhs = gimple_call_lhs(stmt);
   if(lhs)
      gimple_call_set_lhs(new_stmt, lhs);
   gsi_replace (&gsi, new_stmt, true);
   return TRUE;
}

static nboolean optimize_LOGF(MtcsPtxMath *self,gcall *stmt)
{
   if (!flag_unsafe_math_optimizations)
      return FALSE;

   if (gimple_call_num_args(stmt) != 1)
      return FALSE;

   tree arg = gimple_call_arg(stmt, 0);
   tree type = TREE_TYPE(arg);

   /* 只处理 float */
   if (TYPE_PRECISION(type) != 32)
      return FALSE;

   if (builtin_decl_explicit(BUILT_IN_LOG2F) == NULL_TREE)
      return FALSE;

   gimple_stmt_iterator gsi = gsi_for_stmt(stmt);

   location_t loc = gimple_location(stmt);

   /* log2f(arg) */
   tree log2_decl = builtin_decl_explicit(BUILT_IN_LOG2F);

   tree tmp =  make_temp_ssa_name(type, NULL, "log2f");

   gimple *log2_stmt = gimple_build_call(log2_decl, 1, arg);

   gimple_call_set_lhs(log2_stmt, tmp);
   gimple_set_location(log2_stmt, loc);
   gsi_insert_before(&gsi, log2_stmt, GSI_SAME_STMT);

   /* 常量 ln(2) */
   REAL_VALUE_TYPE ln2;
   real_from_string(&ln2, "0.69314718055994530942");
   real_convert(&ln2, TYPE_MODE(type), &ln2);
   tree const_val = build_real(type, ln2);

   tree mult = fold_build2_loc(loc,MULT_EXPR,type,tmp,const_val);
   tree lhs = gimple_call_lhs(stmt);
   gimple *new_stmt = gimple_build_assign(lhs, mult);
   gimple_set_location(new_stmt, loc);
   gsi_replace(&gsi, new_stmt, true);
   return TRUE;
}

//// .globl   __nv_fast_powf
//.visible .func  (.param .b32 func_retval0) __nv_fast_powf(
//.param .b32 __nv_fast_powf_param_0,
//.param .b32 __nv_fast_powf_param_1
//)
//{
//.reg .f32   %f<6>;
//
//
//ld.param.f32   %f1, [__nv_fast_powf_param_0];
//ld.param.f32   %f2, [__nv_fast_powf_param_1];
//lg2.approx.f32    %f3, %f1;
//mul.f32  %f4, %f3, %f2;
//ex2.approx.f32    %f5, %f4;
//st.param.f32   [func_retval0+0], %f5;
//ret;
//
//}

static nboolean optimize_POWF(MtcsPtxMath *self, gcall *stmt)
{
   // 检查优化条件
   if (!flag_unsafe_math_optimizations)
      return FALSE;

   // 基本检查
   if (!is_gimple_call(stmt))
      return FALSE;

   if (gimple_call_num_args(stmt) != 2)
      return FALSE;

   tree fndecl = gimple_call_fndecl(stmt);
   if (!fndecl || DECL_BUILT_IN_CLASS(fndecl) != BUILT_IN_NORMAL)
      return FALSE;

   // 确保是 powf
   if (DECL_FUNCTION_CODE(fndecl) != BUILT_IN_POWF)
      return FALSE;

   // 获取参数
   tree arg0 = gimple_call_arg(stmt, 0);
   tree arg1 = gimple_call_arg(stmt, 1);
   tree type = TREE_TYPE(arg0);

   // 确保是单精度浮点数
   if (TYPE_PRECISION(type) != 32)
      return FALSE;

   // 检查所需的内置函数
   tree log2f_decl = builtin_decl_explicit(BUILT_IN_LOG2F);
   tree exp2f_decl = builtin_decl_explicit(BUILT_IN_EXP2F);

   if (!log2f_decl || !exp2f_decl) {
      if (dump_file)
         fprintf(dump_file, "需要的内置函数不可用\n");
      return FALSE;
   }
   push_gimplify_context (true);
   // 获取位置和迭代器
   location_t loc = gimple_location(stmt);
   gimple_stmt_iterator gsi = gsi_for_stmt(stmt);
   tree lhs = gimple_call_lhs(stmt);

   // 创建 SSA 变量
   tree ssa_log2 = make_ssa_name(type);
   tree ssa_mul = make_ssa_name(type);
   tree ssa_exp2 = make_ssa_name(type);

   // 构建新的 GIMPLE 序列
   gimple_seq seq = NULL;

   // 1. 计算 log2f(arg0)
   gimple *call_log2 = gimple_build_call(log2f_decl, 1, arg0);
   gimple_set_location(call_log2, loc);
   gimple_call_set_lhs(call_log2, ssa_log2);
   gimple_seq_add_stmt(&seq, call_log2);

   // 2. 计算乘法: arg1 * log2f(arg0)
   gimple *mul = gimple_build_assign(ssa_mul, MULT_EXPR, arg1, ssa_log2);
   gimple_set_location(mul, loc);
   gimple_seq_add_stmt(&seq, mul);

   // 3. 计算 exp2f(mul)
   gimple *call_exp2 = gimple_build_call(exp2f_decl, 1, ssa_mul);
   gimple_set_location(call_exp2, loc);
   gimple_call_set_lhs(call_exp2, ssa_exp2);
   gimple_seq_add_stmt(&seq, call_exp2);

   // 4. 如果原始调用有 LHS，赋值结果
   if (lhs) {
      gimple *assign = gimple_build_assign(lhs, ssa_exp2);
      gimple_set_location(assign, loc);
      gimple_seq_add_stmt(&seq, assign);
   }
   // 替换原语句
   pop_gimplify_context (NULL);
   gsi_replace_with_seq(&gsi, seq, true);
   return TRUE;
}

/**
 * 只有加入 -ffast-math 调用 fminf才能转化为min_expr表达式。
 * 在nvptx平台，不依赖 flag_unsafe_math_optimizations
 * insn-flags.h 定义如下：
 * #define HAVE_sminsf3 1
 */
static nboolean optimize_FMINMAX(MtcsPtxMath *self, gcall *stmt)
{
   // 参数数量检查
   if (gimple_call_num_args(stmt) != 2)
      return FALSE;
   // 获取参数
   tree arg0 = gimple_call_arg(stmt, 0);
   tree arg1 = gimple_call_arg(stmt, 1);
   tree type = TREE_TYPE(arg0);
   // 获取函数声明
   tree fndecl = gimple_call_fndecl(stmt);
   // 检查函数类型
   built_in_function code = DECL_FUNCTION_CODE(fndecl);
   enum tree_code exprCode = ERROR_MARK;
   // 确定是哪个最小值函数
   switch (code) {
      case BUILT_IN_FMINF:  // float
         if (TYPE_PRECISION(TREE_TYPE(arg0)) != 32)
            return FALSE;
         exprCode = MIN_EXPR;
         break;
      case BUILT_IN_FMIN:   // double
         if (TYPE_PRECISION(TREE_TYPE(arg0)) != 64)
            return FALSE;
         exprCode = MIN_EXPR;
         break;
      case BUILT_IN_FMINL:  // long double
         exprCode = MIN_EXPR;
         break;
      case BUILT_IN_FMAXF:  // float
         if (TYPE_PRECISION(TREE_TYPE(arg0)) != 32)
            return FALSE;
         exprCode = MAX_EXPR;
         break;
      case BUILT_IN_FMAX:   // double
         if (TYPE_PRECISION(TREE_TYPE(arg0)) != 64)
            return FALSE;
         exprCode = MAX_EXPR;
         break;
      case BUILT_IN_FMAXL:  // long double
         exprCode =MAX_EXPR;
         break;
      default:
         return FALSE;
   }

   // 只有在快速数学模式下才转换
//   if (!flag_unsafe_math_optimizations) {
//      fprintf(stderr, "跳过 fmin* 优化: 需要 -ffast-math 或 -funsafe-math-optimizations\n");
//      return FALSE;
//   }
//
//   // 检查是否有 NaN 处理需求
//   if (flag_trapping_math) {
//      fprintf(stderr, "跳过 fmin fmax * 优化: 启用了 trapping-math\n");
//      return FALSE;
//   }

   // 确保两个参数类型相同
   if (!types_compatible_p(type, TREE_TYPE(arg1))) {
      fprintf(stderr, "跳过 fmin fmax * 优化: 参数类型不匹配\n");
      return FALSE;
   }

   // 获取位置和迭代器
   location_t loc = gimple_location(stmt);
   gimple_stmt_iterator gsi = gsi_for_stmt(stmt);
   tree lhs = gimple_call_lhs(stmt);

   // 如果原始调用没有 LHS，创建一个临时变量
   tree result_var = lhs;
   if (!lhs) {
      result_var = make_ssa_name(type);
   }

   // 构建 MIN_EXPR 赋值
   gimple *min_assign = gimple_build_assign(result_var, exprCode, arg0, arg1);
   gimple_set_location(min_assign, loc);
   // 替换原语句
   gsi_replace(&gsi, min_assign, true);
   return TRUE;
}

/**
 * 如果数学函数可以被优化返回TRUE
 */
nboolean mtcs_ptx_math_convert_call(MtcsPtxMath *self,gimple *call)
{
   gcall *stmt = as_a <gcall *>(call);
   tree callee = gimple_call_fndecl (stmt);
   if(callee==NULL_TREE)
      return FALSE;
   const char *fnName=IDENTIFIER_POINTER(DECL_NAME(callee));
   location_t loc = gimple_location (call);
   enum built_in_function fcode = DECL_FUNCTION_CODE (callee);
   //fprintf(stderr,"mtcsptxmath.cconvertCall_cb 11 转化调用 fcode:%d BUILT_IN_FMINF:%d fndecl:%p %s\n",fcode,BUILT_IN_FMINF,callee,fnName);
   switch (fcode){
      case BUILT_IN_EXPF:
         return optimize_EXPF(self,call);
      case BUILT_IN_POWF:
         return optimize_POWF(self,call);
      case BUILT_IN_FMINF:
      case BUILT_IN_FMIN:
      case BUILT_IN_FMINL:
      case BUILT_IN_FMAXF:
      case BUILT_IN_FMAX:
      case BUILT_IN_FMAXL:
         return optimize_FMINMAX(self,call);
      case BUILT_IN_LOGF:
         return optimize_LOGF(self,call);
   }
   return FALSE;
}

MtcsPtxMath  *mtcs_ptx_math_new(MtcsMode *mtcsMode)
{
   MtcsPtxMath *self = n_slice_alloc0 (sizeof(MtcsPtxMath));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcsPtxMathInit(self);
   return self;
}
