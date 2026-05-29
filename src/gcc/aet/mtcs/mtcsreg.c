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
#define INCLUDE_ALGORITHM /* reverse */
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "tree-pass.h"
#include "cfghooks.h"
#include "df.h"
#include "memmodel.h"
#include "tm_p.h"
#include "insn-config.h"
#include "regs.h"
#include "emit-rtl.h"
#include "recog.h"
#include "cgraph.h"
#include "tree-pretty-print.h" /* for dump_function_header */
#include "varasm.h"
#include "insn-attr.h"
#include "conditions.h"
#include "flags.h"
#include "output.h"
#include "except.h"
#include "rtl-error.h"
#include "toplev.h" /* exact_log2, floor_log2 */
#include "reload.h"
#include "intl.h"
#include "cfgrtl.h"
#include "debug.h"
#include "tree-pass.h"
#include "tree-ssa.h"
#include "cfgloop.h"
#include "stringpool.h"
#include "attribs.h"
#include "asan.h"
#include "rtl-iter.h"
#include "print-rtl.h"
#include "function-abi.h"
#include "common/common-target.h"
#include "diagnostic.h"
#include "context.h"
#include "options.h"
#include "expmed.h"
#include "expr.h"
#include "fold-const.h"
#include "dwarf2out.h"
#include "common/common-targhooks.h"
#include "ira.h"
#include "ira-int.h"
#include "poly-int.h"

#include "aet/aetprinttree.h"
#include "mtcsreg.h"
#include "mtcstarget.h"

//当#if FIRST_PSEUDO_REGISTER <= HOST_BITS_PER_WIDEST_FAST_INT时 HARD_REG_SET类型是unsigned long
//否则 unsigned long[] 数组 统一为 unsigned long *

void   mtcs_reg_init(MtcsReg *self)
{
    memset(self->int_reg_class_contents,0,sizeof self->int_reg_class_contents);
    self->hardRegs.x_simplifiable_subregs = new hash_table <mtcs_simplifiable_subregs_hasher> (30);
    self->hardRegsCount=-1;
    self->firstPseudoRegNo=-1;
    self->lastVirtualRegNo=-1;
    self->hardFramePointerIsFramePointer=true;
    self->firstStackReg=0;
    self->lastStackReg=0;
    //原型 #define DWARF_FRAME_REGISTERS FIRST_PSEUDO_REGISTER
    self->dwarfFrameRegisters = -1;
}

//原型 hard_regno_nregs regs.h
unsigned char mtcs_reg_hard_regno_nregs (MtcsReg *self,unsigned int regno, machine_mode mode)
{
  return self->hardRegs.x_hard_regno_nregs[regno][mode];
}

/* Compute the table of register modes.
   These values are used to record death information for individual registers
   (as opposed to a multi-register mode).
   This function might be invoked more than once, if the target has support
   for changing register usage conventions on a per-function basis.
*/
/**
//原型 init_reg_modes_target rtl.h reginfo.cc
 * 先初始化init_reg_sets 再到machine_mode
 */
void mtcs_reg_init_reg_modes_target (MtcsReg *self)
{
  MtcsMode   *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *target=(MtcsTarget *)mtcsMode->target;

  int i, j;
  self->hardRegs.x_hard_regno_max_nregs = 1;
  int maxMachineMode= mtcs_mode_get_max_number(mtcsMode);
  //printf("FIRST_PSEUDO_REGISTER <= HOST_BITS_PER_WIDEST_FAST_INT --- %d %d\n",
         // FIRST_PSEUDO_REGISTER,HOST_BITS_PER_WIDEST_FAST_INT);
  for (i = 0; i < self->hardRegsCount/*FIRST_PSEUDO_REGISTER*/; i++)
    for (j = 0; j < maxMachineMode; j++){
        unsigned char nregs = target->hard_regno_nregs/*targetm.hard_regno_nregs*/ (target,i, (machine_mode) j);
        self->hardRegs.x_hard_regno_nregs[i][j] = nregs;
        if (nregs > self->hardRegs.x_hard_regno_max_nregs)
            self->hardRegs.x_hard_regno_max_nregs = nregs;
    }
  for (i = 0; i < self->hardRegsCount/*FIRST_PSEUDO_REGISTER*/; i++){
      self->hardRegs.x_reg_raw_mode/*reg_raw_mode*/[i] = mtcs_reg_choose_hard_reg_mode (self,i, 1, NULL);
      /* If we couldn't find a valid mode, just use the previous mode
        if it is suitable, otherwise fall back on word_mode.  */
      if (self->hardRegs.x_reg_raw_mode[i] == VOIDmode){
          if (i > 0 && mtcs_reg_hard_regno_nregs (self,i, self->hardRegs.x_reg_raw_mode[i - 1]) == 1)
              self->hardRegs.x_reg_raw_mode[i] = self->hardRegs.x_reg_raw_mode[i - 1];
          else
              self->hardRegs.x_reg_raw_mode[i] = word_mode;
      }
  }
}

/**
 *FIRST_PSEUDO_REGISTER
 */
void     mtcs_reg_set_hard_reg_count(MtcsReg *self,int hardRegsCount)
{
    self->hardRegsCount=hardRegsCount;
    self->firstPseudoRegNo=hardRegsCount;
    self->lastVirtualRegNo=self->firstPseudoRegNo+5;
}

int  mtcs_reg_get_last_virtual_regno(MtcsReg *self)
{
    return self->lastVirtualRegNo;
}


void  mtcs_reg_copy_reg_class_contents(MtcsReg *self,unsigned int **regClassContents,int dimX,int dimY)
{
     int i,j;
     for(i=0;i<dimX;i++){
         unsigned int *b=(unsigned int *)((char *)regClassContents+dimY*i*sizeof(int));
         for(j=0;j<dimY;j++){
            self->int_reg_class_contents[i][j]=b[j];
         }
     }
}

static void createHardRegs(HardRegSet *hadRegSet,int count)
{
    hadRegSet->count=count;
}

static void initHardReg(MtcsReg *self)
{
   self->hardRegElement=1;
   if(self->hardRegsCount<=HOST_BITS_PER_WIDEST_FAST_INT){
       self->hardRegElement=1;
   }else{
       self->hardRegElement=  (self->hardRegsCount + HOST_BITS_PER_WIDEST_FAST_INT - 1)/ HOST_BITS_PER_WIDEST_FAST_INT;
   }
   n_debug("mtcsreg.c initHardReg hardRegElement:%d sizeof(MtcsReg):%d sizeof(MtcsHardReg):%d MAX_REG_CLASSES:%d\n",
           self->hardRegElement,sizeof(MtcsReg),sizeof(MtcsHardReg),MAX_REG_CLASSES);
   createHardRegs(&self->hardRegs.x_accessible_reg_set,self->hardRegElement);
   createHardRegs(&self->hardRegs.x_operand_reg_set,self->hardRegElement);
   createHardRegs(&self->hardRegs.x_fixed_reg_set,self->hardRegElement);
   createHardRegs(&self->hardRegs.x_savable_regs,self->hardRegElement);
   createHardRegs(&self->hardRegs.x_fixed_nonglobal_reg_set,self->hardRegElement);
   createHardRegs(&self->hardRegs.x_regs_invalidated_by_call,self->hardRegElement);

   int i;
   for(i=0;i<MAX_REG_CLASSES;i++)
       createHardRegs(&self->hardRegs.x_reg_class_contents[i],self->hardRegElement);//=(unsigned long *)xmalloc(sizeof(long)*self->hardRegElement);
}

/**
 * 先初始化init_reg_sets 再到machine_mode
 * 原型 init_reg_sets rtl.h reginfo.cc
 */
void    mtcs_reg_init_reg_sets(MtcsReg *self)
{
    char *fixedReg=self->get_fixed_registers(self);
    char *callUsedRegs=self->get_call_used_registers(self);
    char **regNames=self->get_register_names(self);

    int i, j;
    n_debug("mtcsreg.c mtcs_reg_init_reg_sets  00 FIRST_PSEUDO_REGISTER <= HOST_BITS_PER_WIDEST_FAST_INT --- 主机:%d %d\n",
            FIRST_PSEUDO_REGISTER,HOST_BITS_PER_WIDEST_FAST_INT);
    initHardReg(self);
       /* First copy the register information from the initial int form into
       the regsets.  */
    n_debug("mtcsreg.c mtcs_reg_init_reg_sets 22 mtcsRegClassCount N_REG_CLASSES:%d hardRegsCount:%d\n",
          self->mtcsRegClassCount,self->hardRegsCount);
    for (i = 0; i < self->mtcsRegClassCount/*N_REG_CLASSES*/; i++){
        MtcsRegClass regCl=self->mtcsRegClass[i];
        //hard-reg-set.h #if FIRST_PSEUDO_REGISTER <= HOST_BITS_PER_WIDEST_FAST_INT
        mtcs_reg_clear_hard_reg_set/*CLEAR_HARD_REG_SET*/(&self->hardRegs.x_reg_class_contents[i]/*reg_class_contents[i]*/);
        /* Note that we hard-code 32 here, not HOST_BITS_PER_INT.  */
        for (j = 0; j < self->hardRegsCount; j++)
          if (regCl.regInts[j/32]/*!self->int_reg_class_contents[i][j / 32]*/ & ((unsigned) 1 << (j % 32)))
              mtcs_reg_set_hard_reg_bit(self,&self->hardRegs.x_reg_class_contents[i]/*reg_class_contents[i]*/, j);
    }

    /* Sanity check: make sure the target macros FIXED_REGISTERS and
       CALL_USED_REGISTERS had the right number of initializers.  */

    memcpy (self->hardRegs.x_fixed_regs, fixedReg, self->hardRegsCount);
    memcpy (self->hardRegs.x_call_used_regs, callUsedRegs, self->hardRegsCount);
    for(i=0;i<self->hardRegsCount;i++){
        n_debug("mtcsreg.c mtcs_reg_init_reg_sets 硬件寄存器名:%d %s\n",i,regNames[i]);
        self->hardRegs.x_reg_names[i]=n_strdup(regNames[i]);
    }
    mtcs_reg_set_hard_reg_set (&self->hardRegs.x_accessible_reg_set);
    mtcs_reg_set_hard_reg_set (&self->hardRegs.x_operand_reg_set);
    n_debug("mtcsreg.c mtcs_reg_init_reg_sets  33 完成初始化 end\n");
    //原型 init_reg_class_start_regs reginfo.cc  In insn-preds.cc.
    mtcs_reg_init_reg_class_start_regs(self);
}

/**
 * 来自mtcsptxreg
 * static char *registerNames[16]={
            "%value", "%stack", "%frame", "%args",
            "%chain", "%sspslot", "%sspprev", "%hr7",
            "%hr8", "%hr9", "%hr10", "%hr11",
            "%hr12", "%hr13", "%hr14", "%hr15"
    };
 */
const char *mtcs_reg_get_reg_name(MtcsReg *self,int index)
{
    if(index<0 || index>=self->hardRegsCount){
        n_error("mtcs_reg_get_reg_name index:%d hardRegsCount:%d\n",index,self->hardRegsCount);
    }
    return  self->hardRegs.x_reg_names[index];
}

/**
 * 为10个寄存器设置号码
 */
void     mtcs_reg_set_stack_pointer_to_virtual_regnum(MtcsReg *self,nuint *regNums)
{
    self->normalHardRegsNum.stack_pointer_regnum=regNums[0];
    self->normalHardRegsNum.frame_pointer_regnum=regNums[1];
    self->normalHardRegsNum.hard_frame_pointer_regnum=regNums[2];
    self->normalHardRegsNum.arg_pointer_regnum=regNums[3];
    self->normalHardRegsNum.virtual_incoming_args_regnum=regNums[4];
    self->normalHardRegsNum.virtual_stack_vars_regnum=regNums[5];
    self->normalHardRegsNum.virtual_stack_dynamic_regnum=regNums[6];
    self->normalHardRegsNum.virtual_outgoing_args_regnum=regNums[7];
    self->normalHardRegsNum.virtual_cfa_regnum=regNums[8];
    self->normalHardRegsNum.virtual_preferred_stack_boundary_regnum=regNums[9];
}

void     mtcs_reg_set_return_address_pointer_regnum(MtcsReg *self,int regno)
{
    self->return_address_pointer_regnum=regno;
}

//原型 PIC_OFFSET_TABLE_REGNUM defaults.h
void     mtcs_reg_set_pic_offset_table_regnum(MtcsReg *self,int regno)
{
    self->pic_offset_table_regnum=regno;
}

/**
 * regNum小于硬寄存器数量则是硬寄存器
 * 原型 #define HARD_REGISTER_NUM_P(REG_NO) ((REG_NO) < FIRST_PSEUDO_REGISTER) rtl.h
 */
nboolean mtcs_reg_is_hard(MtcsReg *self,nuint regNo)
{
    return regNo<self->hardRegsCount;
}
/* 1 if the given register REG corresponds to a hard register.  */
//原型 #define HARD_REGISTER_P(REG) HARD_REGISTER_NUM_P (REGNO (REG)) rtl.h
nboolean mtcs_reg_is_hard_rtx(MtcsReg *self,rtx x)
{
    return mtcs_reg_is_hard(self,REGNO(x));
}

/**
 * virsutal 寄存器只有一个
 * 参考
 *  ALWAYS_INLINE unsigned char
       hard_regno_nregs (unsigned int regno, machine_mode mode)
       {
         return this_target_regs->x_hard_regno_nregs[regno][mode];
       }
 */
nuint mtcs_reg_get_reg_nums(MtcsReg *self, machine_mode mode, nuint regno)
{
   if(mtcs_reg_is_hard(self,regno))
      return self->hardRegs.x_hard_regno_nregs[regno][mode];
   return 1;
}

nuint    mtcs_reg_get_hard_reg_count(MtcsReg *self)
{
    return self->hardRegsCount;
}

int      mtcs_reg_get_first_pseudo_register(MtcsReg *self)
{
    return self->firstPseudoRegNo;
}

//原型 #define FIRST_VIRTUAL_REGISTER   (FIRST_PSEUDO_REGISTER)
int      mtcs_reg_get_first_virtual_register(MtcsReg *self)
{
   return self->firstPseudoRegNo;
}

//原型 REGNO_REG_CLASS
int mtcs_reg_get_class(MtcsReg *self,int regno)
{
    return self->get_class(self,regno);
}

//原型 N_REG_CLASSES
int mtcs_reg_get_n_reg_classes(MtcsReg *self)
{
   return self->mtcsRegClassCount;
}

//原型 LIM_REG_CLASSES
int mtcs_reg_get_lim_reg_classes(MtcsReg *self)
{
   return self->mtcsRegClassCount;
}

/* Return an exclusive upper bound on the registers occupied by hard
   register (reg:MODE REGNO).  */
//原型  end_hard_regno regs.h
unsigned int mtcs_reg_end_hard_regno (MtcsReg *self,mtcs_mode mode, unsigned int regno)
{
  return regno + mtcs_reg_hard_regno_nregs (self,regno, (machine_mode)mode);
}

/* Return true if REGS contains the whole of (reg:MODE REGNO).  */
//原型 inline bool in_hard_reg_set_p (const_hard_reg_set regs, machine_mode mode,unsigned int regno)
bool mtcs_reg_in_hard_reg_set_p (MtcsReg *self,HardRegSet *regs, mtcs_mode mode,unsigned int regno)
{
  unsigned int end_regno;
  gcc_assert (regno<self->firstPseudoRegNo/*HARD_REGISTER_NUM_P (regno)*/);
  if (!mtcs_reg_test_hard_reg_bit/*TEST_HARD_REG_BIT*/ (regs, regno))
    return false;
  end_regno = mtcs_reg_end_hard_regno (self,mode, regno);
  if (!mtcs_reg_is_hard(self,end_regno - 1)/*HARD_REGISTER_NUM_P (end_regno - 1)*/)
    return false;
  while (++regno < end_regno)
    if (!mtcs_reg_test_hard_reg_bit/*TEST_HARD_REG_BIT*/ (regs, regno))
      return false;

  return true;
}

//原型 REGNO_PTR_FRAME_P rtl.h
 nboolean mtcs_reg_regnum_ptr_frame_p(MtcsReg *self,int regnum)
{
   return self->regnum_ptr_frame_p(self,regnum);
}

 //原型#define VIRTUAL_REGISTER_NUM_P(REG_NO)   IN_RANGE (REG_NO, FIRST_VIRTUAL_REGISTER, LAST_VIRTUAL_REGISTER)
 //rtl.h
 nboolean mtcs_reg_virtual_register_num_p(MtcsReg *self,int regNo)
 {
     return IN_RANGE (regNo, self->firstPseudoRegNo, self->lastVirtualRegNo/*!LAST_VIRTUAL_REGISTER*/);
 }

 //原型 #define VIRTUAL_REGISTER_P(REG) VIRTUAL_REGISTER_NUM_P (REGNO (REG))
 nboolean mtcs_reg_virtual_register_p(MtcsReg *self,rtx x)
 {
   return mtcs_reg_virtual_register_num_p(self,REGNO(x));
 }

//原型 #define direct_load (this_target_regs->x_direct_load) regs.h
char mtcs_reg_get_direct_load(MtcsReg *self,int index)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    int numberModes=mtcs_mode_get_number(mtcsMode);
    gcc_assert (index>=0 && index<numberModes);
    return self->hardRegs.x_direct_load[index];
}
//原型 #define direct_load (this_target_regs->x_direct_load) rtl.h
void mtcs_reg_set_direct_load(MtcsReg *self,int index,int value)
{
    MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
    int numberModes=mtcs_mode_get_number(mtcsMode);
    gcc_assert (index>=0 && index<numberModes);
    self->hardRegs.x_direct_load[index]=value;
}

void mtcs_reg_set_reg_class(MtcsReg *self,MtcsRegClass *mtcsRegClass,int count)
{
    self->mtcsRegClass=mtcsRegClass;
    self->mtcsRegClassCount=count;
}

MtcsRegClass *mtcs_reg_get_reg_class(MtcsReg *self)
{
   return self->mtcsRegClass;
}

//原型 GENERAL_REGS nvptx.h
void mtcs_reg_set_general_regs(MtcsReg *self,nuint generalRegs)
{
    self->generalRegs=generalRegs;
}
//原型 GENERAL_REGS nvptx.h
nuint mtcs_reg_get_general_regs(MtcsReg *self)
{
    return self->generalRegs;
}

//原型 ALL_REGS nvptx.h
void mtcs_reg_set_all_regs(MtcsReg *self,nuint allRegs)
{
    self->allRegs=allRegs;
}
//原型 ALL_REGS nvptx.h
nuint mtcs_reg_get_all_regs(MtcsReg *self)
{
    return self->allRegs;
}

//原型 MOVE_MAX nvptx.h =8 host=64
void mtcs_reg_set_move_max(MtcsReg *self,nuint value)
{
  self->move_max=value;
}

nuint mtcs_reg_get_move_max(MtcsReg *self)
{
    return self->move_max;
}

//原型 STORE_MAX_PIECES default.h MIN (MOVE_MAX_PIECES, 2 * sizeof (HOST_WIDE_INT))
void mtcs_reg_set_store_max_pieces(MtcsReg *self,nuint value)
{
    self->store_max_pieces=value;
}

nuint mtcs_reg_get_store_max_pieces(MtcsReg *self)
{
    return self->store_max_pieces;
}

//原型 MAX_MOVE_MAX default.h
void mtcs_reg_set_max_move_max(MtcsReg *self,nuint value)
{
    self->max_move_max=value;
}

nuint mtcs_reg_get_max_move_max(MtcsReg *self)
{
    return self->max_move_max;
}
//原型 MOVE_MAX_PIECES default.h
void mtcs_reg_set_move_max_pieces(MtcsReg *self,nuint value)
{
    self->move_max_pieces=value;
}

nuint mtcs_reg_get_move_max_pieces(MtcsReg *self)
{
    return self->move_max_pieces;
}

//原型 COMPARE_MAX_PIECES default.h
void mtcs_reg_set_compare_max_pieces(MtcsReg *self,nuint value)
{
    self->comapre_max_pieces=value;
}

nuint mtcs_reg_get_compare_max_pieces(MtcsReg *self)
{
    return self->comapre_max_pieces;
}

//原型 FIRST_STACK_REG i386.h nvptx没定义
void mtcs_reg_set_first_stack_reg(MtcsReg *self,nuint value)
{
   self->firstStackReg=value;
}

int mtcs_reg_get_first_stack_reg(MtcsReg *self)
{
   return self->firstStackReg;
}

//原型 LAST_STACK_REG i386.h nvptx没定义
void mtcs_reg_set_last_stack_reg(MtcsReg *self,nuint value)
{
   self->lastStackReg=value;
}

int mtcs_reg_get_last_stack_reg(MtcsReg *self)
{
   return self->lastStackReg;
}

//原型 #define DEBUGGER_REGNO(N) N
int mtcs_reg_get_debugger_regno(MtcsReg *self,int regno)
{
   return self->get_debugger_regno(self,regno);
}

//原型 #define DWARF_FRAME_REGNUM(REG) DEBUGGER_REGNO (REG)
int mtcs_reg_get_dwarf_frame_regnum(MtcsReg *self,int regno)
{
   return self->get_dwarf_frame_regnum(self,regno);
}

//原型 #define DWARF2_FRAME_REG_OUT(REGNO, FOR_EH) (REGNO)
int mtcs_reg_get_dwarf2_frame_reg_out(MtcsReg *self,int regno,int forEh)
{
   if(self->get_dwarf2_frame_reg_out)
      return self->get_dwarf2_frame_reg_out(self,regno,forEh);
   return regno;//#define DWARF2_FRAME_REG_OUT(REGNO, FOR_EH) (REGNO) defaults.h
}

//原型 #define DWARF_REG_TO_UNWIND_COLUMN(REGNO) (REGNO)
int mtcs_reg_get_dwarf_reg_to_unwind_column(MtcsReg *self,int regno)
{
   if(self->get_dwarf_reg_to_unwind_column)
      return self->get_dwarf_reg_to_unwind_column(self,regno);
   return regno;//#define DWARF_REG_TO_UNWIND_COLUMN(REGNO) (REGNO) defaults.h
}

//原型 #define DWARF_FRAME_REGISTERS FIRST_PSEUDO_REGISTER
void mtcs_reg_set_dwarf_frame_registers(MtcsReg *self,int value)
{
   self->dwarfFrameRegisters = value;
}

int  mtcs_reg_get_dwarf_frame_registers(MtcsReg *self)
{
   return self->dwarfFrameRegisters;
}

//原型 #define DWARF_FRAME_RETURN_COLUMN   DWARF_FRAME_REGISTERS
int mtcs_reg_get_dwarf_frame_return_column(MtcsReg *self)
{
   MtcsMode   *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConfig *mtcsConfig = mtcs_target_get_config(mtcsTarget);
   if(self->get_dwarf_frame_return_column)
      return self->get_dwarf_frame_return_column(self);
   if(mtcs_config_ifdef(mtcsConfig,MTCS_PC_REGNUM))
      return  mtcs_reg_get_dwarf_frame_regnum(self,mtcs_config_get_value(mtcsConfig,MTCS_PC_REGNUM));
   else
      return mtcs_reg_get_dwarf_frame_registers(self);
   /*!
#ifdef PC_REGNUM
#define DWARF_FRAME_RETURN_COLUMN   DWARF_FRAME_REGNUM (PC_REGNUM)
#else
#define DWARF_FRAME_RETURN_COLUMN   DWARF_FRAME_REGISTERS
#endif
*/
}

/* For hooks which use the MOVE_RATIO macro, this gives the legacy default
   behavior.  SPEED_P is true if we are compiling for speed.  */
//原型 get_move_ratio target.h targhooks.cc
unsigned int mtcs_reg_get_move_ratio (MtcsReg *self,bool speed_p ATTRIBUTE_UNUSED)
{
  unsigned int move_ratio;
#ifdef MOVE_RATIO
  move_ratio = (unsigned int) MOVE_RATIO (speed_p);
#else
#if defined (HAVE_cpymemqi) || defined (HAVE_cpymemhi) || defined (HAVE_cpymemsi) || defined (HAVE_cpymemdi) || defined (HAVE_cpymemti)
  move_ratio = 2;
#else /* No cpymem patterns, pick a default.  */
  move_ratio = ((speed_p) ? 15 : 3);
#endif
#endif
  return move_ratio;
}

/* Save the register information.  */
//原型 save_register_info rtl.h reginfo.cc
void mtcs_reg_save_register_info (MtcsReg *self)
{
  /* Sanity check:  make sure the target macros FIXED_REGISTERS and
     CALL_USED_REGISTERS had the right number of initializers.  */
  memcpy (self->saveRegs.saved_fixed_regs, self->hardRegs.x_fixed_regs, sizeof self->hardRegs.x_fixed_regs);
  memcpy (self->saveRegs.saved_call_used_regs, self->hardRegs.x_call_used_regs, sizeof self->hardRegs.x_call_used_regs);

  /* And similarly for reg_names.  */
  int i;
  for(i=0;i<self->hardRegsCount;i++)
      self->saveRegs.saved_reg_names[i]= n_strdup(self->hardRegs.x_reg_names[i]);
  self->saveRegs.saved_accessible_reg_set = self->hardRegs.x_accessible_reg_set;
  self->saveRegs.saved_operand_reg_set = self->hardRegs.x_operand_reg_set;
}

//原型 init_reg_class_start_regs reginfo.cc  In insn-preds.cc.
void mtcs_reg_init_reg_class_start_regs(MtcsReg *self)
{
    if(self->init_reg_class_start_regs)
        self->init_reg_class_start_regs(self);
}

//原型 HARD_REG_SET ARRAY_SIZE (elts)
int mtcs_reg_get_hard_reg_element_count(MtcsReg *self)
{
    return self->hardRegElement;
}

/* Restore the register information.  */
//原型 restore_register_info reginfo.cc
static void restore_register_info (MtcsReg *self)
{
  memcpy (self->hardRegs.x_fixed_regs, self->saveRegs.saved_fixed_regs, sizeof self->hardRegs.x_fixed_regs);
  memcpy (self->hardRegs.x_call_used_regs, self->saveRegs.saved_call_used_regs, sizeof self->hardRegs.x_call_used_regs);

  int i;
  for(i=0;i<self->hardRegsCount;i++)
      self->hardRegs.x_reg_names[i] = n_strdup(self->saveRegs.saved_reg_names[i]);
  self->hardRegs.x_accessible_reg_set=self->saveRegs.saved_accessible_reg_set;
  self->hardRegs.x_operand_reg_set=self->saveRegs.saved_operand_reg_set;
}

#define MTCS_NO_REGS 0
#define MTCS_LIM_REG_CLASSES (self->mtcsRegClassCount)

/* After switches have been processed, which perhaps alter
   `fixed_regs' and `call_used_regs', convert them to HARD_REG_SETs.  */
//原型 init_reg_sets_1 reginfo.cc
static void init_reg_sets_1 (MtcsReg *self)
{
  MtcsMode   *mtcsMode=MTCS_GET_MODE_OBJECT(self);
  MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
  MtcsFuncAbi *mtcsFuncAbi=mtcs_target_get_func_abi(mtcsTarget);

  unsigned int i, j;
  unsigned int /* machine_mode */ m;

  restore_register_info(self);

//#ifdef REG_ALLOC_ORDER host=1 nvptx=0
//  for (i = 0; i < FIRST_PSEUDO_REGISTER; i++)
//    inv_reg_alloc_order[reg_alloc_order[i]] = i;
//#endif

  /* Let the target tweak things if necessary.  */

  mtcsTarget/*!targetm.conditional_register_usage*/->conditional_register_usage(mtcsTarget);
  //原型 reg_class_contents hard-reg-set.h 宏 self->hardRegs.x_reg_class_contents
  //原型 reg_class_size hard-reg-set.h 宏 self->hardRegs.x_reg_class_size
  //原型 fixed_regs  hard-reg-set.h 宏  self->hardRegs.x_fixed_regs
  //原型 class_only_fixed_regs hard-reg-set.h 宏  self->hardRegs.x_class_only_fixed_regs
  //原型 reg_class_subunion hard-reg-set.h 宏  self->hardRegs.x_reg_class_subunion
  /* Compute number of hard regs in each class.  */
  memset(self->hardRegs.x_reg_class_size,0,sizeof self->hardRegs.x_reg_class_size);/*!memset (reg_class_size, 0, sizeof reg_class_size);*/
  for (i = 0; i <self->mtcsRegClassCount/*!N_REG_CLASSES*/; i++){
      bool any_nonfixed = false;
      for (j = 0; j < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(self); j++)
          if (mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&self->hardRegs.x_reg_class_contents[i]/*!reg_class_contents[i]*/, j)){
            self->hardRegs.x_reg_class_size[i]++;/*!reg_class_size[i]++;*/
            if (!self->hardRegs.x_fixed_regs[j]/*!fixed_regs[j]*/)
              any_nonfixed = true;
          }
      self->hardRegs.x_class_only_fixed_regs/*!class_only_fixed_regs*/[i] = !any_nonfixed;
  }


  /* Initialize the table of subunions.
     reg_class_subunion[I][J] gets the largest-numbered reg-class
     that is contained in the union of classes I and J.  */

  memset (self->hardRegs.x_reg_class_subunion, 0, sizeof self->hardRegs.x_reg_class_subunion);
  /*!memset (reg_class_subunion, 0, sizeof reg_class_subunion);*/
  for (i = 0; i < self->mtcsRegClassCount/*!N_REG_CLASSES*/; i++){
      for (j = 0; j < self->mtcsRegClassCount/*!N_REG_CLASSES*/; j++){
          /*!HARD_REG_SET c;*/
          HardRegSet c={self->hardRegElement};
          int k;
          c = self->hardRegs.x_reg_class_contents[i] | self->hardRegs.x_reg_class_contents[j];
          for (k = 0; k < self->mtcsRegClassCount/*!N_REG_CLASSES*/; k++)
             if (mtcs_reg_hard_reg_set_subset_p (&self->hardRegs.x_reg_class_contents[k], &c)
                && !mtcs_reg_hard_reg_set_subset_p(&self->hardRegs.x_reg_class_contents[k],
                        &self->hardRegs.x_reg_class_contents[(int) self->hardRegs.x_reg_class_subunion[i][j]]))
                self->hardRegs.x_reg_class_subunion[i][j] = (mtcs_reg_class) k;
      }
  }

  /* Initialize the table of superunions.
     reg_class_superunion[I][J] gets the smallest-numbered reg-class
     containing the union of classes I and J.  */

  memset (self->hardRegs.x_reg_class_superunion, 0, sizeof self->hardRegs.x_reg_class_superunion);
  for (i = 0; i < self->mtcsRegClassCount/*!N_REG_CLASSES*/; i++){
      for (j = 0; j < self->mtcsRegClassCount/*!N_REG_CLASSES*/; j++){
          /*!HARD_REG_SET c;*/
          HardRegSet c={self->hardRegElement};
          int k;

          c = self->hardRegs.x_reg_class_contents[i] | self->hardRegs.x_reg_class_contents[j];
          for (k = 0; k < self->mtcsRegClassCount/*!N_REG_CLASSES*/; k++)
            if (mtcs_reg_hard_reg_set_subset_p(&c, &self->hardRegs.x_reg_class_contents[k]))
              break;

          self->hardRegs.x_reg_class_superunion[i][j] = (mtcs_reg_class) k;
      }
  }

  /* Initialize the tables of subclasses and superclasses of each reg class.
     First clear the whole table, then add the elements as they are found.  */

  for (i = 0; i < self->mtcsRegClassCount/*!N_REG_CLASSES*/; i++){
      for (j = 0; j < self->mtcsRegClassCount/*!N_REG_CLASSES*/; j++)
          self->hardRegs.x_reg_class_subclasses[i][j] =MTCS_LIM_REG_CLASSES;/*!LIM_REG_CLASSES;*/
  }

  for (i = 0; i < self->mtcsRegClassCount/*!N_REG_CLASSES*/; i++){
      if (i == (int) MTCS_NO_REGS/*!NO_REGS*/)
          continue;

      for (j = i + 1; j < self->mtcsRegClassCount/*!N_REG_CLASSES*/; j++)
          if (mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/(&self->hardRegs.x_reg_class_contents[i],
                &self->hardRegs.x_reg_class_contents[j])){
            /* Reg class I is a subclass of J.
               Add J to the table of superclasses of I.  */
            mtcs_reg_class /*!enum reg_class*/ *p;

            /* Add I to the table of superclasses of J.  */
            p = &self->hardRegs.x_reg_class_subclasses[j][0];
            while (*p != MTCS_LIM_REG_CLASSES/*!LIM_REG_CLASSES*/) p++;
            *p = (mtcs_reg_class /*!enum reg_class*/) i;
          }
  }

  /* Initialize "constant" tables.  */

  mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&self->hardRegs.x_fixed_reg_set);
  mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&self->hardRegs.x_regs_invalidated_by_call);
  n_debug("mtcsreg.c init_reg_sets_1 00 %ld\n",self->hardRegs.x_regs_invalidated_by_call.elts[0]);

  self->hardRegs.x_operand_reg_set/*!operand_reg_set*/ &= self->hardRegs.x_accessible_reg_set/*!accessible_reg_set*/;
  n_debug("mtcsreg.c init_reg_sets_1 11 %ld\n",self->hardRegs.x_regs_invalidated_by_call);

  int maxMachineMode=mtcs_mode_get_max_number(mtcsMode);//原型   MAX_MACHINE_MODE
  int firstPseudoRegister=mtcs_reg_get_first_pseudo_register(self);//原型 FIRST_PSEUDO_REGISTER
  n_debug("mtcsreg.c init_reg_sets_1 22 %d\n",firstPseudoRegister);
  n_debug("mtcsreg.c init_reg_sets_1 33 %ld firstPseudoRegister:%d\n",
          self->hardRegs.x_regs_invalidated_by_call.elts[0],firstPseudoRegister);

  for (i = 0; i < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; i++){
      /* As a special exception, registers whose class is NO_REGS are
     not accepted by `register_operand'.  The reason for this change
     is to allow the representation of special architecture artifacts
     (such as a condition code register) without extending the rtl
     definitions.  Since registers of class NO_REGS cannot be used
     as registers in any case where register classes are examined,
     it is better to apply this exception in a target-independent way.  */
      if (mtcs_reg_get_class/*!REGNO_REG_CLASS*/(self,i) ==MTCS_NO_REGS)
          mtcs_reg_clear_hard_reg_bit/*!CLEAR_HARD_REG_BIT*/(self,&self->hardRegs.x_operand_reg_set, i);

      /* If a register is too limited to be treated as a register operand,
     then it should never be allocated to a pseudo.  */
      if (!mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&self->hardRegs.x_operand_reg_set, i))
         self->hardRegs.x_fixed_regs/*!fixed_regs*/[i] = 1;

      if (self->hardRegs.x_fixed_regs/*!fixed_regs*/[i])
          mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(self,&self->hardRegs.x_fixed_reg_set/*!fixed_reg_set*/, i);

      /* There are a couple of fixed registers that we know are safe to
     exclude from being clobbered by calls:

     The frame pointer is always preserved across calls.  The arg
     pointer is if it is fixed.  The stack pointer usually is,
     unless TARGET_RETURN_POPS_ARGS, in which case an explicit
     CLOBBER will be present.  If we are generating PIC code, the
     PIC offset table register is preserved across calls, though the
     target can override that.  */

      if (i == self->normalHardRegsNum.stack_pointer_regnum/*!STACK_POINTER_REGNUM*/)
          ;
      else if (self->global_regs/*!global_regs*/[i]){
          mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(self,&self->hardRegs.x_regs_invalidated_by_call/*!regs_invalidated_by_call*/, i);
          n_debug("mtcsreg.c init_reg_sets_1 44 %d %ld\n",i,self->hardRegs.x_regs_invalidated_by_call.elts[0]);

      }else if (i == self->normalHardRegsNum.frame_pointer_regnum/*!FRAME_POINTER_REGNUM*/)
          ;
      else if (!self->hardFramePointerIsFramePointer/*!HARD_FRAME_POINTER_IS_FRAME_POINTER*/
              && i == self->normalHardRegsNum.hard_frame_pointer_regnum/*!HARD_FRAME_POINTER_REGNUM*/)
          ;
      else if (self->normalHardRegsNum.frame_pointer_regnum/*!FRAME_POINTER_REGNUM*/ !=
              self->normalHardRegsNum.arg_pointer_regnum/*!ARG_POINTER_REGNUM*/  && i ==
             self->normalHardRegsNum.arg_pointer_regnum/*!ARG_POINTER_REGNUM*/ && self->hardRegs.x_fixed_regs/*!fixed_regs*/[i])
          ;
      else if (!self->picOffsetTableRegCallClobbered/*!PIC_OFFSET_TABLE_REG_CALL_CLOBBERED*/
              && i == (unsigned) self->pic_offset_table_regnum/*!PIC_OFFSET_TABLE_REGNUM*/
              && self->hardRegs.x_fixed_regs/*!fixed_regs*/[i])
          ;
      else if (self->hardRegs.x_call_used_regs/*!call_used_regs*/[i]){
          mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(self,&self->hardRegs.x_regs_invalidated_by_call, i);
          n_debug("mtcsreg.c init_reg_sets_1 55 %d %ld\n",i,self->hardRegs.x_regs_invalidated_by_call.elts[0]);
      }
  }

  mtcs_reg_set_hard_reg_set/*!SET_HARD_REG_SET*/(&self->hardRegs.x_savable_regs);
  self->hardRegs.x_fixed_nonglobal_reg_set = self->hardRegs.x_fixed_reg_set;

  /* Preserve global registers if called more than once.  */
  for (i = 0; i < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; i++){
      if (self->global_regs[i]){
          self->hardRegs.x_fixed_regs[i] = self->hardRegs.x_call_used_regs[i] = 1;
          mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(self,&self->hardRegs.x_fixed_reg_set, i);
          mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(self,&self->global_reg_set, i);
      }
  }
  /* Recalculate eh_return_data_regs.  */
  self->hardRegs.x_eh_return_data_regs.count=self->hardRegElement;
  mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&self->hardRegs.x_eh_return_data_regs);
  for (i = 0; ; ++i){
      unsigned int regno = mtcs_reg_get_eh_return_data_regno/*!EH_RETURN_DATA_REGNO*/(self,i);
      if (regno == INVALID_REGNUM)
         break;
      mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(self,&self->hardRegs.x_eh_return_data_regs,regno);
  }

  memset (self->hardRegs.x_have_regs_of_mode/*!have_regs_of_mode*/, 0, sizeof (self->hardRegs.x_have_regs_of_mode));
  memset (self->hardRegs.x_contains_reg_of_mode/*!contains_reg_of_mode*/, 0, sizeof (self->hardRegs.x_contains_reg_of_mode));
  for (m = 0; m < (unsigned int)maxMachineMode/*!MAX_MACHINE_MODE*/; m++){
      HardRegSet ok_regs={self->hardRegElement};
      HardRegSet ok_regs2={self->hardRegElement};

      mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&ok_regs);
      mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&ok_regs2);
      for (j = 0; j < firstPseudoRegister/*!FIRST_PSEUDO_REGISTER*/; j++)
         if (!mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&self->hardRegs.x_fixed_nonglobal_reg_set, j)
            && mtcsTarget/*!targetm.hard_regno_mode_ok*/->hard_regno_mode_ok(mtcsTarget,j, (machine_mode) m)){
            mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&ok_regs, j);
            if (!self->hardRegs.x_fixed_regs[j])
                mtcs_reg_test_hard_reg_bit/*!TEST_HARD_REG_BIT*/(&ok_regs2, j);
         }

      for (i = 0; i < self->mtcsRegClassCount/*N_REG_CLASSES*/; i++)
          if ((mtcsTarget/*!targetm.class_max_nregs*/->class_max_nregs(mtcsTarget,(reg_class_t) i,(machine_mode)m)
             <=self->hardRegs.x_reg_class_size[i])
                && mtcs_reg_hard_reg_set_intersect_p/*!hard_reg_set_intersect_p*/(&ok_regs, &self->hardRegs.x_reg_class_contents[i])){
             self->hardRegs.x_contains_reg_of_mode[i][m] = 1;
             if (mtcs_reg_hard_reg_set_intersect_p/*!hard_reg_set_intersect_p*/(&ok_regs2, &self->hardRegs.x_reg_class_contents[i])){
                 self->hardRegs.x_have_regs_of_mode[m] = 1;
                 self->hardRegs.x_contains_allocatable_reg_of_mode[i][m] = 1;
             }
        }
   }
  mtcs_predefined_function_abi *defaultFuncAbi=mtcs_func_abi_get_default(mtcsFuncAbi);
  n_debug("mtcsreg.c init_reg_sets_1 66 defaultFuncAbi:%p\n",defaultFuncAbi);
  defaultFuncAbi/*!default_function_abi*/->initialize (0,self->hardRegs.x_regs_invalidated_by_call);
}

/* Finish initializing the register sets and initialize the register modes.
This function might be invoked more than once, if the target has support
for changing register usage conventions on a per-function basis.
*/
//原型 init_regs rtl.h reginfo.cc
void   mtcs_reg_init_regs (MtcsReg *self)
{
    /* This finishes what was started by init_reg_sets, but couldn't be done
    until after register usage was specified.  */
    init_reg_sets_1 (self);
}


  //原型 HARD_FRAME_POINTER_IS_FRAME_POINTER rtl.h
nboolean mtcs_reg_hard_frame_pointer_is_frame_pointer(MtcsReg *self)
{
    return self->hardFramePointerIsFramePointer;
}

void     mtcs_reg_set_hard_frame_pointer_is_frame_pointer(MtcsReg *self,nboolean is)
{
    self->hardFramePointerIsFramePointer=is;
}

//原型 PIC_OFFSET_TABLE_REG_CALL_CLOBBERED default.h
int      mtcs_reg_get_pic_offset_table_reg_call_clobbered(MtcsReg *self)
{
    return self->picOffsetTableRegCallClobbered;
}

void     mtcs_reg_set_pic_offset_table_reg_call_clobbered(MtcsReg *self,int value)
{
    self->picOffsetTableRegCallClobbered=value;
}

//原型 #define EH_RETURN_DATA_REGNO(N) INVALID_REGNUM default.h
int mtcs_reg_get_eh_return_data_regno(MtcsReg *self,int n)
{
    return self->eh_return_data_regno(self,n);
}

//原型 HARD_FRAME_POINTER_IS_ARG_POINTER rtl.h   (HARD_FRAME_POINTER_REGNUM == ARG_POINTER_REGNUM)
nboolean mtcs_reg_hard_frame_pointer_is_arg_pointer(MtcsReg *self)
{
    return (self->normalHardRegsNum.hard_frame_pointer_regnum==self->normalHardRegsNum.arg_pointer_regnum);
}

//原型 ARG_POINTER_REGNUM 各个平台定义
int mtcs_reg_get_arg_pointer_regnum(MtcsReg *self)
{
    return self->normalHardRegsNum.arg_pointer_regnum;
}

//原型 HARD_FRAME_POINTER_REGNUM 各个平台定义
int mtcs_reg_get_hard_frame_pointer_regnum(MtcsReg *self)
{
    return self->normalHardRegsNum.hard_frame_pointer_regnum;
}

//原型 FRAME_POINTER_REGNUM  各个平台定义
int mtcs_reg_get_frame_pointer_regnum(MtcsReg *self)
{
    return self->normalHardRegsNum.frame_pointer_regnum;
}

//原型 #define PIC_OFFSET_TABLE_REGNUM INVALID_REGNUM defautlts.h
int mtcs_reg_get_pic_offset_table_regnum(MtcsReg *self)
{
    return self->pic_offset_table_regnum;
}

/* Register mappings for target machines without register windows.  */
 //原型 INCOMING_REGNO(N) (N) defaults.h
int mtcs_reg_get_incoming_regno(MtcsReg *self,int regno)
{
   return regno;
}

//原型 #define REG_CAN_CHANGE_MODE_P(REGN, FROM, TO)  (targetm.can_change_mode_class (FROM, TO, REGNO_REG_CLASS (REGN)))
bool mtcs_reg_can_change_mode(MtcsReg *self,int regno,machine_mode from ,machine_mode to)
{
   MtcsMode   *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   reg_class_t regClass=mtcs_reg_get_class/*!REGNO_REG_CLASS*/(self,regno);
   return mtcsTarget/*!targetm.can_change_mode_class*/->can_change_mode_class(mtcsTarget,from,to,regClass);
}

/* Given a register bitmap, turn on the bits in a HARD_REG_SET that
   correspond to the hard registers, if any, set in that map.  This
   could be done far more efficiently by having all sorts of special-cases
   with moving single words, but probably isn't worth the trouble.  */
//原型 reg_set_to_hard_reg_set regset.h reginfo.cc
static void setToHardRegSet(MtcsReg *self,HardRegSet *to, const_bitmap from)
{
   unsigned i;
   bitmap_iterator bi;

   EXECUTE_IF_SET_IN_BITMAP (from, 0, i, bi){
      if (i >= mtcs_reg_get_first_virtual_register/*!FIRST_PSEUDO_REGISTER*/(self))
         return;
      mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(self,to, i);
   }
}

//原型 #define REG_SET_TO_HARD_REG_SET(TO, FROM)          \
//do {                          \
//  CLEAR_HARD_REG_SET (TO);                \
//  reg_set_to_hard_reg_set (&TO, FROM);             \
//} while (0)
void mtcs_reg_reg_set_to_hard_reg_set (MtcsReg *self,HardRegSet *to, const_bitmap from)
{
   mtcs_reg_clear_hard_reg_set(to);
   setToHardRegSet(self,to,from);
}

//原型 enum reg_class base_reg_class addresses.h
enum reg_class mtcs_reg_base_reg_class (MtcsReg *self,machine_mode mode ATTRIBUTE_UNUSED,
      addr_space_t as ATTRIBUTE_UNUSED, enum rtx_code outer_code ATTRIBUTE_UNUSED,
      enum rtx_code index_code ATTRIBUTE_UNUSED,rtx_insn *insn ATTRIBUTE_UNUSED)
{
   return self->base_reg_class(self,mode,as,outer_code,index_code,insn);
}

//原型 enum reg_class index_reg_class addresses.h
enum reg_class mtcs_reg_index_reg_class (MtcsReg *self,rtx_insn *insn ATTRIBUTE_UNUSED)
{
   return self->index_reg_class(self,insn);
}

//原型 enum reg_class ok_for_base_p_1 addresses.h
bool mtcs_reg_ok_for_base_p_1 (MtcsReg *self,unsigned regno ATTRIBUTE_UNUSED,
       machine_mode mode ATTRIBUTE_UNUSED,
       addr_space_t as ATTRIBUTE_UNUSED,
       enum rtx_code outer_code ATTRIBUTE_UNUSED,
       enum rtx_code index_code ATTRIBUTE_UNUSED,
       rtx_insn* insn ATTRIBUTE_UNUSED /*!= NULL*/)
{
   //nvptx 调用 nvptx.h 中的#define REGNO_OK_FOR_BASE_P(X) true
   return self->ok_for_base_p_1(self,regno,mode,as,outer_code,index_code,insn);
}

//原型 bool regno_ok_for_base_p addresses.h
bool mtcs_reg_regno_ok_for_base_p(MtcsReg *self,unsigned regno, machine_mode mode, addr_space_t as,
           enum rtx_code outer_code, enum rtx_code index_code,rtx_insn *insn /*!= NULL*/)
{
   if (regno >= self->hardRegsCount/*FIRST_PSEUDO_REGISTER*/ && reg_renumber[regno] >= 0)
      regno = reg_renumber[regno];
   return self->ok_for_base_p_1(self,regno,mode,as,outer_code,index_code,insn);
}

//原型 #define REGNO_OK_FOR_INDEX_P(X) false nvptx.h
bool mtcs_reg_regno_ok_for_index_p(MtcsReg *self,unsigned regno)
{
   return self->regno_ok_for_index_p(self,regno);
}

//原型 #define WORD_REGISTER_OPERATIONS 0 defaults.h
int mtcs_reg_get_word_register_operations(MtcsReg *self)
{
   return self->wordRegisterOpeations;
}
void mtcs_reg_set_word_register_operations(MtcsReg *self,int value)
{
   self->wordRegisterOpeations = value;
}

//原型 #define INDEX_REG_CLASS NO_REGS nvptx.h
 void mtcs_reg_set_index_reg_class(MtcsReg *self,int value)
 {
    self->indexRegClass = value;
 }

 int mtcs_reg_get_index_reg_class(MtcsReg *self)
 {
    return self->indexRegClass;

 }
//原型 #define PSEUDO_REGNO_BYTES(N)   GET_MODE_SIZE (PSEUDO_REGNO_MODE (N)) regs.h
unsigned short  mtcs_reg_get_pseudo_regno_bytes(MtcsReg *self,int i)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlData *mtcsRtlData=mtcs_func_get_rtl_data(mtcsFunc);

   gcc_assert(i<mtcsRtlData->emit.x_reg_rtx_no);
   return mtcs_mode_get_size(mtcsMode,GET_MODE(mtcsRtlData->regno_reg_rtx[i]));
}
/* Return the reg_class in which pseudo reg number REGNO is best allocated.
   This function is sometimes called before the info has been computed.
   When that happens, just return GENERAL_REGS, which is innocuous.  */
//原型 reg_preferred_class rtl.h reginfo.cc
enum reg_class mtcs_reg_reg_preferred_class (MtcsReg *self,int regno)
{
   if (self->refInfo.reg_pref == 0)
      return mtcs_reg_get_general_regs/*!GENERAL_REGS*/(self);

   gcc_assert (regno < self->refInfo.reg_info_size);
   return (enum reg_class) self->refInfo.reg_pref[regno].prefclass;
}

//原型 reg_alternate_class rtl.h reginfo.cc
enum reg_class mtcs_reg_reg_alternate_class (MtcsReg *self,int regno)
{
   if (self->refInfo.reg_pref == 0)
      return mtcs_reg_get_all_regs/*!ALL_REGS*/(self);

   gcc_assert (regno < self->refInfo.reg_info_size);
   return (enum reg_class) self->refInfo.reg_pref[regno].altclass;
}

/* Return the reg_class which is used by IRA for its allocation.  */
//原型 reg_allocno_class rtl.h reginfo.cc
enum reg_class mtcs_reg_reg_allocno_class (MtcsReg *self,int regno)
{
   if (self->refInfo.reg_pref == 0)
      return NO_REGS;

   gcc_assert (regno < self->refInfo.reg_info_size);
   return (enum reg_class) self->refInfo.reg_pref[regno].allocnoclass;
}


//原型 clear_global_regs_cache reginfo
static void clear_global_regs_cache (MtcsReg *self)
{
   for (size_t i = 0 ; i < mtcs_reg_get_first_pseudo_register/*!FIRST_PSEUDO_REGISTER*/(self); i++){
      self->global_regs[i] = 0;
      self->global_regs_decl[i] = NULL;
   }
}
//原型 reginfo_cc_finalize rtl.h reginfo.cc
void mtcs_reg_reginfo_cc_finalize (MtcsReg *self)
{
  clear_global_regs_cache(self);
  self->no_global_reg_vars = 0;
  mtcs_reg_clear_hard_reg_set/*!CLEAR_HARD_REG_SET*/(&self->global_reg_set);
}


/* The same as previous function plus initializing IRA.  */
//原型 reinit_regs rtl.h reginfo.cc
void mtcs_reg_reinit_regs (MtcsReg *self)
{
   MtcsMode   *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReload *mtcsReload = mtcs_target_get_reload(mtcsTarget);
   MtcsRTL *mtcsRTL = mtcs_target_get_rtl(mtcsTarget);
   MtcsRecog *mtcsRecog = mtcs_target_get_recog(mtcsTarget);

   mtcs_reg_init_regs/*!init_regs*/(self);
   /* caller_save needs to be re-initialized.  */
   mtcsReload->target_reload.x_caller_save_initialized_p = false;
   if (mtcsRTL/*!this_target_rtl*/->target_specific_initialized){
      ira_init ();
      mtcs_recog_recog_init/*!recog_init*/(mtcsRecog);
   }
}

/* 原型 void init_fake_stack_mems (void) reload.h reginfo.cc 被 mtcs_reload_register_move_cost 替换*/

/* 原型 int register_move_cost reload.h reginfo.cc 被 mtcs_reload_register_move_cost 替换*/

/* 原型 int memory_move_cost reload.h reginfo.cc 被 mtcs_reload_memory_move_cost 替换*/

/* 原型 int memory_move_secondary_cost reload.h reginfo.cc 被 mtcs_reload_memory_move_secondary_cost 替换*/

/* Return a machine mode that is legitimate for hard reg REGNO and large
   enough to save nregs.  If we can't find one, return VOIDmode.
   If ABI is nonnull, only consider modes that are preserved across
   calls that use ABI.  */
//原型 choose_hard_reg_mode rtl.h reginfo.cc
machine_mode mtcs_reg_choose_hard_reg_mode (MtcsReg *self,unsigned int regno ATTRIBUTE_UNUSED,
            unsigned int nregs, const mtcs_predefined_function_abi *abi)
{
   MtcsMode   *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsReload *mtcsReload = mtcs_target_get_reload(mtcsTarget);
   MtcsRTL *mtcsRTL = mtcs_target_get_rtl(mtcsTarget);
   MtcsRecog *mtcsRecog = mtcs_target_get_recog(mtcsTarget);

   unsigned int /* machine_mode */ m;
   machine_mode found_mode = VOIDmode, mode;

   /* We first look for the largest integer mode that can be validly
   held in REGNO.  If none, we look for the largest floating-point mode.
   If we still didn't find a valid mode, try CCmode.

   The tests use maybe_gt rather than known_gt because we want (for example)
   N V4SFs to win over plain V4SF even though N might be 1.  */
   MTCS_FOR_EACH_MODE_IN_CLASS(mtcsMode,mode, MODE_INT)
      if (mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(self,regno, mode) == nregs
      && mtcsTarget/*!targetm.hard_regno_mode_ok*/->hard_regno_mode_ok(mtcsTarget,regno, mode)
      && (!abi || !abi->clobbers_reg_p (mode, regno))
      && maybe_gt (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode), mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,found_mode)))
         found_mode = mode;

   MTCS_FOR_EACH_MODE_IN_CLASS(mtcsMode,mode, MODE_FLOAT)
      if (mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(self,regno, mode) == nregs
      && mtcsTarget/*!targetm.hard_regno_mode_ok*/->hard_regno_mode_ok(mtcsTarget,regno, mode)
      && (!abi || !abi->clobbers_reg_p (mode, regno))
      && maybe_gt (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode), mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,found_mode)))
         found_mode = mode;

   MTCS_FOR_EACH_MODE_IN_CLASS(mtcsMode,mode, MODE_VECTOR_FLOAT)
      if (mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(self,regno, mode) == nregs
      && mtcsTarget/*!targetm.hard_regno_mode_ok*/->hard_regno_mode_ok(mtcsTarget,regno, mode)
      && (!abi || !abi->clobbers_reg_p (mode, regno))
      && maybe_gt (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode), mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,found_mode)))
         found_mode = mode;

   MTCS_FOR_EACH_MODE_IN_CLASS(mtcsMode,mode, MODE_VECTOR_INT)
      if (mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(self,regno, mode) == nregs
      && mtcsTarget/*!targetm.hard_regno_mode_ok*/->hard_regno_mode_ok(mtcsTarget,regno, mode)
      && (!abi || !abi->clobbers_reg_p (mode, regno))
      && maybe_gt (mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,mode), mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,found_mode)))
         found_mode = mode;

   if (found_mode != VOIDmode)
      return found_mode;

   /* Iterate over all of the CCmodes.  */
   for (m = (unsigned int) CCmode; m < (unsigned int) mtcs_mode_get_number(mtcsMode)/*!NUM_MACHINE_MODES*/; ++m){
      mode = (machine_mode) m;
      if (mtcs_reg_hard_regno_nregs/*!hard_regno_nregs*/(self,regno, mode) == nregs
      && mtcsTarget/*!targetm.hard_regno_mode_ok*/->hard_regno_mode_ok(mtcsTarget,regno, mode)
      && (!abi || !abi->clobbers_reg_p (mode, regno)))
         return mode;
   }

   /* We can't find a mode valid for this register.  */
   return VOIDmode;
}

/* Specify the usage characteristics of the register named NAME.
   It should be a fixed register if FIXED and a
   call-used register if CALL_USED.  */
//原型 fix_register rtl.h reginfo.cc
void mtcs_reg_fix_register (MtcsReg *self,const char *name, int fixed, int call_used)
{
   MtcsMode   *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConfig *mtcsConfig = mtcs_target_get_config(mtcsTarget);
   MtcsOutput *mtcsOutput = mtcs_target_get_output(mtcsTarget);

   int i;
   int reg, nregs;
   /* Decode the name and update the primary form of
   the register info.  */
   if ((reg = mtcs_output_decode_reg_name_and_count/*!decode_reg_name_and_count*/(mtcsOutput,name, &nregs)) >= 0){
      gcc_assert (nregs >= 1);
      for (i = reg; i < reg + nregs; i++){
         bool re=i==mtcs_reg_get_frame_pointer_regnum/*!FRAME_POINTER_REGNUM*/(self);
         if(mtcs_config_ifdef(mtcsConfig,MTCS_HARD_FRAME_POINTER_REGNUM))
            re=i==mtcs_reg_get_hard_frame_pointer_regnum/*!HARD_FRAME_POINTER_REGNUM*/(self);

         if ((i == mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(self) || re
         /*!
         #ifdef HARD_FRAME_POINTER_REGNUM
         || i == HARD_FRAME_POINTER_REGNUM
         #else
         || i == FRAME_POINTER_REGNUM
         #endif
         */
         )
         && (fixed == 0 || call_used == 0)){
            switch (fixed){
               case 0:
                  switch (call_used){
                     case 0:
                        error ("cannot use %qs as a call-saved register", name);
                        break;

                     case 1:
                        error ("cannot use %qs as a call-used register", name);
                        break;

                     default:
                     gcc_unreachable ();
                  }
                  break;

               case 1:
                  switch (call_used){
                     case 1:
                        error ("cannot use %qs as a fixed register", name);
                        break;

                     case 0:
                        default:
                        gcc_unreachable ();
                  }
                  break;

               default:
               gcc_unreachable ();
            }
         }else{
            self->hardRegs.x_fixed_regs/*!fixed_regs*/[i] = fixed;
            if(mtcs_config_ifdef(mtcsConfig,MTCS_CALL_REALLY_USED_REGISTERS)){
               if (fixed == 0)
                  self->hardRegs.x_call_used_regs/*!call_used_regs*/[i] = call_used;
            }else{
               self->hardRegs.x_call_used_regs/*!call_used_regs*/[i] = call_used;
            }
               /*!
               #ifdef CALL_REALLY_USED_REGISTERS
               if (fixed == 0)
               call_used_regs[i] = call_used;
               #else
               call_used_regs[i] = call_used;
               #endif
               */
         }
      }
   }else{
      warning (0, "unknown register name: %s", name);
   }
}


/* Mark register number I as global.  */
//原型 globalize_reg rtl.h reginfo.cc
void mtcs_reg_globalize_reg (MtcsReg *self,tree decl, int i)
{
   MtcsMode   *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsConfig *mtcsConfig = mtcs_target_get_config(mtcsTarget);
   MtcsOutput *mtcsOutput = mtcs_target_get_output(mtcsTarget);
   MtcsFuncAbi *mtcsFuncAbi = mtcs_target_get_func_abi(mtcsTarget);

   location_t loc = DECL_SOURCE_LOCATION (decl);

   if(mtcs_config_ifdef(mtcsConfig,MTCS_STACK_REGS)){
      if (IN_RANGE (i, mtcs_reg_get_first_stack_reg/*!FIRST_STACK_REG*/(self),
         mtcs_reg_get_last_stack_reg/*!LAST_STACK_REG*/(self))){
         error ("stack register used for global register variable");
         return;
      }
   }
   /*
   #ifdef STACK_REGS
   if (IN_RANGE (i, FIRST_STACK_REG, LAST_STACK_REG))
   {
   error ("stack register used for global register variable");
   return;
   }
   #endif
   */

   if (self->hardRegs.x_fixed_regs/*!fixed_regs*/[i] == 0 && self->no_global_reg_vars)
      error_at (loc, "global register variable follows a function definition");

   if (self->global_regs[i]){
      auto_diagnostic_group d;
      warning_at (loc, 0, "register of %qD used for multiple global register variables",decl);
      inform (DECL_SOURCE_LOCATION (global_regs_decl[i]),"conflicts with %qD", self->global_regs_decl[i]);
      return;
   }

   if (self->hardRegs.x_call_used_regs/*!call_used_regs*/[i] && ! self->hardRegs.x_fixed_regs/*!fixed_regs*/[i])
      warning_at (loc, 0, "call-clobbered register used for global register variable");

   self->global_regs[i] = 1;
   self->global_regs_decl[i] = decl;
   mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(self,&self->global_reg_set, i);

   /* If we're globalizing the frame pointer, we need to set the
   appropriate regs_invalidated_by_call bit, even if it's already
   set in fixed_regs.  */
   if (i != mtcs_reg_get_stack_pointer_regnum/*!STACK_POINTER_REGNUM*/(self)){
      mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(self,&self->hardRegs.x_regs_invalidated_by_call/*!regs_invalidated_by_call*/, i);
      for (unsigned int j = 0; j < MTCS_NUM_ABI_IDS; ++j)
         mtcsFuncAbi->x_function_abis/*!function_abis*/[j].add_full_reg_clobber (i);
   }

   /* If already fixed, nothing else to do.  */
   if (self->hardRegs.x_fixed_regs/*!fixed_regs*/[i])
      return;

   self->hardRegs.x_fixed_regs/*!fixed_regs*/[i] = self->hardRegs.x_call_used_regs/*!call_used_regs*/[i] = 1;

   mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(self,&self->hardRegs.x_fixed_reg_set/*!fixed_reg_set*/, i);

   mtcs_reg_reinit_regs/*!reinit_regs*/(self);
}


/* Allocate space for reg info and initilize it.  */
//原型 allocate_reg_info reginfo.cc
static void allocate_reg_info (MtcsReg *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   int i;

   self->refInfo.max_regno_since_last_resize = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);
   self->refInfo.reg_info_size =  self->refInfo.max_regno_since_last_resize * 3 / 2 + 1;
   gcc_assert (!  self->refInfo.reg_pref && ! reg_renumber);
   reg_renumber = XNEWVEC (short, self->refInfo.reg_info_size);
   self->refInfo.reg_pref = XCNEWVEC (RegPref, self->refInfo.reg_info_size);
   memset (reg_renumber, -1, self->refInfo.reg_info_size * sizeof (short));
   for (i = 0; i < self->refInfo.reg_info_size; i++){
      self->refInfo.reg_pref[i].prefclass = mtcs_reg_get_general_regs/*!GENERAL_REGS*/(self);
      self->refInfo.reg_pref[i].altclass = mtcs_reg_get_all_regs/*!ALL_REGS*/(self);
      self->refInfo.reg_pref[i].allocnoclass = mtcs_reg_get_general_regs/*!GENERAL_REGS*/(self);
   }
}



/* Resize reg info. The new elements will be initialized.  Return TRUE
   if new pseudos were added since the last call.  */
//原型 resize_reg_info rtl.h reginfo.cc
bool mtcs_reg_resize_reg_info (MtcsReg *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   int old, i;
   bool change_p;
   int maxRegNum = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);
   if (self->refInfo.reg_pref == NULL){
      allocate_reg_info(self);
      return true;
   }
   change_p = self->refInfo.max_regno_since_last_resize != maxRegNum/*!max_reg_num ()*/;
   self->refInfo.max_regno_since_last_resize =maxRegNum/*!max_reg_num ()*/;
   if ( self->refInfo.reg_info_size >= maxRegNum/*!max_reg_num ()*/)
      return change_p;
   old =  self->refInfo.reg_info_size;
   self->refInfo.reg_info_size = maxRegNum/*!max_reg_num ()*/ * 3 / 2 + 1;
   gcc_assert ( self->refInfo.reg_pref && reg_renumber);
   reg_renumber = XRESIZEVEC (short, reg_renumber,  self->refInfo.reg_info_size);
   self->refInfo.reg_pref = XRESIZEVEC (RegPref,  self->refInfo.reg_pref,  self->refInfo.reg_info_size);
   memset ( self->refInfo.reg_pref + old, -1,( self->refInfo.reg_info_size - old) * sizeof (RegPref));
   memset (reg_renumber + old, -1, ( self->refInfo.reg_info_size - old) * sizeof (short));
   for (i = old; i <  self->refInfo.reg_info_size; i++) {
      self->refInfo.reg_pref[i].prefclass =  mtcs_reg_get_general_regs/*!GENERAL_REGS*/(self);
      self->refInfo.reg_pref[i].altclass = mtcs_reg_get_all_regs/*!ALL_REGS*/(self);
      self->refInfo.reg_pref[i].allocnoclass =  mtcs_reg_get_general_regs/*!GENERAL_REGS*/(self);
   }
   return true;
}

/* Free up the space allocated by allocate_reg_info.  */
//原型 free_reg_info rtl.h reginfo.cc
void mtcs_reg_free_reg_info (MtcsReg *self)
{
   if (self->refInfo.reg_pref){
      free (self->refInfo.reg_pref);
      self->refInfo.reg_pref = NULL;
   }
   //原型 short *reg_renumber regs.h
   if (reg_renumber){
      free (reg_renumber);
      reg_renumber = NULL;
   }
}


/* Initialize some global data for this pass.  */
//原型 reginfo_init reginfo.cc 用在pass reginfo中
unsigned int mtcs_reg_reginfo_init (MtcsReg *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfscan   *mtcsDfscan =mtcs_target_get_dfscan(mtcsTarget);

   if (df)
      mtcs_dfscan_df_compute_regs_ever_live/*!df_compute_regs_ever_live*/(mtcsDfscan,true);

   /* This prevents dump_reg_info from losing if called
   before reginfo is run.  */
   self->refInfo.reg_pref = NULL;
   self->refInfo.reg_info_size = self->refInfo.max_regno_since_last_resize = 0;
   /* No more global register variables may be declared.  */
   self->no_global_reg_vars = 1;
   return 1;
}

/* Set up preferred, alternate, and allocno classes for REGNO as
   PREFCLASS, ALTCLASS, and ALLOCNOCLASS.  */
//原型 setup_reg_classes rtl.h reginfo.cc
void mtcs_reg_setup_reg_classes (MtcsReg *self,int regno, enum reg_class prefclass,
      enum reg_class altclass,enum reg_class allocnoclass)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   if (self->refInfo.reg_pref == NULL)
      return;
   gcc_assert (self->refInfo.reg_info_size >= mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc));
   self->refInfo.reg_pref[regno].prefclass = prefclass;
   self->refInfo.reg_pref[regno].altclass = altclass;
   self->refInfo.reg_pref[regno].allocnoclass = allocnoclass;
}

/* 原型 reg_scan rtl.h reginfo.cc 被 mtcs_rtl_reg_scan 替换*/

/* Return true if C1 is a subset of C2, i.e., if every register in C1
   is also in C2.  */
//原型 reg_class_subset_p rtl.h reginfo.cc
bool mtcs_reg_reg_class_subset_p (MtcsReg *self,reg_class_t c1, reg_class_t c2)
{
  return (c1 == c2
     || c2 == mtcs_reg_get_all_regs/*!ALL_REGS*/(self)
     || mtcs_reg_hard_reg_set_subset_p/*!hard_reg_set_subset_p*/ (&self->hardRegs.x_reg_class_contents/*!reg_class_contents*/[(int) c1],
           &self->hardRegs.x_reg_class_contents/*!reg_class_contents*/[(int) c2]));
}

/* Return true if there is a register that is in both C1 and C2.  */
//原型 reg_classes_intersect_p rtl.h reginfo.cc
bool mtcs_reg_reg_classes_intersect_p (MtcsReg *self,reg_class_t c1, reg_class_t c2)
{
   return (c1 == c2
     || c1 == mtcs_reg_get_all_regs/*!ALL_REGS*/(self)
     || c2 == mtcs_reg_get_all_regs/*!ALL_REGS*/(self)
     || mtcs_reg_hard_reg_set_intersect_p/*!hard_reg_set_intersect_p*/(
           &self->hardRegs.x_reg_class_contents/*reg_class_contents[i]*/[(int) c1],
           &self->hardRegs.x_reg_class_contents/*reg_class_contents[i]*/[(int) c2]));
}

/* Used to cache the results of simplifiable_subregs.  SHAPE is the input
   parameter and SIMPLIFIABLE_REGS is the result.  */
//在型 simplifiable_subreg hard-reg-set.h reginfo.cc
class mtcs_simplifiable_subreg
{
public:
   mtcs_simplifiable_subreg (const subreg_shape &,int hardRegElement);

  subreg_shape shape;
  HardRegSet /*!HARD_REG_SET*/ simplifiable_regs;
};

inline hashval_t mtcs_simplifiable_subregs_hasher::hash (const mtcs_simplifiable_subreg *value)
{
  inchash::hash h;
  h.add_hwi (value->shape.unique_id ());
  return h.end ();
}

inline bool mtcs_simplifiable_subregs_hasher::equal (const mtcs_simplifiable_subreg *value,
                const subreg_shape *compare)
{
  return value->shape == *compare;
}

inline mtcs_simplifiable_subreg::mtcs_simplifiable_subreg (const subreg_shape &shape_in,int hardRegElement)
  : shape (shape_in)
{
   simplifiable_regs.count=hardRegElement;
   mtcs_reg_clear_hard_reg_set/*CLEAR_HARD_REG_SET*/(&simplifiable_regs);
}


/* Return the set of hard registers that are able to form the subreg
   described by SHAPE.  */
//原型 simplifiable_subregs rtl.h reginfo.cc
const HardRegSet & mtcs_reg_simplifiable_subregs (MtcsReg *self,const subreg_shape &shape)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL   *mtcsRTL = mtcs_target_get_rtl(mtcsTarget);

   if (!self->hardRegs.x_simplifiable_subregs)
      self->hardRegs.x_simplifiable_subregs = new hash_table <mtcs_simplifiable_subregs_hasher> (30);
   inchash::hash h;
   h.add_hwi (shape.unique_id ());
   mtcs_simplifiable_subreg **slot= (self->hardRegs.x_simplifiable_subregs->find_slot_with_hash (&shape, h.end (), INSERT));

   if (!*slot){
      mtcs_simplifiable_subreg *info = new mtcs_simplifiable_subreg (shape,self->hardRegElement);
      for (unsigned int i = 0; i < self->hardRegsCount/*FIRST_PSEUDO_REGISTER*/; ++i)
         if (mtcsTarget/*!targetm.hard_regno_mode_ok*/->hard_regno_mode_ok(mtcsTarget,i, shape.inner_mode)
         && mtcs_rtl_simplify_subreg_regno/*!simplify_subreg_regno*/(mtcsRTL,i, shape.inner_mode, shape.offset,
         shape.outer_mode) >= 0)
            mtcs_reg_set_hard_reg_bit/*!SET_HARD_REG_BIT*/(self,&info->simplifiable_regs, i);
      *slot = info;
   }
   return (*slot)->simplifiable_regs;
}

/* Restrict the choice of register for SUBREG_REG (SUBREG) based
   on information about SUBREG.

   If PARTIAL_DEF, SUBREG is a partial definition of a multipart inner
   register and we want to ensure that the other parts of the inner
   register are correctly preserved.  If !PARTIAL_DEF we need to
   ensure that SUBREG itself can be formed.  */
//原型 record_subregs_of_mode reginfo.cc
static void record_subregs_of_mode (MtcsReg *self,rtx subreg, bool partial_def)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsRTL   *mtcsRTL = mtcs_target_get_rtl(mtcsTarget);

   unsigned int regno;

   if (!REG_P (SUBREG_REG (subreg)))
      return;

   regno = REGNO (SUBREG_REG (subreg));
   if (regno < self->hardRegsCount/*FIRST_PSEUDO_REGISTER*/)
      return;

   subreg_shape shape (shape_of_subreg (subreg));
   if (partial_def){
      /* The number of independently-accessible SHAPE.outer_mode values
      in SHAPE.inner_mode is GET_MODE_SIZE (SHAPE.inner_mode) / SIZE.
      We need to check that the assignment will preserve all the other
      SIZE-byte chunks in the inner register besides the one that
      includes SUBREG.

      In practice it is enough to check whether an equivalent
      SHAPE.inner_mode value in an adjacent SIZE-byte chunk can be formed.
      If the underlying registers are small enough, both subregs will
      be valid.  If the underlying registers are too large, one of the
      subregs will be invalid.

      This relies on the fact that we've already been passed
      SUBREG with PARTIAL_DEF set to false.

      The size of the outer mode must ordered wrt the size of the
      inner mode's registers, since otherwise we wouldn't know at
      compile time how many registers the outer mode occupies.  */
      poly_uint64 size = ordered_max (mtcs_mode_get_regmode_natural_size/*!REGMODE_NATURAL_SIZE*/(mtcsMode,shape.inner_mode),
            mtcs_mode_get_size_poly/*!GET_MODE_SIZE*/(mtcsMode,shape.outer_mode));
      gcc_checking_assert (known_lt (size, mtcs_mode_get_size/*!GET_MODE_SIZE*/(mtcsMode,shape.inner_mode)));
      if (known_ge (shape.offset, size))
         shape.offset -= size;
      else
         shape.offset += size;
   }

   if (self->valid_mode_changes[regno])
      *self->valid_mode_changes[regno] &= mtcs_reg_simplifiable_subregs/*!simplifiable_subregs*/(self,shape);
   else{
      self->valid_mode_changes[regno]= XOBNEW (&self->valid_mode_changes_obstack, HardRegSet);
      self->valid_mode_changes[regno]->count=mtcs_reg_get_hard_reg_element_count(self);
      *self->valid_mode_changes[regno] = mtcs_reg_simplifiable_subregs/*!simplifiable_subregs*/(self,shape);
   }
}

/* Call record_subregs_of_mode for all the subregs in X.  */
//原型 record_subregs_of_mode reginfo.cc
static void find_subregs_of_mode (MtcsReg *self,rtx x)
{
   enum rtx_code code = GET_CODE (x);
   const char * const fmt = GET_RTX_FORMAT (code);
   int i;

   if (code == SUBREG)
      record_subregs_of_mode(self,x, false);

   /* Time for some deep diving.  */
   for (i = GET_RTX_LENGTH (code) - 1; i >= 0; i--){
      if (fmt[i] == 'e')
         find_subregs_of_mode (self,XEXP (x, i));
      else if (fmt[i] == 'E'){
         int j;
         for (j = XVECLEN (x, i) - 1; j >= 0; j--)
            find_subregs_of_mode (self,XVECEXP (x, i, j));
      }
   }
}

//原型 init_subregs_of_mode rtl.h reginfo.cc
void mtcs_reg_init_subregs_of_mode (MtcsReg *self)
{
   MtcsMode *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsFunc   *mtcsFunc=mtcs_target_get_func(mtcsTarget);
   MtcsRtlanal   *mtcsRtlanal=mtcs_target_get_rtlanal(mtcsTarget);

   basic_block bb;
   rtx_insn *insn;

   gcc_obstack_init (&self->valid_mode_changes_obstack);
   self->valid_mode_changes = XCNEWVEC (HardRegSet *,  mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc));

   FOR_EACH_BB_FN (bb, cfun)
      FOR_BB_INSNS (bb, insn)
         if (NONDEBUG_INSN_P (insn)){
            find_subregs_of_mode(self,PATTERN (insn));
            df_ref def;
            FOR_EACH_INSN_DEF (def, insn)
            if (DF_REF_FLAGS_IS_SET (def, DF_REF_PARTIAL)
                  && mtcs_rtlanal_read_modify_subreg_p/*!read_modify_subreg_p*/(mtcsRtlanal,DF_REF_REG (def)))
               record_subregs_of_mode(self,DF_REF_REG (def), true);
         }
}

//原型 valid_mode_changes_for_regno rtl.h reginfo.cc
const HardRegSet *mtcs_reg_valid_mode_changes_for_regno (MtcsReg *self,unsigned int regno)
{
  return self->valid_mode_changes[regno];
}

//原型 finish_subregs_of_mode rtl.h reginfo.cc
void mtcs_reg_finish_subregs_of_mode (MtcsReg *self)
{
  XDELETEVEC (self->valid_mode_changes);
  obstack_free (&self->valid_mode_changes_obstack, NULL);
}

/* Free all data attached to the structure.  This isn't a destructor because
   we don't want to run on exit.  */
//原型 void target_hard_regs::finalize () reginfo.cc
void mtcs_reg_free_hard_regs (MtcsReg *self)
{
  delete self->hardRegs.x_simplifiable_subregs;
}

/************************以下是regstat----------------*************/
/*----------------------------------------------------------------------------
   REG_N_SETS and REG_N_REFS.
   ----------------------------------------------------------------------------*/

/* If a pass need to change these values in some magical way or the
   pass needs to have accurate values for these and is not using
   incremental df scanning, then it should use REG_N_SETS and
   REG_N_USES.  If the pass is doing incremental scanning then it
   should be getting the info from DF_REG_DEF_COUNT and
   DF_REG_USE_COUNT.  */
//原型 regstat_init_n_sets_and_refs regs.h regstat.cc
void mtcs_reg_regstat_init_n_sets_and_refs (MtcsReg *self)
{
   MtcsMode   *mtcsMode=MTCS_GET_MODE_OBJECT(self);
   MtcsTarget *mtcsTarget=(MtcsTarget *)mtcsMode->target;
   MtcsDfscan *mtcsDfscan=mtcs_target_get_dfscan(mtcsTarget);
   MtcsFunc *mtcsFunc=mtcs_target_get_func(mtcsTarget);

   unsigned int i;
   unsigned int max_regno = mtcs_func_max_reg_num/*!max_reg_num*/(mtcsFunc);

   mtcs_dfsan_df_grow_reg_info/*!df_grow_reg_info*/(mtcsDfscan);
   gcc_assert (!regstat_n_sets_and_refs);//regstat_n_sets_and_refs regs.h regstat.cc

   regstat_n_sets_and_refs = XNEWVEC (struct regstat_n_sets_and_refs_t, max_regno);

   if (MAY_HAVE_DEBUG_BIND_INSNS)
      for (i = 0; i < max_regno; i++){
         int use_count;
         df_ref use;
         use_count = DF_REG_USE_COUNT (i);
         for (use = DF_REG_USE_CHAIN (i); use; use = DF_REF_NEXT_REG (use))
            if (DF_REF_INSN_INFO (use) && DEBUG_INSN_P (DF_REF_INSN (use)))
               use_count--;

         SET_REG_N_SETS (i, DF_REG_DEF_COUNT (i));
         SET_REG_N_REFS (i, use_count + REG_N_SETS (i));
      }else
         for (i = 0; i < max_regno; i++){
            SET_REG_N_SETS (i, DF_REG_DEF_COUNT (i));
            SET_REG_N_REFS (i, DF_REG_USE_COUNT (i) + REG_N_SETS (i));
         }

}



