/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

This file is part of AET.

AET is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 3, or (at your option) any later
version.

AET is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC Exception along with this program; see the file COPYING3.
If not see <http://www.gnu.org/licenses/>.
AET was originally developed  by the zclei@sina.com at guiyang china .
*/


#include "config.h"
#include <cstdio>
#define INCLUDE_UNIQUE_PTR
#define INCLUDE_MEMORY
#include "system.h"
#include "coretypes.h"
#include "target.h"

#include "function.h"
#include "tree.h"
#include "timevar.h"
#include "stringpool.h"
#include "cgraph.h"
#include "attribs.h"
#include "toplev.h"

#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "c-family/c-common.h"
#include "gimple-expr.h"

#include "c/c-tree.h"

#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "c-aet.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "c/gimple-parser.h"

#include "../libcpp/include/cpplib.h"
#include "aet-c-parser-header.h"

#include "aetutils.h"
#include "classmgr.h"
#include "supercall.h"
#include "aetinfo.h"
#include "c-aet.h"
#include "aetprinttree.h"
#include "classfunc.h"
#include "funcmgr.h"
#include "classinfo.h"
#include "aetprinttoken.h"
#include "classimpl.h"
#include "mtcsparser.h"


typedef struct _FieldData{
    ClassFunc *func;
    char *sysName;
}FieldData;

typedef struct _RecordErrorInfo
{
   char *file;
   char *atFunc;
   char *call;
   int  line;
   int column;
   char errInfo[512];
   int element;
   ClassFunc *callFunc;
}RecordErrorInfo;



static void superCallInit(SuperCall *self)
{
   self->fieldsTable=n_hash_table_new(n_str_hash,n_str_equal);
   self->recordErrorTable=n_hash_table_new(n_str_hash,n_str_equal);
   self->parentDeviceDeclArray=n_ptr_array_new();
}

static void addErrorInfo(SuperCall *self,ClassName *className,location_t loc,
      ClassFunc *func,int element,ClassName *parent)
{
   NPtrArray *array = n_hash_table_lookup(self->recordErrorTable,className->sysName);
   if(array==NULL){
      array=n_ptr_array_new();
      n_hash_table_insert(self->recordErrorTable,className->sysName,array);
   }
   int i;
   for(i=0;i<array->len;i++){
      RecordErrorInfo *item=n_ptr_array_index(array,i);
      if(item->element==element)
         return;
   }
   RecordErrorInfo *info=n_slice_new0(RecordErrorInfo);
   expanded_location xloc;
   xloc = expand_location(loc);
   info->file = xloc.file;
   info->atFunc = IDENTIFIER_POINTER(DECL_NAME(current_function_decl));
   info->call = func->orgiName;
   info->line = xloc.line;
   info->column = xloc.column;
   info->element = element;//重要，在数组中的位置。
   info->callFunc= func;
   char atClass[256];
   char origName[256];
   int len=func_mgr_get_orig_func_and_class_name(func_mgr_get(), info->atFunc,atClass,origName);

   sprintf(info->errInfo,"%s 第%d行，%d列 类%s中函数%s调用 super$->%s 出错，因为父类%s没有实现。",
         info->file,info->line,info->column,className->userName,len>0?origName:info->atFunc,info->call,parent->userName);
   n_ptr_array_add(array,info);
}

/**
 * 获取类中可以由子类通过super调用的类函数数量
 * 不用考虑接口，因为接口的方法已复制到类中来了。
 */
static int getFieldCount(SuperCall *self,ClassName *className,NPtrArray *fields)
{
   NPtrArray    *funcArray=func_mgr_get_funcs(func_mgr_get(),className);
   int i;
   int count=0;
   if(funcArray!=NULL){
      for(i=0;i<funcArray->len;i++){
         ClassFunc *func=n_ptr_array_index(funcArray,i);
         nboolean  canSuper= class_func_have_super(func);
         if(!canSuper)
            continue;
         if(aet_utils_valid_tree(func->fieldDecl) && !class_func_is_private(func)){
            FieldData *item=n_slice_new(FieldData);
            item->sysName=n_strdup(className->sysName);
            item->func=func;
            n_ptr_array_add(fields,item);
         }
      }
   }
   return count;
}

/**
 *AObjec
 * ^
 * |
 * AIData
 * ^
 * |
 * FaceData
 * 获取从AObject到FaceData的类信息。
 * 再通过类信息获取类的函数。
 */
static NPtrArray *createFields(SuperCall *self,ClassName *className)
{
   //从AObject到className的所有类中声明的方法。
   NPtrArray *fieldArray=n_ptr_array_new();
   ClassInfo *infos[30];
   //取从AObject到 className的类信息 (从小到大)
   int count=class_mgr_get_class_info_asc(class_mgr_get(),className,infos);
   int i;
   for(i=0;i<count;i++){
      getFieldCount(self,&infos[i]->className,fieldArray);//包含接口的方法
   }
   return fieldArray;
}

static void printFields( NPtrArray *fieldArray,ClassName *className)
{
   int i;
   printf("supercall.c printFields 类声明信息:%s\n",className->sysName);
   for(i=0;i<fieldArray->len;i++){
      FieldData *item = n_ptr_array_index(fieldArray,i);
      printf("\t%d\t%s\t\t%s\t%s\n",i,item->func->mangleFunName,item->sysName,item->func->className->sysName);
   }
}

static NPtrArray *getFields(SuperCall *self,ClassName *className)
{
   NPtrArray *fieldArray = n_hash_table_lookup(self->fieldsTable,className->sysName);
   if(fieldArray==NULL){
      fieldArray = createFields(self,className);
      n_hash_table_insert(self->fieldsTable,n_strdup(className->sysName),fieldArray);
      if(n_log_is_debug_file(NULL,NULL))
        printFields(fieldArray,className);
   }
   return fieldArray;
}

/*
*AObjec
* ^
* |
* AIData
* ^
* |
* FaceData
* FaceData的_superAddressArray[0]存入的是FaceData的类函数实现
*/
static ClassFunc *getDefine(ClassFunc *func,NPtrArray *defines)
{
     int i;
     for(i=0;i<defines->len;i++){
        ClassFunc *defineFunc=n_ptr_array_index(defines,i);
        if(aet_utils_valid_tree(defineFunc->fromImplDefine) && !strcmp(func->rawMangleName,defineFunc->rawMangleName))
           return defineFunc;
     }
     return NULL;
}

static int find(NPtrArray *fields,ClassName *from,int start,char *rawMangleName)
{
    int i;
    for(i=start;i>=0;i--){
       FieldData *item = n_ptr_array_index(fields,i);
       if(!strcmp(item->sysName,from->sysName))
              continue;
       if(!strcmp(item->func->rawMangleName,rawMangleName)){
          //查到了声明,如果不是抽象类就取这个位置
          //如果是抽象类，抽象类到from的第一个普通类一定实现了该方法。
          return i;
       }
    }
    return -1;
}

/////////////////以下是对super$的解析----------------------/
/**
 * 把 super$转成表达式 ((AObject*)self)
 */
static tree superAtPostfixExpression(SuperCall *self,ClassName *className,location_t  loc)
{
   c_parser *parser=self->parser->parser;
   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   if(info==NULL){
      c_parser_error (parser, "找不到类。");
      return error_mark_node;
   }
   char *sysName=info->parentName.sysName;
   if(sysName==NULL){
      c_parser_error (parser, "找不到父类！");
      return error_mark_node;
   }
   ClassInfo *parentInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),&info->parentName);
   if(parentInfo==NULL){
      c_parser_error (parser, "找不到父类。");
      return error_mark_node;
   }
   tree parmOrVar=lookup_name(aet_utils_create_ident("self"));
   tree recordId=aet_utils_create_ident(sysName);
   tree record=lookup_name(recordId);
   if(!record || record==NULL_TREE || record==error_mark_node){
      printf("没有找到 %s\n",sysName);
      error("没有找到 class");
   }
   tree recordType=TREE_TYPE(record);
   tree type=build_pointer_type(recordType);
   tree castParent = build1 (NOP_EXPR, type, parmOrVar);
   protected_set_expr_location (castParent, loc);
   AET_LANG_FLAG_5(castParent)=1;//重要 表示这是super$->调用。class_ref_build_deref 会用到。
   return castParent;
}

/**
 * 解析super$
 * 两种形式
 * 1.super$->func()
 * 2.int value = super->func (属于 postfix形式)
 * super_call_parser_at_postfix_expression 解析第二种。用的是生成tree的方法
 * 第一种情况，会进入 c_parser_statement_after_labels 然后调用 class_impl_parser_super 处理构造函数中调用super$(),
 * 调用父函数留到c_parser_expression-->c_parser_expr_no_commas-->c_parser_postfix_expression
 * 与第二种情况相同了
 */
tree  super_call_parser_at_postfix_expression(SuperCall *self,ClassName *className)
{
   c_parser *parser=self->parser->parser;
   if(!self->parser->isAet){
      c_parser_error (parser, "super关键字只能用在类的实现中。");
      return error_mark_node;
   }
   location_t  loc = c_parser_peek_token (parser)->location;
   c_parser_consume_token (parser);//consume super$
   if (c_parser_next_token_is (parser, CPP_DEREF)){
      tree result=superAtPostfixExpression(self,className,loc);
      return result;
   }else if(c_parser_next_token_is (parser, CPP_OPEN_PAREN)){
      //如果出现super(但不是第一条句时错的
      c_parser_error (parser, "类函数中不能有super$()语句。");
   }else{
      c_parser_error (parser, "super后只能是->或(符号。");
   }
   return error_mark_node;
}

/**
 * _superAddressArray存的是地址，作为函数所在地址，调用时要有函数声明。
 */
static tree createTypeDecl(ClassName *atSysName,ClassFunc*primitive)
{
   char fname[255];
   sprintf(fname,"_%s_%s",atSysName->sysName,primitive->rawMangleName);
   tree decl = lookup_name(get_identifier(fname));
   if(decl)
      return decl;
   tree fntype=TREE_TYPE(primitive->fieldDecl);
   fntype=TREE_TYPE(fntype);
   tree pointer=build_pointer_type(fntype);
   tree typeNameId=aet_utils_create_ident(fname);
   decl = build_decl (input_location, TYPE_DECL, typeNameId, pointer);
   DECL_ARTIFICIAL (decl) = 1;
   DECL_CONTEXT(decl)=NULL_TREE;
   DECL_EXTERNAL(decl)=0;
   TREE_STATIC(decl)=0;
   TREE_PUBLIC(decl)=0;
   set_underlying_type (decl);
   record_locally_defined_typedef (decl);
   c_c_decl_bind_file_scope(decl);//放在file_scope，c_c_decl_bind_file_scope是增加的,原本没有
   finish_decl (decl, input_location, NULL_TREE,NULL_TREE, NULL_TREE);
   return decl;
}

/**
 * 给出一个rawMangleName对应的ClassFunc。
 * getFields 返回的NPtrArray大小与 className的_superAddressArray 长度是一样的。
 * 并且_superAddressArray隐含的ClassFunc也是一样的。
 * 编译期并不知道 _superAddressArray中的元素是否有地址。但通过判断可能确认一些。
 */
static FieldData *getLocation(SuperCall *self,ClassName *className,char *rawMangleName,int *x)
{
   //getFields是从AObject到classname
   NPtrArray *fields=getFields(self,className);

   int i;
   int len=fields->len;
   //从ClassName到AObject
   for(i=len-1;i>=0;i--){
      FieldData *item=n_ptr_array_index(fields,i);
      ClassFunc *f=item->func;
      if(!strcmp(f->rawMangleName,rawMangleName)){
         *x=i;
         return item;
      }
   }
   return NULL;
}

/**
 * 找从from到to的类，找非非抽象类的类方法名称是 rawMangleName
 */
static nboolean findFuncAtNotAbstractClass (SuperCall *self,ClassName *className,char *rawMangleName)
{
   NPtrArray *fields=getFields(self,className);
   int len = fields->len;
   int i;
   for(i=0;i<len;i++){
      FieldData *field=n_ptr_array_index(fields,i);
      if(!strcmp(field->func->rawMangleName,rawMangleName)){
         break;
      }
   }

   for(;i<len;i++){
      FieldData *field=n_ptr_array_index(fields,i);
      ClassInfo    *info=class_mgr_get_class_info(class_mgr_get(),field->sysName);
      if(class_info_is_class(info))
         return TRUE;
   }
   return FALSE;
}

/**
 * 从from到to有没有非抽象类
 */
static nboolean findNoAbstractClass(char *from,char *to)
{
   ClassInfo    *info=class_mgr_get_class_info(class_mgr_get(),from);
   if(class_info_is_class(info))
      return TRUE;
   int count=0;
   while(info->parentName.sysName!=NULL){
      info=class_mgr_get_class_info(class_mgr_get(),info->parentName.sysName);
      if(class_info_is_class(info))
         return TRUE;
      if(strcmp(to,info->parentName.sysName))
         break;
   }
   return FALSE;
}

/**
 * 生成函数的typedecl,如:typedef void (*setData)(AObject *self,int value);
 * 然后这样调用
 * (setData) (((AObject *)self)->superCalls[index])
 */
//------------------第三版super设计，数组变成全局的，原来是定义在类中--------------------------
static void createParentSuperVarName(char *sysName,char *afterfix,char *buffer)
{
   sprintf(buffer,"_%s_parent_%s",sysName,afterfix);
}

static void createSuperVarName(char *sysName,char *afterfix,char *buffer)
{
   sprintf(buffer,"_%s_%s",sysName,afterfix);
}

/**
 * 创建这样的表达式
 * ( ( setData )  _TFirst_parent__superFuncAddressArray[1] )
 * setData 函数指针 _TFirst_parent__superFuncAddressArray 指针
 * x是数组索引，需要转成位
*/
static struct c_expr createExpr(SuperCall *self,location_t loc,tree typeDecl,ClassName *className,int x,char *arrayVarName)
{
   char varName[512];
   createParentSuperVarName(className->sysName,arrayVarName,varName);
   tree varDecl = lookup_name(get_identifier(varName));
   //重要 BITS_PER_UNIT 关键 字节转成位
   tree element=build_int_cst(integer_type_node,x*BITS_PER_UNIT);
   element = convert_to_ptrofftype (element);
   tree plusExpr=build2_loc (loc, POINTER_PLUS_EXPR, TREE_TYPE(varDecl), varDecl,element);
   tree ref  = build1 (INDIRECT_REF,build_pointer_type(integer_type_node), plusExpr);
   tree ret = build1 (CONVERT_EXPR, TREE_TYPE(typeDecl), ref);
   struct c_expr expr;
   expr.value=ret;
   return expr;
}

/**
 * * ( ( setData )  _TFirst_parent__superFuncAddressArray[1] )
* setData 函数指针 _TFirst_parent__superDeviceAddressArray[5] 数组
 */
static struct c_expr createSuperDeviceExpr(SuperCall *self,location_t loc,
      tree typeDecl,ClassName *className,int x,char *arrayVarName)
{
   char varName[512];
   createParentSuperVarName(className->sysName,arrayVarName,varName);
   tree varDecl = lookup_name(get_identifier(varName));
   tree element=build_int_cst(integer_type_node,x);
   tree arrayRef = build4 (ARRAY_REF, long_unsigned_type_node, varDecl,element, NULL_TREE,NULL_TREE);
   tree ret = build1 (CONVERT_EXPR, TREE_TYPE(typeDecl), arrayRef);
   struct c_expr expr;
   expr.value=ret;
   return expr;
}

static tree buildSuperVar(location_t loc,char *varName,tree type)
{
   tree id =  get_identifier(varName);
   tree decl = build_decl (loc,VAR_DECL,id, type);
   TREE_STATIC (decl) = 1;
   DECL_ARTIFICIAL (decl) = 1;
   DECL_CONTEXT(decl)=NULL_TREE;
   DECL_EXTERNAL(decl)=0;
   TREE_PUBLIC(decl)=0;
   TREE_USED (decl)=1;
   //重要
   c_c_decl_bind_file_scope(decl);//放在file_scope，c_c_decl_bind_file_scope是增加的,原本没有
   finish_decl (decl, loc, NULL,type, NULL_TREE);
   return decl;
}

/**
 * 创建三个接收父类的super数据的变量
 * 只有调用super时才会触发创建。
 * static unsigned long *_debug_AClass_parent__superFuncAddressArray;
 * static char         **_debug_AClass_parent__superKernelNameArray;
 * static unsigned long *_debug_AClass_parent__superDeviceAddressArray;
 * 设备变量加入managed属性，在ptx中会生对应变量声明
 */
static void createParentSuperVar(SuperCall *self,location_t loc,ClassName *className,char *suffix)
{
   char varName[512];
   createParentSuperVarName(className->sysName,suffix,varName);
   tree id =  get_identifier(varName);
   tree decl=lookup_name(id);
   if(!decl){
      if(strcmp(suffix,AET_SUPER_FUNC_ADDRESS_ARRAY)==0){
         buildSuperVar(loc,varName,build_pointer_type(long_unsigned_type_node));
      }else if(strcmp(suffix,AET_SUPER_KERNEL_NAME_ARRAY)==0){
         buildSuperVar(loc,varName,build_pointer_type(build_pointer_type(char_type_node)));
      }else  if(strcmp(suffix,AET_SUPER_DEVICE_ADDRESS_ARRAY)==0){
         //为了生成mtcs中的变量，必须指定大小，所以不能用指针
         tree dataType=long_unsigned_type_node;
         tree type = build_array_type (dataType,build_index_type (size_int (2000)));//先设一个大小，在初始化时再改
         decl =buildSuperVar(loc,varName,type);
         printf("创建 _superDeviceAddressArray--- :%s\n",varName);
         aet_print_tree(decl);
         DECL_ATTRIBUTES (decl) = tree_cons (get_identifier ("managed"), NULL, DECL_ATTRIBUTES (decl));
         DECL_PRESERVE_P (decl) = 1;
         TREE_USED(decl)=1;
         n_ptr_array_add(self->parentDeviceDeclArray,decl);
      }
   }
}

static nboolean validParentSuperName(char *varName,char *compare)
{
   char buffer[256];
   sprintf(buffer,"_parent_%s",compare);
   if(strstr(varName,buffer)){
      char *ab=varName+1;
      int len =strlen(varName)-strlen(buffer)-1;
      memcpy(buffer,varName+1,len);
      buffer[len]='\0';
      printf("supercall.c validParentSuperName :%s %s %s\n",varName,compare,buffer);
      ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),buffer);
      if(info!=NULL)
         return TRUE;
   }
   return FALSE;
}

/**
 * 判断变量名是不是有效的名字
 * #define AET_SUPER_FUNC_ADDRESS_ARRAY            "_superFuncAddressArray"      //用来保存类方法实现的地址
#define AET_SUPER_KERNEL_NAME_ARRAY                "_superKernelNameArray"       //用来保存类核方法实现的函数名
#define AET_SUPER_DEVICE_ADDRESS_ARRAY             "_superDeviceAddressArray"       //用来保存类设备方法实现的地址
 */
nboolean super_call_valid_mtcs_parent_super_call_var_name(char *varName)
{
   nboolean ret = validParentSuperName(varName,AET_SUPER_KERNEL_NAME_ARRAY);
   if(ret)
      return TRUE;
   ret = validParentSuperName(varName,AET_SUPER_DEVICE_ADDRESS_ARRAY);
   return ret;
}

/**
 * 找不到super对应的声明，如果是由于声明是私有的报错，如果找到私有方法所在的父类声明是公有的呢？
 * 不可能发生这样的情况，语规4.子类的方法要比父类方法的可见性高。
 */
static nboolean findErrorCause (location_t loc,ClassName *className,char *rawMangleName)
{
   //取从AObject到 className的类信息 (从小到大)
   ClassInfo *infos[30];
   int count=class_mgr_get_class_info_asc(class_mgr_get(),className,infos);
   int i;
   for(i=0;i<count;i++){
      NPtrArray    *funcArray=func_mgr_get_funcs(func_mgr_get(),&infos[i]->className);
      int j;
      int count=0;
      if(funcArray!=NULL){
         for(j=0;j<funcArray->len;j++){
            ClassFunc *func=n_ptr_array_index(funcArray,j);
            if(aet_utils_valid_tree(func->fieldDecl) && class_func_is_private(func)
                  && !strcmp(func->rawMangleName,rawMangleName)){
               error_at(loc,"类%qs中的方法%qs是私有的。无法访问，可以改为protected$",infos[i]->className.userName,func->orgiName);
               return TRUE;
            }
         }
      }
   }
   return FALSE;
}

/**
 * c_parser_postfix_expression_after_primary
 *   -->class_impl_replace_func_id
 *     -->super_call_replace_super_call
 *   -->class_impl_record_mtcs_call
 *     -->mtcs_lanuch_replace_call
 *        -->mtcs_lanuch_replace_call
 */
tree super_call_replace_super_call(SuperCall *self,location_t expr_loc,tree exprValue,ClassFunc *func)
{
   if(!self->parser->isAet)
      return NULL_TREE;
   ClassName *currentClassName=class_impl_get()->className;
   ClassInfo    *classInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),currentClassName);
   ClassName *parent=&classInfo->parentName;

   int x;//这是父类的super数组的下标。
   FieldData *field= getLocation(self,parent,func->rawMangleName,&x);
   if(field==NULL){
      //有可能是父类的方法是私有的。
      if(!findErrorCause(expr_loc,parent,func->rawMangleName)){
         n_error("未知错误。调用super$时发生。%s",func->orgiName);
      }
      return NULL_TREE;
   }
   ClassFunc *toFunc=field->func;
   ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),toFunc->className);
   //一般 aet_utils_valid_tree(toFunc->fromImplDefine) 是空的，但如果父类和本类同时在一个.c文件中实现，是有可能存在定义的函数
   //所以要判断fromImplDefine
   //如果没有定义，又是接口方法，又在接象类中。方法可能定义也可能没定义
   if(!aet_utils_valid_tree(toFunc->fromImplDefine) && toFunc->fromInterface && class_info_is_abstract_class(info)){
      printf("选中的方法是一个接口方法%s， 它被声明在抽象类%s中。有可能定义也有可能未定义。\n",toFunc->mangleFunName,field->sysName);
      nboolean find = findFuncAtNotAbstractClass(self,parent,toFunc->rawMangleName);
        //记录文件，函数，行，列号，在初始化代码时，判断x对应的地址是不是0，如果是零报错退出程序。
         printf("找不到接口实现。记录文件，函数，行，列号，在初始化代码时，判断x对应的地址是不是0，如果是零报错退出程序。%s %s\n",
               toFunc->mangleFunName,field->sysName);
         //加入运行时检查
         addErrorInfo(self,currentClassName, expr_loc,toFunc,x,parent);
   }
   if(class_func_is_abstract(toFunc)){
      //如果是抽象方法并且就声明在parent中。
      if(!strcmp(field->sysName,parent->sysName)){
         error_at(expr_loc,"不能直接调用类%s的抽象方法%s。",parent->userName,toFunc->orgiName);
         return NULL_TREE;
      }
      if(!findNoAbstractClass(field->sysName,parent->sysName)){
         //有可能该抽象方法没有实现。
         printf("找不到抽象方法实现。记录文件，函数，行，列号，在初始化代码时，判断x对应的地址是不是0，如果是零报错退出程序。%s %s\n",
               toFunc->mangleFunName,field->sysName);
         //加入运行时检查
         addErrorInfo(self,currentClassName, expr_loc,toFunc,x,parent);
      }
   }

   n_debug("supercall 在本类创建三个接收父类方法的变量:本类:%s toFunc:%s\n",currentClassName->sysName,toFunc->mangleFunName);
   aet_print_tree(exprValue);
   tree typeDecl=createTypeDecl(toFunc->className,toFunc);
   struct c_expr expr;
   if(!class_func_is_mtcs(toFunc)){
      createParentSuperVar(self,expr_loc,currentClassName,AET_SUPER_FUNC_ADDRESS_ARRAY);
      expr=createExpr(self,expr_loc,typeDecl,currentClassName,x,AET_SUPER_FUNC_ADDRESS_ARRAY);
   }else if(class_func_is_kernel(toFunc)){
      createParentSuperVar(self,expr_loc,currentClassName,AET_SUPER_KERNEL_NAME_ARRAY);
      expr=createExpr(self,expr_loc,typeDecl,currentClassName,x,AET_SUPER_KERNEL_NAME_ARRAY);
   }else if(class_func_is_device(toFunc)){
      printf("xxd ss :%d %d\n",class_func_is_host(toFunc),class_func_is_divide(toFunc));
      if(class_func_is_host(toFunc)){
         //如果是host device函数，创建两个parent变量，因为从host device函数分裂的设备函数是setData_device对用户胆不可见的，
         //不会进入这里 到mtcsclones再把主机super调用的表达式替换为调用AET_SUPER_DEVICE_ADDRESS_ARRAY的表达式。
          createParentSuperVar(self,expr_loc,currentClassName,AET_SUPER_FUNC_ADDRESS_ARRAY);
          createParentSuperVar(self,expr_loc,currentClassName,AET_SUPER_DEVICE_ADDRESS_ARRAY);
          expr=createExpr(self,expr_loc,typeDecl,currentClassName,x,AET_SUPER_FUNC_ADDRESS_ARRAY);
      }else{
         createParentSuperVar(self,expr_loc,currentClassName,AET_SUPER_DEVICE_ADDRESS_ARRAY);
         expr=createSuperDeviceExpr(self,expr_loc,typeDecl,currentClassName,x,AET_SUPER_DEVICE_ADDRESS_ARRAY);
      }
   }
//   //主机函数，生成的调用是 ( ( setData )  _AObject_parent_superFuncAddressArray[1] )
//   tree typeDecl=createTypeDecl(toFunc->className,toFunc);
//   struct c_expr expr;
//   if(!class_func_is_mtcs(toFunc) || (class_func_is_host(toFunc) && !class_func_is_divide(toFunc))){
//      printf("是一个核函数吗 1111----%s %d\n",toFunc->mangleFunName,class_func_is_kernel(toFunc));
//
//      expr=createExpr(self,expr_loc,typeDecl,currentClassName,x,AET_SUPER_FUNC_ADDRESS_ARRAY);
//   }else if(class_func_is_kernel(toFunc)){
//      printf("是一个核函数吗----%s\n",toFunc->mangleFunName);
//      expr=createExpr(self,expr_loc,typeDecl,currentClassName,x,AET_SUPER_KERNEL_NAME_ARRAY);
//   }else if(class_func_is_device(toFunc)){
//      if(class_func_is_host(toFunc)){
//         printf("是一个设备和主机函数吗----%s divide:%d\n",toFunc->mangleFunName,class_func_is_divide(toFunc));
//
//         if(!class_func_is_divide(toFunc))
//            expr=createExpr(self,expr_loc,typeDecl,currentClassName,x,AET_SUPER_FUNC_ADDRESS_ARRAY);
//         else
//            expr=createSuperDeviceExpr(self,expr_loc,typeDecl,currentClassName,x,AET_SUPER_DEVICE_ADDRESS_ARRAY);
//      }else{
//         expr=createSuperDeviceExpr(self,expr_loc,typeDecl,currentClassName,x,AET_SUPER_DEVICE_ADDRESS_ARRAY);
//      }
//   }
   if(aet_utils_valid_tree(expr.value)){
       return expr.value;
   }
   return NULL_TREE;
}

static void modifySuperDeviceAddressArray(NString *codes,int i,char *deviceAddressName,ClassFunc *define)
{
   //是第几个设备函数，需要与_TFirst_deviceFuncPointers[]={_Z6TFirst9setdeviceEPN6TFirstE};中的元素位置统一
   char devicePointerVarName[256];
   mtcs_info_create_device_func_pointer_var_name(devicePointerVarName,define->className->sysName);
   int index= func_mgr_get_device_func_index(func_mgr_get(),define->className,define->mangleFunName);
   if(index>=0){
      char deviceAddStr[256];
      sprintf(deviceAddStr,"%d_%s",index,devicePointerVarName);
      n_string_append_printf(codes,"\t\t%s[%d]=(unsigned long )\"%s\";\n",deviceAddressName,i,deviceAddStr);
   }else{
      n_error("找不到设备函数:%s\n",define->mangleFunName);
   }
}

static void fillHostKernelDeviceArray(NString *codes,int i,ClassFunc *define,char *funcAddressName,
      char *kernelName,char *deviceAddressName,nboolean havaKernelFunc,nboolean havaDeviceFunc)
{
   if(!class_func_is_mtcs(define)){
      n_string_append_printf(codes,"\t\t%s[%d]=(unsigned long )%s;\n",funcAddressName,i,define->mangleFunName);
      if(havaKernelFunc)
         n_string_append_printf(codes,"\t\t%s[%d]=NULL;\n",kernelName,i);
      if(havaDeviceFunc)
         n_string_append_printf(codes,"\t\t%s[%d]=0;\n",deviceAddressName,i);
   }else if(class_func_is_kernel(define)){
      n_string_append_printf(codes,"\t\t%s[%d]=0;\n",funcAddressName,i);
      if(havaKernelFunc)
         n_string_append_printf(codes,"\t\t%s[%d]=\"%s\";\n",kernelName,i,define->mangleFunName);
      if(havaDeviceFunc)
         n_string_append_printf(codes,"\t\t%s[%d]=0;\n",deviceAddressName,i);
   }else if(class_func_is_device(define)){
      if(class_func_is_host(define)){
         if(class_func_is_divide(define)){
            n_string_append_printf(codes,"\t\t%s[%d]=0;\n",funcAddressName,i);
            if(havaKernelFunc)
               n_string_append_printf(codes,"\t\t%s[%d]=NULL;\n",kernelName,i);
            if(havaDeviceFunc)
               modifySuperDeviceAddressArray(codes,i,deviceAddressName,define);
         }else{
            //当作主机函数处理
            n_string_append_printf(codes,"\t\t%s[%d]=(unsigned long )%s;\n",funcAddressName,i,define->mangleFunName);
            if(havaKernelFunc)
               n_string_append_printf(codes,"\t\t%s[%d]=NULL;\n",kernelName,i);
            if(havaDeviceFunc)
               n_string_append_printf(codes,"\t\t%s[%d]=0;\n",deviceAddressName,i);
         }
      }else{
         //是纯粹的设备函数
         n_string_append_printf(codes,"\t\t%s[%d]=0;\n",funcAddressName,i);
         if(havaKernelFunc)
            n_string_append_printf(codes,"\t\t%s[%d]=NULL;\n",kernelName,i);
         if(havaDeviceFunc)
            //是第几个设备函数，需要与_TFirst_deviceFuncPointers[]={_Z6TFirst9setdeviceEPN6TFirstE};中的元素位置统一
            modifySuperDeviceAddressArray(codes,i,deviceAddressName,define);
      }
   }
}

/**
 * 根据func的类型，选择数组。
 */
static char *selectParentArray(ClassFunc *func,char *parentFuncAddrArray,char *parentKernelNameArray,char *deviceAddrArray)
{
   char *name=NULL;
   if(!class_func_is_mtcs(func)){
      name=parentFuncAddrArray;
   }else if(class_func_is_kernel(func)){
      name=parentKernelNameArray;

   }else if(class_func_is_device(func)){
      if(class_func_is_host(func)){
         if(class_func_is_divide(func)){
            name=deviceAddrArray;
         }else{
            //当作主机函数处理
            name=parentFuncAddrArray;
         }
      }else{
         //是纯粹的设备
         name=deviceAddrArray;
      }
   }
   return name;
}

/*
 * 生成如下代码
static unsigned long _TFirst__superFuncAddressArray[6];
static char *_TFirst__superKernelNameArray[6];            //根据是否有核函数声明或定义
static unsigned long _TFirst__superDeviceAddressArray[6]; //根据是否有设备函数声明或定义
static volatile int _TFirst_superDataLock=0;
static void TSecond_init_inner_super_data_2_TSecond()
{
   if(!({int v1=1;int v2=1;__atomic_exchange(&_TSecond_superDataLock,&v1,&v2,__ATOMIC_SEQ_CST);v2;})){
      TFirst_init_global_super_data_1_TFirst(_TSecond__superFuncAddressArray,_TSecond__superKernelNameArray,NULL,
      &_TSecond_parent__superFuncAddressArray,NULL,_TSecond_parent__superDeviceAddressArray);
.....
   }
}
*/
char *super_call_create_func_codes(SuperCall *self,ClassName *className)
{
   ClassInfo    *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   NPtrArray *fields = getFields(self,className);
   NPtrArray    *funcArray=func_mgr_get_funcs(func_mgr_get(),className);
   int i;
   NString *codes=n_string_new("");

   //第一步 查找创建接收父类的super变量的三个变量，如果调用super会由代码生成。
   //static unsigned long *_TFirst_parent_superFuncAddressArray;
   //static char **_TFirst_parent_superFuncKernelNameArray;
   //static unsigned long *_TFirst_parent_superDeviceAddressArray;

   char parentFuncAddressName[512];
   char parentKernelName[512];
   char parentDeviceName[512];

   createParentSuperVarName(className->sysName,AET_SUPER_FUNC_ADDRESS_ARRAY,parentFuncAddressName);
   createParentSuperVarName(className->sysName,AET_SUPER_KERNEL_NAME_ARRAY,parentKernelName);
   createParentSuperVarName(className->sysName,AET_SUPER_DEVICE_ADDRESS_ARRAY,parentDeviceName);

   // 当在类中调用super$方法后，会生成对应的parent变量
   //parentFuncAddrDecl不等于空说明有普通函数的super调用
   tree parentFuncAddrDecl=lookup_name(get_identifier(parentFuncAddressName));
   tree parentKernelNameDecl=lookup_name(get_identifier(parentKernelName));
   tree parentDeviceAddrDecl=lookup_name(get_identifier(parentDeviceName));
   if(parentDeviceAddrDecl){
      //原来设的大小是2000,现在可以设真实大小了。
      printf("重新改变 parentDeviceAddrDecl 的大小\n");
      tree domain= build_range_type (integer_type_node,
             build_int_cst (unsigned_type_node, 0), build_int_cst (unsigned_type_node,fields->len-1));
      tree newtype = c_build_array_type (long_unsigned_type_node, domain);
      TREE_TYPE(parentDeviceAddrDecl)=newtype;
      /**
       * 重要 relayout_decl
       * 在mtcsrtl.c中会调用mtcs_rtl_set_mem_attributes_minus_bitpos更新rtl
       * 其中会调用;
       * tree new_size = DECL_SIZE_UNIT (parentDeviceAddrDecl);
       * poly_uint64 const_size;
         if (poly_int_tree_p (new_size, &const_size)){
            ...
         }
       * 获取数组大小，没有 relayout_decl 它的值还是原来的
       */
      relayout_decl (parentDeviceAddrDecl);
   }


   //第二步。创建三个大小确定的变量用来保存函数地址和函数名字。创建用来同步的原子变量
   //static unsigned long _TFirst__superFuncAddressArray[5];
   //static char *_TFirst__superKernelNameArray[5];
   //static unsigned long _TFirst__superDeviceNameArray[5];
   //static volatile int TFirst_superAtomicLock=0;
   char funcAddressName[512];
   createSuperVarName(className->sysName,AET_SUPER_FUNC_ADDRESS_ARRAY,funcAddressName);
   char kernelName[512];
   createSuperVarName(className->sysName,AET_SUPER_KERNEL_NAME_ARRAY,kernelName);
   char deviceAddressName[512];
   createSuperVarName(className->sysName,AET_SUPER_DEVICE_ADDRESS_ARRAY,deviceAddressName);


   nboolean havaKernelFunc =func_mgr_have_kernel_func(func_mgr_get(),className);
   nboolean havaDeviceFunc =func_mgr_have_device_func(func_mgr_get(),className);

   n_string_append_printf(codes,"static unsigned long %s[%d];\n",funcAddressName,fields->len);
   if(havaKernelFunc)
      n_string_append_printf(codes,"static char *%s[%d];\n",kernelName,fields->len);
   if(havaDeviceFunc)
      n_string_append_printf(codes,"static unsigned long %s[%d];\n",deviceAddressName,fields->len);
   n_string_append(codes,"\n");

   //声明原子锁变量
   char lockName[512];
   sprintf(lockName,"_%s_superDataLock",className->sysName);
   n_string_append_printf(codes,"static volatile int %s=0;\n",lockName);
   n_string_append(codes,"\n");

   //第三步 生成初始化_TFirst_superFuncAddressArray、_TFirst_superFuncKernelNameArray函数
   char *innerFuncName = aet_utils_create_init_inner_super_func_name(className->sysName);
   n_string_append_printf(codes,"static void %s()\n",innerFuncName);
   n_string_append(codes,"{\n");
   //n_string_append(codes,"return;\n");//测试用，等会取消

   n_string_append_printf(codes,"\tif(!({int v1=1;int v2=1;__atomic_exchange(&%s,&v1,&v2,__ATOMIC_SEQ_CST);v2;})){\n",lockName);
   if(info->parentName.sysName!=NULL){
      char *parentSuperInitFuncName = aet_utils_create_init_global_super_func_name(info->parentName.sysName);
      /*生成的内容
      debug_AObject_init_global_super_data_1_debug_AObject(_TFirst__superFuncAddressArray,_TFirst__superKernelNameArray,_TFirst__superDeviceNameArray,
      &_TFirst_parent__superFuncAddressArray,&_TFirst_parent__superKernelNameArray,_TFirst_parent__superDeviceNameArray);
       */
      n_string_append_printf(codes,"\t\t%s(%s,%s,%s,\n\t\t%s%s,%s%s,%s);\n",
                           parentSuperInitFuncName,
                           funcAddressName,
                           havaKernelFunc?kernelName:"NULL",
                           havaDeviceFunc?deviceAddressName:"NULL",
                           parentFuncAddrDecl?"&":"",
                           parentFuncAddrDecl?parentFuncAddressName:"NULL",
                           parentKernelNameDecl?"&":"",
                           parentKernelNameDecl?parentKernelName:"NULL",
                           parentDeviceAddrDecl?parentDeviceName:"NULL");

      free(parentSuperInitFuncName);
   }

   // 2.更新有声明有定义的本类，如果是普通类有声明一定有定义。如果是抽象类有声明可以没有定义。所以funcAddressArray本类有部分可能是0
   for(i=0;i<fields->len;i++){
      FieldData *item = n_ptr_array_index(fields,i);
      //2.更新本类。
      if(strcmp(item->sysName,className->sysName))
         continue;
      ClassFunc *define = getDefine(item->func,funcArray);//声明的filed在funcArray是否存在
      if(define){
         n_debug("super_call_create_init_codes 00 生成代码 i:%d %s\n",i,define->mangleFunName);
         fillHostKernelDeviceArray(codes,i,define,funcAddressName,kernelName,deviceAddressName,havaKernelFunc,havaDeviceFunc);
      }else{
         //printf("本类没有定义函数 %s %s\n",className->sysName,item->func->mangleFunName);
         //如果本类是抽象类，没问题，如果不是说明这是一个接口
         n_debug("super_call_create_init_codes 11 本类是抽象类，没问题 %s %s\n",className->sysName,item->func->mangleFunName);
         int pos = find(fields,className,i-1, item->func->rawMangleName);
         n_debug("super_call_create_init_codes 22 在抽象类%s中的函数%s没有定义，往AObject查找，找到了声明,位置:%d\n",
         className->sysName,item->func->orgiName,pos);
         if(pos<0){
            if(class_func_is_divide(item->func)){
               //获取分裂源，通过分裂原的名字，找到本类的的全局变量 deviceFuncPointers(AET_MTCS_DEVICE_FUNC_POINTERS_VAR_NAME)
               ClassFunc *divideSrc=class_func_get_divide_src(item->func);
               char devicePointerVarName[256];
               mtcs_info_create_device_func_pointer_var_name(devicePointerVarName,className->sysName);
               int index= func_mgr_get_device_func_index(func_mgr_get(),className,divideSrc->mangleFunName);
               if(index>=0){
                  char deviceAddStr[256];
                  sprintf(deviceAddStr,"%d_%s",index,devicePointerVarName);
                  //生成 _TFirst__superDeviceAddressArray="0_TFirst_deviceFuncPointers";
                  n_string_append_printf(codes,"\t\t%s[%d]=(unsigned long )\"%s\";\n",deviceAddressName,i,deviceAddStr);
               }else{
                  n_error("未知错误。在类%s中的分裂函数%s,它的分裂源没有定义。\n",className->sysName,item->func->orgiName);
               }
            }else{
               if(!class_info_is_abstract_class(info))
                  n_error("未知错误。在类%s中的函数%s没有定义，往AObject查找，也没有找到声明。\n",className->sysName,item->func->orgiName);
            }
         }else{
            n_string_append_printf(codes,"\t\t%s[%d]=%s[%d];\n",funcAddressName,i,funcAddressName,pos);
            if(havaKernelFunc)
               n_string_append_printf(codes,"\t\t%s[%d]=%s[%d];\n",kernelName,i,kernelName,pos);
            if(havaDeviceFunc)
               n_string_append_printf(codes,"\t\t%s[%d]=%s[%d];\n",deviceAddressName,i,deviceAddressName,pos);
         }
      }
   }
   //3.用本类只有定义覆盖父类_superAddressArray中某个位置。更新的是那过类?，从本类的父类查找第一次出现声明的类和类方法在该类的位置
   //调用super$时，查找类方法第一次出现在那过类。
   for(i=0;i<funcArray->len;i++){
      ClassFunc *func=n_ptr_array_index(funcArray,i);
      //没有声明，只有定义
      if(!aet_utils_valid_tree(func->fieldDecl) && !class_func_is_private(func) && aet_utils_valid_tree(func->fromImplDefine)){
         int j;
         //从本类开始到AObject
         for(j=fields->len-1;j>=0;j--){
            FieldData *item = n_ptr_array_index(fields,j);
            //排除本类
            if(strcmp(item->sysName,className->sysName)){
               if(strcmp(item->func->rawMangleName,func->rawMangleName)==0){
                  //查到的本类只有定义的方法是在那过类声明的。
                  n_debug("super_call_create_init_codes 44  调用super$时，查找类方法第一次出现在那过类 %d %s %s %s\n",
                  j,item->sysName,className->sysName,func->mangleFunName);
                  fillHostKernelDeviceArray(codes,j,func,funcAddressName,kernelName,deviceAddressName,havaKernelFunc,havaDeviceFunc);
                  break;
               }
            }
         }
      }
   }
   //下面是检查地址是不是空的，属于运行时检查。
   NPtrArray *errorArray=n_hash_table_lookup(self->recordErrorTable,className->sysName);
   if(errorArray){
      for(i=0;i<errorArray->len;i++){
         RecordErrorInfo *item=(RecordErrorInfo *)n_ptr_array_index(errorArray,i);
         char *selectArray = selectParentArray(item->callFunc, parentFuncAddressName,parentKernelName,parentDeviceName);
         n_string_append_printf(codes,"\t\tif(%s[%d]==0){\n",selectArray,item->element);
         n_string_append_printf(codes,"\t\t\tfprintf(stderr,\"%s\\n\");\n",item->errInfo);
         n_string_append(codes,"\t\t\tabort();\n");
         n_string_append(codes,"\t\t}\n");
      }
   }
   //给super设备函数设值
   if(parentDeviceAddrDecl){//说明有super调用
      n_string_append_printf(codes,"\t\t%s(%s,%d,\"%s\",0);\n",AET_MTCS_COPY_DEVICE_TO_SUPER_FUNC_NAME,
            parentDeviceName,fields->len,parentDeviceName);
   }

   n_string_append(codes,"\t}\n");
   n_string_append(codes,"}\n");//在这里结束本类的函数 static void TFirst_init_inner_super_data_2_TFirst()
   n_string_append(codes,"\n");

   //下面是生成全局函数 debug_AClass_init_global_super_data_1_debug_AClass
   /*
    void TFirst_init_global_super_data_1_TFirst(unsigned long *addr,char **names,unsigned long *deviceAddr,
    unsigned long **parentAddr,char ***parentNames,unsigned long **parentDevicAddr)
   {
                ....
   }
    */
   char *superInitFuncName = aet_utils_create_init_global_super_func_name(className->sysName);
   n_string_append_printf(codes,"void %s(unsigned long *addr,char **names,unsigned long *deviceAddr,\n\
         \t\tunsigned long **parentAddr,char ***parentNames,unsigned long *parentDevicAddr)\n",
         superInitFuncName);
   n_string_append(codes,"{\n");
   //n_string_append(codes,"return;\n");//测试用，等会取消
   n_string_append(codes,"\tint i=0;\n");
   n_string_append_printf(codes,"\tfor(i=0;i<%d;i++){\n",fields->len);
   n_string_append_printf(codes,"\t\taddr[i]=%s[i];\n",funcAddressName);
   if(havaKernelFunc){
      n_string_append_printf(codes,"\t\tif(names)\n");
      n_string_append_printf(codes,"\t\t\tnames[i]=%s[i];\n",kernelName);
   }
   if(havaDeviceFunc){
      n_string_append_printf(codes,"\t\tif(deviceAddr)\n");
      n_string_append_printf(codes,"\t\t\tdeviceAddr[i]=%s[i];\n",deviceAddressName);
   }
   n_string_append(codes,"\t}\n");
   n_string_append_printf(codes,"\tif(parentAddr)\n");
   n_string_append_printf(codes,"\t\t*parentAddr=%s;\n",funcAddressName);
   if(havaKernelFunc){
      n_string_append_printf(codes,"\tif(parentNames)\n");
      n_string_append_printf(codes,"\t\t*parentNames=%s;\n",kernelName);
   }
   if(havaDeviceFunc){
      n_string_append_printf(codes,"\tif(parentDevicAddr)\n");
      n_string_append_printf(codes,"\t\tmemcpy(parentDevicAddr,%s,sizeof(void*)*%d);\n",deviceAddressName,fields->len);
   }
   n_string_append(codes,"}\n");
   return n_string_free(codes,FALSE);

}

/**
 * 声明初始化类的super数据的全局函数
 *void TFirst_init_global_super_data_1_TFirst(unsigned long *addr,char **names,unsigned long *deviceAddr,
               unsigned long **parentAddr,char ***parentNames,unsigned long *parentDevicAddr)
 * 用该方法创建函数声明，编译报conflic types for 错误。改为生成token方法。问题解决。
 */
void super_call_create_init_func_decl(SuperCall *self,location_t loc,ClassName *className)
{
   char *funName = aet_utils_create_init_global_super_func_name(className->sysName);
   tree id = aet_utils_create_ident (funName);
   tree decl=lookup_name(id);
   if(decl){
      printf("super_call_create_init_func_decl 00 已经声明了函数:%s\n",funName);
      free(funName);
      return;
   }
   tree param_type_list = NULL;
   //第一个参数 类型是 unsigned long *funcAddress
   tree parm1=build_pointer_type(long_unsigned_type_node);
   param_type_list = tree_cons (NULL_TREE, parm1, param_type_list);
   //加入第二个参数 char **kernelName
   tree parm2=build_pointer_type(build_pointer_type(char_type_node));
   param_type_list = tree_cons (NULL_TREE, parm2, param_type_list);
   //第三个参数 类型是 unsigned long *deviceFuncAddress
   tree parm3=build_pointer_type(long_unsigned_type_node);
   param_type_list = tree_cons (NULL_TREE, parm3, param_type_list);

   //加入第四个参数 unsigned long  **parentFuncAddress
   tree parm4=build_pointer_type(build_pointer_type(long_unsigned_type_node));
   param_type_list = tree_cons (NULL_TREE, parm4, param_type_list);
   //加入第五个参数 char  ***parentKernelName
   tree parm5=build_pointer_type(build_pointer_type(char_type_node));
   parm5=build_pointer_type(parm5);
   param_type_list = tree_cons (NULL_TREE, parm5, param_type_list);
   //加入第六个参数 unsigned long  **parentDeviceFuncAddress
   tree parm6=build_pointer_type(long_unsigned_type_node);
   param_type_list = tree_cons (NULL_TREE, parm6, param_type_list);
   //结束参数
   param_type_list = tree_cons (NULL_TREE, void_type_node, param_type_list);//在函数声明的最后一个参数必须是void_type_node

   param_type_list = nreverse (param_type_list);
   tree rtntype=void_type_node;
   tree fntype = build_function_type (rtntype, param_type_list);
   tree fndecl = build_decl (loc, FUNCTION_DECL, id, fntype);
   TREE_STATIC (fndecl) = 0;
   DECL_ARTIFICIAL (fndecl) = 1;
   TREE_PUBLIC (fndecl) = 1;
  // DECL_EXTERNAL (fndecl) = 1;
   //pushdecl (fndecl); 不能调用 pushdecl否则出undefined reference to `_TSecond__superFuncAddressArray'
   c_c_decl_bind_file_scope(fndecl);//放在file_scope，c_c_decl_bind_file_scope是增加的,原本没有
   finish_decl (fndecl, loc, NULL_TREE,NULL_TREE, NULL_TREE);
   free(funName);
}

/**
 * 变量vardecl的名字是不是 _TFirst_parent__superFuncAddressArray
 */
nboolean super_call_is_parent_func_addr_var(SuperCall *self,tree vardecl,ClassName *className)
{
   if(!DECL_P(vardecl))
      return FALSE;
   char *name=IDENTIFIER_POINTER(DECL_NAME(vardecl));
   char compare[512];
   createParentSuperVarName(className->sysName, AET_SUPER_FUNC_ADDRESS_ARRAY,compare);
   return strcmp(name,compare)==0;
}

tree     super_call_get_parent_kernel_name_var(SuperCall *self,ClassName *className)
{
   char name[512];
   createParentSuperVarName(className->sysName, AET_SUPER_KERNEL_NAME_ARRAY,name);
   tree var=lookup_name(get_identifier(name));
   if(!var){
      n_error("不存在的变量:%s %s\n",className->sysName,name);
   }
   return var;
}

//获取_TSecond_parent__superFuncAddressArray变量
tree    super_call_get_parent_device_decl(SuperCall *self,char *sysName)
{
   char varName[256];
   createParentSuperVarName(sysName,AET_SUPER_DEVICE_ADDRESS_ARRAY,varName);
   int i;
   for(i=0;i<self->parentDeviceDeclArray->len;i++){
      tree item=n_ptr_array_index(self->parentDeviceDeclArray,i);
      char *name=IDENTIFIER_POINTER(DECL_NAME(item));
      if(strcmp(name,varName)==0)
         return item;
   }
   return NULL_TREE;
}

SuperCall *super_call_new()
{
   SuperCall *self = n_slice_alloc0 (sizeof(SuperCall));
   superCallInit(self);
   self->parser = aet_parser_get();
   return self;
}
