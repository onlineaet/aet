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
 * base on genopinit.cc
 */
#include "bconfig.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "errors.h"
#include "gensupport.h"
#include "mtcsgen.h"
#include "../../nlib.h"


#define DEF_RTL_EXPR(V, N, X, C) #V,

static const char * const rtx_upname[] = {
#include "rtl.def"
};

#undef DEF_RTL_EXPR

/* Vector in which to collect insns that match.  */
static vec<optab_pattern> patterns;

static void
gen_insn (md_rtx_info *info)
{
  optab_pattern p;
  if (mtcs_gen_find_optab/*!find_optab*/(mtcs_gen_get(),(void*)(&p), XSTR (info->def, 0)))
    patterns.safe_push (p);
}

static int
pattern_cmp (const void *va, const void *vb)
{
  const optab_pattern *a = (const optab_pattern *)va;
  const optab_pattern *b = (const optab_pattern *)vb;
  return a->sort_num - b->sort_num;
}

static int
optab_kind_cmp (const void *va, const void *vb)
{
  const optab_def *a = (const optab_def *)va;
  const optab_def *b = (const optab_def *)vb;
  int diff = a->kind - b->kind;
  if (diff == 0)
    diff = a->op - b->op;
  return diff;
}

static int
optab_rcode_cmp (const void *va, const void *vb)
{
  const optab_def *a = (const optab_def *)va;
  const optab_def *b = (const optab_def *)vb;
  return a->rcode - b->rcode;
}


static FILE *
open_outfile (const char *file_name)
{
  FILE *f = fopen (file_name, "w");
  if (!f)
    fatal ("cannot open file %s: %s", file_name, xstrerror (errno));
  fprintf (f,
	   "/* Generated automatically by the program `mtcsgenopinit'\n"
	   "   from the machine description file `md'.  */\n\n");
  return f;
}

/* Declare the maybe_code_for_* function for ONAME, and provide
   an inline definition of the assserting code_for_* wrapper.  */

static void
handle_overloaded_code_for (FILE *file, overloaded_name *oname,char *platName)
{
  fprintf (file, "\nextern insn_code %s_maybe_code_for_%s (", platName,oname->name);
  for (unsigned int i = 0; i < oname->arg_types.length (); ++i)
    fprintf (file, "%s%s", i == 0 ? "" : ", ", oname->arg_types[i]);
  fprintf (file, ");\n");

  fprintf (file, "inline insn_code\n%s_code_for_%s (", platName,oname->name);
  for (unsigned int i = 0; i < oname->arg_types.length (); ++i)
    fprintf (file, "%s%s arg%d", i == 0 ? "" : ", ", oname->arg_types[i], i);
  fprintf (file, ")\n{\n  insn_code code = %s_maybe_code_for_%s (", platName,oname->name);
  for (unsigned int i = 0; i < oname->arg_types.length (); ++i)
    fprintf (file, "%sarg%d", i == 0 ? "" : ", ", i);
  fprintf (file,
	   ");\n"
	   "  gcc_assert (code != CODE_FOR_nothing);\n"
	   "  return code;\n"
	   "}\n");
}

/* Declare the maybe_gen_* function for ONAME, and provide
   an inline definition of the assserting gen_* wrapper.  */

static void
handle_overloaded_gen (FILE *file, overloaded_name *oname,char *platName)
{
  unsigned HOST_WIDE_INT seen = 0;
  for (overloaded_instance *instance = oname->first_instance->next;
       instance; instance = instance->next)
    {
      pattern_stats stats;
      get_pattern_stats (&stats, XVEC (instance->insn, 1));
      unsigned HOST_WIDE_INT mask
	= HOST_WIDE_INT_1U << stats.num_generator_args;
      if (seen & mask)
	continue;

      seen |= mask;

      fprintf (file, "\nextern rtx %s_maybe_gen_%s (", platName, oname->name);
      for (unsigned int i = 0; i < oname->arg_types.length (); ++i)
	fprintf (file, "%s%s", i == 0 ? "" : ", ", oname->arg_types[i]);
      for (int i = 0; i < stats.num_generator_args; ++i)
	fprintf (file, ", rtx");
      fprintf (file, ");\n");

      fprintf (file, "inline rtx\n%s_gen_%s (", platName,oname->name);
      for (unsigned int i = 0; i < oname->arg_types.length (); ++i)
	fprintf (file, "%s%s arg%d", i == 0 ? "" : ", ",
		 oname->arg_types[i], i);
      for (int i = 0; i < stats.num_generator_args; ++i)
	fprintf (file, ", rtx x%d", i);
      fprintf (file, ")\n{\n  rtx res = %s_maybe_gen_%s (", platName,oname->name);
      for (unsigned int i = 0; i < oname->arg_types.length (); ++i)
	fprintf (file, "%sarg%d", i == 0 ? "" : ", ", i);
      for (int i = 0; i < stats.num_generator_args; ++i)
	fprintf (file, ", x%d", i);
      fprintf (file,
	       ");\n"
	       "  gcc_assert (res);\n"
	       "  return res;\n"
	       "}\n");
    }
}

int
main (int argc, const char **argv)
{
  FILE *h_file, *s_file;
  unsigned int i, j, n, last_kind[5];
  optab_pattern *p;

  progname = "mtcsgenopinit";
  fprintf(stderr,"mtcsgenopinit 00 \n");

  if (NUM_OPTABS > 0xfff || NUM_MACHINE_MODES > 0x3ff)
    fatal ("genopinit range assumptions invalid");

  if (!init_rtx_reader_args_cb (argc, argv, mtcs_gen_handle_arg))
    return (FATAL_EXIT_CODE);
  const char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());
  const char *platNameUpper=mtcs_gen_get_platform_upper_name(mtcs_gen_get());
  char *cFile=mtcs_gen_get_c_file(mtcs_gen_get());
  char *hFile=mtcs_gen_get_h_file(mtcs_gen_get());
  if(!cFile || !hFile || strlen(cFile)==0 || strlen(hFile)==0){
     error("需要指定c文件和h文件。c:%s h:%s",cFile,hFile);
     return FATAL_EXIT_CODE;
  }

  h_file = open_outfile (hFile);
  s_file = open_outfile (cFile);

  /* Read the machine description.  */
  md_rtx_info info;
  while (read_md_rtx (&info))
    switch (GET_CODE (info.def))
      {
      case DEFINE_INSN:
      case DEFINE_EXPAND:
	gen_insn (&info);
	break;

      default:
	break;
      }

  /* Sort the collected patterns.  */
  patterns.qsort (pattern_cmp);
  /* Now that we've handled the "extra" patterns, eliminate them from
     the optabs array.  That way they don't get in the way below.  */
  n = num_optabs;
  for (i = 0; i < n; )
    if (optabs[i].base == NULL)
      optabs[i] = optabs[--n];
    else
      ++i;

  /* Sort the (real) optabs.  Better than forcing the optabs.def file to
     remain sorted by kind.  We also scrogged any real ordering with the
     purging of the X patterns above.  */
  qsort (optabs, n, sizeof (optab_def), optab_kind_cmp);

  fprintf (h_file, "#ifndef %s_INSN_OPINIT_H\n",platName);
  fprintf (h_file, "#define %s_INSN_OPINIT_H 1\n",platName);
  //所有平台都一样 来自"optabs.def" 所以不需要，引用主机即可
  /* Emit the optab enumeration for the header file.  */
  fprintf(h_file,"/*所有平台都一样,来自 optabs.def ，引用主机即可。\n");//注释 第1部分
  fprintf (h_file, "enum optab_tag {\n");
  for (i = j = 0; i < n; ++i)
    {
      optabs[i].op = i;
      fprintf (h_file, "  %s,\n", optabs[i].name);
      if (optabs[i].kind != j)
	last_kind[j++] = i - 1;
    }
  fprintf (h_file, "  FIRST_CONV_OPTAB = %s,\n", optabs[last_kind[0]+1].name);
  fprintf (h_file, "  LAST_CONVLIB_OPTAB = %s,\n", optabs[last_kind[1]].name);
  fprintf (h_file, "  LAST_CONV_OPTAB = %s,\n", optabs[last_kind[2]].name);
  fprintf (h_file, "  FIRST_NORM_OPTAB = %s,\n", optabs[last_kind[2]+1].name);
  fprintf (h_file, "  LAST_NORMLIB_OPTAB = %s,\n", optabs[last_kind[3]].name);
  fprintf (h_file, "  LAST_NORM_OPTAB = %s\n", optabs[i-1].name);
  fprintf (h_file, "};\n\n");
  fprintf(h_file,"*/\n");//注释掉
  fprintf(stderr,"mtcsgenopinit---11- n:%d num_optabs:%d patterns:%d\n",n,num_optabs,patterns.length ());

  fprintf (h_file, "#define %s_NUM_OPTABS          %u\n", platNameUpper,n);
  fprintf (h_file, "#define %s_NUM_CONVLIB_OPTABS  %u\n",
        platNameUpper,last_kind[1] - last_kind[0]);
  fprintf (h_file, "#define %s_NUM_NORMLIB_OPTABS  %u\n",
        platNameUpper,last_kind[3] - last_kind[2]);
  fprintf (h_file, "#define %s_NUM_OPTAB_PATTERNS  %u\n",
        platNameUpper,(unsigned) patterns.length ());

  fprintf(h_file,"/*与主机相同\n");//注释掉 第3部份
  fprintf (h_file,
	   "typedef enum optab_tag optab;\n"
	   "typedef enum optab_tag convert_optab;\n"
	   "typedef enum optab_tag direct_optab;\n"
	   "\n"
       "optab_libcall_d convert_optab_libcall_d 只有optabs-libfuncs.cc引用，在mtcs optabs-libfuncs被mtcslibfuncs代替,\n"
       "所以，可以在mtcslibfuncs 定义这两个结构体。\n"
	   "struct optab_libcall_d\n"
	   "{\n"
	   "  char libcall_suffix;\n"
	   "  const char *libcall_basename;\n"
	   "  void (*libcall_gen) (optab, const char *name,\n"
	   "		       char suffix, machine_mode);\n"
	   "};\n"
	   "\n"
	   "struct convert_optab_libcall_d\n"
	   "{\n"
	   "  const char *libcall_basename;\n"
	   "  void (*libcall_gen) (convert_optab, const char *name,\n"
	   "		       machine_mode, machine_mode);\n"
	   "};\n"
	   "\n"
      "*/\n"
	   "/* Given an enum insn_code, access the function to construct\n"
	   "   the body of that kind of insn.  */\n"
       "/* 被 MTCS_GEN_FCN 替换\n"
	   "#define GEN_FCN(CODE) (insn_data[CODE].genfun)\n"
       "*/\n"
	   "\n"
	   "#ifdef NUM_RTX_CODE\n"
	   "/* Contains the optab used for each rtx code, and vice-versa.  */\n"
       "/* code_to_optab_  optab_to_code_与主机相同\n"
	   "extern const optab code_to_optab_[NUM_RTX_CODE];\n"
	   "extern const enum rtx_code optab_to_code_[NUM_OPTABS];\n"
	   "\n"
	   "static inline optab\n"
	   "code_to_optab (enum rtx_code code)\n"
	   "{\n"
	   "  return code_to_optab_[code];\n"
	   "}\n"
	   "\n"
	   "static inline enum rtx_code\n"
	   "optab_to_code (optab op)\n"
	   "{\n"
	   "  return optab_to_code_[op];\n"
	   "}\n */\n");

  for (overloaded_name *oname = rtx_reader_ptr->get_overloads ();
       oname; oname = oname->next)
    {
      handle_overloaded_code_for (h_file, oname,platName);
      handle_overloaded_gen (h_file, oname,platName);
    }

  fprintf (h_file,
	   "#endif\n"
	   "\n"
        "/* 被 mtcslibfuncs.h 中的 convlib_def normlib_def 替换\n"
	   "extern const struct convert_optab_libcall_d convlib_def[NUM_CONVLIB_OPTABS];\n"
	   "extern const struct optab_libcall_d normlib_def[NUM_NORMLIB_OPTABS];\n"
       "*/\n"
	   "\n"
	   "/* Returns the active icode for the given (encoded) optab.  */\n"
        "/* raw_optab_handler 和 swap_optab_enable 在 mtcsopinit.h 重新声明 mtcsptxopinit.c重新定义\n"
	   "extern enum insn_code raw_optab_handler (unsigned);\n"
	   "extern bool swap_optab_enable (optab, machine_mode, bool);\n"
        "*/\n"
	   "\n"
	   "/* Target-dependent globals.  */\n"
      "/*由于 NUM_OPTAB_PATTERNS NUM_MACHINE_MODES 总是大于设备的，所以重用主机的 struct target_optabs */\n"
	   "// struct target_optabs {\n"
	   "  /* Patterns that are used by optabs that are enabled for this target.  */\n"
	   "//  bool pat_enable[NUM_OPTAB_PATTERNS];\n"
	   "\n"
	   "  /* Index VOIDmode caches if the target supports vec_gather_load for any\n"
	   "     vector mode.  Every other index X caches specifically for mode X.\n"
	   "     1 means yes, -1 means no.  */\n"
	   "//  signed char supports_vec_gather_load[NUM_MACHINE_MODES];\n"
	   "//  signed char supports_vec_scatter_store[NUM_MACHINE_MODES];\n"
	   "// };\n"
      "/* init_all_optabs 改为 xxx_init_all_optabs 被 mtcsopinit.h 中的 void (*init_all_optabs)调用\n"
       "* partial_vectors_supported_p 改为 xxx_partial_vectors_supported_p 被 mtcsopinit.h 中的 bool (*partial_vectors_supported_p)调用 \n"
       "* 增加 xxx_lookup_optab  xxx_get_insn_code"
       "*/\n"
	   "extern void %s_init_all_optabs (struct target_optabs *);\n"
	   "extern bool %s_partial_vectors_supported_p (void);\n"
      "extern int  %s_lookup_optab (unsigned scode);\n"
      "extern enum insn_code  %s_get_insn_code (int index);\n"
	   "\n"
       "/* 不需要， mtcsopinit.h中有替换的\n"
	   "extern struct target_optabs default_target_optabs;\n"
	   "extern struct target_optabs *this_fn_optabs;\n"
	   "#if SWITCHABLE_TARGET\n"
	   "extern struct target_optabs *this_target_optabs;\n"
	   "#else\n"
	   "#define this_target_optabs (&default_target_optabs)\n"
	   "#endif\n"
      "*/\n",platName,platName,platName,platName);

   fprintf(h_file,"extern void *  %s_get_optab_libcall();\n",platName);
   fprintf(h_file,"extern void *  %s_get_convert_optab_libcall();\n",platName);



  fprintf (s_file,
	   "#define IN_TARGET_CODE 1\n"
	   "#include \"config.h\"\n"
	   "#include \"system.h\"\n"
	   "#include \"coretypes.h\"\n"
	   "#include \"backend.h\"\n"
	   "#include \"predict.h\"\n"
	   "#include \"tree.h\"\n"
	   "#include \"rtl.h\"\n"
	   "#include \"alias.h\"\n"
	   "#include \"varasm.h\"\n"
	   "#include \"stor-layout.h\"\n"
	   "#include \"calls.h\"\n"
	   "#include \"memmodel.h\"\n"
	   "#include \"tm_p.h\"\n"
	   "#include \"flags.h\"\n"
	   "#include \"insn-config.h\"\n"
	   "#include \"expmed.h\"\n"
	   "#include \"dojump.h\"\n"
	   "#include \"explow.h\"\n"
	   "#include \"emit-rtl.h\"\n"
	   "#include \"stmt.h\"\n"
	   "#include \"expr.h\"\n"
      "#include \"optabs.h\"\n"
	   "#include \"%s-insn-codes.h\"\n"
      "#include \"%s-insn-modes.h\"\n"
      "#include \"%s-insn-flags.h\"\n"
      "#include \"%s-insn-opinit.h\"\n"
      "#include \"../../mtcslibfuncs.h\"\n",platName,platName,platName,platName);

    if(mtcs_gen_is_ptx(mtcs_gen_get())){
       fprintf (s_file,"#include \"../ptx-common.h\"\n");
    }

  fprintf(s_file,
	   "\nstruct optab_pat {\n"
	   "  unsigned scode;\n"
	   "  enum insn_code icode;\n"
	   "};\n\n");

  fprintf (s_file,
	   "static const struct optab_pat pats[%s_NUM_OPTAB_PATTERNS] = {\n",platNameUpper);
  for (i = 0; patterns.iterate (i, &p); ++i)
    fprintf (s_file, "  { %#08x, (enum insn_code)%s_CODE_FOR_%s },\n", p->sort_num,platNameUpper, p->name);
  fprintf (s_file, "};\n\n");

  /* Some targets like riscv have a large number of patterns.  In order to
     prevent pathological situations in dataflow analysis split the init
     function into separate ones that initialize 1000 patterns each.  */

  const int patterns_per_function = 1000;

  if (patterns.length () > patterns_per_function)
    {
      unsigned num_init_functions
	= patterns.length () / patterns_per_function + 1;
      for (i = 0; i < num_init_functions; i++)
	{
	  fprintf (s_file, "static void\ninit_optabs_%02d "
		   "(struct target_optabs *optabs)\n{\n", i);
	  fprintf (s_file, "  bool *ena = optabs->pat_enable;\n");
	  unsigned start = i * patterns_per_function;
	  unsigned end = MIN (patterns.length (),
			      (i + 1) * patterns_per_function);
	  for (j = start; j < end; ++j)
	    fprintf (s_file, "  ena[%u] = %s_HAVE_%s;\n", j, platNameUpper,patterns[j].name);
	  fprintf (s_file, "}\n\n");
	}

      fprintf (s_file, "void\n%s_init_all_optabs "
	       "(struct target_optabs *optabs)\n{\n",platName);
      for (i = 0; i < num_init_functions; ++i)
	fprintf (s_file, "  init_optabs_%02d (optabs);\n", i);
      fprintf (s_file, "}\n\n");
    }
  else
    {
      fprintf (s_file, "void\n%s_init_all_optabs "
	       "(struct target_optabs *optabs)\n{\n",platName);
      fprintf (s_file, "  bool *ena = optabs->pat_enable;\n");
      for (i = 0; patterns.iterate (i, &p); ++i)
	fprintf (s_file, "  ena[%u] = %s_HAVE_%s;\n", i, platNameUpper,p->name);
      fprintf (s_file, "}\n\n");
    }

  fprintf (s_file,
	   "/* Returns TRUE if the target supports any of the partial vector\n"
	   "   optabs: while_ult_optab, len_load_optab, len_store_optab,\n"
	   "   mask_len_load_optab or mask_len_store_optab,\n"
	   "   for any mode.  */\n"
	   "bool\n%s_partial_vectors_supported_p (void)\n{\n",platName);
  bool any_match = false;
  fprintf (s_file, "\treturn");
  bool first = true;
  for (i = 0; patterns.iterate (i, &p); ++i)
    {
#define CMP_NAME(N) !strncmp (p->name, (N), strlen ((N)))
      if (CMP_NAME("while_ult") || CMP_NAME ("len_load")
	  || CMP_NAME ("len_store")|| CMP_NAME ("mask_len_load")
	  || CMP_NAME ("mask_len_store"))
	{
	  if (first)
	    fprintf (s_file, " %s_HAVE_%s", platNameUpper,p->name);
	  else
	    fprintf (s_file, " || %s_HAVE_%s", platNameUpper,p->name);
	  first = false;
	  any_match = true;
	}
    }
  if (!any_match)
    fprintf (s_file, " false");
  fprintf (s_file, ";\n}\n");

  fprintf(stderr,"进入这里 mtcsgenopinit------\n");
  /* Perform a binary search on a pre-encoded optab+mode*2.  */
  /* ??? Perhaps even better to generate a minimal perfect hash.
     Using gperf directly is awkward since it's so geared to working
     with strings.  Plus we have no visibility into the ordering of
     the hash entries, which complicates the pat_enable array.  */
  fprintf(s_file,"/*-- lookup_handler 被 xxx_lookup_optab 替换，MtcsxxxOpinit调用--*/\n");

  fprintf (s_file,
	   "int\n"
	   "%s_lookup_optab (unsigned scode)\n"
	   "{\n"
	   "  int l = 0, h = %s_NUM_OPTAB_PATTERNS/*!ARRAY_SIZE (pats)*/, m;\n"
	   "  while (h > l)\n"
	   "    {\n"
	   "      m = (h + l) / 2;\n"
	   "      if (scode == pats[m].scode)\n"
	   "        return m;\n"
	   "      else if (scode < pats[m].scode)\n"
	   "        h = m;\n"
	   "      else\n"
	   "        l = m + 1;\n"
	   "    }\n"
	   "  return -1;\n"
	   "}\n\n",platName,platNameUpper);

  fprintf (s_file,
       "enum insn_code \n"
       "%s_get_insn_code (int index)\n"
       "{\n"
       "  return pats[index].icode;\n"
       "}\n\n",platName);
  fprintf(stderr,"进入这里 mtcsgenopinit------11\n");

  fprintf(s_file,"/* raw_optab_handler swap_optab_enable 被mtcsopinit实现\n");
  fprintf (s_file,
	   "enum insn_code\n"
	   "raw_optab_handler (unsigned scode)\n"
	   "{\n"
	   "  int i = lookup_handler (scode);\n"
	   "  return (i >= 0 && this_fn_optabs->pat_enable[i]\n"
	   "          ? pats[i].icode : CODE_FOR_nothing);\n"
	   "}\n\n");

  fprintf (s_file,
	   "bool\n"
	   "swap_optab_enable (optab op, machine_mode m, bool set)\n"
	   "{\n"
	   "  unsigned scode = (op << 20) | m;\n"
	   "  int i = lookup_handler (scode);\n"
	   "  if (i >= 0)\n"
	   "    {\n"
	   "      bool ret = this_fn_optabs->pat_enable[i];\n"
	   "      this_fn_optabs->pat_enable[i] = set;\n"
	   "      return ret;\n"
	   "    }\n"
	   "  else\n"
	   "    {\n"
	   "      gcc_assert (!set);\n"
	   "      return false;\n"
	   "    }\n"
	   "}\n\n");
  fprintf(s_file,"*/\n");

  /* C++ (even G++) does not support (non-trivial) designated initializers.
     To work around that, generate these arrays programatically rather than
     by our traditional multiple inclusion of def files.  */

  fprintf (s_file,
	   "static const struct mtcs_convert_optab_libcall_d "
	   "%s_convlib_def[%s_NUM_CONVLIB_OPTABS] = {\n",platName,platNameUpper);
  fprintf(stderr,"进入这里 mtcsgenopinit------22\n");

  for (i = last_kind[0] + 1; i <= last_kind[1]; ++i){
     if(strcmp(optabs[i].libcall,"NULL")==0)
        fprintf (s_file, "  { %s, %s },\n", optabs[i].base, optabs[i].libcall);
     else
       fprintf (s_file, "  { %s, mtcs_libfuncs_%s },\n", optabs[i].base, optabs[i].libcall);
  }
  fprintf (s_file, "};\n\n");

  fprintf (s_file,
	   "const struct mtcs_optab_libcall_d "
	   "%s_normlib_def[%s_NUM_NORMLIB_OPTABS] = {\n",platName,platNameUpper);
  for (i = last_kind[2] + 1; i <= last_kind[3]; ++i){
     if(strcmp(optabs[i].libcall,"NULL")==0)
        fprintf (s_file, "  { %s, %s, %s },\n",
             optabs[i].suffix, optabs[i].base, optabs[i].libcall);
     else
       fprintf (s_file, "  { %s, %s, mtcs_libfuncs_%s },\n",
	     optabs[i].suffix, optabs[i].base, optabs[i].libcall);
  }
  fprintf (s_file, "};\n\n");

  fprintf (s_file, "/* optab_to_code_ code_to_optab_与主机相同，注释掉\n");

  fprintf (s_file, "enum rtx_code const optab_to_code_[NUM_OPTABS] = {\n");
  for (i = 0; i < n; ++i)
    fprintf (s_file, "  %s,\n", rtx_upname[optabs[i].fcode]);
  fprintf (s_file, "};\n\n");
  fprintf(stderr,"进入这里 mtcsgenopinit------33\n");

  qsort (optabs, n, sizeof (optab_def), optab_rcode_cmp);

  fprintf (s_file, "const optab code_to_optab_[NUM_RTX_CODE] = {\n");
  for (j = 0; optabs[j].rcode == UNKNOWN; ++j)
    continue;
  for (i = 0; i < NON_GENERATOR_NUM_RTX_CODE; ++i)
    {
      if (j < n && optabs[j].rcode == i)
	fprintf (s_file, "  %s,\n", optabs[j++].name);
      else
	fprintf (s_file, "  unknown_optab,\n");
    }
  fprintf (s_file, "};\n\n");
  fprintf (s_file, "*/\n\n");

  fprintf(s_file,"void *  %s_get_convert_optab_libcall()\n{\n   return (void*)%s_convlib_def;\n}\n",platName,platName);
  fprintf(s_file,"void *  %s_get_optab_libcall()\n{\n   return (void*)%s_normlib_def;\n}\n",platName,platName);

  fprintf(stderr,"进入这里 mtcsgenopinit------44\n");

  fprintf (h_file, "#endif\n");
  return (fclose (h_file) == 0 && fclose (s_file) == 0
	  ? SUCCESS_EXIT_CODE : FATAL_EXIT_CODE);
}

