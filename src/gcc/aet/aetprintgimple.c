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
#include "tree-ssanames.h"


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

#include "aetprintgimple.h"
#include "aetprinttree.h"

static void printSeq(gimple_seq seq)
{
   if(!seq ){
      printf("printNode gimple_seq is null \n");
      return;
   }
   FILE *dump_orig;
   dump_flags_t local_dump_flags;
   dump_file_info *dfi;
   dfi = g->get_dumps ()->get_dump_file_info (TDI_original);
   dump_orig = dfi->pstream;
   local_dump_flags = dfi->pflags;
   dump_orig = dump_begin (TDI_original, &local_dump_flags);
   if(!dump_orig)
   dump_orig=stderr;
   print_gimple_seq (dump_orig, seq, 0, TDF_RAW|TDF_SLIM);
}

static void printGimple(gimple *q)
{
   if(!q ){
      printf(" printGimple is null \n");
      return;
   }
   FILE *dump_orig;
   dump_flags_t local_dump_flags;
   dump_file_info *dfi;
   dfi = g->get_dumps ()->get_dump_file_info (TDI_original);
   dump_orig = dfi->pstream;
   local_dump_flags = dfi->pflags;
   dump_orig = dump_begin (TDI_original, &local_dump_flags);
   if(!dump_orig)
      dump_orig=stderr;
   print_gimple_stmt (dump_orig, q, 0, TDF_RAW|TDF_SLIM);
}

void  aet_print_gimple_from(gimple *g,const char *file,const char *func,int line)
{
   if(!n_log_is_debug_file(NULL,NULL))
      return;
   printGimple(g);
}

void   aet_print_gimple_skip_debug(gimple *g)
{
   printGimple(g);
}


void  aet_print_seq_from(gimple_seq seq,const char *file,const char *func,int line)
{
   if(!n_log_is_debug_file(NULL,NULL))
      return;
    printSeq(seq);
}

/**
 * 打印 edge
 */
static void print_edge (FILE *outfile, edge e, bool from)
{
   fprintf (outfile, "      (%s ", from ? "edge-from" : "edge-to");
   basic_block bb = from ? e->src : e->dest;
   gcc_assert (bb);
   switch (bb->index){
      case ENTRY_BLOCK:
         fprintf (outfile, "entry");
         break;
      case EXIT_BLOCK:
         fprintf (outfile, "exit");
         break;
      default:
         fprintf (outfile, "%i", bb->index);
         break;
   }

   /* Express edge flags as a string with " | " separator.
   e.g. (flags "FALLTHRU | DFS_BACK").  */
   if (e->flags){
      fprintf (outfile, " (flags \"");
      bool seen_flag = false;
#define DEF_EDGE_FLAG(NAME,IDX)        \
      do {                  \
         if (e->flags & EDGE_##NAME)        \
         {                 \
            if (seen_flag)          \
               fprintf (outfile, " | ");      \
            fprintf (outfile, "%s", (#NAME));   \
            seen_flag = true;       \
         }                 \
      } while (0);
#include "cfg-flags.def"
#undef DEF_EDGE_FLAG

      fprintf (outfile, "\")");
   }

   fprintf (outfile, ")\n");
}

static void printbb(FILE *dump_orig, int i,basic_block bb)
{
   gimple_stmt_iterator gsi, copy_gsi, seq_gsi;
   basic_block prev=bb->prev_bb;
   basic_block next=bb->next_bb;
   edge e;
   edge_iterator ei;

   fprintf(dump_orig,"i:%d bb:%p bb->index:%d 上一个:%p 下一个:%p loop_father:%p\n",i,bb,bb->index,prev,next,bb->loop_father);
   if(bb->preds){
      fprintf(dump_orig,"块中的preds边:%p\n",bb->preds);
      FOR_EACH_EDGE (e, ei, bb->preds){
         print_edge (dump_orig, e, true);
         print_edge (dump_orig, e, false);
      }
   }
   if(bb->succs){
      fprintf(dump_orig,"块中的succs边:%p\n",bb->succs);
      FOR_EACH_EDGE (e, ei, bb->succs){
         print_edge (dump_orig, e, true);
         print_edge (dump_orig, e, false);
      }
   }

   for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
      gimple_seq stmts;
      gimple *stmt = gsi_stmt (gsi);
      enum gimple_code code = gimple_code (stmt);
      fprintf(dump_orig,"\tbb:%p stmt:%p code:%d %s\n",bb,stmt,code,gimple_code_name[code]);
      location_t loc=gimple_location(stmt);
      expanded_location xloc = expand_location (loc);
      if (xloc.line == 0   && (LOCATION_LOCUS (loc) == UNKNOWN_LOCATION  || LOCATION_LOCUS (loc) == BUILTINS_LOCATION)){
         fprintf(dump_orig,"\tbb:%p stmt:%p code:%d %s 位置是空的\n",bb,stmt,code,gimple_code_name[code]);
      }else{
         fprintf(dump_orig,"\tbb:%p stmt:%p code:%d %s 代码位置 file:%s line:%d col:%d\n",
         bb,stmt,code,gimple_code_name[code],xloc.file,xloc.line, xloc.column);
      }
      fprintf(dump_orig,"\t");
      aet_print_gimple(stmt);
   }
}

static void printssa(FILE *dump_orig,struct function *fun)
{
   size_t i;
   tree name;
   FOR_EACH_SSA_NAME (i, name, fun){
      tree v=SSA_NAME_VAR(name);
      fprintf(dump_orig,"printssa i:%d name:%p SSA_NAME_VERSION:%d var:%p %s\n",i,name,SSA_NAME_VERSION (name),v,function_name(fun));
      aet_print_tree(v);
   }
}


void   aet_print_cgraph_node_from(struct cgraph_node *node,const char *file,const char *func,int line)
{
//   if(!n_log_is_debug_file(NULL,NULL))
//      return;
//   if(!node ){
//      printf(" aet_print_cgraph_node_from cgraph_node is null \n");
//      return;
//   }
//   FILE *dump_orig;
//   dump_flags_t local_dump_flags;
//   dump_file_info *dfi;
//   dfi = g->get_dumps ()->get_dump_file_info (TDI_original);
//   dump_orig = dfi->pstream;
//   local_dump_flags = dfi->pflags;
//   dump_orig = dump_begin (TDI_original, &local_dump_flags);
//   if(!dump_orig)
//      dump_orig=stderr;
//   /* Original cfun for the callee, doesn't change.  */
//   struct function *nodeFun = DECL_STRUCT_FUNCTION (node->decl);
//   fprintf(dump_orig,"打印  cgraph_node 名字:%s %p decl:%p nodeFun:%p availability:%d \n",
//            node->name(),node,node->decl,nodeFun,node->get_availability());
//   gcc_assert(node==node->decl->decl_with_vis.symtab_node)
//
//   cgraph_edge *e;
//   int count=0;
//   /* Update the call expr on the edges to call the new version.  */
//   for (e = node->callers; e; e = e->next_caller){
//      tree fndecl=  e->caller->decl;
//      function *fn = DECL_STRUCT_FUNCTION (e->caller->decl);
//      fprintf(dump_orig,"调用者: count:%d %s fn:%p  caller:%p node:%s \n",
//               count++,IDENTIFIER_POINTER(DECL_NAME(fndecl)),fn,e->caller,node->name());
//   }
//   /* Update the call expr on the edges to call the new version.  */
//   count=0;
//   for (e = node->callees; e; e = e->next_callee){
//      tree fndecl=  e->callee->decl;
//      function *fn = DECL_STRUCT_FUNCTION (e->callee->decl);
//      fprintf(dump_orig,"被调者: count:%d %s 被调函数 fn:%p callee:%p 所在节点 node:%s DECL_RESULT:%p\n",
//               count++,IDENTIFIER_POINTER(DECL_NAME(fndecl)),fn,e->callee,node->name(),DECL_RESULT (fndecl));
//   }
//   if(!nodeFun){
//      fprintf(dump_orig,"nodeFun是空的返回 struct cgraph_node node:%p name:%s decl:%p nodeFun:%p\n",node,node->name(),node->decl,nodeFun);
//      return;
//   }
//   fprintf(dump_orig,"函数 entry 进入块:%p\n",ENTRY_BLOCK_PTR_FOR_FN (nodeFun));
//   printbb(dump_orig,-1,ENTRY_BLOCK_PTR_FOR_FN (nodeFun));
//
//   basic_block bb;
//   int countbb=0;
//   FOR_EACH_BB_FN (bb, nodeFun){
//      printbb(dump_orig,countbb++,bb);
//   }
//   fprintf(dump_orig,"函数 exit 退出块:%p %p\n",EXIT_BLOCK_PTR_FOR_FN (nodeFun));
//   printbb(dump_orig,-2,EXIT_BLOCK_PTR_FOR_FN (nodeFun));
//   fprintf(dump_orig,"打印 ssa\n");
//   printssa(dump_orig,nodeFun);
//   countbb = 0;
//   FOR_EACH_BB_REVERSE_FN (bb, nodeFun){
//      fprintf(dump_orig,"附加信息 --- nodeFun:%p i:%d bb:%p bb->index:%d\n",nodeFun,countbb++,bb,bb->index);
//   }

}

void   aet_print_block_from(basic_block bb,const char *file,const char *func,int line)
{
   if(!n_log_is_debug_file(NULL,NULL))
      return;
   if(!bb ){
      printf(" aet_print_block_from basic_block is null \n");
      return;
   }
   FILE *dump_orig;
   dump_flags_t local_dump_flags;
   dump_file_info *dfi;
   dfi = g->get_dumps ()->get_dump_file_info (TDI_original);
   dump_orig = dfi->pstream;
   local_dump_flags = dfi->pflags;
   dump_orig = dump_begin (TDI_original, &local_dump_flags);
   if(!dump_orig)
   dump_orig=stderr;
   /* Original cfun for the callee, doesn't change.  */
   fprintf(dump_orig,"打印 basic_block-----\n");
   gimple_stmt_iterator gsi, copy_gsi, seq_gsi;
   for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
      gimple *stmt = gsi_stmt (gsi);
      fprintf(dump_orig,"打印 basic_block中的gimple:%p\n",stmt);
      aet_print_gimple(stmt);
   }
}


