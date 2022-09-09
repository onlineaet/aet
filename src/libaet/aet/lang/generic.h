

#ifndef __AET_GENERIC_H__
#define __AET_GENERIC_H__

#include <alib.h>

typedef enum {
	GENERIC_TYPE_CHAR,
	GENERIC_TYPE_SIGNED_CHAR,
	GENERIC_TYPE_UCHAR,
	GENERIC_TYPE_SHORT,
	GENERIC_TYPE_USHORT,
	GENERIC_TYPE_INT,
	GENERIC_TYPE_UINT,
	GENERIC_TYPE_LONG,
	GENERIC_TYPE_ULONG,
	GENERIC_TYPE_LONG_LONG,
	GENERIC_TYPE_ULONG_LONG,
	GENERIC_TYPE_FLOAT,
	GENERIC_TYPE_DOUBLE,
	GENERIC_TYPE_LONG_DOUBLE,
	GENERIC_TYPE_DECIMAL32_FLOAT,
	GENERIC_TYPE_DECIMAL64_FLOAT,
	GENERIC_TYPE_DECIMAL128_FLOAT,
	GENERIC_TYPE_COMPLEX_CHAR,
	GENERIC_TYPE_COMPLEX_UCHAR,
	GENERIC_TYPE_COMPLEX_SHORT,
	GENERIC_TYPE_COMPLEX_USHORT,
	GENERIC_TYPE_COMPLEX_INT,
	GENERIC_TYPE_COMPLEX_UINT,
	GENERIC_TYPE_COMPLEX_LONG,
	GENERIC_TYPE_COMPLEX_ULONG,
	GENERIC_TYPE_COMPLEX_LONG_LONG,
	GENERIC_TYPE_COMPLEX_ULONG_LONG,
	GENERIC_TYPE_COMPLEX_FLOAT,
	GENERIC_TYPE_COMPLEX_DOUBLE,
	GENERIC_TYPE_COMPLEX_LONG_DOUBLE,
	GENERIC_TYPE_FIXED_POINT,
	GENERIC_TYPE_ENUMERAL,
	GENERIC_TYPE_BOOLEAN,
	GENERIC_TYPE_STRUCT,
	GENERIC_TYPE_UNION,
	GENERIC_TYPE_CLASS,
	GENERIC_TYPE_UNKNOWN=-1,
}GenericType;

typedef struct _aet_generic_info{
	char typeName[100];
	char genericName;
	char type;
    char pointerCount;
	int  size;
}aet_generic_info;


typedef void  (*_setGenericBlockFunc_123_typedef)     (void *self,void *object,char *objectName,int pointerCount);

typedef struct _FuncGenBlockInfo FuncGenBlockInfo;
struct _FuncGenBlockInfo
{
	struct blockInfoData{
		char sysName[100];
		void *blockFuncsPointer[30];
	    int count;
	}blockFuncs[20];//原来10个太少引起不少问题
	int countOfBlockClass;
	char from[255];
};

typedef struct _FuncGenParmInfo FuncGenParmInfo;
struct _FuncGenParmInfo
{
	aet_generic_info pms[10];
	int count;
	FuncGenBlockInfo *globalBlockInfo;
	char excute;
	void *_setGenericBlockFunc_123_varname;
};

typedef struct _FuncGenBlockList FuncGenBlockList;
struct _FuncGenBlockList
{
	FuncGenBlockInfo *data;
	FuncGenBlockList *next;
};

static inline int get_block_func_pointer_from_info(FuncGenBlockInfo *blockInfo,int index,const char *sysName,void *selfFunc)
{
	int i;
	for(i=0;i<blockInfo->countOfBlockClass;i++){
		  if(!memcmp(sysName,blockInfo->blockFuncs[i].sysName,strlen(sysName))){
			 if(blockInfo->blockFuncs[i].blockFuncsPointer[index-1]==selfFunc){
				  return -1;
			 }
			 return i;
		  }
	}
	return -2;
}

static inline FuncGenParmInfo createFuncGenParmInfo(FuncGenBlockInfo *blockInfo,void *setblockFunc)
{
	FuncGenParmInfo info;
	info.count=0;
	info.globalBlockInfo=blockInfo;
	info.excute=0;
	info._setGenericBlockFunc_123_varname=setblockFunc;
	return info;
}

#define generic_is_pointer(x) \
({ \
	aet_generic_info info=generic_info$(x); \
	int result=0; \
	if(info.pointerCount==-1) \
	    result= 0; \
	else \
	    result= info.pointerCount; \
    result;\
  })


#define generic_type(x) \
({ \
	aet_generic_info info=generic_info$(x); \
	GenericType result=info.type; \
    result;\
  })

#define generic_type_obj(obj,x) \
({ \
	aet_generic_info info=generic_info$(obj,x); \
	GenericType result=info.type; \
    result;\
  })



#endif /* ! GCC_C_AET_H */
