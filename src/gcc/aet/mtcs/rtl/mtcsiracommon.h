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

#ifndef __GCC_MTCS_IRA_COMMON__
#define __GCC_MTCS_IRA_COMMON__

#include "../../nlib.h"
#include "../mtcscomponent.h"


typedef struct _MtcsIraGlobal
{
   /* Dump file of the allocator if it is not NULL.  */
   //原型 ira_dump_file ira-int.h
    FILE *ira_dump_file;
   /* A modified value of flag `-fira-verbose' used internally.  */
    //原型 internal_flag_ira_verbose ira-int.h
    int internal_flag_ira_verbose;

    //原型 ira_obstack ira.cc
    struct obstack ira_obstack;

    /* Obstack used for storing all bitmaps of the IRA.  */
    //原型 ira_bitmap_obstack ira.cc
    struct bitmap_obstack ira_bitmap_obstack;


}MtcsIraGlobal;


typedef struct _MtcsIraMgr
{
    MtcsComponent parent;
    void *mtcsIra;
    void *mtcsIraInt;
    void *mtcsIraBuild;
    void *mtcsIraConflicts;
    void *mtcsIraLives;
    void *mtcsIraEmit;
    void *mtcsIraCosts;
    void *mtcsIraColor;
    MtcsIraGlobal *mtcsIraGlobal;
    nboolean init;
}MtcsIraMgr;

#define mtcs_ira_mgr_get_ira(mtcsIraMgr) (MtcsIra *)mtcsIraMgr->mtcsIra;
#define mtcs_ira_mgr_get_int(mtcsIraMgr) (MtcsIraInt *)mtcsIraMgr->mtcsIraInt;
#define mtcs_ira_mgr_get_build(mtcsIraMgr) (MtcsIraBuild *)mtcsIraMgr->mtcsIraBuild;
#define mtcs_ira_mgr_get_conflicts(mtcsIraMgr) (MtcsIraConflicts *)mtcsIraMgr->mtcsIraConflicts;
#define mtcs_ira_mgr_get_lives(mtcsIraMgr) (MtcsIraLives *)mtcsIraMgr->mtcsIraLives;
#define mtcs_ira_mgr_get_emit(mtcsIraMgr) (MtcsIraEmit *)mtcsIraMgr->mtcsIraEmit;
#define mtcs_ira_mgr_get_costs(mtcsIraMgr) (MtcsIraCosts *)mtcsIraMgr->mtcsIraCosts;
#define mtcs_ira_mgr_get_color(mtcsIraMgr) (MtcsIraColor *)mtcsIraMgr->mtcsIraColor;

MtcsIraMgr    *mtcs_ira_mgr_new(MtcsMode *mtcsMode);
MtcsIraGlobal *mtcs_ira_mgr_get_global(MtcsIraMgr *self);
//原型 ira_allocate ira-int.h ira.cc
void *mtcs_ira_allocate (size_t len);
//原型 ira_ira_free ira-int.h ira.cc
void mtcs_ira_free (void *addr ATTRIBUTE_UNUSED);
//原型 ira_allocate_bitmap ira-int.h ira.cc
bitmap mtcs_ira_allocate_bitmap ();
//原型 ira_free_bitmap ira-int.h ira.cc
void mtcs_ira_free_bitmap (bitmap b ATTRIBUTE_UNUSED);

#endif

