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
#include "splay-tree.h"
#include "tree-dump.h"
#include "tree-iterator.h"


#include "mtcsclones.h"
#include "mtcstarget.h"
#include "mtcscompile.h"
#include "../aetprinttree.h"
#include "../aetprintgimple.h"
#include "mtcstraversetree.h"

typedef struct dump_info *dump_info_p;
/* Information about a node to be dumped.  */

//typedef struct dump_node_info
//{
//  /* The index for the node.  */
//  unsigned int index;
//  /* Nonzero if the node is a binfo.  */
//  unsigned int binfo_p : 1;
//} *dump_node_info_p;
//
///* A dump_queue is a link in the queue of things to be dumped.  */
//
//typedef struct dump_queue
//{
//  /* The queued tree node.  */
//  splay_tree_node node;
//  /* The next node in the queue.  */
//  struct dump_queue *next;
//} *dump_queue_p;
//
///* A dump_info gives information about how we should perform the dump
//   and about the current state of the dump.  */
//
//struct dump_info
//{
//  const_tree node;
//  /* User flags.  */
//  dump_flags_t flags;
//  /* The next unused node index.  */
//  unsigned int index;
//  /* The next column.  */
//  unsigned int column;
//  /* The first node in the queue of nodes to be written out.  */
//  dump_queue_p queue;
//  /* The last node in the queue.  */
//  dump_queue_p queue_end;
//  /* Free queue nodes.  */
//  dump_queue_p free_list;
//  /* The tree nodes which we have already written out.  The
//     keys are the addresses of the nodes; the values are the integer
//     indices we assigned them.  */
//  splay_tree nodes;
//};

static void mtcsTraverseTreeInit(MtcsTraverseTree *self)
{
    self->treeArray=n_ptr_array_new();
}

static nboolean exists(MtcsTraverseTree *self,tree value)
{
    int i;
    for(i=0;i<self->treeArray->len;i++)
        if(n_ptr_array_index(self->treeArray,i)==value)
            return TRUE;
    return FALSE;
}

static void replaceMode(MtcsTraverseTree *self,tree value)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    if(exists(self,value)){
        return;
    }
    if(DECL_P(value)){
       machine_mode mode=DECL_MODE(value);
       machine_mode dm=(machine_mode)mtcs_mode_host2device(mtcsMode,mode);
       SET_DECL_MODE(value,dm);
       n_ptr_array_add(self->treeArray,value);
    }else{
        enum tree_code code;
        enum tree_code_class code_class;
        code = TREE_CODE (value);
        code_class = TREE_CODE_CLASS (code);
        if (code_class == tcc_type){
            machine_mode mode=TYPE_MODE(value);
            machine_mode dm=(machine_mode)mtcs_mode_host2device(mtcsMode,mode);
            SET_TYPE_MODE(value,dm);
            n_ptr_array_add(self->treeArray,value);
        }
    }
}

/* Add T to the end of the queue of nodes to dump.  Returns the index
   assigned to T.  */

static unsigned int queue (dump_info_p di, const_tree t, int flags)
{
  dump_queue_p dq;
  dump_node_info_p dni;
  unsigned int index;
  /* Assign the next available index to T.  */
  index = ++di->index;
  /* Obtain a new queue node.  */
  if (di->free_list){
      dq = di->free_list;
      di->free_list = dq->next;
  }else
    dq = XNEW (struct dump_queue);
  /* Create a new entry in the splay-tree.  */
  dni = XNEW (struct dump_node_info);
  dni->index = index;
  dni->binfo_p = ((flags & DUMP_BINFO) != 0);
  dq->node = splay_tree_insert (di->nodes, (splay_tree_key) t,(splay_tree_value) dni);
  /* Add it to the end of the queue.  */
  dq->next = 0;
  if (!di->queue_end)
    di->queue = dq;
  else
    di->queue_end->next = dq;
  di->queue_end = dq;

  /* Return the index.  */
  return index;
}

/* Dump the string field S.  */

static void write_string_field (dump_info_p di, const char *field, const char *string)
{
//  dump_maybe_newline (di);
//  fprintf (di->stream, "%-4s: %-7s ", field, string);
//  if (strlen (string) > 7)
//    di->column += 6 + strlen (string) + 1;
//  else
//    di->column += 14;
}

static void write_index (dump_info_p di, unsigned int index)
{
//  fprintf (di->stream, "@%-6u ", index);
//  di->column += 8;
}

/* Dump integer I using FIELD to identify it.  */

static void write_int (dump_info_p di, const char *field, int i)
{
//  dump_maybe_newline (di);
//  fprintf (di->stream, "%-4s: %-7d ", field, i);
//  di->column += 14;
}

/* Dump the floating point value R, using FIELD to identify it.  */

static void write_real (dump_info_p di, const char *field, const REAL_VALUE_TYPE *r)
{
//  char buf[32];
//  real_to_decimal (buf, r, sizeof (buf), 0, true);
//  dump_maybe_newline (di);
//  fprintf (di->stream, "%-4s: %s ", field, buf);
//  di->column += strlen (buf) + 7;
}

/* Dump the fixed-point value F, using FIELD to identify it.  */

static void write_fixed (dump_info_p di, const char *field, const FIXED_VALUE_TYPE *f)
{
//  char buf[32];
//  fixed_to_decimal (buf, f, sizeof (buf));
//  dump_maybe_newline (di);
//  fprintf (di->stream, "%-4s: %s ", field, buf);
//  di->column += strlen (buf) + 7;
}


/* If T has not already been output, queue it for subsequent output.
   FIELD is a string to print before printing the index.  Then, the
   index of T is printed.  */

static void queue_and_write_index (dump_info_p di, const char *field, const_tree t, int flags)
{
  unsigned int index;
  splay_tree_node n;

  /* If there's no node, just return.  This makes for fewer checks in
     our callers.  */
  if (!t)
    return;

  /* See if we've already queued or dumped this node.  */
  n = splay_tree_lookup (di->nodes, (splay_tree_key) t);
  if (n)
    index = ((dump_node_info_p) n->value)->index;
  else
    /* If we haven't, add it to the queue.  */
    index = queue (di, t, flags);

  /* Print the index of the node.  */
  //dump_maybe_newline (di);
 // fprintf (di->stream, "%-4s: ", field);
 // di->column += 6;
  write_index (di, index);
}

/* Dump pointer PTR using FIELD to identify it.  */

static void write_pointer (dump_info_p di, const char *field, void *ptr)
{
//  dump_maybe_newline (di);
//  fprintf (di->stream, "%-4s: %-8" HOST_WIDE_INT_PRINT "x ", field,
//       (unsigned HOST_WIDE_INT) (uintptr_t) ptr);
//  di->column += 15;
}

/* Return nonzero if FLAG has been specified for the dump, and NODE
   is not the root node of the dump.  */

static int write_flag (dump_info_p di, dump_flags_t flag, const_tree node)
{
  return (di->flags & flag) && (node != di->node);
}

static void queue_and_write_type (dump_info_p di, const_tree t)
{
  queue_and_write_index (di, "type", TREE_TYPE (t), DUMP_NONE);
}

#define write_child(field, child) \
        queue_and_write_index (di, field, child, DUMP_NONE)
/* Dump the next node in the queue.  */

static void dequeue_and_dump (MtcsTraverseTree *self,dump_info_p di)
{
  dump_queue_p dq;
  splay_tree_node stn;
  dump_node_info_p dni;
  tree t;
  unsigned int index;
  enum tree_code code;
  enum tree_code_class code_class;
  const char* code_name;

  /* Get the next node from the queue.  */
  dq = di->queue;
  stn = dq->node;
  t = (tree) stn->key;
  dni = (dump_node_info_p) stn->value;
  index = dni->index;

  /* Remove the node from the queue, and put it on the free list.  */
  di->queue = dq->next;
  if (!di->queue)
    di->queue_end = 0;
  dq->next = di->free_list;
  di->free_list = dq;

  /* Print the node index.  */
  write_index (di, index);
  /* And the type of node this is.  */
  if (dni->binfo_p)
    code_name = "binfo";
  else
    code_name = get_tree_code_name (TREE_CODE (t));
  //fprintf (di->stream, "%-16s ", code_name);
  //di->column = 25;

  /* Figure out what kind of node this is.  */
  code = TREE_CODE (t);
  code_class = TREE_CODE_CLASS (code);

  /* Although BINFOs are TREE_VECs, we dump them specially so as to be
     more informative.  */
  if (dni->binfo_p){
      fprintf(stderr,"mtcstraversetree.c 00 dni->binfo_p %p\n",di);

      unsigned ix;
      tree base;
      vec<tree, va_gc> *accesses = BINFO_BASE_ACCESSES (t);
      write_child ("type", BINFO_TYPE (t));

      if (BINFO_VIRTUAL_P (t))
          write_string_field (di, "spec", "virt");

      write_int (di, "bases", BINFO_N_BASE_BINFOS (t));
      for (ix = 0; BINFO_BASE_ITERATE (t, ix, base); ix++){
          tree access = (accesses ? (*accesses)[ix] : access_public_node);
          const char *string = NULL;
          if (access == access_public_node)
            string = "pub";
          else if (access == access_protected_node)
            string = "prot";
          else if (access == access_private_node)
            string = "priv";
          else
            gcc_unreachable ();

          write_string_field (di, "accs", string);
          queue_and_write_index (di, "binf", base, DUMP_BINFO);
      }
      goto done;
  }

  /* We can knock off a bunch of expression nodes in exactly the same
     way.  */
  if (IS_EXPR_CODE_CLASS (code_class)){
      /* If we're dumping children, dump them now.  */
     n_debug("mtcstraversetree.c 11 IS_EXPR_CODE_CLASS %p\n",di);
      aet_print_tree(t);
      queue_and_write_type (di, t);
      switch (code_class){
        case tcc_unary:
            write_child ("op 0", TREE_OPERAND (t, 0));
            break;

        case tcc_binary:
        case tcc_comparison:
            write_child ("op 0", TREE_OPERAND (t, 0));
            write_child ("op 1", TREE_OPERAND (t, 1));
            break;

        case tcc_expression:
        case tcc_reference:
        case tcc_statement:
        case tcc_vl_exp:
            /* These nodes are handled explicitly below.  */
           break;

        default:
          gcc_unreachable ();
      }
  }else if (DECL_P (t)){
      expanded_location xloc;
      /* All declarations have names.  */
      if (DECL_NAME (t))
          write_child ("name", DECL_NAME (t));
      if (HAS_DECL_ASSEMBLER_NAME_P (t)
      && DECL_ASSEMBLER_NAME_SET_P (t)
      && DECL_ASSEMBLER_NAME (t) != DECL_NAME (t))
          write_child ("mngl", DECL_ASSEMBLER_NAME (t));
      if (DECL_ABSTRACT_ORIGIN (t))
          write_child ("orig", DECL_ABSTRACT_ORIGIN (t));
      /* And types.  */
      n_debug("mtcstraversetree.c 22 DECL_P (t) %p mode:%d\n",di,DECL_MODE(t));
      aet_print_tree(t);
      queue_and_write_type (di, t);
      write_child ("scpe", DECL_CONTEXT (t));

      /* And a source position.  */
//      xloc = expand_location (DECL_SOURCE_LOCATION (t));
//      if (xloc.file){
//          const char *filename = lbasename (xloc.file);
//
//          dump_maybe_newline (di);
//          fprintf (di->stream, "srcp: %s:%-6d ", filename,
//               xloc.line);
//          di->column += 6 + strlen (filename) + 8;
//      }
      /* And any declaration can be compiler-generated.  */
      if (CODE_CONTAINS_STRUCT (TREE_CODE (t), TS_DECL_COMMON) && DECL_ARTIFICIAL (t))
          write_string_field (di, "note", "artificial");
      if (DECL_CHAIN (t) && !dump_flag (di, TDF_SLIM, NULL))
          write_child ("chain", DECL_CHAIN (t));

      replaceMode(self,t);
  }else if (code_class == tcc_type){
      /* All types have qualifiers.  */
      int quals = lang_hooks.tree_dump.type_quals (t);
      n_debug("mtcstraversetree.c 33 code_class == tcc_type %p quals:%d mode:%d\n",di,quals,TYPE_MODE(t));
      aet_print_tree(t);

      if (quals != TYPE_UNQUALIFIED){
//          fprintf (di->stream, "qual: %c%c%c     ",
//               (quals & TYPE_QUAL_CONST) ? 'c' : ' ',
//               (quals & TYPE_QUAL_VOLATILE) ? 'v' : ' ',
//               (quals & TYPE_QUAL_RESTRICT) ? 'r' : ' ');
          di->column += 14;
      }

      /* All types have associated declarations.  */
      write_child ("name", TYPE_NAME (t));

      /* All types have a main variant.  */
      if (TYPE_MAIN_VARIANT (t) != t)
          write_child ("unql", TYPE_MAIN_VARIANT (t));

      /* And sizes.  */
      write_child ("size", TYPE_SIZE (t));

      /* All types have alignments.  */
      write_int (di, "algn", TYPE_ALIGN (t));
      replaceMode(self,t);

  }else if (code_class == tcc_constant){
      n_debug("mtcstraversetree.c 44 code_class == tcc_constant %p\n",di);
      aet_print_tree(t);

      /* All constants can have types.  */
      queue_and_write_type (di, t);
  }

  /* Give the language-specific code a chance to print something.  If
     it's completely taken care of things, don't bother printing
     anything more ourselves.  */
   if (lang_hooks.tree_dump.dump_tree (di, t)){
      n_debug("mtcstraversetree.c 55 lang_hooks.tree_dump.dump_tree %p\n",di);
      aet_print_tree(t);
      goto done;
   }
  /* Now handle the various kinds of nodes.  */
  switch (code){
      int i;

    case IDENTIFIER_NODE:
      write_string_field (di, "strg", IDENTIFIER_POINTER (t));
      write_int (di, "lngt", IDENTIFIER_LENGTH (t));
      break;

    case TREE_LIST:
        write_child ("purp", TREE_PURPOSE (t));
        write_child ("valu", TREE_VALUE (t));
        write_child ("chan", TREE_CHAIN (t));
        break;

    case STATEMENT_LIST:
      {
        tree_stmt_iterator it;
        for (i = 0, it = tsi_start (t); !tsi_end_p (it); tsi_next (&it), i++){
            char buffer[32];
            sprintf (buffer, "%u", i);
            write_child (buffer, tsi_stmt (it));
        }
      }
      break;

    case TREE_VEC:
      write_int (di, "lngt", TREE_VEC_LENGTH (t));
      for (i = 0; i < TREE_VEC_LENGTH (t); ++i){
          char buffer[32];
          sprintf (buffer, "%u", i);
          write_child (buffer, TREE_VEC_ELT (t, i));
    }
      break;

    case INTEGER_TYPE:
    case ENUMERAL_TYPE:
      write_int (di, "prec", TYPE_PRECISION (t));
      write_string_field (di, "sign", TYPE_UNSIGNED (t) ? "unsigned": "signed");
      write_child ("min", TYPE_MIN_VALUE (t));
      write_child ("max", TYPE_MAX_VALUE (t));

      if (code == ENUMERAL_TYPE)
          write_child ("csts", TYPE_VALUES (t));
      break;

    case REAL_TYPE:
        write_int (di, "prec", TYPE_PRECISION (t));
        break;

    case FIXED_POINT_TYPE:
      write_int (di, "prec", TYPE_PRECISION (t));
      write_string_field (di, "sign", TYPE_UNSIGNED (t) ? "unsigned": "signed");
      write_string_field (di, "saturating",
             TYPE_SATURATING (t) ? "saturating": "non-saturating");
      break;

    case POINTER_TYPE:
        write_child ("ptd", TREE_TYPE (t));
      break;

    case REFERENCE_TYPE:
        write_child ("refd", TREE_TYPE (t));
      break;

    case METHOD_TYPE:
        write_child ("clas", TYPE_METHOD_BASETYPE (t));
      /* Fall through.  */

    case FUNCTION_TYPE:
        write_child ("retn", TREE_TYPE (t));
        write_child ("prms", TYPE_ARG_TYPES (t));
      break;

    case ARRAY_TYPE:
        write_child ("elts", TREE_TYPE (t));
        write_child ("domn", TYPE_DOMAIN (t));
      break;

    case RECORD_TYPE:
    case UNION_TYPE:
      if (TREE_CODE (t) == RECORD_TYPE)
          write_string_field (di, "tag", "struct");
      else
          write_string_field (di, "tag", "union");

      write_child ("flds", TYPE_FIELDS (t));
      queue_and_write_index (di, "binf", TYPE_BINFO (t),DUMP_BINFO);
      break;

    case CONST_DECL:
        write_child ("cnst", DECL_INITIAL (t));
      break;

    case DEBUG_EXPR_DECL:
      write_int (di, "-uid", DEBUG_TEMP_UID (t));
      /* Fall through.  */

    case VAR_DECL:
    case PARM_DECL:
    case FIELD_DECL:
    case RESULT_DECL:
      if (TREE_CODE (t) == PARM_DECL)
          write_child ("argt", DECL_ARG_TYPE (t));
      else
          write_child ("init", DECL_INITIAL (t));
      write_child ("size", DECL_SIZE (t));
      write_int (di, "algn", DECL_ALIGN (t));

      if (TREE_CODE (t) == FIELD_DECL){
          if (DECL_FIELD_OFFSET (t))
              write_child ("bpos", bit_position (t));
      }else if (VAR_P (t) || TREE_CODE (t) == PARM_DECL){
          write_int (di, "used", TREE_USED (t));
          if (DECL_REGISTER (t))
            write_string_field (di, "spec", "register");
      }
      break;

    case FUNCTION_DECL:
        write_child ("args", DECL_ARGUMENTS (t));
        if (DECL_EXTERNAL (t))
            write_string_field (di, "body", "undefined");
        if (TREE_PUBLIC (t))
            write_string_field (di, "link", "extern");
        else
            write_string_field (di, "link", "static");
        if (DECL_SAVED_TREE (t) /*!&& !write_flag (di, TDF_SLIM, t)*/){
            n_debug("mtcstraversetree.c 66 DECL_SAVED_TREE (t) %p\n",di);
                  aet_print_tree(DECL_SAVED_TREE (t));
          write_child ("body", DECL_SAVED_TREE (t));
        }
        break;

    case INTEGER_CST:
      //fprintf (di->stream, "int: ");
     // print_decs (wi::to_wide (t), di->stream);
      break;

    case STRING_CST:
     // fprintf (di->stream, "strg: %-7s ", TREE_STRING_POINTER (t));
      write_int (di, "lngt", TREE_STRING_LENGTH (t));
      break;

    case REAL_CST:
      write_real (di, "valu", TREE_REAL_CST_PTR (t));
      break;

    case FIXED_CST:
      write_fixed (di, "valu", TREE_FIXED_CST_PTR (t));
      break;

    case TRUTH_NOT_EXPR:
    case ADDR_EXPR:
    case INDIRECT_REF:
    case CLEANUP_POINT_EXPR:
    case VIEW_CONVERT_EXPR:
    case SAVE_EXPR:
    case REALPART_EXPR:
    case IMAGPART_EXPR:
      /* These nodes are unary, but do not have code class `1'.  */
      write_child ("op 0", TREE_OPERAND (t, 0));
      break;

    case TRUTH_ANDIF_EXPR:
    case TRUTH_ORIF_EXPR:
    case INIT_EXPR:
    case MODIFY_EXPR:
    case COMPOUND_EXPR:
    case PREDECREMENT_EXPR:
    case PREINCREMENT_EXPR:
    case POSTDECREMENT_EXPR:
    case POSTINCREMENT_EXPR:
      /* These nodes are binary, but do not have code class `2'.  */
        write_child ("op 0", TREE_OPERAND (t, 0));
        write_child ("op 1", TREE_OPERAND (t, 1));
        break;

    case COMPONENT_REF:
    case BIT_FIELD_REF:
        write_child ("op 0", TREE_OPERAND (t, 0));
        write_child ("op 1", TREE_OPERAND (t, 1));
        write_child ("op 2", TREE_OPERAND (t, 2));
      break;

    case ARRAY_REF:
    case ARRAY_RANGE_REF:
        write_child ("op 0", TREE_OPERAND (t, 0));
        write_child ("op 1", TREE_OPERAND (t, 1));
        write_child ("op 2", TREE_OPERAND (t, 2));
        write_child ("op 3", TREE_OPERAND (t, 3));
      break;

    case COND_EXPR:
        write_child ("op 0", TREE_OPERAND (t, 0));
        write_child ("op 1", TREE_OPERAND (t, 1));
        write_child ("op 2", TREE_OPERAND (t, 2));
      break;

    case TRY_FINALLY_EXPR:
    case EH_ELSE_EXPR:
        write_child ("op 0", TREE_OPERAND (t, 0));
        write_child ("op 1", TREE_OPERAND (t, 1));
      break;

    case CALL_EXPR:
      {
        int i = 0;
        tree arg;
        call_expr_arg_iterator iter;
        dump_child ("fn", CALL_EXPR_FN (t));
        FOR_EACH_CALL_EXPR_ARG (arg, iter, t){
            char buffer[32];
            sprintf (buffer, "%u", i);
            write_child (buffer, arg);
            i++;
        }
      }
      break;

    case CONSTRUCTOR:
      {
        unsigned HOST_WIDE_INT cnt;
        tree index, value;
        write_int (di, "lngt", CONSTRUCTOR_NELTS (t));
        FOR_EACH_CONSTRUCTOR_ELT (CONSTRUCTOR_ELTS (t), cnt, index, value){
            write_child ("idx", index);
            write_child ("val", value);
        }
      }
      break;

    case BIND_EXPR:
        write_child ("vars", TREE_OPERAND (t, 0));
        write_child ("body", TREE_OPERAND (t, 1));
      break;

    case LOOP_EXPR:
        write_child ("body", TREE_OPERAND (t, 0));
      break;

    case EXIT_EXPR:
        write_child ("cond", TREE_OPERAND (t, 0));
      break;

    case RETURN_EXPR:
        write_child ("expr", TREE_OPERAND (t, 0));
      break;

    case TARGET_EXPR:
        write_child ("decl", TREE_OPERAND (t, 0));
        write_child ("init", TREE_OPERAND (t, 1));
        write_child ("clnp", TREE_OPERAND (t, 2));
      /* There really are two possible places the initializer can be.
     After RTL expansion, the second operand is moved to the
     position of the fourth operand, and the second operand
     becomes NULL.  */
        write_child ("init", TREE_OPERAND (t, 3));
      break;

    case CASE_LABEL_EXPR:
        write_child ("name", CASE_LABEL (t));
        if (CASE_LOW (t)){
           write_child ("low ", CASE_LOW (t));
          if (CASE_HIGH (t))
             write_child ("high", CASE_HIGH (t));
        }
        break;
    case LABEL_EXPR:
        write_child ("name", TREE_OPERAND (t,0));
        break;
    case GOTO_EXPR:
        write_child ("labl", TREE_OPERAND (t, 0));
        break;
    case SWITCH_EXPR:
        write_child ("cond", TREE_OPERAND (t, 0));
        write_child ("body", TREE_OPERAND (t, 1));
        break;
    case OMP_CLAUSE:
      {
        int i;
      //  fprintf (di->stream, "%s\n", omp_clause_code_name[OMP_CLAUSE_CODE (t)]);
        for (i = 0; i < omp_clause_num_ops[OMP_CLAUSE_CODE (t)]; i++)
            write_child ("op: ", OMP_CLAUSE_OPERAND (t, i));
      }
      break;
    default:
      /* There are no additional fields to print.  */
      break;
 }

 done:
  if (write_flag (di, TDF_ADDRESS, NULL))
      write_pointer (di, "addr", (void *)t);

  /* Terminate the line.  */
 // fprintf (di->stream, "\n");
}


static void traverseFunDecl(MtcsTraverseTree *self,tree fndecl)
{
    struct dump_info di;
    dump_queue_p dq;
    dump_queue_p next_dq;
    /* Initialize the dump-information structure.  */
    di.index = 0;
    di.column = 0;
    di.queue = 0;
    di.queue_end = 0;
    di.free_list = 0;
    di.flags = TDF_ALL_VALUES;
    di.node = fndecl;
    di.nodes = splay_tree_new (splay_tree_compare_pointers, 0,
                   splay_tree_delete_pointers);
    /* Queue up the first node.  */
     queue (&di, fndecl, DUMP_NONE);
     /* Until the queue is empty, keep dumping nodes.  */
     int count=0;
     while (di.queue){
         fprintf(stderr,"mtcstraversetree.c mtcs_traverse_tree_traverse %d\n",count++);
       dequeue_and_dump (self,&di);
     }
     /* Now, clean up.  */
     for (dq = di.free_list; dq; dq = next_dq){
         next_dq = dq->next;
         free (dq);
     }
     splay_tree_delete (di.nodes);
}

static void replace(MtcsTraverseTree *self,tree t)
{
    if(!t)
        return;
    struct dump_info di;
    dump_queue_p dq;
    dump_queue_p next_dq;
    /* Initialize the dump-information structure.  */
    di.index = 0;
    di.column = 0;
    di.queue = 0;
    di.queue_end = 0;
    di.free_list = 0;
    di.flags = TDF_ALL_VALUES;
    di.node = t;
    di.nodes = splay_tree_new (splay_tree_compare_pointers, 0,
    splay_tree_delete_pointers);
    /* Queue up the first node.  */
    queue (&di, t, DUMP_NONE);
    /* Until the queue is empty, keep dumping nodes.  */
    int count=0;
    while (di.queue){
        fprintf(stderr,"mtcstraversetree.c replace %d\n",count++);
        dequeue_and_dump (self,&di);
    }
    /* Now, clean up.  */
    for (dq = di.free_list; dq; dq = next_dq){
        next_dq = dq->next;
        free (dq);
    }
    splay_tree_delete (di.nodes);
}



/*
 * 有一个tree
 * use_optype_d 定义在 tree-ssa-operands.h中 use_operand_p就是ssa_use_operand_t 定义在tree-core.h中
 * ssa_use_operand_t中有一个 tree *成员
struct GTY(()) gimple_statement_with_ops_base : public gimple
{
  struct use_optype_d GTY((skip (""))) *use_ops;
};



/**
 * 以下是 extend gimple
 */
static void replaceBaseOps(MtcsTraverseTree *self,gimple *stmt)
{
    struct use_optype_d *useOptType=gimple_use_ops (stmt);
    if(useOptType){
        //use_operand_p use_p=useOptType->use_ptr;
       // tree *var = get_use_from_ptr (use_p);
    }
}

static void replaceBind(MtcsTraverseTree *self,gimple *stmt)
{
    fprintf(stderr,"mtcstraversetree.c gbind 00\n");
    gbind *bindStmt = as_a <gbind *> (stmt);
    tree vars=gimple_bind_vars(bindStmt);
    tree block=gimple_bind_block(bindStmt);
    replace(self,vars);
    replace(self,block);
}

static void replaceEH_MUST_NOT_THROW(MtcsTraverseTree *self,gimple *gs)
{
    fprintf(stderr,"mtcstraversetree.c EH_MUST_NOT_THROW 00\n");
    geh_mnt *ehmntStmt = as_a <geh_mnt *> (gs);
    tree fndecl=gimple_eh_must_not_throw_fndecl(ehmntStmt);
    replace(self,fndecl);
}

static void replaceASSUME(MtcsTraverseTree *self,gimple *gs)
{
    fprintf(stderr,"mtcstraversetree.c ASSUME 00\n");
    tree guard=gimple_assume_guard(gs);
    replace(self,guard);
}

static void replacePHI(MtcsTraverseTree *self,gimple *gs)
{
    fprintf(stderr,"mtcstraversetree.c PHI \n");
    gphi *phiStmt = as_a <gphi *> (gs);
    tree result=gimple_phi_result (gs);
    replace(self,result);
    const unsigned nums =gimple_phi_num_args (phiStmt);
    for (unsigned i = 0; i < nums; ++i){
       tree t = gimple_phi_arg_def (phiStmt, i);
       replace(self,t);
    }
}

static void replaceCATCH(MtcsTraverseTree *self,gimple *gs)
{
    fprintf(stderr,"mtcstraversetree.c CATCH \n");
    gcatch *catchStmt = as_a <gcatch *> (gs);
    tree types=gimple_catch_types (catchStmt);
    replace(self,types);
}

static void replaceEH_FILTER(MtcsTraverseTree *self,gimple *gs)
{
    fprintf(stderr,"mtcstraversetree.c EH_FILTER \n");
    geh_filter *ehfilterStmt = as_a <geh_filter *> (gs);
    tree types=gimple_eh_filter_types (ehfilterStmt);
    replace(self,types);
}



/**
 * 以下是 extend gimple_statement_with_ops
 *              extend gimple_statement_with_ops_base
 *                     extend gimple

struct GTY((tag("GSS_WITH_OPS")))  gimple_statement_with_ops : public gimple_statement_with_ops_base
{
  tree GTY((length ("%h.num_ops"))) op[1];
};
*/
static void replaceCOND(MtcsTraverseTree *self,gimple *gs)
{
    replaceBaseOps(self,gs);
    tree lhs=gimple_cond_lhs(gs);
    tree rhs=gimple_cond_rhs(gs);
    const gcond *gc = GIMPLE_CHECK2<const gcond *> (gs);
    tree trueLabel=gimple_cond_true_label(gc);
    tree falseLabel=gimple_cond_false_label(gc);
    replace(self,lhs);
    replace(self,rhs);
    replace(self,trueLabel);
    replace(self,falseLabel);
}

static void replaceGOTO(MtcsTraverseTree *self,gimple *gs)
{
    replaceBaseOps(self,gs);
    tree dest=gimple_goto_dest(gs);
    replace(self,dest);
}

static void replaceSWITCH(MtcsTraverseTree *self,gimple *gs)
{
    replaceBaseOps(self,gs);
    gswitch *switchStmt = as_a <gswitch *> (gs);
    tree switchIndex=gimple_switch_index (switchStmt);
    tree defaultLabel= gimple_switch_default_label (switchStmt);
    replace(self,switchIndex);
    replace(self,defaultLabel);
    nuint nums=gimple_switch_num_labels (switchStmt);
    int i;
    for (i = 0; i < nums; i++){
        tree caseLabel = gimple_switch_label (switchStmt, i);
        replace(self,caseLabel);
    }
}

static void replaceDEBUG(MtcsTraverseTree *self,gimple *gs)
{
    replaceBaseOps(self,gs);
    tree var= gimple_debug_bind_get_var(gs);
    tree value= gimple_debug_source_bind_get_value (gs);
    replace(self,var);
    replace(self,value);
}

static void replaceLABEL(MtcsTraverseTree *self,gimple *gs)
{
    replaceBaseOps(self,gs);
    glabel *labelStmt = as_a <glabel *> (gs);
    tree llabel= gimple_label_label(labelStmt);
    replace(self,llabel);
}

/**
 * 以下是 extend gimple_statement_with_memory_ops
 *              extend gimple_statement_with_memory_ops_base
 *                      extend gimple_statement_with_ops_base
 *                             extend gimple
 */
/**
 * GIMPLE_RETURN
 * GIMPLE_ASSIGN:
 * 这两个gimple继承自
 * gimple_statement_with_memory_ops 有一个tree op[1]
 * gimple_statement_with_memory_ops_base有两个 tree vdef vuse
 * gimple_statement_with_memory_ops继承自 gimple_statement_with_memory_ops_base
 * gimple_statement_with_memory_ops_base继承自gimple_statement_with_ops_base
 *
 * struct GTY((tag("GSS_WITH_MEM_OPS")))
  gimple_statement_with_memory_ops :  public gimple_statement_with_memory_ops_base
{
  tree GTY((length ("%h.num_ops"))) op[1];
};

struct GTY((tag("GSS_WITH_MEM_OPS_BASE"))) gimple_statement_with_memory_ops_base : public gimple_statement_with_ops_base
{
  tree GTY((skip (""))) vdef;
  tree GTY((skip (""))) vuse;
};
 */
static void replaceBaseMemory(MtcsTraverseTree *self,gimple *stmt)
{
    tree  vdef=gimple_vdef(stmt);
    tree  vuse=gimple_vuse(stmt);
    replace(self,vdef);
    replace(self,vuse);
}

static void replaceRETURN(MtcsTraverseTree *self,gimple *gs)
{
    replaceBaseOps(self,gs);
    replaceBaseMemory(self,gs);
    greturn *returnStmt = as_a <greturn *> (gs);
    tree retval= gimple_return_retval (returnStmt);
    replace(self,retval);
}

static void replaceASSIGN(MtcsTraverseTree *self,gimple *gs)
{
    replaceBaseOps(self,gs);
    replaceBaseMemory(self,gs);
    gassign *assignStmt = as_a <gassign *> (gs);
    tree lhs= gimple_assign_lhs (assignStmt);
    tree rhs1=gimple_assign_rhs1 (assignStmt);
    tree rhs2=gimple_assign_rhs2 (assignStmt);
    tree rhs3=gimple_assign_rhs3 (assignStmt);
    replace(self,lhs);
    replace(self,rhs1);
    replace(self,rhs2);
    replace(self,rhs3);
}

/**
 * 以下是 extend gimple_statement_with_memory_ops_base
 *              extend gimple_statement_with_ops_base
 *                     extend gimple
 */

static void replaceASM(MtcsTraverseTree *self,gimple *gs)
{
    replaceBaseOps(self,gs);
    replaceBaseMemory(self,gs);
    gasm *asmStmt = as_a <gasm *> (gs);
    const unsigned noutputs = gimple_asm_noutputs (asmStmt);
    const unsigned ninputs = gimple_asm_ninputs (asmStmt);
    const unsigned nclobbers = gimple_asm_nclobbers (asmStmt);
    const unsigned nlabels = gimple_asm_nlabels (asmStmt);

    for (unsigned i = 0; i < noutputs; ++i){
         tree t = gimple_asm_output_op (asmStmt, i);
         replace(self,t);
    }
    for (unsigned i = 0; i < ninputs; i++){
         tree t = gimple_asm_input_op (asmStmt, i);
         replace(self,t);
    }
    for (unsigned int i = 0; i < nclobbers; ++i){
        tree t = gimple_asm_clobber_op (asmStmt, i);
        replace(self,t);
    }
    for (unsigned int i = 0; i < nlabels; ++i){
        tree t = gimple_asm_label_op (asmStmt, i);
        replace(self,t);
    }
}

static void replaceCALL(MtcsTraverseTree *self,gimple *gs)
{
    replaceBaseOps(self,gs);
    replaceBaseMemory(self,gs);
    gcall *callStmt = as_a <gcall *> (gs);
    tree lhs=gimple_call_lhs (callStmt);
    tree fntype=gimple_call_fntype (callStmt);
    tree fn= gimple_call_fn (callStmt);
    tree fndecl=gimple_call_fndecl(callStmt);
    tree returnType=gimple_call_return_type(callStmt);
    tree chain=gimple_call_chain(callStmt);
    replace(self,lhs);
    replace(self,fntype);
    replace(self,fn);
    replace(self,fndecl);
    replace(self,returnType);
    replace(self,chain);
    const unsigned argCount = gimple_call_num_args (callStmt);
    for (unsigned i = 0; i < argCount; ++i){
         tree t = gimple_call_arg (callStmt, i);
         replace(self,t);
    }
}

static void replaceTRANSACTION(MtcsTraverseTree *self,gimple *gs)
{
    replaceBaseOps(self,gs);
    replaceBaseMemory(self,gs);
    gtransaction *transactionStmt = as_a <gtransaction *> (gs);
    tree norm=gimple_transaction_label_norm (transactionStmt);
    tree uninst=gimple_transaction_label_uninst (transactionStmt);
    tree over=gimple_transaction_label_over (transactionStmt);
    replace(self,norm);
    replace(self,uninst);
    replace(self,over);
}



static void replaceGimple(MtcsTraverseTree *self,gimple *stmt)
{
  enum gimple_code code = gimple_code (stmt);

  switch (code){
  //以下是extend gimple
      case GIMPLE_BIND:
          replaceBind(self,stmt);
          return;
      case GIMPLE_EH_MUST_NOT_THROW:
          replaceEH_MUST_NOT_THROW(self,stmt);
          return;
      case GIMPLE_ASSUME:
          replaceASSUME(self,stmt);
          return;
      case GIMPLE_PHI:
          replacePHI(self,stmt);
          return;
      case GIMPLE_CATCH:
          replaceCATCH(self,stmt);
          return;
      case GIMPLE_EH_FILTER:
          replaceEH_FILTER(self,stmt);
          return;
     /*以下是
          extend gimple_statement_with_ops
                extend gimple_statement_with_ops_base
                     extend gimple
                     */
      case GIMPLE_COND:
          replaceCOND(self,stmt);
          return;
      case GIMPLE_GOTO:
          replaceGOTO(self,stmt);
          return;
      case GIMPLE_SWITCH:
          replaceSWITCH(self,stmt);
          return;
      case GIMPLE_DEBUG:
          replaceDEBUG(self,stmt);
          return;
      case GIMPLE_LABEL:
          replaceLABEL(self,stmt);
          return;
          /**
           * 以下是 extend gimple_statement_with_memory_ops
           *              extend gimple_statement_with_memory_ops_base
           *                      extend gimple_statement_with_ops_base
           *                             extend gimple
           */
      case GIMPLE_RETURN:
          replaceRETURN(self,stmt);
          return;
      case GIMPLE_ASSIGN:
          replaceASSIGN(self,stmt);
          return;
          /**
           * 以下是 extend gimple_statement_with_memory_ops_base
           *              extend gimple_statement_with_ops_base
           *                     extend gimple
           */
      case GIMPLE_ASM:
          replaceASM(self,stmt);
          return;

      case GIMPLE_CALL:
          replaceCALL(self,stmt);
          return;
      case GIMPLE_TRANSACTION:
          replaceTRANSACTION(self,stmt);
          return;
      //没有tree
      case GIMPLE_TRY:
          //extend gtry expand gimple
      case GIMPLE_EH_ELSE:
          //extend gimple
      case GIMPLE_NOP:
          //extend gimple
      case GIMPLE_PREDICT:
          //extend gimple
      case GIMPLE_ERROR_MARK:
          //extend gimple
          fprintf(stderr,"没有tree GIMPLE_TRY code:%d name:%s\n",code,gimple_code_name[code]);
           return;
      case GIMPLE_RESX:
      case GIMPLE_EH_DISPATCH:
          //extend gimple_statement_eh_ctrl extend gimple
          fprintf(stderr,"没有tree GIMPLE_TRY code:%d name:%s\n",code,gimple_code_name[code]);
          return;
      case GIMPLE_WITH_CLEANUP_EXPR:
          //extend gimple_statement_wce extend gimple
          fprintf(stderr,"没有tree GIMPLE_TRY code:%d name:%s\n",code,gimple_code_name[code]);
          return;
      case GIMPLE_OMP_STRUCTURED_BLOCK:
      case GIMPLE_OMP_FOR:
      case GIMPLE_OMP_SCOPE:
      case GIMPLE_OMP_SECTIONS:
      case GIMPLE_OMP_SECTIONS_SWITCH:
      case GIMPLE_OMP_SECTION:
      case GIMPLE_OMP_SINGLE:
      case GIMPLE_OMP_MASTER:
      case GIMPLE_OMP_MASKED:
      case GIMPLE_OMP_TASKGROUP:
      case GIMPLE_OMP_ORDERED:
      case GIMPLE_OMP_SCAN:
      case GIMPLE_OMP_CRITICAL:
      case GIMPLE_OMP_RETURN:
      case GIMPLE_OMP_ATOMIC_LOAD:
      case GIMPLE_OMP_ATOMIC_STORE:
      case GIMPLE_OMP_CONTINUE:
      case GIMPLE_OMP_PARALLEL:
      case GIMPLE_OMP_TASK:
      case GIMPLE_OMP_TARGET:
      case GIMPLE_OMP_TEAMS:
          fprintf(stderr,"mtcs函数中不应该出现omp相关的代码。\n");
          gcc_unreachable ();
      default:
          fprintf(stderr,"mtcs函数中还没替换的gimple类型。code:%d name:%d\n",code,gimple_code_name[code]);
          gcc_unreachable ();
  }

}


static void printGimpleCode()
{
    int i;
    while(true){
        if(gimple_code_name[i]==NULL)
            break;
        fprintf(stderr,"gimple_code_name --- i:%d %s\n",i,gimple_code_name[i]);
        i++;
    }
}

void  mtcs_traverse_tree_node(MtcsTraverseTree *self,struct cgraph_node *node)
{
    /* Original cfun for the callee, doesn't change.  */
    struct function *nodeFun = DECL_STRUCT_FUNCTION (node->decl);
    fprintf(stderr,"mtcstraversetree.c mtcs_traverse_tree_gimple cgraph_node node:%p name:%s decl:%p nodeFun:%p availability:%d\n",
            node,node->name(),node->decl,nodeFun,node->get_availability());
    printGimpleCode();
    cgraph_edge *e;
    int count=0;
    /* Update the call expr on the edges to call the new version.  */
    for (e = node->callers; e; e = e->next_caller){
        tree fndecl=  e->caller->decl;
        function *fn = DECL_STRUCT_FUNCTION (e->caller->decl);
        fprintf(stderr,"mtcstraversetree.c 调用者: count:%d %s fn:%p  caller:%p node:%s\n",
        count++,IDENTIFIER_POINTER(DECL_NAME(fndecl)),fn,e->caller,node->name());
    }
    /* Update the call expr on the edges to call the new version.  */
    count=0;
    for (e = node->callees; e; e = e->next_callee){
        tree fndecl=  e->callee->decl;
        function *fn = DECL_STRUCT_FUNCTION (e->callee->decl);
        fprintf(stderr,"mtcstraversetree.c 被调者: count:%d %s fn:%p callee:%p node:%s\n",
        count++,IDENTIFIER_POINTER(DECL_NAME(fndecl)),fn,e->callee,node->name());
    }
    //遍历函数声明中的DECL_MODE和TYPE_MODE 并替换
    traverseFunDecl(self,node->decl);
    //遍历bb块中的gimple中的树的DECL_MODE和TYPE_MODE 并替换
    basic_block bb;
    FOR_EACH_BB_FN (bb, nodeFun){
        gimple_stmt_iterator gsi, seq_gsi;
        for (gsi = gsi_start_bb (bb); !gsi_end_p (gsi); gsi_next (&gsi)){
            gimple_seq stmts;
            gimple *stmt = gsi_stmt (gsi);
            replaceGimple(self,stmt);
        }
    }
}

/**
 * value是否是被替换的
 */
nboolean mtcs_traverse_tree_be_replaced(MtcsTraverseTree *self,tree value)
{
   return true;// return exists(self,value);
}

/**
 * 恢复tree中的设备mode变为主机的mode
 */
void mtcs_traverse_tree_restore_mode(MtcsTraverseTree *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

      int i;
      for(i=0;i<self->treeArray->len;i++){
             tree value=n_ptr_array_index(self->treeArray,i);

             if(DECL_P(value)){
                machine_mode mode=DECL_MODE(value);
                machine_mode dm=(machine_mode)mtcs_mode_device2host(mtcsMode,mode);
                SET_DECL_MODE(value,dm);
             }else{
                 enum tree_code code;
                 enum tree_code_class code_class;
                 code = TREE_CODE (value);
                 code_class = TREE_CODE_CLASS (code);
                 if (code_class == tcc_type){
                     machine_mode mode=TYPE_MODE(value);
                     machine_mode dm=(machine_mode)mtcs_mode_device2host(mtcsMode,mode);
                     SET_TYPE_MODE(value,dm);
                 }
             }

      }
      n_ptr_array_remove_range(self->treeArray,0,self->treeArray->len);
}

MtcsTraverseTree *mtcs_traverse_tree_new(MtcsMode *mtcsMode)
{
     MtcsTraverseTree *self = n_slice_alloc0 (sizeof(MtcsTraverseTree));
     mtcs_component_set_mode((MtcsComponent *)self,mtcsMode);
     mtcsTraverseTreeInit(self);
     return self;
}

// 整理有tree的gimple
//nline tree
//gimple_block (const gimple *g)
//{
//  return LOCATION_BLOCK (g->location);
//}
//
//inline tree
//gimple_vuse (const gimple *g)
//{
//  const gimple_statement_with_memory_ops *mem_ops_stmt =
//     dyn_cast <const gimple_statement_with_memory_ops *> (g);
//  if (!mem_ops_stmt)
//    return NULL_TREE;
//  return mem_ops_stmt->vuse;
//}
//
//inline tree
//gimple_vdef (const gimple *g)
//{
//  const gimple_statement_with_memory_ops *mem_ops_stmt =
//     dyn_cast <const gimple_statement_with_memory_ops *> (g);
//  if (!mem_ops_stmt)
//    return NULL_TREE;
//  return mem_ops_stmt->vdef;
//}
//
///* Return operand I for statement GS.  */
//
//inline tree
//gimple_op (const gimple *gs, unsigned i)
//{
//  if (gimple_has_ops (gs))
//    {
//      gcc_gimple_checking_assert (i < gimple_num_ops (gs));
//      return gimple_ops (CONST_CAST_GIMPLE (gs))[i];
//    }
//  else
//    return NULL_TREE;
//}
//
///* Return the LHS of assignment statement GS.  */
//
//inline tree
//gimple_assign_lhs (const gassign *gs)
//{
//  return gs->op[0];
//}
//
//inline tree
//gimple_assign_lhs (const gimple *gs)
//{
//  const gassign *ass = GIMPLE_CHECK2<const gassign *> (gs);
//  return gimple_assign_lhs (ass);
//}
//
//
//
///* Return the first operand on the RHS of assignment statement GS.  */
//
//inline tree
//gimple_assign_rhs1 (const gassign *gs)
//{
//  return gs->op[1];
//}
//
//inline tree
//gimple_assign_rhs1 (const gimple *gs)
//{
//  const gassign *ass = GIMPLE_CHECK2<const gassign *> (gs);
//  return gimple_assign_rhs1 (ass);
//}
//
//
//
///* Return the second operand on the RHS of assignment statement GS.
//   If GS does not have two operands, NULL is returned instead.  */
//
//inline tree
//gimple_assign_rhs2 (const gassign *gs)
//{
//  if (gimple_num_ops (gs) >= 3)
//    return gs->op[2];
//  else
//    return NULL_TREE;
//}
//
//inline tree
//gimple_assign_rhs2 (const gimple *gs)
//{
//  const gassign *ass = GIMPLE_CHECK2<const gassign *> (gs);
//  return gimple_assign_rhs2 (ass);
//}
//
//
//
///* Return the third operand on the RHS of assignment statement GS.
//   If GS does not have two operands, NULL is returned instead.  */
//
//inline tree
//gimple_assign_rhs3 (const gassign *gs)
//{
//  if (gimple_num_ops (gs) >= 4)
//    return gs->op[3];
//  else
//    return NULL_TREE;
//}
//
//inline tree
//gimple_assign_rhs3 (const gimple *gs)
//{
//  const gassign *ass = GIMPLE_CHECK2<const gassign *> (gs);
//  return gimple_assign_rhs3 (ass);
//}
//
//
///* Return the LHS of call statement GS.  */
//
//inline tree
//gimple_call_lhs (const gcall *gs)
//{
//  return gs->op[0];
//}
//
//inline tree
//gimple_call_lhs (const gimple *gs)
//{
//  const gcall *gc = GIMPLE_CHECK2<const gcall *> (gs);
//  return gimple_call_lhs (gc);
//}
//
//
//
///* Return the function type of the function called by GS.  */
//
//inline tree
//gimple_call_fntype (const gcall *gs)
//{
//  if (gimple_call_internal_p (gs))
//    return NULL_TREE;
//  return gs->u.fntype;
//}
//
//inline tree
//gimple_call_fntype (const gimple *gs)
//{
//  const gcall *call_stmt = GIMPLE_CHECK2<const gcall *> (gs);
//  return gimple_call_fntype (call_stmt);
//}
//
//
///* Return the tree node representing the function called by call
//   statement GS.  */
//
//inline tree
//gimple_call_fn (const gcall *gs)
//{
//  return gs->op[1];
//}
//
//inline tree
//gimple_call_fn (const gimple *gs)
//{
//  const gcall *gc = GIMPLE_CHECK2<const gcall *> (gs);
//  return gimple_call_fn (gc);
//}
//
//
//
///* If a given GIMPLE_CALL's callee is a FUNCTION_DECL, return it.
//   Otherwise return NULL.  This function is analogous to
//   get_callee_fndecl in tree land.  */
//
//inline tree
//gimple_call_fndecl (const gcall *gs)
//{
//  return gimple_call_addr_fndecl (gimple_call_fn (gs));
//}
//
//inline tree
//gimple_call_fndecl (const gimple *gs)
//{
//  const gcall *gc = GIMPLE_CHECK2<const gcall *> (gs);
//  return gimple_call_fndecl (gc);
//}
//
//
///* Return the type returned by call statement GS.  */
//
//inline tree
//gimple_call_return_type (const gcall *gs)
//{
//  tree type = gimple_call_fntype (gs);
//
//  if (type == NULL_TREE)
//    return TREE_TYPE (gimple_call_lhs (gs));
//
//  /* The type returned by a function is the type of its
//     function type.  */
//  return TREE_TYPE (type);
//}
//
//
///* Return the static chain for call statement GS.  */
//
//inline tree
//gimple_call_chain (const gcall *gs)
//{
//  return gs->op[2];
//}
//
//inline tree
//gimple_call_chain (const gimple *gs)
//{
//  const gcall *gc = GIMPLE_CHECK2<const gcall *> (gs);
//  return gimple_call_chain (gc);
//}
//
//
///* Return the argument at position INDEX for call statement GS.  */
//
//inline tree
//gimple_call_arg (const gcall *gs, unsigned index)
//{
//  gcc_gimple_checking_assert (gimple_num_ops (gs) > index + 3);
//  return gs->op[index + 3];
//}
//
//inline tree
//gimple_call_arg (const gimple *gs, unsigned index)
//{
//  const gcall *gc = GIMPLE_CHECK2<const gcall *> (gs);
//  return gimple_call_arg (gc, index);
//}
//
//
//
///* Return the LHS of the predicate computed by conditional statement GS.  */
//
//inline tree
//gimple_cond_lhs (const gcond *gs)
//{
//  return gs->op[0];
//}
//
//inline tree
//gimple_cond_lhs (const gimple *gs)
//{
//  const gcond *gc = GIMPLE_CHECK2<const gcond *> (gs);
//  return gimple_cond_lhs (gc);
//}
//
//
///* Return the RHS operand of the predicate computed by conditional GS.  */
//
//inline tree
//gimple_cond_rhs (const gcond *gs)
//{
//  return gs->op[1];
//}
//
//inline tree
//gimple_cond_rhs (const gimple *gs)
//{
//  const gcond *gc = GIMPLE_CHECK2<const gcond *> (gs);
//  return gimple_cond_rhs (gc);
//}
//
//
///* Return the label used by conditional statement GS when its
//   predicate evaluates to true.  */
//
//inline tree
//gimple_cond_true_label (const gcond *gs)
//{
//  return gs->op[2];
//}
//
///* Return the label used by conditional statement GS when its
//   predicate evaluates to false.  */
//
//inline tree
//gimple_cond_false_label (const gcond *gs)
//{
//  return gs->op[3];
//}
//
///* Return the LABEL_DECL node used by GIMPLE_LABEL statement GS.  */
//
//inline tree
//gimple_label_label (const glabel *gs)
//{
//  return gs->op[0];
//}
//
//
///* Return the destination of the unconditional jump GS.  */
//
//inline tree
//gimple_goto_dest (const gimple *gs)
//{
//  GIMPLE_CHECK (gs, GIMPLE_GOTO);
//  return gimple_op (gs, 0);
//}
//
//
//
///* Return the variables declared in the GIMPLE_BIND statement GS.  */
//
//inline tree
//gimple_bind_vars (const gbind *bind_stmt)
//{
//  return bind_stmt->vars;
//}
//
///* Return the TREE_BLOCK node associated with GIMPLE_BIND statement
//   GS.  This is analogous to the BIND_EXPR_BLOCK field in trees.  */
//
//inline tree
//gimple_bind_block (const gbind *bind_stmt)
//{
//  return bind_stmt->block;
//}
//
///* Return input operand INDEX of GIMPLE_ASM ASM_STMT.  */
//
//inline tree
//gimple_asm_input_op (const gasm *asm_stmt, unsigned index)
//{
//  gcc_gimple_checking_assert (index < asm_stmt->ni);
//  return asm_stmt->op[index + asm_stmt->no];
//}
//
//
///* Return output operand INDEX of GIMPLE_ASM ASM_STMT.  */
//
//inline tree
//gimple_asm_output_op (const gasm *asm_stmt, unsigned index)
//{
//  gcc_gimple_checking_assert (index < asm_stmt->no);
//  return asm_stmt->op[index];
//}
//
//
///* Return clobber operand INDEX of GIMPLE_ASM ASM_STMT.  */
//
//inline tree
//gimple_asm_clobber_op (const gasm *asm_stmt, unsigned index)
//{
//  gcc_gimple_checking_assert (index < asm_stmt->nc);
//  return asm_stmt->op[index + asm_stmt->ni + asm_stmt->no];
//}
//
///* Return label operand INDEX of GIMPLE_ASM ASM_STMT.  */
//
//inline tree
//gimple_asm_label_op (const gasm *asm_stmt, unsigned index)
//{
//  gcc_gimple_checking_assert (index < asm_stmt->nl);
//  return asm_stmt->op[index + asm_stmt->no + asm_stmt->ni + asm_stmt->nc];
//}
//
//
///* Return the types handled by GIMPLE_CATCH statement CATCH_STMT.  */
//
//inline tree
//gimple_catch_types (const gcatch *catch_stmt)
//{
//  return catch_stmt->types;
//}
//
///* Return the types handled by GIMPLE_EH_FILTER statement GS.  */
//
//inline tree
//gimple_eh_filter_types (const gimple *gs)
//{
//  const geh_filter *eh_filter_stmt = as_a <const geh_filter *> (gs);
//  return eh_filter_stmt->types;
//}
//
//
//
///* Get the function decl to be called by the MUST_NOT_THROW region.  */
//
//inline tree
//gimple_eh_must_not_throw_fndecl (const geh_mnt *eh_mnt_stmt)
//{
//  return eh_mnt_stmt->fndecl;
//}
//
///* Return the SSA name created by GIMPLE_PHI GS.  */
//
//inline tree
//gimple_phi_result (const gphi *gs)
//{
//  return gs->result;
//}
//
//inline tree
//gimple_phi_result (const gimple *gs)
//{
//  const gphi *phi_stmt = as_a <const gphi *> (gs);
//  return gimple_phi_result (phi_stmt);
//}
//
///* Return the tree operand for argument I of PHI node GS.  */
//
//inline tree
//gimple_phi_arg_def (const gphi *gs, size_t index)
//{
//  return gimple_phi_arg (gs, index)->def;
//}
//
//inline tree
//gimple_phi_arg_def (const gimple *gs, size_t index)
//{
//  return gimple_phi_arg (gs, index)->def;
//}
//
///* Return the tree operand for the argument associated with
//   edge E of PHI node GS.  */
//
//inline tree
//gimple_phi_arg_def_from_edge (const gphi *gs, const_edge e)
//{
//  gcc_checking_assert (e->dest == gimple_bb (gs));
//  return gimple_phi_arg (gs, e->dest_idx)->def;
//}
//
//inline tree
//gimple_phi_arg_def_from_edge (const gimple *gs, const_edge e)
//{
//  gcc_checking_assert (e->dest == gimple_bb (gs));
//  return gimple_phi_arg (gs, e->dest_idx)->def;
//}
//
//
///* GS must be an assignment, a call, or a PHI.
//   If it's an assignment, return rhs operand I.
//   If it's a call, return function argument I.
//   If it's a PHI, return the value of PHI argument I.  */
//
//inline tree
//gimple_arg (const gimple *gs, unsigned int i)
//{
//  if (auto phi = dyn_cast<const gphi *> (gs))
//    return gimple_phi_arg_def (phi, i);
//  if (auto call = dyn_cast<const gcall *> (gs))
//    return gimple_call_arg (call, i);
//  return gimple_op (as_a <const gassign *> (gs), i + 1);
//}
//
///* Return the index variable used by the switch statement GS.  */
//
//inline tree
//gimple_switch_index (const gswitch *gs)
//{
//  return gs->op[0];
//}
//
///* Return the label numbered INDEX.  The default label is 0, followed by any
//   labels in a switch statement.  */
//
//inline tree
//gimple_switch_label (const gswitch *gs, unsigned index)
//{
//  gcc_gimple_checking_assert (gimple_num_ops (gs) > index + 1);
//  return gs->op[index + 1];
//}
//
///* Return the default label for a switch statement.  */
//
//inline tree
//gimple_switch_default_label (const gswitch *gs)
//{
//  tree label = gimple_switch_label (gs, 0);
//  gcc_checking_assert (!CASE_LOW (label) && !CASE_HIGH (label));
//  return label;
//}
//
//
//
///* Return the variable bound in a GIMPLE_DEBUG bind statement.  */
//
//inline tree
//gimple_debug_bind_get_var (const gimple *dbg)
//{
//  GIMPLE_CHECK (dbg, GIMPLE_DEBUG);
//  gcc_gimple_checking_assert (gimple_debug_bind_p (dbg));
//  return gimple_op (dbg, 0);
//}
//
///* Return the value bound to the variable in a GIMPLE_DEBUG bind
//   statement.  */
//
//inline tree
//gimple_debug_bind_get_value (const gimple *dbg)
//{
//  GIMPLE_CHECK (dbg, GIMPLE_DEBUG);
//  gcc_gimple_checking_assert (gimple_debug_bind_p (dbg));
//  return gimple_op (dbg, 1);
//}
//
//
///* Return the variable bound in a GIMPLE_DEBUG source bind statement.  */
//
//inline tree
//gimple_debug_source_bind_get_var (const gimple *dbg)
//{
//  GIMPLE_CHECK (dbg, GIMPLE_DEBUG);
//  gcc_gimple_checking_assert (gimple_debug_source_bind_p (dbg));
//  return gimple_op (dbg, 0);
//}
//
///* Return the value bound to the variable in a GIMPLE_DEBUG source bind
//   statement.  */
//
//inline tree
//gimple_debug_source_bind_get_value (const gimple *dbg)
//{
//  GIMPLE_CHECK (dbg, GIMPLE_DEBUG);
//  gcc_gimple_checking_assert (gimple_debug_source_bind_p (dbg));
//  return gimple_op (dbg, 1);
//}
//
//
///* Return the name associated with OMP_CRITICAL statement CRIT_STMT.  */
//
//inline tree
//gimple_omp_critical_name (const gomp_critical *crit_stmt)
//{
//  return crit_stmt->name;
//}
//
///* Return the guard associated with the GIMPLE_ASSUME statement GS.  */
//
//inline tree
//gimple_assume_guard (const gimple *gs)
//{
//  const gimple_statement_assume *assume_stmt
//    = as_a <const gimple_statement_assume *> (gs);
//  return assume_stmt->guard;
//}
//
///* Return the label associated with a GIMPLE_TRANSACTION.  */
//
//inline tree
//gimple_transaction_label_norm (const gtransaction *transaction_stmt)
//{
//  return transaction_stmt->label_norm;
//}
//
//
//inline tree
//gimple_transaction_label_uninst (const gtransaction *transaction_stmt)
//{
//  return transaction_stmt->label_uninst;
//}
//
//
//inline tree
//gimple_transaction_label_over (const gtransaction *transaction_stmt)
//{
//  return transaction_stmt->label_over;
//}
//
///* Return the return value for GIMPLE_RETURN GS.  */
//
//inline tree
//gimple_return_retval (const greturn *gs)
//{
//  return gs->op[0];
//}
// *
// */
