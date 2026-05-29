/* Generate insn-target-def.h, an automatically-generated part of targetm.
   Copyright (C) 1987-2025 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING3.  If not see
<http://www.gnu.org/licenses/>.  */

#include "bconfig.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "errors.h"
#include "read-md.h"
#include "gensupport.h"
#include "hash-table.h"
#include "mtcsgen.h"
#include "../../nlib.h"
/* This class hashes define_insns and define_expands by name.  */
struct insn_hasher : nofree_ptr_hash <rtx_def>
{
   typedef rtx value_type;
   typedef const char *compare_type;

   static inline hashval_t hash (rtx);
   static inline bool equal (rtx, const char *);
};

hashval_t insn_hasher::hash (rtx x)
{
   return htab_hash_string (XSTR (x, 0));
}

bool insn_hasher::equal (rtx x, const char *y)
{
   return strcmp (XSTR (x, 0), y) == 0;
}

/* All define_insns and define_expands, hashed by name.  */
static hash_table <insn_hasher> *insns;

/* Records the prototype suffix X for each invalid_X stub that has been
   generated.  */
static hash_table <nofree_string_hash> *stubs;

/* Records which C conditions have been wrapped in functions, as a mapping
   from the C condition to the function name.  */
static hash_map <nofree_string_hash, const char *> *have_funcs;

/* Return true if the part of the prototype at P is for an argument
   name.  If so, point *END_OUT to the first character after the name.
   If OPNO_OUT is nonnull, set *OPNO_OUT to the number of the associated
   operand.  If REQUIRED_OUT is nonnull, set *REQUIRED_OUT to whether the
   .md pattern is required to match the operand.  */

static bool parse_argument (const char *p, const char **end_out,
		unsigned int *opno_out = 0,bool *required_out = 0)
{
   while (ISSPACE (*p))
      p++;
   if (p[0] == 'x' && ISDIGIT (p[1])){
      p += 1;
      if (required_out)
         *required_out = true;
   }else if (p[0] == 'o' && p[1] == 'p' && p[2] == 't' && ISDIGIT (p[3])){
      p += 3;
      if (required_out)
         *required_out = false;
   }else
      return false;

   char *endptr;
   unsigned int opno = strtol (p, &endptr, 10);
   if (opno_out)
      *opno_out = opno;
   *end_out = endptr;
   return true;
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

/* Output hook definitions for pattern NAME, which has target-insns.def
   prototype PROTOTYPE.  */
static void def_target_insn (NString *defineCodes, NString *declarationCodes,char *name, const char *prototype)
{
   char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());
   char *platUpperName=mtcs_gen_get_platform_upper_name(mtcs_gen_get());
   /* Get an upper-case form of NAME.  */
   unsigned int i;
   char *upper_name = XALLOCAVEC (char, strlen (name) + 1);
   for (i = 0; name[i]; ++i)
      upper_name[i] = TOUPPER (name[i]);
   upper_name[i] = 0;

   /* Check that the prototype is valid and concatenate the types
   together to get a suffix.  */
   char *suffix = XALLOCAVEC (char, strlen (prototype) + 1);
   i = 0;
   unsigned int opno = 0;
   unsigned int required_ops = 0;
   unsigned int this_opno;
   bool required_p;
   for (const char *p = prototype; *p; ++p)
      if (parse_argument (p, &p, &this_opno, &required_p)){
         if (this_opno != opno || (*p != ',' && *p != ')')){
            error ("invalid prototype for '%s'", name);
            exit (FATAL_EXIT_CODE);
         }
         if (required_p && required_ops < opno){
            error ("prototype for '%s' has required operands after optional operands", name);
            exit (FATAL_EXIT_CODE);
         }
         opno += 1;
         if (required_p)
            required_ops = opno;
         /* Skip over ')'s.  */
         if (*p == ',')
            suffix[i++] = '_';
      }else if (*p == ')' || *p == ','){
         /* We found the end of a parameter without finding a
         parameter name.  */
         if (strcmp (prototype, "(void)") != 0){
            error ("argument %d of '%s' did not have the expected name", opno, name);
            exit (FATAL_EXIT_CODE);
         }
      }else if (*p != '(' && !ISSPACE (*p))
         suffix[i++] = *p;

   suffix[i] = 0;
   //fprintf(stderr,"name is :%s suffix:%s\n",upper_name,suffix);

   /* See whether we have an implementation of this pattern.  */
   hashval_t hash = htab_hash_string (name);
   int truth = 0;
   const char *have_name = name;
   if (rtx insn = insns->find_with_hash (name, hash)){
      pattern_stats stats;
      get_pattern_stats (&stats, XVEC (insn, 1));
      unsigned int actual_ops = stats.num_generator_args;
      if (opno == required_ops && opno != actual_ops)
         error_at (get_file_location (insn),"'%s' must have %d operands (excluding match_dups)",name, required_ops);
      else if (actual_ops < required_ops)
         error_at (get_file_location (insn), "'%s' must have at least %d operands (excluding match_dups)",name, required_ops);
      else if (actual_ops > opno)
         error_at (get_file_location (insn), "'%s' must have no more than %d operands (excluding match_dups)", name, opno);

      const char *test = XSTR (insn, 2);
      truth = maybe_eval_c_test (test);
      gcc_assert (truth != 0);
      if (truth < 0){
         /* Try to reuse an existing function that performs the same test.  */
         bool existed;
         const char *&entry = have_funcs->get_or_insert (test, &existed);
         if (!existed){
            entry = name;
            n_string_append_printf(defineCodes,"\nstatic bool have_%s_cb (void)\n",name);
            n_string_append(defineCodes,"{\n");
            n_string_append(defineCodes,"\treturn ");
            print_c_condition(defineCodes, test);
            n_string_append(defineCodes,";\n");
            n_string_append(defineCodes,"}\n");
         }
         have_name = entry;
      }
      /*生成函数定义
      static rtx_insn * gen_allocate_stack_cb (TargetRtx *self,rtx x0, rtx x1)
      {
        return insnify (gen_allocate_stack (x0, x1));
      }
      */
      n_string_append_printf(defineCodes,"\nstatic rtx_insn * gen_%s_cb ",name);
      /* Print the prototype with the argument names after ACTUAL_OPS
      removed.  */
      const char *p = prototype, *end;
      int firstOpenpParen=0;
      while (*p){
         if (parse_argument (p, &end, &this_opno) && this_opno >= actual_ops)
            p = end;
         else{
            char tmp=*p;
            n_string_append_c(defineCodes,*p++);
            if(tmp=='(' && firstOpenpParen==0){
               firstOpenpParen++;
               if(strcmp (prototype, "(void)")==0){
                  n_string_append(defineCodes,"TargetRtx *targetRtx)");
                  break;
               }else{
                  n_string_append(defineCodes,"TargetRtx *targetRtx,");
               }
            }
         }
      }

      n_string_append(defineCodes,"\n{\n");
      //如果 truth<0 检查是否 targetm.havexxx返回真。
      if (truth < 0){
         n_string_append_printf(defineCodes,"\tgcc_checking_assert (targetRtx->have_%s (targetRtx));\n", name);
      }
      n_string_append_printf(defineCodes,"\treturn insnify (targetRtx,%s_gen_%s (", platName,name);

      for (i = 0; i < actual_ops; ++i){
         n_string_append_printf(defineCodes,"%s%s%d", i == 0 ? "" : ", ", i < required_ops ? "x" : "opt", i);
      }
      n_string_append(defineCodes,"));\n");
      n_string_append(defineCodes,"}\n");
   }else{
      const char **slot = stubs->find_slot (suffix, INSERT);
      if (!*slot){
         int firstOpenpParen=0;
         *slot = xstrdup (suffix);
         n_string_append_printf(defineCodes,"\nstatic rtx_insn * invalid_%s ",suffix);
         /* Print the prototype with the argument names removed.  */
         const char *p = prototype;
         while (*p){
            if (!parse_argument (p, &p)){
               char tmp=*p;
               n_string_append_c(defineCodes,*p++);
               if(tmp=='(' && firstOpenpParen==0){
                  firstOpenpParen++;
                  if(strcmp (prototype, "(void)")==0){
                     n_string_append(defineCodes,"TargetRtx *targetRtx)");
                     break;
                  }else{
                     n_string_append(defineCodes,"TargetRtx *targetRtx,");
                  }
               }
            }
         }
         n_string_append(defineCodes,"\n{\n");
         n_string_append(defineCodes,"\tgcc_unreachable ();\n");
         n_string_append(defineCodes,"}\n");
      }
   }

   if(truth==0)
      n_string_append_printf(declarationCodes,"\ttargetRtx->have_%s = bool_void_false;\n",have_name);
   else if (truth == 1)
      n_string_append_printf(declarationCodes,"\ttargetRtx->have_%s = bool_void_true;\n",have_name);
   else
      n_string_append_printf(declarationCodes,"\ttargetRtx->have_%s =have_%s_cb;\n",name,have_name);

   if(truth==0)
      n_string_append_printf(declarationCodes,"\ttargetRtx->gen_%s = invalid_%s;\n",name,suffix);
   else
      n_string_append_printf(declarationCodes,"\ttargetRtx->gen_%s = gen_%s_cb;\n",name,name);

   if (truth == 0)
      n_string_append_printf(declarationCodes,"\ttargetRtx->code_for_%s = CODE_FOR_nothing;\n",name);
   else
      n_string_append_printf(declarationCodes,"\ttargetRtx->code_for_%s =(enum insn_code) %s_CODE_FOR_%s;\n",name,platUpperName,name);

   n_string_append_printf(declarationCodes,"\n");
}

//生成 targetrtx.h targetrtx.c的代码
static void createGenFuncCFileHFile(NString *hCodes,NString *cCodes,char *name, const char *prototype)
{
   char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());
   char *platUpperName=mtcs_gen_get_platform_upper_name(mtcs_gen_get());
   /* Get an upper-case form of NAME.  */
   unsigned int i;
   char *upper_name = XALLOCAVEC (char, strlen (name) + 1);
   for (i = 0; name[i]; ++i)
      upper_name[i] = TOUPPER (name[i]);
   upper_name[i] = 0;

   /* Check that the prototype is valid and concatenate the types
   together to get a suffix.  */
   char *suffix = XALLOCAVEC (char, strlen (prototype) + 1);
   i = 0;
   unsigned int opno = 0;
   unsigned int required_ops = 0;
   unsigned int this_opno;
   bool required_p;
   for (const char *p = prototype; *p; ++p)
      if (parse_argument (p, &p, &this_opno, &required_p)){
         if (this_opno != opno || (*p != ',' && *p != ')')){
            error ("invalid prototype for '%s'", name);
            exit (FATAL_EXIT_CODE);
         }
         if (required_p && required_ops < opno){
            error ("prototype for '%s' has required operands after optional operands", name);
            exit (FATAL_EXIT_CODE);
         }
         opno += 1;
         if (required_p)
            required_ops = opno;
         /* Skip over ')'s.  */
         if (*p == ',')
            suffix[i++] = '_';
      }else if (*p == ')' || *p == ','){
         /* We found the end of a parameter without finding a
         parameter name.  */
         if (strcmp (prototype, "(void)") != 0){
            error ("argument %d of '%s' did not have the expected name", opno, name);
            exit (FATAL_EXIT_CODE);
         }
      }else if (*p != '(' && !ISSPACE (*p))
         suffix[i++] = *p;

   suffix[i] = 0;

   //生成函数声明和定义
   //生成targetrtx.h或target.c中的源代码
   n_string_append_printf(cCodes,"rtx_insn * target_rtx_gen_%s ",name);
   n_string_append_printf(hCodes,"rtx_insn * target_rtx_gen_%s ",name);

   //实际参数名 像这样 "x0,opt1,opt2,opt3"
   NString *actual=n_string_new("");
   //无参数
   if(strcmp (prototype, "(void)")==0){
      n_string_append(cCodes,"(TargetRtx *self)");
      n_string_append(hCodes,"(TargetRtx *self)");
   }else{
      //有参数 像 (rtx x0, rtx opt1, rtx opt2, rtx opt3）
      NString *r=n_string_new(prototype);
      int s1=n_string_indexof(r,"(");
      int s2=n_string_indexof(r,")");
      //  fprintf(stderr,"---------yyy-------------%s %d %d\n",r->str,s1,s2);

      //r1=rtx x0, rtx opt1, rtx opt2, rtx opt3
      NString *r1=n_string_substring_from(r,s1+1,s2);
      char **params=n_strsplit(r1->str,",",-1);
      int len=n_strv_length(params);
      n_string_append_printf(cCodes,"(TargetRtx *self,%s)",r1->str);
      n_string_append_printf(hCodes,"(TargetRtx *self,%s)",r1->str);
      int i;
      for(i=0;i<len;i++){
         char *item = params[i];
         if(item[0] == ' ')
            item=item+1;
         item=strstr(item," ");
         //fprintf(stderr,"---------zzz-------------%s %d i:%d params[i]:%s item:%s\n",r1->str,len,i,params[i],item);
         if(i==len-1){
            n_string_append_printf(actual,"%s",item);
         }else{
            n_string_append_printf(actual,"%s, ",item);
         }
      }
      n_string_free(r,TRUE);
      n_string_free(r1,TRUE);
      n_strfreev(params);
   }

   n_string_append(cCodes,"\n{\n");
   n_string_append(hCodes,";\n");
   //如果 truth<0 检查是否 targetm.havexxx返回真。
   if(actual->len>0)
      n_string_append_printf(cCodes,"\treturn self->gen_%s(self,%s",name,actual->str);
   else
      n_string_append_printf(cCodes,"\treturn self->gen_%s(self",name);
   n_string_append(cCodes,");\n");
   n_string_append(cCodes,"}\n\n");
   n_string_free(actual,TRUE);
}

//生成 targetrtx.h targetrtx.c的代码
static void createHaveFuncCFileHFile(NString *hCodes,NString *cCodes,char *name, const char *prototype)
{
   char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());
   char *platUpperName=mtcs_gen_get_platform_upper_name(mtcs_gen_get());
   /* Get an upper-case form of NAME.  */
   unsigned int i;
   char *upper_name = XALLOCAVEC (char, strlen (name) + 1);
   for (i = 0; name[i]; ++i)
      upper_name[i] = TOUPPER (name[i]);
   upper_name[i] = 0;

   /* Check that the prototype is valid and concatenate the types
   together to get a suffix.  */
   char *suffix = XALLOCAVEC (char, strlen (prototype) + 1);
   i = 0;
   unsigned int opno = 0;
   unsigned int required_ops = 0;
   unsigned int this_opno;
   bool required_p;
   for (const char *p = prototype; *p; ++p)
      if (parse_argument (p, &p, &this_opno, &required_p)){
         if (this_opno != opno || (*p != ',' && *p != ')')){
            error ("invalid prototype for '%s'", name);
            exit (FATAL_EXIT_CODE);
         }
         if (required_p && required_ops < opno){
            error ("prototype for '%s' has required operands after optional operands", name);
            exit (FATAL_EXIT_CODE);
         }
         opno += 1;
         if (required_p)
            required_ops = opno;
         /* Skip over ')'s.  */
         if (*p == ',')
            suffix[i++] = '_';
      }else if (*p == ')' || *p == ','){
         /* We found the end of a parameter without finding a
         parameter name.  */
         if (strcmp (prototype, "(void)") != 0){
            error ("argument %d of '%s' did not have the expected name", opno, name);
            exit (FATAL_EXIT_CODE);
         }
      }else if (*p != '(' && !ISSPACE (*p))
         suffix[i++] = *p;

   suffix[i] = 0;

   //生成函数声明和定义
   //生成targetrtx.h或target.c中的源代码
   n_string_append_printf(cCodes,"bool target_rtx_have_%s (TargetRtx *self) ",name);
   n_string_append_printf(hCodes,"bool target_rtx_have_%s (TargetRtx *self);\n",name);
   n_string_append(cCodes,"\n{\n");
   n_string_append_printf(cCodes,"\treturn self->have_%s(self);\n",name);
   n_string_append(cCodes,"}\n\n");
}


/* Record the DEFINE_INSN or DEFINE_EXPAND described by INFO.  */
static void add_insn (md_rtx_info *info)
{
   rtx def = info->def;
   const char *name = XSTR (def, 0);
   if (name[0] == 0 || name[0] == '*')
      return;

   hashval_t hash = htab_hash_string (name);
   rtx *slot = insns->find_slot_with_hash (name, hash, INSERT);
   if (*slot)
      error_at (info->loc, "duplicate definition of '%s'", name);
   else
      *slot = def;
}

int main (int argc, const char **argv)
{
   progname = "gentarget-def";

   if (!init_rtx_reader_args_cb(argc, argv,mtcs_gen_handle_arg))
      return (FATAL_EXIT_CODE);
   char *platName=mtcs_gen_get_platform_name(mtcs_gen_get());
   char *platUpperName=mtcs_gen_get_platform_upper_name(mtcs_gen_get());

   insns = new hash_table <insn_hasher> (31);
   stubs = new hash_table <nofree_string_hash> (31);
   have_funcs = new hash_map <nofree_string_hash, const char *>;

   md_rtx_info info;
   while (read_md_rtx (&info))
      switch (GET_CODE (info.def)){
         case DEFINE_INSN:
         case DEFINE_EXPAND:
            add_insn (&info);
            break;

         default:
            break;
      }
   /* Output a routine to convert an rtx to an rtx_insn sequence.
   ??? At some point the gen_* functions themselves should return
   rtx_insns.  */

   NString *defineCodes=n_string_new("");
   NString *declarationCodes=n_string_new("");

   n_string_append(defineCodes,"/* Generated automatically by the program `mtcsgentargetdef'.  */\n");
   n_string_append_printf(defineCodes,"\n");
   n_string_append_printf(defineCodes,"#ifndef __%s_INSN_TARGET_DEF_H__\n",platUpperName);
   n_string_append_printf(defineCodes,"#define __%s_INSN_TARGET_DEF_H__\n",platUpperName);

   n_string_append(defineCodes, "\nstatic inline rtx_insn *insnify (TargetRtx *targetRtx,rtx x)\n");
   n_string_append(defineCodes,"{\n");
   n_string_append(defineCodes,"\tMtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(targetRtx);\n");
   n_string_append(defineCodes,"\tMtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;\n");
   n_string_append(defineCodes,"\tMtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);\n");
   n_string_append(defineCodes,"\tMtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);\n");
   n_string_append(defineCodes,"\tMtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);\n");

   n_string_append(defineCodes,"\tif (!x)\n");
   n_string_append(defineCodes,"\t\treturn NULL;\n");
   n_string_append(defineCodes,"\tif (rtx_insn *insn = dyn_cast <rtx_insn *> (x))\n");
   n_string_append(defineCodes,"\t\treturn insn;\n");
   n_string_append(defineCodes,"\tmtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);\n");
   n_string_append(defineCodes,"\tmtcs_emit_emit/*!emit*/(mtcsEmit,x, false);\n");
   n_string_append(defineCodes,"\trtx_insn *res = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);\n");
   n_string_append(defineCodes,"\tmtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);\n");
   n_string_append(defineCodes,"\treturn res;\n");
   n_string_append(defineCodes,"}\n");

   n_string_append(defineCodes,"\nstatic bool bool_void_false(TargetRtx *targetRtx)\n");
   n_string_append(defineCodes,"{\n");
   n_string_append(defineCodes,"\treturn false;\n");
   n_string_append(defineCodes,"}\n");

   n_string_append(defineCodes,"\nstatic bool bool_void_true(TargetRtx *targetRtx)\n");
   n_string_append(defineCodes,"{\n");
   n_string_append(defineCodes,"\treturn true;\n");
   n_string_append(defineCodes,"}\n");

   n_string_append_printf(declarationCodes,"\nstatic void %s_target_rtx_init (TargetRtx *targetRtx)\n",platName);
   n_string_append(declarationCodes,"{\n");


#define DEF_TARGET_INSN(INSN, ARGS) \
   def_target_insn (defineCodes,declarationCodes,#INSN, #ARGS);
   #include "target-insns.def"
#undef DEF_TARGET_INSN

   NString *cCodes=n_string_new("");
   NString *hCodes=n_string_new("");
#define DEF_TARGET_INSN(INSN, ARGS) \
   createGenFuncCFileHFile (hCodes,cCodes,#INSN, #ARGS);
   #include "target-insns.def"
#undef DEF_TARGET_INSN

#define DEF_TARGET_INSN(INSN, ARGS) \
   createHaveFuncCFileHFile (hCodes,cCodes,#INSN, #ARGS);
   #include "target-insns.def"
#undef DEF_TARGET_INSN

   //初始化函数结束
   n_string_append(declarationCodes,"}\n");
   n_string_append(defineCodes,declarationCodes->str);
   n_string_append(defineCodes,"\n");
   n_string_append_printf(defineCodes,"\n#endif /*  __%s_INSN_TARGET_DEF_H__*/\n",platUpperName);
   printf("%s",defineCodes->str);

   n_string_append(hCodes,cCodes->str);
   FILE *f=fopen("/home/sns/xx.c","w");
   fwrite(hCodes->str,1,hCodes->len,f);
   fclose(f);

   if (have_error || ferror (stdout) || fflush (stdout) || fclose (stdout))
      return FATAL_EXIT_CODE;

   return SUCCESS_EXIT_CODE;
}
