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
#include "gimple.h"
#include "alloc-pool.h"
#include "timevar.h"
#include "memmodel.h"
#include "tm_p.h"
#include "optabs-libfuncs.h"
#include "insn-config.h"
#include "ira.h"
#include "recog.h"
#include "cgraph.h"
#include "coverage.h"
#include "diagnostic.h"
#include "varasm.h"
#include "tree-inline.h"
#include "realmpfr.h"   /* For GMP/MPFR/MPC versions, in print_version.  */
#include "version.h"
#include "flags.h"
#include "insn-attr.h"
#include "output.h"
#include "toplev.h"
#include "expr.h"
#include "intl.h"
#include "tree-diagnostic.h"
#include "reload.h"
#include "lra.h"
#include "dwarf2asm.h"
#include "debug.h"
#include "common/common-target.h"
#include "langhooks.h"
#include "cfgloop.h" /* for init_set_costs */
#include "hosthooks.h"
#include "opts.h"
#include "opts-diagnostic.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "tsan.h"
#include "plugin.h"
#include "context.h"
#include "pass_manager.h"
#include "auto-profile.h"
#include "dwarf2out.h"
#include "ipa-reference.h"
#include "symbol-summary.h"
#include "tree-vrp.h"
#include "sreal.h"
#include "ipa-cp.h"
#include "ipa-prop.h"
#include "ipa-utils.h"
#include "gcse.h"
#include "omp-offload.h"
#include "edit-context.h"
#include "tree-pass.h"
#include "dumpfile.h"
#include "ipa-fnsummary.h"
#include "dump-context.h"
#include "print-tree.h"
#include "optinfo-emit-json.h"
#include "ipa-modref-tree.h"
#include "ipa-modref.h"
#include "ipa-param-manipulation.h"
#include "dbgcnt.h"
#include "gcc-urlifier.h"
#include "value-prof.h"

#include "tree-pass.h"
#include "stringpool.h"
#include "gimple-ssa.h"
#include "cgraph.h"
#include "coverage.h"
#include "lto-streamer.h"
#include "fold-const.h"
#include "varasm.h"
#include "stor-layout.h"
#include "output.h"
#include "cfgcleanup.h"
#include "gimple-iterator.h"
#include "gimple-fold.h"
#include "gimplify.h"
#include "gimplify-me.h"
#include "tree-cfg.h"
#include "tree-into-ssa.h"
#include "tree-ssa.h"
#include "langhooks.h"
#include "toplev.h"
#include "debug.h"
#include "symbol-summary.h"
#include "tree-vrp.h"
#include "sreal.h"
#include "ipa-cp.h"
#include "ipa-prop.h"
#include "gimple-pretty-print.h"
#include "plugin.h"
#include "ipa-fnsummary.h"
#include "ipa-utils.h"
#include "except.h"
#include "cfgloop.h"
#include "context.h"
#include "pass_manager.h"
#include "tree-nested.h"
#include "dbgcnt.h"
#include "lto-section-names.h"
#include "stringpool.h"
#include "attribs.h"
#include "ipa-inline.h"
#include "omp-offload.h"
#include "symtab-thunks.h"
#include "ipa-reference.h"
#include "ipa-modref-tree.h"
#include "ipa-modref.h"
#include "cfganal.h"
#include "tree-ssa-operands.h"
#include "gimple-ssa.h"
#include "symtab-clones.h"
#include "tree-ssanames.h"
#include "cfghooks.h"

#include "../aetprinttree.h"
#include "../aetprintgimple.h"
#include "../mtcsinfo.h"
#include "mtcscompile.h"
#include "mtcspassmgr.h"
#include "mtcstarget.h"
#include "mtcstool.h"
#include "ptx/mtcsptx.h"
#include "ptx/ptxtool.h"

static void         createTarget(MtcsCompile *self);
static nboolean     collectFunc(MtcsCompile *self,struct function *fun);
static void         collectVars(MtcsCompile *self);
static void         expand_all_functions(MtcsCompile *self);
static void         initOptions(MtcsTarget *mtcsTarget);
static void         convertOptionsToArray(MtcsTarget *mtcsTarget);
static MtcsVarNode *createVarNode(MtcsCompile *self,struct varpool_node *origNode,MtcsTarget *mtcsTarget);
/**
 * 注册两个收集passes到 all_passes 和 all_late_ipa_passes
 */
static void registerCollectPass();
/**
 * 注册插件回调函数
 */
static void compileMtcs_cb (void *event_data, void *data ATTRIBUTE_UNUSED)
{
   MtcsCompile *self = (MtcsCompile *)data;
   mtcs_compile_compile(self);
}

/*
 * 另外一个用户mtcsParser发通知到这里
 */
static void astEnd_cb(AetMediatorUser *user,nboolean haveMtcs)
{
   static const char *pluginName="mtcs_compile";
   MtcsCompile *self=(MtcsCompile *)user;
   n_log_enter_mtcs();
   if(haveMtcs){
      self->haveMtcsFuncOrVar=TRUE;
      n_debug("mtcscompile.c 语法树生成完毕 创建mtcs target,并加入两个pass \n");
      registerCollectPass();
      mtcs_adjust_pass_disable_vect_pass(self->mtcsAdjustPass);
      mtcs_adjust_pass_disable_fnsplit_pass(self->mtcsAdjustPass);
      mtcs_adjust_pass_disable_slp_pass(self->mtcsAdjustPass);
      createTarget(self);
//      flag_plugin_added = true;
//      register_callback (pluginName, PLUGIN_FINISH_UNIT,compileMtcs_cb, (void*)self);
   }
}

/**
 * 根据平台 version、isa获取 虚拟硬件能力层级
 * 对于cuda平台：
 * version 代表 PTX ISA 版本 比如:7.5、8.0等
 * isa 代表 真正的 GPU 架构，比如：sm_30、sm_75等
 */
static char *getComputeVersion_cb(AetMediatorUser *user,const char *platName,int version,int isa)
{
   if(strcmp(platName,"cuda")==0){
      PtxIsa ptxIsa=(PtxIsa)isa;
      return ptx_tool_sm_version_to_string(ptxIsa);
   }else{
      n_error("%s平台还未实现！",platName);
   }
   return NULL;
}

static char *getAsmVarName_cb(AetMediatorUser *user,const char *platName,int version,int isa,char *fileName)
{
   char    *ret=mtcs_tool_create_asm_varname(platName,isa,version,fileName);
   n_debug("mtcscompile.c getAsmVarName_cb-- %s %s\n",ret,fileName);
   return ret;
}

//创建MtcsCompile对象，也是AetMediatorUser
nboolean  aet_mediator_create_compile()
{
   mtcs_compile_get();
   return TRUE;
}

static void initMediator(MtcsCompile *self)
{
   AetMediator *mediator = aet_mediator_get();
   AetMediatorUser *mediatorUser =(AetMediatorUser *)self;
   mediatorUser->mediator= mediator;
   mediatorUser->astEnd = astEnd_cb;
   mediatorUser->getComputeVersion = getComputeVersion_cb;
   mediatorUser->getAsmVarName = getAsmVarName_cb;
   aet_mediator_add_user(mediator,mediatorUser);
}

static void mtcsCompileInit(MtcsCompile *self)
{
    self->currentMtcsTarget=NULL;
    self->targetCount=0;
    self->haveMtcsFuncOrVar=FALSE;
    self->running=FALSE;
    self->undecidedVarArray=n_ptr_array_new();
    self->mtcsAdjustPass=mtcs_adjust_pass_new();
    initMediator(self);
}

/* Initialize the compiler back end.  This function is called only once,
   when starting the compiler.  */
static void backend_init (MtcsCompile *self)
{
   MtcsTarget *mtcsTarget=self->currentMtcsTarget;
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   //原型 init_emit_once rtl.h emit-rtl.cc
   mtcs_rtl_init_emit_once(mtcsRTL);
   //原型 init_rtlanal rtl.h rtlanal.cc
   mtcs_rtlanal_init_rtlanal(mtcsRtlanal);
   n_debug("mtcscompile.c backend_init 00 init_varasm_once %s\n",in_fnames[0]);
   //  init_inline_once ();
   //原型   init_varasm_once ();
   mtcs_asm_init_varasm_once(mtcsAsm);
   n_debug("mtcscompile.c backend_init 11 save_register_info %s\n",in_fnames[0]);
   //原型  save_register_info ();
   mtcs_reg_save_register_info(mtcsReg);
   n_debug("mtcscompile.c backend_init 22 init_emit_regs %s\n",in_fnames[0]);
   //  /* Middle end needs this initialization for default mem attributes
   //     used by early calls to make_decl_rtl.  */
   //原型  init_emit_regs ();'
   mtcs_rtl_init_emit_regs(mtcsRTL);

   n_debug("mtcscompile.c backend_init 33 init_regs %s\n",in_fnames[0]);
   //  /* Middle end needs this initialization for mode tables used to assign
   //     modes to vector variables.  */
   //原型  init_regs ();
   mtcs_reg_init_regs(mtcsReg);
}

static void init_asm_output(MtcsCompile *self,char *name)
{
   MtcsTarget *mtcsTarget=self->currentMtcsTarget;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
   target_asm_out_file_start/*!targetm.asm_out.file_start*/(mtcsMachine->asmOut);
}

/* Initialize things that are both lang-dependent and target-dependent.
   This function can be called more than once if target parameters change.  */
//原型 toplev.cc lang_dependent_init_target
static void lang_dependent_init_target (MtcsCompile *self)
{
   MtcsTarget *mtcsTarget=self->currentMtcsTarget;
   MtcsLibfuncs *mtcsLibfuncs=mtcs_target_get_libfuncs(mtcsTarget);
   /* This creates various _DECL nodes, so needs to be called after the
   front end is initialized.  It also depends on the HAVE_xxx macros
   generated from the target machine description.  */
   //原型 init_optabs (); optabs-libfuncs.h optabs-libfuncs.cc
   mtcs_libfuncs_init_optabs(mtcsLibfuncs);
   n_debug("mtcscompile.c lang_dependent_init_target init_optabs 结束\n");
   //gcc_assert (!this_target_rtl->target_specific_initialized);
}

/* Language-dependent initialization.  Returns nonzero on success.  */
static int lang_dependent_init (MtcsCompile *self,const char *name)
{
   MtcsTarget *mtcsTarget=self->currentMtcsTarget;
   MtcsLang *mtcsLang=mtcs_target_get_lang(mtcsTarget);
   MtcsDebug *mtcsDebug=mtcs_target_get_debug(mtcsTarget);

   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   n_debug("mtcscompile.c  lang_dependent_init 00 %s\n",in_fnames[0]);
   location_t save_loc = input_location;
   /* Other front-end initialization.  */
   input_location = BUILTINS_LOCATION;
   if (mtcs_lang_init_tree_and_builtins/*!lang_hooks.init*/(mtcsLang) == 0)
      return 0;
   input_location = save_loc;
   n_debug("mtcscompile.c lang_dependent_init 11 %s flag_wpa:%d mtcs flag_wpa:%d\n",in_fnames[0],flag_wpa,mtcsOptionsItem->x_flag_wpa);
   init_asm_output (self,name);
   lang_dependent_init_target (self);
   mtcs_debug_init(mtcsDebug,in_fnames[0]);
   return 1;
}

/* Initialization of the front end environment, before command line
   options are parsed.  Signal handlers, internationalization etc.
   ARGV0 is main's argv[0].  */
//原型 general_init toplev.cc
static void general_init (MtcsCompile *self)
{
   MtcsTarget *mtcsTarget=self->currentMtcsTarget;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   /* Initialize register usage now so switches may override.  */
   //原型 init_reg_sets ();
   mtcs_reg_init_reg_sets(mtcsReg);
}

/**
 * 调用docomplie时符号表symtab已替换为mtcs的符号表。
 */
//原型 toplev.cc do_compile
static void doCompile(MtcsCompile *self)
{
    MtcsTarget *mtcsTarget=self->currentMtcsTarget;
    MtcsMode   *mtcsMode=mtcs_target_get_mode(mtcsTarget);
    MtcsFinal  *mtcsFinal=mtcs_target_get_final(mtcsTarget);
    MtcsAsm    *mtcsAsm = mtcs_target_get_asm(mtcsTarget);
    MtcsVar    *mtcsVar = mtcs_target_get_var(mtcsTarget);
    MtcsClones *mtcsClones=mtcs_target_get_clones(mtcsTarget);
    MtcsPort *mtcsPort=mtcs_target_get_port(mtcsTarget);
    MtcsOptions   *mtcsOptions=mtcs_target_get_options(mtcsTarget);
    MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
    MtcsDebug *mtcsDebug=mtcs_target_get_debug(mtcsTarget);
    MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

    //第一部分
    //原型 init_adjust_machine_modes ();
    mtcs_mode_init_adjust_machine_modes(mtcsMode);
   //原型 init_derived_machine_modes ();
    mtcs_mode_init_derived_machine_modes(mtcsMode);
    //第二部分
    //下面注释的6行代码由mtcsmode.c mtcs_mode_init_int方法实现
    //          int i;
    //          for (i = 0; i < NUM_INT_N_ENTS; i ++)
    //            if (targetm.scalar_mode_supported_p (int_n_data[i].m)  && ! standard_type_bitsize (int_n_data[i].bitsize))
    //              int_n_enabled_p[i] = true;
    //            else
    //              int_n_enabled_p[i] = false;
    mtcs_mode_init_int(mtcsMode);
    /* Initialize mpfrs exponent range.  This is important to get
    underflow/overflow in a reasonable timeframe.  */
    //第三部分
    machine_mode mode;
    int min_exp = -1;
    int max_exp = 1;
    MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,mode, MODE_FLOAT)
        if (mtcs_mode_is_scalar_float_p/*!SCALAR_FLOAT_MODE_P*/(mtcsMode,mode)){
            const real_format *fmt = mtcs_mode_get_real_format/*!REAL_MODE_FORMAT*/(mtcsMode,mode);
            if (fmt){
                /* fmt->emin - fmt->p + 1 should be enough but the
                back-and-forth dance in real_to_decimal_for_mode we
                do for checking fails due to rounding effects then.  */
                if ((fmt->emin - fmt->p) < min_exp)
                    min_exp = fmt->emin - fmt->p;
                if (fmt->emax > max_exp)
                    max_exp = fmt->emax;
            }
        }
    n_debug("mtcscompile.c do_compile 00 backend_init pid:%d main_input_filename:%s opts:%p\n",getpid(),
          main_input_filename,mtcsOptionsItem);

    /* E.g. mpc_norm assumes it can square a number without bothering with
    with range scaling, so until that is fixed, double the minimum
    and maximum exponents, plus add some buffer for arithmetics
    on the squared numbers.  */
    if (mpfr_set_emin (2 * (min_exp - 1))  || mpfr_set_emax (2 * (max_exp + 1)))
        sorry ("mpfr not configured to handle all floating modes");

    //第四部分
    /* Set up the back-end if requested.  */
    backend_init(self);
    n_debug("mtcscompile.c do_compile 11 lang_dependent_init insn:%p\n",get_last_insn ());
    //第五部分
    /* Language-dependent initialization.  Returns true on success.  */
    if(lang_dependent_init(self,main_input_filename)){
        //ggc_protect_identifiers = true;
        ((symbol_table *)mtcsTarget->symtab)->initialize ();
       // copySymtab(self,mtcsTarget->symtab);
        // 原型 init_final (main_input_filename);
        mtcs_final_init_final(mtcsFinal,main_input_filename);
       // coverage_init (aux_base_name);
        n_debug("mtcscompile.c do_compile 22  mtcs_port_port\n");
        mtcs_port_port(mtcsPort);
        n_debug("mtcscompile.c do_compile 33 bitsizetype:%p\n",bitsizetype);
        MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
        MtcsTree  *mtcsTree=mtcs_target_get_tree(mtcsTarget);

        mtcsMode->mtcsBackupRestore.backup(&mtcsMode->mtcsBackupRestore);//备份主机的mode
        mtcsRTL->mtcsBackupRestore.backup(&mtcsRTL->mtcsBackupRestore);//备份主机的RTL

        mtcs_port_replace_bitsizetype(mtcsPort);//重要方法
        mtcsTree->mtcsBackupRestore.backup(&mtcsTree->mtcsBackupRestore);//备份设备的tree覆盖主机的global_trees
        mtcs_tree_backup_builtin_info(mtcsTree);
        n_debug("mtcscompile.c do_compile 44 重要的 expand_all_functions 开始\n");
        section *text_section=mtcs_asm_get_text_section (mtcsAsm);
        mtcs_asm_switch_to_section/*!switch_to_section*/(mtcsAsm,text_section);
        mtcs_debug_assembly_start/*!(*debug_hooks->assembly_start)*/(mtcsDebug);
        symtab_node::checking_verify_symtab_nodes ();

        expand_all_functions(self);//zclei
        n_debug("mtcscompile.c do_compile 55  全局变量到汇编 insn:%p\n",get_last_insn ());
        mtcs_var_output_variables/*!output_variables*/(mtcsVar);

        /* This must be at the end.  Some target ports emit end of file directives
           into the assembly file here, and hence we cannot output anything to the
           assembly file after this point.  */
        //原型 toplev.cc compile_file
        target_asm_out_file_end/*!targetm.asm_out.file_end*/(mtcsMachine->asmOut);
        n_debug("mtcscompile.c do_compile 66  恢复主机的 global_trees  RTL mode bitsizetype insn:%p\n",get_last_insn ());
        mtcsTree->mtcsBackupRestore.restore(&mtcsTree->mtcsBackupRestore);//恢复主机的global_trees
        mtcs_tree_restore_builtin_info(mtcsTree);
        mtcsRTL->mtcsBackupRestore.restore(&mtcsRTL->mtcsBackupRestore);//恢复主机的RTL
        mtcsMode->mtcsBackupRestore.restore(&mtcsMode->mtcsBackupRestore);//恢复主机的mode
        mtcs_port_restore_bitsizetype(mtcsPort);//重要，否则aet_print_tree 会报错
    }
    //关闭汇编文件
    mtcs_asm_close(mtcsAsm);
}


/* Figure out what functions we want to assemble.  */
static void mark_functions_to_output (MtcsCompile *self)
{
  bool check_same_comdat_groups = false;
  cgraph_node *node;
  if (flag_checking)
    FOR_EACH_FUNCTION (node)
      gcc_assert (!node->process);

  FOR_EACH_FUNCTION (node){
      tree decl = node->decl;
      gcc_assert (!node->process || node->same_comdat_group);
      if (node->process)
          continue;

      /* We need to output all local functions that are used and not
     always inlined, as well as those that are reachable from
     outside the current compilation unit.  */
      if (node->analyzed  && !node->thunk  && !node->alias  && !node->inlined_to
         && !TREE_ASM_WRITTEN (decl) && !DECL_EXTERNAL (decl)){
          node->process = 1;
          if (node->same_comdat_group){
              cgraph_node *next;
              for (next = dyn_cast<cgraph_node *> (node->same_comdat_group);next != node; next = dyn_cast<cgraph_node *> (next->same_comdat_group))
                if (!next->thunk && !next->alias && !next->comdat_local_p ())
                  next->process = 1;
          }
      }else if (node->same_comdat_group){
          if (flag_checking)
            check_same_comdat_groups = true;
      }else{
      /* We should've reclaimed all functions that are not needed.  */
          if (flag_checking && !node->inlined_to && gimple_has_body_p (decl)
              /* FIXME: in ltrans unit when offline copy is outside partition but inline copies
             are inside partition, we can end up not removing the body since we no longer
             have analyzed node pointing to it.  */
              && !node->in_other_partition   && !node->alias   && !node->clones  && !DECL_EXTERNAL (decl)){
              node->debug ();
              internal_error ("failed to reclaim unneeded function");
          }
          gcc_assert (node->inlined_to
                  || !gimple_has_body_p (decl)
                  || node->in_other_partition
                  || node->clones
                  || DECL_ARTIFICIAL (decl)
                  || DECL_EXTERNAL (decl));

      }
  }

  if (flag_checking && check_same_comdat_groups)
    FOR_EACH_FUNCTION (node)
      if (node->same_comdat_group && !node->process){
          tree decl = node->decl;
          if (!node->inlined_to  && gimple_has_body_p (decl)
              /* FIXME: in an ltrans unit when the offline copy is outside a
             partition but inline copies are inside a partition, we can
             end up not removing the body since we no longer have an
             analyzed node pointing to it.  */
              && !node->in_other_partition   && !node->clones  && !DECL_EXTERNAL (decl)){
              node->debug ();
              internal_error ("failed to reclaim unneeded function in same comdat group");
          }
      }
}

//原型 expand_all_functions cgraphunit.cc
static void expand_all_functions(MtcsCompile *self)
{
   MtcsTarget *mtcsTarget=self->currentMtcsTarget;
   MtcsCgraph *mtcsCgraph=mtcs_target_get_cgraph(mtcsTarget);
   cgraph_node *node;
   cgraph_node **order = XCNEWVEC (cgraph_node *,
   symtab->cgraph_count);
   cgraph_node **tp_first_run_order = XCNEWVEC (cgraph_node *,
   symtab->cgraph_count);
   unsigned int expanded_func_count = 0, profiled_func_count = 0;
   int order_pos, tp_first_run_order_pos = 0, new_order_pos = 0;
   int i;
   n_debug("mtcscompile.c mtcs_compile_expand_function 00 %d\n",symtab->cgraph_count);
   mark_functions_to_output(self);
   symtab->state = EXPANSION;

   order_pos = ipa_reverse_postorder (order);
   gcc_assert (order_pos == symtab->cgraph_count);
   n_debug("mtcscompile.c mtcs_compile_expand_function 11 %d\n",symtab->cgraph_count);

   /* Garbage collector may remove inline clones we eliminate during
   optimization.  So we must be sure to not reference them.  */
   for (i = 0; i < order_pos; i++)
      if (order[i]->process){
         n_debug("mtcscompile.c mtcs_compile_expand_function 22 %s\n",order[i]->name());

         if (order[i]->tp_first_run  && opt_for_fn (order[i]->decl, flag_profile_reorder_functions)){
            n_debug("mtcscompile.c mtcs_compile_expand_function 33 %s\n",order[i]->name());
            tp_first_run_order[tp_first_run_order_pos++] = order[i];
         }else{
            n_debug("mtcscompile.c mtcs_compile_expand_function 44 %s\n",order[i]->name());
            order[new_order_pos++] = order[i];
         }
      }

   /* First output functions with time profile in specified order.  */
   qsort (tp_first_run_order, tp_first_run_order_pos,sizeof (cgraph_node *), tp_first_run_node_cmp);

   for (i = 0; i < tp_first_run_order_pos; i++){
      node = tp_first_run_order[i];
      if (node->process){
         expanded_func_count++;
         profiled_func_count++;
         n_debug("mtcscompile.cTime profile order in expand_all_functions:%s:%d\n",node->dump_asm_name (), node->tp_first_run);

         node->process = 0;
         n_debug("mtcscompile.cmtcs_compile_expand_function 55 %s %s\n",node->name(),node->dump_asm_name ());
         mtcs_cgraph_node_expand(mtcsCgraph,node);//node->expand ();
         n_debug("mtcscompile.c mtcs_compile_expand_function 66 %s %s\n",node->name(),node->dump_asm_name ());
      }
   }

   /* Output functions in RPO so callees get optimized before callers.  This
   makes ipa-ra and other propagators to work.
   FIXME: This is far from optimal code layout.
   Make multiple passes over the list to defer processing of gc
   candidates until all potential uses are seen.  */
   int gc_candidates = 0;
   int prev_gc_candidates = 0;

   while (1){
      for (i = new_order_pos - 1; i >= 0; i--){
         node = order[i];
         if (node->gc_candidate)
            gc_candidates++;
         else if (node->process){
            expanded_func_count++;
            node->process = 0;
            n_debug("mtcscompile.c mtcs_compile_expand_function 77 %s %s insn:%p\n",node->name(),node->dump_asm_name (),get_last_insn ());
            mtcs_cgraph_node_expand(mtcsCgraph,node);//node->expand ();
            n_debug("mtcscompile.c mtcs_compile_expand_function 88 expand end %s %s\n",node->name(),node->dump_asm_name ());
         }
      }
      if (!gc_candidates || gc_candidates == prev_gc_candidates)
         break;
      prev_gc_candidates = gc_candidates;
      gc_candidates = 0;
   }

   /* Free any unused gc_candidate functions.  */
   if (gc_candidates)
      for (i = new_order_pos - 1; i >= 0; i--){
         node = order[i];
         if (node->gc_candidate){
            struct function *fn = DECL_STRUCT_FUNCTION (node->decl);
            n_info("mtcscompile.c 删除未使用的函数 Deleting unused function %s\n",IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (node->decl)));
            node->process = false;
            free_dominance_info (fn, CDI_DOMINATORS);
            free_dominance_info (fn, CDI_POST_DOMINATORS);
            node->release_body (false);
         }
      }

   if (tp_first_run_order_pos)
      fprintf (stderr, "Expanded functions with time profile:%u/%u\n", profiled_func_count, expanded_func_count);

   symtab->process_new_functions ();
   free_gimplify_stack ();
   //zclei 取消了ipa 所以下面三行不需要了
   // MtcsIpaInline *mtcsIpaInline=(MtcsIpaInline *)mtcs_target_get_pass(mtcsTarget,IPA_PASS,"inline");
   // delete mtcsIpaInline->ipa_saved_clone_sources/*!ipa_saved_clone_sources*/;
   // mtcsIpaInline->ipa_saved_clone_sources/*!ipa_saved_clone_sources*/ = NULL;
   free (order);
   free (tp_first_run_order);
}

/**
 * 备份主机符号表
 */
static symbol_table *hostSymtab;
static void symtabBackup()
{
       hostSymtab=symtab;
}

static void symtabRestore()
{
   symtab=hostSymtab;
}


static varpool_node *getUndecideVar(MtcsCompile *self,tree decl)
{
   int i;
   for(i=0;i<self->undecidedVarArray->len;i++){
      varpool_node *item=n_ptr_array_index(self->undecidedVarArray,i);
      if(item->decl==decl){
         return item;
      }
   }
   return NULL;
}

/**
 * 收集MTCS是否引用不是MTCS变量，而是由内部声明的全局变量。
 */
static void collectUnDecideVars(MtcsCompile *self,struct cgraph_node *clone,NPtrArray *undecideVars)
{
   tree fndecl=  clone->decl;
   struct function *nodeFun;
   nodeFun = DECL_STRUCT_FUNCTION (fndecl);
   basic_block bb;
   FOR_EACH_BB_FN (bb, nodeFun){
      gimple_stmt_iterator gsi;
      for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
         gimple *stmt = gsi_stmt (gsi);
         enum gimple_code code = gimple_code (stmt);
         if(code==GIMPLE_ASSIGN){
            gassign *assignStmt = as_a <gassign *> (stmt);
            tree rhs=gimple_assign_rhs1 (assignStmt);
            if(rhs && TREE_CODE(rhs)==ARRAY_REF){
               n_debug("mtcscompile.c collectUnDecideVars 检查是否引用了内部声明的全局变量CSWTCH\n");
               tree op=TREE_OPERAND(rhs,0);
               varpool_node *item=getUndecideVar(self,op);
               if(item!=NULL && !n_ptr_array_find(undecideVars,item,NULL)){
                  n_ptr_array_add(undecideVars,item);
               }
               aet_print_tree(op);
            }
         }
      }
   }
}

/**
 * 追加由于优化产生的全局变量，例如 CSWTCH.1 等
 */
static void appendUndecideVar(MtcsCompile *self)
{
   int i,j;
   NPtrArray *unDecideVars=n_ptr_array_new();
   for(i=0;i<self->targetCount;i++){
      MtcsTarget *mtcsTarget=self->targets[i];
      MtcsVar *mtcsVar=mtcs_target_get_var(mtcsTarget);
      MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
      NPtrArray *funArray=mtcsFunc->funcArray;
      n_ptr_array_remove_range(unDecideVars,0,unDecideVars->len);
      for(j=0;j<funArray->len;j++){
         MtcsFuncNode *funNode=n_ptr_array_index(funArray,j);
         collectUnDecideVars(self,funNode->node,unDecideVars);
      }
      for(j=0;j<unDecideVars->len;j++){
         struct varpool_node *origNode=n_ptr_array_index(unDecideVars,j);
         MtcsVarNode *node=createVarNode(self,origNode,mtcsTarget);
         n_debug("mtcscompile.c appendUndecideVar 收集未定变量 :%d %d %d rtl:%p\n",
               origNode->no_reorder,DECL_HARD_REGISTER (origNode->decl),
               DECL_HAS_VALUE_EXPR_P (origNode->decl),origNode->decl->decl_with_rtl.rtl);
         node->innerCreate = TRUE;//重要，输出汇编时检查有.号，要替换。
         mtcs_var_add_mtcs_node(mtcsVar,node);
      }
   }
}

/**
 * 要求AST可以写入note，为什么在mtcs阶段才写入note
 * 因为在mtcs阶段才能生成链接的函数。
 * 具体写入在middlefile.c中
 */
static void writeNote(MtcsCompile *self)
{
   AetMediator *mediator = aet_mediator_get();
   AetMediatorUser *mediatorUser =(AetMediatorUser *)self;
   aet_mediator_write_note(mediator,mediatorUser);
}

/**
 * 原来有多行关于options的功能在这里执行。
 *  mtcs_options_init_once(mtcsOptions);//原型 toplev.cc main()
 *  mtcs_options_init_opts_obstack(mtcsOptions);
 *  ....
 *   convertOptionsToArray(self);
 *   ...
 *   mtcs_opts_process_options(mtcsOpts,no_backend);
 *   在mtcsclones.c中克隆函数时调用 mtcsCloneCreate-->mtcs_func_pop_cfun-->invoke_set_current_function_hook
 *   由于mtcs_optimization_current_node不是空的，但mtcs_optimization_default_node、opt是空的，引起段错误。
 *   所以把上面options的初始化代码在创建mtcstarget时就开始执行。详见initOptions
 */
void mtcs_compile_compile(MtcsCompile *self)
{
   if(!self->haveMtcsFuncOrVar){
      n_debug("mtcscompile.c mtcs_compile_compile 没有任何函数和变量，返回。target:%d\n",self->targetCount);
      writeNote(self);//重要方法
      return;
   }
   //追加未确定的变量到MTCS
   appendUndecideVar(self);
   n_log_enter_mtcs();
   int i=0;
   self->running=TRUE;
   //设in_ltop_p为TREU，很多地方依赖该变量，编译完后恢复。
   int old_in_lto_p=in_lto_p;
   in_lto_p=true;
   for(i=0;i<self->targetCount;i++){
      self->currentMtcsTarget=self->targets[i];
      MtcsTarget *mtcsTarget= self->currentMtcsTarget;
      n_debug("mtcscompile.c mtcs_compile_compile 00 开始编译核函数 host symtab:%p device symtab:%p\n",symtab,self->currentMtcsTarget->symtab);
      symtabBackup();
      symtab=self->currentMtcsTarget->symtab;
      //由于同一个平台有多个版本号，往往这些版本号是全局的，所以在编译前用每个target的版本号设为全局变量。
      mtcs_target_publish_version(mtcsTarget);
      general_init(self);
      MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
      MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
      MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
      MtcsOpts *mtcsOpts=mtcs_target_get_opts(mtcsTarget);
      MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
      mtcsOptions->mtcsBackupRestore.backup(&mtcsOptions->mtcsBackupRestore);//备份主机的选项
      mtcs_options_override_host_options(mtcsOptions);//用mtcs的选项覆盖主机的选项
      n_debug("mtcscompile.c mtcs_compile_compile 11 x_flag_unsafe_math_optimizations:%d mtcsOptionsItem:%p insn:%p\n",
            mtcsOptionsItem->x_flag_unsafe_math_optimizations,mtcsOptionsItem,get_last_insn ());
      symtab->global_info_ready = true;
      doCompile(self);
      n_debug("mtcscompile.c mtcs_compile_compile 22 恢复主机的选项 最后的insn:%p\n",get_last_insn ());
      mtcsOptions->mtcsBackupRestore.restore(&mtcsOptions->mtcsBackupRestore);//恢复主机的选项
      symtabRestore();
      n_debug("mtcscompile.c mtcs_compile_compile 33 编译完成了 把汇编代码写入主机变量中。并加入调用数学库的代码。\n");
      mtcs_asm_create_asm_var(mtcsAsm);
      //保存需要链接的函数
      char *linkFunc=mtcs_target_get_link_funcname(mtcsTarget);
      int   version=      mtcs_target_get_isa(mtcsTarget);
      int   isa=      mtcs_target_get_isa(mtcsTarget);
      const char* platName=mtcs_target_get_platform_name(mtcsTarget);
      AetMediator *mediator = aet_mediator_get();
      AetMediatorUser *mediatorUser =(AetMediatorUser *)self;
      aet_mediator_add_link_func(mediator,linkFunc,version,isa,platName,mediatorUser);
   }
   self->running=FALSE;
   in_lto_p=old_in_lto_p;
   writeNote(self);//重要方法
}


static void printBBEdge (basic_block bb)
{
   edge_iterator ei;
   edge e;

   for (ei = ei_start (bb->succs); (e = ei_safe_edge (ei)); ){
     fprintf(stderr,"mtcscompile.c printBBEdge succs bb:%p src:%p dst:%p\n",bb,e->src,e->dest);
     ei_next (&ei);

   }
   for (ei = ei_start (bb->preds); (e = ei_safe_edge (ei)); ){
      fprintf(stderr,"mtcscompile.c printBBEdge preds bb:%p src:%p dst:%p\n",bb,e->src,e->dest);
      ei_next (&ei);

   }
}

static void printPHI(basic_block bb)
{
   int i;
   for (gphi_iterator gpi = gsi_start_phis (bb); !gsi_end_p (gpi);  gsi_next (&gpi)){
      gphi *phi = gpi.phi ();
      bool err2 = false;
      unsigned i;
      if (gimple_bb (phi) != bb){
         fprintf(stderr,"mtcscompile.c printPHI 出错了 bb:%p\n",bb);
         error ("gimple_bb (phi) is set to a wrong basic block");
         err2 = true;
      }
      for (i = 0; i < gimple_phi_num_args (phi); i++)
         {
           tree t = gimple_phi_arg_def (phi, i);
           fprintf(stderr,"mtcscompile.c printPHI tree is ----bb:%p tree:%p\n",bb,t);
         }
   }
}

static void printNodeBB(  struct function *mtcs_cfun)
{
   edge e;
   edge_iterator ei;
   basic_block bb;
   gimple_stmt_iterator gsi;
   struct cfg_hooks cfg_hooks= get_cfg_hooks ();


   FOR_EACH_BB_FN (bb, mtcs_cfun){
      n_debug("mtcscompile.cprintNodeBB 00  bb:%p %s\n",bb,cfg_hooks.name);
      for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
         gimple *stmt = gsi_stmt (gsi);
         tree use = gimple_vuse (stmt);
         use_operand_p xd = gimple_vuse_op(stmt);
         n_debug("mtcscompile.c cprintNodeBB 11  bb:%p stmt:%p use:%p use_operand_p:%p\n",bb,stmt,use,xd);
         aet_print_tree(use);
         aet_print_gimple(stmt);
      }
   }
   n_debug("mtcscompile.c cprintNodeBB 22 打印 SSANAMES  cfun:%p mtcs_cfun:%p\n",cfun,mtcs_cfun);
   if(mtcs_cfun->gimple_df){
      int i=0;
      for(i=0;i<SSANAMES (mtcs_cfun)->length();i++){
         n_debug("mtcscompile.c cprintNodeBB 33 i:%d\n",i);
         tree ret = (*SSANAMES (mtcs_cfun))[i];
         aet_print_tree(ret);
      }
   }

}

#define OP_SIZE_INIT 0
#define OP_SIZE_1 (1024 - sizeof (void *))
#define OP_SIZE_2 (1024 * 4 - sizeof (void *))
#define OP_SIZE_3 (1024 * 16 - sizeof (void *))

//查看空函数有没有bb
static void checkBB(MtcsCompile *self)
{
   int i;
   cgraph_node *cnode;
   varpool_node *vnode;

   //symtab 是全局变量
   FOR_EACH_DEFINED_FUNCTION (cnode){
      if(strstr(cnode->name(),"testxx")){
         fprintf(stderr,"mtcscompile.c checkBB 00  %s\n",cnode->name());
         tree fndecl=  cnode->decl;
         struct function *mtcs_cfun;
         mtcs_cfun = DECL_STRUCT_FUNCTION (fndecl);
         push_cfun (mtcs_cfun);
         fprintf(stderr,"mtcscompile.c checkBB 11 full_profile:%d fndecl:%p mtcs_cfun:%p\n",cfun->cfg->full_profile,fndecl,mtcs_cfun);
         fprintf(stderr,"mtcscompile.c checkBB 22 ENTRY_BLOCK_PTR_FOR_FN:%p\n", ENTRY_BLOCK_PTR_FOR_FN (cfun));
         basic_block eb =ENTRY_BLOCK_PTR_FOR_FN (cfun);
         fprintf(stderr,"mtcscompile.c checkBB 33 EDGE_COUNT (eb->succs):%d\n", EDGE_COUNT (eb->succs));
         edge e;
         edge_iterator ei;
         basic_block bb;
         gimple_stmt_iterator gsi;
         struct cfg_hooks cfg_hooks= get_cfg_hooks ();
         FOR_EACH_BB_FN (bb, mtcs_cfun){
            fprintf(stderr,"mtcscompile.c checkBB 44  bb:%p %s\n",bb,cfg_hooks.name);
            for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
               gimple *stmt = gsi_stmt (gsi);
               fprintf(stderr,"mtcscompile.c checkBB 55  printBBGimple 00 bb:%p\n",bb);
               aet_print_gimple(stmt);
            }
         }
         fprintf(stderr,"mtcscompile.c checkBB 66 full_profile:%d \n",cfun->cfg->full_profile);
         fprintf(stderr,"mtcscompile.c checkBB 77 ENTRY_BLOCK_PTR_FOR_FN:%p\n", ENTRY_BLOCK_PTR_FOR_FN (cfun));
         basic_block ebb =ENTRY_BLOCK_PTR_FOR_FN (cfun);
         basic_block lastBB=BASIC_BLOCK_FOR_FN(cfun,last_basic_block_for_fn(cfun)-1);

         fprintf(stderr,"mtcscompile.c checkBB 88 EDGE_COUNT (eb->succs):%d\n", EDGE_COUNT (ebb->succs));
         printPHI(lastBB);
         pop_cfun ();
      }
   }
}

void mtcs_compile_test_edge(MtcsCompile *self)
{
   if(!self->haveMtcsFuncOrVar)
       return ;
    fprintf(stderr,"mtcs_compile_test_edge 00 haveMtcsFuncOrVar:%d\n",self->haveMtcsFuncOrVar);
    MtcsClones *mtcsClones=mtcs_target_get_clones(self->targets[0]);
    mtcs_clones_test_edge(mtcsClones);
}

MtcsTarget *mtcs_compile_get_current_target(MtcsCompile *self)
{
   return self->currentMtcsTarget;
}

nboolean  mtcs_compile_is_compiling(MtcsCompile *self)
{
   return self->running;
}

/*****************************以下是收集mtcs 函数和变量----------------------*/

/* Look for all functions inlined to NODE and update their inlined_to pointers
   to INLINED_TO.  */
static void update_inlined_to_pointer (struct cgraph_node *node, struct cgraph_node *inlined_to)
{
   struct cgraph_edge *e;
   for (e = node->callees; e; e = e->next_callee)
      if (e->callee->inlined_to){
         e->callee->inlined_to = inlined_to;
         update_inlined_to_pointer (e->callee, inlined_to);
      }
}

/* Return true when NODE can be target of an indirect call.  */

static bool is_indirect_call_target_p (struct cgraph_node *node, void *)
{
   return node->indirect_call_target;
}

/* Return true when NODE has ADDR reference.  */
static bool has_addr_references_p (struct cgraph_node *node, void *)
{
   int i;
   struct ipa_ref *ref = NULL;
   for (i = 0; node->iterate_referring (i, ref); i++)
      if (ref->use == IPA_REF_ADDR)
         return true;
   return false;
}


//删除 function由passes.cc 中的 execute_one中 进入    if (todo_after & TODO_discard_function)
//调用       cgraph_node::get (fn)->release_body ();
//原型 symbol_table::remove_unreachable_nodes (FILE *file) cgraph.h 类 symbol_table ipa.cc
static void removeNode (struct cgraph_node *node)
{
   if (node->clone_of){
      fprintf (stderr, "removeNode 11  name:%s %s node->aux :%d\n", node->name(),node->dump_name (),node->aux);
      node->remove_from_clone_tree ();
   }
   node->body_removed = true;
   node->analyzed = false;
   node->definition = false;
   node->cpp_implicit_alias = false;
   node->alias = false;
   node->transparent_alias = false;
   node->thunk = false;
   node->weakref = false;
   /* After early inlining we drop always_inline attributes on
   bodies of functions that are still referenced (have their
   address taken).  */
   DECL_ATTRIBUTES (node->decl) = remove_attribute ("always_inline", DECL_ATTRIBUTES (node->decl));
   if (!node->in_other_partition)
      node->local = false;
   node->remove_callees ();
   node->remove_all_references ();
   if (node->inlined_to  && !node->callers){
      gcc_assert (node->clones);
      node->inlined_to = NULL;
      update_inlined_to_pointer (node, node);
   }
   node->aux = NULL;

   if (node->address_taken && !node->used_from_other_partition){
      if (!node->call_for_symbol_and_aliases (has_addr_references_p, NULL, true)){
         node->address_taken = false;
         if (node->local_p ()
         /* Virtual functions may be kept in cgraph just because
         of possible later devirtualization.  Do not mark them as
         local too early so we won't optimize them out before
         we are done with polymorphic call analysis.  */
         && (!node->call_for_symbol_and_aliases (is_indirect_call_target_p, NULL, true))){
            node->local = true;
         }
      }
   }

   function *fn = DECL_STRUCT_FUNCTION (node->decl);
   fprintf (stderr, "removeNode 22  fn:%p name:%s %s node->aux :%d\n", fn,node->name(),node->dump_name (),node->aux);
   TREE_ASM_WRITTEN(node->decl) = 1; //关键 否则在 cgraph_node::expand (void) 中调用 gcc_assert (TREE_ASM_WRITTEN (decl));出错
   symtab_node::checking_verify_symtab_nodes ();
   FOR_EACH_DEFINED_FUNCTION (node){
      fprintf (stderr, "removeNode 33 还剩的节点 name:%s %s node->aux :%d\n", node->name(),node->dump_name (),node->aux);
   }
}

static MtcsVarNode *createVarNode(MtcsCompile *self,struct varpool_node *origNode,MtcsTarget *mtcsTarget)
{
    MtcsClones *mtcsClones=mtcs_target_get_clones(mtcsTarget);
    struct varpool_node *clone=mtcs_clones_clone_var(mtcsClones,origNode);
    if(clone==NULL){
        n_error("不能克隆变量\n");
    }
    MtcsVarNode *node =n_slice_alloc0 (sizeof(MtcsVarNode));
    node->node=clone;
    node->hostDecl = origNode->decl;
    node->promoteId=-1;
    return node;
}
/**
 * 判断变量是不是内部生成的本文件范围内的静态变量CSWTCH.2
 * 生成CSWTCH.2是因为调用了 tree-switch-conversion.cc 中的两个gimple pass
 * "switchconv" 或 "switchlower_O0" : "switchlower",
 * 变量具体生成在 switch_conversion::build_one_array
 */
static nboolean isCSWTCH(tree decl)
{
   if(!decl || !VAR_P(decl) || !DECL_NAME(decl))
      return FALSE;
   const char *name=IDENTIFIER_POINTER(DECL_NAME(decl));
   if(!startswith(name,"CSWTCH."))
      return FALSE;
   return TREE_CODE(TREE_TYPE(decl))==ARRAY_TYPE
      && TREE_STATIC (decl)
      &&  DECL_INITIAL (decl)
      && DECL_ARTIFICIAL (decl)
      && DECL_IGNORED_P (decl)
      && TREE_CONSTANT (decl)
      && TREE_READONLY (decl)
      && DECL_IGNORED_P (decl);
}

static void collectVars(MtcsCompile *self)
{
   struct symtab_node *n;
   int i;
   varpool_node *vnode;
   FOR_EACH_VARIABLE (vnode){
      n_debug("mtcscompile.c collectVars 收集变量 00 vnode:%p %s\n",vnode,vnode->name());
      if (!DECL_HARD_REGISTER (vnode->decl) && !DECL_HAS_VALUE_EXPR_P (vnode->decl)){
         MtcsVarType mtcsType = mtcs_info_get_var_type(vnode->decl);
         n_debug("mtcscompile.c collectVars 收集变量 11 vnode:%p %s %d\n",vnode,vnode->name(),mtcsType);
         if(mtcsType==MTCS_VAR_NOT){
            if(isCSWTCH(vnode->decl) && !n_ptr_array_find(self->undecidedVarArray,vnode,NULL)){
               //如果有MTCS函数引用到该变量，则加入
               n_debug("mtcscompile.c collectVars 收集变量 22 不是MTCS变量，是优化产生的变量 vnode:%p %s decl:%p\n",
                     vnode,vnode->name(),vnode->decl);
               n_ptr_array_add(self->undecidedVarArray,vnode);
            }
            continue;
         }
         for(i=0;i<self->targetCount;i++){
            MtcsTarget *mtcsTarget=self->targets[i];
            MtcsVar *mtcsVar=mtcs_target_get_var(mtcsTarget);
            MtcsVarNode *node=createVarNode(self,vnode,mtcsTarget);
            n_debug("mtcscompile.c collectVars 收集变量 33 :%d %d %d rtl:%p\n",
                  vnode->no_reorder,DECL_HARD_REGISTER (vnode->decl),DECL_HAS_VALUE_EXPR_P (vnode->decl),vnode->decl->decl_with_rtl.rtl);
            mtcs_var_add_mtcs_node(mtcsVar,node);
         }
      }
   }
}

/**
 * 从主机克隆核函数和设备函数
 */
static MtcsFuncNode *createFuncNode(MtcsCompile *self,struct cgraph_node *origNode,MtcsTarget *mtcsTarget)
{
   MtcsClones *mtcsClones=mtcs_target_get_clones(mtcsTarget);
   struct cgraph_node *clone=mtcs_clones_clone_func(mtcsClones,origNode);
   if(clone==NULL){
      n_error("不能克隆核函数\n");
      return NULL;
   }
   MtcsFuncNode *node =n_slice_alloc0 (sizeof(MtcsFuncNode));
   node->node=clone;
   return node;
}


static void printNodeBB(struct cgraph_node *cnode)
{
   tree fndecl=  cnode->decl;
   struct function *nodeFun;
   nodeFun = DECL_STRUCT_FUNCTION (fndecl);
   n_debug("mtcscompile.c printNodeBB 00 name:%s nodeFun:%p cfun:%p\n",cnode->name(),nodeFun,cfun);
   basic_block bb;
   FOR_EACH_BB_FN (bb, nodeFun){
      gimple_stmt_iterator gsi, seq_gsi;
      n_debug("mtcscompile.c printNodeBB 开始 bb:%p name:%s\n",bb,cnode->name());
      for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
         gimple *stmt = gsi_stmt (gsi);
         aet_print_gimple(stmt);
      }
   }
   n_debug("mtcscompile.c printNodeBB 11 name:%s nodeFun:%p cfun:%p\n",cnode->name(),nodeFun,cfun);
}

/**
 * 移走主机中的MTCS内部函数  __builtin_mtcs_level_dim __builtin_mtcs_shared_var
 * 核函数中 int a = threadIdx.x 被转成
 * int a =__builtin_mtcs_level_dim(THREAD_IDX,0);
 * 把核函数当主机函数编译时，再转成 int a =0
 * 该方法是从mtcsclones.c转过来的，因为有多个平台，克隆后移走，其它平台就没法调用该语句了。
 */
static void relaceBuiltinFnAtHost(struct cgraph_node *hostNodeFun)
{
   if(!hostNodeFun)
      return;

   struct function *nodeFun = DECL_STRUCT_FUNCTION (hostNodeFun->decl);
   basic_block bb;
   FOR_EACH_BB_FN (bb, nodeFun){
      gimple_stmt_iterator gsi, seq_gsi;
      for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); ){
         gimple *stmt = gsi_stmt (gsi);
        // n_debug("mtcscompile.c relaceBuiltinFnAtHost 00 替换主机的mtcs内部函数为0 stmt:%p cfun:%p nodeFun:%p hostNodeFun:%p\n",
               //stmt,cfun,nodeFun,hostNodeFun);
         //aet_print_gimple(stmt);
         enum gimple_code code = gimple_code (stmt);
         if(code != GIMPLE_CALL){
            gsi_next (&gsi);
            continue;
         }
         gcall *callStmt = as_a <gcall *> (stmt);
         tree fndecl = gimple_call_fndecl(callStmt);
         if(mtcs_builtins_is_builtin_fn(fndecl) || mtcs_builtins_is_internal_fn(fndecl)){
            tree lhs = gimple_call_lhs (callStmt);
            n_debug("mtcscompile.c relaceBuiltinFnAtHost 11 替换主机的mtcs内部函数为0 stmt:%p lhs:%p\n",stmt,lhs);
            //现在只有两个内部函数 __builtin_mtcs_level_dim __builtin_mtcs_shared_var
            //其中 __builtin_mtcs_shared_var 是在 mtcsparser中通过add_stmt加入的，所以gimple没有左值。直接移走
            if(!lhs){
               unlink_stmt_vdef (callStmt);//要加 unlink_stmt_vdef release_defs 否则在verify_ssa 报 错误：定义缺失 bug 014
               gsi_remove (&gsi, true);
               release_defs (callStmt);
               continue;
            }else{
               int fncode = mtcs_builtins_get_code(fndecl);
               n_debug("mtcscompile.c relaceBuiltinFnAtHost 11 xx替换主机的mtcs内部函数为0 fncode:%d\n",fncode);
               aet_print_tree(lhs);
               aet_print_tree(TREE_TYPE(lhs));
               tree type = TREE_TYPE(lhs);
               tree region = NULL;
               if(TREE_CODE(type)==INTEGER_TYPE)
                  region = build_int_cst (TREE_TYPE(lhs), 0);
               else if (TREE_CODE(type)==REAL_TYPE){
                  region = build_real(TREE_TYPE(lhs), dconst0);
                  //要加 unlink_stmt_vdef release_defs 否则在verify_ssa 报 错误：定义缺失 bug 014
                  //如果是整型不用加，为什么？
                  //unlink_stmt_vdef (callStmt);
               }else{
                  aet_print_tree_skip_debug(TREE_TYPE(lhs));
                  n_error("不支持从替换核函数或设备函数到主机函数。");
               }
               unlink_stmt_vdef (callStmt);
               gassign *grpl = gimple_build_assign (lhs, region);
               n_debug("mtcscompile.c relaceBuiltinFnAtHost 22 替换主机的mtcs内部函数为 0 stmt:%p grpl:%p\n",stmt,grpl);
               gsi_replace (&gsi, grpl, false);
            }
         }
         gsi_next (&gsi);
      }
   }
}

/**
 * 由在 all_pass中的 mtcs_collect_funcs pass 驱动
 * 收集c代码中的核函数 设备函数, 返回true说明需要清除function
 * 注意：进入这里mtcs_optimization_current_node还未初始化，
 * 选项初始化是在mtcs_compile_compile方法中。
 */
static nboolean collectFunc(MtcsCompile *self,struct function *fun)
{
   tree decl = fun->decl;
   MtcsFuncType mtcsType = mtcs_info_get_func_type(decl);
   if(mtcsType==MTCS_FUNC_NOT)
      return FALSE;
   n_debug("mtcscompile.c collectFunc 00 收集核函数或设备函数 decl:%p TREE_ASM_WRITTEN (decl):%d\n",decl,TREE_ASM_WRITTEN (decl));
   int i=0;
   cgraph_node *cnode;
   cgraph_node *origNode=NULL;
   //第一步，收集各平台的函数
   for(i=0;i<self->targetCount;i++){
      MtcsTarget *mtcsTarget=self->targets[i];
      MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
      //symtab 是全局变量
      FOR_EACH_DEFINED_FUNCTION (cnode){
         if(cnode->decl==fun->decl){
            MtcsFuncNode *node=createFuncNode(self,cnode,mtcsTarget);
            mtcs_func_add_mtcs_node(mtcsFunc,node);
            origNode=cnode;
            break;
         }
      }
   }

   checkBB(self);
   n_debug("mtcscompile.c collectFunc 11 collectFunc type:%d\n",mtcsType);
   //在初始对象的方法TFirst_init_1234ergR5678_TFirst里，不应该给对象方法赋值函数地址
  //否则报错,因为调用removeNode移走了函数。
  //TFirst_init_1234ergR5678_TFirst':
  //ai0.c:(.text+0x264): undefined reference to `_Z6TFirst10testkernelEPN6TFirstE'
   //主机中不需要mtcs的内部函数 __builtin_mtcs_level_dim __builtin_mtcs_shared_var
   //if(mtcsType==MTCS_FUNC_KERNEL || mtcsType==MTCS_FUNC_DEVICE)
     /// removeNode(cnode);// 清除主机的cgraph_node
   if(mtcsType==MTCS_FUNC_KERNEL || mtcsType==MTCS_FUNC_DEVICE || mtcsType==MTCS_FUNC_DEVICE_HOST)
      relaceBuiltinFnAtHost(origNode);
   return 0;//(mtcsType==MTCS_FUNC_KERNEL || mtcsType==MTCS_FUNC_DEVICE);
}

/**
 * 收集MTCS函数和变量的两个pass
 */
namespace
{
   const pass_data pass_data_mtcs_collect_funcs =
   {
      GIMPLE_PASS, /* type */
      "mtcs_collect_funcs", /* name */
      OPTGROUP_NONE, /* optinfo_flags */
      TV_MACH_DEP, /* tv_id */
      0, /* properties_required */
      0, /* properties_provided */
      0, /* properties_destroyed */
      0, /* todo_flags_start */
      0, /* todo_flags_finish */
   };

   class pass_mtcs_collect_funcs : public gimple_opt_pass //public rtl_opt_pass
   {
   public:
      pass_mtcs_collect_funcs (gcc::context *ctxt)
      : gimple_opt_pass (pass_data_mtcs_collect_funcs, ctxt)
      {
      }

      /* opt_pass methods: */
      unsigned int execute (function *fun) final override
      {
         nboolean ret= collectFunc(mtcs_compile_get(),fun);
         //返回 TODO_discard_function 由passes.cc中清除function
         return ret?TODO_discard_function:0;
      }
   };


   //---------------收集变量-------------------
   const pass_data pass_data_mtcs_collect_vars =
   {
     SIMPLE_IPA_PASS,      /* type */
     "mtcs_collect_vars",       /* name */
     OPTGROUP_OMP,         /* optinfo_flags */
     TV_NONE,        /* tv_id */
     ( PROP_ssa | PROP_cfg ), /* properties_required */
     0,           /* properties_provided */
     0,           /* properties_destroyed */
     0,           /* todo_flags_start */
     0,           /* todo_flags_finish */
   };

   class pass_mtcs_collect_vars : public simple_ipa_opt_pass
   {
   public:
      pass_mtcs_collect_vars(gcc::context *ctxt)
       : simple_ipa_opt_pass(pass_data_mtcs_collect_vars, ctxt)
     {}

     /* opt_pass methods: */
     unsigned int execute (function *) final override
     {
        collectVars(mtcs_compile_get());
        return 0;
     }
   };

} // anon namespace


/**
 * 注册两个收集passes到 all_passes 和 all_late_ipa_passes
 * 注册两个pass 1个禁用fnsplit 另一个恢复。
 */
static void registerCollectPass()
{
   gimple_opt_pass *mtcsCollectFuncs = new pass_mtcs_collect_funcs (g);
   register_pass_info mtcsCollectFuncsInfo = { mtcsCollectFuncs, "*warn_function_noreturn", 1, PASS_POS_INSERT_AFTER };
   register_pass (&mtcsCollectFuncsInfo);
   //加在pass'simdclone'后
   simple_ipa_opt_pass *mtcsCollectVars = new pass_mtcs_collect_vars (g);
   register_pass_info mtcsCollectVarsInfo = { mtcsCollectVars, "simdclone", 1, PASS_POS_INSERT_AFTER };
   register_pass (&mtcsCollectVarsInfo);
}

//原型 decode_cmdline_options_to_array_default_mask (); toplev.cc 调用
/* Convert the options to an array.  */
/*
toplev.cc -----参数是:i:0 /home/sns/gcc14-20240421/gccnvptx/libexec/gcc/x86_64-pc-linux-gnu/14.0.1/accel/nvptx-none/lto1 pid:366396
toplev.cc -----参数是:i:1 -quiet pid:366396
toplev.cc -----参数是:i:2 -dumpbase pid:366396
toplev.cc -----参数是:i:3 ./aitest.xnvptx-none.mkoffload pid:366396
toplev.cc -----参数是:i:4 -m64 pid:366396
toplev.cc -----参数是:i:5 -misa=sm_75 pid:366396
toplev.cc -----参数是:i:6 -mptx=_ pid:366396
toplev.cc -----参数是:i:7 -misa=sm_75 pid:366396
toplev.cc -----参数是:i:8 -mptx=_ pid:366396
toplev.cc -----参数是:i:9 -misa=sm_75 pid:366396
toplev.cc -----参数是:i:10 -mptx=_ pid:366396
toplev.cc -----参数是:i:11 -O3 pid:366396
toplev.cc -----参数是:i:12 -O3 pid:366396
toplev.cc -----参数是:i:13 -fno-openmp pid:366396
toplev.cc -----参数是:i:14 -fcf-protection=none pid:366396
toplev.cc -----参数是:i:15 -foffload-abi=lp64 pid:366396
toplev.cc -----参数是:i:16 -fopenacc pid:366396
toplev.cc -----参数是:i:17 -fPIC pid:366396
toplev.cc -----参数是:i:18 /home/sns/workspace/ai/pc-build/debug/ai.o pid:366396
toplev.cc -----参数是:i:19 -o pid:366396
toplev.cc -----参数是:i:20 /tmp/ccnuXN2e.s pid:366396
*/
static void convertOptionsToArray(MtcsTarget *mtcsTarget)
{
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOpts *mtcsOpts=mtcs_target_get_opts(mtcsTarget);
//   int argc=13;
//   const char *argv[13]={
//           "/home/sns/gcc14-20240421/gccnvptx/libexec/gcc/x86_64-pc-linux-gnu/14.0.1/accel/nvptx-none/lto1",
//           "-quiet",
//           "-dumpbase",
//           "./aitest.xnvptx-none.mkoffload",
//           "-m64",
//           "-misa=sm_75",
//           "-mptx=_",
//           "-fopenacc",
//           "-O3",
//           "-fPIC",
//           "/home/sns/workspace/ai/pc-build/debug/ai.o",
//           "-o",
//           "/tmp/ccQXBsTw.s"
//   };
//
   const char *argv[10];
   int argc=2;
   if (optimize == 0){
      argv[1]= "-O0";
   }else if(optimize == 1){
      argv[1]= "-O1";
   }else if(optimize == 2){
      argv[1]= "-O2";
   }else if(optimize == 3){
      argv[1]= "-O3";
   }
   //write_symbols是主机的选项
   if(write_symbols!=NO_DEBUG){
      argv[2]= "-g";
      argc++;
   }
   n_debug("mtcscompile.c convertOptionsToArray optimize:%d 是否调试:%s\n",optimize,write_symbols!=NO_DEBUG?"是":"否");
   unsigned int lang_mask=mtcsOptions->initial_lang_mask | CL_COMMON | CL_TARGET;
   mtcs_opts_decode_cmdline_options_to_array/*decode_cmdline_options_to_array*/(mtcsOpts,argc,CONST_CAST2 (const char **,char **, argv),
           lang_mask, &mtcsOpts->save_decoded_options,&mtcsOpts->save_decoded_options_count);
}

/**
 * 初始化选项
 */
static void initOptions(MtcsTarget *mtcsTarget)
{
   symtabBackup();
   symtab=mtcsTarget->symtab;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
   MtcsOpts *mtcsOpts=mtcs_target_get_opts(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   mtcs_options_init_once(mtcsOptions);//原型 toplev.cc main()
   mtcs_options_init_opts_obstack(mtcsOptions);
   /* Initialize global options structures; this must be repeated for
   each structure used for parsing options.  */
   n_debug("mtcscompile.c initOptions 00 x_flag_ipa_profile:%d mtcsOptionsItem:%p\n",
         mtcsOptionsItem->x_flag_ipa_profile,mtcsOptionsItem);
   mtcs_options_init_options_struct(mtcsOptions,mtcsOptions->global_options,mtcsOptions->global_options_set);
   mtcs_options_hooks_init_options_struct(mtcsOptions);
   convertOptionsToArray(mtcsTarget);
   /* Save Optimization decoded options.  */
   mtcs_opt_save_optimization_decoded_options(mtcsOpts);
   /* Perform language-specific options initialization.  */
   mtcs_options_hooks_init_options(mtcsOptions,mtcsOpts->save_decoded_options_count,mtcsOpts->save_decoded_options);
   /* Parse the options and do minimal processing; basically just
   enough to default flags appropriately.  */
   mtcs_opts_decode_options/*!decode_options*/(mtcsOpts,  UNKNOWN_LOCATION, global_dc);
   //从主机中取出目标需要的选项,并赋值
   //必须放在 mtcs_opts_decode_options 之后，因为mtcs_opts_decode_options 会重置选项。
   mtcs_options_set_options_from_host(mtcsOptions);
   mtcs_opts_handle_common_deferred_options/*!handle_common_deferred_options*/(mtcsOpts);
   /*!no_backend = lang_hooks.post_options (&main_input_filename);*/
   bool no_backend=mtcs_options_post_options/*!no_backend = lang_hooks.post_options (&main_input_filename);*/(mtcsOptions,NULL);
   /*!process_options(self);原型 toplev.cc process_options*/
   mtcs_opts_process_options(mtcsOpts,no_backend);
   symtabRestore();
}

/**
 * 创建目标
 */
static void createTarget(MtcsCompile *self)
{
   if(self->targetCount>0)
      return;
   //获取平台参数
   int *isaAndVersion[10];
   int i;
   for(i=0;i<10;i++)
      isaAndVersion[i]=xmalloc(sizeof(int)*2);
   //0 ptx,1 gcn...
   int count=mtcs_tool_get_isa_and_version(isaAndVersion,0);
   if(count==0){
     n_debug("mtcscompile.c 创建缺省的 ptx目标---- \n");
     self->targets[0]=(MtcsTarget *)mtcs_ptx_new();
   }else{
      n_debug("mtcscompile.c 创建第一个带架构版本和PTX版本的目标 总数量:%d smisa:%d ptxversion:%d\n",
            count,isaAndVersion[0][0],isaAndVersion[0][1]);
     self->targets[0]=(MtcsTarget *)mtcs_ptx_new_full(isaAndVersion[0][0],isaAndVersion[0][1]);
   }
   self->currentMtcsTarget=self->targets[0];
   initOptions(self->targets[0]);
   self->currentMtcsTarget=NULL;
   self->targetCount=1;
   for(i=0;i<10;i++)
       free(isaAndVersion[i]);
}

MtcsCompile *mtcs_compile_get()
{
   static MtcsCompile *singleton = NULL;
   if (!singleton){
      singleton =n_slice_alloc0 (sizeof(MtcsCompile));
      mtcsCompileInit(singleton);
   }
   return singleton;
}

void mtcs_info_print_node()
{
   mtcs_compile_test_edge(mtcs_compile_get());
}

