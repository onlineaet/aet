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

#ifndef __GCC_MTCS_SSA_PROPAGATE__
#define __GCC_MTCS_SSA_PROPAGATE__

#include "../nlib.h"
#include "mtcscomponent.h"


typedef struct _MtcsSsaPropagate  MtcsSsaPropagate;
struct _MtcsSsaPropagate
{
   MtcsComponent parent;
   bitmap cfg_blocks;
   int *bb_to_cfg_order;
   int *cfg_order_to_bb;

   /* Worklists of SSA edges which will need reexamination as their
   definition has changed.  SSA edges are def-use edges in the SSA
   web.  For each D-U edge, we store the target statement or PHI node
   UID in a bitmap.  UIDs order stmts in execution order.  We use
   two worklists to first make forward progress before iterating.  */
   bitmap ssa_edge_worklist;
   vec<gimple *> uid_to_stmt;

   /* Current RPO index in the iteration.  */
   int curr_order;
};

/* Lattice values used for propagation purposes.  Specific instances
   of a propagation engine must return these values from the statement
   and PHI visit functions to direct the engine.  */
enum mtcs_ssa_prop_result {
    /* The statement produces nothing of interest.  No edges will be
       added to the work lists.  */
    MTCS_SSA_PROP_NOT_INTERESTING,

    /* The statement produces an interesting value.  The set SSA_NAMEs
       returned by SSA_PROP_VISIT_STMT should be added to
       INTERESTING_SSA_EDGES.  If the statement being visited is a
       conditional jump, SSA_PROP_VISIT_STMT should indicate which edge
       out of the basic block should be marked executable.  */
    MTCS_SSA_PROP_INTERESTING,

    /* The statement produces a varying (i.e., useless) value and
       should not be simulated again.  If the statement being visited
       is a conditional jump, all the edges coming out of the block
       will be considered executable.  */
    MTCS_SSA_PROP_VARYING
};



/* Public interface into the SSA propagation engine.  Clients should inherit
   from this class and provide their own visitors.  */

class mtcs_ssa_propagation_engine
{
 public:

  virtual ~mtcs_ssa_propagation_engine (void) { }

  /* Virtual functions the clients must provide to visit statements
     and phi nodes respectively.  */
  virtual enum mtcs_ssa_prop_result visit_stmt (gimple *, edge *, tree *) = 0;
  virtual enum mtcs_ssa_prop_result visit_phi (gphi *) = 0;

  /* Main interface into the propagation engine.  */
  void ssa_propagate (void);
  MtcsSsaPropagate *self;
 private:
  /* Internal implementation details.  */
  void simulate_stmt (gimple *stmt);
  void simulate_block (basic_block);
};

class mtcs_substitute_and_fold_engine : public range_query
{
 public:
   mtcs_substitute_and_fold_engine (bool fold_all_stmts = false)
    : fold_all_stmts (fold_all_stmts) { }

  virtual tree value_of_expr (tree expr, gimple * = NULL) = 0;
  virtual tree value_on_edge (edge, tree expr) override;
  virtual tree value_of_stmt (gimple *, tree name = NULL) override;
  virtual bool range_of_expr (vrange &r, tree expr, gimple * = NULL);

  virtual ~mtcs_substitute_and_fold_engine (void) { }
  virtual bool fold_stmt (gimple_stmt_iterator *) { return false; }

  bool substitute_and_fold (basic_block = NULL);
  bool replace_uses_in (gimple *);
  bool replace_phi_args_in (gphi *);

  virtual void pre_fold_bb (basic_block) { }
  virtual void post_fold_bb (basic_block) { }
  virtual void pre_fold_stmt (gimple *) { }
  virtual void post_new_stmt (gimple *) { }

  bool propagate_into_phi_args (basic_block);

  /* Users like VRP can set this when they want to perform
     folding for every propagation.  */
  bool fold_all_stmts;
  MtcsSsaPropagate *self;

};



MtcsSsaPropagate *mtcs_ssa_propagate_new(MtcsMode *mtcsMode);
//原型 stmt_makes_single_store tree-ssa-propagate.h tree-ssa-propagate.cc
bool mtcs_ssa_propagate_stmt_makes_single_store (MtcsSsaPropagate *self,gimple *stmt);
//原型 may_propagate_copy tree-ssa-propagate.h tree-ssa-propagate.cc
bool mtcs_ssa_propagate_may_propagate_copy (MtcsSsaPropagate *self,tree dest, tree orig, bool dest_not_abnormal_phi_edge_p);
//原型 may_propagate_copy_into_stmt tree-ssa-propagate.h tree-ssa-propagate.cc
bool mtcs_ssa_propagate_may_propagate_copy_into_stmt (MtcsSsaPropagate *self,gimple *dest, tree orig);
//原型 may_propagate_copy_into_asm tree-ssa-propagate.h tree-ssa-propagate.cc
bool mtcs_ssa_propagate_may_propagate_copy_into_asm (MtcsSsaPropagate *self,tree dest ATTRIBUTE_UNUSED);
//原型 replace_exp tree-ssa-propagate.h tree-ssa-propagate.cc
void mtcs_ssa_propagate_replace_exp (MtcsSsaPropagate *self,use_operand_p op_p, tree val);
//原型 propagate_value tree-ssa-propagate.h tree-ssa-propagate.cc
void mtcs_ssa_propagate_propagate_value (MtcsSsaPropagate *self,use_operand_p op_p, tree val);
//原型 propagate_tree_value tree-ssa-propagate.h tree-ssa-propagate.cc
void mtcs_ssa_propagate_propagate_tree_value (MtcsSsaPropagate *self,tree *op_p, tree val);
//原型 propagate_tree_value_into_stmt tree-ssa-propagate.h tree-ssa-propagate.cc
void mtcs_ssa_propagate_propagate_tree_value_into_stmt (MtcsSsaPropagate *self,gimple_stmt_iterator *gsi, tree val);
//原型 clean_up_loop_closed_phi tree-ssa-propagate.h tree-ssa-propagate.cc
unsigned mtcs_ssa_propagate_clean_up_loop_closed_phi (MtcsSsaPropagate *self,function *fun);


#endif
