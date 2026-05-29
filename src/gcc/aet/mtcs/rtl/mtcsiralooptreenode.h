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

#ifndef __GCC_MTCS_IRA_LOOP_TREE_NODE__
#define __GCC_MTCS_IRA_LOOP_TREE_NODE__

#include "../../nlib.h"
#include "../mtcscomponent.h"

//原型 typedef struct ira_loop_tree_node
typedef struct _MtcsIraLoopTreeNode  MtcsIraLoopTreeNode;

/* A structure representing conflict information for an allocno
   (or one of its subwords).  */
//原型
struct _MtcsIraLoopTreeNode
{
  /* The node represents basic block if children == NULL.  */
    basic_block bb;    /* NULL for loop.  */
    /* NULL for BB or for loop tree root if we did not build CFG loop tree.  */
    class loop *loop;
    /* NEXT/SUBLOOP_NEXT is the next node/loop-node of the same parent.
       SUBLOOP_NEXT is always NULL for BBs.  */
    MtcsIraLoopTreeNode *subloop_next, *next;
    /* CHILDREN/SUBLOOPS is the first node/loop-node immediately inside
       the node.  They are NULL for BBs.  */
    MtcsIraLoopTreeNode *subloops, *children;
    /* The node immediately containing given node.  */
    MtcsIraLoopTreeNode *parent;

    /* Loop level in range [0, ira_loop_tree_height).  */
    int level;

    /* All the following members are defined only for nodes representing
       loops.  */

    /* The loop number from CFG loop tree.  The root number is 0.  */
    int loop_num;

    /* True if the loop was marked for removal from the register
       allocation.  */
    bool to_remove_p;

    /* Allocnos in the loop corresponding to their regnos.  If it is
       NULL the loop does not form a separate register allocation region
       (e.g. because it has abnormal enter/exit edges and we cannot put
       code for register shuffling on the edges if a different
       allocation is used for a pseudo-register on different sides of
       the edges).  Caps are not in the map (remember we can have more
       one cap with the same regno in a region).  */
    void /*!MtcsIraAllocno*/ **regno_allocno_map; //避免与MtcsIraAllocno 相互引用

    /* True if there is an entry to given loop not from its parent (or
       grandparent) basic block.  For example, it is possible for two
       adjacent loops inside another loop.  */
    bool entered_from_non_parent_p;

    /* Maximal register pressure inside loop for given register class
       (defined only for the pressure classes).  */
    int reg_pressure[MAX_N_REG_CLASSES/*!N_REG_CLASSES*/];

    /* Numbers of allocnos referred or living in the loop node (except
       for its subloops).  */
    bitmap all_allocnos;

    /* Numbers of allocnos living at the loop borders.  */
    bitmap border_allocnos;

    /* Regnos of pseudos modified in the loop node (including its
       subloops).  */
    bitmap modified_regnos;

    /* Numbers of copies referred in the corresponding loop.  */
    bitmap local_copies;
};

MtcsIraLoopTreeNode *mtcs_ira_loop_tree_node_new(MtcsMode *mtcsMode);
//原型 static bool loop_is_inside_p (MtcsIraLoopTreeNode * node, MtcsIraLoopTreeNode * parent) ira-build.cc
bool mtcs_ira_loop_tree_node_loop_is_inside_p (MtcsIraLoopTreeNode * self, MtcsIraLoopTreeNode * parent);
//原型 finish_loop_tree_node ira-build.cc
void mtcs_ira_loop_tree_node_free(MtcsIraLoopTreeNode *self);

#endif
