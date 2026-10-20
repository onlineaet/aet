#ifndef __GCC_MTCS_INFO_H__
#define __GCC_MTCS_INFO_H__

#include "nlib.h"

#define MTCS_DEVICE_HOST_STRING "device_host"
#define MTCS_FN_VISIBLE "mtcs visible"

#define MTCS_GLOBAL_STR "global"
#define MTCS_DEVICE_STR "device"
#define MTCS_HOST_STR   "host"
#define MTCS_SHARED_STR   "shared"
#define MTCS_MANAGED_STR  "managed"
#define MTCS_CONSTANT_STR "constant"


#define MTCS_INTERNAL_FN_CODE_START 10000
#define MTCS_INTERNAL_FN_CODE_END   (MTCS_INTERNAL_FN_CODE_START+100)

#define MTCS_BUILTIN_FN_CODE_START 2000
#define MTCS_BUILTIN_FN_CODE_END   (MTCS_BUILTIN_FN_CODE_START+2000)

typedef enum{
   INTERNEL_CODE_DIM = MTCS_INTERNAL_FN_CODE_START,
   INTERNAL_CODE_SHARED_VAR,
}InternalFnCode;

typedef enum{
   BUILTIN_CLASS_SYNC,
}MtcsBuiltinClass;

  //第一个参数 表示区域 0 = matDim=gridDim 1 = unitDim=blockDim 2=unitIdex=blockIdx 3=threadIdx
typedef enum{
   MAT_DIM,
   UNIT_DIM,
   UNIT_IDX,
   THREAD_IDX
}MtcsRegion;

#define MTCS_CUDA_PLAT_STRING  "cuda"
#define MTCS_GCN_PLAT_STRING   "gcn"
#define MTCS_SPIRV_PLAT_STRING "spirv"

//平台类型
typedef enum{
   MTCS_PLAT_DEFAULT,
   MTCS_PLAT_CUDA,
   MTCS_PLAT_GCN,
   MTCS_PLAT_SPIRV,
   MTCS_PLAT_UNKNOWN
}MtcsPlatformType;

void mtcs_info_print_node();

//mtcs函数类型
typedef enum{
    MTCS_FUNC_KERNEL,
    MTCS_FUNC_DEVICE,
    MTCS_FUNC_DEVICE_HOST,
    MTCS_FUNC_NOT = -1,
}MtcsFuncType;

//mtcs变量类型
typedef enum{
    MTCS_VAR_SHARED,
    MTCS_VAR_MANAGED,
    MTCS_VAR_CONSTANT,
    MTCS_VAR_DEVICE,
    MTCS_VAR_NOT = -1,
}MtcsVarType;

inline const char *mtcs_info_get_mtcs_type_string(MtcsFuncType type)
{
   if(type==MTCS_FUNC_KERNEL)
      return MTCS_GLOBAL_STR;
   else if(type==MTCS_FUNC_DEVICE)
      return MTCS_DEVICE_STR;
   else if(type==MTCS_FUNC_DEVICE_HOST)
      return MTCS_DEVICE_HOST_STRING;
   else
      gcc_unreachable();
}


MtcsFuncType mtcs_info_get_func_type(tree decl);
MtcsVarType  mtcs_info_get_var_type(tree decl);
MtcsVarType  mtcs_info_get_and_check_var_type(tree decl,location_t loc);
MtcsFuncType mtcs_info_get_and_check_func_type(tree decl,location_t loc);

//函数声明是不是内部函数
nboolean    mtcs_info_is_internal_fn(tree fndecl);
//函数声明是不是内建函数
nboolean    mtcs_info_is_builtin_fn(tree fndecl);

nboolean     mtcs_info_is_visible(tree decl);
tree         mtcs_info_create_kernel_name(char *orig);
//下面4个函数是intrinsic需要的
nboolean     mtcs_info_is_intrinsic_function_decl(tree fndecl);
// intrinsic 函数实现
nboolean     mtcs_info_is_intrinsic_function_impl(tree fndecl);
char        *mtcs_info_get_intrinsic_target(tree fndecl);
char        *mtcs_info_get_intrinsic_isa(tree fndecl);

static inline void  mtcs_info_create_kernel_name(char *orig,char *name)
{
   sprintf(name,"mtcs_kernel_%s",orig);
}

MtcsPlatformType     mtcs_info_get_platform(char *ident);

void mtcs_info_create_device_func_pointer_var_name(char *buffer,char *sysName);

static inline void mtcs_info_create_host_device_peer_name(char *buffer,char *origName)
{
      sprintf(buffer,"%s_device",origName);
}



#endif
