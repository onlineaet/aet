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


#ifndef __GCC_MTCS_DSE__
#define __GCC_MTCS_DSE__

#include "../../nlib.h"
#include "../mtcscomponent.h"
#include "../mtcsmicro.h"
#include "../mtcspass.h"
#include "alloc-pool.h"


typedef struct _MtcsDse MtcsDse;

struct insn_info_type;

/* This structure holds information about a candidate store.  */
class store_info
{
public:

   /* False means this is a clobber.  */
   bool is_set;

   /* False if a single HOST_WIDE_INT bitmap is used for positions_needed.  */
   bool is_large;

   /* The id of the mem group of the base address.  If rtx_varies_p is
   true, this is -1.  Otherwise, it is the index into the group
   table.  */
   int group_id;

   /* This is the cselib value.  */
   cselib_val *cse_base;

   /* This canonized mem.  */
   rtx mem;

   /* Canonized MEM address for use by canon_true_dependence.  */
   rtx mem_addr;

   /* The offset of the first byte associated with the operation.  */
   poly_int64 offset;

   /* The number of bytes covered by the operation.  This is always exact
   and known (rather than -1).  */
   poly_int64 width;

   /* The address space that the memory reference uses.  */
   unsigned char addrspace;

   union
   {
      /* A bitmask as wide as the number of bytes in the word that
      contains a 1 if the byte may be needed.  The store is unused if
      all of the bits are 0.  This is used if IS_LARGE is false.  */
      unsigned HOST_WIDE_INT small_bitmask;

      struct
      {
         /* A bitmap with one bit per byte, or null if the number of
         bytes isn't known at compile time.  A cleared bit means
         the position is needed.  Used if IS_LARGE is true.  */
         bitmap bmap;

         /* When BITMAP is nonnull, this counts the number of set bits
         (i.e. unneeded bytes) in the bitmap.  If it is equal to
         WIDTH, the whole store is unused.

         When BITMAP is null:
         - the store is definitely not needed when COUNT == 1
         - all the store is needed when COUNT == 0 and RHS is nonnull
         - otherwise we don't know which parts of the store are needed.  */
         int count;
      } large;
   } positions_needed;

   /* The next store info for this insn.  */
   class store_info *next;

   /* The right hand side of the store.  This is used if there is a
   subsequent reload of the mems address somewhere later in the
   basic block.  */
   rtx rhs;

   /* If rhs is or holds a constant, this contains that constant,
   otherwise NULL.  */
   rtx const_rhs;

   /* Set if this store stores the same constant value as REDUNDANT_REASON
   insn stored.  These aren't eliminated early, because doing that
   might prevent the earlier larger store to be eliminated.  */
   struct insn_info_type *redundant_reason;
};


/* This structure holds information about a load.  These are only
   built for rtx bases.  */
class read_info_type
{
public:
  /* The id of the mem group of the base address.  */
  int group_id;

  /* The offset of the first byte associated with the operation.  */
  poly_int64 offset;

  /* The number of bytes covered by the operation, or -1 if not known.  */
  poly_int64 width;

  /* The mem being read.  */
  rtx mem;

  /* The next read_info for this insn.  */
  class read_info_type *next;
};


/* One of these records is created for each insn.  */
struct insn_info_type
{
  /* Set true if the insn contains a store but the insn itself cannot
     be deleted.  This is set if the insn is a parallel and there is
     more than one non dead output or if the insn is in some way
     volatile.  */
  bool cannot_delete;
  /* This field is only used by the global algorithm.  It is set true
     if the insn contains any read of mem except for a (1).  This is
     also set if the insn is a call or has a clobber mem.  If the insn
     contains a wild read, the use_rec will be null.  */
  bool wild_read;

  /* This is true only for CALL instructions which could potentially read
     any non-frame memory location. This field is used by the global
     algorithm.  */
  bool non_frame_wild_read;

  /* This field is only used for the processing of const functions.
     These functions cannot read memory, but they can read the stack
     because that is where they may get their parms.  We need to be
     this conservative because, like the store motion pass, we don't
     consider CALL_INSN_FUNCTION_USAGE when processing call insns.
     Moreover, we need to distinguish two cases:
     1. Before reload (register elimination), the stores related to
   outgoing arguments are stack pointer based and thus deemed
   of non-constant base in this pass.  This requires special
   handling but also means that the frame pointer based stores
   need not be killed upon encountering a const function call.
     2. After reload, the stores related to outgoing arguments can be
   either stack pointer or hard frame pointer based.  This means
   that we have no other choice than also killing all the frame
   pointer based stores upon encountering a const function call.
     This field is set after reload for const function calls and before
     reload for const tail function calls on targets where arg pointer
     is the frame pointer.  Having this set is less severe than a wild
     read, it just means that all the frame related stores are killed
     rather than all the stores.  */
  bool frame_read;

  /* This field is only used for the processing of const functions.
     It is set if the insn may contain a stack pointer based store.  */
  bool stack_pointer_based;

  /* This is true if any of the sets within the store contains a
     cselib base.  Such stores can only be deleted by the local
     algorithm.  */
  bool contains_cselib_groups;

  /* The insn. */
  rtx_insn *insn;

  /* The list of mem sets or mem clobbers that are contained in this
     insn.  If the insn is deletable, it contains only one mem set.
     But it could also contain clobbers.  Insns that contain more than
     one mem set are not deletable, but each of those mems are here in
     order to provide info to delete other insns.  */
  store_info *store_rec;

  /* The linked list of mem uses in this insn.  Only the reads from
     rtx bases are listed here.  The reads to cselib bases are
     completely processed during the first scan and so are never
     created.  */
  read_info_type * read_rec;

  /* The live fixed registers.  We assume only fixed registers can
     cause trouble by being clobbered from an expanded pattern;
     storing only the live fixed registers (rather than all registers)
     means less memory needs to be allocated / copied for the individual
     stores.  */
  regset fixed_regs_live;

  /* The prev insn in the basic block.  */
  struct insn_info_type * prev_insn;

  /* The linked list of insns that are in consideration for removal in
     the forwards pass through the basic block.  This pointer may be
     trash as it is not cleared when a wild read occurs.  The only
     time it is guaranteed to be correct is when the traversal starts
     at active_local_stores.  */
  struct insn_info_type * next_local_store;

  MtcsDse *mtcsDse;//回调需要用到
};



struct dse_bb_info_type
{
  /* Pointer to the insn info for the last insn in the block.  These
     are linked so this is how all of the insns are reached.  During
     scanning this is the current insn being scanned.  */
  struct insn_info_type * last_insn;

  /* The info for the global dataflow problem.  */


  /* This is set if the transfer function should and in the wild_read
     bitmap before applying the kill and gen sets.  That vector knocks
     out most of the bits in the bitmap and thus speeds up the
     operations.  */
  bool apply_wild_read;

  /* The following 4 bitvectors hold information about which positions
     of which stores are live or dead.  They are indexed by
     get_bitmap_index.  */

  /* The set of store positions that exist in this block before a wild read.  */
  bitmap gen;

  /* The set of load positions that exist in this block above the
     same position of a store.  */
  bitmap kill;

  /* The set of stores that reach the top of the block without being
     killed by a read.

     Do not represent the in if it is all ones.  Note that this is
     what the bitvector should logically be initialized to for a set
     intersection problem.  However, like the kill set, this is too
     expensive.  So initially, the in set will only be created for the
     exit block and any block that contains a wild read.  */
  bitmap in;

  /* The set of stores that reach the bottom of the block from it's
     successors.

     Do not represent the in if it is all ones.  Note that this is
     what the bitvector should logically be initialized to for a set
     intersection problem.  However, like the kill and in set, this is
     too expensive.  So what is done is that the confluence operator
     just initializes the vector from one of the out sets of the
     successors of the block.  */
  bitmap out;

  /* The following bitvector is indexed by the reg number.  It
     contains the set of regs that are live at the current instruction
     being processed.  While it contains info for all of the
     registers, only the hard registers are actually examined.  It is used
     to assure that shift and/or add sequences that are inserted do not
     accidentally clobber live hard regs.  */
  bitmap regs_live;

  MtcsDse *mtcsDse;
};


/* There is a group_info for each rtx base that is used to reference
   memory.  There are also not many of the rtx bases because they are
   very limited in scope.  */

struct group_info
{
  /* The actual base of the address.  */
  rtx rtx_base;

  /* The sequential id of the base.  This allows us to have a
     canonical ordering of these that is not based on addresses.  */
  int id;

  /* True if there are any positions that are to be processed
     globally.  */
  bool process_globally;

  /* True if the base of this group is either the frame_pointer or
     hard_frame_pointer.  */
  bool frame_related;

  /* A mem wrapped around the base pointer for the group in order to do
     read dependency.  It must be given BLKmode in order to encompass all
     the possible offsets from the base.  */
  rtx base_mem;

  /* Canonized version of base_mem's address.  */
  rtx canon_base_addr;

  /* These two sets of two bitmaps are used to keep track of how many
     stores are actually referencing that position from this base.  We
     only do this for rtx bases as this will be used to assign
     positions in the bitmaps for the global problem.  Bit N is set in
     store1 on the first store for offset N.  Bit N is set in store2
     for the second store to offset N.  This is all we need since we
     only care about offsets that have two or more stores for them.

     The "_n" suffix is for offsets less than 0 and the "_p" suffix is
     for 0 and greater offsets.

     There is one special case here, for stores into the stack frame,
     we will or store1 into store2 before deciding which stores look
     at globally.  This is because stores to the stack frame that have
     no other reads before the end of the function can also be
     deleted.  */
  bitmap store1_n, store1_p, store2_n, store2_p;

  /* These bitmaps keep track of offsets in this group escape this function.
     An offset escapes if it corresponds to a named variable whose
     addressable flag is set.  */
  bitmap escaped_n, escaped_p;

  /* The positions in this bitmap have the same assignments as the in,
     out, gen and kill bitmaps.  This bitmap is all zeros except for
     the positions that are occupied by stores for this group.  */
  bitmap group_kill;

  /* The offset_map is used to map the offsets from this base into
     positions in the global bitmaps.  It is only created after all of
     the all of stores have been scanned and we know which ones we
     care about.  */
  int *offset_map_n, *offset_map_p;
  int offset_map_size_n, offset_map_size_p;
};

/* This structure holds the set of changes that are being deferred
   when removing read operation.  See replace_read.  */
struct deferred_change
{
  /* The mem that is being replaced.  */
  rtx *loc;
  /* The reg it is being replaced with.  */
  rtx reg;
  struct deferred_change *next;
};

struct mtcs_invariant_group_base_hasher;

struct _MtcsDse
{
    MtcsComponent parent;

    /* Obstack for the DSE dataflow bitmaps.  We don't want to put these
       on the default obstack because these bitmaps can grow quite large
       (~2GB for the small (!) test case of PR54146) and we'll hold on to
       all that memory until the end of the compiler run.
       As a bonus, delete_tree_live_info can destroy all the bitmaps by just
       releasing the whole obstack.  */
     bitmap_obstack dse_bitmap_obstack;

    /* Obstack for other data.  As for above: Kinda nice to be able to
       throw it all away at the end in one big sweep.  */
     struct obstack dse_obstack;

    /* Scratch bitmap for cselib's cselib_expand_value_rtx.  */
     bitmap scratch;// = NULL;

     /* Tables of group_info structures, hashed by base value.  */
     hash_table<mtcs_invariant_group_base_hasher> *rtx_group_table;


     object_allocator<store_info> cse_store_info_pool;// ("cse_store_info_pool");

     object_allocator<store_info> rtx_store_info_pool;// ("rtx_store_info_pool");

     object_allocator<read_info_type> read_info_type_pool;// ("read_info_pool");

     object_allocator<insn_info_type> insn_info_type_pool;// ("insn_info_pool");
     /* The linked list of stores that are under consideration in this
        basic block.  */
     struct insn_info_type *active_local_stores;
     int active_local_stores_len;
     object_allocator<dse_bb_info_type> dse_bb_info_type_pool;// ("bb_info_pool");
     /* Table to hold all bb_infos.  */
     struct dse_bb_info_type **bb_table;

     object_allocator<group_info> group_info_pool;// ("rtx_group_info_pool");
     /* Index into the rtx_group_vec.  */
     int rtx_group_next_id;
     vec<group_info *> rtx_group_vec;

      object_allocator<deferred_change> deferred_change_pool;// ("deferred_change_pool");

      struct deferred_change *deferred_change_list;// = NULL;

     /* This is true except if cfun->stdarg -- i.e. we cannot do
        this for vararg functions because they play games with the frame.  */
      bool stores_off_frame_dead_at_return;

     /* Counter for stats.  */
      int globally_deleted;
      int locally_deleted;

      bitmap all_blocks;

     /* Locations that are killed by calls in the global phase.  */
      bitmap kill_on_calls;

     /* The number of bits used in the global bitmaps.  */
      unsigned int current_position;
};


MtcsDse *mtcs_dse_new(MtcsMode *mtcsMode);
//原型 check_for_inc_dec rtl.h dse.cc postreload.cc调用
bool mtcs_dse_check_for_inc_dec (MtcsDse *self,rtx_insn *insn);

//原型 NEXT_PASS (pass_rtl_dse1, 1); RTL_PASS dse.cc dse1 y 有条件执行 optimize > 0 && flag_dse && dbg_cnt (dse1);  rest_of_handle_dse;
typedef struct _MtcsPassDse1 MtcsPassDse1;
struct _MtcsPassDse1
{
   MtcsPass parent;
};
MtcsPassDse1 *mtcs_pass_dse1_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_data_rtl_dse2, 1); RTL_PASS dse.cc dse2 y 有条件执行 optimize > 0 && flag_dse && dbg_cnt (dse2);  rest_of_handle_dse;
typedef struct _MtcsPassDse2 MtcsPassDse2;
struct _MtcsPassDse2
{
   MtcsPass parent;
};
MtcsPassDse2 *mtcs_pass_dse2_new(MtcsMode *mtcsMode);


#endif
