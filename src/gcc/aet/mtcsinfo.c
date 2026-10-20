#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "stringpool.h"
#include "attribs.h"
#include "diagnostic-core.h"

#include "mtcsinfo.h"
#include "aetmicro.h"
#include "aetprinttree.h"

/**
 *
 * 先判断 MTCS_FUNC_DEVICE_HOST再到 MTCS_FUNC_DEVICE
 * 因为源代码中     void    __host__ __device__ getData(float data){
 * 有 __device__ 会返回的是 MTCS_FUNC_DEVICE
 */
MtcsFuncType mtcs_info_get_func_type(tree decl)
{
   tree g1 = lookup_attribute (MTCS_GLOBAL_STR, DECL_ATTRIBUTES (decl));
   tree g2 = lookup_attribute (MTCS_DEVICE_STR, DECL_ATTRIBUTES (decl));
   tree g3 = lookup_attribute (MTCS_HOST_STR, DECL_ATTRIBUTES (decl));
   if(g1)
      return MTCS_FUNC_KERNEL;//这是一个核函数

   if(g2){
      if(g3)
         return MTCS_FUNC_DEVICE_HOST;//即是设备函数也是主机函数
      else
         return MTCS_FUNC_DEVICE;
   }
   return MTCS_FUNC_NOT;
}

MtcsFuncType mtcs_info_get_and_check_func_type(tree decl,location_t loc)
{
   if(!decl)
      return MTCS_FUNC_NOT;
   tree g1 = lookup_attribute (MTCS_GLOBAL_STR, DECL_ATTRIBUTES (decl));
   tree g2 = lookup_attribute (MTCS_DEVICE_STR, DECL_ATTRIBUTES (decl));
   tree g3 = lookup_attribute (MTCS_HOST_STR, DECL_ATTRIBUTES (decl));
   if(g1){
      if(g2){
         error_at(loc,"核函数不能定义为设备函数。111");
      }
      if(g3){
         error_at(loc,"核函数不能定义为主机函数。222");
      }
      return MTCS_FUNC_KERNEL;//这是一个核函数
   }

   if(g2){
      if(g3){
         return MTCS_FUNC_DEVICE_HOST;//即是设备函数也是主机函数
      }else{
         return MTCS_FUNC_DEVICE;
      }
   }
   return MTCS_FUNC_NOT;
}

MtcsVarType mtcs_info_get_var_type(tree decl)
{
   if(!decl || !VAR_P(decl))
      return MTCS_VAR_NOT;
   tree sharedAtt = lookup_attribute (MTCS_SHARED_STR, DECL_ATTRIBUTES (decl));
   tree managedAtt = lookup_attribute (MTCS_MANAGED_STR, DECL_ATTRIBUTES (decl));
   tree constantAtt = lookup_attribute (MTCS_CONSTANT_STR, DECL_ATTRIBUTES (decl));
   tree deviceAtt = lookup_attribute (MTCS_DEVICE_STR, DECL_ATTRIBUTES (decl));

   if(sharedAtt)
      return MTCS_VAR_SHARED;
   if(managedAtt)
      return MTCS_VAR_MANAGED;
   if(constantAtt)
      return MTCS_VAR_CONSTANT;
   if(deviceAtt)
      return MTCS_VAR_DEVICE;
   return MTCS_VAR_NOT;
}

MtcsVarType  mtcs_info_get_and_check_var_type(tree decl,location_t loc)
{
   nboolean g1 = lookup_attribute (MTCS_SHARED_STR, DECL_ATTRIBUTES (decl));
   nboolean g2 = lookup_attribute (MTCS_MANAGED_STR, DECL_ATTRIBUTES (decl));
   nboolean g3 = lookup_attribute (MTCS_CONSTANT_STR, DECL_ATTRIBUTES (decl));
   nboolean g4 = lookup_attribute (MTCS_GLOBAL_STR, DECL_ATTRIBUTES (decl));
   nboolean g5 = lookup_attribute (MTCS_HOST_STR, DECL_ATTRIBUTES (decl));
   nboolean g6 = lookup_attribute (MTCS_DEVICE_STR, DECL_ATTRIBUTES (decl));

   char *varName = IDENTIFIER_POINTER(DECL_NAME(decl));
   //fprintf(stderr,"getMtcsVarType 00 %s g1:%d g2:%d g3:%d g4:%d g5:%d\n",varName,g1,g2,g3,g4,g5);
   if(g4 || g5){
      error_at(loc,"变量 %s,不能用 __global__ 或 __host__ 修饰。",varName);
      return MTCS_VAR_NOT;
   }

   if(g1){
      if(g2 || g3 || g6){
         error_at(loc,"变量 __share__ %s,不能用 __managed__ 、 __constant__、 __device__ 修饰。",varName);
      }
      return MTCS_VAR_SHARED;
   }

   if(g2){
      if(g1 || g3){
         error_at(loc,"变量 __managed__ %s,不能用 __shared__ 或 __constant__ 修饰。",varName);
      }
      return MTCS_VAR_MANAGED;
   }

   if(g3){
      if(g1 || g2){
         error_at(loc,"变量 __constant__ %s,不能用 __shared__ 或 __managed__ 修饰。",varName);
      }
      return MTCS_VAR_CONSTANT;
   }
   if(g6){
        if(g1 || g4){
           error_at(loc,"变量 __device__ %s,不能用 __shared__ 或 __global__ 修饰。",varName);
        }
        return MTCS_VAR_DEVICE;
     }
   return MTCS_VAR_NOT;
}

/**
 * 是不是平台字符串
 */
MtcsPlatformType     mtcs_info_get_platform(char *ident)
{
   if(ident==NULL)
      return MTCS_PLAT_UNKNOWN;
   MtcsPlatformType type=MTCS_PLAT_UNKNOWN;
   char *down=n_ascii_strdown(ident,strlen(ident));
   char quotation1[20];
   char quotation2[20];
   char quotation3[20];
   sprintf(quotation1,"\"%s\"",MTCS_CUDA_PLAT_STRING);
   sprintf(quotation2,"\"%s\"",MTCS_GCN_PLAT_STRING);
   sprintf(quotation3,"\"%s\"",MTCS_SPIRV_PLAT_STRING);
   if(!strcmp(down,MTCS_CUDA_PLAT_STRING) || !strcmp(down,quotation1))
      type=MTCS_PLAT_CUDA;
   else if(!strcmp(down,MTCS_GCN_PLAT_STRING) || !strcmp(down,quotation2))
      type=MTCS_PLAT_GCN;
   else if(!strcmp(down,MTCS_SPIRV_PLAT_STRING) || !strcmp(down,quotation3) )
      type=MTCS_PLAT_SPIRV;
   free(down);
   return type;
}

//创建设备函数指针变量名 deviceFuncPointers
void mtcs_info_create_device_func_pointer_var_name(char *buffer,char *sysName)
{
    sprintf(buffer,"_%s_%s",sysName,AET_MTCS_DEVICE_FUNC_POINTERS_VAR_NAME);
}

nboolean     mtcs_info_is_visible(tree decl)
{
   tree visible = lookup_attribute (MTCS_FN_VISIBLE, DECL_ATTRIBUTES (decl));
   return visible!=NULL;
}

tree mtcs_info_create_kernel_name(char *orig)
{
   char newName[512];
   sprintf(newName,"mtcs_kernel_%s",orig);
   return get_identifier(newName);
}

//函数声明是不是内部函数
nboolean    mtcs_info_is_internal_fn(tree fndecl)
{
   if(!fndecl)
      return FALSE;
   tree_function_decl &fn = FUNCTION_DECL_CHECK (fndecl)->function_decl;
   return (fn.built_in_class ==NOT_BUILT_IN
          && fn.function_code>=MTCS_INTERNAL_FN_CODE_START
          && fn.function_code<= MTCS_INTERNAL_FN_CODE_END);
}

//函数声明是不是内建函数
nboolean    mtcs_info_is_builtin_fn(tree fndecl)
{
   if(!fndecl)
      return FALSE;
   tree_function_decl &fn = FUNCTION_DECL_CHECK (fndecl)->function_decl;
   return (fn.built_in_class ==NOT_BUILT_IN
         && fn.function_code>=MTCS_BUILTIN_FN_CODE_START
         && fn.function_code<= MTCS_BUILTIN_FN_CODE_END);
}

/**
 * intrinsic 函数声明
 */
nboolean mtcs_info_is_intrinsic_function_decl(tree fndecl)
{
   if(!fndecl)
      return FALSE;
   tree intrinsic=lookup_attribute (MTCS_INTRINSIC_FUNCTION_DECL, DECL_ATTRIBUTES (fndecl));
   if(!intrinsic)
      return FALSE;
   return TRUE;
}

/**
 * intrinsic 函数实现
 */
nboolean mtcs_info_is_intrinsic_function_impl(tree fndecl)
{
   if(!fndecl)
      return FALSE;
   tree intrinsic=lookup_attribute (MTCS_INTRINSIC_FUNCTION_IMPLEMENTS, DECL_ATTRIBUTES (fndecl));
   if(!intrinsic)
      return FALSE;
   return TRUE;
}

static char *getIntrinsicImpl(tree fndecl,int index)
{
   if(!fndecl)
      return NULL;
   tree intrinsic=lookup_attribute (MTCS_INTRINSIC_FUNCTION_IMPLEMENTS, DECL_ATTRIBUTES (fndecl));
   if(!intrinsic)
      return NULL;
   int idx = 0;
   for (tree a = TREE_VALUE (intrinsic); a; a = TREE_CHAIN (a)){
      tree d = TREE_VALUE (a);
      char *name =NULL;
      if(TREE_CODE(d)==IDENTIFIER_NODE)
         name =IDENTIFIER_POINTER(d);
      else
         name =TREE_STRING_POINTER (d);
      if(idx==index)
         return name;
      idx++;
   }
   return NULL;
}

//参见 __attribute__((mii("cuda","sm_35")))  0=cuda 1=sm_35
char *mtcs_info_get_intrinsic_target(tree fndecl)
{
   return getIntrinsicImpl(fndecl,0);
}

char *mtcs_info_get_intrinsic_isa(tree fndecl)
{
   return getIntrinsicImpl(fndecl,1);
}

