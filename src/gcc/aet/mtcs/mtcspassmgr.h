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

#ifndef __GCC_MTCS_PASS_MGR__
#define __GCC_MTCS_PASS_MGR__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcspass.h"

/* Range [start, last].  */

struct mtcs_uid_range
{
  unsigned int start;
  unsigned int last;
  const char *assem_name;
  struct mtcs_uid_range *next;
};

typedef struct _MtcsPassMgr MtcsPassMgr;
struct _MtcsPassMgr
{
  MtcsComponent parent;
  NPtrArray *regularIpaPassArray;
  NPtrArray *lateIpaPassArray;
  NPtrArray *allPassArray;

  vec<mtcs_uid_range *> enabled_pass_uid_range_tab;
  vec<mtcs_uid_range *> disabled_pass_uid_range_tab;
  struct profile_record *profile_record;
  MtcsPass *current_pass;
};

MtcsPassMgr    *mtcs_pass_mgr_new(MtcsMode *mtcsMode);
//原型  execute_one_pass tree-pass.h passes.cc
bool            mtcs_pass_mgr_execute_one_pass (MtcsPassMgr *self,MtcsPass *pass);
nboolean        mtcs_pass_mgr_add_regular_ipa_pass(MtcsPassMgr *self,MtcsPass *pass);
nboolean        mtcs_pass_mgr_add_late_ipa_pass(MtcsPassMgr *self,MtcsPass *pass);
nboolean        mtcs_pass_mgr_add_all_pass(MtcsPassMgr *self,MtcsPass *pass);
void            mtcs_pass_mgr_set_todo_flags_start(MtcsPassMgr *self,MtcsPass *newPass);
//原型 execute_ipa_summary_passes tree-pass.h passes.cc
void            mtcs_pass_mgr_execute_ipa_summary_passes (MtcsPassMgr *self);
MtcsPass       *mtcs_pass_mgr_get_current_pass(MtcsPassMgr *self);
MtcsPass       *mtcs_pass_mgr_get_pass(MtcsPassMgr *self,enum opt_pass_type type,char *name);

//原型  execute_all_ipa_transforms tree-pass.h passes.cc
void            mtcs_pass_mgr_execute_all_ipa_transforms (MtcsPassMgr *self,nboolean do_not_collect);
//原型   execute_pass_list (cfun, g->get_passes ()->all_passes);cgraphunit.cc
void            mtcs_pass_mgr_execute_all_pass (MtcsPassMgr *self,struct function *fn);
//原型 execute_ipa_pass_list tree-pass.h passes.cc
void            mtcs_pass_mgr_execute_regular_ipa (MtcsPassMgr *self);
//原型 execute_ipa_pass_list tree-pass.h passes.cc
void            mtcs_pass_mgr_execute_late_ipa (MtcsPassMgr *self);

#endif
