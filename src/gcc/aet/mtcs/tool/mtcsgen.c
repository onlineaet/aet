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
#include "bconfig.h"
#define INCLUDE_STRING
#define INCLUDE_VECTOR
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "rtl.h"
#include "obstack.h"
#include "errors.h"
#include "read-md.h"
#include "gensupport.h"
#include "vec.h"
#include "../ptx/gen/ptx-insn-modes.h" //在aet.mk中加入mtcsgen 依赖 ptx/gen/ptx-insn-modes.h 实现先编译 ptx-insn-modes.h再编译mtcsgen
#include "aet/nlib.h"
#include "mtcsgen.h"


char *mtcs_prefix="ptx";
char *mtcs_prefix_uppercase="PTX";
char *mtcs_prefix_object="Ptx";

struct std_pred_table
{
  const char *name;
  bool special;
  bool allows_const_p;
  RTX_CODE codes[NUM_RTX_CODE];
};

static const struct std_pred_table std_preds[] = {
  {"mtcs_preds_general_operand", false, true, {SUBREG, REG, MEM}},
  {"mtcs_preds_address_operand", true, true, {SUBREG, REG, MEM, PLUS, MINUS, MULT,
                   ZERO_EXTEND, SIGN_EXTEND, AND}},
  {"mtcs_preds_register_operand", false, false, {SUBREG, REG}},
  {"mtcs_preds_pmode_register_operand", true, false, {SUBREG, REG}},
  {"mtcs_preds_scratch_operand", false, false, {SCRATCH, REG}},
  {"mtcs_preds_immediate_operand", false, true, {UNKNOWN}},
  {"mtcs_preds_const_int_operand", false, false, {CONST_INT}},
#if TARGET_SUPPORTS_WIDE_INT
  {"mtcs_preds_const_scalar_int_operand", false, false, {CONST_INT, CONST_WIDE_INT}},
  {"mtcs_preds_const_double_operand", false, false, {CONST_DOUBLE}},
#else
  {"mtcs_preds_const_double_operand", false, false, {CONST_INT, CONST_DOUBLE}},
#endif
  {"mtcs_preds_nonimmediate_operand", false, false, {SUBREG, REG, MEM}},
  {"mtcs_preds_nonmemory_operand", false, true, {SUBREG, REG}},
  {"mtcs_preds_push_operand", false, false, {MEM}},
  {"mtcs_preds_pop_operand", false, false, {MEM}},
  {"mtcs_preds_memory_operand", false, false, {SUBREG, MEM}},
  {"mtcs_preds_indirect_operand", false, false, {SUBREG, MEM}},
  {"mtcs_preds_ordered_comparison_operator", false, false, {EQ, NE,
                         LE, LT, GE, GT,
                         LEU, LTU, GEU, GTU}},
  {"mtcs_preds_comparison_operator", false, false, {EQ, NE,
                     LE, LT, GE, GT,
                     LEU, LTU, GEU, GTU,
                     UNORDERED, ORDERED,
                     UNEQ, UNGE, UNGT,
                     UNLE, UNLT, LTGT}}
};



//原型 init_predicate_table (void) gensupport.cc
void mtcs_gen_append_preds(MtcsGen *self)
{
        if(5>3)
           return;
        size_t i, j;
        struct pred_data *pred;
        int count=ARRAY_SIZE (std_preds);
        for (i = 0; i < count; i++){
            pred = XCNEW (struct pred_data);
            pred->name = std_preds[i].name;
            pred->special = std_preds[i].special;
            for (j = 0; std_preds[i].codes[j] != 0; j++)
               add_predicate_code (pred, std_preds[i].codes[j]);
            if (std_preds[i].allows_const_p)
               for (j = 0; j < NUM_RTX_CODE; j++)
                  if (GET_RTX_CLASS (j) == RTX_CONST_OBJ)
                    add_predicate_code (pred, (enum rtx_code) j);
            add_predicate (pred);
        }
}

/* Return true if instruction NAME matches pattern PAT, storing information
   about the match in P if so.  */
//原型 find_optab genupport.cc 因为调用match_pattern 引用了 GET_MODE_NAME 和 MAX_MACHINE_MODE
//所以移到这里重新实现
static bool match_pattern (MtcsGen *self,optab_pattern *p, const char *name, const char *pat)
{
  bool force_float = false;
  bool force_int = false;
  bool force_partial_int = false;
  bool force_fixed = false;
 // match_pattern_test(self,p,name,pat);
  char *oldpat=pat;
  char *oldname=name;
  if (pat == NULL)
    return false;
  for (; ; ++pat){
      if (*pat != '$'){
          if (*pat != *name++)
            return false;
          if (*pat == '\0')
            return true;
          continue;
      }
      switch (*++pat){
        case 'I':
          force_int = 1;
          break;
        case 'P':
          force_partial_int = 1;
          break;
        case 'F':
          force_float = 1;
          break;
        case 'Q':
          force_fixed = 1;
          break;

        case 'a':
        case 'b':
          {
            int i;

            /* This loop will stop at the first prefix match, so
               look through the modes in reverse order, in case
               there are extra CC modes and CC is a prefix of the
               CC modes (as it should be).  */
            for (i = self->maxMachineMode - 1; i >= 0; i--){
                const char *p, *q;
                for (p = self->modeNames[i]/*!GET_MODE_NAME (i)*/, q = name; *p; p++, q++)
                  if (TOLOWER (*p) != *q)
                    break;
                if (*p == 0
                    && (! force_int || self->modeClass[i] == MODE_INT
                    || self->modeClass[i] == MODE_VECTOR_INT)
                    && (! force_partial_int
                    || self->modeClass[i] == MODE_INT
                    || self->modeClass[i] == MODE_PARTIAL_INT
                    || self->modeClass[i] == MODE_VECTOR_INT)
                    && (! force_float
                    || self->modeClass[i] == MODE_FLOAT
                    || self->modeClass[i] == MODE_DECIMAL_FLOAT
                    || self->modeClass[i] == MODE_COMPLEX_FLOAT
                    || self->modeClass[i] == MODE_VECTOR_FLOAT)
                    && (! force_fixed
                    || self->modeClass[i] == MODE_FRACT
                    || self->modeClass[i] == MODE_UFRACT
                    || self->modeClass[i] == MODE_ACCUM
                    || self->modeClass[i] == MODE_UACCUM
                    || self->modeClass[i] == MODE_VECTOR_FRACT
                    || self->modeClass[i] == MODE_VECTOR_UFRACT
                    || self->modeClass[i] == MODE_VECTOR_ACCUM
                    || self->modeClass[i] == MODE_VECTOR_UACCUM))
                  break;
            }

            if (i < 0)
              return false;
            name += strlen ( self->modeNames[i]/*!GET_MODE_NAME (i)*/);
            if (*pat == 'a')
              p->m1 = i;
            else
              p->m2 = i;

            force_int = false;
            force_partial_int = false;
            force_float = false;
            force_fixed = false;
          }
          break;

        default:
          gcc_unreachable ();
      }
  }
}

/* Return true if NAME is the name of an optab, describing it in P if so.  */
//optabs gensupport.h gensupport.cc
//bool mtcs_gen_find_optab (MtcsGen *self,optab_pattern *p, const char *name)
//原型 find_optab genupport.cc 因为调用match_pattern 引用了 GET_MODE_NAME 和 MAX_MACHINE_MODE
//所以移到这里重新实现
bool mtcs_gen_find_optab (MtcsGen *self,void *op, const char *name)
{
   optab_pattern *p=(optab_pattern *)op;
  if (*name == 0 || *name == '*')
    return false;
  /* See if NAME matches one of the patterns we have for the optabs
     we know about.  */
  for (unsigned int pindex = 0; pindex < num_optabs/*!ARRAY_SIZE (optabs)*/; pindex++){
      p->m1 = p->m2 = 0;
      if (match_pattern(self,p, name, optabs[pindex].pattern)){
          p->name = name;
          p->op = optabs[pindex].op;
          p->sort_num = (p->op << 20) | (p->m2 << 10) | p->m1;
          return true;
      }
  }
  return false;
}

/**
 * read-rtl.cc中的get_mode_token改为 mtcsGetModeToken_cb
 */
static const char *mtcsGetModeToken_cb (int mode)
{
  const char *platUpperName=mtcs_gen_get_platform_upper_name(mtcs_gen_get());
  const char *modeName=mtcs_gen_get_mode_name(mtcs_gen_get(),mode);
  if(rtx_reader_ptr && strstr(rtx_reader_ptr->get_filename(),"mtcs_ptx") && !strcmp(progname,"mtcsgenemit"))
     fprintf(stderr,"mtcsGetModeToken_cb --- %d %s %s\n",mode,platUpperName,modeName);
  return concat (platUpperName,"_", modeName, "mode", NULL);
}

static HOST_WIDE_INT mtcsFindMode_cb (const char *name)
{
   MtcsGen *self=mtcs_gen_get();
   int maxMachineMode=self->maxMachineMode;
   int i;

   for (i = 0; i < maxMachineMode; i++)
      if (strcmp (self->modeNames[i], name) == 0){
         //if(rtx_reader_ptr && strstr(rtx_reader_ptr->get_filename(),"mtcs_ptx") && !strcmp(progname,"mtcsgenemit"))
          //  fprintf(stderr,"read-rtl.cc  find_mode --- prog:%s %d %s\n",progname,i,name);
         return i;
      }

   fatal_with_file_and_line ("unknown mode `%s'", name);
}

static void setPlatform(MtcsGen *self,char *platFormName,bool modifyMode)
{
    if(strcmp(platFormName,"ptx"))
        error("平台名不是ptx") ;
    self->platformName=xstrdup(platFormName);
    self->platformNameUppercase=xstrdup(platFormName);
    self->platformNameObject=xstrdup(platFormName);
    char c=platFormName[0];
    int count=0;
    while(c!='\0'){
        self->platformNameUppercase[count++]=TOUPPER(c);
        c=platFormName[count];
    }
    self->platformNameObject[0]=TOUPPER(platFormName[0]);
    self->maxMachineMode=PTX_MAX_MACHINE_MODE;
    self->modeNames=ptx_get_modes_name();//ptxModeName;
    self->modeClass=ptx_get_mode_class();
    self->modeInner=ptx_get_mode_inner();
    self->modeNunits=ptx_get_mode_nunits();
    if(modifyMode)
       read_rtl_reset_modes(self->modeNames,self->maxMachineMode,(void *)mtcsGetModeToken_cb,(void *)mtcsFindMode_cb);
}


/*
 * 生成文件的保存路径，由平台名+gen组成完整路径
 */
static void setSaveRootPath(MtcsGen *self,char *path)
{
    self->saveRootPath=xstrdup(path);
}

static void mtcsGenInit(MtcsGen *self)
{
    self->platformName=NULL;
    self->platformNameUppercase=NULL;
    self->platformNameObject=NULL;
    self->saveRootPath=NULL;
    self->cFile=NULL;
    self->hFile=NULL;
}

/**
 * ptx
 */
const char *mtcs_gen_get_platform_name(MtcsGen *self)
{
   return self->platformName;
}

/**
 * PTX
 */
const char *mtcs_gen_get_platform_upper_name(MtcsGen *self)
{
   return self->platformNameUppercase;
}

/**
 * Ptx
 */
const char *mtcs_gen_get_platform_object_name(MtcsGen *self)
{
   return self->platformNameObject;
}

const char *mtcs_gen_get_save_root_path(MtcsGen *self)
{
   return self->saveRootPath;
}


/**
 * 从主机mode转设备
 */
int mtcs_gen_hostmode_to_devicemode(MtcsGen *self,int hostMode)
{
   const char *modeName=GET_MODE_NAME(hostMode);
   int i;
   for(i=0;i<self->maxMachineMode;i++){
         if(strcmp(modeName,self->modeNames[i])==0)
             return i;
   }
   error("不能从主机mode转成设备mode hostName:%s hostMode:%d\n",modeName,hostMode);
   return -1;
}

bool         mtcs_gen_is_ptx(MtcsGen *self)
{
    return strcmp(self->platformName,"ptx")==0;
}
bool         mtcs_gen_is_gcn(MtcsGen *self)
{
    return false;
}
bool         mtcs_gen_is_spirv(MtcsGen *self)
{
    return false;
}

char       *mtcs_gen_get_c_file(MtcsGen *self)
{
   return self->cFile;
}
char       *mtcs_gen_get_h_file(MtcsGen *self)
{
   return self->hFile;
}

/**
 * object=output emit recog
 *
static MtcsTarget *mtcsTarget;

void mtcs_ptx_output_set_target(MtcsPtxOutput *self, void *target)
{
  mtcsTarget=(MtcsTarget *)target;
}
 */
char *mtcs_gen_create_target_code(MtcsGen *self,char *object)
{
   char *objectType=xstrdup(object);
   objectType[0]=TOUPPER(objectType[0]);
   char result[1024];
   sprintf(result,"\nstatic MtcsTarget *mtcsTarget;\n\nvoid mtcs_%s_%s_set_target(Mtcs%s%s *self, void *target)\n{\n mtcsTarget=(MtcsTarget *)target;\n}\n\n",
           self->platformName,object,self->platformNameObject,objectType);
   char *ret=xstrdup(result);
   return ret;
}

char        *mtcs_gen_get_mode_name(MtcsGen *self,int mode)
{
    if(mode<0 || mode>=self->maxMachineMode){
        fprintf(stderr,"出错了get_mode_name 00  %d %d 主机 modeName:%s\n",mode,self->maxMachineMode,GET_MODE_NAME(mode));
        error("没有指定平台 主机 modeName %s\n",GET_MODE_NAME(mode));
        gcc_unreachable ();
    }
    if(strstr(self->modeNames[mode],"RETURN")){
        fprintf(stderr,"出错了get_mode_name 11------- %d %d %s\n",mode,self->maxMachineMode,self->modeNames[mode]);
        gcc_unreachable ();
    }
    return self->modeNames[mode];
}

/**
 * 从 E_SImode 转成 PTX_SImode
 */
int   mtcs_gen_convert_host_mode_name(MtcsGen *self,char *hostModeName,char *deviceModeName)
{
    if(hostModeName==NULL || strlen(hostModeName)<=6)
        return -2;
    if(hostModeName[0]!='E' || hostModeName[1]!='_')
        return -2;

    char *hmn=hostModeName+2;//跳过E_
    int len=strlen(hmn)-4;//strlen("mode");
    int i;
    int destMode=-1;
    for(i=0;i<self->maxMachineMode;i++){
        if(!strncmp(self->modeNames[i],hmn,len)){
            destMode=i;
            break;
        }
    }
    if(destMode==-1){
        error("在%s中没有找到machine_mode :%s\n",self->platformName,hostModeName);
        return -1;
    }
    //从hostModeName找出mode
    int hostMode=-1;
    for(i=0;i<NUM_MACHINE_MODES;i++){
        if (!strncmp (GET_MODE_NAME (i), hmn,len) == 0){
            hostMode= i;
            break;
        }
    }
    if(hostMode!=destMode){
        fprintf(stderr,"主机 mode:%d 设备 mode:%d modeName:%s\n",hostMode,destMode,hostModeName);
    }
    sprintf(deviceModeName,"%s_%smode",self->platformNameUppercase,self->modeNames[destMode]);
    return 0;
}

static bool handleArg_cb(const char *arg,bool modifyMode)
{
    MtcsGen *self=mtcs_gen_get();
    if(strncmp(arg,"-W",2)==0){
        char *plat=arg+2;
        char *p;
        for (p = plat; *p != 0; p++)
             *p = TOLOWER (*p);
        fprintf(stderr,"mtcsgen.c --handle_arg 00 设置平台信息 %s %s  progname:%s\n",arg,plat,progname);
        setPlatform(self,plat,modifyMode);
        return true;
    }
    if(strncmp(arg,"-c",2)==0){
       //输出c文件
       char *path=arg+2;
       if(path && strlen(path)>0)
          self->cFile=xstrdup(path);
       else
          self->cFile=xstrdup("");
       return true;
    }
    if(strncmp(arg,"-h",2)==0){
          //输出h文件
       char *path=arg+2;
       if(path && strlen(path)>0)
          self->hFile=xstrdup(path);
       else
          self->hFile=xstrdup("");
       return true;
    }
    //生成文件的保存路径，由平台名+gen组成完整路径
    if(strncmp(arg,"-S",2)==0){
         fprintf(stderr,"mtcsgen--handle_arg 11 设置平台信息 %s  progname:%s\n",arg,progname);
         char *path=arg+2;
         setSaveRootPath(self,path);
         return true;
    }
    return false;
}

//需要重设machine_mode
bool  mtcs_gen_handle_arg (const char *arg)
{
   return handleArg_cb(arg,true);
}

//不需要重设machine_mode
bool  mtcs_gen_handle_arg_no_mode (const char *arg)
{
   return handleArg_cb(arg,false);
}

/* Nonzero if MODE is a vector mode.  */
//原型 #define VECTOR_MODE_P(MODE)             \
  (GET_MODE_CLASS (MODE) == MODE_VECTOR_BOOL        \
   || GET_MODE_CLASS (MODE) == MODE_VECTOR_INT      \
   || GET_MODE_CLASS (MODE) == MODE_VECTOR_FLOAT    \
   || GET_MODE_CLASS (MODE) == MODE_VECTOR_FRACT    \
   || GET_MODE_CLASS (MODE) == MODE_VECTOR_UFRACT   \
   || GET_MODE_CLASS (MODE) == MODE_VECTOR_ACCUM    \
   || GET_MODE_CLASS (MODE) == MODE_VECTOR_UACCUM)
bool mtcs_gen_is_vector_p(MtcsGen *self,int mode)
{
    unsigned char cl=self->modeClass[mode];
    return (cl==MODE_VECTOR_BOOL || cl==MODE_VECTOR_INT || cl==MODE_VECTOR_FLOAT || cl==MODE_VECTOR_FRACT ||
            cl==MODE_VECTOR_UFRACT || cl==MODE_VECTOR_ACCUM || cl==MODE_VECTOR_UACCUM);
}

int  mtcs_gen_get_inner(MtcsGen *self,int mode)
{
    return self->modeInner[mode];
}

poly_uint16 mtcs_gen_get_nunits (MtcsGen *self,int  mode)
{
    return self->modeNunits[mode];
}

//原型 GET_MODE_CLASS
unsigned char  mtcs_gen_get_class(MtcsGen *self,int mode)
{
    return self->modeClass[mode];
}

//替换mode 把 VOIDmode 替换为(machine_mode)PTX_VOIDmode ....
static void replaceMode_1(MtcsGen *self,NString *str,nboolean conv)
{
   char *convStr="(machine_mode)";
   if(!conv)
      convStr="";
   int i;

   char dest1[30];
   char dest2[30];
   char dest3[30];
   char dest4[30];
   char dest5[30];
   char dest6[30];
   char dest7[30];
   char dest8[30];

   char p1[30];
   char p2[30];
   char p3[30];
   char p4[30];
   char p5[30];
   char p6[30];
   char p7[30];
   char p8[30];

   char *mName;
   for(i=0;i<self->maxMachineMode+1;i++){
      //从PTX_BLKmode 获得 BLKmode
      if(i==self->maxMachineMode)
         mName="P";
      else
         mName=self->modeNames[i];//VOID,BLK,BI...
      sprintf(dest1,"\"%s%s_%smode",convStr,self->platformNameUppercase,mName);
      sprintf(dest2," %s%s_%smode,",convStr,self->platformNameUppercase,mName);
      sprintf(dest3,"(%s%s_%smode", convStr,self->platformNameUppercase,mName);
      sprintf(dest4," %s%s_%smode)",convStr,self->platformNameUppercase,mName);
      sprintf(dest5,"(%s%s_%smode)",convStr,self->platformNameUppercase,mName);
      sprintf(dest6," %s%s_%smode ",convStr,self->platformNameUppercase,mName);
      sprintf(dest7,",%s%s_%smode)",convStr,self->platformNameUppercase,mName);
      sprintf(dest8,",%s%s_%smode,",convStr,self->platformNameUppercase,mName);

      sprintf(p1,"\"%smode",mName);
      sprintf(p2," %smode,",mName);
      sprintf(p3,"(%smode",mName);
      sprintf(p4," %smode)",mName);
      sprintf(p5,"(%smode)",mName);
      sprintf(p6," %smode ",mName);
      sprintf(p7,",%smode)",mName);
      sprintf(p8,",%smode,",mName);

      n_string_replace(str,p1,dest1,-1);
      n_string_replace(str,p2,dest2,-1);
      n_string_replace(str,p3,dest3,-1);
      n_string_replace(str,p4,dest4,-1);
      n_string_replace(str,p5,dest5,-1);
      n_string_replace(str,p6,dest6,-1);
      n_string_replace(str,p7,dest7,-1);
      n_string_replace(str,p8,dest8,-1);

   }
}

static void replaceTARGET_SOFT_STACK(MtcsGen *self,NString *str)
{
   char *p1="TARGET_SOFT_STACK)";
   char *p2="(!TARGET_SOFT_STACK";
   char *rstr1="mtcs_options_target_soft_stack/*!TARGET_SOFT_STACK*/(mtcsTarget->mtcsOptions))";
   char *rstr2="(!mtcs_options_target_soft_stack/*!TARGET_SOFT_STACK*/(mtcsTarget->mtcsOptions)";
   n_string_replace(str,p1,rstr1,-1);
   n_string_replace(str,p2,rstr2,-1);

}

//替换E_Pmode、E_SImode...为 PTX_Pmode PTX_SImode
static void replaceMode_2(MtcsGen *self,NString *str)
{
   int i;
   char dest1[30];
   char p1[30];
   char *mName;
   for(i=0;i<self->maxMachineMode+1;i++){
      //从PTX_BLKmode 获得 BLKmode
      if(i==self->maxMachineMode)
         mName="P";
      else
         mName=self->modeNames[i];//VOID,BLK,BI...
      sprintf(dest1,"(machine_mode)%s_%smode",self->platformNameUppercase,mName);
      sprintf(p1,"E_%smode",mName);
      n_string_replace(str,p1,dest1,-1);
   }
}

static NString *getFileName(FILE *fd,char **destFile)
{
   int handle = fileno(fd);
   char filepath[256];
   char fileName[512];

   snprintf(filepath, sizeof(filepath), "/proc/self/fd/%d", handle);
   ssize_t len = readlink(filepath, fileName, sizeof(fileName) - 1);
   fileName[len]='\0';
   if(strncmp(fileName,"/dev",4)==0)
      return NULL;
   fclose(fd);
   struct stat sb;
   int rv=0;
   if (stat(fileName, &sb) == 0){
      rv = sb.st_size;
   }else{
      return NULL;
   }
   NString *strs=n_string_sized_new(rv+1);
   FILE *f=fopen(fileName,"r");
   int ret=fread(strs->str,1,rv+1,f);
   fclose(f);
   if(ret!=rv){
      n_string_free(strs,TRUE);
      return NULL;
   }
   fprintf(stderr,"fileName is :%s %s size:%d\n",filepath,fileName,rv);
   strs->str[rv]='\0';
   strs->len=rv;
   *destFile=xstrdup(fileName);
   return strs;
}

/**
 * 替换出现在ptx-insn-emit.c中的所有mode ,加前缀 (machine_mode)PTX_
 */
int mtcs_gen_replace_mode_for_emit(MtcsGen *self,NString *src)
{
   replaceMode_1(self,src,TRUE);
   replaceTARGET_SOFT_STACK(self,src);
   return 0;
}

/**
 * 替换xxx-insn-flags.h中的
 * Pmode、SImode.... --> PTX_Pmode、PTX_SImode...
 */
int mtcs_gen_replace_mode_for_flags_file(MtcsGen *self,FILE *fd)
{
   char *destFile=NULL;
   NString *strs=getFileName(fd,&destFile);
   if(strs==NULL)
      return -1;
   replaceMode_1(self,strs,FALSE);
   FILE *f=fopen(destFile,"w");
   fwrite(strs->str,1,strs->len,f);
   fclose(f);
   return 0;
}

/**
 * 替换xxx-insn-recog.c中的mode TARGET_SOFT_STACK
 */
int mtcs_gen_replace_recog(MtcsGen *self,NString *strs)
{
   replaceMode_2(self,strs);
   replaceMode_1(self,strs,TRUE);
   replaceTARGET_SOFT_STACK(self,strs);
   return 0;
}

//代替 rtx_reader_ptr->print_c_condition (f, exp);
void mtcs_gen_print_c_condition(MtcsGen *self,NString *outFile,char *exp)
{
   int pid=getpid();
   char fileName[256];
   sprintf(fileName,"/dev/shm/temp_-#$_%d",pid);
   FILE *f=fopen(fileName,"w");
   rtx_reader_ptr->print_c_condition (f, exp);
   fclose(f);
   char buffer[1024];
   FILE *op=fopen(fileName,"r");
   int rev=fread(buffer,1,1024,op);
   fclose(op);
   buffer[rev]='\0';
   remove(fileName);
   n_string_append(outFile,buffer);
}

//在gensupport.cc中定义的保留谓词
static char *reservePreds[]={
        {"general_operand"},
        {"address_operand"},
        {"register_operand"},
        {"pmode_register_operand"},
        {"scratch_operand"},
        {"immediate_operand"},
        {"const_int_operand"},
        {"const_scalar_int_operand"},
        {"const_double_operand"},
        {"nonimmediate_operand"},
        {"nonmemory_operand"},
        {"push_operand"},
        {"pop_operand"},
        {"memory_operand"},
        {"indirect_operand"},
        {"ordered_comparison_operator"},
        {"comparison_operator"},
        {NULL}
};

nboolean mtcs_gen_is_reserve_preds(MtcsGen *self,char *predsName)
{
   int i=0;
   char *item;
   while((item=reservePreds[i++])!=NULL){
      if(strcmp(item,predsName)==0)
         return TRUE;
   }
   return 0;
}

void mtcs_gen_replace_common_preds(MtcsGen *self,NString *src)
{
   int i=0;
   char *item;
   while((item=reservePreds[i++])!=NULL){
      char find[128];
      char replace[256];
      sprintf(find," %s (",item);//模式1
      sprintf(replace," mtcs_preds_%s/*!%s*/(mtcsTarget->mtcsPreds,",item,item);
      n_string_replace(src,find,replace,-1);
      sprintf(find,"!%s (",item);//模式2
      sprintf(replace,"!mtcs_preds_%s/*!%s*/(mtcsTarget->mtcsPreds,",item,item);
      n_string_replace(src,find,replace,-1);
      sprintf(find,"(%s (",item);//模式3
      sprintf(replace,"(mtcs_preds_%s/*!%s*/(mtcsTarget->mtcsPreds,",item,item);
      n_string_replace(src,find,replace,-1);

   }
}


/**
 * 替换平台谓词 加前缀 ptx_
 register_operand
 register_or_complex_di_df_register_operand
 const0_operand (rtx, machine_mode);
 */
void mtcs_gen_replace_plat_preds(MtcsGen *self,NString *src)
{
   const char *platName=mtcs_gen_get_platform_name(self);
   struct pred_data *p;
   FOR_ALL_PREDICATES (p){
      if(!mtcs_gen_is_reserve_preds(self,p->name)){
         char replace[512];
         sprintf(replace,"%s_%s",platName,p->name);
         n_string_replace(src,p->name,replace,-1);
      }
   }
}

MtcsGen *mtcs_gen_get()
{
    static MtcsGen *singleton = NULL;
    if (!singleton){
         singleton =xmalloc (sizeof(MtcsGen));
         mtcsGenInit(singleton);
    }
    return singleton;
}
