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

#ifndef __GCC_MTCS_REG__
#define __GCC_MTCS_REG__

#include "../nlib.h"
#include "mtcsmicro.h"
#include "mtcscomponent.h"
#include "mtcsfuncabi.h"

#define MAX_PSEUDO_REGISTER 200 //定义最大的伪寄存器
#define MAX_REG_CLASSES   50 //定义最大的寄存器类别数

#define MAX_REG_INTS   ((MAX_FIRST_PSEUDO_REGISTER + (32 - 1)) / 32)

#define MTCS_HARD_CONST(X) ((unsigned long) (X))
#define MTCS_UHOST_BITS_PER_WIDE_INT ((unsigned) HOST_BITS_PER_WIDEST_FAST_INT)

//统一 reg_class REG_CLASS_NAMES REG_CLASS_CONTENTS
typedef struct _MtcsRegClass
{
    nuint number;
    char *name;     //原型 reg_class_names hard-reg-set.h
    int regInts[10];//一般不会超过10个
}MtcsRegClass;

/* Structure used to record preferences of given pseudo.  */
//原型 struct reg_pref reginfo.cc
typedef struct _RegPref
{
  /* (enum reg_class) prefclass is the preferred class.  May be
     NO_REGS if no class is better than memory.  */
  char prefclass;

  /* altclass is a register class that we should use for allocating
     pseudo if no register in the preferred class is available.
     If no register in this class is available, memory is preferred.

     It might appear to be more general to have a bitmask of classes here,
     but since it is recommended that there be a class corresponding to the
     union of most major pair of classes, that generality is not required.  */
  char altclass;

  /* allocnoclass is a register class that IRA uses for allocating
     the pseudo.  */
  char allocnoclass;
}RegPref;

class mtcs_simplifiable_subreg;
struct mtcs_simplifiable_subregs_hasher : nofree_ptr_hash <mtcs_simplifiable_subreg>
{
  typedef const subreg_shape *compare_type;

  static inline hashval_t hash (const mtcs_simplifiable_subreg *);
  static inline bool equal (const mtcs_simplifiable_subreg *, const subreg_shape *);
};


//原型 hard-reg-set.h struct target_hard_regs
typedef struct _MtcsHardReg {
    //target_hard_regs 平台硬件寄存器 来自hard-reg-set.h
  //void finalize ();

  /* The set of registers that actually exist on the current target.  */
    //原型 x_accessible_reg_set hard-reg-set.h #define accessible_reg_set (this_target_hard_regs->x_accessible_reg_set)
    HardRegSet x_accessible_reg_set;

  /* The set of registers that should be considered to be register
     operands.  It is a subset of x_accessible_reg_set.  */
    HardRegSet x_operand_reg_set;

  /* Indexed by hard register number, contains 1 for registers
     that are fixed use (stack pointer, pc, frame pointer, etc.;.
     These are the registers that cannot be used to allocate
     a pseudo reg whose life does not cross calls.  */
  char x_fixed_regs[MAX_FIRST_PSEUDO_REGISTER];

  /* The same info as a HARD_REG_SET.  */
  HardRegSet x_fixed_reg_set;

  /* Indexed by hard register number, contains 1 for registers
     that are fixed use or are clobbered by function calls.
     These are the registers that cannot be used to allocate
     a pseudo reg whose life crosses calls.  */
  char x_call_used_regs[MAX_FIRST_PSEUDO_REGISTER];

  /* For targets that use reload rather than LRA, this is the set
     of registers that we are able to save and restore around calls
     (i.e. those for which we know a suitable mode and set of
     load/store instructions exist).  For LRA targets it contains
     all registers.

     This is legacy information and should be removed if all targets
     switch to LRA.  */
  HardRegSet x_savable_regs;

  /* Contains registers that are fixed use -- i.e. in fixed_reg_set -- but
     only if they are not merely part of that set because they are global
     regs.  Global regs that are not otherwise fixed can still take part
     in register allocation.  */
  HardRegSet x_fixed_nonglobal_reg_set;

  /* Contains 1 for registers that are set or clobbered by calls.  */
  /* ??? Ideally, this would be just call_used_regs plus global_regs, but
     for someone's bright idea to have call_used_regs strictly include
     fixed_regs.  Which leaves us guessing as to the set of fixed_regs
     that are actually preserved.  We know for sure that those associated
     with the local stack frame are safe, but scant others.  */
  HardRegSet x_regs_invalidated_by_call;
  /* The set of registers that are used by EH_RETURN_DATA_REGNO.  */
  HardRegSet x_eh_return_data_regs;
  /* Table of register numbers in the order in which to try to use them.  */
  int x_reg_alloc_order[MAX_FIRST_PSEUDO_REGISTER];

  /* The inverse of reg_alloc_order.  */
  int x_inv_reg_alloc_order[MAX_FIRST_PSEUDO_REGISTER];

  /* For each reg class, a HARD_REG_SET saying which registers are in it.  */
  //原型 reg_class_contents hard-reg-set.h 宏
  HardRegSet x_reg_class_contents[MAX_REG_CLASSES];

  /* For each reg class, a boolean saying whether the class contains only
     fixed registers.  */
  bool x_class_only_fixed_regs[MAX_REG_CLASSES];

  /* For each reg class, number of regs it contains.  */
  unsigned int x_reg_class_size[MAX_REG_CLASSES];

  /* For each reg class, table listing all the classes contained in it.  */
  mtcs_reg_class /*!enum reg_class*/ x_reg_class_subclasses[MAX_REG_CLASSES][MAX_REG_CLASSES];

  /* For each pair of reg classes,
     a largest reg class contained in their union.  */
  mtcs_reg_class /*!enum reg_class*/ x_reg_class_subunion[MAX_REG_CLASSES][MAX_REG_CLASSES];

  /* For each pair of reg classes,
     the smallest reg class that contains their union.  */
  mtcs_reg_class /*!enum reg_class*/ x_reg_class_superunion[MAX_REG_CLASSES][MAX_REG_CLASSES];

  /* Vector indexed by hardware reg giving its name.  */
  const char *x_reg_names[MAX_FIRST_PSEUDO_REGISTER];

  /* Records which registers can form a particular subreg, with the subreg
     being identified by its outer mode, inner mode and offset.  */
  hash_table <mtcs_simplifiable_subregs_hasher> *x_simplifiable_subregs;
  //从结体件target_regs(regs.h)合并到这里
  /* For each starting hard register, the number of consecutive hard
     registers that a given machine mode occupies.  */
  unsigned char x_hard_regno_nregs[MAX_FIRST_PSEUDO_REGISTER][MAX_FIRST_PSEUDO_REGISTER];

  /* The max value found in x_hard_regno_nregs.  */
  unsigned char x_hard_regno_max_nregs;

  /* For each hard register, the widest mode object that it can contain.
     This will be a MODE_INT mode if the register can hold integers.  Otherwise
     it will be a MODE_FLOAT or a MODE_CC mode, whichever is valid for the
     register.  */
  //原型   machine_mode x_reg_raw_mode[FIRST_PSEUDO_REGISTER]; regs.h
  machine_mode x_reg_raw_mode[MAX_FIRST_PSEUDO_REGISTER];

  /* Vector indexed by machine mode saying whether there are regs of
     that mode.  */
  bool x_have_regs_of_mode[MAX_FIRST_PSEUDO_REGISTER];

  /* 1 if the corresponding class contains a register of the given mode.  */
  char x_contains_reg_of_mode[MAX_FIRST_PSEUDO_REGISTER][MAX_FIRST_PSEUDO_REGISTER];

  /* 1 if the corresponding class contains a register of the given mode
     which is not global and can therefore be allocated.  */
  char x_contains_allocatable_reg_of_mode[MAX_FIRST_PSEUDO_REGISTER][MAX_FIRST_PSEUDO_REGISTER];

  /* Record for each mode whether we can move a register directly to or
     from an object of that mode in memory.  If we can't, we won't try
     to use that mode directly when accessing a field of that mode.  */
  char x_direct_load[MAX_FIRST_PSEUDO_REGISTER];
  char x_direct_store[MAX_FIRST_PSEUDO_REGISTER];

  /* Record for each mode whether we can float-extend from memory.  */
  bool x_float_extend_from_mem[MAX_FIRST_PSEUDO_REGISTER][MAX_FIRST_PSEUDO_REGISTER];


}MtcsHardReg;

typedef struct _MtcsReg MtcsReg;
struct _MtcsReg
{
    MtcsComponent parent;
    int hardRegsCount;//int first_pseudo_register;//每个平台不一样 ptx=16 gcn=677 FIRST_PSEUDO_REGISTER 平台第一个伪寄存器的编号
    int hardRegElement; //原型 HARD_REG_SET ARRAY_SIZE (elts)


    /* Target-dependent globals.  */
    struct { //常用寄存器号码
       nuint stack_pointer_regnum;//原型 STACK_POINTER_REGNUM
       nuint frame_pointer_regnum; //原型 FRAME_POINTER_REGNUM
       nuint hard_frame_pointer_regnum; //原型 HARD_FRAME_POINTER_REGNUM
       nuint arg_pointer_regnum;  //原型 ARG_POINTER_REGNUM
       nuint virtual_incoming_args_regnum;//原型 VIRTUAL_INCOMING_ARGS_REGNUM rtl.h
       nuint virtual_stack_vars_regnum;    //原型 VIRTUAL_STACK_VARS_REGNUM rtl.h
       nuint virtual_stack_dynamic_regnum; //原型 VIRTUAL_STACK_DYNAMIC_REGNUM rtl.h
       nuint virtual_outgoing_args_regnum; //原型 VIRTUAL_OUTGOING_ARGS_REGNUM rtl.h
       nuint virtual_cfa_regnum;           //原型 VIRTUAL_CFA_REGNUM rtl.h
       nuint virtual_preferred_stack_boundary_regnum; //原型 VIRTUAL_PREFERRED_STACK_BOUNDARY_REGNUM rtl.h
    }normalHardRegsNum;
    int  return_address_pointer_regnum;//RETURN_ADDRESS_POINTER_REGNUM
    int  pic_offset_table_regnum;//原型 PIC_OFFSET_TABLE_REGNUM defaults.h

    MtcsHardReg hardRegs;
    char *(*get_fixed_registers)(MtcsReg *self);
    char *(*get_call_used_registers)(MtcsReg *self);
    char **(*get_register_names)(MtcsReg *self);
    //原型 REGNO_REG_CLASS
    int   (*get_class)(MtcsReg *self,int regno);
    //原型 REGNO_PTR_FRAME_P rtl.h
    nboolean (*regnum_ptr_frame_p)(MtcsReg *self,int regnum);
    //原型 init_reg_class_start_regs reginfo.cc  In insn-preds.cc.
    void  (*init_reg_class_start_regs)(MtcsReg *self);
    //原型 #define EH_RETURN_DATA_REGNO(N) INVALID_REGNUM default.h
    int (*eh_return_data_regno)(MtcsReg *self,int n);
    //原型 enum reg_class base_reg_class addresses.h
    enum reg_class (*base_reg_class) (MtcsReg *self,machine_mode mode ATTRIBUTE_UNUSED,
          addr_space_t as ATTRIBUTE_UNUSED, enum rtx_code outer_code ATTRIBUTE_UNUSED,
          enum rtx_code index_code ATTRIBUTE_UNUSED,rtx_insn *insn ATTRIBUTE_UNUSED);
    //原型 enum reg_class index_reg_class addresses.h
    enum reg_class (* index_reg_class) (MtcsReg *self,rtx_insn *insn ATTRIBUTE_UNUSED);
    //原型 bool ok_for_base_p_1 addresses.h
    bool(*  ok_for_base_p_1) (MtcsReg *self,unsigned regno ATTRIBUTE_UNUSED,
           machine_mode mode ATTRIBUTE_UNUSED,
           addr_space_t as ATTRIBUTE_UNUSED,
           enum rtx_code outer_code ATTRIBUTE_UNUSED,
           enum rtx_code index_code ATTRIBUTE_UNUSED,
           rtx_insn* insn ATTRIBUTE_UNUSED /*!= NULL*/);
    //原型 #define REGNO_OK_FOR_INDEX_P(X) false nvptx.h
    bool (*regno_ok_for_index_p)(MtcsReg *self,unsigned regno);
    //原型 #define DEBUGGER_REGNO(N) N
    int (* get_debugger_regno)(MtcsReg *self,int regno);
   //原型 #define DWARF_FRAME_REGNUM(REG) DEBUGGER_REGNO (REG)
    int (* get_dwarf_frame_regnum)(MtcsReg *self,int regno);
   //原型 #define DWARF2_FRAME_REG_OUT(REGNO, FOR_EH) (REGNO)
    int (* get_dwarf2_frame_reg_out)(MtcsReg *self,int regno,int forEh);
    //原型 #define DWARF_REG_TO_UNWIND_COLUMN(REGNO) (REGNO)
    int (* get_dwarf_reg_to_unwind_column)(MtcsReg *self,int regno);
    //原型 #define DWARF_FRAME_REGISTERS FIRST_PSEUDO_REGISTER
    int dwarfFrameRegisters;
    //原型 #define DWARF_FRAME_RETURN_COLUMN   DWARF_FRAME_REGISTERS
    int (*get_dwarf_frame_return_column)(MtcsReg *self);
    //一但设定硬件寄存器 firstPseudoRegNo lastVirtualRegNo也确定
    int firstPseudoRegNo;//伪寄存器号是从hardRegsCount开始,例如 ptx 的 hardRegsCount=16 则firstPseudoRegNo=16
    int lastVirtualRegNo;//最后一个虚拟寄存器号是firstPseudoRegNo+5，留给virtual_incoming_args_regnum,virtual_stack_vars_regnum等。


    MtcsRegClass *mtcsRegClass;//原型nvptx.h REG_CLASS_NAMES
    nuint mtcsRegClassCount; //原型 N_REG_CLASSES nvptx.h
    nuint generalRegs ;//原型 GENERAL_REGS nvptx.h
    nuint allRegs ;//原型 ALL_REGS nvptx.h

    /* MOVE_MAX_PIECES is the number of bytes at a time which we can
       move efficiently, as opposed to  MOVE_MAX which is the maximum
       number of bytes we can move with a single instruction.  */
    nuint move_max ;//原型 MOVE_MAX nvptx.h default.h引用
    nuint store_max_pieces;//原型 STORE_MAX_PIECES default.h
    nuint max_move_max;//原型 MAX_MOVE_MAX default.h
    nuint move_max_pieces;//原型 MOVE_MAX_PIECES default.h
    nuint comapre_max_pieces;//原型 COMPARE_MAX_PIECES default.h
    int firstStackReg;//原型 FIRST_STACK_REG i386.h nvptx没定义
    int lastStackReg;//原型 LAST_STACK_REG i386.h nvptx没定义

    struct{
        /* We need to save copies of some of the register information which
           can be munged by command-line switches so we can restore it during
           subsequent back-end reinitialization.  */
        char saved_fixed_regs[MAX_FIRST_PSEUDO_REGISTER];
        char saved_call_used_regs[MAX_FIRST_PSEUDO_REGISTER];
        const char *saved_reg_names[MAX_FIRST_PSEUDO_REGISTER];
        HardRegSet saved_accessible_reg_set;
        HardRegSet saved_operand_reg_set;
    }saveRegs;
    //原型 global_regs hard-reg-set.h reginfo.cc
    char global_regs[MAX_FIRST_PSEUDO_REGISTER];
    //原型 global_reg_set hard-reg-set.h reginfo.cc
    HardRegSet global_reg_set;
    //原型 HARD_FRAME_POINTER_IS_FRAME_POINTER rtl.h
    nboolean hardFramePointerIsFramePointer;
    //原型 PIC_OFFSET_TABLE_REG_CALL_CLOBBERED default.h
    int picOffsetTableRegCallClobbered;
    //原型 ELIMINABLE_REGS 每个平台设置不一样的ELIMINABLE_REGS
    struct{
         int from;
         int to;
    }eliminableRegs[10];
    int elimiableRegsCount;


    struct{
        /* Record preferences of each pseudo.  This is available after RA is
         run.  */
        //原型 struct reg_pref *reg_pref reginfo.cc;
        RegPref *reg_pref;
        /* Current size of reg_info.  */
       //原型 reg_info_size reginfo.cc ;
       int reg_info_size;
       /* Max_reg_num still last resize_reg_info call.  */
       //原型 max_regno_since_last_resize reginfo.cc ;
       int max_regno_since_last_resize;
     }refInfo;

     /* Data for initializing fixed_regs.  */
     //原型 initial_fixed_regs reginfo.cc
     const char *initial_fixed_regs; //[] = FIXED_REGISTERS;
     //原型 initial_call_used_regs reginfo.cc
     const char *initial_call_used_regs;//[] = CALL_USED_REGISTERS;
     /* Declaration for the global register. */
     //原型 global_regs_decl rtl.h reginfo.cc
     tree global_regs_decl[MAX_FIRST_PSEUDO_REGISTER/*!FIRST_PSEUDO_REGISTER*/];
     /* Used to initialize reg_alloc_order.  */
     //原型 int initial_reg_alloc_order[FIRST_PSEUDO_REGISTER] = REG_ALLOC_ORDER reginfo.cc
     int *initial_reg_alloc_order;
     //原型 int_reg_class_contents reginfo.cc
     unsigned int_reg_class_contents[MAX_REG_CLASSES][MAX_REG_INTS];
     /* Array containing all of the register names.  */
     //原型 static const char *const initial_reg_names[] = REGISTER_NAMES;
     const char *const initial_reg_names[MAX_REG_CLASSES] ;
     /* No more global register variables may be declared; true once
        reginfo has been initialized.  */
     //原型 no_global_reg_vars reginfo.cc
      int no_global_reg_vars = 0;
      /* Passes for keeping and updating info about modes of registers
         inside subregisters.  */
      //原型 valid_mode_changes reginfo.cc
      HardRegSet **valid_mode_changes;
      //原型 valid_mode_changes_obstack reginfo.cc
       obstack valid_mode_changes_obstack;

       //原型 #define WORD_REGISTER_OPERATIONS 0 defaults.h
       int wordRegisterOpeations;
      //原型 #define INDEX_REG_CLASS NO_REGS nvptx.h
       int indexRegClass;

};

void     mtcs_reg_init(MtcsReg *self);
void     mtcs_reg_set_hard_reg_count(MtcsReg *self,int first_pseudo_register);
void     mtcs_reg_copy_reg_class_contents(MtcsReg *self,unsigned int **regClassContents,int dimX,int dimY);


const char *mtcs_reg_get_reg_name(MtcsReg *self,int index);
void     mtcs_reg_set_stack_pointer_to_virtual_regnum(MtcsReg *self,nuint *regNums);
//原型 #define HARD_REGISTER_NUM_P(REG_NO) ((REG_NO) < FIRST_PSEUDO_REGISTER) rtl.h
nboolean mtcs_reg_is_hard(MtcsReg *self,nuint regno);
//原型 #define HARD_REGISTER_P(REG) HARD_REGISTER_NUM_P (REGNO (REG)) rtl.h
nboolean mtcs_reg_is_hard_rtx(MtcsReg *self,rtx x);
nuint    mtcs_reg_get_reg_nums(MtcsReg *self, machine_mode mode, nuint regno);
nuint    mtcs_reg_get_hard_reg_count(MtcsReg *self);
void     mtcs_reg_set_return_address_pointer_regnum(MtcsReg *self,int regno);
void     mtcs_reg_set_pic_offset_table_regnum(MtcsReg *self,int regno);
//原型 LAST_VIRTUAL_REGISTER  rtl.h
int      mtcs_reg_get_last_virtual_regno(MtcsReg *self);
//原型 FIRST_PSEUDO_REGISTER rtl.h
int      mtcs_reg_get_first_pseudo_register(MtcsReg *self);
//原型 #define FIRST_VIRTUAL_REGISTER   (FIRST_PSEUDO_REGISTER)
int      mtcs_reg_get_first_virtual_register(MtcsReg *self);
//原型 N_REG_CLASSES
int mtcs_reg_get_n_reg_classes(MtcsReg *self);
//原型 LIM_REG_CLASSES
int mtcs_reg_get_lim_reg_classes(MtcsReg *self);
//原型 REGNO_REG_CLASS
int      mtcs_reg_get_class(MtcsReg *self,int regno);
//原型 inline bool in_hard_reg_set_p (const_hard_reg_set regs, machine_mode mode,unsigned int regno)
bool     mtcs_reg_in_hard_reg_set_p (MtcsReg *self,HardRegSet *regs, mtcs_mode mode,unsigned int regno);
//原型 end_hard_regno regs.h
unsigned int mtcs_reg_end_hard_regno (MtcsReg *self,mtcs_mode mode, unsigned int regno);
//原型 hard_regno_nregs regs.h
unsigned char mtcs_reg_hard_regno_nregs (MtcsReg *self,unsigned int regno, machine_mode mode);
//原型 REGNO_PTR_FRAME_P rtl.h
nboolean mtcs_reg_regnum_ptr_frame_p(MtcsReg *self,int regnum);
//原型#define VIRTUAL_REGISTER_NUM_P(REG_NO)   IN_RANGE (REG_NO, FIRST_VIRTUAL_REGISTER, LAST_VIRTUAL_REGISTER)
//rtl.h
nboolean mtcs_reg_virtual_register_num_p(MtcsReg *self,int regNo);
//原型 #define VIRTUAL_REGISTER_P(REG) VIRTUAL_REGISTER_NUM_P (REGNO (REG))
nboolean mtcs_reg_virtual_register_p(MtcsReg *self,rtx x);
//原型 REG_CLASS_NAMES  REG_CLASS_CONTENTS
void mtcs_reg_set_reg_class(MtcsReg *self,MtcsRegClass *mtcsRegClass,int count);
MtcsRegClass *mtcs_reg_get_reg_class(MtcsReg *self);

//原型 GENERAL_REGS nvptx.h
void mtcs_reg_set_general_regs(MtcsReg *self,nuint generalRegs);
//原型 GENERAL_REGS nvptx.h
nuint mtcs_reg_get_general_regs(MtcsReg *self);

//原型 ALL_REGS nvptx.h
void mtcs_reg_set_all_regs(MtcsReg *self,nuint allRegs);
//原型 ALL_REGS nvptx.h
nuint mtcs_reg_get_all_regs(MtcsReg *self);

//原型 #define direct_load (this_target_regs->x_direct_load) regs.h
char mtcs_reg_get_direct_load(MtcsReg *self,int index);
//原型 #define direct_load (this_target_regs->x_direct_load) rtl.h
void mtcs_reg_set_direct_load(MtcsReg *self,int index,int value);
//原型 MOVE_MAX nvptx.h =8 host=64
void mtcs_reg_set_move_max(MtcsReg *self,nuint value);
nuint mtcs_reg_get_move_max(MtcsReg *self);
//原型 STORE_MAX_PIECES default.h MIN (MOVE_MAX_PIECES, 2 * sizeof (HOST_WIDE_INT))
void mtcs_reg_set_store_max_pieces(MtcsReg *self,nuint value);
nuint mtcs_reg_get_store_max_pieces(MtcsReg *self);
//原型 MAX_MOVE_MAX default.h
void mtcs_reg_set_max_move_max(MtcsReg *self,nuint value);
nuint mtcs_reg_get_max_move_max(MtcsReg *self);
//原型 MOVE_MAX_PIECES default.h
void mtcs_reg_set_move_max_pieces(MtcsReg *self,nuint value);
nuint mtcs_reg_get_move_max_pieces(MtcsReg *self);
//原型 COMPARE_MAX_PIECES default.h
void mtcs_reg_set_compare_max_pieces(MtcsReg *self,nuint value);
nuint mtcs_reg_get_compare_max_pieces(MtcsReg *self);
//原型 FIRST_STACK_REG i386.h nvptx没定义
void mtcs_reg_set_first_stack_reg(MtcsReg *self,nuint value);
int mtcs_reg_get_first_stack_reg(MtcsReg *self);
//原型 LAST_STACK_REG i386.h nvptx没定义
void mtcs_reg_set_last_stack_reg(MtcsReg *self,nuint value);
int mtcs_reg_get_last_stack_reg(MtcsReg *self);
//原型 #define DEBUGGER_REGNO(N) N
int mtcs_reg_get_debugger_regno(MtcsReg *self,int regno);
//原型 #define DWARF_FRAME_REGNUM(REG) DEBUGGER_REGNO (REG)
int mtcs_reg_get_dwarf_frame_regnum(MtcsReg *self,int regno);
//原型 #define DWARF2_FRAME_REG_OUT(REGNO, FOR_EH) (REGNO)
int mtcs_reg_get_dwarf2_frame_reg_out(MtcsReg *self,int regno,int forEh);
//原型 #define DWARF_REG_TO_UNWIND_COLUMN(REGNO) (REGNO)
int mtcs_reg_get_dwarf_reg_to_unwind_column(MtcsReg *self,int regno);
//原型 #define DWARF_FRAME_REGISTERS FIRST_PSEUDO_REGISTER
void mtcs_reg_set_dwarf_frame_registers(MtcsReg *self,int value);
int  mtcs_reg_get_dwarf_frame_registers(MtcsReg *self);
//原型 #define DWARF_FRAME_RETURN_COLUMN   DWARF_FRAME_REGISTERS
int mtcs_reg_get_dwarf_frame_return_column(MtcsReg *self);

//原型 get_move_ratio target.h targhooks.cc
unsigned int mtcs_reg_get_move_ratio (MtcsReg *self, bool speed_p ATTRIBUTE_UNUSED);
//原型 #define REG_SET_TO_HARD_REG_SET(TO, FROM)          \
//do {                          \
//  CLEAR_HARD_REG_SET (TO);                \
//  reg_set_to_hard_reg_set (&TO, FROM);             \
//} while (0)
void mtcs_reg_reg_set_to_hard_reg_set (MtcsReg *self,HardRegSet *to, const_bitmap from);


//原型 SET_HARD_REG_BIT hard-reg-set.h
static inline void  mtcs_reg_set_hard_reg_bit(MtcsReg *self,HardRegSet *set, unsigned int bit)
{
    if(set->count==1){
        set->elts[0]|= MTCS_HARD_CONST (1) << bit;
    }else{
        set->elts[bit / MTCS_UHOST_BITS_PER_WIDE_INT]|= MTCS_HARD_CONST (1) << (bit % MTCS_UHOST_BITS_PER_WIDE_INT);
    }
}

//原型 CLEAR_HARD_REG_BIT hard-reg-set.h
static inline void  mtcs_reg_clear_hard_reg_bit(MtcsReg *self,HardRegSet *set, unsigned int bit)
{
    if(set->count==1){
        set->elts[0]&= ~(MTCS_HARD_CONST (1) << bit);
    }else{
        set->elts[bit / MTCS_UHOST_BITS_PER_WIDE_INT]&= ~(MTCS_HARD_CONST (1) << (bit % MTCS_UHOST_BITS_PER_WIDE_INT));
    }
}

//原型 #define TEST_HARD_REG_BIT(SET, BIT)   (!!((SET) & (HARD_CONST (1) << (BIT)))) hard-reg-set.h
static inline bool  mtcs_reg_test_hard_reg_bit(HardRegSet *set, unsigned int bit)
{
    if(set->count==1){
        return (!!(set->elts[0]&(MTCS_HARD_CONST (1) << bit)));
    }else{
        return (set->elts[bit / MTCS_UHOST_BITS_PER_WIDE_INT]
             & (MTCS_HARD_CONST (1) << (bit % MTCS_UHOST_BITS_PER_WIDE_INT)));
    }
}

//原型 CLEAR_HARD_REG_SET hard-reg-set.h
static inline void  mtcs_reg_clear_hard_reg_set(HardRegSet *set)
{
    if(set->count==1){
        set->elts[0]=MTCS_HARD_CONST (0);
    }else{
        int i;
        for(i=0;i<set->count;i++){
            set->elts[i]=0;
        }
    }
}

//原型 SET_HARD_REG_SET
static inline void  mtcs_reg_set_hard_reg_set(HardRegSet *set)
{

    if(set->count==1){
        set->elts[0]=~MTCS_HARD_CONST (0);
    }else{
        int i;
        for(i=0;i<set->count;i++){
            set->elts[i]=-1;
        }
    }
}
//原型 hard_reg_set_subset_p hard-reg-set.h
inline bool mtcs_reg_hard_reg_set_subset_p (HardRegSet *x, HardRegSet *y)
{
    if(x->count==1)
      return (x->elts[0] & ~(y->elts[0])) == HARD_CONST (0);

    HARD_REG_ELT_TYPE bad = 0;
    for (unsigned int i = 0; i < x->count; ++i)
      bad |= (x->elts[i] & ~y->elts[i]);
    return bad == 0;
}
//原型 hard_reg_set_intersect_p hard-reg-set.h
inline bool mtcs_reg_hard_reg_set_intersect_p (HardRegSet *x, HardRegSet *y)
{
    if(x->count==1)
        return (x->elts[0] & y->elts[0]) != HARD_CONST (0);
    HARD_REG_ELT_TYPE good = 0;
    for (unsigned int i = 0; i < x->count; ++i)
      good |= (x->elts[i] & y->elts[i]);
    return good != 0;
}

//原型 hard_reg_set_empty_p
inline bool mtcs_reg_hard_reg_set_empty_p (HardRegSet *x)
{
    if(x->count==1)
        return x->elts[0] == HARD_CONST (0);
    HARD_REG_ELT_TYPE bad = 0;
    for (unsigned int i = 0; i < x->count; ++i)
      bad |= x->elts[i];
    return bad == 0;
}

//原型 VIRTUAL_STACK_DYNAMIC_REGNUM rtl.h
inline int mtcs_reg_get_virtual_stack_dynamic_regnum(MtcsReg *self)
{
  return self->normalHardRegsNum.virtual_stack_dynamic_regnum ;
}
//原型 STACK_POINTER_REGNUM
inline int mtcs_reg_get_stack_pointer_regnum(MtcsReg *self)
{
   return self->normalHardRegsNum.stack_pointer_regnum;
}

/* Add to REGS all the registers required to store a value of mode MODE
   in register REGNO.  */
//原型 add_to_hard_reg_set regs.h
inline void mtcs_reg_add_to_hard_reg_set (MtcsReg *self,HardRegSet *regs, machine_mode mode,unsigned int regno)
{
  unsigned int end_regno;
  end_regno = mtcs_reg_end_hard_regno/*!end_hard_regno*/(self,mode, regno);
  do
      mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(self,regs, regno);
  while (++regno < end_regno);
}

/* Likewise, but remove the registers.  */
//原型 remove_from_hard_reg_set regs.h
inline void mtcs_reg_remove_from_hard_reg_set (MtcsReg *self,HardRegSet *regs, machine_mode mode,unsigned int regno)
{
  unsigned int end_regno;

  end_regno = mtcs_reg_end_hard_regno/*!end_hard_regno*/(self,mode, regno);
  do
      mtcs_reg_clear_hard_reg_bit/*!CLEAR_HARD_REG_BIT*/(self,regs, regno);
  while (++regno < end_regno);
}

/* Return true if (reg:MODE REGNO) includes an element of REGS.  */
//原型 overlaps_hard_reg_set_p regs.h
inline bool mtcs_reg_overlaps_hard_reg_set_p (MtcsReg *self,const HardRegSet *regs, machine_mode mode,
             unsigned int regno)
{
  unsigned int end_regno;
  if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(regs, regno))
    return true;

  end_regno =mtcs_reg_end_hard_regno/*!end_hard_regno*/(self,mode, regno);
  while (++regno < end_regno)
    if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(regs, regno))
      return true;
  return false;
}

//原型 init_reg_class_start_regs reginfo.cc  In insn-preds.cc.
void mtcs_reg_init_reg_class_start_regs(MtcsReg *self);
//原型 HARD_REG_SET ARRAY_SIZE (elts)
int mtcs_reg_get_hard_reg_element_count(MtcsReg *self);

//原型 HARD_FRAME_POINTER_IS_FRAME_POINTER rtl.h
nboolean mtcs_reg_hard_frame_pointer_is_frame_pointer(MtcsReg *self);
void     mtcs_reg_set_hard_frame_pointer_is_frame_pointer(MtcsReg *self,nboolean is);
//原型 PIC_OFFSET_TABLE_REG_CALL_CLOBBERED default.h
int      mtcs_reg_get_pic_offset_table_reg_call_clobbered(MtcsReg *self);
void     mtcs_reg_set_pic_offset_table_reg_call_clobbered(MtcsReg *self,int value);

/* Provide defaults for stuff that may not be defined when using
   sjlj exceptions.  */
//原型 #define EH_RETURN_DATA_REGNO(N) INVALID_REGNUM default.h
int mtcs_reg_get_eh_return_data_regno(MtcsReg *self,int n);
//原型 HARD_FRAME_POINTER_IS_ARG_POINTER rtl.h   (HARD_FRAME_POINTER_REGNUM == ARG_POINTER_REGNUM)
nboolean mtcs_reg_hard_frame_pointer_is_arg_pointer(MtcsReg *self);


//原型 ARG_POINTER_REGNUM 各个平台定义
int mtcs_reg_get_arg_pointer_regnum(MtcsReg *self);
//原型 HARD_FRAME_POINTER_REGNUM 各个平台定义
int mtcs_reg_get_hard_frame_pointer_regnum(MtcsReg *self);
//原型 FRAME_POINTER_REGNUM  各个平台定义
int mtcs_reg_get_frame_pointer_regnum(MtcsReg *self);
//原型 #define PIC_OFFSET_TABLE_REGNUM INVALID_REGNUM defautlts.h
int mtcs_reg_get_pic_offset_table_regnum(MtcsReg *self);
/* Register mappings for target machines without register windows.  */
 //原型 INCOMING_REGNO(N) (N) defaults.h
int mtcs_reg_get_incoming_regno(MtcsReg *self,int regno);

//原型 #define REG_CAN_CHANGE_MODE_P(REGN, FROM, TO)  (targetm.can_change_mode_class (FROM, TO, REGNO_REG_CLASS (REGN)))
bool mtcs_reg_can_change_mode(MtcsReg *self,int regno,machine_mode from ,machine_mode to);

//原型 #define PSEUDO_REGNO_BYTES(N)   GET_MODE_SIZE (PSEUDO_REGNO_MODE (N)) regs.h
unsigned short   mtcs_reg_get_pseudo_regno_bytes(MtcsReg *self,int i);


/**************以下4个方法原型来自文件addresses.h*********************/
//原型 enum reg_class base_reg_class addresses.h
enum reg_class mtcs_reg_base_reg_class (MtcsReg *self,machine_mode mode ATTRIBUTE_UNUSED,
      addr_space_t as ATTRIBUTE_UNUSED, enum rtx_code outer_code ATTRIBUTE_UNUSED,
      enum rtx_code index_code ATTRIBUTE_UNUSED,rtx_insn *insn ATTRIBUTE_UNUSED = NULL);
//原型 enum reg_class index_reg_class addresses.h
enum reg_class mtcs_reg_index_reg_class (MtcsReg *self,rtx_insn *insn ATTRIBUTE_UNUSED);
//原型 bool ok_for_base_p_1 addresses.h
bool mtcs_reg_ok_for_base_p_1 (MtcsReg *self,unsigned regno ATTRIBUTE_UNUSED,
       machine_mode mode ATTRIBUTE_UNUSED,
       addr_space_t as ATTRIBUTE_UNUSED,
       enum rtx_code outer_code ATTRIBUTE_UNUSED,
       enum rtx_code index_code ATTRIBUTE_UNUSED,
       rtx_insn* insn ATTRIBUTE_UNUSED /*!= NULL*/);
//原型 bool regno_ok_for_base_p addresses.h
bool mtcs_reg_regno_ok_for_base_p(MtcsReg *self,unsigned regno, machine_mode mode, addr_space_t as,
           enum rtx_code outer_code, enum rtx_code index_code,rtx_insn *insn /*!= NULL*/);
/*******************addresses.h---------------------------*/

//原型 #define REGNO_OK_FOR_INDEX_P(X) false nvptx.h
bool mtcs_reg_regno_ok_for_index_p(MtcsReg *self,unsigned regno);

//原型 #define WORD_REGISTER_OPERATIONS 0 defaults.h
int mtcs_reg_get_word_register_operations(MtcsReg *self);
void mtcs_reg_set_word_register_operations(MtcsReg *self,int value);
//原型 #define INDEX_REG_CLASS NO_REGS nvptx.h
 int mtcs_reg_get_index_reg_class(MtcsReg *self);
 void mtcs_reg_set_index_reg_class(MtcsReg *self,int value);

/* The implementation of the iterator functions is fully analogous to
   the bitmap iterators.  */
inline void mtcs_hard_reg_set_iter_init (MtcsReg *self,hard_reg_set_iterator *iter, HardRegSet *set,
                        unsigned min, unsigned *regno)
{
   iter->pelt = set->elts;
   iter->length = set->count;
   iter->word_no = min / HARD_REG_ELT_BITS;
   if (iter->word_no < iter->length){
      iter->bits = iter->pelt[iter->word_no];
      iter->bits >>= min % HARD_REG_ELT_BITS;

      /* This is required for correct search of the next bit.  */
      min += !iter->bits;
   }
   *regno = min;
}

inline bool mtcs_hard_reg_set_iter_set (MtcsReg *self,hard_reg_set_iterator *iter, unsigned *regno)
{
   while (1){
      /* Return false when we're advanced past the end of the set.  */
      if (iter->word_no >= iter->length)
         return false;

      if (iter->bits){
         /* Find the correct bit and return it.  */
         while (!(iter->bits & 1)){
            iter->bits >>= 1;
            *regno += 1;
         }
         return (*regno < self->firstPseudoRegNo/*!FIRST_PSEUDO_REGISTER*/);
      }

      /* Round to the beginning of the next word.  */
      *regno = (*regno + HARD_REG_ELT_BITS - 1);
      *regno -= *regno % HARD_REG_ELT_BITS;

      /* Find the next non-zero word.  */
      while (++iter->word_no < iter->length){
         iter->bits = iter->pelt[iter->word_no];
         if (iter->bits)
            break;
         *regno += HARD_REG_ELT_BITS;
      }
   }
}

inline void mtcs_hard_reg_set_iter_next (MtcsReg *self,hard_reg_set_iterator *iter, unsigned *regno)
{
  iter->bits >>= 1;
  *regno += 1;
}

#define MTCS_EXECUTE_IF_SET_IN_HARD_REG_SET(MTCSREG,SET, MIN, REGNUM, ITER)          \
  for (mtcs_hard_reg_set_iter_init (MTCSREG,&(ITER), (SET), (MIN), &(REGNUM));       \
       mtcs_hard_reg_set_iter_set (MTCSREG,&(ITER), &(REGNUM));                      \
       mtcs_hard_reg_set_iter_next (MTCSREG,&(ITER), &(REGNUM)))

/************--------------------以下来自reginfo.cc----------------------**/
//原型 init_reg_sets rtl.h reginfo.cc
void     mtcs_reg_init_reg_sets(MtcsReg *self);
//原型 init_reg_modes_target rtl.h reginfo.cc
void     mtcs_reg_init_reg_modes_target (MtcsReg *self);
//原型 fix_register rtl.h reginfo.cc
void mtcs_reg_fix_register (MtcsReg *self,const char *name, int fixed, int call_used);
//原型 reginfo_cc_finalize rtl.h reginfo.cc
void mtcs_reg_reginfo_cc_finalize (MtcsReg *self);
//原型 reinit_regs rtl.h reginfo.cc
void mtcs_reg_reinit_regs (MtcsReg *self);
//原型 init_regs rtl.h reginfo.cc
void   mtcs_reg_init_regs (MtcsReg *self);
//原型 save_register_info rtl.h reginfo.cc
void mtcs_reg_save_register_info (MtcsReg *self);
//原型 resize_reg_info rtl.h reginfo.cc
bool mtcs_reg_resize_reg_info (MtcsReg *self);
//原型 reg_preferred_class rtl.h reginfo.cc
enum reg_class mtcs_reg_reg_preferred_class (MtcsReg *self,int regno);
//原型 reg_alternate_class rtl.h reginfo.cc
enum reg_class mtcs_reg_reg_alternate_class (MtcsReg *self,int regno);
//原型 reg_allocno_class rtl.h reginfo.cc
enum reg_class mtcs_reg_reg_allocno_class (MtcsReg *self,int regno);

//原型 globalize_reg rtl.h reginfo.cc
void mtcs_reg_globalize_reg (MtcsReg *self,tree decl, int i);
//原型 free_reg_info rtl.h reginfo.cc
void mtcs_reg_free_reg_info (MtcsReg *self);
//原型 choose_hard_reg_mode rtl.h reginfo.cc
machine_mode mtcs_reg_choose_hard_reg_mode (MtcsReg *self,unsigned int regno ATTRIBUTE_UNUSED,
            unsigned int nregs, const mtcs_predefined_function_abi *abi);
//原型 setup_reg_classes rtl.h reginfo.cc
void mtcs_reg_setup_reg_classes (MtcsReg *self,int regno, enum reg_class prefclass,
      enum reg_class altclass,enum reg_class allocnoclass);
//原型 reg_class_subset_p rtl.h reginfo.cc
bool mtcs_reg_reg_class_subset_p (MtcsReg *self,reg_class_t c1, reg_class_t c2);
//原型 reg_classes_intersect_p rtl.h reginfo.cc
bool mtcs_reg_reg_classes_intersect_p (MtcsReg *self,reg_class_t c1, reg_class_t c2);
//原型 simplifiable_subregs rtl.h reginfo.cc
const HardRegSet & mtcs_reg_simplifiable_subregs (MtcsReg *self,const subreg_shape &shape);
//原型 init_subregs_of_mode rtl.h reginfo.cc
void mtcs_reg_init_subregs_of_mode (MtcsReg *self);
//原型 valid_mode_changes_for_regno rtl.h reginfo.cc
const HardRegSet *mtcs_reg_valid_mode_changes_for_regno (MtcsReg *self,unsigned int regno);
//原型 finish_subregs_of_mode rtl.h reginfo.cc
void mtcs_reg_finish_subregs_of_mode (MtcsReg *self);
//原型 void target_hard_regs::finalize () reginfo.cc
void mtcs_reg_free_hard_regs (MtcsReg *self);
//原型 reginfo_init reginfo.cc 用在pass reginfo中
unsigned int mtcs_reg_reginfo_init (MtcsReg *self);

/************************以下是regstat----------------*************/
//原型 regstat_init_n_sets_and_refs regs.h regstat.cc
void mtcs_reg_regstat_init_n_sets_and_refs (MtcsReg *self);


#endif



