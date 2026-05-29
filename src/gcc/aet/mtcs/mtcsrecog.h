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

#ifndef __GCC_MTCS_RECOG__
#define __GCC_MTCS_RECOG__

#include "../nlib.h"
#include "mtcscomponent.h"
#include "mtcspass.h"
#include "mtcsmicro.h"
#include "tree.h"

/* Random number that should be large enough for all purposes.  Also define
   a type that has at least MAX_RECOG_ALTERNATIVES + 1 bits, with the extra
   bit giving an invalid value that can be used to mean "uninitialized".  */
#define MAX_RECOG_ALTERNATIVES 35
//原型 alternative_mask recog.h
typedef uint64_t mtcs_alternative_mask;  /* Keep in sync with genattrtab.cc.  */

/* A mask of all alternatives.  */
//原型 ALL_ALTERNATIVES recog.h
#define MTCS_ALL_ALTERNATIVES ((mtcs_alternative_mask) -1)

/* A mask containing just alternative X.  */
//原型 ALTERNATIVE_BIT recog.h
#define MTCS_ALTERNATIVE_BIT(X) ((mtcs_alternative_mask) 1 << (X))

/* Types of operands.  */
//原型 op_type recog.h
enum mtcs_op_type {
  MTCS_OP_IN,
  MTCS_OP_OUT,
  MTCS_OP_INOUT
};

//原型 operand_alternative recog.h
struct mtcs_operand_alternative
{
  /* Pointer to the beginning of the constraint string for this alternative,
     for easier access by alternative number.  */
  const char *constraint;

  /* The register class valid for this alternative (possibly NO_REGS).  */
  ENUM_BITFIELD (reg_class) cl : 16;

  /* "Badness" of this alternative, computed from number of '?' and '!'
     characters in the constraint string.  */
  unsigned int reject : 16;

  /* -1 if no matching constraint was found, or an operand number.  */
  int matches : 8;
  /* The same information, but reversed: -1 if this operand is not
     matched by any other, or the operand number of the operand that
     matches this one.  */
  int matched : 8;

  /* Bit ID is set if the constraint string includes a register constraint with
     register filter ID.  Use test_register_filters (REGISTER_FILTERS, REGNO)
     to test whether REGNO is a valid start register for the operand.  */
  unsigned int register_filters : MAX (1/*NUM_REGISTER_FILTERS*/, 1);//NUM_REGISTER_FILTERS host nvptx都是0

  /* Nonzero if '&' was found in the constraint string.  */
  unsigned int earlyclobber : 1;
  /* Nonzero if TARGET_MEM_CONSTRAINT was found in the constraint
     string.  */
  unsigned int memory_ok : 1;
  /* Nonzero if 'p' was found in the constraint string.  */
  unsigned int is_address : 1;
  /* Nonzero if 'X' was found in the constraint string, or if the constraint
     string for this alternative was empty.  */
  unsigned int anything_ok : 1;
};

/* Set by constrain_operands to the number of the alternative that
   matched.  */

/* The following vectors hold the results from insn_extract.  */
//MAX_RECOG_OPERANDS host=nvptx=30;
//MAX_DUP_OPERANDS host=14 nvptx=2
//原型 recog_data_d recog.h
struct mtcs_recog_data_d
{
  /* It is very tempting to make the 5 operand related arrays into a
     structure and index on that.  However, to be source compatible
     with all of the existing md file insn constraints and output
     templates, we need `operand' as a flat array.  Without that
     member, making an array for the rest seems pointless.  */

  /* Gives value of operand N.  */
  rtx operand[MAX_MAX_RECOG_OPERANDS/*!MAX_RECOG_OPERANDS*/];

  /* Gives location where operand N was found.  */
  rtx *operand_loc[MAX_MAX_RECOG_OPERANDS/*!MAX_RECOG_OPERANDS*/];

  /* Gives the constraint string for operand N.  */
  const char *constraints[MAX_MAX_RECOG_OPERANDS/*!MAX_RECOG_OPERANDS*/];

  /* Nonzero if operand N is a match_operator or a match_parallel.  */
  char is_operator[MAX_MAX_RECOG_OPERANDS/*!MAX_RECOG_OPERANDS*/];

  /* Gives the mode of operand N.  */
  machine_mode operand_mode[MAX_MAX_RECOG_OPERANDS/*!MAX_RECOG_OPERANDS*/];

  /* Gives the type (in, out, inout) for operand N.  */
  enum mtcs_op_type operand_type[MAX_MAX_RECOG_OPERANDS/*!MAX_RECOG_OPERANDS*/];

  /* Gives location where the Nth duplicate-appearance of an operand
     was found.  This is something that matched MATCH_DUP.  */
  rtx *dup_loc[MAX_MAX_DUP_OPERANDS/*!MAX_DUP_OPERANDS*/];

  /* Gives the operand number that was duplicated in the Nth
     duplicate-appearance of an operand.  */
  char dup_num[MAX_MAX_DUP_OPERANDS/*!MAX_DUP_OPERANDS*/];

  /* ??? Note that these are `char' instead of `unsigned char' to (try to)
     avoid certain lossage from K&R C, wherein `unsigned char' default
     promotes to `unsigned int' instead of `int' as in ISO C.  As of 1999,
     the most common places to bootstrap from K&R C are SunOS and HPUX,
     both of which have signed characters by default.  The only other
     supported natives that have both K&R C and unsigned characters are
     ROMP and Irix 3, and neither have been seen for a while, but do
     continue to consider unsignedness when performing arithmetic inside
     a comparison.  */

  /* The number of operands of the insn.  */
  char n_operands;

  /* The number of MATCH_DUPs in the insn.  */
  char n_dups;

  /* The number of alternatives in the constraints for the insn.  */
  char n_alternatives;

  /* True if insn is ASM_OPERANDS.  */
  bool is_asm;

  /* In case we are caching, hold insn data was generated for.  */
  rtx_insn *insn;
};


/* RAII class for saving/restoring recog_data.  */
//原型 recog_data_saver recog.h
//class mtcs_recog_data_saver
//{
//  mtcs_recog_data_d m_saved_data;
//public:
//  mtcs_recog_data_saver () : m_saved_data (recog_data) {}
//  ~mtcs_recog_data_saver () { recog_data = m_saved_data; }
//};

/* An enum of boolean attributes that may only depend on the current
   subtarget, not on things like operands or compiler phase.  */
//原型 bool_attr recog.h
enum mtcs_bool_attr {
  MTCS_BA_ENABLED,
  MTCS_BA_PREFERRED_FOR_SPEED,
  MTCS_BA_PREFERRED_FOR_SIZE,
  MTCS_BA_LAST = MTCS_BA_PREFERRED_FOR_SIZE
};

typedef struct _MtcsRecog MtcsRecog;
struct _MtcsRecog
{
   MtcsComponent parent;
   //原型 LEGITIMATE_PIC_OPERAND_P default.h 每个平台都有
   nboolean (*is_legitimate_pic_operand_p)(MtcsRecog *self,rtx op);
   //原型 extern void add_clobbers (rtx, int); recog.h
   void (*add_clobbers)(MtcsRecog *self,rtx pattern ATTRIBUTE_UNUSED, int insn_code_number);
   //原型 extern bool added_clobbers_hard_reg_p (int);recog.h
   bool (*added_clobbers_hard_reg_p) (MtcsRecog *self,int insn_code_number);
   //原型extern rtx_insn *peephole2_insns (rtx, rtx_insn *, int *); recog.h insn-recog.cc
   rtx_insn *(*peephole2_insns) (MtcsRecog *self, rtx x1 ATTRIBUTE_UNUSED,rtx_insn *insn ATTRIBUTE_UNUSED, int *pmatch_len_ ATTRIBUTE_UNUSED);
   //原型rtx_insn *split_insns (rtx x1 ATTRIBUTE_UNUSED, rtx_insn *insn ATTRIBUTE_UNUSED) recog.h insn-recog.cc
   rtx_insn *(*split_insns) (MtcsRecog *self,rtx x1 ATTRIBUTE_UNUSED, rtx_insn *insn ATTRIBUTE_UNUSED);
   //原型int recog (rtx x1 ATTRIBUTE_UNUSED,rtx_insn *insn ATTRIBUTE_UNUSED,int *pnum_clobbers ATTRIBUTE_UNUSED) recog.h insn-recog.cc
   int (*recog) (MtcsRecog *self,rtx x1 ATTRIBUTE_UNUSED,rtx_insn *insn ATTRIBUTE_UNUSED,int *pnum_clobbers ATTRIBUTE_UNUSED);
   //原型 insn_extract recog.h recog.cc 由mtcsgenextract.c 生成文件 平台-insn-extract.c文件实现。
   void (*insn_extract)(MtcsRecog *self,rtx_insn *insn);

   /* Target-dependent globals.  */
   struct  {
     bool x_initialized;
     mtcs_alternative_mask x_bool_attr_masks[MAX_NUM_INSN_CODES/*!NUM_INSN_CODES*/][MTCS_BA_LAST + 1];
     struct mtcs_operand_alternative *x_op_alt[MAX_NUM_INSN_CODES/*!NUM_INSN_CODES*/];
   }target_recog;
   //原型  extern struct recog_data_d recog_data; recog.h
   struct mtcs_recog_data_d recog_data;
   //原型 volatile_ok recog.h recog.cc
   int volatile_ok;
   //原型 MAX_RECOG_OPERANDS insn-config.h 平台相关
   int maxRecogOperands;
   //原型 num_changes recog.cc
   int num_changes;
   //原型 temporarily_undone_changes recog.cc
   int temporarily_undone_changes ;
   //原型 changes_allocated recog.cc
   int changes_allocated;
   //原型 static change_t *changes; recog.cc
   void *changes;
   //原型 extern const operand_alternative *recog_op_alt recog.h
   struct mtcs_operand_alternative *recog_op_alt;
   /* Used to provide recog_op_alt for asms.  */
   //原型 asm_op_alt recog.cc
   struct mtcs_operand_alternative asm_op_alt[MAX_MAX_RECOG_OPERANDS/*!MAX_RECOG_OPERANDS*/ * MAX_RECOG_ALTERNATIVES];
};

/* RAII class for temporarily setting volatile_ok.  */
//原型 temporary_volatile_ok recog.h
class mtcs_temporary_volatile_ok
{
public:
   mtcs_temporary_volatile_ok (MtcsRecog *mtcsRecog,int value) : save_volatile_ok (mtcsRecog->volatile_ok)
  {
     mtcsRecog->volatile_ok = value;
  }

  ~mtcs_temporary_volatile_ok () { mtcsRecog->volatile_ok = save_volatile_ok; }

private:
  mtcs_temporary_volatile_ok (const mtcs_temporary_volatile_ok &);
  int save_volatile_ok;
  MtcsRecog *mtcsRecog;
};

void mtcs_recog_init(MtcsRecog *self);
//原型 #define memory_address_p(mode,addr)   memory_address_addr_space_p ((mode), (addr), ADDR_SPACE_GENERIC)
bool mtcs_recog_memory_address_p (MtcsRecog *self,mtcs_mode mode ATTRIBUTE_UNUSED, rtx addr);
//原型 memory_address_addr_space_p recog.h 实现recog.cc
bool mtcs_recog_memory_address_addr_space_p (MtcsRecog *self,mtcs_mode mode ATTRIBUTE_UNUSED, rtx addr,
                         addr_space_t as, code_helper = ERROR_MARK);
//原型 strict_memory_address_addr_space_p recog.h reload.cc
bool mtcs_recog_strict_memory_address_addr_space_p (MtcsRecog *self,machine_mode mode ATTRIBUTE_UNUSED,
                    rtx addr, addr_space_t as, code_helper = ERROR_MARK);
//原型 #define strict_memory_address_p(mode,addr)
//mtcs_recog_strict_memory_address_addr_space_p ((mode), (addr), ADDR_SPACE_GENERIC) recog.h
bool mtcs_recog_strict_memory_address_p (MtcsRecog *self,machine_mode mode ATTRIBUTE_UNUSED,rtx addr);
//原型 LEGITIMATE_PIC_OPERAND_P default.h 每个平台都有
nboolean mtcs_recog_is_legitimate_pic_operand_p(MtcsRecog *self,rtx op);
//原型 offsettable_address_addr_space_p recog.h recog.cc
bool mtcs_recog_offsettable_address_addr_space_p (MtcsRecog *self,int strictp, machine_mode mode, rtx y,
                  addr_space_t as);
//原型 mode_dependent_address_p recog.h recog.cc
bool mtcs_recog_mode_dependent_address_p (MtcsRecog *self,rtx addr, addr_space_t addrspace);
//原型 offsettable_nonstrict_memref_p recog.h recog.cc
bool mtcs_recog_offsettable_nonstrict_memref_p (MtcsRecog *self,rtx op);
//原型 extern void add_clobbers (rtx, int); recog.h insn-recog.cc
void mtcs_recog_add_clobbers (MtcsRecog *self,rtx pattern ATTRIBUTE_UNUSED, int insn_code_number);
//原型 extern bool added_clobbers_hard_reg_p (int);recog.h insn-recog.cc
bool mtcs_recog_added_clobbers_hard_reg_p (MtcsRecog *self,int insn_code_number);
//原型extern rtx_insn *peephole2_insns (rtx, rtx_insn *, int *); recog.h insn-recog.cc
rtx_insn *mtcs_recog_peephole2_insns (MtcsRecog *self, rtx x1 ATTRIBUTE_UNUSED,
    rtx_insn *insn ATTRIBUTE_UNUSED, int *pmatch_len_ ATTRIBUTE_UNUSED);
//原型rtx_insn * split_insns (rtx x1 ATTRIBUTE_UNUSED, rtx_insn *insn ATTRIBUTE_UNUSED) recog.h insn-recog.cc
rtx_insn *mtcs_recog_split_insns (MtcsRecog *self,rtx x1 ATTRIBUTE_UNUSED, rtx_insn *insn ATTRIBUTE_UNUSED);
//原型int recog (rtx x1 ATTRIBUTE_UNUSED,rtx_insn *insn ATTRIBUTE_UNUSED,int *pnum_clobbers ATTRIBUTE_UNUSED) recog.h insn-recog.cc
int mtcs_recog_recog (MtcsRecog *self,rtx x1 ATTRIBUTE_UNUSED,rtx_insn *insn ATTRIBUTE_UNUSED,int *pnum_clobbers ATTRIBUTE_UNUSED);
//原型 recog_init recog.h recog.cc
void mtcs_recog_recog_init (MtcsRecog *self);
//原型 init_recog recog.h recog.cc
void mtcs_recog_init_recog (MtcsRecog *self);
//原型 init_recog_no_volatile recog.h recog.cc
void mtcs_recog_init_recog_no_volatile (MtcsRecog *self);
//原型 asm_operand_ok recog.h recog.cc
int mtcs_recog_asm_operand_ok (MtcsRecog *self,rtx op, const char *constraint, const char **constraints);
//原型 extract_insn recog.h recog.cc
 //MAX_RECOG_OPERANDS是平台相关的，其它引用函数 asm_noperands、decode_asm_operands 不需要改变
void mtcs_recog_extract_insn (MtcsRecog *self,rtx_insn *insn);
//原型 MAX_RECOG_OPERANDS insn-config.h 平台相关
void mtcs_recog_set_max_recog_operands(MtcsRecog *self,int maxValue);
int  mtcs_recog_get_max_recog_operands(MtcsRecog *self);
//原型 reg_fits_class_p recog.h recog.cc
bool mtcs_recog_reg_fits_class_p (MtcsRecog *self,const_rtx operand, reg_class_t cl, int offset,
        machine_mode mode);
//原型 constrain_operands recog.h recog.cc
bool mtcs_recog_constrain_operands (MtcsRecog *self,int strict,mtcs_alternative_mask/*!alternative_mask*/ alternatives);
//原型 constrain_operands_cached recog.h recog.cc
bool mtcs_recog_constrain_operands_cached (MtcsRecog *self,rtx_insn *insn, int strict);
//原型 check_asm_operands recog.h recog.cc
bool mtcs_recog_check_asm_operands (MtcsRecog *self,rtx x);
//原型 get_enabled_alternatives recog.h recog.cc
mtcs_alternative_mask mtcs_recog_get_enabled_alternatives (MtcsRecog *self,rtx_insn *insn);

/* Try recognizing the instruction INSN,
   and return the code number that results.
   Remember the code so that repeated calls do not
   need to spend the time for actual rerecognition.

   This function is the normal interface to instruction recognition.
   The automatically-generated function `recog' is normally called
   through this one.  */
//原型 recog_memoized recog.h
inline int mtcs_recog_recog_memoized (MtcsRecog *self,rtx_insn *insn)
{
   if (INSN_CODE (insn) < 0)
      INSN_CODE (insn) =mtcs_recog_recog/*!recog*/(self,PATTERN (insn), insn, 0);
   return INSN_CODE (insn);
}

//原型 num_validated_changes recog.h recog.cc
int mtcs_recog_num_validated_changes (MtcsRecog *self);
//原型 validate_change recog.h recog.cc
bool mtcs_recog_validate_change (MtcsRecog *self,rtx object, rtx *loc, rtx new_rtx, bool in_group);
//原型 apply_change_group recog.h recog.cc
bool mtcs_recog_apply_change_group (MtcsRecog *self);
//原型 verify_changes recog.h recog.cc
bool mtcs_recog_verify_changes (MtcsRecog *self,int num);
//原型 insn_invalid_p recog.h recog.cc
bool mtcs_recog_insn_invalid_p (MtcsRecog *self,rtx_insn *insn, bool in_group);
//原型 get_preferred_alternatives recog.h recog.cc
mtcs_alternative_mask mtcs_recog_get_preferred_alternatives (MtcsRecog *self,rtx_insn *insn);
//原型 get_preferred_alternatives recog.h recog.cc 重载函数
mtcs_alternative_mask mtcs_recog_get_preferred_alternatives (MtcsRecog *self,rtx_insn *insn, basic_block bb);
//原型 confirm_change_group recog.h recog.cc
void mtcs_recog_confirm_change_group (MtcsRecog *self);
//原型 cancel_changes recog.h recog.cc
void mtcs_recog_cancel_changes (MtcsRecog *self,int num);
//原型 copy_frame_info_to_split_insn recog.h recog.cc
void mtcs_recog_copy_frame_info_to_split_insn (MtcsRecog *self,rtx_insn *old_insn, rtx_insn *new_insn);
//原型 validate_replace_rtx recog.h recog.cc
bool mtcs_recog_validate_replace_rtx (MtcsRecog *self,rtx from, rtx to, rtx_insn *insn);
//原型 validate_unshare_change recog.h recog.cc
bool mtcs_recog_validate_unshare_change (MtcsRecog *self,rtx object, rtx *loc, rtx new_rtx, bool in_group);
//原型 temporarily_undo_changes recog.h recog.cc
void mtcs_recog_temporarily_undo_changes (MtcsRecog *self,int num);
//原型 validate_change_xveclen recog.h recog.cc
bool mtcs_recog_validate_change_xveclen (MtcsRecog *self,rtx object, rtx *loc, int new_len, bool in_group);
//原型 redo_changes recog.h recog.cc
void mtcs_recog_redo_changes (MtcsRecog *self,int num);
//原型 preprocess_constraints recog.h recog.cc
void mtcs_recog_preprocess_constraints (MtcsRecog *self,int n_operands, int n_alternatives,
         const char **constraints, struct mtcs_operand_alternative *op_alt_base, rtx **oploc);
//原型 preprocess_constraints recog.h recog.cc 重载函数
void mtcs_recog_preprocess_constraints (MtcsRecog *self,rtx_insn *insn);
//原型 preprocess_insn_constraints recog.h recog.cc
const struct mtcs_operand_alternative * mtcs_recog_preprocess_insn_constraints (MtcsRecog *self,unsigned int icode);
//原型 alternative_class recog.h
enum reg_class mtcs_recog_alternative_class (const struct mtcs_operand_alternative *alt, int i);
//原型 alternative_class recog.h
unsigned int mtcs_recog_alternative_register_filters (const struct mtcs_operand_alternative *alt, int i);
//原型 validate_replace_src_group recog.h recog.cc
void mtcs_recog_validate_replace_src_group (MtcsRecog *self,rtx from, rtx to, rtx_insn *insn);
//原型 valid_insn_p recog.h recog.cc
bool mtcs_recog_valid_insn_p (MtcsRecog *self,rtx_insn *insn);
//原型 split_all_insns rtl.h recog.cc
void mtcs_recog_split_all_insns (MtcsRecog *self);
//原型 extract_insn_cached recog.h recog.cc
void mtcs_recog_extract_insn_cached (MtcsRecog *self,rtx_insn *insn);
//原型 split_all_insns_noflow rtl.h recog.cc
void mtcs_recog_split_all_insns_noflow (MtcsRecog *self);
//原型 validate_replace_rtx_group recog.h recog.cc
void mtcs_recog_validate_replace_rtx_group (MtcsRecog *self,rtx from, rtx to, rtx_insn *insn);
//原型 num_changes_pending recog.h recog.cc
int mtcs_recog_num_changes_pending (MtcsRecog *self);
/* A class for substituting one rtx for another within an instruction,
   or for recursively simplifying the instruction as-is.  Derived classes
   can record or filter certain decisions.  */
//原型 insn_propagation recog.h recog.cc
class mtcs_insn_propagation : public simplify_context
{
public:
  /* Assignments for RESULT_FLAGS.

     UNSIMPLIFIED is true if a substitution has been made inside an rtx
     X and if neither X nor its parent expressions could be simplified.

     FIRST_SPARE_RESULT is the first flag available for derived classes.  */
  static const uint16_t UNSIMPLIFIED = 1U << 0;
  static const uint16_t FIRST_SPARE_RESULT = 1U << 1;

  mtcs_insn_propagation (rtx_insn *);
  mtcs_insn_propagation (rtx_insn *, rtx, rtx, bool = true);
  bool apply_to_pattern (rtx *);
  bool apply_to_rvalue (rtx *);
  bool apply_to_note (rtx *);

  /* Return true if we should accept a substitution into the address of
     memory expression MEM.  Undoing changes OLD_NUM_CHANGES and up restores
     MEM's original address.  */
  virtual bool check_mem (int /*old_num_changes*/,
           rtx /*mem*/) { return true; }

  /* Note that we've simplified OLD_RTX into NEW_RTX.  When substituting,
     this only happens if a substitution occured within OLD_RTX.
     Undoing OLD_NUM_CHANGES and up will restore the old form of OLD_RTX.
     OLD_RESULT_FLAGS is the value that RESULT_FLAGS had before processing
     OLD_RTX.  */
  virtual void note_simplification (int /*old_num_changes*/,
                uint16_t /*old_result_flags*/,
                rtx /*old_rtx*/, rtx /*new_rtx*/) {}
private:
  bool apply_to_mem_1 (rtx);
  bool apply_to_lvalue_1 (rtx);
  bool apply_to_rvalue_1 (rtx *);
  bool apply_to_pattern_1 (rtx *);

public:
  /* The instruction that we are simplifying or propagating into.  */
  rtx_insn *insn;

  /* If FROM is nonnull, we're replacing FROM with TO, otherwise we're
     just doing a recursive simplification.  */
  rtx from;
  rtx to;

  /* The number of times that we have replaced FROM with TO.  */
  unsigned int num_replacements;

  /* A bitmask of flags that describe the result of the simplificiation;
     see above for details.  */
  uint16_t result_flags : 16;

  /* True if we should unshare TO when making the next substitution,
     false if we can use TO itself.  */
  uint16_t should_unshare : 1;

  /* True if we should call check_mem after substituting into a memory.  */
  uint16_t should_check_mems : 1;

  /* True if we should call note_simplification after each simplification.  */
  uint16_t should_note_simplifications : 1;

  /* For future expansion.  */
  uint16_t spare : 13;

  /* Gives the reason that a substitution failed, for debug purposes.  */
  const char *failure_reason;
  MtcsRecog *mtcsRecog;

};

/* Try to replace FROM with TO in INSN.  SHARED_P is true if TO is shared
   with other instructions, false if INSN can use TO directly.  */

inline mtcs_insn_propagation::mtcs_insn_propagation (rtx_insn *insn, rtx from, rtx to,
                  bool shared_p)
  : insn (insn),
    from (from),
    to (to),
    num_replacements (0),
    result_flags (0),
    should_unshare (shared_p),
    should_check_mems (false),
    should_note_simplifications (false),
    spare (0),
    failure_reason (nullptr)
{
}

/* Try to simplify INSN without performing a substitution.  */

inline mtcs_insn_propagation::mtcs_insn_propagation (rtx_insn *insn)
  : mtcs_insn_propagation (insn, NULL_RTX, NULL_RTX)
{
}

//重要，还原被替换的指令
class mtcs_insn_change_watermark
{
public:
   mtcs_insn_change_watermark (MtcsRecog *recog) : mtcsRecog (recog)
   {
      m_old_num_changes = mtcs_recog_num_validated_changes(mtcsRecog);
   }
  ~mtcs_insn_change_watermark ();
  void keep ()
  {
     m_old_num_changes = mtcs_recog_num_validated_changes(mtcsRecog);
  }

  MtcsRecog *mtcsRecog;

private:
  int m_old_num_changes;
};

inline mtcs_insn_change_watermark::~mtcs_insn_change_watermark ()
{
  if (m_old_num_changes < mtcs_recog_num_validated_changes(mtcsRecog))
     mtcs_recog_cancel_changes/*!cancel_changes*/(mtcsRecog,m_old_num_changes);
}




/****************************以下是基于MtcsRecog的 rtl pass **************************************/
//原型 NEXT_PASS (pass_split_all_insns, 1); RTL_PASS recog.cc split1 n 无条件执行  split_all_insns ();
typedef struct _MtcsPassSplitAllInsns MtcsPassSplitAllInsns;
struct _MtcsPassSplitAllInsns
{
   MtcsPass parent;
};
MtcsPassSplitAllInsns *mtcs_pass_split_all_insns_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_split_after_reload, 1); RTL_PASS recog.cc split2 y 有条件执行  optimize > 0; split_all_insns
typedef struct _MtcsPassSplitAfterReload MtcsPassSplitAfterReload;
struct _MtcsPassSplitAfterReload
{
   MtcsPass parent;
};
MtcsPassSplitAfterReload *mtcs_pass_split_after_reload_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_split_before_sched2, 1); RTL_PASS recog.cc split3 y 有条件执行  enable_split_before_sched2; split_all_insns
typedef struct _MtcsPassSplitBeforeSched2 MtcsPassSplitBeforeSched2;
struct _MtcsPassSplitBeforeSched2
{
   MtcsPass parent;
};
MtcsPassSplitBeforeSched2 *mtcs_pass_split_before_sched2_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_split_before_regstack, 1); RTL_PASS recog.cc split4 y 有条件执行  enable_split_before_sched2; split_all_insns
typedef struct _MtcsPassSplitBeforeRegstack MtcsPassSplitBeforeRegstack;
struct _MtcsPassSplitBeforeRegstack
{
   MtcsPass parent;
};
MtcsPassSplitBeforeRegstack *mtcs_pass_split_before_regstack_new(MtcsMode *mtcsMode);

//原型 NEXT_PASS (pass_split_for_shorten_branches, 1); RTL_PASS recog.cc split5 y 有条件执行  enable_split_before_sched2; split_all_insns
typedef struct _MtcsPassSplitForShortenBranches MtcsPassSplitForShortenBranches;
struct _MtcsPassSplitForShortenBranches
{
   MtcsPass parent;
};
MtcsPassSplitForShortenBranches *mtcs_pass_split_for_shorten_branches_new(MtcsMode *mtcsMode);

#endif
