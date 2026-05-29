#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "backend.h"
#include "target.h"
#include "rtl.h"
#include "tree.h"
#include "stringpool.h"
#include "attribs.h"

#include "mtcsinfo.h"
#include "aetmicro.h"

/**
 *
 * 先判断 MTCS_FUNC_DEVICE_HOST再到 MTCS_FUNC_DEVICE
 * 因为源代码中     void    __host__ __device__ getData(float data){
 * 有 __device__ 会返回的是 MTCS_FUNC_DEVICE
 */
MtcsFuncType mtcs_info_get_func_type(tree decl)
{
   tree kernelAtt = lookup_attribute (MTCS_GLOBAL_STRING, DECL_ATTRIBUTES (decl));
   tree deviceAtt = lookup_attribute (MTCS_DEVICE_STRING, DECL_ATTRIBUTES (decl));
   tree deviceHostAtt = lookup_attribute (MTCS_DEVICE_HOST_STRING, DECL_ATTRIBUTES (decl));
   if(kernelAtt)
      return MTCS_FUNC_KERNEL;
   if(deviceHostAtt)
      return MTCS_FUNC_DEVICE_HOST;
   if(deviceAtt)
      return MTCS_FUNC_DEVICE;

   return MTCS_FUNC_NOT;
}

MtcsVarType mtcs_info_get_var_type(tree decl)
{
   if(!VAR_P(decl))
      return MTCS_VAR_NOT;
   tree sharedAtt = lookup_attribute (MTCS_SHARED_STRING, DECL_ATTRIBUTES (decl));
   tree managedAtt = lookup_attribute (MTCS_MANAGED_STRING, DECL_ATTRIBUTES (decl));
   tree constantAtt = lookup_attribute (MTCS_CONSTANT_STRING, DECL_ATTRIBUTES (decl));
   tree deviceAtt = lookup_attribute (MTCS_DEVICE_STRING, DECL_ATTRIBUTES (decl));

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
