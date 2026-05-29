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
 * base on function.cc
 */



/* This file handles generation of all the assembler code
   *except* the instructions of a function.
   This includes declarations of variables and their initial values.

   We also output the assembler code for constants stored in memory
   and are responsible for combining constants with the same value.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "predict.h"
#include "memmodel.h"
#include "tm_p.h"
#include "stringpool.h"
#include "regs.h"
#include "cgraph.h"
#include "diagnostic-core.h"
#include "fold-const.h"
#include "stor-layout.h"
#include "varasm.h"
#include "version.h"
#include "flags.h"
#include "stmt.h"
#include "expr.h"
#include "expmed.h"
#include "optabs.h"
#include "output.h"
#include "langhooks.h"
#include "debug.h"
#include "common/common-target.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "rtl-iter.h"
#include "file-prefix-map.h" /* remap_debug_filename()  */
#include "alloc-pool.h"
#include "toplev.h"
#include "opts.h"
#include "asan.h"
#include "recog.h"
#include "dwarf2out.h"
#include "dwarf2.h"
#include "dwarf2asm.h"
#include "except.h"
#include "emit-rtl.h"
#include "function-abi.h"
#include "alias.h"
#include "tree-ssa.h"
#include "tree-dfa.h"
#include "gimple-expr.h"
#include "rtl-error.h"
#include "shrink-wrap.h"

#include "mtcsfunc.h"
#include "mtcstarget.h"
#include "mtcscompile.h"
#include "mtcsprintrtl.h"
#include "mtcsdfcore.h"
#include "mtcsdfproblems.h"

#include "../aetprinttree.h"


#ifndef STACK_ALIGNMENT_NEEDED
#define STACK_ALIGNMENT_NEEDED 1
#endif


#define STACK_BYTES (mtcs_func_get_stack_boundary(self) / BITS_PER_UNIT)

#define CEIL_ROUND(VALUE,ALIGN) (((VALUE) + (ALIGN) - 1) & ~((ALIGN)- 1))


/* Structures to communicate between the subroutines of assign_parms.
   The first holds data persistent across all parameters, the second
   is cleared out for each parameter.  */

struct assign_parm_data_all
{
  /* When INIT_CUMULATIVE_ARGS gets revamped, allocating CUMULATIVE_ARGS
     should become a job of the target or otherwise encapsulated.  */
  MtcsCumulativeArgs/*!CUMULATIVE_ARGS*/ *args_so_far_v;
  cumulative_args_t args_so_far;
  struct args_size stack_args_size;
  tree function_result_decl;
  tree orig_fnargs;
  rtx_insn *first_conversion_insn;
  rtx_insn *last_conversion_insn;
  HOST_WIDE_INT pretend_args_size;
  HOST_WIDE_INT extra_pretend_bytes;
  int reg_parm_stack_space;
};

struct assign_parm_data_one
{
  tree nominal_type;
  mtcs_function_arg_info/*!function_arg_info*/ arg;
  rtx entry_parm;
  rtx stack_parm;
  machine_mode nominal_mode;
  machine_mode passed_mode;
  struct locate_and_pad_arg_data locate;
  int partial;
};

/* Private type used by get_hard_reg_initial_reg, get_hard_reg_initial_val,
   and has_hard_reg_initial_val..  */
struct GTY(()) initial_value_pair {
  rtx hard_reg;
  rtx pseudo;
};

/* ???  This could be a VEC but there is currently no way to define an
   opaque VEC type.  This could be worked around by defining struct
   initial_value_pair in function.h.  */
//原型 initial_value_struct emit-rtl.h function.cc
struct GTY(()) initial_value_struct {
  int num_entries;
  int max_entries;
  initial_value_pair * GTY ((length ("%h.num_entries"))) entries;
};



/* These hashes record the prologue and epilogue insns.  */

struct mtcs_insn_cache_hasher : ggc_cache_ptr_hash<rtx_def>
{
  static hashval_t hash (rtx x) { return htab_hash_pointer (x); }
  static bool equal (rtx a, rtx b) { return a == b; }
};

//原型 invoke_set_current_function_hook funciton.cc
static void invoke_set_current_function_hook (MtcsFunc *self,tree fndecl);
//原型 prepare_function_start function.cc
static void prepare_function_start (MtcsFunc *self);
static void do_clobber_return_reg (rtx reg, void *arg ATTRIBUTE_UNUSED);
static void do_use_return_reg (rtx, void *);

/* In order to evaluate some expressions, such as function calls returning
   structures in memory, we need to temporarily allocate stack locations.
   We record each allocated temporary in the following structure.

   Associated with each temporary slot is a nesting level.  When we pop up
   one level, all temporaries associated with the previous level are freed.
   Normally, all temporaries are freed after the execution of the statement
   in which they were created.  However, if we are inside a ({...}) grouping,
   the result may be in a temporary and hence must be preserved.  If the
   result could be in a temporary, we preserve it if we can determine which
   one it is in.  If we cannot determine which temporary may contain the
   result, all temporaries are preserved.  A temporary is preserved by
   pretending it was allocated at the previous nesting level.  */

class GTY(()) mtcs_temp_slot {
public:
  /* Points to next temporary slot.  */
  class mtcs_temp_slot *next;
  /* Points to previous temporary slot.  */
  class mtcs_temp_slot *prev;
  /* The rtx to used to reference the slot.  */
  rtx slot;
  /* The size, in units, of the slot.  */
  poly_int64 size;
  /* The type of the object in the slot, or zero if it doesn't correspond
     to a type.  We use this to determine whether a slot can be reused.
     It can be reused if objects of the type of the new slot will always
     conflict with objects of the type of the old slot.  */
  tree type;
  /* The alignment (in bits) of the slot.  */
  unsigned int align;
  /* True if this temporary is currently in use.  */
  bool in_use;
  /* Nesting level at which this slot is being used.  */
  int level;
  /* The offset of the slot from the frame_pointer, including extra space
     for alignment.  This info is for combine_temp_slots.  */
  poly_int64 base_offset;
  /* The size of the slot, including extra space for alignment.  This
     info is for combine_temp_slots.  */
  poly_int64 full_size;
};

/* Entry for the below hash table.  */
struct GTY((for_user)) temp_slot_address_entry {
  hashval_t hash;
  rtx address;
  class mtcs_temp_slot *temp_slot;
  MtcsFunc *self;
};

struct mtcs_temp_address_hasher : ggc_ptr_hash<temp_slot_address_entry>
{
  static hashval_t hash (temp_slot_address_entry *);
  static bool equal (temp_slot_address_entry *, temp_slot_address_entry *);
};

//原型 get_stack_local_alignment function.cc
static unsigned int get_stack_local_alignment (MtcsFunc *self,tree type, machine_mode mode);
//原型 #define ARG_POINTER_CFA_OFFSET(FNDECL)  (FIRST_PARM_OFFSET (FNDECL) + crtl->args.pretend_args_size) defaults.h
static int getArgPointerCfgOffset_cb(MtcsFunc *self,tree fndecl);

static void mtcsFuncInit(MtcsFunc *self)
{
   self->mtcsRtlData=mtcs_rtl_data_new();
   self->in_dummy_function=FALSE;
   self->funcArray=n_ptr_array_new();
   //原型 #define RETURN_ADDR_OFFSET 0
   self->returnAddrOffset=0;
   //原型 #define ARG_POINTER_CFA_OFFSET(FNDECL)  (FIRST_PARM_OFFSET (FNDECL) + crtl->args.pretend_args_size) defaults.h
   self->get_arg_pointer_cfg_offset=getArgPointerCfgOffset_cb;
}

/* Return the hash value for an address -> temp slot mapping.  */
hashval_t mtcs_temp_address_hasher::hash (temp_slot_address_entry *t)
{
  return t->hash;
}

/* Compare two address -> temp slot mapping entries.  */
bool mtcs_temp_address_hasher::equal (temp_slot_address_entry *t1, temp_slot_address_entry *t2)
{
  return exp_equiv_p (t1->address, t2->address, 0, true);
}

/* Removes temporary slot TEMP from LIST.  */
//原型 cut_slot_from_list function.cc
static void cut_slot_from_list(class mtcs_temp_slot *temp, class mtcs_temp_slot **list)
{
  if (temp->next)
    temp->next->prev = temp->prev;
  if (temp->prev)
    temp->prev->next = temp->next;
  else
    *list = temp->next;
  temp->prev = temp->next = NULL;
}

/* Determine whether it is possible to fit a stack slot of size SIZE and
   alignment ALIGNMENT into an area in the stack frame that starts at
   frame offset START and has a length of LENGTH.  If so, store the frame
   offset to be used for the stack slot in *POFFSET and return true;
   return false otherwise.  This function will extend the frame size when
   given a start/length pair that lies at the end of the frame.  */
//原型 try_fit_stack_local function.cc
static bool try_fit_stack_local(MtcsFunc *self,poly_int64 start, poly_int64 length,
             poly_int64 size, unsigned int alignment, poly_int64 *poffset)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;

  poly_int64 this_frame_offset;
  int frame_off, frame_alignment, frame_phase;
  /* Calculate how many bytes the start of local variables is off from
     stack alignment.  */
  frame_alignment = mtcs_func_get_preferred_stack_boundary/*!PREFERRED_STACK_BOUNDARY*/(self)/BITS_PER_UNIT;
  frame_off = mtcsTarget->starting_frame_offset/*!targetm.starting_frame_offset*/(mtcsTarget) % frame_alignment;
  frame_phase = frame_off ? frame_alignment - frame_off : 0;

  /* Round the frame offset to the specified alignment.  */

  if (mtcs_func_get_frame_grows_downward/*!FRAME_GROWS_DOWNWARD*/(self))
    this_frame_offset= (aligned_lower_bound (start + length - size - frame_phase, alignment)+ frame_phase);
  else
    this_frame_offset = aligned_upper_bound (start - frame_phase, alignment) + frame_phase;

  /* See if it fits.  If this space is at the edge of the frame,
     consider extending the frame to make it fit.  Our caller relies on
     this when allocating a new slot.  */
  if (maybe_lt (this_frame_offset, start)){
      if (known_eq (mtcsRtlData/*!frame_offset*/->x_frame_offset, start))
         mtcsRtlData/*!frame_offset*/->x_frame_offset = this_frame_offset;
      else
          return false;
  }else if (maybe_gt (this_frame_offset + size, start + length)){
      if (known_eq (mtcsRtlData/*!frame_offset*/->x_frame_offset, start + length))
         mtcsRtlData/*!frame_offset*/->x_frame_offset = this_frame_offset + size;
      else
          return false;
  }

  *poffset = this_frame_offset;
  return true;
}

/* Create a new frame_space structure describing free space in the stack
   frame beginning at START and ending at END, and chain it into the
   function's frame_space_list.  */
//原型 add_frame_space function.cc
static void add_frame_space (MtcsFunc *self,poly_int64 start, poly_int64 end)
{
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(self);

   class frame_space *space = ggc_alloc<frame_space> ();
   space->next = mtcsRtlData/*!crtl*/->frame_space_list;
   mtcsRtlData/*!crtl*/->frame_space_list = space;
   space->start = start;
   space->length = end - start;
}

/* Inserts temporary slot TEMP to LIST.  */
//原型 insert_slot_to_list function.cc
static void insert_slot_to_list (class mtcs_temp_slot *temp, class mtcs_temp_slot **list)
{
  temp->next = *list;
  if (*list)
    (*list)->prev = temp;
  temp->prev = NULL;
  *list = temp;
}

/* Return stack slot alignment in bits for TYPE and MODE.  */
//原型 get_stack_local_alignment function.cc
static unsigned int get_stack_local_alignment(MtcsFunc *self,tree type, machine_mode mode)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsAlign   *mtcsAlign=mtcs_target_get_align(mtcsTarget);

  unsigned int alignment;
  n_debug("mtcsfunc.c get_stack_local_alignment -- %p %d\n",type,mode);
  if (mode == mtcsMode->modes.M_BLKmode)
    alignment = mtcs_align_get_biggest_alignment/*!BIGGEST_ALIGNMENT*/(mtcsAlign);
  else
    alignment =mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode);
  n_debug("mtcsfunc.c get_stack_local_alignment -- 11%p %d\n",type,mode);

  /* Allow the frond-end to (possibly) increase the alignment of this
     stack slot.  */
  if (! type)
    type = lang_hooks.types.type_for_mode (mode, 0);
  n_debug("mtcsfunc.c get_stack_local_alignment --22 %p %d\n",type,mode);

  return mtcs_align_get_stack_slot_alignment/*!STACK_SLOT_ALIGNMENT*/(mtcsAlign,type, mode, alignment);
}

/* Returns the maximal temporary slot level.  */
static int max_slot_level (MtcsFunc *self)
{
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  if (!mtcsRtlData->x_used_temp_slots/*!used_temp_slots*/)
    return -1;
  return mtcsRtlData->x_used_temp_slots/*!used_temp_slots*/->length () - 1;
}

/* Returns the list of used temp slots at LEVEL.  */
static class mtcs_temp_slot **temp_slots_at_level (MtcsFunc *self,int level)
{
   MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
   if (level >= (int) vec_safe_length (mtcsRtlData->x_used_temp_slots/*!used_temp_slots*/))
      vec_safe_grow_cleared (mtcsRtlData->x_used_temp_slots/*!used_temp_slots*/, level + 1, true);

   return &(*mtcsRtlData->x_used_temp_slots/*!used_temp_slots*/)[level];
}

/* Compute the hash value for an address -> temp slot mapping.
   The value is cached on the mapping entry.  */
static hashval_t temp_slot_address_compute_hash (struct temp_slot_address_entry *t)
{
  int do_not_record = 0;
  return hash_rtx (t->address, GET_MODE (t->address),&do_not_record, NULL, false);
}

void mtcs_func_set_frame_grows_downward(MtcsFunc *self,int upOrDown)
{
    self->frameGrowsDownward=upOrDown;
}

//原型 FRAME_GROWS_DOWNWARD
int  mtcs_func_get_frame_grows_downward(MtcsFunc *self)
{
    return self->frameGrowsDownward;
}

//原型 STACK_GROWS_DOWNWARD
void mtcs_func_set_stack_grows_downward(MtcsFunc *self,int upOrDown)
{
    self->stackGrowsDownward=upOrDown;
}

int mtcs_func_get_stack_grows_downward(MtcsFunc *self)
{
    return self->stackGrowsDownward;
}

//原型 ARGS_GROW_DOWNWARD default.h gcn.h
void         mtcs_func_set_args_grows_downward(MtcsFunc *self,int upOrDown)
{
    self->argsGrowsDownward=upOrDown;
}

int          mtcs_func_get_args_grows_downward(MtcsFunc *self)
{
    return self->argsGrowsDownward;
}

//原型 STACK_BOUNDARY defaults.h
void         mtcs_func_set_stack_boundary(MtcsFunc *self,int stackBoundary)
{
    self->stackBoundary=stackBoundary;
    mtcs_rtl_data_set_stack_boundary(self->mtcsRtlData,stackBoundary);
}

int  mtcs_func_get_stack_boundary(MtcsFunc *self)
{
    return self->stackBoundary;
}

//原型 #define PREFERRED_STACK_BOUNDARY STACK_BOUNDARY
void         mtcs_func_set_preferred_stack_boundary(MtcsFunc *self,int stackBoundary)
{
   self->preferredStackBoundary=stackBoundary;

}

int          mtcs_func_get_preferred_stack_boundary(MtcsFunc *self)
{
   return self->preferredStackBoundary;
}
//原型 #define INCOMING_STACK_BOUNDARY PREFERRED_STACK_BOUNDARY
void         mtcs_func_set_incoming_stack_boundary(MtcsFunc *self,int stackBoundary)
{
   self->incomingStackBoundary=stackBoundary;
}

int          mtcs_func_get_incoming_stack_boundary(MtcsFunc *self)
{
   return self->incomingStackBoundary;
}

//原型 STACK_POINTER_OFFSET #define STACK_POINTER_OFFSET    0 defaults.h
void  mtcs_func_set_stack_pointer_offset(MtcsFunc *self,int stackPointerOffset)
{
   self->stackPointerOffset=stackPointerOffset;
}

int  mtcs_func_get_stack_pointer_offset(MtcsFunc *self)
{
   return self->stackPointerOffset;
}

MtcsRtlData *mtcs_func_get_rtl_data(MtcsFunc *self)
{
    return self->mtcsRtlData;
}

//原型 get_frame_size function.h function.cc
poly_int64  mtcs_func_get_frame_size(MtcsFunc *self)
{
    if (self->frameGrowsDownward)
      return -self->mtcsRtlData->x_frame_offset;
    else
      return self->mtcsRtlData->x_frame_offset;
}

/* Put the various virtual registers into REGNO_REG_RTX.  */
//原型 static void init_virtual_regs emit-rtl.cc
static void init_virtual_regs (MtcsFunc *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  mtcsRtlData->regno_reg_rtx[mtcsReg->normalHardRegsNum.virtual_incoming_args_regnum/*!VIRTUAL_INCOMING_ARGS_REGNUM*/] =
          mtcs_rtl_get_virtual_incoming_args_rtx/*!virtual_incoming_args_rtx*/(mtcsRTL);

  mtcsRtlData->regno_reg_rtx[mtcsReg->normalHardRegsNum.virtual_stack_vars_regnum/*!VIRTUAL_STACK_VARS_REGNUM*/] =
          mtcs_rtl_get_virtual_stack_args_rtx/*!virtual_stack_vars_rtx*/(mtcsRTL);

  mtcsRtlData->regno_reg_rtx[mtcsReg->normalHardRegsNum.virtual_stack_dynamic_regnum/*!VIRTUAL_STACK_DYNAMIC_REGNUM*/] =
          mtcs_rtl_get_virtual_stack_dynamic_rtx/*!virtual_stack_dynamic_rtx*/(mtcsRTL);

  mtcsRtlData->regno_reg_rtx[mtcsReg->normalHardRegsNum.virtual_outgoing_args_regnum/*!VIRTUAL_OUTGOING_ARGS_REGNUM*/] =
          mtcs_rtl_get_virtual_outgoing_args_rtx/*!virtual_outgoing_args_rtx*/(mtcsRTL);

  mtcsRtlData->regno_reg_rtx[mtcsReg->normalHardRegsNum.virtual_cfa_regnum/*!VIRTUAL_CFA_REGNUM*/] =
          mtcs_rtl_get_virtual_cfa_rtx/*!virtual_cfa_rtx*/(mtcsRTL);

  mtcsRtlData->regno_reg_rtx[mtcsReg->normalHardRegsNum.virtual_preferred_stack_boundary_regnum/*!VIRTUAL_PREFERRED_STACK_BOUNDARY_REGNUM*/] =
          mtcs_rtl_get_virtual_preferred_stack_boundary_rtx/*!virtual_preferred_stack_boundary_rtx*/(mtcsRTL);

}

/* Initialize data structures and variables in this file
   before generating rtl for each function.  */
//原型 init_emit rtl.h emit-rtl.cc
void mtcs_func_init_emit (MtcsFunc *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  int lastVirtualRegno= mtcs_reg_get_last_virtual_regno(mtcsReg);/*!LAST_VIRTUAL_REGISTER*/

  mtcs_rtl_data_set_first_insn/*!set_first_insn*/(mtcsRtlData,NULL);
  mtcs_rtl_data_set_last_insn/*!set_last_insn*/(mtcsRtlData,NULL);
  if (mtcsOptionsItem->x_param_min_nondebug_insn_uid)
      mtcsRtlData->emit.x_cur_insn_uid/*!cur_insn_uid*/ = mtcsOptionsItem->x_param_min_nondebug_insn_uid;
  else
      mtcsRtlData->emit.x_cur_insn_uid/*!cur_insn_uid*/ = 1;
  mtcsRtlData->emit.x_cur_debug_insn_uid/*!cur_debug_insn_uid*/ = 1;
  mtcsRtlData->emit.x_reg_rtx_no/*!reg_rtx_no*/ = lastVirtualRegno/*!LAST_VIRTUAL_REGISTER*/ + 1;
  mtcsRtlData->emit.x_first_label_num/*!first_label_num*/ = mtcs_rtl_get_label_num/*!label_num*/(mtcsRTL);
  mtcs_rtl_data_get_current_sequence/*!get_current_sequence*/(mtcsRtlData)->next = NULL;

  /* Init the tables that describe all the pseudo regs.  */
  mtcsRtlData->emit.regno_pointer_align_length = lastVirtualRegno/*!LAST_VIRTUAL_REGISTER*/ + 101;
  mtcsRtlData->emit.regno_pointer_align  = XCNEWVEC (unsigned char, mtcsRtlData->emit.regno_pointer_align_length);

  mtcsRtlData->regno_reg_rtx/*!regno_reg_rtx*/ = ggc_cleared_vec_alloc<rtx> (mtcsRtlData->emit.regno_pointer_align_length);

  /* Put copies of all the hard registers into regno_reg_rtx.  */
  memcpy (mtcsRtlData->regno_reg_rtx/*!regno_reg_rtx*/,
          mtcsRTL->x_initial_regno_reg_rtx/*!initial_regno_reg_rtx*/,
          mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg) * sizeof (rtx));

  /* Put copies of all the virtual register rtx into regno_reg_rtx.  */
  init_virtual_regs(self);

  /* Indicate that the virtual registers and stack locations are
     all pointers.  */
  REG_POINTER (mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL)) = 1;
  REG_POINTER (mtcs_rtl_get_frame_pointer_rtx/*!frame_pointer_rtx*/(mtcsRTL)) = 1;
  REG_POINTER (mtcs_rtl_get_arg_pointer_rtx/*!arg_pointer_rtx*/(mtcsRTL)) = 1;

  REG_POINTER (mtcs_rtl_get_virtual_incoming_args_rtx/*!virtual_incoming_args_rtx*/(mtcsRTL)) = 1;
  REG_POINTER (mtcs_rtl_get_virtual_stack_args_rtx/*!virtual_stack_vars_rtx*/(mtcsRTL)) = 1;
  REG_POINTER (mtcs_rtl_get_virtual_stack_dynamic_rtx/*!virtual_stack_dynamic_rtx*/(mtcsRTL)) = 1;
  REG_POINTER (mtcs_rtl_get_virtual_outgoing_args_rtx/*!virtual_outgoing_args_rtx*/(mtcsRTL)) = 1;
  REG_POINTER (mtcs_rtl_get_virtual_cfa_rtx/*!mtcs_rtl_get_virtual_cfa_rtx*/(mtcsRTL)) = 1;


  int stackBoundary=self-> stackBoundary;//原型 STACK_BOUNDARY
//#ifdef STACK_BOUNDARY //host=1 nvptx=1
  //#define REGNO_POINTER_ALIGN(REGNO) (crtl->emit.regno_pointer_align[REGNO]) 原型 function.h
  mtcsRtlData->emit.regno_pointer_align[mtcsReg->normalHardRegsNum.stack_pointer_regnum]=stackBoundary;
  mtcsRtlData->emit.regno_pointer_align[mtcsReg->normalHardRegsNum.frame_pointer_regnum]=stackBoundary;
  mtcsRtlData->emit.regno_pointer_align[mtcsReg->normalHardRegsNum.hard_frame_pointer_regnum]=stackBoundary;
  mtcsRtlData->emit.regno_pointer_align[mtcsReg->normalHardRegsNum.arg_pointer_regnum]=stackBoundary;
  mtcsRtlData->emit.regno_pointer_align[mtcsReg->normalHardRegsNum.virtual_incoming_args_regnum]=stackBoundary;
  mtcsRtlData->emit.regno_pointer_align[mtcsReg->normalHardRegsNum.virtual_stack_vars_regnum]=stackBoundary;
  mtcsRtlData->emit.regno_pointer_align[mtcsReg->normalHardRegsNum.virtual_stack_dynamic_regnum]=stackBoundary;
  mtcsRtlData->emit.regno_pointer_align[mtcsReg->normalHardRegsNum.virtual_outgoing_args_regnum]=stackBoundary;
  mtcsRtlData->emit.regno_pointer_align[mtcsReg->normalHardRegsNum.virtual_cfa_regnum]=BITS_PER_WORD;
  /*!
  REGNO_POINTER_ALIGN (STACK_POINTER_REGNUM) = STACK_BOUNDARY;
  REGNO_POINTER_ALIGN (FRAME_POINTER_REGNUM) = STACK_BOUNDARY;
  REGNO_POINTER_ALIGN (HARD_FRAME_POINTER_REGNUM) = STACK_BOUNDARY;
  REGNO_POINTER_ALIGN (ARG_POINTER_REGNUM) = STACK_BOUNDARY;
  REGNO_POINTER_ALIGN (VIRTUAL_INCOMING_ARGS_REGNUM) = STACK_BOUNDARY;
  REGNO_POINTER_ALIGN (VIRTUAL_STACK_VARS_REGNUM) = STACK_BOUNDARY;
  REGNO_POINTER_ALIGN (VIRTUAL_STACK_DYNAMIC_REGNUM) = STACK_BOUNDARY;
  REGNO_POINTER_ALIGN (VIRTUAL_OUTGOING_ARGS_REGNUM) = STACK_BOUNDARY;
  REGNO_POINTER_ALIGN (VIRTUAL_CFA_REGNUM) = BITS_PER_WORD;
  */
//#endif

#ifdef INIT_EXPANDERS //host=0 nvptx=0
  INIT_EXPANDERS;
#endif
}

//原型 rtl_profile_for_bb predict.h predict.cc
void mtcs_func_rtl_profile_for_bb (MtcsFunc *self,basic_block bb)
{
  self->mtcsRtlData->maybe_hot_insn_p = maybe_hot_bb_p (cfun, bb);
}
/* Set RTL expansion to default mode (i.e. when profile info is not known).  */
//原型 default_rtl_profile predict.h predict.cc
void mtcs_func_default_rtl_profile (MtcsFunc *self)
{
  self->mtcsRtlData->maybe_hot_insn_p = true;  /*!crtl->maybe_hot_insn_p = true;*/
}

/**
 * 原型 #define SUPPORTS_STACK_ALIGNMENT (MAX_STACK_ALIGNMENT > STACK_BOUNDARY)
 */
nboolean mtcs_func_is_support_stack_alignment(MtcsFunc *self)
{
    return self->is_support_stack_alignment(self);
}

nuint  mtcs_func_get_max_support_stack_alignment(MtcsFunc *self)
{
    return self->get_max_support_stack_alignment(self);
}


/* Find the temp slot corresponding to the object at address X.  */
//原型 function.cc
static class mtcs_temp_slot *find_temp_slot_from_address (MtcsFunc *self,rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   class mtcs_temp_slot *p;
   struct temp_slot_address_entry tmp, *t;

   /* First try the easy way:
   See if X exists in the address -> temp slot mapping.  */
   tmp.address = x;
   tmp.temp_slot = NULL;
   tmp.hash = temp_slot_address_compute_hash (&tmp);
   t = self->temp_slot_address_table->find_with_hash (&tmp, tmp.hash);
   if (t)
      return t->temp_slot;

   /* If we have a sum involving a register, see if it points to a temp
   slot.  */
   if (GET_CODE (x) == PLUS && REG_P (XEXP (x, 0)) && (p = find_temp_slot_from_address (self,XEXP (x, 0))) != 0)
      return p;
   else if (GET_CODE (x) == PLUS && REG_P (XEXP (x, 1))  && (p = find_temp_slot_from_address (self,XEXP (x, 1))) != 0)
      return p;

   /* Last resort: Address is a virtual stack var address.  */
   poly_int64 offset;
   if (strip_offset (x, &offset) == mtcs_rtl_get_virtaul_stack_var_rtx/*!virtual_stack_vars_rtx*/(mtcsRTL)){
      int i;
      for (i = max_slot_level (self); i >= 0; i--)
         for (p = *temp_slots_at_level (self,i); p; p = p->next)
            if (known_in_range_p (offset, p->base_offset, p->full_size))
               return p;
   }

   return NULL;
}

/* Add ADDRESS as an alias of TEMP_SLOT to the addess -> temp slot mapping.  */
//原型 insert_temp_slot_address function.cc
static void insert_temp_slot_address (MtcsFunc *self,rtx address, class mtcs_temp_slot *temp_slot)
{
  struct temp_slot_address_entry *t = ggc_alloc<temp_slot_address_entry> ();
  t->address = copy_rtx (address);
  t->temp_slot = temp_slot;
  t->hash = temp_slot_address_compute_hash (t);
  t->self=self;
  *self->temp_slot_address_table->find_slot_with_hash (t, t->hash, INSERT) = t;
}

/* Indicate that NEW_RTX is an alternate way of referring to the temp
   slot that previously was known by OLD_RTX.  */
//原型 update_temp_slot_address function.h function.cc
void mtcs_func_update_temp_slot_address (MtcsFunc *self,rtx old_rtx, rtx new_rtx)
{
  class mtcs_temp_slot *p;
  n_debug("mtcsfunc.c mtcs_func_update_temp_slot_address 00 %p %p\n",old_rtx,new_rtx);
  if (rtx_equal_p (old_rtx, new_rtx))
    return;
  n_debug("mtcsfunc.c mtcs_func_update_temp_slot_address 11 %p %p\n",old_rtx,new_rtx);
  p = find_temp_slot_from_address (self,old_rtx);
  /* If we didn't find one, see if both OLD_RTX is a PLUS.  If so, and
     NEW_RTX is a register, see if one operand of the PLUS is a
     temporary location.  If so, NEW_RTX points into it.  Otherwise,
     if both OLD_RTX and NEW_RTX are a PLUS and if there is a register
     in common between them.  If so, try a recursive call on those
     values.  */
  n_debug("mtcsfunc.c mtcs_func_update_temp_slot_address 22 %p\n",p);

  if (p == 0){
      if (GET_CODE (old_rtx) != PLUS)
          return;

      if (REG_P (new_rtx)){
          mtcs_func_update_temp_slot_address (self,XEXP (old_rtx, 0), new_rtx);
          mtcs_func_update_temp_slot_address (self,XEXP (old_rtx, 1), new_rtx);
          return;
      }else if (GET_CODE (new_rtx) != PLUS)
          return;

      if (rtx_equal_p (XEXP (old_rtx, 0), XEXP (new_rtx, 0)))
          mtcs_func_update_temp_slot_address (self,XEXP (old_rtx, 1), XEXP (new_rtx, 1));
      else if (rtx_equal_p (XEXP (old_rtx, 1), XEXP (new_rtx, 0)))
          mtcs_func_update_temp_slot_address (self,XEXP (old_rtx, 0), XEXP (new_rtx, 1));
      else if (rtx_equal_p (XEXP (old_rtx, 0), XEXP (new_rtx, 1)))
          mtcs_func_update_temp_slot_address (self,XEXP (old_rtx, 1), XEXP (new_rtx, 0));
      else if (rtx_equal_p (XEXP (old_rtx, 1), XEXP (new_rtx, 1)))
          mtcs_func_update_temp_slot_address (self,XEXP (old_rtx, 0), XEXP (new_rtx, 0));

      return;
  }
  n_debug("mtcsfunc.c mtcs_func_update_temp_slot_address 33 %p\n",p);

  /* Otherwise add an alias for the temp's address.  */
  insert_temp_slot_address(self,new_rtx, p);
}

/* Return true if EXP is an aggregate type (or a value with aggregate type).
   This means a type for which function calls must pass an address to the
   function or get an address back from the function.
   EXP may be a type node or an expression (whose type is tested).  */
//原型 aggregate_value_p function.h function.cc
bool mtcs_func_aggregate_value_p (MtcsFunc *self,const_tree exp, const_tree fntype)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsFuncAbi *mtcsFuncAbi=mtcs_target_get_func_abi(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  const_tree type = (TYPE_P (exp)) ? exp : TREE_TYPE (exp);
  int i, regno, nregs;
  rtx reg;

  if (fntype)
    switch (TREE_CODE (fntype)){
      case CALL_EXPR:
        {
          tree fndecl = get_callee_fndecl (fntype);
          if (fndecl)
            fntype = TREE_TYPE (fndecl);
          else if (CALL_EXPR_FN (fntype))
            fntype = TREE_TYPE (TREE_TYPE (CALL_EXPR_FN (fntype)));
          else
            /* For internal functions, assume nothing needs to be
               returned in memory.  */
            return false;
        }
        break;
      case FUNCTION_DECL:
        fntype = TREE_TYPE (fntype);
        break;
      case FUNCTION_TYPE:
      case METHOD_TYPE:
        break;
      case IDENTIFIER_NODE:
        fntype = NULL_TREE;
        break;
      default:
        /* We don't expect other tree types here.  */
        gcc_unreachable ();
    }

  if (VOID_TYPE_P (type))
    return false;

  if (error_operand_p (fntype))
    return false;

  /* If a record should be passed the same as its first (and only) member
     don't pass it as an aggregate.  */
  if (TREE_CODE (type) == RECORD_TYPE && TYPE_TRANSPARENT_AGGR (type))
    return mtcs_func_aggregate_value_p (self,first_field (type), fntype);

  /* If the front end has decided that this needs to be passed by
     reference, do so.  */
  if ((TREE_CODE (exp) == PARM_DECL || TREE_CODE (exp) == RESULT_DECL)
      && DECL_BY_REFERENCE (exp))
    return true;

  /* Function types that are TREE_ADDRESSABLE force return in memory.  */
  if (fntype && TREE_ADDRESSABLE (fntype))
    return true;

  /* Types that are TREE_ADDRESSABLE must be constructed in memory,
     and thus can't be returned in registers.  */
  if (TREE_ADDRESSABLE (type))
    return true;

  if (TYPE_EMPTY_P (type))
    return false;

  if (flag_pcc_struct_return && AGGREGATE_TYPE_P (type))
    return true;

  if (target_calls_return_in_memory/*!targetm.calls.return_in_mem*/(mtcsMachine->calls,type, fntype))
    return true;

  /* Make sure we have suitable call-clobbered regs to return
     the value in; if not, we must return it in memory.  */
  n_debug("mtcsfunc.c funciton.cc aggregate_value_p 00\n");

  reg = mtcs_func_hard_function_value/*!hard_function_value*/(self,type, 0, fntype, 0);
  n_debug("mtcsfunc.c funciton.cc aggregate_value_p 11 reg:%p REG_P (reg):%d\n",reg,REG_P (reg));

  /* If we have something other than a REG (e.g. a PARALLEL), then assume
     it is OK.  */
  if (!REG_P (reg))
    return false;

  /* Use the default ABI if the type of the function isn't known.
     The scheme for handling interoperability between different ABIs
     requires us to be able to tell when we're calling a function with
     a nondefault ABI.  */
  mtcs_predefined_function_abi *defaultAbi = mtcs_func_abi_get_default(mtcsFuncAbi);

  const mtcs_predefined_function_abi &abi = (fntype ?
        mtcs_func_abi_fntype_abi/*!fntype_abi*/(mtcsFuncAbi,fntype) : *defaultAbi/*!default_function_abi*/);
  regno = REGNO (reg);
  nregs = mtcs_reg_hard_regno_nregs (mtcsReg,regno, TYPE_MODE (type));
  n_debug("mtcsfunc.c funciton.cc aggregate_value_p 22 reg:%p nregs:%d\n",reg,nregs);
  for (i = 0; i < nregs; i++)
    if (!mtcsReg->hardRegs.x_fixed_regs/*!fixed_regs*/[regno + i] && !abi.clobbers_full_reg_p (regno + i)){
       n_debug("mtcsfunc.c aggregate_value_p 33 reg:%p nregs:%d i:d\n",reg,nregs,i);
      return true;
    }
  return false;
}

/* Return an rtx representing the register or memory location
   in which a scalar value of data type VALTYPE
   was returned by a function call to function FUNC.
   FUNC is a FUNCTION_DECL, FNTYPE a FUNCTION_TYPE node if the precise
   function is known, otherwise 0.
   OUTGOING is 1 if on a machine with register windows this function
   should return the register in which the function will put its result
   and 0 otherwise.  */
//原型 hard_function_value explow.h explow.cc
rtx mtcs_func_hard_function_value (MtcsFunc *self,const_tree valtype, const_tree func, const_tree fntype,
             int outgoing ATTRIBUTE_UNUSED)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  rtx val;
  n_debug("mtcsfunc.c  mtcs_func_hard_function_value 00 %p\n", mtcsMachine->calls->function_value);
  val = target_calls_function_value/*!targetm.calls.function_value*/(mtcsMachine->calls,valtype, func ? func : fntype, outgoing);
  n_debug("mtcsfunc.c  mtcs_func_hard_function_value 11\n");

  if (REG_P (val) && GET_MODE (val) == mtcsMode->modes.M_BLKmode){
      unsigned HOST_WIDE_INT bytes = arg_int_size_in_bytes (valtype);
      opt_scalar_int_mode tmpmode;
      /* int_size_in_bytes can return -1.  We don't need a check here
     since the value of bytes will then be large enough that no
     mode will match anyway.  */

      MTCS_FOR_EACH_MODE_IN_CLASS (mtcsMode,tmpmode, MODE_INT){
          /* Have we found a large enough mode?  */
          if (mtcs_mode_get_size(mtcsMode,tmpmode.require ()) >= bytes)
            break;
      }
      mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,val, tmpmode.require ());
  }
  return val;
}

//原型 ACCUMULATE_OUTGOING_ARGS host=0 nvptx=1
nboolean    mtcs_func_is_accumulate_outgoing_args (MtcsFunc *self)
{
    return self->accumulate_outgoing_args;
}

//原型 ACCUMULATE_OUTGOING_ARGS host=0 nvptx=1
void    mtcs_func_set_accumulate_outgoing_args (MtcsFunc *self,nboolean is)
{
    self->accumulate_outgoing_args=is;
}

//原型 OUTGOING_REG_PARM_STACK_SPACE
//i386 #define OUTGOING_REG_PARM_STACK_SPACE(FNTYPE) \
//(TARGET_64BIT && ix86_function_type_abi (FNTYPE) == MS_ABI)
//缺省 default.h #define OUTGOING_REG_PARM_STACK_SPACE(FNTYPE)=0

nboolean    mtcs_func_is_outgoint_reg_parm_stack_space (MtcsFunc *self,tree type)
{
    return 0;
}

//原型 PARM_BOUNDARY host=BITS_PER_WORD nvptx=32
void    mtcs_func_set_parm_boundary(MtcsFunc *self,int parm_boundary)
{
    self->parm_boundary=parm_boundary;
}
//原型 PARM_BOUNDARY host=BITS_PER_WORD nvptx=32
int     mtcs_func_get_parm_boundary(MtcsFunc *self)
{
    return self->parm_boundary;
}

//原型 #define FUNCTION_BOUNDARY 32
void    mtcs_func_set_function_boundary(MtcsFunc *self,int boundary)
{
   self->functionBoundary=boundary;
}

int     mtcs_func_get_function_boundary(MtcsFunc *self)
{
   return self->functionBoundary;
}

/* Round the stack offset in *OFFSET_PTR up to a multiple of BOUNDARY.
   BOUNDARY is measured in bits, but must be a multiple of a storage unit.  */

static void pad_to_arg_alignment (MtcsFunc *self,struct args_size *offset_ptr, int boundary,struct args_size *alignment_pad)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);

  tree save_var = NULL_TREE;
  poly_int64 save_constant = 0;
  int boundary_in_bytes = boundary / BITS_PER_UNIT;
  poly_int64 sp_offset = STACK_POINTER_OFFSET;

#ifdef SPARC_STACK_BOUNDARY_HACK
  /* ??? The SPARC port may claim a STACK_BOUNDARY higher than
     the real alignment of %sp.  However, when it does this, the
     alignment of %sp+STACK_POINTER_OFFSET is STACK_BOUNDARY.  */
  if (SPARC_STACK_BOUNDARY_HACK)
    sp_offset = 0;
#endif

  if (boundary >self->parm_boundary/*!PARM_BOUNDARY*/){
      save_var = offset_ptr->var;
      save_constant = offset_ptr->constant;
  }

  alignment_pad->var = NULL_TREE;
  alignment_pad->constant = 0;

  if (boundary > BITS_PER_UNIT){
      int misalign;
      if (offset_ptr->var || !known_misalignment (offset_ptr->constant + sp_offset,boundary_in_bytes, &misalign)){
          tree sp_offset_tree = ssize_int (sp_offset);
          tree offset = mtcs_const_size_binop/*!size_binop*/(mtcsConst,PLUS_EXPR,
                mtcs_func_args_size_tree/*!ARGS_SIZE_TREE*/(self,*offset_ptr),sp_offset_tree);
          tree rounded;
          if (mtcs_func_get_args_grows_downward/*!ARGS_GROW_DOWNWARD*/(self))
            rounded = round_down (offset, boundary / BITS_PER_UNIT);
          else
            rounded = round_up   (offset, boundary / BITS_PER_UNIT);

          offset_ptr->var = mtcs_const_size_binop/*!size_binop*/(mtcsConst,MINUS_EXPR, rounded, sp_offset_tree);
          /* ARGS_SIZE_TREE includes constant term.  */
          offset_ptr->constant = 0;
          if (boundary > PARM_BOUNDARY)
            alignment_pad->var = mtcs_const_size_binop/*!size_binop*/(mtcsConst,MINUS_EXPR, offset_ptr->var,save_var);
      }else{
          if (mtcs_func_get_args_grows_downward/*!ARGS_GROW_DOWNWARD*/(self))
            offset_ptr->constant -= misalign;
          else
            offset_ptr->constant += -misalign & (boundary_in_bytes - 1);

          if (boundary > self->parm_boundary/*!PARM_BOUNDARY*/)
            alignment_pad->constant = offset_ptr->constant - save_constant;
      }
  }
}


static void pad_below (MtcsFunc *self,struct args_size *offset_ptr, machine_mode passed_mode, tree sizetree)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  unsigned int align = self->parm_boundary/*!PARM_BOUNDARY*/ / BITS_PER_UNIT;
  int misalign;
  if (passed_mode != mtcsMode->modes.M_BLKmode
      && known_misalignment ((poly_uint16)mtcs_mode_get_size(mtcsMode,passed_mode), align, &misalign))
    offset_ptr->constant += -misalign & (align - 1);
  else{
      if (TREE_CODE (sizetree) != INTEGER_CST || (TREE_INT_CST_LOW (sizetree) & (align - 1)) != 0){
          /* Round the size up to multiple of PARM_BOUNDARY bits.  */
          tree s2 = round_up (sizetree, align);
          /* Add it in.  */
          mtcs_func_add_parm_size/*!ADD_PARM_SIZE*/(self,offset_ptr/*!*offset_ptr*/, s2);
          mtcs_func_sub_parm_size/*!SUB_PARM_SIZE*/(self,offset_ptr/*!*offset_ptr*/, sizetree);
      }
  }
}

/* A subroutine of assign_parms.  Initialize ALL.  */
//原型 assign_parms_initialize_all function.cc
static void assign_parms_initialize_all(MtcsFunc *self,struct assign_parm_data_all *all)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsArgs *mtcsArgs=mtcs_target_get_args(mtcsTarget);
  tree fntype ATTRIBUTE_UNUSED;
  memset (all, 0, sizeof (*all));
  fntype = TREE_TYPE (current_function_decl);
  all->args_so_far_v=mtcs_args_create_cumulative_args(mtcsArgs);
#ifdef INIT_CUMULATIVE_INCOMING_ARGS //host=0 nvptx=0
  INIT_CUMULATIVE_INCOMING_ARGS (all->args_so_far_v, fntype, NULL_RTX);
#else
  mtcs_args_init_cumulative_args/*!INIT_CUMULATIVE_ARGS*/(mtcsArgs,all->args_so_far_v, fntype, NULL_RTX, current_function_decl, -1);
#endif
  all->args_so_far = pack_cumulative_args ((CUMULATIVE_ARGS *)/*强转为CUMULATIVE_ARGS都是指针*/all->args_so_far_v);
#ifdef INCOMING_REG_PARM_STACK_SPACE
  all->reg_parm_stack_space = mtcs_func_get_stack_dynamic_offset/*!INCOMING_REG_PARM_STACK_SPACE*/(self,current_function_decl);
#endif
}

/* If ARGS contains entries with complex types, split the entry into two
   entries of the component type.  Return a new list of substitutions are
   needed, else the old list.  */

static void split_complex_args (MtcsFunc *self,vec<tree> *args)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
  MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  unsigned i;
  tree p;

  FOR_EACH_VEC_ELT (*args, i, p){
      tree type = TREE_TYPE (p);
      if (TREE_CODE (type) == COMPLEX_TYPE
              && target_calls_split_complex_arg/*!targetm.calls.split_complex_arg*/(mtcsMachine->calls,type)){
          tree decl;
          tree subtype = TREE_TYPE (type);
          bool addressable = TREE_ADDRESSABLE (p);

          /* Rewrite the PARM_DECL's type with its component.  */
          p = copy_node (p);
          TREE_TYPE (p) = subtype;
          DECL_ARG_TYPE (p) = TREE_TYPE (DECL_ARG_TYPE (p));
          SET_DECL_MODE (p, VOIDmode);
          DECL_SIZE (p) = NULL;
          DECL_SIZE_UNIT (p) = NULL;
          /* If this arg must go in memory, put it in a pseudo here.
             We can't allow it to go in memory as per normal parms,
             because the usual place might not have the imag part
             adjacent to the real part.  */
          DECL_ARTIFICIAL (p) = addressable;
          DECL_IGNORED_P (p) = addressable;
          TREE_ADDRESSABLE (p) = 0;
          mtcs_stor_layout_layout_decl/*!layout_decl*/(mtcsStorLayout,p, 0);
          (*args)[i] = p;

          /* Build a second synthetic decl.  */
          decl = mtcs_tree_build_decl/*!build_decl*/(mtcsTree,EXPR_LOCATION (p),PARM_DECL, NULL_TREE, subtype);
          DECL_ARG_TYPE (decl) = DECL_ARG_TYPE (p);
          DECL_ARTIFICIAL (decl) = addressable;
          DECL_IGNORED_P (decl) = addressable;
          mtcs_stor_layout_layout_decl/*!layout_decl*/(mtcsStorLayout,decl, 0);
          args->safe_insert (++i, decl);
      }
  }
}

/* A subroutine of assign_parms.  Adjust the parameter list to incorporate
   the hidden struct return argument, and (abi willing) complex args.
   Return the new parameter list.  */
//属于函数处理传入参数部分
//原型 assign_parms_augmented_arg_list function.cc
static vec<tree> assign_parms_augmented_arg_list (MtcsFunc *self,struct assign_parm_data_all *all)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsTree *mtcsTree = mtcs_target_get_tree(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  tree fndecl = current_function_decl;
  tree fntype = TREE_TYPE (fndecl);
  vec<tree> fnargs = vNULL;
  tree arg;
  n_debug("mtcsfunc.c  assign_parms_augmented_arg_list 00 fndecl:%p\n",fndecl);

  for (arg = DECL_ARGUMENTS (fndecl); arg; arg = DECL_CHAIN (arg)){
     n_debug("mtcsfunc.c  assign_parms_augmented_arg_list 11 fndecl:%p arg:%p\n",fndecl,arg);
     fnargs.safe_push (arg);
  }

  all->orig_fnargs = DECL_ARGUMENTS (fndecl);

  /* If struct value address is treated as the first argument, make it so.  */
  if (mtcs_func_aggregate_value_p/*!aggregate_value_p*/(self,DECL_RESULT (fndecl), fndecl)
      && ! cfun->returns_pcc_struct
      && target_calls_struct_value_rtx/*!targetm.calls.struct_value_rtx*/(mtcsMachine->calls,TREE_TYPE (fndecl), 1) == 0)
  {
      n_debug("mtcsfunc.c  assign_parms_augmented_arg_list 00 重要\n");
      tree type = mtcs_tree_build_pointer_type/*!build_pointer_type*/(mtcsTree,TREE_TYPE (fntype));
      tree decl;

      decl = mtcs_tree_build_decl/*!build_decl*/(mtcsTree,DECL_SOURCE_LOCATION (fndecl),
             PARM_DECL, get_identifier (".result_ptr"), type);
      DECL_ARG_TYPE (decl) = type;
      DECL_ARTIFICIAL (decl) = 1;
      DECL_NAMELESS (decl) = 1;
      TREE_CONSTANT (decl) = 1;
      /* We don't set DECL_IGNORED_P or DECL_REGISTER here.  If this
     changes, the end of the RESULT_DECL handling block in
     use_register_for_decl must be adjusted to match.  */

      DECL_CHAIN (decl) = all->orig_fnargs;
      all->orig_fnargs = decl;
      fnargs.safe_insert (0, decl);

      all->function_result_decl = decl;
  }
  /* If the target wants to split complex arguments into scalars, do so.  */
  if (mtcsMachine->calls->split_complex_arg/*!targetm.calls.split_complex_arg*/)
     split_complex_args (self,&fnargs);
  return fnargs;
}

/* A subroutine of assign_parms.  Invoke setup_incoming_varargs.  */
//原型 assign_parms_setup_varargs function.cc
static void assign_parms_setup_varargs (MtcsFunc *self,struct assign_parm_data_all *all,
                struct assign_parm_data_one *data, bool no_rtl)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  int varargs_pretend_bytes = 0;
  mtcs_function_arg_info last_named_arg = data->arg;
  last_named_arg.named = true;
  target_calls_setup_incoming_varargs/*!targetm.calls.setup_incoming_varargs*/(mtcsMachine->calls,
          all->args_so_far, last_named_arg, &varargs_pretend_bytes, no_rtl);

  /* If the back-end has requested extra stack space, record how much is
     needed.  Do not change pretend_args_size otherwise since it may be
     nonzero from an earlier partial argument.  */
  if (varargs_pretend_bytes > 0)
    all->pretend_args_size = varargs_pretend_bytes;
}

/* A subroutine of assign_parms.  Examine PARM and pull out type and mode
   data for the parameter.  Incorporate ABI specifics such as pass-by-
   reference and type promotion.  */
//原型 assign_parm_find_data_types function.cc
static void assign_parm_find_data_types (MtcsFunc *self,struct assign_parm_data_all *all, tree parm,
                 struct assign_parm_data_one *data)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsCalls *mtcsCalls =mtcs_target_get_calls(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  int unsignedp;
  *data = assign_parm_data_one ();
  /* NAMED_ARG is a misnomer.  We really mean 'non-variadic'. */
  if (!cfun->stdarg)
    data->arg.named = 1;  /* No variadic parms.  */
  else if (DECL_CHAIN (parm))
    data->arg.named = 1;  /* Not the last non-variadic parm. */
  else if (target_calls_strict_argument_naming/*!targetm.calls.strict_argument_naming*/(mtcsMachine->calls,all->args_so_far))
    data->arg.named = 1;  /* Only variadic ones are unnamed.  */
  else
    data->arg.named = 0;  /* Treat as variadic.  */

  data->nominal_type = TREE_TYPE (parm);
  data->arg.type = DECL_ARG_TYPE (parm);

  /* Look out for errors propagating this far.  Also, if the parameter's
     type is void then its value doesn't matter.  */
  if (TREE_TYPE (parm) == error_mark_node
      /* This can happen after weird syntax errors
     or if an enum type is defined among the parms.  */
      || TREE_CODE (parm) != PARM_DECL
      || data->arg.type == NULL
      || VOID_TYPE_P (data->nominal_type)){
      data->nominal_type = data->arg.type = void_type_node;
      data->nominal_mode = data->passed_mode = data->arg.mode = VOIDmode;
      return;
   }

  /* Find mode of arg as it is passed, and mode of arg as it should be
     during execution of this function.  */
  data->passed_mode = data->arg.mode = TYPE_MODE (data->arg.type);
  data->nominal_mode = TYPE_MODE (data->nominal_type);

  /* If the parm is to be passed as a transparent union or record, use the
     type of the first field for the tests below.  We have already verified
     that the modes are the same.  */
  if (RECORD_OR_UNION_TYPE_P (data->arg.type)  && TYPE_TRANSPARENT_AGGR (data->arg.type))
    data->arg.type = TREE_TYPE (first_field (data->arg.type));

  /* See if this arg was passed by invisible reference.  */
  if (mtcs_calls_apply_pass_by_reference_rules/*!apply_pass_by_reference_rules*/(mtcsCalls,all->args_so_far_v, data->arg)){
      data->nominal_type = data->arg.type;
      data->passed_mode = data->nominal_mode = data->arg.mode;
  }
  /* Find mode as it is passed by the ABI.  */
  unsignedp = TYPE_UNSIGNED (data->arg.type);
  n_debug("mtcsfunc.c  assign_parm_find_data_types 00 mode:%d unsignedp:%d\n", data->arg.mode,unsignedp);

  data->arg.mode  = mtcs_mode_promote_function_mode/*!promote_function_mode*/(mtcsMode,data->arg.type,
          data->arg.mode, &unsignedp, TREE_TYPE (current_function_decl), 0);
}

/* A subroutine of assign_parms.  Set DATA->ENTRY_PARM corresponding to
   the incoming location of the current parameter.  */
//确定命名参数的来源和位置
//原型 assign_parm_find_entry_rtl function.cc
static void assign_parm_find_entry_rtl (MtcsFunc *self,struct assign_parm_data_all *all,
                struct assign_parm_data_one *data)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  HOST_WIDE_INT pretend_bytes = 0;
  rtx entry_parm;
  bool in_regs;
  n_debug("mtcsfunc.c assign_parm_find_entry_rtl 00 reg_parm_stack_space:%d partial:%d arg.mode:%d\n",
          all->reg_parm_stack_space,data->partial,data->arg.mode);

  if (data->arg.mode == VOIDmode){
      data->entry_parm = data->stack_parm = const0_rtx;
      return;
  }
   //nvptx 空函数体
  target_calls_warn_parameter_passing_abi/*!targetm.calls.warn_parameter_passing_abi*/(mtcsMachine->calls,
        all->args_so_far,data->arg.type);
  n_debug("mtcsfunc.c assign_parm_find_entry_rtl 00 reg_parm_stack_space:%d partial:%d arg.mode:%d\n",
          all->reg_parm_stack_space,data->partial,data->arg.mode);
  //nvptx 具体实现
  entry_parm =  target_calls_function_incoming_arg/*!targetm.calls.function_incoming_arg*/(mtcsMachine->calls,
                                all->args_so_far, data->arg);
  if (entry_parm == 0)
    data->arg.mode = data->passed_mode;
  /* Determine parm's home in the stack, in case it arrives in the stack
     or we should pretend it did.  Compute the stack position and rtx where
     the argument arrives and its size.

     There is one complexity here:  If this was a parameter that would
     have been passed in registers, but wasn't only because it is
     __builtin_va_alist, we want locate_and_pad_parm to treat it as if
     it came in a register so that REG_PARM_STACK_SPACE isn't skipped.
     In this case, we call FUNCTION_ARG with NAMED set to 1 instead of 0
     as it was the previous time.  */
  in_regs = (entry_parm != 0);
#ifdef STACK_PARMS_IN_REG_PARM_AREA //host=0 nvptx=0
  in_regs = true;
#endif
  if (!in_regs && !data->arg.named){
      if (target_calls_pretend_outgoing_varargs_named/*!targetm.calls.pretend_outgoing_varargs_named*/(mtcsMachine->calls,all->args_so_far)){
          rtx tem;
          mtcs_function_arg_info named_arg = data->arg;
          named_arg.named = true;
          tem = target_calls_function_incoming_arg/*!targetm.calls.function_incoming_arg*/(mtcsMachine->calls,
                                  all->args_so_far,named_arg);
          in_regs = tem != NULL;
      }
  }
  /* If this parameter was passed both in registers and in the stack, use
     the copy on the stack.  */
  //如果函数参数必须是栈参数 entry_parm=0
  if (target_calls_must_pass_in_stack/*!targetm.calls.must_pass_in_stack*/(mtcsMachine->calls,data->arg))
    entry_parm = 0;

  if (entry_parm){
      int partial;
      partial = target_calls_arg_partial_bytes/*!targetm.calls.arg_partial_bytes*/(mtcsMachine->calls,
                    all->args_so_far, data->arg);
      data->partial = partial;

      /* The caller might already have allocated stack space for the
     register parameters.  */
      if (partial != 0 && all->reg_parm_stack_space == 0){
          /* Part of this argument is passed in registers and part
             is passed on the stack.  Ask the prologue code to extend
             the stack part so that we can recreate the full value.

             PRETEND_BYTES is the size of the registers we need to store.
             CURRENT_FUNCTION_PRETEND_ARGS_SIZE is the amount of extra
             stack space that the prologue should allocate.

             Internally, gcc assumes that the argument pointer is aligned
             to STACK_BOUNDARY bits.  This is used both for alignment
             optimizations (see init_emit) and to locate arguments that are
             aligned to more than PARM_BOUNDARY bits.  We must preserve this
             invariant by rounding CURRENT_FUNCTION_PRETEND_ARGS_SIZE up to
             a stack boundary.  */

          /* We assume at most one partial arg, and it must be the first
             argument on the stack.  */
          gcc_assert (!all->extra_pretend_bytes && !all->pretend_args_size);

          pretend_bytes = partial;
          all->pretend_args_size = CEIL_ROUND (pretend_bytes, STACK_BYTES);

          /* We want to align relative to the actual stack pointer, so
             don't include this in the stack size until later.  */
          all->extra_pretend_bytes = all->pretend_args_size;
      }
  }

  mtcs_func_locate_and_pad_parm/*!locate_and_pad_parm*/(self,data->arg.mode, data->arg.type, in_regs,
               all->reg_parm_stack_space,
               entry_parm ? data->partial : 0, current_function_decl,
               &all->stack_args_size, &data->locate);

  /* Update parm_stack_boundary if this parameter is passed in the
     stack.  */
  if (!in_regs && mtcsRtlData/*!crtl*/->parm_stack_boundary < data->locate.boundary)
    mtcsRtlData/*!crtl*/->parm_stack_boundary = data->locate.boundary;

  /* Adjust offsets to include the pretend args.  */
  pretend_bytes = all->extra_pretend_bytes - pretend_bytes;
  data->locate.slot_offset.constant += pretend_bytes;
  data->locate.offset.constant += pretend_bytes;

  data->entry_parm = entry_parm;
}

/* A subroutine of assign_parms.  If there is actually space on the stack
   for this parm, count it in stack_args_size and return true.  */
//判断参数是不是栈参数
//原型 assign_parm_is_stack_parm function.cc
static bool assign_parm_is_stack_parm (MtcsFunc *self,struct assign_parm_data_all *all,
               struct assign_parm_data_one *data)
{
  /* Trivially true if we've no incoming register.  */
   /* 如果没有传入寄存器，则结果显然为真。 */
  if (data->entry_parm == NULL)
    ;
  /* Also true if we're partially in registers and partially not,
     since we've arranged to drop the entire argument on the stack.  */
  /* 如果我们部分位于寄存器中，部分不位于寄存器中，则结果也成立，
  因为我们已经安排将整个参数放到堆栈中。*/
  else if (data->partial != 0)
    ;
  /* Also true if the target says that it's passed in both registers
     and on the stack.  */
  /* 如果目标声明它已同时传入寄存器和堆栈，则结果也为真。*/
  else if (GET_CODE (data->entry_parm) == PARALLEL
       && XEXP (XVECEXP (data->entry_parm, 0, 0), 0) == NULL_RTX)
    ;
  /* Also true if the target says that there's stack allocated for
     all register parameters.  */
  /* 如果目标表明已为所有寄存器参数分配了堆栈，则也为真。*/
  else if (all->reg_parm_stack_space > 0)
    ;
  /* Otherwise, no, this parameter has no ABI defined stack slot.  */
  else
    return false;

  all->stack_args_size.constant += data->locate.size.constant;
  if (data->locate.size.var)
     mtcs_func_add_parm_size/*!ADD_PARM_SIZE*/(self,&all->stack_args_size, data->locate.size.var);

  return true;
}

/* A subroutine of assign_parms.  Given that this parameter is allocated
   stack space by the ABI, find it.  */
//原型 assign_parm_find_stack_rtl funciton.cc
static void assign_parm_find_stack_rtl (MtcsFunc *self,tree parm, struct assign_parm_data_one *data)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  rtx offset_rtx, stack_parm;
  unsigned int align, boundary;

  /* If we're passing this arg using a reg, make its stack home the
     aligned stack slot.  */
  if (data->entry_parm)
    offset_rtx = mtcs_func_args_size_rtx/*!ARGS_SIZE_RTX*/(self,data->locate.slot_offset);
  else
    offset_rtx = mtcs_func_args_size_rtx/*!ARGS_SIZE_RTX*/(self,data->locate.offset);
  stack_parm = mtcsRtlData/*!crtl*/->args.internal_arg_pointer;
  if (offset_rtx != const0_rtx)
    stack_parm = gen_rtx_PLUS (mtcs_mode_get_Pmode(mtcsMode), stack_parm, offset_rtx);
  stack_parm = gen_rtx_MEM (data->arg.mode, stack_parm);

  if (!data->arg.pass_by_reference){
      mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,stack_parm, parm, 1);
      /* set_mem_attributes could set MEM_SIZE to the passed mode's size,
       while promoted mode's size is needed.  */
      if (data->arg.mode != mtcsMode->modes.M_BLKmode  && data->arg.mode != DECL_MODE (parm)){
          mtcs_rtl_set_mem_size/*!set_mem_size*/(mtcsRTL,
                      stack_parm, mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,data->arg.mode));
          if (mtcs_rtl_get_mem_expr/*!MEM_EXPR*/(mtcsRTL,stack_parm)
                  && mtcs_rtl_is_mem_offset_known_p/*!MEM_OFFSET_KNOWN_P*/(mtcsRTL,stack_parm)){
              poly_int64 offset = mtcs_mode_subreg_lowpart_offset/*!subreg_lowpart_offset*/(mtcsMode,DECL_MODE (parm),data->arg.mode);
              if (maybe_ne (offset, 0))
                  mtcs_rtl_set_mem_offset/*!set_mem_offset*/(mtcsRTL,stack_parm,
                          mtcs_rtl_get_mem_offset/*!MEM_OFFSET*/(mtcsRTL,stack_parm) - offset);
          }
     }
  }

  boundary = data->locate.boundary;
  align = BITS_PER_UNIT;

  /* If we're padding upward, we know that the alignment of the slot
     is TARGET_FUNCTION_ARG_BOUNDARY.  If we're using slot_offset, we're
     intentionally forcing upward padding.  Otherwise we have to come
     up with a guess at the alignment based on OFFSET_RTX.  */
  poly_int64 offset;
  if (data->locate.where_pad == PAD_NONE || data->entry_parm)
    align = boundary;
  else if (data->locate.where_pad == PAD_UPWARD){
      align = boundary;
      /* If the argument offset is actually more aligned than the nominal
     stack slot boundary, take advantage of that excess alignment.
     Don't make any assumptions if STACK_POINTER_OFFSET is in use.  */
      if (poly_int_rtx_p (offset_rtx, &offset)  && known_eq (STACK_POINTER_OFFSET, 0)){
          unsigned int offset_align = known_alignment (offset) * BITS_PER_UNIT;
          if (offset_align == 0 || offset_align > STACK_BOUNDARY)
            offset_align = STACK_BOUNDARY;
          align = MAX (align, offset_align);
      }
  }else if (poly_int_rtx_p (offset_rtx, &offset)){
      align = least_bit_hwi (boundary);
      unsigned int offset_align = known_alignment (offset) * BITS_PER_UNIT;
      if (offset_align != 0)
          align = MIN (align, offset_align);
  }
  mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,stack_parm, align);

  if (data->entry_parm)
      mtcs_rtl_set_reg_attrs_for_parm/*!set_reg_attrs_for_parm*/(mtcsRTL,data->entry_parm, stack_parm);

  data->stack_parm = stack_parm;
}

/* A subroutine of assign_parms.  Adjust DATA->ENTRY_RTL such that it's
   always valid and contiguous.  */
//原型 assign_parm_adjust_entry_rtl function.cc
static void assign_parm_adjust_entry_rtl (MtcsFunc *self,struct assign_parm_data_one *data)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);

  rtx entry_parm = data->entry_parm;
  rtx stack_parm = data->stack_parm;

  /* If this parm was passed part in regs and part in memory, pretend it
     arrived entirely in memory by pushing the register-part onto the stack.
     In the special case of a DImode or DFmode that is split, we could put
     it together in a pseudoreg directly, but for now that's not worth
     bothering with.  */
  if (data->partial != 0){
      /* Handle calls that pass values in multiple non-contiguous
     locations.  The Irix 6 ABI has examples of this.  */
      if (GET_CODE (entry_parm) == PARALLEL)
          mtcs_expr_emit_group_store/*!emit_group_store*/(mtcsExpr,
                  mtcs_explow_validize_mem/*!validize_mem*/(mtcsExplow,copy_rtx (stack_parm)), entry_parm,
              data->arg.type, int_size_in_bytes (data->arg.type));
      else{
          gcc_assert (data->partial % UNITS_PER_WORD == 0);
          mtcs_expr_move_block_from_reg/*!move_block_from_reg*/(mtcsExpr,REGNO (entry_parm),
                  mtcs_explow_validize_mem/*!validize_mem*/(mtcsExplow,copy_rtx (stack_parm)),
                       data->partial / UNITS_PER_WORD);
      }

      entry_parm = stack_parm;
  }

  /* If we didn't decide this parm came in a register, by default it came
     on the stack.  */
  else if (entry_parm == NULL)
    entry_parm = stack_parm;

  /* When an argument is passed in multiple locations, we can't make use
     of this information, but we can save some copying if the whole argument
     is passed in a single register.  */
  else if (GET_CODE (entry_parm) == PARALLEL
       && data->nominal_mode != mtcsMode->modes.M_BLKmode
       && data->passed_mode != mtcsMode->modes.M_BLKmode){
      size_t i, len = XVECLEN (entry_parm, 0);

      for (i = 0; i < len; i++)
         if (XEXP (XVECEXP (entry_parm, 0, i), 0) != NULL_RTX
            && REG_P (XEXP (XVECEXP (entry_parm, 0, i), 0))
            && (GET_MODE (XEXP (XVECEXP (entry_parm, 0, i), 0))
            == data->passed_mode)
            && INTVAL (XEXP (XVECEXP (entry_parm, 0, i), 1)) == 0){
            entry_parm = XEXP (XVECEXP (entry_parm, 0, i), 0);
            break;
         }
  }

  data->entry_parm = entry_parm;
}

/* A subroutine of assign_parms.  Adjust DATA->STACK_RTL such that it's
   always valid and properly aligned.  */
//原型 assign_parm_adjust_stack_rtl function.cc
static void assign_parm_adjust_stack_rtl (MtcsFunc *self,struct assign_parm_data_one *data)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem = mtcsOptions->global_options;

  rtx stack_parm = data->stack_parm;

  /* If we can't trust the parm stack slot to be aligned enough for its
     ultimate type, don't use that slot after entry.  We'll make another
     stack slot, if we need one.  */
  if (stack_parm  && ((mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,data->nominal_mode) >
                 mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,stack_parm)
       && ((mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,movmisalign_optab, data->nominal_mode)
        != CODE_FOR_nothing)
           || mtcsTarget->slow_unaligned_access/*!targetm.slow_unaligned_access*/(mtcsTarget,data->nominal_mode,
                   mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,stack_parm))))
      || (data->nominal_type
          && TYPE_ALIGN (data->nominal_type) >mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,stack_parm)
          && mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,stack_parm) <
          mtcs_func_get_preferred_stack_boundary/*!PREFERRED_STACK_BOUNDARY*/(self))))
    stack_parm = NULL;

  /* If parm was passed in memory, and we need to convert it on entry,
     don't store it back in that same slot.  */
  else if (data->entry_parm == stack_parm
       && data->nominal_mode != mtcsMode->modes.M_BLKmode
       && data->nominal_mode != data->passed_mode)
    stack_parm = NULL;

  /* If stack protection is in effect for this function, don't leave any
     pointers in their passed stack slots.  */
  else if (mtcsRtlData/*!crtl*/->stack_protect_guard
       && (mtcsOptionsItem->x_flag_stack_protect == SPCT_FLAG_ALL
           || data->arg.pass_by_reference
           || POINTER_TYPE_P (data->nominal_type)))
    stack_parm = NULL;

  data->stack_parm = stack_parm;
}

/* A subroutine of assign_parms.  Return true if the current parameter
   should be stored as a BLKmode in the current frame.  */
//原型 assign_parm_setup_block_p function.cc
static bool assign_parm_setup_block_p (MtcsFunc *self,struct assign_parm_data_one *data)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);

  if (data->nominal_mode == mtcsMode->modes.M_BLKmode)
    return true;
  if (GET_MODE (data->entry_parm) == mtcsMode->modes.M_BLKmode)
    return true;

#ifdef BLOCK_REG_PADDING //host=0 nvptx=0
  /* Only assign_parm_setup_block knows how to deal with register arguments
     that are padded at the least significant end.  */
  if (REG_P (data->entry_parm)
      && known_lt (GET_MODE_SIZE (data->arg.mode), UNITS_PER_WORD)
      && (BLOCK_REG_PADDING (data->passed_mode, data->arg.type, 1)
      == (BYTES_BIG_ENDIAN ? PAD_UPWARD : PAD_DOWNWARD)))
    return true;
#endif

  return false;
}

/* A subroutine of assign_parms.  Arrange for the parameter to be
   present and valid in DATA->STACK_RTL.  */
//原型 assign_parm_setup_block funciton.cc
static void assign_parm_setup_block (MtcsFunc *self,struct assign_parm_data_all *all,
             tree parm, struct assign_parm_data_one *data)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsExpand *mtcsExpand=mtcs_target_get_expand(mtcsTarget);
  MtcsGimpleExpr *mtcsGimpleExpr=mtcs_target_get_gimple_expr(mtcsTarget);

  rtx entry_parm = data->entry_parm;
  rtx stack_parm = data->stack_parm;
  rtx target_reg = NULL_RTX;
  bool in_conversion_seq = false;
  HOST_WIDE_INT size;
  HOST_WIDE_INT size_stored;

  if (GET_CODE (entry_parm) == PARALLEL)
    entry_parm = mtcs_expr_emit_group_move_into_temps/*!emit_group_move_into_temps*/(mtcsExpr,entry_parm);

  /* If we want the parameter in a pseudo, don't use a stack slot.  */
  if (mtcs_gimple_expr_is_gimple_reg/*!is_gimple_reg*/(mtcsGimpleExpr,parm)
          && mtcs_func_use_register_for_decl/*!use_register_for_decl*/(self,parm)){
      tree def = ssa_default_def (cfun, parm);
      gcc_assert (def);
      machine_mode mode = mtcs_mode_promote_ssa_mode/*!promote_ssa_mode*/(mtcsMode,def, NULL);
      rtx reg = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
      if (GET_CODE (reg) != CONCAT)
          stack_parm = reg;
      else{
          target_reg = reg;
          /* Avoid allocating a stack slot, if there isn't one
             preallocated by the ABI.  It might seem like we should
             always prefer a pseudo, but converting between
             floating-point and integer modes goes through the stack
             on various machines, so it's better to use the reserved
             stack slot than to risk wasting it and allocating more
             for the conversion.  */
          if (stack_parm == NULL_RTX){
              int save = generating_concat_p;
              generating_concat_p = 0;
              stack_parm = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
              generating_concat_p = save;
          }
      }
      data->stack_parm = NULL;
  }
  int maxSupportStackAlignment= mtcs_func_get_max_support_stack_alignment(self);//MAX_SUPPORTED_STACK_ALIGNMENT
  size = int_size_in_bytes (data->arg.type);
  size_stored = CEIL_ROUND (size, UNITS_PER_WORD);
  if (stack_parm == 0){
      HOST_WIDE_INT parm_align= (mtcs_align_get_strict_alignment/*!STRICT_ALIGNMENT*/(mtcsAlign)
       ? MAX (DECL_ALIGN (parm), BITS_PER_WORD) : DECL_ALIGN (parm));

      SET_DECL_ALIGN (parm, parm_align);
      if (DECL_ALIGN (parm) > maxSupportStackAlignment/*!MAX_SUPPORTED_STACK_ALIGNMENT*/){
          rtx allocsize =mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,size_stored, mtcs_mode_get_Pmode(mtcsMode));
          mtcs_explow_get_dynamic_stack_size/*!get_dynamic_stack_size*/(mtcsExplow,&allocsize, 0, DECL_ALIGN (parm), NULL);
          stack_parm = mtcs_func_assign_stack_local/*!assign_stack_local*/(self,
                  mtcsMode->modes.M_BLKmode, UINTVAL (allocsize),
                  maxSupportStackAlignment/*!MAX_SUPPORTED_STACK_ALIGNMENT*/);
          rtx addr = mtcs_explow_align_dynamic_address/*!align_dynamic_address*/(mtcsExplow,XEXP (stack_parm, 0),
                            DECL_ALIGN (parm));
          mtcs_rtl_mark_reg_pointer/*!mark_reg_pointer*/(mtcsRTL,addr, DECL_ALIGN (parm));
          stack_parm = gen_rtx_MEM (GET_MODE (stack_parm), addr);
          MEM_NOTRAP_P (stack_parm) = 1;
      }else
          stack_parm = mtcs_func_assign_stack_local/*!assign_stack_local*/(self,
                  mtcsMode->modes.M_BLKmode, size_stored,DECL_ALIGN (parm));
      if (known_eq (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (entry_parm)), size))
          mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,stack_parm, GET_MODE (entry_parm));
      mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,stack_parm, parm, 1);
  }

  /* If a BLKmode arrives in registers, copy it to a stack slot.  Handle
     calls that pass values in multiple non-contiguous locations.  */
  if (REG_P (entry_parm) || GET_CODE (entry_parm) == PARALLEL){
      rtx mem;

      /* Note that we will be storing an integral number of words.
     So we have to be careful to ensure that we allocate an
     integral number of words.  We do this above when we call
     assign_stack_local if space was not allocated in the argument
     list.  If it was, this will not work if PARM_BOUNDARY is not
     a multiple of BITS_PER_WORD.  It isn't clear how to fix this
     if it becomes a problem.  Exception is when BLKmode arrives
     with arguments not conforming to word_mode.  */

      if (data->stack_parm == 0)
          ;
      else if (GET_CODE (entry_parm) == PARALLEL)
          ;
      else
          gcc_assert (!size || !(PARM_BOUNDARY % BITS_PER_WORD));

      mem = mtcs_explow_validize_mem/*!validize_mem*/(mtcsExplow,copy_rtx (stack_parm));

      /* Handle values in multiple non-contiguous locations.  */
      if (GET_CODE (entry_parm) == PARALLEL && !MEM_P (mem))
          mtcs_expr_emit_group_store/*!emit_group_store*/(mtcsExpr,mem, entry_parm, data->arg.type, size);
      else if (GET_CODE (entry_parm) == PARALLEL){
          mtcs_emit_push_to_sequence2/*!push_to_sequence2*/(mtcsEmit,all->first_conversion_insn,all->last_conversion_insn);
          mtcs_expr_emit_group_store/*!emit_group_store*/(mtcsExpr,mem, entry_parm, data->arg.type, size);
          all->first_conversion_insn =mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
          all->last_conversion_insn = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
          mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
          in_conversion_seq = true;
      }else if (size == 0)
          ;
      /* If SIZE is that of a mode no bigger than a word, just use
     that mode's store operation.  */
      else if (size <= UNITS_PER_WORD){
          unsigned int bits = size * BITS_PER_UNIT;
          machine_mode mode = mtcs_mode_int_mode_for_size/*!int_mode_for_size*/(mtcsMode,bits, 0).else_blk ();
          if (mode != mtcsMode->modes.M_BLKmode
    #ifdef BLOCK_REG_PADDING
              && (size == UNITS_PER_WORD
              || (BLOCK_REG_PADDING (mode, data->arg.type, 1)
                  != (BYTES_BIG_ENDIAN ? PAD_UPWARD : PAD_DOWNWARD)))
    #endif
              )
          {
              rtx reg;

              /* We are really truncating a word_mode value containing
             SIZE bytes into a value of mode MODE.  If such an
             operation requires no actual instructions, we can refer
             to the value directly in mode MODE, otherwise we must
             start with the register in word_mode and explicitly
             convert it.  */
              if (mode == mtcsMode->word_mode
                || mtcs_mode_truly_noop_truncation_p/*!TRULY_NOOP_TRUNCATION_MODES_P*/(mtcsMode,mode, mtcsMode->word_mode))
                  reg = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,mode, REGNO (entry_parm));
              else{
                  reg = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,word_mode, REGNO (entry_parm));
                  reg = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,mode, copy_to_reg (reg), 1);
              }
              /* We use adjust_address to get a new MEM with the mode
             changed.  adjust_address is better than change_address
             for this purpose because adjust_address does not lose
             the MEM_EXPR associated with the MEM.

             If the MEM_EXPR is lost, then optimizations like DSE
             assume the MEM escapes and thus is not subject to DSE.  */
              mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,
                      mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,mem, mode, 0), reg);
          }

    #ifdef BLOCK_REG_PADDING
          /* Storing the register in memory as a full word, as
             move_block_from_reg below would do, and then using the
             MEM in a smaller mode, has the effect of shifting right
             if BYTES_BIG_ENDIAN.  If we're bypassing memory, the
             shifting must be explicit.  */
          else if (!MEM_P (mem))
            {
              rtx x;

              /* If the assert below fails, we should have taken the
             mode != BLKmode path above, unless we have downward
             padding of smaller-than-word arguments on a machine
             with little-endian bytes, which would likely require
             additional changes to work correctly.  */
              gcc_checking_assert (BYTES_BIG_ENDIAN
                       && (BLOCK_REG_PADDING (mode,
                                  data->arg.type, 1)
                           == PAD_UPWARD));

              int by = (UNITS_PER_WORD - size) * BITS_PER_UNIT;

              x = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,word_mode, REGNO (entry_parm));
              x = expand_shift (RSHIFT_EXPR, word_mode, x, by,
                    NULL_RTX, 1);
              x = force_reg (word_mode, x);
              x = gen_lowpart_SUBREG (GET_MODE (mem), x);

              emit_move_insn (mem, x);
            }
    #endif

      /* Blocks smaller than a word on a BYTES_BIG_ENDIAN
         machine must be aligned to the left before storing
         to memory.  Note that the previous test doesn't
         handle all cases (e.g. SIZE == 3).  */
      else if (size != UNITS_PER_WORD
#ifdef BLOCK_REG_PADDING
           && (BLOCK_REG_PADDING (mode, data->arg.type, 1)
               == PAD_DOWNWARD)
#else
           && BYTES_BIG_ENDIAN
#endif
           )
      {
          rtx tem, x;
          int by = (UNITS_PER_WORD - size) * BITS_PER_UNIT;
          rtx reg = mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,word_mode, REGNO (entry_parm));

          x = mtcs_expmed_expand_shift/*!expand_shift*/(mtcsExpmed,LSHIFT_EXPR,
                  mtcsMode->word_mode, reg, by, NULL_RTX, 1);
          tem = mtcs_rtl_change_address/*!change_address*/(mtcsRTL,mem, mtcsMode->word_mode, 0);
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,tem, x);
      }else
          mtcs_expr_move_block_from_reg/*!move_block_from_reg*/(mtcsExpr,
                  REGNO (entry_parm), mem, size_stored / UNITS_PER_WORD);
      }else if (!MEM_P (mem)){
          gcc_checking_assert (size > UNITS_PER_WORD);
    #ifdef BLOCK_REG_PADDING
          gcc_checking_assert (BLOCK_REG_PADDING (GET_MODE (mem),
                              data->arg.type, 0)
                       == PAD_UPWARD);
    #endif
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,mem, entry_parm);
      }else
          mtcs_expr_move_block_from_reg/*!move_block_from_reg*/(mtcsExpr,
                  REGNO (entry_parm), mem,size_stored / UNITS_PER_WORD);
  }else if (data->stack_parm == 0 && !TYPE_EMPTY_P (data->arg.type)){
      mtcs_emit_push_to_sequence2/*!push_to_sequence2*/(mtcsEmit,all->first_conversion_insn, all->last_conversion_insn);
      mtcs_expr_emit_block_move/*!emit_block_move*/(mtcsExpr,stack_parm, data->entry_parm, GEN_INT (size),
               BLOCK_OP_NORMAL);
      all->first_conversion_insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
      all->last_conversion_insn = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
      in_conversion_seq = true;
  }

  if (target_reg){
      if (!in_conversion_seq)
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target_reg, stack_parm);
      else{
          mtcs_emit_push_to_sequence2/*!push_to_sequence2*/(mtcsEmit,all->first_conversion_insn, all->last_conversion_insn);
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,target_reg, stack_parm);
          all->first_conversion_insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
          all->last_conversion_insn = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
          mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
      }
      stack_parm = target_reg;
  }

  data->stack_parm = stack_parm;
  mtcs_expand_set_parm_rtl/*!set_parm_rtl*/(mtcsExpand,parm, stack_parm);
}

/* A subroutine of assign_parms.  Reconstitute any values which were
   passed in multiple registers and would fit in a single register.  */
//原型 assign_parm_remove_parallels funciton.cc
static void assign_parm_remove_parallels (MtcsFunc *self,struct assign_parm_data_one *data)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  rtx entry_parm = data->entry_parm;

  /* Convert the PARALLEL to a REG of the same mode as the parallel.
     This can be done with register operations rather than on the
     stack, even if we will store the reconstituted parameter on the
     stack later.  */
  if (GET_CODE (entry_parm) == PARALLEL
          && GET_MODE (entry_parm) != mtcsMode->modes.M_BLKmode){
      rtx parmreg = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (entry_parm));
      mtcs_expr_emit_group_store/*!emit_group_store*/(mtcsExpr,parmreg, entry_parm, data->arg.type,
            mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (entry_parm)));
      entry_parm = parmreg;
  }

  data->entry_parm = entry_parm;
}

typedef struct _RecordHardRegSetsCallbackData{
   MtcsFunc *mtcsFunc;
   HardRegSet *pset;
}RecordHardRegSetsCallbackData;
//原型 record_hard_reg_sets rtl.h rtlanal.cc
static void recordHardRegSets_cb(rtx x, const_rtx pat ATTRIBUTE_UNUSED, void *userData)
{
  RecordHardRegSetsCallbackData *info=(RecordHardRegSetsCallbackData *)userData;
  MtcsFunc *self=info->mtcsFunc;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

  HardRegSet/*!HARD_REG_SET*/ *pset = (HardRegSet *)info->pset;
  if (REG_P (x) && mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,x))
      mtcs_reg_add_to_hard_reg_set/*!add_to_hard_reg_set*/(mtcsReg,pset, GET_MODE (x), REGNO (x));
}

/* A subroutine of assign_parms.  Allocate a pseudo to hold the current
   parameter.  Get it there.  Perform all ABI specified conversions.  */
//原型 assign_parm_setup_reg function.cc
static void assign_parm_setup_reg (MtcsFunc *self,struct assign_parm_data_all *all, tree parm,
               struct assign_parm_data_one *data)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsExpand *mtcsExpand=mtcs_target_get_expand(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);

  rtx parmreg, validated_mem;
  rtx equiv_stack_parm;
  machine_mode promoted_nominal_mode;
  int unsignedp = TYPE_UNSIGNED (TREE_TYPE (parm));
  bool did_conversion = false;
  bool need_conversion, moved;
  enum insn_code icode;
  rtx rtl;
  /* Store the parm in a pseudoregister during the function, but we may
     need to do it in a wider mode.  Using 2 here makes the result
     consistent with promote_decl_mode and thus expand_expr_real_1.  */
  n_debug("mtcsfunc.c  assign_parm_setup_reg 00 mode:%d unsignedp:%d\n", data->nominal_mode,unsignedp);

  promoted_nominal_mode = mtcs_mode_promote_function_mode/*!promote_function_mode*/(mtcsMode,
          data->nominal_type,data->nominal_mode, &unsignedp, TREE_TYPE (current_function_decl), 2);
  parmreg = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,promoted_nominal_mode);
  if (!DECL_ARTIFICIAL (parm))
    mark_user_reg (parmreg);
  /* If this was an item that we received a pointer to,
     set rtl appropriately.  */
  if (data->arg.pass_by_reference){
      rtl = gen_rtx_MEM (TYPE_MODE (TREE_TYPE (data->arg.type)), parmreg);
      mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,rtl, parm, 1);
  }else
    rtl = parmreg;

  assign_parm_remove_parallels(self,data);
  /* Copy the value into the register, thus bridging between
     assign_parm_find_data_types and expand_expr_real_1.  */
  equiv_stack_parm = data->stack_parm;
  validated_mem = mtcs_explow_validize_mem/*!validize_mem*/(mtcsExplow,copy_rtx (data->entry_parm));
  need_conversion = (data->nominal_mode != data->passed_mode
             || promoted_nominal_mode != data->arg.mode);
  moved = false;
  if (need_conversion
      && mtcs_mode_get_class/*!GET_MODE_CLASS*/(mtcsMode,data->nominal_mode) == MODE_INT
      && data->nominal_mode == data->passed_mode
      && data->nominal_mode == GET_MODE (data->entry_parm)){
      /* ENTRY_PARM has been converted to PROMOTED_MODE, its
     mode, by the caller.  We now have to convert it to
     NOMINAL_MODE, if different.  However, PARMREG may be in
     a different mode than NOMINAL_MODE if it is being stored
     promoted.

     If ENTRY_PARM is a hard register, it might be in a register
     not valid for operating in its mode (e.g., an odd-numbered
     register for a DFmode).  In that case, moves are the only
     thing valid, so we can't do a convert from there.  This
     occurs when the calling sequence allow such misaligned
     usages.

     In addition, the conversion may involve a call, which could
     clobber parameters which haven't been copied to pseudo
     registers yet.

     First, we try to emit an insn which performs the necessary
     conversion.  We verify that this insn does not clobber any
     hard registers.  */

      rtx op0, op1;

      icode = mtcs_optabs_can_extend_p/*!can_extend_p*/(mtcsOptabs,promoted_nominal_mode,
              data->passed_mode,unsignedp);

      op0 = parmreg;
      op1 = validated_mem;
      if (icode != CODE_FOR_nothing
        && mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(mtcsOptabs,icode, 0, op0)
        && mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(mtcsOptabs,icode, 1, op1)){
          enum rtx_code code = unsignedp ? ZERO_EXTEND : SIGN_EXTEND;
          rtx_insn *insn, *insns;
          rtx t = op1;

          HardRegSet hardregs={mtcs_reg_get_hard_reg_element_count(mtcsReg)};/*!HARD_REG_SET hardregs;*/

          mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
          /* If op1 is a hard register that is likely spilled, first
             force it into a pseudo, otherwise combiner might extend
             its lifetime too much.  */
          if (GET_CODE (t) == SUBREG)
            t = SUBREG_REG (t);
          if (REG_P (t)
              && mtcs_reg_is_hard_rtx/*!HARD_REGISTER_P*/(mtcsReg,t)
              && ! mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(
                      &mtcsReg->hardRegs.x_fixed_reg_set/*!fixed_reg_set*/, REGNO (t))
              && mtcsTarget->class_likely_spilled_p/*!targetm.class_likely_spilled_p*/
                   (mtcsTarget,mtcs_reg_get_class/*!REGNO_REG_CLASS*/(mtcsReg,REGNO (t)))){
              t = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (op1));
              mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,t, op1);
          }else
              t = op1;
          rtx_insn *pat =mtcs_optabs_gen_extend_insn/*!gen_extend_insn*/(mtcsOptabs,op0,
                  t, promoted_nominal_mode,data->passed_mode, unsignedp);
          mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,pat);
          insns =mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);

          moved = true;
          mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&hardregs);
          for (insn = insns; insn && moved; insn = NEXT_INSN (insn)){
              if (INSN_P (insn)){
                  /*!note_stores (insn, record_hard_reg_sets, &hardregs);*/
                 RecordHardRegSetsCallbackData userData={self,&hardregs};
                 mtcs_rtlanal_note_stores/*!note_stores*/(mtcsRtlanal,insn, recordHardRegSets_cb, &userData/*!&hardregs*/);
              }
              if (!mtcs_reg_hard_reg_set_empty_p/*!hard_reg_set_empty_p*/(&hardregs))
                  moved = false;
          }

          mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
          if (moved){
              mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,insns);
              if (equiv_stack_parm != NULL_RTX)
                  equiv_stack_parm = gen_rtx_fmt_e (code, GET_MODE (parmreg),equiv_stack_parm);
          }
      }
  }

  if (moved)
    /* Nothing to do.  */
    ;
  else if (need_conversion){
      /* We did not have an insn to convert directly, or the sequence
     generated appeared unsafe.  We must first copy the parm to a
     pseudo reg, and save the conversion until after all
     parameters have been moved.  */

      int save_tree_used;
      rtx tempreg = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (data->entry_parm));

      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,tempreg, validated_mem);

      mtcs_emit_push_to_sequence2/*!push_to_sequence2*/(mtcsEmit,all->first_conversion_insn, all->last_conversion_insn);
      tempreg = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,data->nominal_mode, tempreg, unsignedp);

      if (mtcs_rtl_partial_subreg_p/*!partial_subreg_p*/(mtcsRTL,tempreg)
        && GET_MODE (tempreg) == data->nominal_mode
        && REG_P (SUBREG_REG (tempreg))
        && data->nominal_mode == data->passed_mode
        && GET_MODE (SUBREG_REG (tempreg)) == GET_MODE (data->entry_parm)){
          /* The argument is already sign/zero extended, so note it
             into the subreg.  */
          SUBREG_PROMOTED_VAR_P (tempreg) = 1;
          SUBREG_PROMOTED_SET (tempreg, unsignedp);
      }

      /* TREE_USED gets set erroneously during expand_assignment.  */
      save_tree_used = TREE_USED (parm);
      mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,parm, rtl);
      mtcs_expr_expand_assignment/*!expand_assignment*/(mtcsExpr,parm,
            mtcs_expmed_make_tree/*!make_tree*/(mtcsExpmed,data->nominal_type, tempreg), false);
      mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,parm, NULL_RTX);
      TREE_USED (parm) = save_tree_used;
      all->first_conversion_insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
      all->last_conversion_insn = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);

      did_conversion = true;
  }else if (MEM_P (data->entry_parm)
       && mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,promoted_nominal_mode)
          > mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,data->entry_parm)
       && (((icode = mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,movmisalign_optab,
                    promoted_nominal_mode)) != CODE_FOR_nothing)
       || mtcsTarget->slow_unaligned_access/*!targetm.slow_unaligned_access*/(mtcsTarget,promoted_nominal_mode,
               mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,data->entry_parm)))){
      if (icode != CODE_FOR_nothing)
          mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,GEN_FCN (icode) (parmreg, validated_mem));
      else
        rtl = parmreg = mtcs_expmed_extract_bit_field/*!extract_bit_field*/(mtcsExpmed,validated_mem,
                mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,promoted_nominal_mode), 0,
                unsignedp, parmreg,
                promoted_nominal_mode, VOIDmode, false, NULL);
  }else
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,parmreg, validated_mem);

  /* If we were passed a pointer but the actual value can live in a register,
     retrieve it and use it directly.  Note that we cannot use nominal_mode,
     because it will have been set to Pmode above, we must use the actual mode
     of the parameter instead.  */
  if (data->arg.pass_by_reference && TYPE_MODE (TREE_TYPE (parm)) != mtcsMode->modes.M_BLKmode){
      /* Use a stack slot for debugging purposes if possible.  */
      if (mtcs_func_use_register_for_decl/*!use_register_for_decl*/(self,parm)){
          parmreg = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,TYPE_MODE (TREE_TYPE (parm)));
          mark_user_reg (parmreg);
      }else{
          int align = mtcs_align_get_stack_slot_alignment/*!STACK_SLOT_ALIGNMENT*/(mtcsAlign,TREE_TYPE (parm),
                            TYPE_MODE (TREE_TYPE (parm)), TYPE_ALIGN (TREE_TYPE (parm)));
          parmreg = mtcs_func_assign_stack_local/*!assign_stack_local*/(self,TYPE_MODE (TREE_TYPE (parm)),
                      mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,TYPE_MODE (TREE_TYPE (parm))), align);
          mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,parmreg, parm, 1);
      }

      /* We need to preserve an address based on VIRTUAL_STACK_VARS_REGNUM for
     the debug info in case it is not legitimate.  */
      if (GET_MODE (parmreg) != GET_MODE (rtl)){
          rtx tempreg = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (rtl));
          int unsigned_p = TYPE_UNSIGNED (TREE_TYPE (parm));

          mtcs_emit_push_to_sequence2/*!push_to_sequence2*/(mtcsEmit,all->first_conversion_insn,
                     all->last_conversion_insn);
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,tempreg, rtl);
          tempreg =  mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,GET_MODE (parmreg), tempreg, unsigned_p);
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,
                  MEM_P (parmreg) ? copy_rtx (parmreg) : parmreg,tempreg);
          all->first_conversion_insn =mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
          all->last_conversion_insn = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
          mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);

          did_conversion = true;
      }else
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,MEM_P (parmreg) ? copy_rtx (parmreg) : parmreg, rtl);

      rtl = parmreg;

      /* STACK_PARM is the pointer, not the parm, and PARMREG is
       now the parm.  */
      data->stack_parm = NULL;
  }

  mtcs_expand_set_parm_rtl/*!set_parm_rtl*/(mtcsExpand,parm, rtl);

  /* Mark the register as eliminable if we did no conversion and it was
     copied from memory at a fixed offset, and the arg pointer was not
     copied to a pseudo-reg.  If the arg pointer is a pseudo reg or the
     offset formed an invalid address, such memory-equivalences as we
     make here would screw up life analysis for it.  */
  if (data->nominal_mode == data->passed_mode
      && !did_conversion
      && data->stack_parm != 0
      && MEM_P (data->stack_parm)
      && data->locate.offset.var == 0
      && reg_mentioned_p (mtcs_rtl_get_virtual_incoming_args_rtx/*!virtual_incoming_args_rtx*/(mtcsRTL),
              XEXP (data->stack_parm, 0))){
      rtx_insn *linsn = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
      rtx_insn *sinsn;
      rtx set;

      /* Mark complex types separately.  */
      if (GET_CODE (parmreg) == CONCAT){
          scalar_mode submode = mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,GET_MODE (parmreg));
          int regnor = REGNO (XEXP (parmreg, 0));
          int regnoi = REGNO (XEXP (parmreg, 1));
          rtx stackr = mtcs_rtl_adjust_address_nv/*!adjust_address_nv*/(mtcsRTL,data->stack_parm, submode, 0);
          rtx stacki = mtcs_rtl_adjust_address_nv/*!adjust_address_nv*/(mtcsRTL,
                  data->stack_parm, submode,mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,submode));

          /* Scan backwards for the set of the real and
             imaginary parts.  */
          for (sinsn = linsn; sinsn != 0; sinsn = prev_nonnote_insn (sinsn)){
              set = single_set (sinsn);
              if (set == 0)
                  continue;

              if (SET_DEST (set) == mtcsRtlData->regno_reg_rtx/*!regno_reg_rtx*/[regnoi])
                 mtcs_rtl_set_unique_reg_note/*!set_unique_reg_note*/(mtcsRTL,sinsn, REG_EQUIV, stacki);
              else if (SET_DEST (set) == mtcsRtlData->regno_reg_rtx/*!regno_reg_rtx*/[regnor])
                 mtcs_rtl_set_unique_reg_note/*!set_unique_reg_note*/(mtcsRTL,sinsn, REG_EQUIV, stackr);
          }
      }else
         mtcs_rtl_set_dst_reg_note/*!set_dst_reg_note*/(mtcsRTL,linsn, REG_EQUIV, equiv_stack_parm, parmreg);
  }

  /* For pointer data type, suggest pointer register.  */
  if (POINTER_TYPE_P (TREE_TYPE (parm)))
      mtcs_rtl_mark_reg_pointer/*!mark_reg_pointer*/(mtcsRTL,parmreg,TYPE_ALIGN (TREE_TYPE (TREE_TYPE (parm))));
}

/* A subroutine of assign_parms.  Allocate stack space to hold the current
   parameter.  Get it there.  Perform all ABI specified conversions.  */
//原型 assign_parm_setup_stack function.cc
static void assign_parm_setup_stack (MtcsFunc *self,struct assign_parm_data_all *all, tree parm,
                 struct assign_parm_data_one *data)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsExpand *mtcsExpand=mtcs_target_get_expand(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);

  /* Value must be stored in the stack slot STACK_PARM during function
     execution.  */
  bool to_conversion = false;
  n_debug("mtcsfunc.c assign_parm_setup_stack 00 \n");

  assign_parm_remove_parallels(self,data);
  n_debug("mtcsfunc.c assign_parm_setup_stack 11 \n");
  if (data->arg.mode != data->nominal_mode){
      /* Conversion is required.  */
      rtx tempreg = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (data->entry_parm));
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,tempreg,
              mtcs_explow_validize_mem/*!validize_mem*/(mtcsExplow,copy_rtx (data->entry_parm)));
      /* Some ABIs require scalar floating point modes to be passed
     in a wider scalar integer mode.  We need to explicitly
     truncate to an integer mode of the correct precision before
     using a SUBREG to reinterpret as a floating point value.  */
      if (mtcs_mode_is_scalar_float_p/*!SCALAR_FLOAT_MODE_P*/(mtcsMode,data->nominal_mode)
        && mtcs_mode_is_scalar_int_p/*!SCALAR_INT_MODE_P*/(mtcsMode,data->arg.mode)
        && known_lt (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,data->nominal_mode),
              mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,data->arg.mode)))
      tempreg = mtcs_expr_convert_wider_int_to_float/*!convert_wider_int_to_float*/(mtcsExpr,
            data->nominal_mode,data->arg.mode, tempreg);

      mtcs_emit_push_to_sequence2/*!push_to_sequence2*/(mtcsEmit,all->first_conversion_insn, all->last_conversion_insn);
      to_conversion = true;

      data->entry_parm = mtcs_expr_convert_to_mode/*!convert_to_mode*/(mtcsExpr,data->nominal_mode, tempreg,
                      TYPE_UNSIGNED (TREE_TYPE (parm)));

      if (data->stack_parm){
          poly_int64 offset  = mtcs_mode_subreg_lowpart_offset/*!subreg_lowpart_offset*/(mtcsMode,
                  data->nominal_mode,GET_MODE (data->stack_parm));
          /* ??? This may need a big-endian conversion on sparc64.  */
          data->stack_parm  = mtcs_rtl_adjust_address/*!adjust_address*/(mtcsRTL,data->stack_parm, data->nominal_mode, 0);
          if (maybe_ne (offset, 0) && mtcs_rtl_is_mem_offset_known_p/*!MEM_OFFSET_KNOWN_P*/(mtcsRTL,data->stack_parm))
              mtcs_rtl_set_mem_offset/*!set_mem_offset*/(mtcsRTL,
                      data->stack_parm, mtcs_rtl_get_mem_offset/*!MEM_OFFSET*/(mtcsRTL,data->stack_parm) + offset);
      }
  }
  n_debug("mtcsfunc.c assign_parm_setup_stack 22 \n");

  if (data->entry_parm != data->stack_parm){
      rtx src, dest;
      if (data->stack_parm == 0){
         n_debug("mtcsfunc.c assign_parm_setup_stack 33 \n");

          int align = mtcs_align_get_stack_slot_alignment/*!STACK_SLOT_ALIGNMENT*/(mtcsAlign,data->arg.type,
                            GET_MODE (data->entry_parm),
                            TYPE_ALIGN (data->arg.type));
          n_debug("mtcsfunc.c assign_parm_setup_stack 44 align:%d \n",align);

          if (align < (int)mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,GET_MODE (data->entry_parm))
              && ((mtcs_opinit_optab_handler/*!optab_handler*/(mtcsOpinit,movmisalign_optab,
                      GET_MODE (data->entry_parm)) != CODE_FOR_nothing)
              || mtcsTarget->slow_unaligned_access/*!targetm.slow_unaligned_access*/(mtcsTarget,
                      GET_MODE (data->entry_parm),align))){
             n_debug("mtcsfunc.c assign_parm_setup_stack 55 align:%d \n",align);

            align = mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,GET_MODE (data->entry_parm));
          }
          n_debug("mtcsfunc.c assign_parm_setup_stack 66 align:%d \n",align);

         data->stack_parm = mtcs_func_assign_stack_local/*!assign_stack_local*/(self,GET_MODE (data->entry_parm),
                 mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,GET_MODE (data->entry_parm)),align);
         n_debug("mtcsfunc.c assign_parm_setup_stack 66aa align:%d \n",align);

         align = mtcs_rtl_get_mem_align/*!MEM_ALIGN*/(mtcsRTL,data->stack_parm);
         n_debug("mtcsfunc.c assign_parm_setup_stack 66bb align:%d \n",align);

         mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,data->stack_parm, parm, 1);
         n_debug("mtcsfunc.c assign_parm_setup_stack 66cc align:%d \n",align);

         mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,data->stack_parm, align);
      }
      n_debug("mtcsfunc.c assign_parm_setup_stack 66dd  \n");

      dest = mtcs_explow_validize_mem/*!validize_mem*/(mtcsExplow,copy_rtx (data->stack_parm));
      n_debug("mtcsfunc.c assign_parm_setup_stack 66ee  \n");

      src = mtcs_explow_validize_mem/*!validize_mem*/(mtcsExplow,copy_rtx (data->entry_parm));
      n_debug("mtcsfunc.c assign_parm_setup_stack 77  \n");

      if (TYPE_EMPTY_P (data->arg.type))
    /* Empty types don't really need to be copied.  */;
      else if (MEM_P (src)){
          /* Use a block move to handle potentially misaligned entry_parm.  */
          if (!to_conversion)
              mtcs_emit_push_to_sequence2/*!push_to_sequence2*/(mtcsEmit,all->first_conversion_insn,
                       all->last_conversion_insn);
          to_conversion = true;
          mtcs_expr_emit_block_move/*!emit_block_move*/(mtcsExpr,dest, src,
                   GEN_INT (int_size_in_bytes (data->arg.type)), BLOCK_OP_NORMAL);
      }else{
          if (!REG_P (src))
            src = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,GET_MODE (src), src);
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,dest, src);
      }
  }
  n_debug("mtcsfunc.c assign_parm_setup_stack 88 \n");

  if (to_conversion){
      all->first_conversion_insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
      all->last_conversion_insn = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
  }
  n_debug("mtcsfunc.c assign_parm_setup_stack 99 \n");

  mtcs_expand_set_parm_rtl/*!set_parm_rtl*/(mtcsExpand,parm, data->stack_parm);
  n_debug("mtcsfunc.c assign_parm_setup_stack 100 \n");

}

/* A subroutine of assign_parms.  If the ABI splits complex arguments, then
   undo the frobbing that we did in assign_parms_augmented_arg_list.  */
//原型 assign_parms_unsplit_complex function.cc
static void assign_parms_unsplit_complex (MtcsFunc *self,struct assign_parm_data_all *all,
                  vec<tree> fnargs)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsExpand *mtcsExpand=mtcs_target_get_expand(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  tree parm;
  tree orig_fnargs = all->orig_fnargs;
  unsigned i = 0;

  for (parm = orig_fnargs; parm; parm = TREE_CHAIN (parm), ++i){
      if (TREE_CODE (TREE_TYPE (parm)) == COMPLEX_TYPE
        && target_calls_split_complex_arg/*!targetm.calls.split_complex_arg*/(mtcsMachine->calls,TREE_TYPE (parm))){
          rtx tmp, real, imag;
          scalar_mode inner = mtcs_mode_get_inner/*!GET_MODE_INNER*/(mtcsMode,DECL_MODE (parm));

          real = mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,fnargs[i]);
          imag = mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,fnargs[i + 1]);
          if (inner != GET_MODE (real)){
              real = mtcs_rtl_gen_lowpart_SUBREG/*!gen_lowpart_SUBREG*/(mtcsRTL,inner, real);
              imag = mtcs_rtl_gen_lowpart_SUBREG/*!gen_lowpart_SUBREG*/(mtcsRTL,inner, imag);
          }

          if (TREE_ADDRESSABLE (parm)){
              rtx rmem, imem;
              HOST_WIDE_INT size = int_size_in_bytes (TREE_TYPE (parm));
              int align =mtcs_align_get_stack_slot_alignment/*!STACK_SLOT_ALIGNMENT*/(mtcsAlign,TREE_TYPE (parm),
                            DECL_MODE (parm),TYPE_ALIGN (TREE_TYPE (parm)));

              /* split_complex_arg put the real and imag parts in
             pseudos.  Move them to memory.  */
              tmp = mtcs_func_assign_stack_local/*!assign_stack_local*/(self,DECL_MODE (parm), size, align);
              mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,tmp, parm, 1);
              rmem = mtcs_rtl_adjust_address_nv/*!adjust_address_nv*/(mtcsRTL,tmp, inner, 0);
              imem = mtcs_rtl_adjust_address_nv/*!adjust_address_nv*/(mtcsRTL,tmp, inner,
                          mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,inner));
              mtcs_emit_push_to_sequence2/*!push_to_sequence2*/(mtcsEmit,all->first_conversion_insn,
                     all->last_conversion_insn);
              mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,rmem, real);
              mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,imem, imag);
              all->first_conversion_insn =mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
              all->last_conversion_insn = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
              mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
          }else
              tmp = gen_rtx_CONCAT (DECL_MODE (parm), real, imag);

          mtcs_expand_set_parm_rtl/*!set_parm_rtl*/(mtcsExpand,parm, tmp);

          real = DECL_INCOMING_RTL (fnargs[i]);
          imag = DECL_INCOMING_RTL (fnargs[i + 1]);
          if (inner != GET_MODE (real)){
              real = mtcs_rtl_gen_lowpart_SUBREG/*!gen_lowpart_SUBREG*/(mtcsRTL,inner, real);
              imag = mtcs_rtl_gen_lowpart_SUBREG/*!gen_lowpart_SUBREG*/(mtcsRTL,inner, imag);
          }
          tmp = gen_rtx_CONCAT (DECL_MODE (parm), real, imag);
          mtcs_rtl_set_decl_incoming_rtl/*!set_decl_incoming_rtl*/(mtcsRTL,parm, tmp, false);
          i++;
      }
  }
}

/* Assign RTL expressions to the function's parameters.  This may involve
   copying them into registers and using those registers as the DECL_RTL.
+-------------------------------+ <- 高地址
|                               |
|  incoming stack arguments     | 这部分是caller函数栈的参数区也称 outgoing stack arguments
|                               | 对callee来说称为 incoming stack arguments
+-------------------------------+ <-- incoming stack pointer (aligned)/virtual_incoming_args_rtx/arg_pointer_rtx
*/
//1.确定所有命名参数的来源 2.命名参数的存储位置(本函数的栈区域和偏移) 3.处理不定参数存在本函数的什么区域
//原型 assign_parms function.cc
static void assign_parms (MtcsFunc *self,tree fndecl)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpmed *mtcsExpmed=mtcs_target_get_expmed(mtcsTarget);
  MtcsExpand *mtcsExpand=mtcs_target_get_expand(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  struct assign_parm_data_all all;
  tree parm;
  vec<tree> fnargs;
  unsigned i;
  const char *fnName=IDENTIFIER_POINTER(DECL_NAME(fndecl));
   //nvptx 调用的是 default_internal_arg_pointer 返回的是 virtual_incoming_args_rtx 这是caller的outgoing stack argument区的首地址
  mtcsRtlData->args.internal_arg_pointer = target_calls_internal_arg_pointer/*!targetm.calls.internal_arg_pointer*/(mtcsMachine->calls);

  assign_parms_initialize_all(self,&all);
  //本函数的所有命名参数存入 all 返回 vec
  fnargs = assign_parms_augmented_arg_list(self,&all);

  if (TYPE_NO_NAMED_ARGS_STDARG_P (TREE_TYPE (fndecl)) && fnargs.is_empty ()){
      struct assign_parm_data_one data = {};
      data.arg.mtcsMode=mtcsMode;
      assign_parms_setup_varargs(self,&all, &data, false);
  }

  FOR_EACH_VEC_ELT (fnargs, i, parm){
     char *pname=IDENTIFIER_POINTER(DECL_NAME(parm));

     n_debug("mtcsfunc.c assign_parms 00 遍历所有的命名参数 %s 的参数是:%s\n",fnName,pname);
      struct assign_parm_data_one data;
      data.arg.mtcsMode=mtcsMode;
      /* Extract the type of PARM; adjust it according to ABI.  */
      assign_parm_find_data_types(self,&all, parm, &data);
      /* Early out for errors and void parameters.  */
      if (data.passed_mode == VOIDmode){
          mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,parm, const0_rtx);
          DECL_INCOMING_RTL (parm) = mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,parm);
          continue;
      }
      /* Estimate stack alignment from parameter alignment.  */
      if (mtcs_func_is_support_stack_alignment/*!SUPPORTS_STACK_ALIGNMENT*/(self)){
          unsigned int align =target_calls_function_arg_boundary/*!targetm.calls.function_arg_boundary*/(mtcsMachine->calls,
                data.arg.mode,data.arg.type);
          n_debug("mtcsfunc.c assign_parms 11 %s %s align:%d SUPPORTS_STACK_ALIGNMENT:%d mode:%d\n",
                          fnName,pname,align,mtcs_func_is_support_stack_alignment/*!SUPPORTS_STACK_ALIGNMENT*/(self),data.arg.mode);
          align = mtcs_mode_get_mininum_alignment/*!MINIMUM_ALIGNMENT*/(mtcsMode,data.arg.type, data.arg.mode, align);
          if (TYPE_ALIGN (data.nominal_type) > align){
             n_debug("mtcsfunc.c assign_parms 22 %s %s TYPE_ALIGN (data.nominal_type):%d align:%d\n",
                           fnName,pname,TYPE_ALIGN (data.nominal_type),align);
            align = mtcs_mode_get_mininum_alignment/*!MINIMUM_ALIGNMENT*/(mtcsMode,data.nominal_type,
                           TYPE_MODE (data.nominal_type),
                           TYPE_ALIGN (data.nominal_type));
            n_debug("mtcsfunc.c assign_parms 44 %s %s TYPE_MODE (data.nominal_type):%d align:%d\n",
                    fnName,pname,TYPE_MODE (data.nominal_type),align);
          }
          if (mtcsRtlData/*!crtl*/->stack_alignment_estimated < align){
             n_debug("mtcsfunc.c assign_parms 55 %s %s crtl->stack_alignment_estimated:%d align:%d\n",
                             fnName,pname,mtcsRtlData->stack_alignment_estimated,align);
              gcc_assert (!mtcsRtlData/*!ctrl*/->stack_realign_processed);
              mtcsRtlData/*!crtl*/->stack_alignment_estimated = align;
          }
      }
      /* Find out where the parameter arrives in this function. 找出参数到达该函数的那个位置 */
      assign_parm_find_entry_rtl(self,&all, &data);
      /* Find out where stack space for this parameter might be.  */
      if (assign_parm_is_stack_parm (self,&all, &data)){
         n_debug("mtcsfunc.c assign_parms 66 %s %s assign_parm_is_stack_parm=true\n",
                              fnName,pname);
          assign_parm_find_stack_rtl(self,parm, &data);
          assign_parm_adjust_entry_rtl(self,&data);
          /* For arguments that occupy no space in the parameter
             passing area, have non-zero size and have address taken,
             force creation of a stack slot so that they have distinct
             address from other parameters.  */
          if (TYPE_EMPTY_P (data.arg.type)
              && TREE_ADDRESSABLE (parm)
              && data.entry_parm == data.stack_parm
              && MEM_P (data.entry_parm)
              && int_size_in_bytes (data.arg.type)){
             n_debug("mtcsfunc.c assign_parms 77 %s %s \n",fnName,pname);
            data.stack_parm = NULL_RTX;
          }
      }
      /* Record permanently how this parm was passed.  */
      //在这里已确定了命名参数来源位置, 位置有寄存器和栈( incoming 也称outgoing)并记录在 DECL_INCOMING_RTL(parm)中
      if (data.arg.pass_by_reference){
         n_debug("mtcsfunc.c assign_parms 88 命名参数来源是栈 caller的outgoint stack argument %s %s data.arg.pass_by_reference\n",
               fnName,pname);
          rtx incoming_rtl = gen_rtx_MEM (TYPE_MODE (TREE_TYPE (data.arg.type)),data.entry_parm);
          mtcs_rtl_set_decl_incoming_rtl/*!set_decl_incoming_rtl*/(mtcsRTL,parm, incoming_rtl, true);
      }else{
         n_debug("mtcsfunc.c assign_parms 99 命名参数来源是寄存器 %s %s set_decl_incoming_rtl\n",fnName,pname);

          mtcs_rtl_set_decl_incoming_rtl/*!set_decl_incoming_rtl*/(mtcsRTL,parm, data.entry_parm, false);
      }

      assign_parm_adjust_stack_rtl(self,&data);
      n_debug("mtcsfunc.c assign_parms 100aa %d %d\n",
            data.arg.pass_by_reference ,mtcs_func_use_register_for_decl/*!use_register_for_decl*/(self,parm));

      if (assign_parm_setup_block_p(self,&data)){
         n_debug("mtcsfunc.c assign_parms 100 %s %s set_decl_incoming_rtl\n",fnName,pname);
          assign_parm_setup_block(self,&all, parm, &data);
      }else if (data.arg.pass_by_reference || mtcs_func_use_register_for_decl/*!use_register_for_decl*/(self,parm)){
         n_debug("mtcsfunc.c assign_parms 101 %s %s set_decl_incoming_rtl\n",fnName,pname);
          assign_parm_setup_reg(self,&all, parm, &data);
      }else{
         n_debug("mtcsfunc.c assign_parms 102 %s %s set_decl_incoming_rtl\n",fnName,pname);
          assign_parm_setup_stack(self,&all, parm, &data);
      }
      n_debug("mtcsfunc.c assign_parms 102aa %s %s set_decl_incoming_rtl %d %p self->currentFun:%p cfun:%p %d\n",
            fnName,pname,cfun->stdarg,DECL_CHAIN (parm),self->currentFun,cfun,self->currentFun->stdarg);
      //aet_print_tree(DECL_CHAIN (parm));
      if (cfun->stdarg && !DECL_CHAIN (parm)){
         n_debug("mtcsfunc.c assign_parms 103 可变参数存储位置 %s %s set_decl_incoming_rtl\n",fnName,pname);
          assign_parms_setup_varargs(self,&all, &data, false);
      }
      n_debug("mtcsfunc.c assign_parms 103aa 可变参数存储位置 %s %s set_decl_incoming_rtl\n",fnName,pname);

      /* Update info on where next arg arrives in registers.  */
      target_calls_function_arg_advance/*!targetm.calls.function_arg_advance*/(mtcsMachine->calls,all.args_so_far, data.arg);
  }
  if (mtcsMachine->calls->split_complex_arg/*!targetm.calls.split_complex_arg*/)
    assign_parms_unsplit_complex(self,&all, fnargs);

  fnargs.release ();

  /* Output all parameter conversion instructions (possibly including calls)
     now that all parameters have been copied out of hard registers.  */
  mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,all.first_conversion_insn);

  /* Estimate reload stack alignment from scalar return mode.  */
  if (mtcs_func_is_support_stack_alignment/*!SUPPORTS_STACK_ALIGNMENT*/(self)){
     n_debug("mtcsfunc.c assign_parms 104 %s  SUPPORTS_STACK_ALIGNMENT:%d\n",
           fnName,mtcs_func_is_support_stack_alignment/*!SUPPORTS_STACK_ALIGNMENT*/(self));

      if (DECL_RESULT (fndecl)){
          tree type = TREE_TYPE (DECL_RESULT (fndecl));
          machine_mode mode = TYPE_MODE (type);
          n_debug("mtcsfunc.c assign_parms 105 %s  mode:%d\n",fnName,mode);
          if (mode != mtcsMode->modes.M_BLKmode && mode != VOIDmode && !AGGREGATE_TYPE_P (type)){
              unsigned int align = mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode);
              n_debug("mtcsfunc.c assign_parms 105-- %s  mode:%d align:%d\n",fnName,mode,align);
              if (mtcsRtlData/*!crtl*/->stack_alignment_estimated < align){
                 n_debug("mtcsfunc.c assign_parms 106 %s  mode:%d align:%d %d\n",
                                     fnName,mode,align,mtcsRtlData->stack_alignment_estimated);
                  gcc_assert (!mtcsRtlData/*!crtl*/->stack_realign_processed);
                  mtcsRtlData/*!crtl*/->stack_alignment_estimated = align;
              }
          }
      }
  }

  /* If we are receiving a struct value address as the first argument, set up
     the RTL for the function result. As this might require code to convert
     the transmitted address to Pmode, we do this here to ensure that possible
     preliminary conversions of the address have been emitted already.  */
  if (all.function_result_decl){
     n_debug("mtcsfunc.c assign_parms 107 %s all.function_result_decl\n",fnName);
      tree result = DECL_RESULT (current_function_decl);
      rtx addr = mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,all.function_result_decl);
      rtx x;

      if (DECL_BY_REFERENCE (result)){
         n_debug("mtcsfunc.c assign_parms 108 %s all.function_result_decl DECL_BY_REFERENCE\n",fnName);
          SET_DECL_VALUE_EXPR (result, all.function_result_decl);
          x = addr;
      }else{
         n_debug("mtcsfunc.c assign_parms 109 %s all.function_result_decl \n",fnName);
          SET_DECL_VALUE_EXPR (result,
                       build1 (INDIRECT_REF, TREE_TYPE (result),
                           all.function_result_decl));
          addr = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,
                  mtcs_mode_get_Pmode(mtcsMode), addr);
          x = gen_rtx_MEM (DECL_MODE (result), addr);
          mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,x, result, 1);
      }

      DECL_HAS_VALUE_EXPR_P (result) = 1;
      mtcs_expand_set_parm_rtl/*!set_parm_rtl*/(mtcsExpand,result, x);
  }

  /* We have aligned all the args, so add space for the pretend args.  */
  mtcsRtlData/*!crtl*/->args.pretend_args_size = all.pretend_args_size;
  all.stack_args_size.constant += all.extra_pretend_bytes;
  mtcsRtlData/*!crtl*/->args.size = all.stack_args_size.constant;

  /* Adjust function incoming argument size for alignment and
     minimum length.  */

  mtcsRtlData/*!crtl*/->args.size = upper_bound (mtcsRtlData/*!crtl*/->args.size, all.reg_parm_stack_space);
  mtcsRtlData/*!crtl*/->args.size = aligned_upper_bound (mtcsRtlData/*!crtl*/->args.size,
          mtcs_func_get_parm_boundary/*!PARM_BOUNDARY*/(self) / BITS_PER_UNIT);

  if (mtcs_func_get_args_grows_downward/*!ARGS_GROW_DOWNWARD*/(self)){
     n_debug("mtcsfunc.c assign_parms 109 %s ARGS_GROW_DOWNWARD:%d\n",
           fnName,mtcs_func_get_args_grows_downward/*!ARGS_GROW_DOWNWARD*/(self));

      mtcsRtlData/*!crtl*/->args.arg_offset_rtx = (all.stack_args_size.var == 0
       ? mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,-all.stack_args_size.constant,mtcs_mode_get_Pmode(mtcsMode))
       : mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,size_diffop (all.stack_args_size.var,
                       size_int (-all.stack_args_size.constant)),
              NULL_RTX, VOIDmode, EXPAND_NORMAL));
  }else{
     n_debug("mtcsfunc.c assign_parms 110 %s ARGS_GROW_DOWNWARD:%d\n",
           fnName,mtcs_func_get_args_grows_downward/*!ARGS_GROW_DOWNWARD*/(self));

      mtcsRtlData/*!crtl*/->args.arg_offset_rtx = mtcs_func_args_size_rtx/*!ARGS_SIZE_RTX*/(self,all.stack_args_size);
  }

  /* See how many bytes, if any, of its args a function should try to pop
     on return.  */

  mtcsRtlData/*!crtl*/->args.pops_args = target_calls_return_pops_args/*!targetm.calls.return_pops_args*/(mtcsMachine->calls,
                                                  fndecl,TREE_TYPE (fndecl),mtcsRtlData/*!crtl*/->args.size);

  /* For stdarg.h function, save info about
     regs and stack space used by the named args.  */

  mtcsRtlData/*!crtl*/->args.info =(void*) all.args_so_far_v;

  /* Set the rtx used for the function return value.  Put this in its
     own variable so any optimizers that need this information don't have
     to include tree.h.  Do this here so it gets done when an inlined
     function gets output.  */

  mtcsRtlData/*!crtl*/->return_rtx = (DECL_RTL_SET_P (DECL_RESULT (fndecl))
       ? mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,DECL_RESULT (fndecl)) : NULL_RTX);

  /* If scalar return value was computed in a pseudo-reg, or was a named
     return value that got dumped to the stack, copy that to the hard
     return register.  */
  if (DECL_RTL_SET_P (DECL_RESULT (fndecl))){
     n_debug("mtcsfunc.c assign_parms 111 %s DECL_RTL_SET_P (DECL_RESULT (fndecl))\n",fnName);

      tree decl_result = DECL_RESULT (fndecl);
      rtx decl_rtl = mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,decl_result);
      if (REG_P (decl_rtl)
        ? REGNO (decl_rtl) >= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)
        : DECL_REGISTER (decl_result)){
          rtx real_decl_rtl;

          /* Unless the psABI says not to.  */
          if (TYPE_EMPTY_P (TREE_TYPE (decl_result)))
              real_decl_rtl = NULL_RTX;
          else{
              real_decl_rtl = target_calls_function_value/*!targetm.calls.function_value*/(mtcsMachine->calls,
                      TREE_TYPE (decl_result),fndecl, true);
              REG_FUNCTION_VALUE_P (real_decl_rtl) = 1;
          }
          /* The delay slot scheduler assumes that crtl->return_rtx
             holds the hard register containing the return value, not a
             temporary pseudo.  */
          mtcsRtlData/*!crtl*/->return_rtx = real_decl_rtl;
      }
  }
  n_debug("mtcsfunc.c assign_parms 112 %s 结束\n",fnName);

}


/* Compute the size and offset from the start of the stacked arguments for a
   parm passed in mode PASSED_MODE and with type TYPE.

   INITIAL_OFFSET_PTR points to the current offset into the stacked
   arguments.

   The starting offset and size for this parm are returned in
   LOCATE->OFFSET and LOCATE->SIZE, respectively.  When IN_REGS is
   nonzero, the offset is that of stack slot, which is returned in
   LOCATE->SLOT_OFFSET.  LOCATE->ALIGNMENT_PAD is the amount of
   padding required from the initial offset ptr to the stack slot.

   IN_REGS is nonzero if the argument will be passed in registers.  It will
   never be set if REG_PARM_STACK_SPACE is not defined.

   REG_PARM_STACK_SPACE is the number of bytes of stack space reserved
   for arguments which are passed in registers.

   FNDECL is the function in which the argument was defined.

   There are two types of rounding that are done.  The first, controlled by
   TARGET_FUNCTION_ARG_BOUNDARY, forces the offset from the start of the
   argument list to be aligned to the specific boundary (in bits).  This
   rounding affects the initial and starting offsets, but not the argument
   size.

   The second, controlled by TARGET_FUNCTION_ARG_PADDING and PARM_BOUNDARY,
   optionally rounds the size of the parm to PARM_BOUNDARY.  The
   initial offset is not affected by this rounding, while the size always
   is and the starting offset may be.  */

/*  LOCATE->OFFSET will be negative for ARGS_GROW_DOWNWARD case;
    INITIAL_OFFSET_PTR is positive because locate_and_pad_parm's
    callers pass in the total size of args so far as
    INITIAL_OFFSET_PTR.  LOCATE->SIZE is always positive.  */
//原型 locate_and_pad_parm function.h function.cc
void mtcs_func_locate_and_pad_parm (MtcsFunc *self,machine_mode passed_mode, tree type, int in_regs,int reg_parm_stack_space,
        int partial,tree fndecl ATTRIBUTE_UNUSED,struct args_size *initial_offset_ptr,struct locate_and_pad_arg_data *locate)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsReg   *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

  tree sizetree;
  pad_direction where_pad;
  unsigned int boundary, round_boundary;
  int part_size_in_regs;
  n_debug("mtcsfunc.c  mtcs_func_locate_and_pad_parm 00 求栈参数的大小及offset"
        " mode:%d,in_regs：%d reg_parm_stack_space:%d\n",passed_mode,in_regs,reg_parm_stack_space);
  /* If we have found a stack parm before we reach the end of the
     area reserved for registers, skip that area.  */
  if (! in_regs){
      if (reg_parm_stack_space > 0){
          if (initial_offset_ptr->var || !ordered_p (initial_offset_ptr->constant,reg_parm_stack_space)){
              initial_offset_ptr->var = mtcs_const_size_binop/*!size_binop*/(mtcsConst,MAX_EXPR,
                    mtcs_func_args_size_tree/*!ARGS_SIZE_TREE*/(self,*initial_offset_ptr),ssize_int (reg_parm_stack_space));
              initial_offset_ptr->constant = 0;
          }else
            initial_offset_ptr->constant = ordered_max (initial_offset_ptr->constant,reg_parm_stack_space);
          }
      }

      part_size_in_regs = (reg_parm_stack_space == 0 ? partial : 0);

      sizetree = (type? arg_size_in_bytes (type): size_int (mtcs_mode_get_size(mtcsMode,passed_mode)));
      where_pad = target_calls_function_arg_padding/*!targetm.calls.function_arg_padding*/(mtcsMachine->calls,passed_mode, type);
      boundary =target_calls_function_arg_boundary/*!targetm.calls.function_arg_boundary*/(mtcsMachine->calls,passed_mode, type);
      round_boundary = target_calls_function_arg_round_boundary/*!targetm.calls.function_arg_round_boundary*/(mtcsMachine->calls,
                                    passed_mode,type);
      locate->where_pad = where_pad;

      /* Alignment can't exceed MAX_SUPPORTED_STACK_ALIGNMENT.  */
      if (boundary > mtcs_func_get_max_support_stack_alignment/*!MAX_SUPPORTED_STACK_ALIGNMENT*/(self))
        boundary = mtcs_func_get_max_support_stack_alignment/*!MAX_SUPPORTED_STACK_ALIGNMENT*/(self);

      locate->boundary = boundary;

      if (mtcs_func_is_support_stack_alignment/*SUPPORTS_STACK_ALIGNMENT*/(self)){
          /* stack_alignment_estimated can't change after stack has been
         realigned.  */
          if (mtcsRtlData->stack_alignment_estimated < boundary){
              if (!mtcsRtlData->stack_realign_processed)
                  mtcsRtlData->stack_alignment_estimated = boundary;
          else{
              /* If stack is realigned and stack alignment value
             hasn't been finalized, it is OK not to increase
             stack_alignment_estimated.  The bigger alignment
             requirement is recorded in stack_alignment_needed
             below.  */
              gcc_assert (!mtcsRtlData->stack_realign_finalized && mtcsRtlData->stack_realign_needed);
         }
      }
  }

  if (mtcs_func_get_args_grows_downward/*!ARGS_GROW_DOWNWARD*/(self)){
      locate->slot_offset.constant = -initial_offset_ptr->constant;
      if (initial_offset_ptr->var)
          locate->slot_offset.var = mtcs_const_size_binop/*!size_binop*/(mtcsConst,MINUS_EXPR, ssize_int (0),initial_offset_ptr->var);

      {
        tree s2 = sizetree;
        if (where_pad != PAD_NONE   && (!tree_fits_uhwi_p (sizetree)|| (tree_to_uhwi (sizetree) * BITS_PER_UNIT) % round_boundary))
          s2 = round_up (s2, round_boundary / BITS_PER_UNIT);
        mtcs_func_sub_parm_size/*!SUB_PARM_SIZE*/(self,&locate->slot_offset, s2);
      }
      locate->slot_offset.constant += part_size_in_regs;

      if (!in_regs || reg_parm_stack_space > 0)
          pad_to_arg_alignment (self,&locate->slot_offset, boundary,&locate->alignment_pad);

      locate->size.constant = (-initial_offset_ptr->constant - locate->slot_offset.constant);
      if (initial_offset_ptr->var)
          locate->size.var = mtcs_const_size_binop/*!size_binop*/(mtcsConst,MINUS_EXPR,
                mtcs_const_size_binop/*!size_binop*/(mtcsConst,MINUS_EXPR,ssize_int (0),
                      initial_offset_ptr->var),locate->slot_offset.var);

      /* Pad_below needs the pre-rounded size to know how much to pad
     below.  */
      locate->offset = locate->slot_offset;
      if (where_pad == PAD_DOWNWARD)
          pad_below (self,&locate->offset, passed_mode, sizetree);
  }else{
      if (!in_regs || reg_parm_stack_space > 0)
          pad_to_arg_alignment (self,initial_offset_ptr, boundary,&locate->alignment_pad);
      locate->slot_offset = *initial_offset_ptr;

//#ifdef PUSH_ROUNDING //host=1 nvptx=0
//      if (passed_mode != mtcsMode->modes.M_BLKmode)
//         sizetree = size_int (PUSH_ROUNDING (TREE_INT_CST_LOW (sizetree)));
//#endif

      /* Pad_below needs the pre-rounded size to know how much to pad below
     so this must be done before rounding up.  */
      locate->offset = locate->slot_offset;
      if (where_pad == PAD_DOWNWARD)
          pad_below (self,&locate->offset, passed_mode, sizetree);

      if (where_pad != PAD_NONE  && (!tree_fits_uhwi_p (sizetree) || (tree_to_uhwi (sizetree) * BITS_PER_UNIT) % round_boundary))
          sizetree = round_up (sizetree, round_boundary / BITS_PER_UNIT);

      mtcs_func_add_parm_size/*!ADD_PARM_SIZE*/(self,&locate->size, sizetree);
      n_debug("mtcsfunc.c  mtcs_func_locate_and_pad_parm 11 ADD_PARM_SIZE 部分在寄存器"
            " part_size_in_regs:%d,locate->size.constant：%d \n",part_size_in_regs,locate->size.constant);
      locate->size.constant -= part_size_in_regs;
  }
  locate->offset.constant +=
        target_calls_function_arg_offset/*!targetm.calls.function_arg_offset*/(mtcsMachine->calls,passed_mode, type);
}


void mtcs_func_init(MtcsFunc *self)
{
     self->accumulate_outgoing_args=FALSE;
     self->currentFun=NULL;
     mtcsFuncInit(self);
}

/* Generate RTL for the start of the function SUBR (a FUNCTION_DECL tree node)
   and initialize static variables for generating RTL for the statements
   of the function.  */
//原型 init_function_start function.h  function.c
void mtcs_func_init_function_start (MtcsFunc *self,tree subr)
{
  /* Initialize backend, if needed.  */
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsAsm  *mtcsAsm=mtcs_target_get_asm(mtcsTarget);

  n_debug("mtcsfunc.c init_function_start 00\n");
  mtcs_rtl_initialize_rtl/*!initialize_rtl ();*/(mtcsRTL);
  n_debug("mtcsfunc.c init_function_start 11\n");
  prepare_function_start(self);
  n_debug("mtcsfunc.c init_function_start 22\n");
  mtcs_asm_decide_function_section/*!decide_function_section*/(mtcsAsm,subr);

  /* Warn if this value is an aggregate type,
     regardless of which calling convention we are using for it.  */
  if (AGGREGATE_TYPE_P (TREE_TYPE (DECL_RESULT (subr))))
    warning_at (DECL_SOURCE_LOCATION (DECL_RESULT (subr)),OPT_Waggregate_return, "function returns an aggregate");
}

/* Allocate a temporary stack slot and record it for possible later
   reuse.

   MODE is the machine mode to be given to the returned rtx.

   SIZE is the size in units of the space required.  We do no rounding here
   since assign_stack_local will do any required rounding.

   TYPE is the type that will be used for the stack slot.  */
//原型 assign_stack_temp_for_type function.h function.cc
rtx mtcs_func_assign_stack_temp_for_type (MtcsFunc *self,machine_mode mode, poly_int64 size, tree type)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  MtcsAlign  *mtcsAlign=mtcs_target_get_align(mtcsTarget);
  MtcsAlias *mtcsAlias=mtcs_target_get_alias(mtcsTarget);

  unsigned int align;
  class mtcs_temp_slot *p, *best_p = 0, *selected = NULL, **pp;
  rtx slot;

  gcc_assert (known_size_p (size));
  n_debug("mtcsfunc.c  mtcs_func_assign_stack_temp_for_type %d\n",mode);
  aet_print_tree(type);
  align = get_stack_local_alignment (self,type, mode);

  /* Try to find an available, already-allocated temporary of the proper
     mode which meets the size and alignment requirements.  Choose the
     smallest one with the closest alignment.

     If assign_stack_temp is called outside of the tree->rtl expansion,
     we cannot reuse the stack slots (that may still refer to
     VIRTUAL_STACK_VARS_REGNUM).  */
  if (!self->virtuals_instantiated){
      for (p = mtcsRtlData/*!avail_temp_slots*/->x_avail_temp_slots; p; p = p->next){
          if (p->align >= align
              && known_ge (p->size, size)
              && GET_MODE (p->slot) == mode
              && mtcs_alias_objects_must_conflict_p/*!objects_must_conflict_p*/(mtcsAlias,p->type, type)
              && (best_p == 0
              || (known_eq (best_p->size, p->size)
                  ? best_p->align > p->align
                  : known_ge (best_p->size, p->size)))){
              if (p->align == align && known_eq (p->size, size)){
                  selected = p;
                  cut_slot_from_list (selected, &mtcsRtlData/*!avail_temp_slots*/->x_avail_temp_slots);
                  best_p = 0;
                  break;
              }
              best_p = p;
          }
      }
  }

  /* Make our best, if any, the one to use.  */
  if (best_p){
      selected = best_p;
      cut_slot_from_list(selected, &mtcsRtlData/*!avail_temp_slots*/->x_avail_temp_slots);
      /* If there are enough aligned bytes left over, make them into a new
     temp_slot so that the extra bytes don't get wasted.  Do this only
     for BLKmode slots, so that we can be sure of the alignment.  */
      if (GET_MODE (best_p->slot) == mtcsMode->modes.M_BLKmode){
          int alignment = best_p->align / BITS_PER_UNIT;
          poly_int64 rounded_size = aligned_upper_bound (size, alignment);

          if (known_ge (best_p->size - rounded_size, alignment)){
              p = ggc_alloc<mtcs_temp_slot> ();
              p->in_use = false;
              p->size = best_p->size - rounded_size;
              p->base_offset = best_p->base_offset + rounded_size;
              p->full_size = best_p->full_size - rounded_size;
              p->slot = mtcs_rtl_adjust_address_nv/*!adjust_address_nv*/(mtcsRTL,
                    best_p->slot, mtcsMode->modes.M_BLKmode, rounded_size);
              p->align = best_p->align;
              p->type = best_p->type;
              insert_slot_to_list(p, &mtcsRtlData/*!avail_temp_slots*/->x_avail_temp_slots);
              vec_safe_push (mtcsRtlData->x_stack_slot_list/*!stack_slot_list*/, p->slot);
              best_p->size = rounded_size;
              best_p->full_size = rounded_size;
           }
      }
  }

  /* If we still didn't find one, make a new temporary.  */
  if (selected == 0){
      poly_int64 frame_offset_old = mtcsRtlData/*!frame_offset*/->x_frame_offset;
      p = ggc_alloc<mtcs_temp_slot> ();

      /* We are passing an explicit alignment request to assign_stack_local.
     One side effect of that is assign_stack_local will not round SIZE
     to ensure the frame offset remains suitably aligned.

     So for requests which depended on the rounding of SIZE, we go ahead
     and round it now.  We also make sure ALIGNMENT is at least
     BIGGEST_ALIGNMENT.  */
      gcc_assert (mode != mtcsMode->modes.M_BLKmode || align == mtcs_align_get_biggest_alignment/*!BIGGEST_ALIGNMENT*/(mtcsAlign));
      p->slot = mtcs_func_assign_stack_local_1(self,mode,
              (mode == mtcsMode->modes.M_BLKmode ? aligned_upper_bound (size,(int) align/BITS_PER_UNIT): size),align, 0);
      p->align = align;

      /* The following slot size computation is necessary because we don't
     know the actual size of the temporary slot until assign_stack_local
     has performed all the frame alignment and size rounding for the
     requested temporary.  Note that extra space added for alignment
     can be either above or below this stack slot depending on which
     way the frame grows.  We include the extra space if and only if it
     is above this slot.  */
      if (mtcs_func_get_frame_grows_downward(self)/*!FRAME_GROWS_DOWNWARD*/)
          p->size = frame_offset_old - mtcsRtlData/*!frame_offset*/->x_frame_offset;
      else
          p->size = size;

      /* Now define the fields used by combine_temp_slots.  */
      if (mtcs_func_get_frame_grows_downward(self)/*!FRAME_GROWS_DOWNWARD*/){
          p->base_offset = mtcsRtlData/*!frame_offset*/->x_frame_offset;
          p->full_size = frame_offset_old - mtcsRtlData/*!frame_offset*/->x_frame_offset;
      }else{
          p->base_offset = frame_offset_old;
          p->full_size = mtcsRtlData/*!frame_offset*/->x_frame_offset - frame_offset_old;
      }
      selected = p;
  }

  p = selected;
  p->in_use = true;
  p->type = type;
  p->level =self->mtcsRtlData->x_temp_slot_level;
  self->n_temp_slots_in_use++;

  pp = temp_slots_at_level(self,p->level);
  insert_slot_to_list (p, pp);
  insert_temp_slot_address(self,XEXP (p->slot, 0), p);

  /* Create a new MEM rtx to avoid clobbering MEM flags of old slots.  */
  slot = gen_rtx_MEM (mode, XEXP (p->slot, 0));
  vec_safe_push (mtcsRtlData->x_stack_slot_list/*!stack_slot_list*/, slot);

  /* If we know the alias set for the memory that will be used, use
     it.  If there's no TYPE, then we don't know anything about the
     alias set for the memory.  */
  mtcs_rtl_set_mem_alias_set/*!set_mem_alias_set*/(mtcsRTL,slot,
        type ? mtcs_alias_get_alias_set/*!get_alias_set*/(mtcsAlias,type) : 0);
  mtcs_rtl_set_mem_align/*!set_mem_align*/(mtcsRTL,slot, align);

  /* If a type is specified, set the relevant flags.  */
  if (type != 0)
    MEM_VOLATILE_P (slot) = TYPE_VOLATILE (type);
  MEM_NOTRAP_P (slot) = 1;
  return slot;
}

/* Allocate a temporary stack slot and record it for possible later
   reuse.  First two arguments are same as in preceding function.  */
//原型 assign_stack_temp function.h function.cc
rtx mtcs_func_assign_stack_temp (MtcsFunc *self,machine_mode mode, poly_int64 size)
{
  return mtcs_func_assign_stack_temp_for_type (self,mode, size, NULL_TREE);
}

/* Allocate a stack slot of SIZE bytes and return a MEM rtx for it
   with machine mode MODE.

   ALIGN controls the amount of alignment for the address of the slot:
   0 means according to MODE,
   -1 means use BIGGEST_ALIGNMENT and round size to multiple of that,
   -2 means use BITS_PER_UNIT,
   positive specifies alignment boundary in bits.

   KIND has ASLK_REDUCE_ALIGN bit set if it is OK to reduce
   alignment and ASLK_RECORD_PAD bit set if we should remember
   extra space we allocated for alignment purposes.  When we are
   called from assign_stack_temp_for_type, it is not set so we don't
   track the same stack slot in two independent lists.

   We do not round to stack_boundary here.  */
//原型 assign_stack_local_1 function.h function.cc
rtx mtcs_func_assign_stack_local_1 (MtcsFunc *self,machine_mode mode, poly_int64 size,int align, int kind)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  MtcsAlign  *mtcsAlign=mtcs_target_get_align(mtcsTarget);

  int maxSupportStackAlignment= mtcs_func_get_max_support_stack_alignment(self);//MAX_SUPPORTED_STACK_ALIGNMENT
  machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
  rtx x, addr;
  poly_int64 bigend_correction = 0;
  poly_int64 slot_offset = 0, old_frame_offset;
  unsigned int alignment, alignment_in_bits;
   n_debug("mtcsfunc.c mtcs_func_assign_stack_local_1 00 mode:%d size:%d align:%d kind:%d\n",mode,size,align,kind);
  if (align == 0){
      alignment = get_stack_local_alignment (self,NULL, mode);
      alignment /= BITS_PER_UNIT;
  }else if (align == -1){
      alignment = mtcs_align_get_biggest_alignment/*!BIGGEST_ALIGNMENT*/(mtcsAlign) / BITS_PER_UNIT;
      size = aligned_upper_bound (size, alignment);
  }else if (align == -2)
    alignment = 1; /* BITS_PER_UNIT / BITS_PER_UNIT */
  else
    alignment = align / BITS_PER_UNIT;

  alignment_in_bits = alignment * BITS_PER_UNIT;

  /* Ignore alignment if it exceeds MAX_SUPPORTED_STACK_ALIGNMENT.  */
  if (alignment_in_bits >maxSupportStackAlignment/*!MAX_SUPPORTED_STACK_ALIGNMENT*/){
      alignment_in_bits = maxSupportStackAlignment/*!MAX_SUPPORTED_STACK_ALIGNMENT*/;
      alignment = maxSupportStackAlignment/*!MAX_SUPPORTED_STACK_ALIGNMENT*/ / BITS_PER_UNIT;
  }

  if (mtcs_func_is_support_stack_alignment(self)/*!SUPPORTS_STACK_ALIGNMENT*/){
     n_debug("mtcsfunc.c mtcs_func_assign_stack_local_1 11aa mode:%d size:%d align:%d kind:%d\n",mode,size,align,kind);

      if (mtcsRtlData->stack_alignment_estimated < alignment_in_bits){
          if (!mtcsRtlData->stack_realign_processed)
              mtcsRtlData->stack_alignment_estimated = alignment_in_bits;
          else{
              /* If stack is realigned and stack alignment value
             hasn't been finalized, it is OK not to increase
             stack_alignment_estimated.  The bigger alignment
             requirement is recorded in stack_alignment_needed
             below.  */
              gcc_assert (!mtcsRtlData->stack_realign_finalized);
              if (!mtcsRtlData->stack_realign_needed){
                  /* It is OK to reduce the alignment as long as the
                     requested size is 0 or the estimated stack
                     alignment >= mode alignment.  */
                  gcc_assert ((kind & ASLK_REDUCE_ALIGN)
                          || known_eq (size, 0)
                          || (mtcsRtlData->stack_alignment_estimated
                          >=mtcs_mode_get_alignment/*!GET_MODE_ALIGNMENT*/(mtcsMode,mode)));
                  alignment_in_bits = mtcsRtlData->stack_alignment_estimated;
                  alignment = alignment_in_bits / BITS_PER_UNIT;
             }
          }
      }
  }
  n_debug("mtcsfunc.c mtcs_func_assign_stack_local_1 11 mode:%d size:%d align:%d kind:%d\n",mode,size,align,kind);

  if (mtcsRtlData->stack_alignment_needed < alignment_in_bits)
      mtcsRtlData->stack_alignment_needed = alignment_in_bits;
  if (mtcsRtlData->max_used_stack_slot_alignment < alignment_in_bits)
      mtcsRtlData->max_used_stack_slot_alignment = alignment_in_bits;

  if (mode != mtcsMode->modes.M_BLKmode || maybe_ne (size, 0)){
     n_debug("mtcsfunc.c mtcs_func_assign_stack_local_1 11bb mode:%d size:%d align:%d kind:%d slot_offset:%d \n",
                mode,size,align,kind,slot_offset);
      if (kind & ASLK_RECORD_PAD){
         n_debug("mtcsfunc.c mtcs_func_assign_stack_local_1 11cc mode:%d size:%d align:%d kind:%d slot_offset:%d \n",
                    mode,size,align,kind,slot_offset);
          class frame_space **psp;
          for (psp = &mtcsRtlData->frame_space_list; *psp; psp = &(*psp)->next){
              class frame_space *space = *psp;
              n_debug("mtcsfunc.c mtcs_func_assign_stack_local_1 11dd mode:%d size:%d align:%d kind:%d slot_offset:%d\n",
                       mode,size,align,kind,slot_offset);
              if (!try_fit_stack_local(self,space->start, space->length, size,alignment, &slot_offset))
                  continue;
              *psp = space->next;
              if (known_gt (slot_offset, space->start))
                  add_frame_space(self,space->start, slot_offset);
              if (known_lt (slot_offset + size, space->start + space->length))
                  add_frame_space(self,slot_offset + size, space->start + space->length);
              n_debug("mtcsfunc.c mtcs_func_assign_stack_local_1 11ee mode:%d size:%d align:%d kind:%d slot_offset:%d\n",
                    mode,size,align,kind,slot_offset);

              goto found_space;
          }
      }
  }else if (!STACK_ALIGNMENT_NEEDED){
      slot_offset = mtcsRtlData/*!frame_offset*/->x_frame_offset;
      goto found_space;
  }
  n_debug("mtcsfunc.c mtcs_func_assign_stack_local_1 22 mode:%d size:%d align:%d kind:%d slot_offset:%d\n",mode,size,align,kind,slot_offset);

  old_frame_offset = mtcsRtlData/*!frame_offset*/->x_frame_offset;

  if (mtcs_func_get_frame_grows_downward/*!FRAME_GROWS_DOWNWARD*/(self)){
     mtcsRtlData/*!frame_offset*/->x_frame_offset -= size;
      try_fit_stack_local (self,mtcsRtlData/*!frame_offset*/->x_frame_offset, size, size, alignment, &slot_offset);
      n_debug("mtcsfunc.c mtcs_func_assign_stack_local_1 22aa mode:%d size:%d align:%d kind:%d slot_offset：%d\n",mode,size,align,kind,slot_offset);

      if (kind & ASLK_RECORD_PAD){
          if (known_gt (slot_offset, mtcsRtlData/*!frame_offset*/->x_frame_offset))
            add_frame_space(self,mtcsRtlData/*!frame_offset*/->x_frame_offset, slot_offset);
          if (known_lt (slot_offset + size, old_frame_offset))
            add_frame_space(self,slot_offset + size, old_frame_offset);
      }
  }else{
     mtcsRtlData/*!frame_offset*/->x_frame_offset += size;
      try_fit_stack_local(self,old_frame_offset, size, size, alignment, &slot_offset);

      if (kind & ASLK_RECORD_PAD){
          if (known_gt (slot_offset, old_frame_offset))
            add_frame_space(self,old_frame_offset, slot_offset);
          if (known_lt (slot_offset + size, mtcsRtlData/*!frame_offset*/->x_frame_offset))
            add_frame_space(self,slot_offset + size, mtcsRtlData/*!frame_offset*/->x_frame_offset);
      }
  }
  n_debug("mtcsfunc.c mtcs_func_assign_stack_local_1 33 mode:%d size:%d align:%d kind:%d\n",mode,size,align,kind);

 found_space:
  /* On a big-endian machine, if we are allocating more space than we will use,
     use the least significant bytes of those that are allocated.  */
  if (mode != mtcsMode->modes.M_BLKmode){
      /* The slot size can sometimes be smaller than the mode size;
     e.g. the rs6000 port allocates slots with a vector mode
     that have the size of only one element.  However, the slot
     size must always be ordered wrt to the mode size, in the
     same way as for a subreg.  */
      gcc_checking_assert (ordered_p (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode), size));
      if (BYTES_BIG_ENDIAN && maybe_lt (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode), size))
          bigend_correction = size - mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode);
  }
  n_debug("mtcsfunc.c mtcs_func_assign_stack_local_1 44 mode:%d size:%d align:%d kind:%d virtuals_instantiated:%d %d %d\n",
        mode,size,align,kind,self->virtuals_instantiated,slot_offset,bigend_correction);
  /* If we have already instantiated virtual registers, return the actual
     address relative to the frame pointer.  */
  if (self->virtuals_instantiated)
    addr = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,pMode, mtcs_rtl_get_frame_pointer_rtx/*!frame_pointer_rtx*/(mtcsRTL),
            mtcs_mode_trunc_int_for_mode(mtcsMode,slot_offset + bigend_correction
               + mtcsTarget->starting_frame_offset/*!targetm.starting_frame_offset*/(mtcsTarget), pMode));
  else
    addr = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,
          pMode, mtcs_rtl_get_virtual_stack_args_rtx/*!virtual_stack_vars_rtx*/(mtcsRTL),
            mtcs_mode_trunc_int_for_mode(mtcsMode,slot_offset + bigend_correction,pMode));

  x = gen_rtx_MEM (mode, addr);
  mtcs_rtl_set_mem_align (mtcsRTL,x, alignment_in_bits);
  MEM_NOTRAP_P (x) = 1;
  n_debug("mtcsfunc.c mtcs_func_assign_stack_local_1 55 mode:%d size:%d align:%d kind:%d\n",mode,size,align,kind);

  vec_safe_push (mtcsRtlData->x_stack_slot_list/*!stack_slot_list*/, x);

  if (mtcs_func_frame_offset_overflow/*!frame_offset_overflow*/(self,
        mtcsRtlData/*!frame_offset*/->x_frame_offset, current_function_decl))
     mtcsRtlData/*!frame_offset*/->x_frame_offset = 0;
  n_debug("mtcsfunc.c mtcs_func_assign_stack_local_1 66 mode:%d size:%d align:%d kind:%d\n",mode,size,align,kind);

  return x;
}

/* Wrap up assign_stack_local_1 with last parameter as false.  */
//原型 assign_stack_local function.h function.cc
rtx mtcs_func_assign_stack_local(MtcsFunc *self,machine_mode mode, poly_int64 size, int align)
{
  return mtcs_func_assign_stack_local_1(self,mode, size, align, ASLK_RECORD_PAD);
}

/* Issue an error message and return TRUE if frame OFFSET overflows in
   the signed target pointer arithmetics for function FUNC.  Otherwise
   return FALSE.  */
//原型 frame_offset_overflow function.h function.cc
bool mtcs_func_frame_offset_overflow(MtcsFunc *self,poly_int64 offset, tree func)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
  n_debug("mtcsfunc.c mtcs_func_frame_offset_overflow 00 %llu pmode:%d\n",offset,pMode);
  poly_uint64 size = mtcs_func_get_frame_grows_downward/*!FRAME_GROWS_DOWNWARD*/(self) ? -offset : offset;
  n_debug("mtcsfunc.c mtcs_func_frame_offset_overflow 11 %llu pmode:%d\n",size,pMode);

  unsigned HOST_WIDE_INT limit
    = ((HOST_WIDE_INT_1U << (mtcs_mode_get_size/*!GET_MODE_BITSIZE*/(mtcsMode,pMode) - 1))
       /* Leave room for the fixed part of the frame.  */
       - 64 * UNITS_PER_WORD);
  n_debug("mtcsfunc.c mtcs_func_frame_offset_overflow 22 %llu pmode:%d\n",limit,pMode);

  if (!coeffs_in_range_p (size, 0U, limit)){
      unsigned HOST_WIDE_INT hwisize;
      if (size.is_constant (&hwisize))
          error_at (DECL_SOURCE_LOCATION (func),"total size of local objects %wu exceeds maximum %wu",hwisize, limit);
      else
          error_at (DECL_SOURCE_LOCATION (func),"total size of local objects exceeds maximum %wu",limit);
      return true;
  }

  return false;
}

/* Assign a temporary.
   If TYPE_OR_DECL is a decl, then we are doing it on behalf of the decl
   and so that should be used in error messages.  In either case, we
   allocate of the given type.
   MEMORY_REQUIRED is 1 if the result must be addressable stack memory;
   it is 0 if a register is OK.
   DONT_PROMOTE is 1 if we should not promote values in register
   to wider modes.  */
//原型 assign_temp function.h functin.cc
//需要检查 PROMOTE_MODE在nvptx是否定义
rtx mtcs_func_assign_temp (MtcsFunc *self,tree type_or_decl, int memory_required,int dont_promote ATTRIBUTE_UNUSED)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);

  tree type, decl;
  machine_mode mode;
#ifdef PROMOTE_MODE
  int unsignedp;
#endif

  if (DECL_P (type_or_decl))
    decl = type_or_decl, type = TREE_TYPE (decl);
  else
    decl = NULL, type = type_or_decl;
  n_debug("mtcsfunc.c mtcs_func_assign_temp 000 DECL_P (type_or_decl):%d\n",DECL_P (type_or_decl));
  aet_print_tree(decl);
  aet_print_tree(type_or_decl);

  mode = TYPE_MODE (type);
#ifdef PROMOTE_MODE
  unsignedp = TYPE_UNSIGNED (type);
#endif

  /* Allocating temporaries of TREE_ADDRESSABLE type must be done in the front
     end.  See also create_tmp_var for the gimplification-time check.  */
  gcc_assert (!TREE_ADDRESSABLE (type) && COMPLETE_TYPE_P (type));

  if (mode == mtcsMode->modes.M_BLKmode || memory_required){
      poly_int64 size;
      rtx tmp;

      /* Unfortunately, we don't yet know how to allocate variable-sized
     temporaries.  However, sometimes we can find a fixed upper limit on
     the size, so try that instead.  */
      if (!poly_int_tree_p (TYPE_SIZE_UNIT (type), &size))
          size = max_int_size_in_bytes (type);

      /* Zero sized arrays are a GNU C extension.  Set size to 1 to avoid
     problems with allocating the stack space.  */
      if (known_eq (size, 0))
          size = 1;

      /* The size of the temporary may be too large to fit into an integer.  */
      /* ??? Not sure this should happen except for user silliness, so limit
     this to things that aren't compiler-generated temporaries.  The
     rest of the time we'll die in assign_stack_temp_for_type.  */
      if (decl  && !known_size_p (size)  && TREE_CODE (TYPE_SIZE_UNIT (type)) == INTEGER_CST){
          error ("size of variable %q+D is too large", decl);
          size = 1;
      }

      tmp = mtcs_func_assign_stack_temp_for_type(self,mode, size, type);
      return tmp;
  }

#ifdef PROMOTE_MODE
  if (! dont_promote)
    mode = mtcs_mode_promote_mode/*!promote_mode*/(mtcsMode,type, mode, &unsignedp);
#endif

  return mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mode);
}

/* Record a final call to CALLEE at LOCATION.  */
//原型 record_final_call function.h function.cc
void mtcs_func_record_final_call (MtcsFunc *self,tree callee, location_t location)
{
  struct callinfo_callee datum = { location, callee };
  //vec_safe_push (self->su->callees, datum);
  vec_safe_push (cfun->su->callees, datum);//改回cfun

}

/* Make temporary slot TEMP available.  */
//原型 make_slot_available function.cc
static void make_slot_available (MtcsFunc *self,class mtcs_temp_slot *temp)
{
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  cut_slot_from_list (temp, temp_slots_at_level (self,temp->level));
  insert_slot_to_list (temp, &mtcsRtlData->x_avail_temp_slots);
  temp->in_use = false;
  temp->level = -1;
  self->n_temp_slots_in_use--;
}

/* Remove an address -> temp slot mapping entry if the temp slot is
   not in use anymore.  Callback for remove_unused_temp_slot_addresses.  */
//原型  remove_unused_temp_slot_addresses_1 function.cc
int mtcs_func_remove_unused_temp_slot_addresses_1 (temp_slot_address_entry **slot, void *)
{
  const struct temp_slot_address_entry *t = *slot;
  MtcsFunc *self=t->self;
  if (! t->temp_slot->in_use)
    self->temp_slot_address_table->clear_slot (slot);
  return 1;
}

/* Remove all mappings of addresses to unused temp slots.  */
static void remove_unused_temp_slot_addresses (MtcsFunc *self)
{
  /* Use quicker clearing if there aren't any active temp slots.  */
  if (self->n_temp_slots_in_use)
    self->temp_slot_address_table->traverse
      <void *, mtcs_func_remove_unused_temp_slot_addresses_1/*!remove_unused_temp_slot_addresses_1*/> (NULL);
  else
    self->temp_slot_address_table->empty ();
}

/* Combine temporary stack slots which are adjacent on the stack.

   This allows for better use of already allocated stack space.  This is only
   done for BLKmode slots because we can be sure that we won't have alignment
   problems in this case.  */
//原型 combine_temp_slots function.cc
static void combine_temp_slots (MtcsFunc *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;

  class mtcs_temp_slot *p, *q, *next, *next_q;
  int num_slots;

  /* We can't combine slots, because the information about which slot
     is in which alias set will be lost.  */
  if (flag_strict_aliasing)
    return;

  /* If there are a lot of temp slots, don't do anything unless
     high levels of optimization.  */
  if (! flag_expensive_optimizations)
    for (p = mtcsRtlData/*!avail_temp_slots*/->x_avail_temp_slots, num_slots = 0; p; p = p->next, num_slots++)
      if (num_slots > 100 || (num_slots > 10 && optimize == 0))
    return;

  for (p = mtcsRtlData/*!avail_temp_slots*/->x_avail_temp_slots; p; p = next){
      int delete_p = 0;
      next = p->next;
      if (GET_MODE (p->slot) != mtcsMode->modes.M_BLKmode)
          continue;

      for (q = p->next; q; q = next_q){
          int delete_q = 0;
          next_q = q->next;
          if (GET_MODE (q->slot) != mtcsMode->modes.M_BLKmode)
            continue;

          if (known_eq (p->base_offset + p->full_size, q->base_offset)){
              /* Q comes after P; combine Q into P.  */
              p->size += q->size;
              p->full_size += q->full_size;
              delete_q = 1;
          }else if (known_eq (q->base_offset + q->full_size, p->base_offset)){
              /* P comes after Q; combine P into Q.  */
              q->size += p->size;
              q->full_size += p->full_size;
              delete_p = 1;
              break;
          }
          if (delete_q)
            cut_slot_from_list(q, &mtcsRtlData/*!avail_temp_slots*/->x_avail_temp_slots);
      }

      /* Either delete P or advance past it.  */
      if (delete_p)
          cut_slot_from_list(p, &mtcsRtlData/*!avail_temp_slots*/->x_avail_temp_slots);
  }
}

/* Moves temporary slot TEMP to LEVEL.  */
//原型 move_slot_to_level function.cc
static void move_slot_to_level (MtcsFunc *self,class mtcs_temp_slot *temp, int level)
{
  cut_slot_from_list(temp, temp_slots_at_level(self,temp->level));
  insert_slot_to_list (temp, temp_slots_at_level(self,level));
  temp->level = level;
}

/* If X could be a reference to a temporary slot, mark that slot as
   belonging to the to one level higher than the current level.  If X
   matched one of our slots, just mark that one.  Otherwise, we can't
   easily predict which it is, so upgrade all of them.

   This is called when an ({...}) construct occurs and a statement
   returns a value in memory.  */
//原型 preserve_temp_slots function.h function.cc
void mtcs_func_preserve_temp_slots (MtcsFunc *self,rtx x)
{
  class mtcs_temp_slot *p = 0, *next;
  if (x == 0)
    return;
  /* If X is a register that is being used as a pointer, see if we have
     a temporary slot we know it points to.  */
  if (REG_P (x) && REG_POINTER (x))
    p = find_temp_slot_from_address(self,x);
  /* If X is not in memory or is at a constant address, it cannot be in
     a temporary slot.  */
  if (p == 0 && (!MEM_P (x) || CONSTANT_P (XEXP (x, 0))))
    return;
  /* First see if we can find a match.  */
  if (p == 0)
    p = find_temp_slot_from_address (self,XEXP (x, 0));

  if (p != 0){
      if (p->level == temp_slot_level)
          move_slot_to_level (self,p, temp_slot_level - 1);
      return;
  }

  /* Otherwise, preserve all non-kept slots at this level.  */
  for (p = *temp_slots_at_level(self,temp_slot_level); p; p = next){
      next = p->next;
      move_slot_to_level(self,p, temp_slot_level - 1);
  }
}

/* Free all temporaries used so far.  This is normally called at the
   end of generating code for a statement.  */
//原型 free_temp_slots function.h function.cc
void mtcs_func_free_temp_slots (MtcsFunc *self)
{
  class mtcs_temp_slot *p, *next;
  bool some_available = false;

  for (p = *temp_slots_at_level (self,self->mtcsRtlData->x_temp_slot_level/*!temp_slot_level*/); p; p = next){
      next = p->next;
      make_slot_available (self,p);
      some_available = true;
  }

  if (some_available){
      remove_unused_temp_slot_addresses(self);
      combine_temp_slots (self);
  }
}

//#ifdef INCOMING_REG_PARM_STACK_SPACE
//#define STACK_DYNAMIC_OFFSET(FNDECL)    \
//((ACCUMULATE_OUTGOING_ARGS                            \
//  ? (crtl->outgoing_args_size                     \
//     + (OUTGOING_REG_PARM_STACK_SPACE ((!(FNDECL) ? NULL_TREE : TREE_TYPE (FNDECL))) ? 0 \
//                           : INCOMING_REG_PARM_STACK_SPACE (FNDECL))) \
//  : 0) + (STACK_POINTER_OFFSET))
//#else
//#define STACK_DYNAMIC_OFFSET(FNDECL)    \
//  ((ACCUMULATE_OUTGOING_ARGS ? crtl->outgoing_args_size : poly_int64 (0)) \
// + (STACK_POINTER_OFFSET))
//#endif
//#endif

//原型 STACK_DYNAMIC_OFFSET (FNDECL) function.cc
//INCOMING_REG_PARM_STACK_SPACE 在平台定义依赖 REG_PARM_STACK_SPACE nvptx没有定义
//所以不需要实现在INCOMING_REG_PARM_STACK_SPACE下的 STACK_DYNAMIC_OFFSET
poly_int64 mtcs_func_get_stack_dynamic_offset(MtcsFunc *self,tree fndecl)
{
    if(!self->get_stack_dynamic_offset){
        n_error("平台未实现方法 get_stack_dynamic_offset");
        return 0;
    }
    return self->get_stack_dynamic_offset(self,fndecl);
}

/* Push deeper into the nesting level for stack temporaries.  */
//原型 push_temp_slots function.h function.cc
void mtcs_func_push_temp_slots (MtcsFunc *self)
{
  self->mtcsRtlData->x_temp_slot_level/*!temp_slot_level*/++;

}

/* Pop a temporary nesting level.  All slots in use in the current level
   are freed.  */
//原型 pop_temp_slots function.h function.cc
void mtcs_func_pop_temp_slots (MtcsFunc *self)
{
    mtcs_func_free_temp_slots (self);
    self->mtcsRtlData->x_temp_slot_level/*!temp_slot_level*/--;
}

//原型 #ifndef STACK_CHECK_PROTECT
int  mtcs_func_get_stack_check_protect(MtcsFunc *self)
{
   return self->get_stack_check_protect(self);
}

//原型 #ifndef STACK_OLD_CHECK_PROTECT
int  mtcs_func_get_stack_old_check_protect(MtcsFunc *self)
{
   return self->get_stack_old_check_protect(self);
}

//原型 #ifndef STACK_CHECK_MOVING_SP
int  mtcs_func_get_stack_check_moving_sp(MtcsFunc *self)
{
   return self->get_stack_check_moving_sp(self);
}

//原型 #define DEFAULT_INCOMING_FRAME_SP_OFFSET 各平台定义 dwarf2cfi.cc定义缺省的。
int  mtcs_func_get_default_incoming_frame_sp_offset(MtcsFunc *self)
{
   if(self->get_default_incoming_frame_sp_offset)
      return self->get_default_incoming_frame_sp_offset(self);
   return mtcs_func_get_incoming_frame_sp_offset(self);
}
//原型 #define INCOMING_FRAME_SP_OFFSET 0 各平台定义 defaults.h定义缺省的
int  mtcs_func_get_incoming_frame_sp_offset(MtcsFunc *self)
{
   if(self->get_incoming_frame_sp_offset)
      return self->get_incoming_frame_sp_offset(self);
   return 0; //defaults.h
}

/* Expand code to verify the stack_protect_guard.  This is invoked at
   the end of a function to be protected.  */
//原型 stack_protect_epilogue function.h function.cc
void mtcs_func_stack_protect_epilogue (MtcsFunc *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  MtcsExpr   *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

  tree guard_decl = mtcsRtlData/*!crtl*/->stack_protect_guard_decl;
  rtx_code_label *label =mtcs_rtl_gen_label_rtx(mtcsRTL);
  rtx x, y;
  rtx_insn *seq = NULL;

  x = mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,mtcsRtlData/*!crtl*/->stack_protect_guard);

  if (target_rtx_have_stack_protect_combined_test/*!targetm.have_stack_protect_combined_test*/(mtcsMachine->tmrtx)
          && guard_decl) {
      gcc_assert (DECL_P (guard_decl));
      y = mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,guard_decl);
      /* Allow the target to compute address of Y and compare it with X without
     leaking Y into a register.  This combined address + compare pattern
     allows the target to prevent spilling of any intermediate results by
     splitting it after register allocator.  */
      seq =target_rtx_gen_stack_protect_combined_test/*!targetm.gen_stack_protect_combined_test*/(mtcsMachine->tmrtx,x, y, label);
  }else{
      if (guard_decl)
          y =  mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,guard_decl);
      else
          y = const0_rtx;

      /* Allow the target to compare Y with X without leaking either into
     a register.  */
      if (target_rtx_have_stack_protect_test/*!targetm.have_stack_protect_test*/(mtcsMachine->tmrtx))
          seq = target_rtx_gen_stack_protect_test/*!targetm.gen_stack_protect_test*/(mtcsMachine->tmrtx,x, y, label);
  }

  if (seq)
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,seq);
  else
      mtcs_optabs_emit_cmp_and_jump_insns/*!emit_cmp_and_jump_insns*/(mtcsOptabs,x, y, EQ, NULL_RTX, ptr_mode, 1, label);

  /* The noreturn predictor has been moved to the tree level.  The rtl-level
     predictors estimate this branch about 20%, which isn't enough to get
     things moved out of line.  Since this is the only extant case of adding
     a noreturn function at the rtl level, it doesn't seem worth doing ought
     except adding the prediction by hand.  */
  rtx_insn *tmp = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
  if (JUMP_P (tmp))
    predict_insn_def (tmp, PRED_NORETURN, TAKEN);

  mtcs_calls_expand_call/*!expand_call*/(mtcsCalls,
          mtcsTarget/*!targetm.stack_protect_fail*/->stack_protect_fail(mtcsTarget), NULL_RTX, /*ignore=*/true);
  mtcs_func_free_temp_slots/*!free_temp_slots*/(self);
  mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,label);
}

//原型 STACK_PUSH_CODE default.h
int mtcs_func_get_stack_push_code(MtcsFunc *self)
{
    return self->stackPushCode;
}
void mtcs_func_set_stack_push_code(MtcsFunc *self,int value)
{
    self->stackPushCode=value;
}

//原型 #define STACK_CHECK_PROBE_INTERVAL_EXP 12 defaults.h
int mtcs_func_get_stack_check_probe_interval_exp(MtcsFunc *self)
{
    return self->stackCheckProbeInteralExp;
}

void mtcs_func_set_stack_check_probe_interval_exp(MtcsFunc *self,int value)
{
    self->stackCheckProbeInteralExp=value;
    //原型 #define STACK_CHECK_MAX_FRAME_SIZE ((1 << STACK_CHECK_PROBE_INTERVAL_EXP) - UNITS_PER_WORD) default.h
    self-> stackCheckMaxFrameSize=((1 << self->stackCheckProbeInteralExp) - UNITS_PER_WORD);
}

//原型 #define STACK_CHECK_MAX_FRAME_SIZE ((1 << STACK_CHECK_PROBE_INTERVAL_EXP) - UNITS_PER_WORD) default.h
int mtcs_func_get_stack_check_max_frame_size(MtcsFunc *self)
{
    return self->stackCheckMaxFrameSize;
}


/* cfun should never be set directly; use this function.  */
//原型 set_cfun function.h funciton.cc
void mtcs_func_set_cfun (MtcsFunc *self,struct function *new_cfun, bool force)
{
  if (self->currentFun != new_cfun || force){
      self->currentFun = new_cfun;
      invoke_set_current_function_hook(self,new_cfun ? new_cfun->decl : NULL_TREE);
      redirect_edge_var_map_empty ();
  }
  //不能直接 cfun=self->currentFun,必须用 undef cfun
#undef cfun
  cfun=self->currentFun;
#define cfun (cfun+0)

}

void mtcs_func_set_current_function_decl(MtcsFunc *self,tree t)
{
    self->current_function_decl=t;
}

/* Save the current context for compilation of a nested function.
   This is called from language-specific code.  */
//原型 push_function_context funciton.h function.cc
void mtcs_func_push_function_context (MtcsFunc *self)
{
  if (self->currentFun == NULL)
    mtcs_func_allocate_struct_function/*!allocate_struct_function*/(self,NULL, false);
  self->function_context_stack.safe_push (self->currentFun);
  mtcs_func_set_cfun/*!set_cfun*/(self,NULL);
}


/* Restore the last saved context, at the end of a nested function.
   This function is called from language-specific code.  */
//原型 pop_function_context function.h function.cc
void mtcs_func_pop_function_context (MtcsFunc *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

  struct function *p = self->function_context_stack.pop ();
  mtcs_func_set_cfun (self,p);
  /*!current_function_decl = p->decl;*/
  self->current_function_decl = p->decl;
  current_function_decl=self->current_function_decl;
  /* Reset variables that have known state during rtx generation.  */
  self->virtuals_instantiated = 0;
  mtcsRTL->generating_concat_p = 1;
}

/* Push the current cfun onto the stack, and set cfun to new_cfun.  Also set
   current_function_decl accordingly.  */
//原型 push_cfun function.h function.cc
void mtcs_func_push_cfun (MtcsFunc *self,struct function *new_cfun)
{
  gcc_assert ((!self->currentFun && !self->current_function_decl)
          || (self->currentFun && self->current_function_decl == self->currentFun->decl));
  self->cfun_stack.safe_push (self->currentFun);
  self->current_function_decl = new_cfun ? new_cfun->decl : NULL_TREE;
  current_function_decl=self->current_function_decl;
  mtcs_func_set_cfun (self,new_cfun);
}

/* Pop cfun from the stack.  Also set current_function_decl accordingly.  */
//原型 pop_cfun function.h function.cc
void mtcs_func_pop_cfun (MtcsFunc *self)
{
  struct function *new_cfun = self->cfun_stack.pop ();
  /* When in_dummy_function, we do have a cfun but current_function_decl is
     NULL.  We also allow pushing NULL cfun and subsequently changing
     current_function_decl to something else and have both restored by
     pop_cfun.  */
  n_debug("mtcsfunc.c mtcsfun.c mtcs_func_pop_cfun 00 cfun:%p current_function_decl:%p in_dummy_function:%d currentFun:%p new_cfun:%p\n",
           cfun,current_function_decl,self->in_dummy_function,self->currentFun,new_cfun);
  gcc_checking_assert (self->in_dummy_function || !self->currentFun/*!cfun*/
          || self->current_function_decl/*!current_function_decl*/ == self->currentFun->decl);
  mtcs_func_set_cfun/*!set_cfun*/(self,new_cfun);
  self->current_function_decl = new_cfun ? new_cfun->decl : NULL_TREE;
  current_function_decl = self->current_function_decl;
}

/* Return value of funcdef and increase it.  */
//原型 get_next_funcdef_no function.h function.cc 没有其它地方调用改成私有的方法
static int getNextFuncdefNo(MtcsFunc *self)
{
  return self->funcdef_no++;
}

/* Invoke the target hook when setting cfun.  Update the optimization options
   if the function uses different options than the default.  */
//原型 invoke_set_current_function_hook funciton.cc
static void invoke_set_current_function_hook (MtcsFunc *self,tree fndecl)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsOpinit *mtcsOpinit=mtcs_target_get_opinit(mtcsTarget);
  MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOpts *mtcsOpts=mtcs_target_get_opts(mtcsTarget);
  MtcsAlign *mtcsAlign=mtcs_target_get_align(mtcsTarget);
  MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

  if (!self->in_dummy_function){
      tree opts = ((fndecl) ? DECL_FUNCTION_SPECIFIC_OPTIMIZATION (fndecl):mtcs_optimization_default_node/*!optimization_default_node*/);
      n_debug("mtcsfunc.c invoke_set_current_function_hook --00 cl_optimization_restore opts:%p"\
            " fndecl:%p mtcs_optimization_default_node:%p mtcs_optimization_current_node:%p\n",
              opts,fndecl,mtcs_optimization_default_node,mtcs_optimization_current_node);

      if (!opts)
       opts = mtcs_optimization_default_node/*optimization_default_node*/;

      /* Change optimization options if needed.  */
      if (mtcs_optimization_current_node/*!optimization_current_node*/ != opts){
          n_debug("mtcsfunc.c invoke_set_current_function_hook --11 cl_optimization_restore optimization_current_node:%p\n",
                 mtcs_optimization_current_node);
          mtcs_optimization_current_node/*!optimization_current_node*/ = opts;
          mtcs_options_cl_optimization_restore/*!cl_optimization_restore*/(mtcsOptions,
                  mtcsOptions->global_options, mtcsOptions->global_options_set,TREE_OPTIMIZATION (opts));
      }

      mtcsTarget/*!targetm.set_current_function*/->set_current_function(mtcsTarget,fndecl);
      mtcsOpinit/*!this_fn_optabs*/->this_fn_optabs =mtcsOpinit/*!this_target_optabs*/->thisTargetOptabs ;

      /* Initialize global alignment variables after op.  */
      mtcs_align_parse_alignment_opts/*!parse_alignment_opts*/(mtcsAlign);

      if (opts != mtcs_optimization_default_node/*!optimization_default_node*/){
          n_debug("mtcsfunc.c invoke_set_current_function_hook --22 cl_optimization_restore\n");
          mtcs_optabs_init_tree_optimization_optabs/*!init_tree_optimization_optabs*/(mtcsOptabs,opts);
          if (TREE_OPTIMIZATION_OPTABS (opts))
              mtcsOpinit->this_fn_optabs/*!this_fn_optabs*/= (struct target_optabs *)TREE_OPTIMIZATION_OPTABS (opts);
      }
  }
}


/* Allocate and initialize the stack usage info data structure for the
   current function.  */
//原型 allocate_stack_usage_info function.cc
static void allocate_stack_usage_info (MtcsFunc *self)
{
  gcc_assert (!self->currentFun->su);
  self->currentFun->su = ggc_cleared_alloc<stack_usage> ();
  self->currentFun->su->static_stack_size = -1;
}

/* Allocate a function structure for FNDECL and set its contents
   to the defaults.  Set cfun to the newly-allocated object.
   Some of the helper functions invoked during initialization assume
   that cfun has already been set.  Therefore, assign the new object
   directly into cfun and invoke the back end hook explicitly at the
   very end, rather than initializing a temporary and calling set_cfun
   on it.

   ABSTRACT_P is true if this is a function that will never be seen by
   the middle-end.  Such functions are front-end concepts (like C++
   function templates) that do not correspond directly to functions
   placed in object files.  */
//原型 allocate_struct_function function.h function.cc
void mtcs_func_allocate_struct_function (MtcsFunc *self,tree fndecl, bool abstract_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsExcept *mtcsExcept=mtcs_target_get_except(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);

   tree fntype = fndecl ? TREE_TYPE (fndecl) : NULL_TREE;
   self->currentFun=ggc_cleared_alloc<function> ();
   //不能直接 cfun=self->currentFun,必须用 undef cfun
 #undef cfun
   cfun=self->currentFun;
 #define cfun (cfun+0)

   mtcs_except_init_eh_for_function/*!init_eh_for_function*/(mtcsExcept);
   n_debug("mtcsfunc.c  allocate_struct_function 00 创建 machine fndecl:%p\n",fndecl);
   self->currentFun/*!cfun->machine*/->machine =self->init_machine_status(self);/*!cfun->machine = (*init_machine_status) ();*/

   //#ifdef OVERRIDE_ABI_FORMAT host=1 nvptx=0
   //  OVERRIDE_ABI_FORMAT (fndecl);
   //#endif

   if (fndecl != NULL_TREE){
      DECL_STRUCT_FUNCTION (fndecl) =self->currentFun;
      self->currentFun->decl = fndecl;
      self->currentFun->funcdef_no/*!current_function_funcdef_no*/ =getNextFuncdefNo/*!get_next_funcdef_no*/(self);
   }
   n_debug("mtcsfunc.c  allocate_struct_function 11:lang_hooks.emits_begin_stmt:%d\n",lang_hooks.emits_begin_stmt);
   invoke_set_current_function_hook(self,fndecl);
   n_debug("mtcsfunc.c  allocate_struct_function --11 fndecl:%p\n",fndecl);

   if (fndecl != NULL_TREE){
      tree result = DECL_RESULT (fndecl);
      if (!abstract_p){
         /* Now that we have activated any function-specific attributes
         that might affect layout, particularly vector modes, relayout
         each of the parameters and the result.  */
         n_debug("mtcsfunc.c  allocate_struct_function 22 fndecl:%p\n",fndecl);
         mtcs_stor_layout_relayout_decl/*!relayout_decl*/(mtcsStorLayout,result);
         for (tree parm = DECL_ARGUMENTS (fndecl); parm;  parm = DECL_CHAIN (parm))
            mtcs_stor_layout_relayout_decl/*!relayout_decl*/(mtcsStorLayout,parm);
         n_debug("mtcsfunc.c  allocate_struct_function 33 fndecl:%p\n",fndecl);
         /* Similarly relayout the function decl.  */
         target_option_relayout_function/*!targetm.target_option.relayout_function*/(mtcsMachine->option,fndecl);
      }
      n_debug("mtcsfunc.c  allocate_struct_function --22 fndecl:%p\n",fndecl);

      if (!abstract_p && mtcs_func_aggregate_value_p/*!aggregate_value_p*/(self,result, fndecl)){
         //    #ifdef PCC_STATIC_STRUCT_RETURN     host=0 nvptx=0
         //          cfun->returns_pcc_struct = 1;
         //    #endif
         self->currentFun->returns_struct = 1;
      }
      n_debug("mtcsfunc.c  allocate_struct_function --44 fndecl:%p\n",fndecl);

      self->currentFun->stdarg = stdarg_p (fntype);

      n_debug("mtcsfunc.c  allocate_struct_function 44 %p %d %d self->currentFun:%p\n",
      fndecl,cfun->stdarg,self->currentFun->stdarg,self->currentFun);

      /* Assume all registers in stdarg functions need to be saved.  */
      self->currentFun->va_list_gpr_size = VA_LIST_MAX_GPR_SIZE;
      self->currentFun->va_list_fpr_size = VA_LIST_MAX_FPR_SIZE;
      /* ??? This could be set on a per-function basis by the front-end
      but is this worth the hassle?  */
      self->currentFun->can_throw_non_call_exceptions = mtcsOptionsItem->x_flag_non_call_exceptions;
      self->currentFun->can_delete_dead_exceptions = mtcsOptionsItem->x_flag_delete_dead_exceptions;
      if (!mtcsOptionsItem->x_profile_flag && !mtcsOptionsItem->x_flag_instrument_function_entry_exit)
         DECL_NO_INSTRUMENT_FUNCTION_ENTRY_EXIT (fndecl) = 1;

      if (mtcsOptionsItem->x_flag_callgraph_info)
         allocate_stack_usage_info(self);
   }

   /* Don't enable begin stmt markers if var-tracking at assignments is
   disabled.  The markers make little sense without the variable
   binding annotations among them.  */
   self->currentFun->debug_nonbind_markers = lang_hooks.emits_begin_stmt  && MAY_HAVE_DEBUG_MARKER_STMTS;
}

/* This is like allocate_struct_function, but pushes a new cfun for FNDECL
   instead of just setting it.  */
//原型 push_struct_function function.h function.cc
void mtcs_func_push_struct_function (MtcsFunc *self,tree fndecl, bool abstract_p)
{
  /* When in_dummy_function we might be in the middle of a pop_cfun and
     current_function_decl and cfun may not match.  */
    /*!
  gcc_assert (self->in_dummy_function  || (!cfun && !current_function_decl)
          || (cfun && current_function_decl == cfun->decl));
          */
    n_debug("mtcsfunc.c  push_struct_function 00 cfun:%p current_function_decl:%p in_dummy_function:%d currentFun:%p\n",
            cfun,current_function_decl,self->in_dummy_function,self->currentFun);
  gcc_assert (self->in_dummy_function  || (!self->currentFun && !current_function_decl)
          || (self->currentFun && current_function_decl == self->currentFun->decl));
  self->cfun_stack.safe_push (self->currentFun/*!cfun*/);
  self->current_function_decl = fndecl;
  current_function_decl = self->current_function_decl;
  mtcs_func_allocate_struct_function/*!allocate_struct_function*/(self,fndecl, abstract_p);
}

//从mtccompile收集的函数加入到funcArray中
void mtcs_func_add_mtcs_node(MtcsFunc *self,MtcsFuncNode *node)
{
    n_ptr_array_add(self->funcArray,node);
}

MtcsFuncNode *mtcs_func_get_node(MtcsFunc *self,struct cgraph_node *node)
{
    int i;
    for(i=0;i<self->funcArray->len;i++){
        MtcsFuncNode *mtcsFuncNode=n_ptr_array_index(self->funcArray,i);
        if(mtcsFuncNode->node==node)
            return mtcsFuncNode;
    }
    n_error("cgraph_node *node 没有同位体! %s\n",node->name());
    return NULL;
}

//cgraph_node是从mtcs_compile克隆而来的，node中的decl已经包含有struct function *
//所以在这里做2件事 1.创建machine
//原型 push_struct_function function.h function.cc
void mtcs_func_push_struct_function_no_create(MtcsFunc *self,tree fndecl, bool abstract_p)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsExcept *mtcsExcept=mtcs_target_get_except(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsStorLayout *mtcsStorLayout=mtcs_target_get_stor_layout(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   /* When in_dummy_function we might be in the middle of a pop_cfun and
   current_function_decl and cfun may not match.  */
   /*!
   gcc_assert (self->in_dummy_function  || (!cfun && !current_function_decl)
   || (cfun && current_function_decl == cfun->decl));
   */
   n_debug("mtcsfunc.c  mtcs_func_push_struct_function_no_create 00 cfun:%p"
         "current_function_decl:%p in_dummy_function:%d currentFun:%p\n",
                     cfun,current_function_decl,self->in_dummy_function,self->currentFun);
   gcc_assert (self->in_dummy_function  || (!self->currentFun && !current_function_decl)
         || (self->currentFun && current_function_decl == self->currentFun->decl));
   gcc_assert(DECL_STRUCT_FUNCTION(fndecl));
   self->cfun_stack.safe_push (self->currentFun/*!cfun*/);
   self->current_function_decl = fndecl;
   current_function_decl = self->current_function_decl;
   // 不需要分配新的 mtcs_func_allocate_struct_function/*!allocate_struct_function*/(self,fndecl, abstract_p);

   tree fntype = fndecl ? TREE_TYPE (fndecl) : NULL_TREE;
   self->currentFun=DECL_STRUCT_FUNCTION(fndecl);
   set_cfun(self->currentFun);
   //mtcs_except_init_eh_for_function/*!init_eh_for_function*/(mtcsExcept);
   //n_debug("mtcsfunc.c allocate_struct_function 00 创建 machine\n");
   self->currentFun/*!cfun->machine*/->machine =self->init_machine_status(self);/*!cfun->machine = (*init_machine_status) ();*/
   self->currentFun->funcdef_no/*!current_function_funcdef_no*/ =getNextFuncdefNo/*!get_next_funcdef_no*/(self);
   n_debug("mtcsfunc.c  mtcs_func_push_struct_function_no_create 11:lang_hooks.emits_begin_stmt:%d self->currentFun:%p\n",
   lang_hooks.emits_begin_stmt,self->currentFun,self->currentFun->x_current_loops);
   invoke_set_current_function_hook(self,fndecl);
   tree result = DECL_RESULT (fndecl);
   if (!abstract_p){
      /* Now that we have activated any function-specific attributes
      that might affect layout, particularly vector modes, relayout
      each of the parameters and the result.  */
      n_debug("mtcsfunc.c  mtcs_func_push_struct_function_no_create 22  fndecl:%p\n",fndecl);
      mtcs_stor_layout_relayout_decl/*!relayout_decl*/(mtcsStorLayout,result);
      for (tree parm = DECL_ARGUMENTS (fndecl); parm;  parm = DECL_CHAIN (parm))
         mtcs_stor_layout_relayout_decl/*!relayout_decl*/(mtcsStorLayout,parm);
      n_debug("mtcsfunc.c  mtcs_func_push_struct_function_no_create 33  fndecl:%p\n",fndecl);
      /* Similarly relayout the function decl.  */
      target_option_relayout_function/*!targetm.target_option.relayout_function*/(mtcsMachine->option,fndecl);
   }

   if (!abstract_p && mtcs_func_aggregate_value_p/*!aggregate_value_p*/(self,result, fndecl)){
      //    #ifdef PCC_STATIC_STRUCT_RETURN     host=0 nvptx=0
      //          cfun->returns_pcc_struct = 1;
      //    #endif
      self->currentFun->returns_struct = 1;
   }

   self->currentFun->stdarg = stdarg_p (fntype);

   n_debug("mtcsfunc.c  mtcs_func_push_struct_function_no_create 44 self->currentFun->stdarg:%d fndecl:%p %p %p\n",
         self->currentFun->stdarg,fndecl,self->currentFun,cfun);

   /* Assume all registers in stdarg functions need to be saved.  */
   self->currentFun->va_list_gpr_size = VA_LIST_MAX_GPR_SIZE;
   self->currentFun->va_list_fpr_size = VA_LIST_MAX_FPR_SIZE;
   /* ??? This could be set on a per-function basis by the front-end
   but is this worth the hassle?  */
   self->currentFun->can_throw_non_call_exceptions = mtcsOptionsItem->x_flag_non_call_exceptions;
   self->currentFun->can_delete_dead_exceptions = mtcsOptionsItem->x_flag_delete_dead_exceptions;
   if (!mtcsOptionsItem->x_profile_flag && !mtcsOptionsItem->x_flag_instrument_function_entry_exit)
      DECL_NO_INSTRUMENT_FUNCTION_ENTRY_EXIT (fndecl) = 1;

   if (mtcsOptionsItem->x_flag_callgraph_info)
      allocate_stack_usage_info(self);

   /* Don't enable begin stmt markers if var-tracking at assignments is
   disabled.  The markers make little sense without the variable
   binding annotations among them.  */
   self->currentFun->debug_nonbind_markers = lang_hooks.emits_begin_stmt  && MAY_HAVE_DEBUG_MARKER_STMTS;

}

//原型 push_dummy_function function.h function.cc
void mtcs_func_push_dummy_function (MtcsFunc *self,bool with_decl)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   tree fn_decl, fn_type, fn_result_decl;
   gcc_assert (!self->in_dummy_function);
   self->in_dummy_function = true;
   if (with_decl){
      fn_type = build_function_type_list (void_type_node, NULL_TREE);
      fn_decl = mtcs_tree_build_decl/*!build_decl*/(mtcsTree,UNKNOWN_LOCATION, FUNCTION_DECL, NULL_TREE,fn_type);
      fn_result_decl = mtcs_tree_build_decl/*!build_decl*/(mtcsTree,UNKNOWN_LOCATION, RESULT_DECL,NULL_TREE, void_type_node);
      DECL_RESULT (fn_decl) = fn_result_decl;
      DECL_ARTIFICIAL (fn_decl) = 1;
      tree fn_name = get_identifier (" ");
      SET_DECL_ASSEMBLER_NAME (fn_decl, fn_name);
   }else
      fn_decl = NULL_TREE;
   mtcs_func_push_struct_function/*!push_struct_function*/(self,fn_decl);
}

/* Reset crtl and other non-struct-function variables to defaults as
   appropriate for emitting rtl at the start of a function.  */
//原型 prepare_function_start function.cc
static void prepare_function_start (MtcsFunc *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  MtcsFuncAbi *mtcsFuncAbi=mtcs_target_get_func_abi(mtcsTarget);
  MtcsAsm *mtcsAsm =mtcs_target_get_asm(mtcsTarget);
  MtcsExpr *mtcsExpr =mtcs_target_get_expr(mtcsTarget);
  MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsOptions *mtcsOptions =mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

  gcc_assert (!mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData));
  if (self->in_dummy_function)
      mtcsRtlData->abi=mtcs_func_abi_get_default(mtcsFuncAbi);/*!crtl->abi = &default_function_abi;*/
  else
      mtcsRtlData->abi=&mtcs_func_abi_fndecl_abi(mtcsFuncAbi,cfun->decl).base_abi ();/*!crtl->abi = &fndecl_abi (cfun->decl).base_abi ();*/
  mtcs_func_init_temp_slots/*!init_temp_slots*/(self);
  mtcs_func_init_emit/*!init_emit*/(self);
  mtcs_asm_init_varasm_status/*!init_varasm_status*/(mtcsAsm);
  mtcs_expr_init_expr/*!init_expr*/(mtcsExpr);
  mtcs_func_default_rtl_profile/*!default_rtl_profile*/(self);

  if (mtcsOptionsItem->x_flag_stack_usage_info && !mtcsOptionsItem->x_flag_callgraph_info)
    allocate_stack_usage_info(self);

  cse_not_expected = ! mtcsOptionsItem->x_optimize;

  /* Caller save not needed yet.  */
  caller_save_needed = 0;

  /* We haven't done register allocation yet.  */
  reg_renumber = 0;

  /* Indicate that we have not instantiated virtual registers yet.  */
  self->virtuals_instantiated = 0;

  /* Indicate that we want CONCATs now.  */
  mtcsRTL->generating_concat_p = 1;

  /* Indicate we have no need of a frame pointer yet.  */
   mtcsRtlData->x_frame_pointer_needed/*!frame_pointer_needed (crtl->frame_pointer_needed) 这是一个宏定义*/ = 0;
}

/* Initialize the rtl expansion mechanism so that we can do simple things
   like generate sequences.  This is used to provide a context during global
   initialization of some passes.  You must call expand_dummy_function_end
   to exit this context.  */
//原型 init_dummy_function_start function.h function.cc
void mtcs_func_init_dummy_function_start (MtcsFunc *self)
{
    mtcs_func_push_dummy_function/*!push_dummy_function*/(self,false);
    prepare_function_start(self);
}

/* Initialize temporary slots.  */
//原型 init_temp_slots function.h function.cc
void mtcs_func_init_temp_slots (MtcsFunc *self)
{
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  /* We have not allocated any temporaries yet.  */
  mtcsRtlData->x_avail_temp_slots = 0;
  vec_alloc (mtcsRtlData->x_used_temp_slots, 0);
  mtcsRtlData->x_temp_slot_level = 0;
  self->n_temp_slots_in_use = 0;

  /* Set up the table to map addresses to temp slots.  */
  if (! self->temp_slot_address_table)
    self->temp_slot_address_table = hash_table<mtcs_temp_address_hasher>::create_ggc (32);
  else
    self->temp_slot_address_table->empty ();
}

//原型 pop_dummy_function function.h function.cc
void mtcs_func_pop_dummy_function (MtcsFunc *self)
{
  mtcs_func_pop_cfun/*!pop_cfun*/(self);
  self->in_dummy_function = false;
}

/* Undo the effects of init_dummy_function_start.  */
//原型 expand_dummy_function_end function.h function.cc
void mtcs_func_expand_dummy_function_end (MtcsFunc *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  gcc_assert (self->in_dummy_function);
  /* End any sequences that failed to be closed due to syntax errors.  */
  while (mtcs_rtl_data_in_sequence_p/*!in_sequence_p*/(mtcsRtlData))
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);

  /* Outside function body, can't compute type's actual size
     until next function's body starts.  */
  gcc_assert(self->currentFun==cfun);
  free_after_parsing (cfun);
  mtcs_func_free_after_compilation/*!free_after_compilation*/(self,cfun);
  mtcs_func_pop_dummy_function/*!pop_dummy_function*/(self);
}

/* Clear out all parts of the state in F that can safely be discarded
   after the function has been compiled, to let garbage collection
   reclaim the memory.  */
//原型 free_after_compilation function.h function.cc
void mtcs_func_free_after_compilation (MtcsFunc *self,struct function *f)
{
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  self->prologue_insn_hash = NULL;
  self->epilogue_insn_hash = NULL;

  free (mtcsRtlData->emit.regno_pointer_align);
  mtcs_rtl_data_reset(mtcsRtlData);/*!memset (crtl, 0, sizeof (struct rtl_data));*/
  f->eh = NULL;
  f->machine = NULL;
  f->cfg = NULL;
  f->curr_properties &= ~PROP_cfg;
  delete f->cond_uids;
  mtcsRtlData->regno_reg_rtx/*!regno_reg_rtx*/ = NULL;
}

/* Return true if we should assign DECL a pseudo register; false if it
   should live on the local stack.  */
//原型 use_register_for_decl function.h funciton.cc
bool mtcs_func_use_register_for_decl (MtcsFunc *self,const_tree decl)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   if (TREE_CODE (decl) == SSA_NAME){
      n_debug("mtcsfunc.c mtcs_func_use_register_for_decl 00\n");
      /* We often try to use the SSA_NAME, instead of its underlying
      decl, to get type information and guide decisions, to avoid
      differences of behavior between anonymous and named
      variables, but in this one case we have to go for the actual
      variable if there is one.  The main reason is that, at least
      at -O0, we want to place user variables on the stack, but we
      don't mind using pseudos for anonymous or ignored temps.
      Should we take the SSA_NAME, we'd conclude all SSA_NAMEs
      should go in pseudos, whereas their corresponding variables
      might have to go on the stack.  So, disregarding the decl
      here would negatively impact debug info at -O0, enable
      coalescing between SSA_NAMEs that ought to get different
      stack/pseudo assignments, and get the incoming argument
      processing thoroughly confused by PARM_DECLs expected to live
      in stack slots but assigned to pseudos.  */
      if (!SSA_NAME_VAR (decl))
         return TYPE_MODE (TREE_TYPE (decl)) != mtcsMode->modes.M_BLKmode
         && !(mtcsOptionsItem->x_flag_float_store && FLOAT_TYPE_P (TREE_TYPE (decl)));

      decl = SSA_NAME_VAR (decl);
   }
   n_debug("mtcsfunc.c mtcs_func_use_register_for_decl 11\n");

   /* Honor volatile.  */
   if (TREE_SIDE_EFFECTS (decl))
      return false;

   /* Honor addressability.  */
   if (TREE_ADDRESSABLE (decl))
      return false;
   n_debug("mtcsfunc.c mtcs_func_use_register_for_decl 22\n");

   /* RESULT_DECLs are a bit special in that they're assigned without
   regard to use_register_for_decl, but we generally only store in
   them.  If we coalesce their SSA NAMEs, we'd better return a
   result that matches the assignment in expand_function_start.  */
   if (TREE_CODE (decl) == RESULT_DECL){
      n_debug("mtcsfunc.c mtcs_func_use_register_for_decl 33\n");

      /* If it's not an aggregate, we're going to use a REG or a
      PARALLEL containing a REG.  */
      if (!mtcs_func_aggregate_value_p/*!aggregate_value_p*/(self,decl, current_function_decl))
         return true;

      /* If expand_function_start determines the return value, we'll
      use MEM if it's not by reference.  */
      if (cfun->returns_pcc_struct
        || (target_calls_struct_value_rtx/*!targetm.calls.struct_value_rtx*/(mtcsMachine->calls,
              TREE_TYPE (current_function_decl), 1)))
         return DECL_BY_REFERENCE (decl);

      /* Otherwise, we're taking an extra all.function_result_decl
      argument.  It's set up in assign_parms_augmented_arg_list,
      under the (negated) conditions above, and then it's used to
      set up the RESULT_DECL rtl in assign_params, after looping
      over all parameters.  Now, if the RESULT_DECL is not by
      reference, we'll use a MEM either way.  */
      if (!DECL_BY_REFERENCE (decl))
         return false;

      /* Otherwise, if RESULT_DECL is DECL_BY_REFERENCE, it will take
      the function_result_decl's assignment.  Since it's a pointer,
      we can short-circuit a number of the tests below, and we must
      duplicate them because we don't have the function_result_decl
      to test.  */
      if (!target_calls_allocate_stack_slots_for_args/*!targetm.calls.allocate_stack_slots_for_args*/(mtcsMachine->calls))
         return true;
      /* We don't set DECL_IGNORED_P for the function_result_decl.  */
      if (mtcsOptionsItem->x_optimize)
         return true;
      if (cfun->tail_call_marked)
         return true;
      /* We don't set DECL_REGISTER for the function_result_decl.  */
      return false;
   }
   n_debug("mtcsfunc.c mtcs_func_use_register_for_decl 44\n");

   /* Only register-like things go in registers.  */
   if (DECL_MODE (decl) == mtcsMode->modes.M_BLKmode)
      return false;

   /* If -ffloat-store specified, don't put explicit float variables
   into registers.  */
   /* ??? This should be checked after DECL_ARTIFICIAL, but tree-ssa
   propagates values across these stores, and it probably shouldn't.  */
   if (mtcsOptionsItem->x_flag_float_store && FLOAT_TYPE_P (TREE_TYPE (decl)))
      return false;

   if (!target_calls_allocate_stack_slots_for_args/*!targetm.calls.allocate_stack_slots_for_args*/(mtcsMachine->calls))
      return true;
   n_debug("mtcsfunc.c mtcs_func_use_register_for_decl 55\n");

   /* If we're not interested in tracking debugging information for
   this decl, then we can certainly put it in a register.  */
   if (DECL_IGNORED_P (decl))
      return true;
   n_debug("mtcsfunc.c mtcs_func_use_register_for_decl 66 %d %d\n",
         (TREE_CODE (decl) == PARM_DECL && cfun->tail_call_marked),RECORD_OR_UNION_TYPE_P (TREE_TYPE (decl)));

   if (mtcsOptionsItem->x_optimize)
      return true;
   n_debug("mtcsfunc.c mtcs_func_use_register_for_decl 77\n");

   /* Thunks force a tail call even at -O0 so we need to avoid creating a
   dangling reference in case the parameter is passed by reference.  */
   if (TREE_CODE (decl) == PARM_DECL && cfun->tail_call_marked)
      return true;

   if (!DECL_REGISTER (decl))
      return false;
   n_debug("mtcsfunc.c mtcs_func_use_register_for_decl 66\n");

   /* When not optimizing, disregard register keyword for types that
   could have methods, otherwise the methods won't be callable from
   the debugger.  */
   if (RECORD_OR_UNION_TYPE_P (TREE_TYPE (decl)))
      return false;
   return true;
}

//传入参数的处理
//1.确定所有命名参数的来源 2.命名参数的存储位置(本函数的栈区域和偏移) 3.处理不定参数存在本函数的什么区域
//原型 expand_function_start function.h function.cc
//subr==current_function_decl
void mtcs_func_expand_function_start (MtcsFunc *self,tree subr)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
  MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
  MtcsExpr  *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsExpand  *mtcsExpand=mtcs_target_get_expand(mtcsTarget);
  MtcsEmit  *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExplow  *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);
  MtcsGimpleExpr *mtcsGimpleExpr=mtcs_target_get_gimple_expr(mtcsTarget);
  MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
  MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

    /*!currently_expanding_function_start=true;*/
  self->currently_expanding_function_start = true;

  /* Make sure volatile mem refs aren't considered
     valid operands of arithmetic insns.  */
  mtcs_recog_init_recog_no_volatile/*!init_recog_no_volatile*/(mtcsRecog);

  mtcsRtlData->profile = (mtcsOptionsItem->x_profile_flag && ! DECL_NO_INSTRUMENT_FUNCTION_ENTRY_EXIT (subr));
  mtcsRtlData->limit_stack = (mtcsRTL->stack_limit_rtx/*!stack_limit_rtx*/!= NULL_RTX && ! DECL_NO_LIMIT_STACK (subr));

  /* Make the label for return statements to jump to.  Do not special
     case machines with special return instructions -- they will be
     handled later during jump, ifcvt, or epilogue creation.  */
  mtcsRtlData->x_return_label/*! return_label*/ = mtcs_rtl_gen_label_rtx/*!gen_label_rtx*/(mtcsRTL);

  /* Initialize rtx used to return the value.  */
  /* Do this before assign_parms so that we copy the struct value address
     before any library calls that assign parms might generate.  */

  /* Decide whether to return the value in memory or in a register.  */
  tree res = DECL_RESULT (subr);
  if (mtcs_func_aggregate_value_p/*!aggregate_value_p*/(self,res, subr)){
      /* Returning something that won't go in a register.  */
      rtx value_address = 0;

#ifdef PCC_STATIC_STRUCT_RETURN  //host=0 nvptx=0
      if (cfun->returns_pcc_struct)
    {
      int size = int_size_in_bytes (TREE_TYPE (res));
      value_address = assemble_static_space (size);
    }
      else
#endif
      {
          //nvptx返回空
          rtx sv =target_calls_struct_value_rtx/*!targetm.calls.struct_value_rtx*/(mtcsMachine->calls,TREE_TYPE (subr), 2);
          /* Expect to be passed the address of a place to store the value.
             If it is passed as an argument, assign_parms will take care of
             it.  */
          n_debug("mtcsfunc.c  mtcs_func_expand_function_start 00  struct_value_rtx sv:%p\n",sv);

          if (sv){
              value_address = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,mtcs_mode_get_Pmode(mtcsMode));
              mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,value_address, sv);
          }
      }
      if (value_address){
          n_debug("mtcsfunc.c  mtcs_func_expand_function_start 11 value_address:%p\n",value_address);

          rtx x = value_address;
          if (!DECL_BY_REFERENCE (res)){
              x = gen_rtx_MEM (DECL_MODE (res), x);
              mtcs_rtl_set_mem_attributes/*!set_mem_attributes*/(mtcsRTL,x, res, 1);
          }
          mtcs_expand_set_parm_rtl/*!set_parm_rtl*/(mtcsExpand,res, x);
      }
  }else if (DECL_MODE (res) == VOIDmode){
    /* If return mode is void, this decl rtl should not be used.  */
      n_debug("mtcsfunc.c  mtcs_func_expand_function_start 22 DECL_MODE (res) == VOIDmode\n");
      mtcs_expand_set_parm_rtl/*!set_parm_rtl*/(mtcsExpand,res, NULL_RTX);
  }else{
      /* Compute the return values into a pseudo reg, which we will copy
     into the true return register after the cleanups are done.  */
      tree return_type = TREE_TYPE (res);

      /* If we may coalesce this result, make sure it has the expected mode
     in case it was promoted.  But we need not bother about BLKmode.  */
      machine_mode promoted_mode = mtcsOptionsItem->x_flag_tree_coalesce_vars
            && mtcs_gimple_expr_is_gimple_reg/*!is_gimple_reg*/(mtcsGimpleExpr,res)
      ? mtcs_mode_promote_ssa_mode/*!promote_ssa_mode*/(mtcsMode,ssa_default_def (cfun, res), NULL)
              :mtcsMode->modes.M_BLKmode;
      n_debug("mtcsfunc.c  mtcs_func_expand_function_start 33 promoted_mode:%d\n",promoted_mode);

      if (promoted_mode != mtcsMode->modes.M_BLKmode){
          n_debug("mtcsfunc.c  mtcs_func_expand_function_start 44 promoted_mode != mtcsMode->modes.M_BLKmode blk:%d\n",
                  mtcsMode->modes.M_BLKmode);

          mtcs_expand_set_parm_rtl/*!set_parm_rtl*/(mtcsExpand,res, mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,promoted_mode));
      }else if (TYPE_MODE (return_type) != mtcsMode->modes.M_BLKmode
           && target_calls_return_in_msb/*targetm.calls.return_in_msb*/(mtcsMachine->calls,return_type)){
        /* expand_function_end will insert the appropriate padding in
           this case.  Use the return value's natural (unpadded) mode
           within the function proper.  */
          n_debug("mtcsfunc.c  mtcs_func_expand_function_start 55 TYPE_MODE (return_type):%d\n",
                  TYPE_MODE (return_type));
          mtcs_expand_set_parm_rtl/*!set_parm_rtl*/(mtcsExpand,res, mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,TYPE_MODE (return_type)));
      }else{
          /* In order to figure out what mode to use for the pseudo, we
             figure out what the mode of the eventual return register will
             actually be, and use that.  */
          rtx hard_reg = mtcs_func_hard_function_value/*!hard_function_value*/(self,return_type, subr, 0, 1);

          /* Structures that are returned in registers are not
             aggregate_value_p, so we may see a PARALLEL or a REG.  */
          if (REG_P (hard_reg)){
              n_debug("mtcsfunc.c  mtcs_func_expand_function_start 77 REG_P (hard_reg):%d\n",
                      GET_MODE (hard_reg));
              mtcs_expand_set_parm_rtl/*!set_parm_rtl*/(mtcsExpand,res, mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (hard_reg)));
          }else{
              n_debug("mtcsfunc.c  mtcs_func_expand_function_start 88 REG_P (hard_reg)=false\n");
              gcc_assert (GET_CODE (hard_reg) == PARALLEL);
              mtcs_expand_set_parm_rtl/*!set_parm_rtl*/(mtcsExpand,res, gen_group_rtx (hard_reg));
          }
      }

      /* Set DECL_REGISTER flag so that expand_function_end will copy the
     result to the real return register(s).  */
      DECL_REGISTER (res) = 1;
  }
  const char *fnName=IDENTIFIER_POINTER(DECL_NAME(cfun->decl));
  n_debug("mtcsfunc.c  mtcs_func_expand_function_start 99 %p %p %s\n",subr,cfun->decl,fnName);
  /* Initialize rtx for parameters and local variables.
     In some cases this requires emitting insns.  */
  assign_parms(self,subr);
  n_debug("mtcsfunc.c  mtcs_func_expand_function_start 100 参数分配完成 %s\n",fnName);

  /* If function gets a static chain arg, store it.  */
  if (cfun->static_chain_decl){
     n_debug("mtcsfunc.c  mtcs_func_expand_function_start 101 cfun->static_chain_decl %s\n",fnName);

      tree parm = cfun->static_chain_decl;
      rtx local, chain;
      rtx_insn *insn;
      int unsignedp;

      local = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,promote_decl_mode (parm, &unsignedp));
      chain = target_calls_static_chain/*!targetm.calls.static_chain*/(mtcsMachine->calls,current_function_decl, true);
      mtcs_rtl_set_decl_incoming_rtl/*!set_decl_incoming_rtl*/(mtcsRTL,parm, chain, false);
      mtcs_expand_set_parm_rtl/*!set_parm_rtl*/(mtcsExpand,parm, local);
      mtcs_rtl_mark_reg_pointer/*!mark_reg_pointer*/(mtcsRTL,local, TYPE_ALIGN (TREE_TYPE (TREE_TYPE (parm))));

      if (GET_MODE (local) != GET_MODE (chain)){
         n_debug("mtcsfunc.c  mtcs_func_expand_function_start 102 GET_MODE (local) != GET_MODE (chain) %s\n",fnName);
          mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,local, chain, unsignedp);
          insn =  mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
      }else{
         n_debug("mtcsfunc.c  mtcs_func_expand_function_start 103 %s\n",fnName);
          insn = mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,local, chain);
      }

      /* Mark the register as eliminable, similar to parameters.  */
      if (MEM_P (chain) && reg_mentioned_p (mtcs_rtl_get_arg_pointer_rtx/*!arg_pointer_rtx*/(mtcsRTL), XEXP (chain, 0))){
         n_debug("mtcsfunc.c  mtcs_func_expand_function_start 104  %s\n",fnName);
         mtcs_rtl_set_dst_reg_note/*!set_dst_reg_note*/(mtcsRTL,insn, REG_EQUIV, chain, local);
      }

      /* If we aren't optimizing, save the static chain onto the stack.  */
      if (!mtcsOptionsItem->x_optimize){
         n_debug("mtcsfunc.c  mtcs_func_expand_function_start 105 !optimize %s\n",fnName);
          tree saved_static_chain_decl = mtcs_tree_build_decl/*!build_decl*/(mtcsTree,DECL_SOURCE_LOCATION (parm), VAR_DECL,
                  DECL_NAME (parm), TREE_TYPE (parm));
          machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
          rtx saved_static_chain_rtx = mtcs_func_assign_stack_local/*!assign_stack_local*/(self,
                  pMode, mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,pMode), 0);
          mtcs_rtl_set_decl_rtl/*!SET_DECL_RTL*/(mtcsRTL,saved_static_chain_decl, saved_static_chain_rtx);
          mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,saved_static_chain_rtx, chain);
          SET_DECL_VALUE_EXPR (parm, saved_static_chain_decl);
          DECL_HAS_VALUE_EXPR_P (parm) = 1;
      }
  }

  /* The following was moved from init_function_start.
     The move was supposed to make sdb output more accurate.  */
  /* Indicate the beginning of the function body,
     as opposed to parm setup.  */
  mtcs_emit_emit_note/*!emit_note*/(mtcsEmit,NOTE_INSN_FUNCTION_BEG);

  gcc_assert (NOTE_P ( mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData)));

  function_beg_insn = parm_birth_insn =  mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);

  /* If the function receives a non-local goto, then store the
     bits we need to restore the frame pointer.  */
  if (cfun->nonlocal_goto_save_area){
      tree t_save;
      rtx r_save;
      n_debug("mtcsfunc.c  mtcs_func_expand_function_start 106 cfun->nonlocal_goto_save_area %s\n",fnName);

      tree var = TREE_OPERAND (cfun->nonlocal_goto_save_area, 0);
      gcc_assert (DECL_RTL_SET_P (var));

      t_save = build4 (ARRAY_REF,
               TREE_TYPE (TREE_TYPE (cfun->nonlocal_goto_save_area)),
               cfun->nonlocal_goto_save_area,
               integer_zero_node, NULL_TREE, NULL_TREE);
      r_save = mtcs_expr_expand_expr/*!expand_expr*/(mtcsExpr,t_save, NULL_RTX, VOIDmode, EXPAND_WRITE);
      gcc_assert (GET_MODE (r_save) == Pmode);

      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,r_save, mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL));
      mtcs_explow_update_nonlocal_goto_save_area/*!update_nonlocal_goto_save_area*/(mtcsExplow);
  }

  if (mtcsRtlData/*!crtl*/->profile){
     n_debug("mtcsfunc.c  mtcs_func_expand_function_start 107 crtl->profile %s\n",fnName);
#ifdef PROFILE_HOOK
      PROFILE_HOOK (current_function_funcdef_no);
#endif
  }

  /* If we are doing generic stack checking, the probe should go here.  */
  if (mtcsOptionsItem->x_flag_stack_check == GENERIC_STACK_CHECK){
     n_debug("mtcsfunc.c  mtcs_func_expand_function_start 108 flag_stack_check == GENERIC_STACK_CHECK %s\n",fnName);
    stack_check_probe_note = mtcs_emit_emit_note/*!emit_note*/(mtcsEmit,NOTE_INSN_DELETED);
  }

  self->currently_expanding_function_start = false;
}

static void do_use_return_reg (rtx reg, void *arg ATTRIBUTE_UNUSED)
{
  MtcsFunc *self=(MtcsFunc *)arg;
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  mtcs_emit_emit_use/*!emit_use*/(mtcsEmit,reg);
}

//原型 use_return_register function.cc
static void use_return_register (MtcsFunc *self)
{
    mtcs_func_diddle_return_value/*!diddle_return_value*/(self,do_use_return_reg, (void *)self);
}

/* Generate RTL for the end of the current function.  */
//原型 expand_function_end function.h function.cc
void mtcs_func_expand_function_end (MtcsFunc *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsRTL  *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsExpr  *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsExpand  *mtcsExpand=mtcs_target_get_expand(mtcsTarget);
   MtcsEmit  *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsExplow  *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;
   MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
   MtcsDojump *mtcsDojump=mtcs_target_get_dojump(mtcsTarget);
   MtcsExcept *mtcsExcept=mtcs_target_get_except(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsCalls *mtcsCalls=mtcs_target_get_calls(mtcsTarget);
   MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   n_debug("mtcsfunc.c  mtcs_func_expand_function_end 00 x_arg_pointer_save_area:%d arg_pointer_save_area_init:%d\n",
         mtcsRtlData->x_arg_pointer_save_area, mtcsRtlData/*!crtl*/->arg_pointer_save_area_init);
   /* If arg_pointer_save_area was referenced only from a nested
   function, we will not have initialized it yet.  Do that now.  */
   if (mtcsRtlData->x_arg_pointer_save_area && ! mtcsRtlData/*!crtl*/->arg_pointer_save_area_init)
      mtcs_func_get_arg_pointer_save_area/*!get_arg_pointer_save_area*/(self);

   n_debug("mtcsfunc.c  mtcs_func_expand_function_end 11 x_flag_stack_check:%d %d\n",mtcsOptionsItem->x_flag_stack_check,GENERIC_STACK_CHECK);

   /* If we are doing generic stack checking and this function makes calls,
   do a stack probe at the start of the function to ensure we have enough
   space for another stack frame.  */
   if (mtcsOptionsItem->x_flag_stack_check == GENERIC_STACK_CHECK){
      rtx_insn *insn, *seq;
      n_debug("mtcsfunc.c  mtcs_func_expand_function_end 22\n");

      for (insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); insn; insn = NEXT_INSN (insn))
         if (CALL_P (insn)){
            n_debug("mtcsfunc.c  mtcs_func_expand_function_end 33\n");

            rtx max_frame_size = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,
            mtcs_func_get_stack_check_max_frame_size/*!STACK_CHECK_MAX_FRAME_SIZE*/(self));
            mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
            if (mtcs_func_get_stack_check_moving_sp/*!STACK_CHECK_MOVING_SP*/(self)){
               n_debug("mtcsfunc.c  mtcs_func_expand_function_end 44\n");

               mtcs_explow_anti_adjust_stack_and_probe/*!anti_adjust_stack_and_probe*/(mtcsExplow,max_frame_size, true);
            }else{
               n_debug("mtcsfunc.c  mtcs_func_expand_function_end 55\n");

               mtcs_explow_probe_stack_range/*!probe_stack_range*/(mtcsExplow,
               mtcs_func_get_stack_old_check_protect/*!STACK_OLD_CHECK_PROTECT*/(self), max_frame_size);
            }
            seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
            mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
            set_insn_locations (seq, prologue_location);
            mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,seq, stack_check_probe_note);
            break;
         }
   }

   /* End any sequences that failed to be closed due to syntax errors.  */
   while (mtcs_rtl_data_in_sequence_p/*!in_sequence_p*/(mtcsRtlData)){
      n_debug("mtcsfunc.c  mtcs_func_expand_function_end 66\n");
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
   }

   mtcs_dojump_clear_pending_stack_adjust/*!clear_pending_stack_adjust*/(mtcsDojump);
   mtcs_dojump_do_pending_stack_adjust/*!do_pending_stack_adjust*/(mtcsDojump);

   /* Output a linenumber for the end of the function.
   SDB depended on this.  */
   mtcs_emit_set_curr_insn_location/*!set_curr_insn_location*/(mtcsEmit,input_location);

   /* Before the return label (if any), clobber the return
   registers so that they are not propagated live to the rest of
   the function.  This can only happen with functions that drop
   through; if there had been a return statement, there would
   have either been a return rtx, or a jump to the return label.

   We delay actual code generation after the current_function_value_rtx
   is computed.  */
   rtx_insn *clobber_after = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
   /* Output the label for the actual return from the function.  */
   mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,mtcsRtlData->x_return_label/*!return_label*/);

   if (target_common_except_unwind_info/*!targetm_common.except_unwind_info*/
         (mtcsMachine->common,mtcsOptions->global_options/*!&global_options*/) == UI_SJLJ){
      /* Let except.cc know where it should emit the call to unregister
      the function context for sjlj exceptions.  */
      n_debug("mtcsfunc.c  mtcs_func_expand_function_end 77 mtcsOptionsItem->x_flag_exceptions:%d\n",
            mtcsOptionsItem->x_flag_exceptions);

      if (mtcsOptionsItem->x_flag_exceptions)
         mtcs_except_sjlj_emit_function_exit_after/*!sjlj_emit_function_exit_after*/(mtcsExcept,
               mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData));
   }

   /* If this is an implementation of throw, do what's necessary to
   communicate between __builtin_eh_return and the epilogue.  */
   mtcs_except_expand_eh_return/*!expand_eh_return*/(mtcsExcept);

   /* If stack protection is enabled for this function, check the guard.  */
   if (mtcsRtlData/*!crtl*/->stack_protect_guard
   && mtcsTarget->stack_protect_runtime_enabled_p/*!targetm.stack_protect_runtime_enabled_p*/(mtcsTarget)
   && mtcsRtlData->x_naked_return_label == NULL_RTX){
      n_debug("mtcsfunc.c  mtcs_func_expand_function_end 88 \n");
      mtcs_func_stack_protect_epilogue/*!stack_protect_epilogue*/(self);
   }
   /* If scalar return value was computed in a pseudo-reg, or was a named
   return value that got dumped to the stack, copy that to the hard
   return register.  */
   if (DECL_RTL_SET_P (DECL_RESULT (current_function_decl))){
      n_debug("mtcsfunc.c  mtcs_func_expand_function_end 99 \n");
      tree decl_result = DECL_RESULT (current_function_decl);
      rtx decl_rtl = mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,decl_result);
      if ((REG_P (decl_rtl)
      ? REGNO (decl_rtl) >= mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)
      : DECL_REGISTER (decl_result))
      /* Unless the psABI says not to.  */
      && !TYPE_EMPTY_P (TREE_TYPE (decl_result))){
         n_debug("mtcsfunc.c  mtcs_func_expand_function_end 99aa \n");
         rtx real_decl_rtl = mtcsRtlData/*!crtl*/->return_rtx;
         complex_mode cmode;
         mtcs_print_rtl_single(stderr,real_decl_rtl);
         /* This should be set in assign_parms.  */
         gcc_assert (REG_FUNCTION_VALUE_P (real_decl_rtl));

         /* If this is a BLKmode structure being returned in registers,
         then use the mode computed in expand_return.  Note that if
         decl_rtl is memory, then its mode may have been changed,
         but that crtl->return_rtx has not.  */
         if (GET_MODE (real_decl_rtl) == mtcsMode->modes.M_BLKmode){
            n_debug("mtcsfunc.c  mtcs_func_expand_function_end 99bb \n");
            mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,real_decl_rtl, GET_MODE (decl_rtl));
         }
         /* If a non-BLKmode return value should be padded at the least
         significant end of the register, shift it left by the appropriate
         amount.  BLKmode results are handled using the group load/store
         machinery.  */
         if (TYPE_MODE (TREE_TYPE (decl_result)) !=mtcsMode->modes.M_BLKmode
         && REG_P (real_decl_rtl)
         && target_calls_return_in_msb/*targetm.calls.return_in_msb*/(mtcsMachine->calls,TREE_TYPE (decl_result))){
            n_debug("mtcsfunc.c  mtcs_func_expand_function_end 99cc \n");
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,mtcs_rtl_gen_rtx_REG/*!gen_rtx_REG*/(mtcsRTL,GET_MODE (decl_rtl),
                  REGNO (real_decl_rtl)),decl_rtl);
            mtcs_calls_shift_return_value/*!shift_return_value*/(mtcsCalls,GET_MODE (decl_rtl), true, real_decl_rtl);
         }else if (GET_CODE (real_decl_rtl) == PARALLEL){
            n_debug("mtcsfunc.c  mtcs_func_expand_function_end 99dd \n");

            /* If expand_function_start has created a PARALLEL for decl_rtl,
            move the result to the real return registers.  Otherwise, do
            a group load from decl_rtl for a named return.  */
            if (GET_CODE (decl_rtl) == PARALLEL)
               mtcs_expr_emit_group_move/*!emit_group_move*/(mtcsExpr,real_decl_rtl, decl_rtl);
            else
               mtcs_expr_emit_group_load/*!emit_group_load*/(mtcsExpr,real_decl_rtl, decl_rtl,
            TREE_TYPE (decl_result),int_size_in_bytes (TREE_TYPE (decl_result)));
         }
         /* In the case of complex integer modes smaller than a word, we'll
         need to generate some non-trivial bitfield insertions.  Do that
         on a pseudo and not the hard register.  */
         else if (GET_CODE (decl_rtl) == CONCAT
         && mtcs_mode_is_complex_int/*!is_complex_int_mode*/(mtcsMode,GET_MODE (decl_rtl), &cmode)
         && mtcs_mode_get_bitsize/*!GET_MODE_BITSIZE*/(mtcsMode,cmode) <= BITS_PER_WORD){
            n_debug("mtcsfunc.c  mtcs_func_expand_function_end 99ee \n");
            int old_generating_concat_p;
            rtx tmp;

            old_generating_concat_p = generating_concat_p;
            generating_concat_p = 0;
            tmp = mtcs_emit_gen_reg_rtx/*!gen_reg_rtx*/(mtcsEmit,GET_MODE (decl_rtl));
            generating_concat_p = old_generating_concat_p;

            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,tmp, decl_rtl);
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,real_decl_rtl, tmp);
         }
         /* If a named return value dumped decl_return to memory, then
         we may need to re-do the PROMOTE_MODE signed/unsigned
         extension.  */
         else if (GET_MODE (real_decl_rtl) != GET_MODE (decl_rtl)){
            int unsignedp = TYPE_UNSIGNED (TREE_TYPE (decl_result));
            n_debug("mtcsfunc.c  mtcs_func_expand_function_end 99ff mode:%d unsignedp:%d\n",GET_MODE (decl_rtl),unsignedp);
            mtcs_mode_promote_function_mode/*!promote_function_mode*/(mtcsMode,TREE_TYPE (decl_result),
            GET_MODE (decl_rtl), &unsignedp,TREE_TYPE (current_function_decl), 1);
            mtcs_expr_convert_move/*!convert_move*/(mtcsExpr,real_decl_rtl, decl_rtl, unsignedp);
         }else{
            n_debug("mtcsfunc.c  mtcs_func_expand_function_end 99gg \n");
            mtcs_print_rtl_single(stderr,real_decl_rtl);
            mtcs_print_rtl_single(stderr,decl_rtl);

            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,real_decl_rtl, decl_rtl);
         }
      }
   }

   /* If returning a structure, arrange to return the address of the value
   in a place where debuggers expect to find it.

   If returning a structure PCC style,
   the caller also depends on this value.
   And cfun->returns_pcc_struct is not necessarily set.  */
   if ((cfun->returns_struct || cfun->returns_pcc_struct)
   && !mtcsMachine->calls->omit_struct_return_reg/*!targetm.calls.omit_struct_return_reg*/){
      n_debug("mtcsfunc.c  mtcs_func_expand_function_end 100 \n");

      rtx value_address = mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,DECL_RESULT (current_function_decl));
      tree type = TREE_TYPE (DECL_RESULT (current_function_decl));
      rtx outgoing;

      if (DECL_BY_REFERENCE (DECL_RESULT (current_function_decl)))
         type = TREE_TYPE (type);
      else
         value_address = XEXP (value_address, 0);

      outgoing = target_calls_function_value/*!targetm.calls.function_value*/(mtcsMachine->calls,
      build_pointer_type (type),current_function_decl, true);

      /* Mark this as a function return value so integrate will delete the
      assignment and USE below when inlining this function.  */
      REG_FUNCTION_VALUE_P (outgoing) = 1;

      /* The address may be ptr_mode and OUTGOING may be Pmode.  */
      scalar_int_mode mode = mtcs_mode_as_a <scalar_int_mode> (mtcsMode,GET_MODE (outgoing));
      value_address = mtcs_explow_convert_memory_address/*!convert_memory_address*/(mtcsExplow,mode, value_address);

      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,outgoing, value_address);

      /* Show return register used to hold result (in this case the address
      of the result.  */
      mtcsRtlData/*!crtl*/->return_rtx = outgoing;
   }

   /* Emit the actual code to clobber return register.  Don't emit
   it if clobber_after is a barrier, then the previous basic block
   certainly doesn't fall thru into the exit block.  */
   if (!BARRIER_P (clobber_after)){
      n_debug("mtcsfunc.c  mtcs_func_expand_function_end 101 clobber_after:%p\n",clobber_after);
      mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
      mtcs_func_clobber_return_register/*!clobber_return_register*/(self);
      rtx_insn *seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
      mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,seq, clobber_after);
   }

   /* Output the label for the naked return from the function.  */
   if (mtcsRtlData->x_naked_return_label/*!naked_return_label*/){
      n_debug("mtcsfunc.c  mtcs_func_expand_function_end 102 \n");
      mtcs_emit_emit_label/*!emit_label*/(mtcsEmit,mtcsRtlData->x_naked_return_label);
   }
   /* @@@ This is a kludge.  We want to ensure that instructions that
   may trap are not moved into the epilogue by scheduling, because
   we don't always emit unwind information for the epilogue.  */
   if (cfun->can_throw_non_call_exceptions
   && target_common_except_unwind_info/*!targetm_common.except_unwind_info*/
   (mtcsMachine->common,mtcsOptions->global_options/*!&global_options*/) != UI_SJLJ){
      n_debug("mtcsfunc.c  mtcs_func_expand_function_end 103 \n");
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,gen_blockage ());
   }

   /* If stack protection is enabled for this function, check the guard.  */
   if (mtcsRtlData/*!crtl*/->stack_protect_guard
   && mtcsTarget->stack_protect_runtime_enabled_p/*!targetm.stack_protect_runtime_enabled_p*/(mtcsTarget)
   && mtcsRtlData->x_naked_return_label/*!naked_return_label*/== NULL_RTX){
      n_debug("mtcsfunc.c  mtcs_func_expand_function_end 104 \n");
      mtcs_func_stack_protect_epilogue/*!stack_protect_epilogue*/(self);
   }

   /* If we had calls to alloca, and this machine needs
   an accurate stack pointer to exit the function,
   insert some code to save and restore the stack pointer.  */
   if (! mtcs_func_get_exit_ignore_stack/*!EXIT_IGNORE_STACK*/(self)
   && cfun->calls_alloca){
      n_debug("mtcsfunc.c  mtcs_func_expand_function_end 105 \n");
      rtx tem = 0;
      mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
      mtcs_explow_emit_stack_save/*!emit_stack_save*/(mtcsExplow,SAVE_FUNCTION, &tem);
      n_debug("mtcsfunc.c  mtcs_func_expand_function_end 105aa tem:%p\n",tem);

      rtx_insn *seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
      mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,seq, parm_birth_insn);
      mtcs_explow_emit_stack_restore/*!emit_stack_restore*/(mtcsExplow,SAVE_FUNCTION, tem);
   }

   /* ??? This should no longer be necessary since stupid is no longer with
   us, but there are some parts of the compiler (eg reload_combine, and
   sh mach_dep_reorg) that still try and compute their own lifetime info
   instead of using the general framework.  */
   n_debug("mtcsfunc.c mtcs_func_expand_function_end 106\n");
   use_return_register(self);
}

//原型 get_arg_pointer_save_area function.h function.cc
rtx mtcs_func_get_arg_pointer_save_area (MtcsFunc *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit  *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
  MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
  MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);

  rtx ret = arg_pointer_save_area;
  machine_mode pMode=mtcs_mode_get_Pmode(mtcsMode);
  if (! ret){
      ret = mtcs_func_assign_stack_local/*!assign_stack_local*/(self,pMode,
              mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,pMode), 0);
      mtcsRtlData->x_arg_pointer_save_area/*!arg_pointer_save_area*/= ret;
  }
  if (! mtcsRtlData/*!crtl*/->arg_pointer_save_area_init){
      /* Save the arg pointer at the beginning of the function.  The
     generated stack slot may not be a valid memory address, so we
     have to check it and fix it if necessary.  */
      mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,mtcs_explow_validize_mem/*!validize_mem*/(mtcsExplow,copy_rtx (ret)),
              mtcsRtlData/*!crtl*/->args.internal_arg_pointer);
      rtx_insn *seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
      mtcs_emit_push_topmost_sequence/*!push_topmost_sequence*/(mtcsEmit);
      mtcs_emit_emit_insn_after/*!emit_insn_after*/(mtcsEmit,
            seq, mtcs_cfg_rtl_entry_of_function/*!entry_of_function*/(mtcsCfgRtl));
      mtcs_emit_pop_topmost_sequence/*!pop_topmost_sequence*/(mtcsEmit);
      mtcsRtlData/*!crtl*/->arg_pointer_save_area_init = true;
  }
  return ret;
}

/* Helper for diddle_return_value.  */

static void diddle_return_value_1 (MtcsFunc *self,void (*doit) (rtx, void *), void *arg, rtx outgoing)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg  *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  n_debug("mtcsfunc.c  diddle_return_value_1 00 outgoing:%p\n",outgoing);

  if (!outgoing)
    return;
  if (REG_P (outgoing)){
     n_debug("mtcsfunc.c  diddle_return_value_1 11 outgoing:%p\n",outgoing);

    (*doit) (outgoing, arg);
  }else if (GET_CODE (outgoing) == PARALLEL){
      int i;
      for (i = 0; i < XVECLEN (outgoing, 0); i++){
          rtx x = XEXP (XVECEXP (outgoing, 0, i), 0);
          if (REG_P (x) && REGNO (x) < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)){
             n_debug("mtcsfunc.c  diddle_return_value_1 22 outgoing:%p\n",outgoing);
            (*doit) (x, arg);
          }
      }
  }
}

/* Call DOIT for each hard register used as a return value from
   the current function.  */
//原型 diddle_return_value function.h function.cc
void  mtcs_func_diddle_return_value (MtcsFunc *self,void (*doit) (rtx, void *), void *arg)
{
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(self);
   diddle_return_value_1(self,doit, arg, mtcsRtlData/*!crtl*/->return_rtx);
}

static void do_clobber_return_reg (rtx reg, void *arg ATTRIBUTE_UNUSED)
{
    MtcsFunc *self=(MtcsFunc *)arg;
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
    MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
    n_debug("mtcsfunc.c  do_clobber_return_reg %p %d\n",reg,(GET_CODE (reg) == CONCAT));
    mtcs_print_rtl_single(stderr,reg);
    mtcs_emit_emit_clobber/*!emit_clobber*/(mtcsEmit,reg);
}

//原型 clobber_return_register function.h funciton.cc
void mtcs_func_clobber_return_register (MtcsFunc *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsReg  *mtcsReg=mtcs_target_get_reg(mtcsTarget);
  MtcsAsm *mtcsAsm=mtcs_target_get_asm(mtcsTarget);
  n_debug("mtcsfunc.c  clobber_return_register 00 diddle_return_value\n");

  mtcs_func_diddle_return_value/*!diddle_return_value*/(self,do_clobber_return_reg, (void *)self);
  /* In case we do use pseudo to return value, clobber it too.  */
  if (DECL_RTL_SET_P (DECL_RESULT (current_function_decl))){
      tree decl_result = DECL_RESULT (current_function_decl);
      rtx decl_rtl = mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,decl_result);
      n_debug("mtcsfunc.c  clobber_return_register 11 :%d\n",REG_P (decl_rtl));

      if (REG_P (decl_rtl) && REGNO (decl_rtl) >=
              mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg)){
         n_debug("mtcsfunc.c  clobber_return_register 22 \n");
          do_clobber_return_reg (decl_rtl, (void*)self);
      }
  }
}

//原型 emit_initial_value_sets function.h function.cc
void mtcs_func_emit_initial_value_sets (MtcsFunc *self)
{
  MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsEmit  *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
  MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
  MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
  MtcsRtlData *mtcsRtlData=self->mtcsRtlData;

  struct initial_value_struct *ivs = mtcsRtlData/*!crtl*/->hard_reg_initial_vals;
  int i;
  rtx_insn *seq;
  if (ivs == 0)
    return;
  mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);

  for (i = 0; i < ivs->num_entries; i++)
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,ivs->entries[i].pseudo, ivs->entries[i].hard_reg);
  seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
  mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);

  mtcs_cfg_rtl_emit_insn_at_entry/*!emit_insn_at_entry*/(mtcsCfgRtl,seq);
}

//原型 #define EXIT_IGNORE_STACK 0 defaults.h host=1
void mtcs_func_set_exit_ignore_stack(MtcsFunc *self,int value)
{
    self->exitIgnoreStack=value;
}

//原型 #define SETUP_FRAME_ADDRESSES() defaults.h i386.h ix86_setup_frame_addresses ()
void mtcs_func_setup_frame_addresses(MtcsFunc *self)
{
   if(self->setup_frame_addresses)
      self->setup_frame_addresses(self);
}

int  mtcs_func_get_exit_ignore_stack(MtcsFunc *self)
{
   return self->exitIgnoreStack;
}

//原型 #define RETURN_ADDR_OFFSET 0
int  mtcs_func_get_return_addr_offset(MtcsFunc *self)
{
   return self->returnAddrOffset;
}

void  mtcs_func_set_return_addr_offset(MtcsFunc *self,int value)
{
    self->returnAddrOffset=value;
}

//原型 #define FUNCTION_ARG_REGNO_P(r) 0
bool mtcs_func_is_function_arg_regno(MtcsFunc *self,int regno)
{
   return self->is_function_arg_regno(self,regno);
}

//原型 max_reg_num rtl.h emit-rtl.cc
int mtcs_func_max_reg_num(MtcsFunc *self)
{
   return self->mtcsRtlData->emit.x_reg_rtx_no;
}

//原型 FIRST_PARM_OFFSET host=0 nvptx=0
int mtcs_func_get_first_parm_offset(MtcsFunc *self,tree fndecl)
{
   return self->get_first_parm_offset(self,fndecl);
}


void mtcs_func_print_rtl(MtcsFunc *self)
{
   struct sequence_stack *seq;
   MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
   for (seq = mtcs_rtl_data_get_current_sequence(mtcsRtlData); seq; seq = seq->next){
      n_debug("mtcsfunc.c emit.req req:%p seq->first:%p seq->last:%p seq->next:%p\n",seq,seq->first,seq->last,seq->next);
   }
}

/* Helper for instantiate_decls called via walk_tree: Process all decls
   in the given DECL_VALUE_EXPR.  */
//原型 instantiate_expr mtcsfunc.cc
static tree instantiate_expr (tree *tp, int *walk_subtrees, void *data ATTRIBUTE_UNUSED)
{
   MtcsFunc *self=(MtcsFunc*)data;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm  *mtcsAsm = mtcs_target_get_asm(mtcsTarget);

   tree t = *tp;
   if (! EXPR_P (t)){
      *walk_subtrees = 0;
      if (DECL_P (t)){
         if (DECL_RTL_SET_P (t))
            mtcs_func_instantiate_decl_rtl/*!instantiate_decl_rtl*/(self,mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,t));
         if (TREE_CODE (t) == PARM_DECL && DECL_NAMELESS (t) && DECL_INCOMING_RTL (t))
            mtcs_func_instantiate_decl_rtl/*!instantiate_decl_rtl*/(self,DECL_INCOMING_RTL (t));
         if ((VAR_P (t) || TREE_CODE (t) == RESULT_DECL)  && DECL_HAS_VALUE_EXPR_P (t)){
            tree v = DECL_VALUE_EXPR (t);
            walk_tree (&v, instantiate_expr, data/*!NULL*/, NULL);
         }
      }
   }
   return NULL;
}

/* Subroutine of instantiate_decls: Process all decls in the given
   BLOCK node and all its subblocks.  */
//原型 instantiate_decls_1 function.cc
static void instantiate_decls_1 (MtcsFunc *self,tree let)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm  *mtcsAsm = mtcs_target_get_asm(mtcsTarget);

   tree t;

   for (t = BLOCK_VARS (let); t; t = DECL_CHAIN (t)){
      if (DECL_RTL_SET_P (t))
         mtcs_func_instantiate_decl_rtl/*!instantiate_decl_rtl*/(self,mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,t));
      if (VAR_P (t) && DECL_HAS_VALUE_EXPR_P (t)){
         tree v = DECL_VALUE_EXPR (t);
         walk_tree (&v, instantiate_expr, (void*)self/*!NULL*/, NULL);
      }
   }

   /* Process all subblocks.  */
   for (t = BLOCK_SUBBLOCKS (let); t; t = BLOCK_CHAIN (t))
      instantiate_decls_1 (self,t);
}

/* Scan all decls in FNDECL (both variables and parameters) and instantiate
   all virtual registers in their DECL_RTL's.  */
//原型 instantiate_decls function.cc
static void instantiate_decls (MtcsFunc *self,tree fndecl)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsAsm  *mtcsAsm = mtcs_target_get_asm(mtcsTarget);

   tree decl;
   unsigned ix;

   /* Process all parameters of the function.  */
   for (decl = DECL_ARGUMENTS (fndecl); decl; decl = DECL_CHAIN (decl)){
      mtcs_func_instantiate_decl_rtl/*!instantiate_decl_rtl*/(self,mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,decl));
      mtcs_func_instantiate_decl_rtl/*!instantiate_decl_rtl*/(self,DECL_INCOMING_RTL (decl));
      if (DECL_HAS_VALUE_EXPR_P (decl)){
         tree v = DECL_VALUE_EXPR (decl);
         walk_tree (&v, instantiate_expr, (void *)self/*!NULL*/, NULL);
      }
   }

   if ((decl = DECL_RESULT (fndecl))  && TREE_CODE (decl) == RESULT_DECL){
      if (DECL_RTL_SET_P (decl))
      mtcs_func_instantiate_decl_rtl/*!instantiate_decl_rtl*/(self,mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,decl));
      if (DECL_HAS_VALUE_EXPR_P (decl)){
         tree v = DECL_VALUE_EXPR (decl);
         walk_tree (&v, instantiate_expr, (void *)self/*!NULL*/, NULL);
      }
   }

   /* Process the saved static chain if it exists.  */
   decl = DECL_STRUCT_FUNCTION (fndecl)->static_chain_decl;
   if (decl && DECL_HAS_VALUE_EXPR_P (decl))
      mtcs_func_instantiate_decl_rtl/*!instantiate_decl_rtl*/(self,mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,DECL_VALUE_EXPR (decl)));

   /* Now process all variables defined in the function or its subblocks.  */
   if (DECL_INITIAL (fndecl))
      instantiate_decls_1(self,DECL_INITIAL (fndecl));

   FOR_EACH_LOCAL_DECL (cfun, ix, decl)
      if (DECL_RTL_SET_P (decl))
         mtcs_func_instantiate_decl_rtl/*!instantiate_decl_rtl*/(self,mtcs_asm_decl_rtl/*!DECL_RTL*/(mtcsAsm,decl));
   vec_free (cfun->local_decls);
}


/* Given a piece of RTX and a pointer to a HOST_WIDE_INT, if the RTX
   is a virtual register, return the equivalent hard register and set the
   offset indirectly through the pointer.  Otherwise, return 0.  */
//原型 instantiate_new_reg function.cc
static rtx instantiate_new_reg (MtcsFunc *self,rtx x, poly_int64 *poffset)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsAlign   *mtcsAlign=mtcs_target_get_align(mtcsTarget);
   MtcsRtlData *mtcsRtlData=self->mtcsRtlData;

   rtx new_rtx;
   poly_int64 offset;

   if (x == mtcs_rtl_get_virtual_incoming_args_rtx/*!virtual_incoming_args_rtx*/(mtcsRTL)){
      if (mtcs_align_stack_realign_drap/*!stack_realign_drap*/(mtcsAlign)){
         /* Replace virtual_incoming_args_rtx with internal arg
         pointer if DRAP is used to realign stack.  */
         new_rtx = mtcsRtlData/*!crtl*/->args.internal_arg_pointer;
         offset = 0;
      }else
         new_rtx = mtcs_rtl_get_arg_pointer_rtx/*!arg_pointer_rtx*/(mtcsRTL), offset =self->in_arg_offset;
   }else if (x == mtcs_rtl_get_virtual_stack_args_rtx/*!virtual_stack_vars_rtx*/(mtcsRTL))
      new_rtx = mtcs_rtl_get_frame_pointer_rtx/*!frame_pointer_rtx*/(mtcsRTL), offset = self->var_offset;
   else if (x == mtcs_rtl_get_virtual_stack_dynamic_rtx/*!virtual_stack_dynamic_rtx*/(mtcsRTL))
      new_rtx =mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL), offset = self->dynamic_offset;
   else if (x == mtcs_rtl_get_virtual_outgoing_args_rtx/*!virtual_outgoing_args_rtx*/(mtcsRTL))
      new_rtx = mtcs_rtl_get_stack_pointer_rtx/*!stack_pointer_rtx*/(mtcsRTL), offset = self->out_arg_offset;
   else if (x == mtcs_rtl_get_virtual_cfa_rtx/*!virtual_cfa_rtx*/(mtcsRTL)){
      if(self->get_frame_pointer_cfa_offset) /*!#ifdef FRAME_POINTER_CFA_OFFSET*/
         new_rtx =mtcs_rtl_get_frame_pointer_rtx/*!frame_pointer_rtx*/(mtcsRTL);
      else /*!#else*/
         new_rtx = mtcs_rtl_get_arg_pointer_rtx/*!arg_pointer_rtx*/(mtcsRTL);
      /*!#endif*/
      offset = self->cfa_offset;
   }else if (x ==mtcs_rtl_get_virtual_preferred_stack_boundary_rtx/*!virtual_preferred_stack_boundary_rtx*/(mtcsRTL)){
      new_rtx = mtcs_rtl_GEN_INT/*!GEN_INT*/(mtcsRTL,mtcsRtlData/*!crtl*/->preferred_stack_boundary / BITS_PER_UNIT);
      offset = 0;
   }else
      return NULL_RTX;

   *poffset = offset;
   return new_rtx;
}


/* A subroutine of instantiate_virtual_regs.  Instantiate any virtual
   registers present inside of *LOC.  The expression is simplified,
   as much as possible, but is not to be considered "valid" in any sense
   implied by the target.  Return true if any change is made.  */
//原型 instantiate_virtual_regs_in_rtx function.cc
static bool instantiate_virtual_regs_in_rtx (MtcsFunc *self,rtx *loc)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   if (!*loc)
      return false;
   bool changed = false;
   subrtx_ptr_iterator::array_type array;
   FOR_EACH_SUBRTX_PTR (iter, array, loc, NONCONST){
      rtx *loc = *iter;
      if (rtx x = *loc){
         rtx new_rtx;
         poly_int64 offset;
         switch (GET_CODE (x)){
            case REG:
               new_rtx = instantiate_new_reg(self,x, &offset);
               if (new_rtx){
                  *loc = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,GET_MODE (x), new_rtx, offset);
                  changed = true;
               }
               iter.skip_subrtxes ();
               break;

            case PLUS:
               new_rtx = instantiate_new_reg(self,XEXP (x, 0), &offset);
               if (new_rtx){
                  XEXP (x, 0) = new_rtx;
                  *loc = mtcs_rtl_plus_constant/*!plus_constant*/(mtcsRTL,GET_MODE (x), x, offset, true);
                  changed = true;
                  iter.skip_subrtxes ();
                  break;
               }
               /* FIXME -- from old code */
               /* If we have (plus (subreg (virtual-reg)) (const_int)), we know
               we can commute the PLUS and SUBREG because pointers into the
               frame are well-behaved.  */
               break;
            default:
               break;
         }
      }
   }
   return changed;
}

/* A subroutine of instantiate_virtual_regs_in_insn.  Return true if X
   matches the predicate for insn CODE operand OPERAND.  */
//原型 safe_insn_predicate function.cc
static bool safe_insn_predicate (MtcsFunc *self,int code, int operand, rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   return code < 0 || mtcs_optabs_insn_operand_matches/*!insn_operand_matches*/(mtcsOptabs,
         (enum insn_code) code, operand, x);
}

/* A subroutine of instantiate_virtual_regs.  Instantiate any virtual
   registers present inside of insn.  The result will be a valid insn.  */
//原型 instantiate_virtual_regs_in_insn function.cc
static void instantiate_virtual_regs_in_insn (MtcsFunc *self,rtx_insn *insn)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsExplow *mtcsExplow=mtcs_target_get_explow(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsSimplifyRtx *mtcsSimplifyRtx=mtcs_target_get_simplify_rtx(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);

   poly_int64 offset;
   int insn_code, i;
   bool any_change = false;
   rtx set, new_rtx, x;
   rtx_insn *seq;

   /* There are some special cases to be handled first.  */
   set = single_set (insn);
   if (set){
      /* We're allowed to assign to a virtual register.  This is interpreted
      to mean that the underlying register gets assigned the inverse
      transformation.  This is used, for example, in the handling of
      non-local gotos.  */
      new_rtx = instantiate_new_reg(self,SET_DEST (set), &offset);
      n_debug("mtcsfunc.c  instantiate_virtual_regs_in_insn 00\n");
      if (new_rtx){
         mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
         n_debug("mtcsfunc.c  instantiate_virtual_regs_in_insn 11\n");

         instantiate_virtual_regs_in_rtx(self,&SET_SRC (set));
         x = mtcs_simplify_rtx_gen_binary/*!simplify_gen_binary*/(mtcsSimplifyRtx,PLUS, GET_MODE (new_rtx), SET_SRC (set),
               mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,-offset, GET_MODE (new_rtx)));
         x = mtcs_expr_force_operand/*!force_operand*/(mtcsExpr,x, new_rtx);
         if (x != new_rtx)
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,new_rtx, x);

         seq =mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
         mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);

         mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,seq, insn);
         mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,insn);
         return;
      }
      n_debug("mtcsfunc.c  instantiate_virtual_regs_in_insn 22\n");

      /* Handle a straight copy from a virtual register by generating a
      new add insn.  The difference between this and falling through
      to the generic case is avoiding a new pseudo and eliminating a
      move insn in the initial rtl stream.  */
      new_rtx = instantiate_new_reg(self,SET_SRC (set), &offset);
      if (new_rtx
      && maybe_ne (offset, 0)
      && REG_P (SET_DEST (set))
      && REGNO (SET_DEST (set)) > mtcs_reg_get_last_virtual_regno/*!LAST_VIRTUAL_REGISTER*/(mtcsReg)){
         mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);

         x = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,GET_MODE (SET_DEST (set)), PLUS, new_rtx,
               mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,offset, GET_MODE (SET_DEST (set))),SET_DEST (set), 1, OPTAB_LIB_WIDEN);
         if (x != SET_DEST (set))
            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,SET_DEST (set), x);

         seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
         mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);

         mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,seq, insn);
         mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,insn);
         return;
      }
      n_debug("mtcsfunc.c  instantiate_virtual_regs_in_insn 33\n");

      //原型 extract_insn recog.h recog.cc
      //MAX_RECOG_OPERANDS是平台相关的，其它引用函数 asm_noperands、decode_asm_operands 不需要改变
      //host=30 nvptx=30
      mtcs_recog_extract_insn/*!extract_insn*/(mtcsRecog,insn);
      insn_code = INSN_CODE (insn);
      n_debug("mtcsfunc.c  instantiate_virtual_regs_in_insn 44\n");

      /* Handle a plus involving a virtual register by determining if the
      operands remain valid if they're modified in place.  */
      poly_int64 delta;
      if (GET_CODE (SET_SRC (set)) == PLUS
      &&  mtcsRecog->recog_data.n_operands >= 3
      &&  mtcsRecog->recog_data.operand_loc[1] == &XEXP (SET_SRC (set), 0)
      &&  mtcsRecog->recog_data.operand_loc[2] == &XEXP (SET_SRC (set), 1)
      && poly_int_rtx_p ( mtcsRecog->recog_data.operand[2], &delta)
      && (new_rtx = instantiate_new_reg(self, mtcsRecog->recog_data.operand[1], &offset))){
         offset += delta;
         n_debug("mtcsfunc.c  instantiate_virtual_regs_in_insn 55\n");

         /* If the sum is zero, then replace with a plain move.  */
         if (known_eq (offset, 0)
         && REG_P (SET_DEST (set))
         && REGNO (SET_DEST (set)) > mtcs_reg_get_last_virtual_regno/*!LAST_VIRTUAL_REGISTER*/(mtcsReg)){
            mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);

            mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,SET_DEST (set), new_rtx);
            seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
            mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);

            mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,seq, insn);
            mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,insn);
            return;
         }

         x = mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,offset,  mtcsRecog->recog_data.operand_mode[2]);

         /* Using validate_change and apply_change_group here leaves
         recog_data in an invalid state.  Since we know exactly what
         we want to check, do those two by hand.  */
         if (safe_insn_predicate(self,insn_code, 1, new_rtx) && safe_insn_predicate(self,insn_code, 2, x)){
            * mtcsRecog->recog_data.operand_loc[1] =  mtcsRecog->recog_data.operand[1] = new_rtx;
            * mtcsRecog->recog_data.operand_loc[2] =  mtcsRecog->recog_data.operand[2] = x;
            any_change = true;
            /* Fall through into the regular operand fixup loop in
            order to take care of operands other than 1 and 2.  */
         }
      }
   }else{
      mtcs_recog_extract_insn/*!extract_insn*/(mtcsRecog,insn);
      insn_code = INSN_CODE (insn);
   }
   n_debug("mtcsfunc.c  instantiate_virtual_regs_in_insn 66 insn_code:%d\n",insn_code);

   /* In the general case, we expect virtual registers to appear only in
   operands, and then only as either bare registers or inside memories.  */
   for (i = 0; i <  mtcsRecog->recog_data.n_operands; ++i){
      x =  mtcsRecog->recog_data.operand[i];
      switch (GET_CODE (x)){
         case MEM:
         {
            n_debug("mtcsfunc.c  instantiate_virtual_regs_in_insn 77 操作数 MEM i:%d\n",i);

            rtx addr = XEXP (x, 0);

            if (!instantiate_virtual_regs_in_rtx(self,&addr))
               continue;
            mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
            n_debug("mtcsfunc.c  instantiate_virtual_regs_in_insn 77aa 操作数 MEM i:%d\n",i);

            x = mtcs_rtl_replace_equiv_address/*!replace_equiv_address*/(mtcsRTL,x, addr, true);
            n_debug("mtcsfunc.c  instantiate_virtual_regs_in_insn 77bb 操作数 MEM\n");

            /* It may happen that the address with the virtual reg
            was valid (e.g. based on the virtual stack reg, which might
            be acceptable to the predicates with all offsets), whereas
            the address now isn't anymore, for instance when the address
            is still offsetted, but the base reg isn't virtual-stack-reg
            anymore.  Below we would do a force_reg on the whole operand,
            but this insn might actually only accept memory.  Hence,
            before doing that last resort, try to reload the address into
            a register, so this operand stays a MEM.  */
            if (!safe_insn_predicate(self,insn_code, i, x)){
               n_debug("mtcsfunc.c  instantiate_virtual_regs_in_insn 77cc 操作数 MEM\n");

               addr = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,GET_MODE (addr), addr);
               n_debug("mtcsfunc.c  instantiate_virtual_regs_in_insn 77dd 操作数 MEM\n");

               x = mtcs_rtl_replace_equiv_address/*!replace_equiv_address*/(mtcsRTL,x, addr, true);
            }
            n_debug("mtcsfunc.c  instantiate_virtual_regs_in_insn 77ee 操作数 MEM\n");

            seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
            mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);

            if (seq)
               mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,seq, insn);
         }
            break;

         case REG:
            n_debug("mtcsfunc.c  instantiate_virtual_regs_in_insn 88 操作数 REG\n");
            new_rtx = instantiate_new_reg(self,x, &offset);
            if (new_rtx == NULL)
               continue;
            if (known_eq (offset, 0))
               x = new_rtx;
            else{
               mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);

               /* Careful, special mode predicates may have stuff in
               insn_data[insn_code].operand[i].mode that isn't useful
               to us for computing a new value.  */
               /* ??? Recognize address_operand and/or "p" constraints
               to see if (plus new offset) is a valid before we put
               this through expand_simple_binop.  */
               x = mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,GET_MODE (x), PLUS, new_rtx,
               mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,offset, GET_MODE (x)),
               NULL_RTX, 1, OPTAB_LIB_WIDEN);
               seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
               mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
               mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,seq, insn);
            }
            break;

         case SUBREG:
            n_debug("mtcsfunc.c  instantiate_virtual_regs_in_insn 99 操作数 SUBREG\n");
            new_rtx = instantiate_new_reg(self,SUBREG_REG (x), &offset);
            if (new_rtx == NULL)
               continue;
            if (maybe_ne (offset, 0)){
               mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
               new_rtx =  mtcs_optabs_expand_simple_binop/*!expand_simple_binop*/(mtcsOptabs,GET_MODE (new_rtx), PLUS, new_rtx,
                     mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,offset, GET_MODE (new_rtx)),NULL_RTX, 1, OPTAB_LIB_WIDEN);
               seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
               mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
               mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,seq, insn);
            }
            x = mtcs_simplify_rtx_gen_subreg/*!simplify_gen_subreg*/(mtcsSimplifyRtx, mtcsRecog->recog_data.operand_mode[i],
                  new_rtx,GET_MODE (new_rtx), SUBREG_BYTE (x));
            gcc_assert (x);
            break;

         default:
            continue;
      }
      n_debug("mtcsfunc.c  instantiate_virtual_regs_in_insn 100\n");

      /* At this point, X contains the new value for the operand.
      Validate the new value vs the insn predicate.  Note that
      asm insns will have insn_code -1 here.  */
      if (!safe_insn_predicate(self,insn_code, i, x)){
         mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
         if (REG_P (x)){
            gcc_assert (REGNO (x) <= mtcs_reg_get_last_virtual_regno/*!LAST_VIRTUAL_REGISTER*/(mtcsReg));
            x = mtcs_explow_copy_to_reg/*!copy_to_reg*/(mtcsExplow,x);
         }else
            x = mtcs_explow_force_reg/*!force_reg*/(mtcsExplow,mtcsOutput->insn_data[insn_code].operand[i].mode, x);
         seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
         mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
         if (seq)
            mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,seq, insn);
      }

      * mtcsRecog->recog_data.operand_loc[i] =  mtcsRecog->recog_data.operand[i] = x;
      any_change = true;
   }//end    for (i = 0; i < recog_data.n_operands; ++i){


   if (any_change){
      /* Propagate operand changes into the duplicates.  */
      for (i = 0; i <  mtcsRecog->recog_data.n_dups; ++i)
         * mtcsRecog->recog_data.dup_loc[i] = copy_rtx ( mtcsRecog->recog_data.operand[(unsigned) mtcsRecog->recog_data.dup_num[i]]);

      /* Force re-recognition of the instruction for validation.  */
      INSN_CODE (insn) = -1;
   }
   n_debug("mtcsfunc.c  instantiate_virtual_regs_in_insn 101\n");

   if (asm_noperands (PATTERN (insn)) >= 0){
      if (!mtcs_recog_check_asm_operands/*!check_asm_operands*/(mtcsRecog,PATTERN (insn))){
         error_for_asm (insn, "impossible constraint in %<asm%>");
         /* For asm goto, instead of fixing up all the edges
         just clear the template and clear input and output operands
         and strip away clobbers.  */
         if (JUMP_P (insn)){
            rtx asm_op = extract_asm_operands (PATTERN (insn));
            PATTERN (insn) = asm_op;
            mtcs_rtl_put_mode/*!PUT_MODE*/(mtcsRTL,asm_op, VOIDmode);
            ASM_OPERANDS_TEMPLATE (asm_op) = ggc_strdup ("");
            ASM_OPERANDS_OUTPUT_CONSTRAINT (asm_op) = "";
            ASM_OPERANDS_OUTPUT_IDX (asm_op) = 0;
            ASM_OPERANDS_INPUT_VEC (asm_op) = rtvec_alloc (0);
            ASM_OPERANDS_INPUT_CONSTRAINT_VEC (asm_op) = rtvec_alloc (0);
         }else
            mtcs_cfg_rtl_delete_insn/*!delete_insn*/(mtcsCfgRtl,insn);
      }
   }else{
      if (mtcs_recog_recog_memoized/*!recog_memoized*/(mtcsRecog,insn) < 0)
         fatal_insn_not_found (insn);
   }
}

/* Pass through the INSNS of function FNDECL and convert virtual register
   references to hard register references.  */
//虚拟寄存器消除
//原型 instantiate_virtual_regs function.cc instantiate_virtual_regs是rtl pass vregs的excute方法 改为公共，
//MtcsPassInstantiateVirtualRegs 可以调用
void mtcs_func_instantiate_virtual_regs (MtcsFunc *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRecog *mtcsRecog=mtcs_target_get_recog(mtcsTarget);
   MtcsRtlData *mtcsRtlData=self->mtcsRtlData;

   rtx_insn *insn;
   //FIRST_PARM_OFFSET host=0 nvptx=0
   /* Compute the offsets to use for this function.  */
   //arg_pointer到第一个传入栈参数(incoming arg)之间的偏移
   self->in_arg_offset = mtcs_func_get_first_parm_offset/*!FIRST_PARM_OFFSET*/(self,current_function_decl);
   //第一个局部变量尾地址到 frame_pointer_rtx之间的偏移.
   self->var_offset = mtcsTarget/*!targetm.starting_frame_offset*/->starting_frame_offset(mtcsTarget);
   /* dynamic_offset = crtl->outgoing_args_size + STACK_POINTER_OFFSET; 其中:
      * outgoing_args_size 是当前函数outgoing栈的大小(编译时确定)
      * STACK_POINTER_OFFSET 是硬件寄存器sp到第一个outgoing参数首地址的偏移
   */
   self->dynamic_offset = mtcs_func_get_stack_dynamic_offset(self,current_function_decl);/*!get_stack_dynamic_offset ();*/
   self->out_arg_offset = mtcs_func_get_stack_pointer_offset/*!STACK_POINTER_OFFSET*/(self);
   if(self->get_frame_pointer_cfa_offset) //等同于 #ifdef FRAME_POINTER_CFA_OFFSET
   //#ifdef FRAME_POINTER_CFA_OFFSET
      self->cfa_offset = self->get_frame_pointer_cfa_offset/*!FRAME_POINTER_CFA_OFFSET*/(self,current_function_decl);
   //#else
   else
      self->cfa_offset = self->get_arg_pointer_cfg_offset/*!ARG_POINTER_CFA_OFFSET*/(self,current_function_decl);
   //#endif
   n_debug("mtcsfunc.c  mtcs_func_instantiate_virtual_regs 00 %d %d %d %d %d\n",
         self->in_arg_offset,self->var_offset,self->dynamic_offset,self->out_arg_offset,self->cfa_offset);

   /* Initialize recognition, indicating that volatile is OK.  */
   mtcs_recog_init_recog/*!init_recog*/(mtcsRecog);

   /* Scan through all the insns, instantiating every virtual register still
   present.  */
   for (insn = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData); insn; insn = NEXT_INSN (insn))
      if (INSN_P (insn)){
         /* These patterns in the instruction stream can never be recognized.
         Fortunately, they shouldn't contain virtual registers either.  */
         n_debug("mtcsfunc.c  mtcs_func_instantiate_virtual_regs 11 %p  %d %s %s\n",
               insn,GET_CODE(insn),GET_RTX_NAME(GET_CODE(insn)),GET_RTX_FORMAT (GET_CODE (insn)));

         if (GET_CODE (PATTERN (insn)) == USE
         || GET_CODE (PATTERN (insn)) == CLOBBER
         || GET_CODE (PATTERN (insn)) == ASM_INPUT
         || DEBUG_MARKER_INSN_P (insn)){
            n_debug("mtcsfunc.c  mtcs_func_instantiate_virtual_regs 22 %p  %d %s %s\n",
                  insn,GET_CODE(insn),GET_RTX_NAME(GET_CODE(insn)),GET_RTX_FORMAT (GET_CODE (insn)));

            continue;
         }else if (DEBUG_BIND_INSN_P (insn)){
            n_debug("mtcsfunc.c  mtcs_func_instantiate_virtual_regs 33 %p  %d %s %s\n",
                  insn,GET_CODE(insn),GET_RTX_NAME(GET_CODE(insn)),GET_RTX_FORMAT (GET_CODE (insn)));

            instantiate_virtual_regs_in_rtx(self,INSN_VAR_LOCATION_PTR (insn));
         }else{
            n_debug("mtcsfunc.c  mtcs_func_instantiate_virtual_regs 44 %p  %d %s %s\n",
                  insn,GET_CODE(insn),GET_RTX_NAME(GET_CODE(insn)),GET_RTX_FORMAT (GET_CODE (insn)));

            instantiate_virtual_regs_in_insn(self,insn);
            n_debug("mtcsfunc.c  mtcs_func_instantiate_virtual_regs 44-- instantiate_virtual_regs_in_insn\n");

         }

         if (insn->deleted ())
            continue;

         instantiate_virtual_regs_in_rtx(self,&REG_NOTES (insn));
         n_debug("mtcsfunc.c  mtcs_func_instantiate_virtual_regs 55 instantiate_virtual_regs_in_rtx\n");
         mtcs_print_rtl(stderr,insn);
         /* Instantiate any virtual registers in CALL_INSN_FUNCTION_USAGE.  */
         if (CALL_P (insn)){
            n_debug("mtcsfunc.c  mtcs_func_instantiate_virtual_regs 66 instantiate_virtual_regs_in_rtx\n");
            instantiate_virtual_regs_in_rtx(self,&CALL_INSN_FUNCTION_USAGE (insn));
         }
      }

   n_debug("mtcsfunc.c  mtcs_func_instantiate_virtual_regs 77 instantiate_decls\n");
   /* Instantiate the virtual registers in the DECLs for debugging purposes.  */
   instantiate_decls(self,current_function_decl);
   mtcsTarget/*!targetm.instantiate_decls*/->instantiate_decls(mtcsTarget);

   /* Indicate that, from now on, assign_stack_local should use
   frame_pointer_rtx.  */
   self->virtuals_instantiated = 1;
}



//原型 #define ARG_POINTER_CFA_OFFSET(FNDECL)  (FIRST_PARM_OFFSET (FNDECL) + crtl->args.pretend_args_size) defaults.h
static int getArgPointerCfgOffset_cb(MtcsFunc *self,tree fndecl)
{
    return (mtcs_func_get_first_parm_offset(self,fndecl)+self->mtcsRtlData->args.pretend_args_size);
}



/* Subroutine of instantiate_decls.  Given RTL representing a decl,
   do any instantiation required.  */
//原型 instantiate_decl_rtl function.h function.cc
void mtcs_func_instantiate_decl_rtl (MtcsFunc *self,rtx x)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);

   rtx addr;
   if (x == 0)
      return;
   /* If this is a CONCAT, recurse for the pieces.  */
   if (GET_CODE (x) == CONCAT){
      mtcs_func_instantiate_decl_rtl/*!instantiate_decl_rtl*/(self,XEXP (x, 0));
      mtcs_func_instantiate_decl_rtl/*!instantiate_decl_rtl*/(self,XEXP (x, 1));
      return;
   }
   /* If this is not a MEM, no need to do anything.  Similarly if the
   address is a constant or a register that is not a virtual register.  */
   if (!MEM_P (x))
      return;

   addr = XEXP (x, 0);
   if (CONSTANT_P (addr)  || (REG_P (addr)  && !mtcs_reg_virtual_register_p/*!VIRTUAL_REGISTER_P*/(mtcsReg,addr)))
      return;

   instantiate_virtual_regs_in_rtx(self,&XEXP (x, 0));
}

//原型 #define EPILOGUE_USES(REG) false defaults.h
bool mtcs_func_epilogue_uses(MtcsFunc *self,nuint regno)
{
   return self->epilogue_uses(self,regno);
}

//原型 #define INITIAL_ELIMINATION_OFFSET(FROM, TO, OFFSET)
HOST_WIDE_INT mtcs_func_initial_elimination_offset(MtcsFunc *self,int from, int to)
{
   return self->initial_elimination_offset(self,from,to);
}

/* INSN has been duplicated or replaced by as COPY, perhaps by duplicating a
   basic block, splitting or peepholes.  If INSN is a prologue or epilogue
   insn, then record COPY as well.  */
//原型 maybe_copy_prologue_epilogue_insn function.h funciton.cc
void mtcs_func_maybe_copy_prologue_epilogue_insn (MtcsFunc *self,rtx insn, rtx copy)
{
   hash_table<mtcs_insn_cache_hasher> *hash;
   rtx *slot;
   hash = self->epilogue_insn_hash;
   if (!hash || !hash->find (insn)){
      hash = self->prologue_insn_hash;
      if (!hash || !hash->find (insn))
         return;
   }
   slot = hash->find_slot (copy, INSERT);
   gcc_assert (*slot == NULL);
   *slot = copy;
}

/* Return the number of actual (non-debug) insns emitted in this
   function.  */
//原型 get_max_insn_count rtl.h emit-rtl.cc
int mtcs_func_get_max_insn_count (MtcsFunc *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   int n =self->mtcsRtlData->emit.x_cur_insn_uid/*!cur_insn_uid*/;

   /* The table size must be stable across -g, to avoid codegen
   differences due to debug insns, and not be affected by
   -fmin-insn-uid, to avoid excessive table size and to simplify
   debugging of -fcompare-debug failures.  */
   if (self->mtcsRtlData->emit.x_cur_debug_insn_uid/*!cur_debug_insn_uid*/ >mtcsOptionsItem->x_param_min_nondebug_insn_uid)
      n -= self->mtcsRtlData->emit.x_cur_debug_insn_uid/*!cur_debug_insn_uid*/;
   else
      n -= mtcsOptionsItem->x_param_min_nondebug_insn_uid;

   return n;
}

/* Return the hardreg-pseudoreg initial values pair entry I and
   TRUE if I is a valid entry, or FALSE if I is not a valid entry.  */
//原型 initial_value_entry function.h function.cc
bool mtcs_func_initial_value_entry (MtcsFunc *self,int i, rtx *hreg, rtx *preg)
{
   MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
   struct initial_value_struct *ivs =mtcsRtlData/*!crtl*/->hard_reg_initial_vals;
   if (!ivs || i >= ivs->num_entries)
      return false;

   *hreg = ivs->entries[i].hard_reg;
   *preg = ivs->entries[i].pseudo;
   return true;
}

/* If CONSTRAINT is a matching constraint, then return its number.
   Otherwise, return -1.  */
//原型 matching_constraint_num function.cc
static int matching_constraint_num (const char *constraint)
{
   if (*constraint == '%')
      constraint++;

   if (IN_RANGE (*constraint, '0', '9'))
      return strtoul (constraint, NULL, 10);

   return -1;
}


/* This mini-pass fixes fall-out from SSA in asm statements that have
   in-out constraints.  Say you start with

     orig = inout;
     asm ("": "+mr" (inout));
     use (orig);

   which is transformed very early to use explicit output and match operands:

     orig = inout;
     asm ("": "=mr" (inout) : "0" (inout));
     use (orig);

   Or, after SSA and copyprop,

     asm ("": "=mr" (inout_2) : "0" (inout_1));
     use (inout_1);

   Clearly inout_2 and inout_1 can't be coalesced easily anymore, as
   they represent two separate values, so they will get different pseudo
   registers during expansion.  Then, since the two operands need to match
   per the constraints, but use different pseudo registers, reload can
   only register a reload for these operands.  But reloads can only be
   satisfied by hardregs, not by memory, so we need a register for this
   reload, just because we are presented with non-matching operands.
   So, even though we allow memory for this operand, no memory can be
   used for it, just because the two operands don't match.  This can
   cause reload failures on register-starved targets.

   So it's a symptom of reload not being able to use memory for reloads
   or, alternatively it's also a symptom of both operands not coming into
   reload as matching (in which case the pseudo could go to memory just
   fine, as the alternative allows it, and no reload would be necessary).
   We fix the latter problem here, by transforming

     asm ("": "=mr" (inout_2) : "0" (inout_1));

   back to

     inout_2 = inout_1;
     asm ("": "=mr" (inout_2) : "0" (inout_2));  */
//原型 match_asm_constraints_1 function.cc
static void match_asm_constraints_1 (MtcsFunc *self,rtx_insn *insn, rtx *p_sets, int noutputs)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsEmit *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRtlanal *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);
   MtcsPreds *mtcsPreds=mtcs_target_get_preds(mtcsTarget);
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);

   int i;
   bool changed = false;
   rtx op = SET_SRC (p_sets[0]);
   int ninputs = ASM_OPERANDS_INPUT_LENGTH (op);
   rtvec inputs = ASM_OPERANDS_INPUT_VEC (op);
   bool *output_matched = XALLOCAVEC (bool, noutputs);

   memset (output_matched, 0, noutputs * sizeof (bool));
   for (i = 0; i < ninputs; i++){
      rtx input, output;
      rtx_insn *insns;
      const char *constraint = ASM_OPERANDS_INPUT_CONSTRAINT (op, i);
      int match, j;

      match = matching_constraint_num (constraint);
      if (match < 0)
         continue;

      gcc_assert (match < noutputs);
      output = SET_DEST (p_sets[match]);
      input = RTVEC_ELT (inputs, i);
      /* Only do the transformation for pseudos.  */
      if (! REG_P (output)
      || rtx_equal_p (output, input)
      || !(REG_P (input) || SUBREG_P (input)
      || MEM_P (input) || CONSTANT_P (input))
      || !mtcs_preds_general_operand/*!general_operand*/(mtcsPreds,input, GET_MODE (output)))
         continue;

      /* We can't do anything if the output is also used as input,
      as we're going to overwrite it.  */
      for (j = 0; j < ninputs; j++)
         if (mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,output, RTVEC_ELT (inputs, j)))
            break;
      if (j != ninputs)
         continue;

      /* Avoid changing the same input several times.  For
      asm ("" : "=mr" (out1), "=mr" (out2) : "0" (in), "1" (in));
      only change it once (to out1), rather than changing it
      first to out1 and afterwards to out2.  */
      if (i > 0){
         for (j = 0; j < noutputs; j++)
            if (output_matched[j] && input == SET_DEST (p_sets[j]))
               break;
         if (j != noutputs)
            continue;
      }
      output_matched[match] = true;

      mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
      mtcs_expr_emit_move_insn/*!emit_move_insn*/(mtcsExpr,output, copy_rtx (input));
      insns = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
      mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,insns, insn);

      constraint = ASM_OPERANDS_OUTPUT_CONSTRAINT(SET_SRC(p_sets[match]));
      bool early_clobber_p = strchr (constraint, '&') != NULL;

      /* Now replace all mentions of the input with output.  We can't
      just replace the occurrence in inputs[i], as the register might
      also be used in some other input (or even in an address of an
      output), which would mean possibly increasing the number of
      inputs by one (namely 'output' in addition), which might pose
      a too complicated problem for reload to solve.  E.g. this situation:

      asm ("" : "=r" (output), "=m" (input) : "0" (input))

      Here 'input' is used in two occurrences as input (once for the
      input operand, once for the address in the second output operand).
      If we would replace only the occurrence of the input operand (to
      make the matching) we would be left with this:

      output = input
      asm ("" : "=r" (output), "=m" (input) : "0" (output))

      Now we suddenly have two different input values (containing the same
      value, but different pseudos) where we formerly had only one.
      With more complicated asms this might lead to reload failures
      which wouldn't have happen without this pass.  So, iterate over
      all operands and replace all occurrences of the register used.

      However, if one or more of the 'input' uses have a non-matching
      constraint and the matched output operand is an early clobber
      operand, then do not replace the input operand, since by definition
      it conflicts with the output operand and cannot share the same
      register.  See PR89313 for details.  */

      for (j = 0; j < noutputs; j++)
         if (!rtx_equal_p (SET_DEST (p_sets[j]), input)
         && mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,input, SET_DEST (p_sets[j])))
            SET_DEST (p_sets[j]) = mtcs_rtlanal_replace_rtx/*!replace_rtx*/(mtcsRtlanal,SET_DEST (p_sets[j]),input, output);

      for (j = 0; j < ninputs; j++)
         if (mtcs_rtlanal_reg_overlap_mentioned_p/*!reg_overlap_mentioned_p*/(mtcsRtlanal,input, RTVEC_ELT (inputs, j))){
            if (!early_clobber_p || match == matching_constraint_num(ASM_OPERANDS_INPUT_CONSTRAINT (op, j)))
               RTVEC_ELT (inputs, j) = mtcs_rtlanal_replace_rtx/*!replace_rtx*/(mtcsRtlanal,RTVEC_ELT (inputs, j),input, output);
         }

      changed = true;
   }

   if (changed)
      mtcs_dfscan_df_insn_rescan/*!df_insn_rescan*/(mtcsDfscan,insn);
}

/* Add a list of INSNS to the hash HASHP, possibly allocating HASHP
   for the first time.  */
static void record_insns (rtx_insn *insns, rtx end, hash_table<mtcs_insn_cache_hasher> **hashp)
{
   rtx_insn *tmp;
   hash_table<mtcs_insn_cache_hasher> *hash = *hashp;
   if (hash == NULL)
      *hashp = hash = hash_table<mtcs_insn_cache_hasher>::create_ggc (17);
   for (tmp = insns; tmp != end; tmp = NEXT_INSN (tmp)){
      rtx *slot = hash->find_slot (tmp, INSERT);
      gcc_assert (*slot == NULL);
      *slot = tmp;
   }
}

/* Return a sequence to be used as the split prologue for the current
   function, or NULL.  */
static rtx_insn * make_split_prologue_seq (MtcsFunc *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   if (!mtcsOptionsItem->x_flag_split_stack
   || lookup_attribute ("no_split_stack", DECL_ATTRIBUTES (cfun->decl)))
      return NULL;
   n_debug("mtcsfunc.c  make_split_prologue_seq 00 insn:%p\n",get_last_insn());
   mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
   mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,
         target_rtx_gen_split_stack_prologue/*!targetm.gen_split_stack_prologue*/(mtcsMachine->tmrtx));
   rtx_insn *seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
   mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
   n_debug("mtcsfunc.c  make_split_prologue_seq 11 insn:%p\n",get_last_insn());

   record_insns (seq, NULL, &self->prologue_insn_hash);
   n_debug("mtcsfunc.c  make_split_prologue_seq 22 insn:%p\n",get_last_insn());

   set_insn_locations (seq, prologue_location);
   n_debug("mtcsfunc.c  make_split_prologue_seq 33 insn:%p\n",get_last_insn());


   return seq;
}

/* Return a sequence to be used as the prologue for the current function,
   or NULL.  */
static rtx_insn * make_prologue_seq (MtcsFunc *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

   if (!target_rtx_have_prologue /*!targetm.have_prologue*/(mtcsMachine->tmrtx))
      return NULL;
   n_debug("mtcsfunc.c  make_prologue_seq 00 insn:%p\n",get_last_insn());

   mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
   rtx_insn *seq = target_rtx_gen_prologue/*!targetm.gen_prologue*/(mtcsMachine->tmrtx);
   mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,seq);

   /* Insert an explicit USE for the frame pointer
   if the profiling is on and the frame pointer is required.  */
   if (mtcsRtlData/*!crtl*/->profile && mtcsRtlData->x_frame_pointer_needed/*!frame_pointer_needed*/)
      mtcs_emit_emit_use/*!emit_use*/(mtcsEmit,mtcs_rtl_get_hard_frame_pointer_rtx/*!hard_frame_pointer_rtx*/(mtcsRTL));
   n_debug("mtcsfunc.c  make_prologue_seq 11 insn:%p\n",get_last_insn());

   /* Retain a map of the prologue insns.  */
   record_insns (seq, NULL, &self->prologue_insn_hash);
   mtcs_emit_emit_note/*!emit_note*/(mtcsEmit,NOTE_INSN_PROLOGUE_END);
   n_debug("mtcsfunc.c  make_prologue_seq 22 insn:%p\n",get_last_insn());

   /* Ensure that instructions are not moved into the prologue when
   profiling is on.  The call to the profiling routine can be
   emitted within the live range of a call-clobbered register.  */
   if (!mtcsTarget/*!targetm.profile_before_prologue*/->profile_before_prologue(mtcsTarget) && mtcsRtlData/*!crtl*/->profile)
      mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,gen_blockage ());
   n_debug("mtcsfunc.c  make_prologue_seq 33 insn:%p\n",get_last_insn());

   seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
   mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
   set_insn_locations (seq, prologue_location);
   n_debug("mtcsfunc.c  make_prologue_seq 44 insn:%p\n",get_last_insn());

   return seq;
}

/* Emit a sequence of insns to zero the call-used registers before RET
   according to ZERO_REGS_TYPE.  */
static void gen_call_used_regs_seq (MtcsFunc *self,rtx_insn *ret, unsigned int zero_regs_type)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsDfproblems *mtcsDfproblems=mtcs_target_get_dfproblems(mtcsTarget);
   MtcsDfscan *mtcsDfscan = mtcs_target_get_dfscan(mtcsTarget);
   MtcsOutput *mtcsOutput=mtcs_target_get_output(mtcsTarget);
   MtcsOptabs *mtcsOptabs=mtcs_target_get_optabs(mtcsTarget);
   MtcsMachine *mtcsMachine=mtcs_target_get_machine(mtcsTarget);

   bool only_gpr = true;
   bool only_used = true;
   bool only_arg = true;

   /* No need to zero call-used-regs in main ().  */
   if (MAIN_NAME_P (DECL_NAME (current_function_decl)))
      return;

   /* No need to zero call-used-regs if __builtin_eh_return is called
   since it isn't a normal function return.  */
   if (mtcsRtlData/*!crtl*/->calls_eh_return)
      return;

   /* If only_gpr is true, only zero call-used registers that are
   general-purpose registers; if only_used is true, only zero
   call-used registers that are used in the current function;
   if only_arg is true, only zero call-used registers that pass
   parameters defined by the flatform's calling conversion.  */

   using namespace zero_regs_flags;

   only_gpr = zero_regs_type & ONLY_GPR;
   only_used = zero_regs_type & ONLY_USED;
   only_arg = zero_regs_type & ONLY_ARG;

   if ((zero_regs_type & LEAFY_MODE) && mtcs_output_leaf_function_p/*!leaf_function_p*/(mtcsOutput))
      only_used = true;

   /* For each of the hard registers, we should zero it if:
   1. it is a call-used register;
   and 2. it is not a fixed register;
   and 3. it is not live at the return of the routine;
   and 4. it is general registor if only_gpr is true;
   and 5. it is used in the routine if only_used is true;
   and 6. it is a register that passes parameter if only_arg is true.  */

   /* First, prepare the data flow information.  */
   basic_block bb = BLOCK_FOR_INSN (ret);
   auto_bitmap live_out;
   bitmap_copy (live_out, df_get_live_out (bb));
   mtcs_dfproblems_df_simulate_initialize_backwards/*!df_simulate_initialize_backwards*/(mtcsDfproblems,bb, live_out);
   mtcs_dfproblems_df_simulate_one_insn_backwards/*!df_simulate_one_insn_backwards*/(mtcsDfproblems,bb, ret, live_out);

   HardRegSet selected_hardregs={mtcs_reg_get_hard_reg_element_count(mtcsReg)};
   HardRegSet all_call_used_regs={mtcs_reg_get_hard_reg_element_count(mtcsReg)};
   mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&selected_hardregs);
   mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&all_call_used_regs);
   int firstPseudoRegister = mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(mtcsReg);
   nuint generalRegs=mtcs_reg_get_general_regs(mtcsReg);/*!GENERAL_REGS*/

   for (unsigned int regno = 0; regno <firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; regno++){
      if (!mtcsRtlData/*!crtl*/->abi->clobbers_full_reg_p (regno))
         continue;
      if (mtcsReg->hardRegs.x_fixed_regs/*!fixed_regs*/[regno])
         continue;
      if (REGNO_REG_SET_P (live_out, regno))
         continue;
      #ifdef LEAF_REG_REMAP //host=0 nvptx=0
      if (mtcsRtlData/*!crtl*/->uses_only_leaf_regs && LEAF_REG_REMAP (regno) < 0)
         continue;
      #endif
      /* This is a call used register that is dead at return.  */
      mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&all_call_used_regs, regno);

      if (only_gpr
      && !mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(
      &mtcsReg->hardRegs.x_reg_class_contents/*!reg_class_contents*/[generalRegs/*!GENERAL_REGS*/], regno))
         continue;
      if (only_used && !mtcs_dfscan_df_regs_ever_live_p/*!df_regs_ever_live_p*/(mtcsDfscan,regno))
         continue;
      if (only_arg && !mtcs_func_is_function_arg_regno/*!FUNCTION_ARG_REGNO_P*/(self,regno))
         continue;

      /* Now this is a register that we might want to zero.  */
      mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(mtcsReg,&selected_hardregs, regno);
   }

   if (mtcs_reg_hard_reg_set_empty_p/*!hard_reg_set_empty_p*/(&selected_hardregs))
      return;

   /* Now that we have a hard register set that needs to be zeroed, pass it to
   target to generate zeroing sequence.  */
   HardRegSet zeroed_hardregs={mtcs_reg_get_hard_reg_element_count(mtcsReg)};
   mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);

   zeroed_hardregs = target_calls_zero_call_used_regs/*!targetm.calls.zero_call_used_regs*/(mtcsMachine->calls,&selected_hardregs);

   /* For most targets, the returned set of registers is a subset of
   selected_hardregs, however, for some of the targets (for example MIPS),
   clearing some registers that are in selected_hardregs requires clearing
   other call used registers that are not in the selected_hardregs, under
   such situation, the returned set of registers must be a subset of
   all call used registers.  */
   gcc_assert (mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&zeroed_hardregs, &all_call_used_regs));

   rtx_insn *seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
   mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
   if (seq){
      /* Emit the memory blockage and register clobber asm volatile before
      the whole sequence.  */
      mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
      mtcs_optabs_expand_asm_reg_clobber_mem_blockage/*!expand_asm_reg_clobber_mem_blockage*/(mtcsOptabs,&zeroed_hardregs);
      rtx_insn *seq_barrier = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
      mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);

      mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,seq_barrier, ret);
      mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,seq, ret);

      /* Update the data flow information.  */
      mtcsRtlData/*!crtl*/->must_be_zero_on_return |= zeroed_hardregs;
      mtcs_dfscan_df_update_exit_block_uses/*!df_update_exit_block_uses*/(mtcsDfscan);
   }
}

/* Return a sequence to be used as the epilogue for the current function,
   or NULL.  */
static rtx_insn *make_epilogue_seq (MtcsFunc *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsRTL   *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);

   if (!target_rtx_have_epilogue/*!targetm.have_epilogue*/(mtcsMachine->tmrtx))
      return NULL;
   n_debug("mtcsfunc.c  make_epilogue_seq 00 insn:%p\n",get_last_insn());

   mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
   mtcs_emit_emit_note/*!emit_note*/(mtcsEmit,NOTE_INSN_EPILOGUE_BEG);
   n_debug("mtcsfunc.c  make_epilogue_seq 00xx insn:%p\n",get_last_insn());

   rtx_insn *seq = target_rtx_gen_epilogue/*!targetm.gen_epilogue*/(mtcsMachine->tmrtx);
   n_debug("mtcsfunc.c  make_epilogue_seq 00ww insn:%p\n",get_last_insn());

   if (seq)
      mtcs_emit_emit_jump_insn/*!emit_jump_insn*/(mtcsEmit,seq);
   n_debug("mtcsfunc.c  make_epilogue_seq 11 insn:%p\n",get_last_insn());

   /* Retain a map of the epilogue insns.  */
   record_insns (seq, NULL, &self->epilogue_insn_hash);
   set_insn_locations (seq, epilogue_location);
   n_debug("mtcsfunc.c  make_epilogue_seq 22 insn:%p\n",get_last_insn());

   seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
   rtx_insn *returnjump = mtcs_rtl_data_get_last_insn/*!get_last_insn*/(mtcsRtlData);
   mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
   n_debug("mtcsfunc.c  make_epilogue_seq 33 insn:%p\n",get_last_insn());

   if (JUMP_P (returnjump))
      set_return_jump_label (returnjump);
   n_debug("mtcsfunc.c  make_epilogue_seq 44 insn:%p\n",get_last_insn());

   return seq;
}

/* Generate the prologue and epilogue RTL if the machine supports it.  Thread
   this into place with notes indicating where the prologue ends and where
   the epilogue begins.  Update the basic block information when possible.

   Notes on epilogue placement:
   There are several kinds of edges to the exit block:
   * a single fallthru edge from LAST_BB
   * possibly, edges from blocks containing sibcalls
   * possibly, fake edges from infinite loops

   The epilogue is always emitted on the fallthru edge from the last basic
   block in the function, LAST_BB, into the exit block.

   If LAST_BB is empty except for a label, it is the target of every
   other basic block in the function that ends in a return.  If a
   target has a return or simple_return pattern (possibly with
   conditional variants), these basic blocks can be changed so that a
   return insn is emitted into them, and their target is adjusted to
   the real exit block.

   Notes on shrink wrapping: We implement a fairly conservative
   version of shrink-wrapping rather than the textbook one.  We only
   generate a single prologue and a single epilogue.  This is
   sufficient to catch a number of interesting cases involving early
   exits.

   First, we identify the blocks that require the prologue to occur before
   them.  These are the ones that modify a call-saved register, or reference
   any of the stack or frame pointer registers.  To simplify things, we then
   mark everything reachable from these blocks as also requiring a prologue.
   This takes care of loops automatically, and avoids the need to examine
   whether MEMs reference the frame, since it is sufficient to check for
   occurrences of the stack or frame pointer.

   We then compute the set of blocks for which the need for a prologue
   is anticipatable (borrowing terminology from the shrink-wrapping
   description in Muchnick's book).  These are the blocks which either
   require a prologue themselves, or those that have only successors
   where the prologue is anticipatable.  The prologue needs to be
   inserted on all edges from BB1->BB2 where BB2 is in ANTIC and BB1
   is not.  For the moment, we ensure that only one such edge exists.

   The epilogue is placed as described above, but we make a
   distinction between inserting return and simple_return patterns
   when modifying other blocks that end in a return.  Blocks that end
   in a sibcall omit the sibcall_epilogue if the block is not in
   ANTIC.  */
//原型 thread_prologue_and_epilogue_insns function.h function.cc
void mtcs_func_thread_prologue_and_epilogue_insns (MtcsFunc *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);
   MtcsEmit   *mtcsEmit=mtcs_target_get_emit(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsCfgBuild *mtcsCfgBuild = mtcs_target_get_cfg_build(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsMachine  *mtcsMachine = mtcs_target_get_machine(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   n_debug("mtcsfunc.c  mtcs_func_thread_prologue_and_epilogue_insns 00 insn:%p\n",get_last_insn());
   mtcs_dfcore_df_analyze/*!df_analyze*/(mtcsDfcore);
   n_debug("mtcsfunc.c  mtcs_func_thread_prologue_and_epilogue_insns 11xx insn:%p\n",get_last_insn());

   /* Can't deal with multiple successors of the entry block at the
   moment.  Function should always have at least one entry
   point.  */
   gcc_assert (single_succ_p (ENTRY_BLOCK_PTR_FOR_FN (cfun)));
   n_debug("mtcsfunc.c  mtcs_func_thread_prologue_and_epilogue_insns 11xxww insn:%p\n",get_last_insn());

   edge entry_edge = single_succ_edge (ENTRY_BLOCK_PTR_FOR_FN (cfun));
   n_debug("mtcsfunc.c  mtcs_func_thread_prologue_and_epilogue_insns 11xxee insn:%p\n",get_last_insn());

   edge orig_entry_edge = entry_edge;

   rtx_insn *split_prologue_seq = make_split_prologue_seq(self);
   n_debug("mtcsfunc.c  mtcs_func_thread_prologue_and_epilogue_insns 11xxvv insn:%p\n",get_last_insn());

   rtx_insn *prologue_seq = make_prologue_seq(self);
   n_debug("mtcsfunc.c  mtcs_func_thread_prologue_and_epilogue_insns 11xxzz insn:%p\n",get_last_insn());

   rtx_insn *epilogue_seq = make_epilogue_seq(self);
   n_debug("mtcsfunc.c  mtcs_func_thread_prologue_and_epilogue_insns 11 insn:%p\n",get_last_insn());

   /* Try to perform a kind of shrink-wrapping, making sure the
   prologue/epilogue is emitted only around those parts of the
   function that require it.  */
   try_shrink_wrapping (&entry_edge, prologue_seq);

   /* If the target can handle splitting the prologue/epilogue into separate
   components, try to shrink-wrap these components separately.  */
   try_shrink_wrapping_separate (entry_edge->dest);

   /* If that did anything for any component we now need the generate the
   "main" prologue again.  Because some targets require some of these
   to be called in a specific order (i386 requires the split prologue
   to be first, for example), we create all three sequences again here.
   If this does not work for some target, that target should not enable
   separate shrink-wrapping.  */
   if (mtcsRtlData/*!crtl*/->shrink_wrapped_separate){
      split_prologue_seq = make_split_prologue_seq(self);
      prologue_seq = make_prologue_seq(self);
      epilogue_seq = make_epilogue_seq(self);
   }
   n_debug("mtcsfunc.c  mtcs_func_thread_prologue_and_epilogue_insns 22 insn:%p\n",get_last_insn());

   mtcs_func_rtl_profile_for_bb/*!rtl_profile_for_bb*/(self,EXIT_BLOCK_PTR_FOR_FN (cfun));

   /* A small fib -- epilogue is not yet completed, but we wish to re-use
   this marker for the splits of EH_RETURN patterns, and nothing else
   uses the flag in the meantime.  */
   epilogue_completed = 1;

   /* Find non-fallthru edges that end with EH_RETURN instructions.  On
   some targets, these get split to a special version of the epilogue
   code.  In order to be able to properly annotate these with unwind
   info, try to split them now.  If we get a valid split, drop an
   EPILOGUE_BEG note and mark the insns as epilogue insns.  */
   edge e;
   edge_iterator ei;
   FOR_EACH_EDGE (e, ei, EXIT_BLOCK_PTR_FOR_FN (cfun)->preds){
      rtx_insn *prev, *last, *trial;

      if (e->flags & EDGE_FALLTHRU)
         continue;
      last = BB_END (e->src);
      if (!eh_returnjump_p (last))
         continue;

      prev = PREV_INSN (last);
      trial = mtcs_emit_try_split/*!try_split*/(mtcsEmit,PATTERN (last), last, 1);
      if (trial == last)
         continue;

      record_insns (NEXT_INSN (prev), NEXT_INSN (trial), &self->epilogue_insn_hash);
      mtcs_emit_emit_note_after/*!emit_note_after*/(mtcsEmit,NOTE_INSN_EPILOGUE_BEG, prev);
   }
   n_debug("mtcsfunc.c  mtcs_func_thread_prologue_and_epilogue_insns 33 insn:%p\n",get_last_insn());

   edge exit_fallthru_edge = find_fallthru_edge (EXIT_BLOCK_PTR_FOR_FN (cfun)->preds);
   n_debug("mtcsfunc.c  mtcs_func_thread_prologue_and_epilogue_insns 33aa %p\n",exit_fallthru_edge);

   if (exit_fallthru_edge){
      if (epilogue_seq){
         n_debug("mtcsfunc.c  mtcs_func_thread_prologue_and_epilogue_insns 33bb %p\n",exit_fallthru_edge);

         mtcs_cfg_rtl_insert_insn_on_edge/*!insert_insn_on_edge*/(mtcsCfgRtl,epilogue_seq, exit_fallthru_edge);
         n_debug("mtcsfunc.c  mtcs_func_thread_prologue_and_epilogue_insns 33cc %p\n",exit_fallthru_edge);

         mtcs_cfg_rtl_commit_edge_insertions/*!commit_edge_insertions*/(mtcsCfgRtl);
         n_debug("mtcsfunc.c  mtcs_func_thread_prologue_and_epilogue_insns 33dd %p\n",exit_fallthru_edge);

         /* The epilogue insns we inserted may cause the exit edge to no longer
         be fallthru.  */
         FOR_EACH_EDGE (e, ei, EXIT_BLOCK_PTR_FOR_FN (cfun)->preds){
            if (((e->flags & EDGE_FALLTHRU) != 0)  && returnjump_p (BB_END (e->src)))
               e->flags &= ~EDGE_FALLTHRU;
         }

         mtcs_cfg_build_find_sub_basic_blocks/*!find_sub_basic_blocks*/(mtcsCfgBuild,BLOCK_FOR_INSN (epilogue_seq));
      }else if (next_active_insn (BB_END (exit_fallthru_edge->src))){
         /* We have a fall-through edge to the exit block, the source is not
         at the end of the function, and there will be an assembler epilogue
         at the end of the function.
         We can't use force_nonfallthru here, because that would try to
         use return.  Inserting a jump 'by hand' is extremely messy, so
         we take advantage of cfg_layout_finalize using
         fixup_fallthru_exit_predecessor.  */
         mtcs_cfg_rtl_cfg_layout_initialize/*!cfg_layout_initialize*/(mtcsCfgRtl,0);
         basic_block cur_bb;
         FOR_EACH_BB_FN (cur_bb, cfun)
            if (cur_bb->index >= NUM_FIXED_BLOCKS && cur_bb->next_bb->index >= NUM_FIXED_BLOCKS)
               cur_bb->aux = cur_bb->next_bb;
         mtcs_cfg_rtl_cfg_layout_finalize/*!cfg_layout_finalize*/(mtcsCfgRtl);
      }
   }

   n_debug("mtcsfunc.c  mtcs_func_thread_prologue_and_epilogue_insns 44 insn:%p\n",get_last_insn());

   /* Insert the prologue.  */
   mtcs_func_rtl_profile_for_bb/*!rtl_profile_for_bb*/(self,ENTRY_BLOCK_PTR_FOR_FN (cfun));

   if (split_prologue_seq || prologue_seq){
      rtx_insn *split_prologue_insn = split_prologue_seq;
      if (split_prologue_seq){
         while (split_prologue_insn && !NONDEBUG_INSN_P (split_prologue_insn))
            split_prologue_insn = NEXT_INSN (split_prologue_insn);
         mtcs_cfg_rtl_insert_insn_on_edge/*!insert_insn_on_edge*/(mtcsCfgRtl,split_prologue_seq, orig_entry_edge);
      }

      rtx_insn *prologue_insn = prologue_seq;
      if (prologue_seq){
         while (prologue_insn && !NONDEBUG_INSN_P (prologue_insn))
            prologue_insn = NEXT_INSN (prologue_insn);
         mtcs_cfg_rtl_insert_insn_on_edge/*!insert_insn_on_edge*/(mtcsCfgRtl,prologue_seq, entry_edge);
      }

      mtcs_cfg_rtl_commit_edge_insertions/*!commit_edge_insertions*/(mtcsCfgRtl);

      /* Look for basic blocks within the prologue insns.  */
      if (split_prologue_insn  && BLOCK_FOR_INSN (split_prologue_insn) == NULL)
         split_prologue_insn = NULL;
      if (prologue_insn  && BLOCK_FOR_INSN (prologue_insn) == NULL)
         prologue_insn = NULL;
      if (split_prologue_insn || prologue_insn){
         auto_sbitmap blocks (last_basic_block_for_fn (cfun));
         bitmap_clear (blocks);
         if (split_prologue_insn)
            bitmap_set_bit (blocks, BLOCK_FOR_INSN (split_prologue_insn)->index);
         if (prologue_insn)
            bitmap_set_bit (blocks, BLOCK_FOR_INSN (prologue_insn)->index);
         mtcs_cfg_build_find_many_sub_basic_blocks/*!find_many_sub_basic_blocks*/(mtcsCfgBuild,blocks);
      }
   }

   n_debug("mtcsfunc.c  mtcs_func_thread_prologue_and_epilogue_insns 55 insn:%p\n",get_last_insn());

   mtcs_func_default_rtl_profile/*!default_rtl_profile*/(self);

   /* Emit sibling epilogues before any sibling call sites.  */
   for (ei = ei_start (EXIT_BLOCK_PTR_FOR_FN (cfun)->preds); (e = ei_safe_edge (ei)); ei_next (&ei)){
      /* Skip those already handled, the ones that run without prologue.  */
      if (e->flags & EDGE_IGNORE){
         e->flags &= ~EDGE_IGNORE;
         continue;
      }
      rtx_insn *insn = BB_END (e->src);
      if (!(CALL_P (insn) && SIBLING_CALL_P (insn)))
         continue;

      rtx_insn *ep_seq;
      if (mtcsTarget/*!targetm.emit_epilogue_for_sibcall*/->emit_epilogue_for_sibcall){
         mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
         mtcsTarget/*!targetm.emit_epilogue_for_sibcall*/->emit_epilogue_for_sibcall(mtcsTarget,as_a<rtx_call_insn *> (insn));
         ep_seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
         mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);
      }else
         ep_seq = target_rtx_gen_sibcall_epilogue/*!targetm.gen_sibcall_epilogue*/(mtcsMachine->tmrtx);

      if (ep_seq){
         mtcs_emit_start_sequence/*!start_sequence*/(mtcsEmit);
         mtcs_emit_emit_note/*!emit_note*/(mtcsEmit,NOTE_INSN_EPILOGUE_BEG);
         mtcs_emit_emit_insn/*!emit_insn*/(mtcsEmit,ep_seq);
         rtx_insn *seq = mtcs_rtl_data_get_insns/*!get_insns*/(mtcsRtlData);
         mtcs_emit_end_sequence/*!end_sequence*/(mtcsEmit);

         /* Retain a map of the epilogue insns.  Used in life analysis to
         avoid getting rid of sibcall epilogue insns.  Do this before we
         actually emit the sequence.  */
         record_insns (seq, NULL, &self->epilogue_insn_hash);
         set_insn_locations (seq, epilogue_location);

         mtcs_emit_emit_insn_before/*!emit_insn_before*/(mtcsEmit,seq, insn);

         mtcs_cfg_build_find_sub_basic_blocks/*!find_sub_basic_blocks*/(mtcsCfgBuild,BLOCK_FOR_INSN (insn));
      }
   }

   if (epilogue_seq){
      rtx_insn *insn, *next;

      /* Similarly, move any line notes that appear after the epilogue.
      There is no need, however, to be quite so anal about the existence
      of such a note.  Also possibly move
      NOTE_INSN_FUNCTION_BEG notes, as those can be relevant for debug
      info generation.  */
      for (insn = epilogue_seq; insn; insn = next){
         next = NEXT_INSN (insn);
         if (NOTE_P (insn) && (NOTE_KIND (insn) == NOTE_INSN_FUNCTION_BEG))
            mtcs_rtl_reorder_insns/*!reorder_insns*/(mtcsRTL,insn, insn, PREV_INSN (epilogue_seq));
      }
   }
   n_debug("mtcsfunc.c  mtcs_func_thread_prologue_and_epilogue_insns 66\n");

   /* Threading the prologue and epilogue changes the artificial refs in the
   entry and exit blocks, and may invalidate DF info for tail calls.  */
   if (mtcsOptionsItem->x_optimize
   || mtcsOptionsItem->x_flag_optimize_sibling_calls
   || mtcsOptionsItem->x_flag_ipa_icf_functions
   || mtcsOptionsItem->x_in_lto_p)
      mtcs_dfscan_df_update_entry_exit_and_calls/*!df_update_entry_exit_and_calls*/(mtcsDfscan);
   else{
      mtcs_dfscan_df_update_entry_block_defs/*!df_update_entry_block_defs*/(mtcsDfscan);
      mtcs_dfscan_df_update_exit_block_uses/*!df_update_exit_block_uses*/(mtcsDfscan);
   }
}

/* Add the value of the tree INC to the `struct args_size' TO.  */
//原型 ADD_PARM_SIZE function.h 原型传的是变量不是指针
void mtcs_func_add_parm_size(MtcsFunc *self,struct args_size *to,tree var)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);

   tree inc = var;
   if (tree_fits_shwi_p (inc))
      to->constant += tree_to_shwi (inc);
   else if (to->var == 0)
      to->var = mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,ssizetype, inc);
   else
      to->var = mtcs_const_size_binop/*!size_binop*/(mtcsConst,PLUS_EXPR, to->var,
            mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,ssizetype, inc));
}

//原型 SUB_PARM_SIZE function.h 原型传的是变量不是指针
void mtcs_func_sub_parm_size(MtcsFunc *self,struct args_size *to,tree var)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);

   tree dec = var;
   if (tree_fits_shwi_p (dec))
      to->constant -= tree_to_shwi (dec);
   else if (to->var == 0)
      to->var = mtcs_const_size_binop/*!size_binop*/(mtcsConst,MINUS_EXPR, ssize_int (0),
            mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,ssizetype, dec));
   else
      to->var = mtcs_const_size_binop/*!size_binop*/(mtcsConst,MINUS_EXPR, to->var,
            mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,ssizetype, dec));
}

/* Convert the implicit sum in a `struct args_size' into a tree
   of type ssizetype.  */
//#define ARGS_SIZE_TREE(SIZE)              \
//((SIZE).var == 0 ? ssize_int ((SIZE).constant)        \
// : size_binop (PLUS_EXPR, fold_convert (ssizetype, (SIZE).var),   \
//          ssize_int ((SIZE).constant)))
//原型 #define ARGS_SIZE_TREE(SIZE) function.h
tree mtcs_func_args_size_tree(MtcsFunc *self,struct args_size size)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConst *mtcsConst=mtcs_target_get_const(mtcsTarget);
   MtcsTree *mtcsTree=mtcs_target_get_tree(mtcsTarget);

   if(size.var==0)
      return mtcs_tree_ssize_int/*!ssize_int*/(mtcsTree,size.constant);
   else
      return mtcs_const_size_binop/*!size_binop*/(mtcsConst,PLUS_EXPR,
            mtcs_const_fold_convert/*!fold_convert*/(mtcsConst,ssizetype, size.var),
            mtcs_tree_ssize_int/*!ssize_int*/(mtcsTree,size.constant));
}

/* Convert the implicit sum in a `struct args_size' into an rtx.  */
//#define ARGS_SIZE_RTX(SIZE)               \
//((SIZE).var == 0 ? gen_int_mode ((SIZE).constant, Pmode) \
// : expand_normal (ARGS_SIZE_TREE (SIZE)))
//原型 #define ARGS_SIZE_RTX(SIZE) function.h
rtx mtcs_func_args_size_rtx(MtcsFunc *self,struct args_size size)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsExpr *mtcsExpr=mtcs_target_get_expr(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);

   if(size.var==0)
      return mtcs_rtl_gen_int_mode/*!gen_int_mode*/(mtcsRTL,size.constant,mtcs_mode_get_Pmode(mtcsMode));
   else
      return  mtcs_expr_expand_normal/*!expand_normal*/(mtcsExpr,mtcs_func_args_size_tree/*!ARGS_SIZE_TREE*/(self,size));
}

//原型 rest_of_handle_thread_prologue_and_epilogue function.cc
static void rest_of_handle_thread_prologue_and_epilogue (MtcsFunc *self,function *fun)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRtlData *mtcsRtlData=self->mtcsRtlData;
   MtcsReg *mtcsReg=mtcs_target_get_reg(mtcsTarget);
   MtcsRTL *mtcsRTL=mtcs_target_get_rtl(mtcsTarget);
   MtcsCfgCleanup *mtcsCfgCleanup=mtcs_target_get_cfg_cleanup(mtcsTarget);
   MtcsCfgRtl *mtcsCfgRtl=mtcs_target_get_cfg_rtl(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   /* prepare_shrink_wrap is sensitive to the block structure of the control
   flow graph, so clean it up first.  */
   if (mtcsOptionsItem->x_optimize)
      mtcs_cfg_cleanup_cleanup_cfg/*!cleanup_cfg*/(mtcsCfgCleanup,0);


   /* On some machines, the prologue and epilogue code, or parts thereof,
   can be represented as RTL.  Doing so lets us schedule insns between
   it and the rest of the code and also allows delayed branch
   scheduling to operate in the epilogue.  */
   mtcs_func_thread_prologue_and_epilogue_insns/*!thread_prologue_and_epilogue_insns*/(self);

   /* Some non-cold blocks may now be only reachable from cold blocks.
   Fix that up.  */
   mtcs_cfg_rtl_fixup_partitions/*!fixup_partitions*/(mtcsCfgRtl);

   /* After prologue and epilogue generation, the judgement on whether
   one memory access onto stack frame may trap or not could change,
   since we get more exact stack information by now.  So try to
   remove any EH edges here, see PR90259.  */
   if (fun->can_throw_non_call_exceptions)
      mtcs_cfg_rtl_purge_all_dead_edges/*!purge_all_dead_edges*/(mtcsCfgRtl);

   /* Shrink-wrapping can result in unreachable edges in the epilogue,
   see PR57320.  */
   mtcs_cfg_cleanup_cleanup_cfg/*!cleanup_cfg*/(mtcsCfgCleanup,mtcsOptionsItem->x_optimize ? CLEANUP_EXPENSIVE : 0);

   /* The stack usage info is finalized during prologue expansion.  */
   if (mtcsOptionsItem->x_flag_stack_usage_info || mtcsOptionsItem->x_flag_callgraph_info)
      output_stack_usage ();
}





/*-------------------------------- 以下是基于MtcsFunc的rtl pass -------------------*/
//原型 NEXT_PASS (pass_instantiate_virtual_regs, 1);    RTL_PASS   functions.cc   vregs   y  无条件执行 instantiate_virtual_regs
static nuint instantiate_virtual_regs_execute_cb(MtcsPass *mtcsPass,function *func)
{
   MtcsPassInstantiateVirtualRegs *self=(MtcsPassInstantiateVirtualRegs *)mtcsPass;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   mtcs_func_instantiate_virtual_regs/*!instantiate_virtual_regs*/(mtcsFunc);
   return 0;
}

static void mtcsPassInstantiateVirtualRegsInit(MtcsPassInstantiateVirtualRegs *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->execute =instantiate_virtual_regs_execute_cb;
    mtcs_pass_set_properties((MtcsPass *)self,
            0,/* properties_required */
            0, /* properties_provided */
            0 /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          0 /*todo_flags_finish */);
}

MtcsPassInstantiateVirtualRegs *mtcs_pass_instantiate_virtual_regs_new(MtcsMode *mtcsMode)
{
   MtcsPassInstantiateVirtualRegs *self = n_slice_alloc0 (sizeof(MtcsPassInstantiateVirtualRegs));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"vregs");
   mtcsPassInstantiateVirtualRegsInit(self);
   return self;
}

//原型 NEXT_PASS (pass_match_asm_constraints, 1);  RTL_PASS  functions.cc  asmcons   y  无条件执行 ...match_asm_constraints_1..
static nuint match_asm_constraints_execute_cb(MtcsPass *mtcsPass,function *fun)
{
   MtcsPassMatchAsmConstraints *self=(MtcsPassMatchAsmConstraints *)mtcsPass;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcsFunc->mtcsRtlData;
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);

   basic_block bb;
   rtx_insn *insn;
   rtx pat, *p_sets;
   int noutputs;

   if (!mtcsRtlData/*!crtl*/->has_asm_statement)
      return 0;

   mtcs_dfcore_df_set_flags/*!df_set_flags*/(mtcsDfcore,DF_DEFER_INSN_RESCAN);
   FOR_EACH_BB_FN (bb, fun){
      FOR_BB_INSNS (bb, insn){
         if (!INSN_P (insn))
            continue;

         pat = PATTERN (insn);
         if (GET_CODE (pat) == PARALLEL)
            p_sets = &XVECEXP (pat, 0, 0), noutputs = XVECLEN (pat, 0);
         else if (GET_CODE (pat) == SET)
            p_sets = &PATTERN (insn), noutputs = 1;
         else
            continue;

         if (GET_CODE (*p_sets) == SET  && GET_CODE (SET_SRC (*p_sets)) == ASM_OPERANDS)
            match_asm_constraints_1(mtcsFunc,insn, p_sets, noutputs);
      }
   }

   return TODO_df_finish;
}

static void mtcsPassMatchAsmConstraintsInit(MtcsPassMatchAsmConstraints *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->execute =match_asm_constraints_execute_cb;
    mtcs_pass_set_properties((MtcsPass *)self,
            0,/* properties_required */
            0, /* properties_provided */
            0 /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          0 /*todo_flags_finish */);
}

MtcsPassMatchAsmConstraints *mtcs_pass_match_asm_constraints_new(MtcsMode *mtcsMode)
{
   MtcsPassMatchAsmConstraints *self = n_slice_alloc0 (sizeof(MtcsPassMatchAsmConstraints));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"asmcons");
   mtcsPassMatchAsmConstraintsInit(self);
   return self;
}

//原型 NEXT_PASS (pass_late_thread_prologue_and_epilogue, 1);  RTL_PASS  functions.cc  late_pro_and_epilogue   y 有条件执行 targetm.use_late_prologue_epilogue
static nuint late_thread_prologue_and_epilogue_execute_cb(MtcsPass *mtcsPass,function *fun)
{
   MtcsPassLateThreadPrologueAndEpilogue *self=(MtcsPassLateThreadPrologueAndEpilogue *)mtcsPass;
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsInsnAttr *mtcsInsnAttr=mtcs_target_get_insn_attr(mtcsTarget);

   gcc_assert (!mtcs_insn_attr_get_delay_slots/*!DELAY_SLOTS*/(mtcsInsnAttr));
   rest_of_handle_thread_prologue_and_epilogue (mtcsFunc,fun);
   return 0;

}

static nboolean late_thread_prologue_and_epilogue_gate_cb(MtcsPass *mtcsPass,function *fun)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   return mtcsTarget/*!targetm.use_late_prologue_epilogue*/->use_late_prologue_epilogue(mtcsTarget);
}

static void mtcsPassLateThreadPrologueAndEpilogueInit(MtcsPassLateThreadPrologueAndEpilogue *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->execute =late_thread_prologue_and_epilogue_execute_cb;
    mtcsPass->gate =late_thread_prologue_and_epilogue_gate_cb;
    mtcs_pass_set_properties((MtcsPass *)self,
            0,/* properties_required */
            0, /* properties_provided */
            0 /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          ( TODO_df_verify | TODO_df_finish )/*todo_flags_finish */);
}

MtcsPassLateThreadPrologueAndEpilogue *mtcs_pass_late_thread_prologue_and_epilogue_new (MtcsMode *mtcsMode)
{
   MtcsPassLateThreadPrologueAndEpilogue *self = n_slice_alloc0 (sizeof(MtcsPassLateThreadPrologueAndEpilogue));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"late_pro_and_epilogue");
   mtcsPassLateThreadPrologueAndEpilogueInit(self);
   return self;
}

//原型 NEXT_PASS (pass_zero_call_used_regs, 1);  RTL_PASS  function.cc  zero_call_used_regs   y 无条件执行 tree attr_zero_regs
static nuint zero_call_used_regs_execute_cb(MtcsPass *mtcsPass,function *fun)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(mtcsPass);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsInsnAttr *mtcsInsnAttr=mtcs_target_get_insn_attr(mtcsTarget);
   MtcsDfcore *mtcsDfcore=mtcs_target_get_dfcore(mtcsTarget);
   MtcsOptions *mtcsOptions=mtcs_target_get_options(mtcsTarget);
   MtcsOptionsItem *mtcsOptionsItem=mtcsOptions->global_options;

   using namespace zero_regs_flags;
   unsigned int zero_regs_type = UNSET;

   tree attr_zero_regs = lookup_attribute ("zero_call_used_regs", DECL_ATTRIBUTES (fun->decl));

   /* Get the type of zero_call_used_regs from function attribute.
   We have filtered out invalid attribute values already at this point.  */
   if (attr_zero_regs){
      /* The TREE_VALUE of an attribute is a TREE_LIST whose TREE_VALUE
      is the attribute argument's value.  */
      attr_zero_regs = TREE_VALUE (attr_zero_regs);
      gcc_assert (TREE_CODE (attr_zero_regs) == TREE_LIST);
      attr_zero_regs = TREE_VALUE (attr_zero_regs);
      gcc_assert (TREE_CODE (attr_zero_regs) == STRING_CST);

      for (unsigned int i = 0; zero_call_used_regs_opts[i].name != NULL; ++i)
         if (strcmp (TREE_STRING_POINTER (attr_zero_regs), zero_call_used_regs_opts[i].name) == 0){
            zero_regs_type = zero_call_used_regs_opts[i].flag;
            break;
         }
   }

   if (!zero_regs_type)
      zero_regs_type = mtcsOptionsItem->x_flag_zero_call_used_regs;

   /* No need to zero call-used-regs when no user request is present.  */
   if (!(zero_regs_type & ENABLED))
      return 0;

   edge_iterator ei;
   edge e;

   /* This pass needs data flow information.  */
   mtcs_dfcore_df_analyze/*!df_analyze*/(mtcsDfcore);

   /* Iterate over the function's return instructions and insert any
   register zeroing required by the -fzero-call-used-regs command-line
   option or the "zero_call_used_regs" function attribute.  */
   FOR_EACH_EDGE (e, ei, EXIT_BLOCK_PTR_FOR_FN (cfun)->preds){
      rtx_insn *insn = BB_END (e->src);
      if (JUMP_P (insn) && ANY_RETURN_P (JUMP_LABEL (insn)))
         gen_call_used_regs_seq(mtcsFunc,insn, zero_regs_type);
   }

   return 0;

}

static void mtcsPassZeroCallUsedRegsInit(MtcsPassZeroCallUsedRegs *self)
{
    MtcsPass *mtcsPass=(MtcsPass *)self;
    mtcsPass->execute =zero_call_used_regs_execute_cb;
    mtcs_pass_set_properties((MtcsPass *)self,
            0,/* properties_required */
            0, /* properties_provided */
            0 /* properties_destroyed */);
    mtcs_pass_set_todo_flags((MtcsPass *)self,
          0, /* todo_flags_start */
          0/*todo_flags_finish */);
}

MtcsPassZeroCallUsedRegs *mtcs_pass_zero_call_use_regs_new (MtcsMode *mtcsMode)
{
   MtcsPassZeroCallUsedRegs *self = n_slice_alloc0 (sizeof(MtcsPassZeroCallUsedRegs));
   mtcs_component_set_mode((MtcsComponent*)self,mtcsMode);
   mtcs_pass_init((MtcsPass *)self,RTL_PASS,"zero_call_used_regs");
   mtcsPassZeroCallUsedRegsInit(self);
   return self;
}
