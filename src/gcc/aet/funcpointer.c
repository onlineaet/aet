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
#include "c-family/c-pragma.h"
#include "c/c-tree.h"
#include "c/c-parser.h"

#include "aetutils.h"
#include "classmgr.h"
#include "aetinfo.h"
#include "c-aet.h"
#include "aetprinttree.h"
#include "classutil.h"
#include "funcpointer.h"
#include "varmgr.h"
#include "genericutil.h"
#include "genericparser.h"
#include "funcmgr.h"
#include "classctor.h"
#include "classimpl.h"


static int getParams(tree funcType,int *varargs)
{
   int count=0;
   for (tree al = TYPE_ARG_TYPES (funcType); al; al = TREE_CHAIN (al)){
      tree type=TREE_VALUE(al);
      if(type == void_type_node){
         // printf("有void_type_node count:%d\n",count);
         *varargs=0;
         break;
      }
      count++;
   }
   return count;
}

static int getParamTree(tree funcType,tree *results)
{
   int count=0;
   for (tree al = TYPE_ARG_TYPES (funcType); al; al = TREE_CHAIN (al)){
      tree type=TREE_VALUE(al);
      if(type == void_type_node){
         // printf("有void_type_node count:%d\n",count);
         break;
      }
      results[count]=type;
      count++;
   }
   return count;
}


static tree getFunctionType(tree value)
{
    tree ret=value;
    int i;
    for(i=0;i<3;i++){
        tree type=TREE_TYPE(ret);
        if(TREE_CODE(type)==FUNCTION_TYPE)
            return type;
        ret=type;
    }
    return NULL;
}

static tree getTypeFromPointerType(tree type)
{
   int i;
   for(i=0;i<100;i++){
      if(TREE_CODE(type)!=POINTER_TYPE)
         return type;
      type=TREE_TYPE(type);
   }
   return i;
}

static nboolean find (tree lhs_main_type, tree rhs_struct_type)
{
   tree field = TYPE_FIELDS (rhs_struct_type);
   if (!RECORD_OR_UNION_TYPE_P (TREE_TYPE (field)))
      return FALSE;
   tree fieldtype = (TYPE_ATOMIC (TREE_TYPE (field))
         ? c_build_qualified_type (TREE_TYPE (field),TYPE_QUAL_ATOMIC) : TYPE_MAIN_VARIANT (TREE_TYPE (field)));
   int ret=comptypes (lhs_main_type, fieldtype);
   if(ret==1)
      return TRUE;
   return find(lhs_main_type,fieldtype);
}

/**
 * 右边的参数与左边的比较。如果正确返回 0
 */
static int  compare(tree ltype, tree rtype)
{
   enum tree_code code = TREE_CODE (ltype);
   tree lmain= TYPE_MAIN_VARIANT (ltype);
   tree rmain= TYPE_MAIN_VARIANT (rtype);
   int rlcmpmain=comptypes (rmain,lmain);
   int rlcmptype=comptypes (rtype,ltype);

   if(code==RECORD_TYPE && TREE_CODE(rtype)!=RECORD_TYPE){
      printf("左值是结构体，右值不是。\n");
      return -22;
   }

   if(code==RECORD_TYPE && TREE_CODE(rtype)==RECORD_TYPE){
      if(rtype!=ltype){
         printf("左值是结构体，右值是结构体，但不是同一个。\n");
         return -23;
      }
   }

   if(code==UNION_TYPE && TREE_CODE(rtype)!=UNION_TYPE){
      printf("左值是联合体，右值不是。\n");
      return -24;
   }

   if(code==UNION_TYPE && TREE_CODE(rtype)==UNION_TYPE){
      if(rtype!=ltype){
         printf("左值是联合体，右值是联合体，但不是同一个。\n");
         return -25;
      }
   }

   if (rlcmpmain  || (code == ARRAY_TYPE && rlcmptype) || (gnu_vector_type_p(ltype) && rlcmptype)
   || (code == POINTER_TYPE  && TREE_CODE (rtype) == ARRAY_TYPE  && rlcmptype)){

      if (code == POINTER_TYPE){
         if (TREE_CODE (rtype) == ARRAY_TYPE){
            return -1;
         }
      }

      if (code == ARRAY_TYPE && TREE_CODE (rtype) != STRING_CST  && TREE_CODE (rtype) != CONSTRUCTOR){
         return -2;
      }
      /* Added to enable additional -Wsuggest-attribute=format warnings.  */
      if (TREE_CODE (rtype) == POINTER_TYPE){
         //printf("右值是POINTER_TYPE\n");
         ;
      }
   }

   enum tree_code codel=code;
   enum tree_code coder=TREE_CODE(rtype);

   if (codel == INTEGER_TYPE || codel == REAL_TYPE || codel == FIXED_POINT_TYPE ||
   codel == ENUMERAL_TYPE || codel == COMPLEX_TYPE || codel == BOOLEAN_TYPE){
      nboolean re= (coder == INTEGER_TYPE || coder == REAL_TYPE || coder == FIXED_POINT_TYPE ||
               coder == ENUMERAL_TYPE || coder == COMPLEX_TYPE  || coder == BOOLEAN_TYPE);
      if(!re){
         printf("左边是数字，右边不是。\n");
         return -3;//左边是数字，右边不是
      }
      return 0;
   }

   if (codel == POINTER_TYPE){
      if(coder!=POINTER_TYPE){
         printf("左边是指针，右边不是。\n");
         return -4;//左边是指针，右边不是。
      }
      //判断指针次数
      int lc= class_util_get_pointers(ltype);
      int rc= class_util_get_pointers(rtype);
      if(lc!=rc){
         printf("左值指针次数与右值的不等。左：%d 右:%d\n",lc,rc);
         return -5;//右边的指针次数与左边不等。
      }
      //类型判断
      tree ltyp=  getTypeFromPointerType(ltype);
      tree rtyp=  getTypeFromPointerType(rtype);
      //     printf("去除pointer------left:\n");
      //     aet_print_tree_skip_debug(ltyp);
      //     printf("\n\n");
      //     printf("去除pointer------right:\n");
      // aet_print_tree_skip_debug(rtyp);

      if (VOID_TYPE_P (ltyp) &&  !VOID_TYPE_P (rtyp)){
         //printf("左值是void 右值不是，允许。\n",get_tree_code_name(TREE_CODE(rtype)));
         return 0;
      }
      if (!VOID_TYPE_P (ltyp) &&  VOID_TYPE_P (rtyp)){
         printf("右值是void 左值不是，不允许。\n",get_tree_code_name(TREE_CODE(ltyp)));
         return -6;
      }
      codel=TREE_CODE(ltyp);
      coder=TREE_CODE(rtyp);
      if (codel == INTEGER_TYPE || codel == REAL_TYPE || codel == FIXED_POINT_TYPE ||
      codel == ENUMERAL_TYPE || codel == COMPLEX_TYPE || codel == BOOLEAN_TYPE){
         nboolean re= (coder == INTEGER_TYPE || coder == REAL_TYPE || coder == FIXED_POINT_TYPE ||
         coder == ENUMERAL_TYPE || coder == COMPLEX_TYPE  || coder == BOOLEAN_TYPE);
         if(!re){
            //             if(coder==VOID_TYPE){
            //                  return 0;
            //             }else{
            printf("左值是数字指针，右值不是数字指针。\n");
            return -3;//左值是数字，右值不是
            //}
         }
         return 0;
      }
      if(codel==RECORD_TYPE && coder!=RECORD_TYPE){
         printf("左值是结构体，右值不是。\n");
         return -7;
      }

      if(codel==RECORD_TYPE && coder==RECORD_TYPE){
         if(ltyp==rtyp){
            //printf("两个结构是一样的。\n");
            return 0;
         }
         char *sysNameL=class_util_get_class_name_by_record(ltyp);
         char *sysNameR=class_util_get_class_name_by_record(rtyp);
         if(sysNameL!=NULL && sysNameR==NULL){
            printf("右值结构体不能转为左值类。\n");
            return -9;
         }else if(sysNameL!=NULL && sysNameR!=NULL){
            ClassRelationship  ship= class_mgr_relationship(class_mgr_get(), sysNameR,sysNameL);
            if(ship==CLASS_RELATIONSHIP_CHILD || ship==CLASS_RELATIONSHIP_OTHER_IMPL)
               return 0;
            if(ship==CLASS_RELATIONSHIP_PARENT ){
               printf("右值是左值的父类。\n");
               return -10;
            }
            if(ship==CLASS_RELATIONSHIP_IMPL ){
               printf("右值是左值的接口。\n");
               return -11;
            }
            printf("右值与左值无关系。\n");
            return -12;
         }else if(sysNameL==NULL && sysNameR!=NULL){
            printf("右值类不能转成左值结构体。\n");
            return -13;
         }else if(sysNameL==NULL && sysNameR==NULL){
            printf("找两个结构体是不是父子关系。\n");
            nboolean child=find(ltyp,rtyp);
            if(!child){
               printf("右值是结构体，但不是左值的儿子。\n");
               return -14;
            }
            //printf("右值是结构体，是左值的儿子。\n");
            return 0;
         }
      }

      if(codel==UNION_TYPE && coder!=UNION_TYPE){
         printf("左值是联合体，右值不是。\n");
         return -15;
      }

      if(codel==UNION_TYPE && coder==UNION_TYPE){
         if(ltyp==rtyp){
            printf("两个联合体是一样的。\n");
            return 0;
         }
         printf("左值与右值联合体不是同一类型。\n");
         return -15;
      }
   }//end pointer
   return 0;
}

int func_pointer_check(tree lhs,tree rhs,int *paramNum)
{
    tree lfunctype=getFunctionType(lhs);
    tree rfunctype=getFunctionType(rhs);
    int lvarargs=1;
    int rvarargs=1;

    int lcount=getParams(lfunctype,&lvarargs);
    int rcount=getParams(rfunctype,&rvarargs);
    if(lcount!=rcount){
        //n_error("参数数量不匹配：l:%d r:%d\n",lcount,rcount);
        printf("左值参数个数据与右值不等。。左:%d 右:%d\n",lcount,rcount);
        return -1;
    }

    tree lretn=TREE_TYPE (lfunctype);
    tree rretn=TREE_TYPE (rfunctype);
    int ret=compare(lretn,rretn);
    if(ret!=0){
        printf("左值的返回值与右值类型不一样。ret:%d\n",ret);
        return ret;
    }

    tree lparams[100];
    getParamTree(lfunctype,lparams);
    tree rparams[100];
    getParamTree(rfunctype,rparams);
    int i;
    for(i=0;i<lcount;i++){
        ret=compare(lparams[i],rparams[i]);
        if(ret!=0){
            printf("参数不匹配。i:%d ret:%d\n",i,ret);
            *paramNum=i;
            return ret;
        }
    }

    //取出参数个数
    return 0;
}

/**
 * 两个函数指针的比较，可以参数不一样
 */
int func_pointer_check_two(tree lhs,tree rhs,int *paramNum)
{
   tree lfunctype=getFunctionType(lhs);
   tree rfunctype=getFunctionType(rhs);
   int lvarargs=1;
   int rvarargs=1;
   int lcount=getParams(lfunctype,&lvarargs);
   int rcount=getParams(rfunctype,&rvarargs);
   tree lretn=TREE_TYPE (lfunctype);
   tree rretn=TREE_TYPE (rfunctype);
   int ret=compare(lretn,rretn);
   if(ret!=0){
      printf("左值的返回值与右值类型不一样。ret:%d\n",ret);
      return ret;
   }

   tree lparams[100];
   getParamTree(lfunctype,lparams);
   tree rparams[100];
   getParamTree(rfunctype,rparams);
   int i;
   for(i=0;i<lcount;i++){
      //printf("there ------ i: %d lcount:%d rcount:%d\n",i,lcount,rcount);
      if(i==rcount)
         return 0;
      ret=compare(lparams[i],rparams[i]);
      if(ret!=0){
         printf("func_pointer_check_two 参数不匹配。i:%d ret:%d\n",i,ret);
         *paramNum=i;
         return ret;
      }
   }
   //取出参数个数
   return 0;
}

static void funcPointerInit(FuncPointer *self)
{
   self->collectFuncPointer=n_ptr_array_new();
}


/**
 * 是否是正在编译泛型块函数
 */
static nboolean atCompileGenericBlock(FuncPointer *self)
{
   return (current_function_decl
      && aet_parser_is_generic_state(self->parser)
      && generic_util_is_block_func_name(IDENTIFIER_POINTER(DECL_NAME(current_function_decl))));
}


static int getDirective(tree componentRef)
{
   const char *str = aet_utils_get_const_type_string(componentRef,NULL);
   if(!str)
      return -1;
   //str是 aet_generic_E
   int count = 0;
   Directive **ds = generic_parser_get_directive(generic_parser_get(),&count);
   int i;
   for(i=0;i<count;i++)
      if(str[strlen(str)-1]==ds[i]->declName[0])
         return i;
   return -1;
}

typedef struct _FuncPointerCall
{
   tree blockFunc;  //所在泛型块函数
   tree funcPointer;//被调用的函数指针
   Directive **directive;//所在泛型块的编译指示，像这样"E_int_0_4,F_GreatFunc_1_8"
   int unitCount;
   int genDeclPos;//泛型声明在声明模型的位置，从0开始
   VarEntity *entity;//类变量 类型是泛型声明 F
}FuncPointerCall;

static nboolean findFuncPointerCall(FuncPointer *self,tree blockFunc,tree funcPointer)
{
   int i;
   for(i=0;i<self->collectFuncPointer->len;i++){
      FuncPointerCall *item = n_ptr_array_index(self->collectFuncPointer,i);
      if(item->blockFunc ==blockFunc && item->funcPointer == funcPointer)
         return TRUE;
   }
   return FALSE;
}

static Directive **cloneDirective(int *dsc)
{
   int count = 0;
   Directive **ds = generic_parser_get_directive(generic_parser_get(),&count);
   int i;
   Directive **dest=xmalloc(sizeof(Directive *)*count);
   for(i=0;i<count;i++){
      Directive *src = ds[i];
      Directive *item=n_slice_new(Directive);
      item->declName = n_strdup(src->declName);
      item->defineTypeName = n_strdup(src->defineTypeName);
      item->pointerCount = src->pointerCount;
      item->size = src->size;
      dest[i] = item;
   }
   *dsc = count;
   return dest;
}

static bool walk_modified (tree *tp, int *walk_subtrees, void *data)
{
   tree var = (tree) data;

   if (TREE_CODE (*tp) == MODIFY_EXPR || TREE_CODE (*tp) == INIT_EXPR){
      tree lhs = TREE_OPERAND (*tp, 0);
      if (lhs == var || (TREE_CODE (lhs) == SSA_NAME && SSA_NAME_VAR (lhs) == var))
         return true;   /* 找到修改 */
   }
   return NULL_TREE;
}

/**
 * 如果类变量声明：F compare
 * pointer=0
 * genDeclStr = F
 * 查找构造函数中 F 参数出现几次
 * genDeclIndex 记录匹配 varPointer，varGenDeclStr的参数位置
 */
static int getGenParmType(tree fieldDecl,int varPointer,char *varGenDeclStr,int *genDeclIndex)
{
   tree fieldType=TREE_TYPE(fieldDecl);
   tree funcType=TREE_TYPE(fieldType);
   int count = 0; //泛型参数有几个
   int orderNumber = 0;//第几个参数
   int genAtPos = 0;//泛型参数是第几号参数
   for (tree al = TYPE_ARG_TYPES (funcType); al; al = TREE_CHAIN (al)){
      tree type=TREE_VALUE(al);
      if (type == void_type_node)
         break;
      int pointer=0;
      const char *str = aet_utils_get_const_type_string(type,&pointer);
      //printf("getGenParmType 00 %s %s %d\n",IDENTIFIER_POINTER(DECL_NAME(fieldDecl)),str,pointer);
      if(generic_util_is_generic_ident(str)){
         //类中的变量函数指针声明如下: F funcpointer F 对应的 directive是在解析泛型块函数时获取的
         //构造函数中的参数有 F = str[strlen(str)-1]
         //printf("getMatchCtor 22 %d %d %d %d\n",str[strlen(str)-1],varGenDeclStr[0],pointer,varPointer);
         if(str[strlen(str)-1] == varGenDeclStr[0] && pointer == varPointer){
            genAtPos = orderNumber;
            count++;
         }
      }
      orderNumber++;
   }
   *genDeclIndex = genAtPos;
   return count;
}

/**
 * 找出合适的构造函数，满足构造函数中的参数的泛型声明与变量声明的泛型类型一样 ，
 * 并且F参数只能有一个,构造函数必须在class$中声明。
 * genParmPos 记录泛型 F 是第几个参数
 */
static int getMatchCtor(FuncPointerCall *item,ClassFunc **match,int *genParmPos)
{
   int mathcCount = 0;
   ClassName *className = class_mgr_get_class_name_by_sys(class_mgr_get(),item->entity->sysName);
   //类 className中构造函数个数
   NPtrArray *ctorArray = func_mgr_get_constructors(func_mgr_get(),className);
   int i;
   int varPointer=item->directive[item->genDeclPos]->pointerCount;
   const char *varGenDeclStr = item->directive[item->genDeclPos]->declName;
   for(i=0;i<ctorArray->len;i++){
      ClassFunc *func= n_ptr_array_index(ctorArray,i);
      n_debug("funcpointer.c getMatchCtor 00 mangleFunName:%s %d %s\n",func->mangleFunName,varPointer,varGenDeclStr);
      //找出只有一个参数是 F的构造参数
      if(!func->fieldDecl)
         continue;
      int genDeclIndex = 0 ;
      int count=getGenParmType(func->fieldDecl,varPointer,varGenDeclStr,&genDeclIndex);
      if(count == 1){
        //说明构造函数中只有一个F参数，也匹配 F *指针模式
         n_debug("funcpointer.c getMatchCtor 11 找到一个匹配的构造函数:%s %s genAtPos：%d\n",
               item->entity->sysName,func->mangleFunName,genDeclIndex);
         match[mathcCount] = func;
         genParmPos[mathcCount] = genDeclIndex;
         mathcCount++;
      }
   }
   return mathcCount;
}

//从已调用的构造函数中查找函数指针参数，调用的构造函数记录在ClassCtor中
static tree catchCtorFuncPointer(FuncPointerCall *item,ClassFunc *func,int pos)
{
   ClassCtor *classCtor = class_impl_get()->classCtor;
   int i,j;
   NPtrArray *ctors=classCtor->recordCtorArray;
   //printf("catchCtor 调用构造函数的数量:%d\n",ctors->len);
   for(i=0;i<ctors->len;i++){
      tree call = n_ptr_array_index(ctors,i);
      tree fn=CALL_EXPR_FN(call);
      gcc_assert(TREE_CODE(fn)==COMPONENT_REF);
      tree indirect = TREE_OPERAND(fn,0);
      tree field = TREE_OPERAND(fn,1);
     // printf("funcpointer 构造函数的call %p %p %p %p pos:%d\n",call,field,func->fieldDecl,func->fromImplDefine,pos);
      if(field==func->fieldDecl || field == func->fromImplDefine){
         //找出泛型参数声明是第几个 得到了传给类中final$ funcpointer的实参
         //比较调用的构造函数的引用的泛型定义与item中的dirtive是否相同
         tree funcpointer = CALL_EXPR_ARG(call, pos);
         aet_print_tree(call);
         aet_print_tree(funcpointer);
         GenericModel *m1=c_aet_get_generics_model(indirect);
         if(m1==NULL)
            m1=c_aet_get_generics_model(TREE_OPERAND(indirect,0));
         if(m1==NULL)
            return NULL_TREE;
         //开始比较
         int  genericCount =  generic_model_get_count(m1);
         gcc_assert(genericCount==item->unitCount);
         for(j=0;j<genericCount;j++){
            GenericUnit *unit= generic_model_get(m1,j);
            Directive *directive=item->directive[j];
            if(strcmp(directive->defineTypeName,unit->name) || directive->pointerCount!=unit->pointerCount)
               return NULL_TREE;
         }
         n_debug("变量引用的构造函数，变量的泛型定义与泛型块的定义是一样的:%s funcpointer:%p\n",
               generic_model_tostring(m1),funcpointer);
         return funcpointer;
      }
   }
   return NULL_TREE;
}


/**
 * 正在编译泛型块函数,如果ref是一个函数指针调用
 * 并且初始值是类泛型变量
 * /**
 * 围绕泛型块函数
 * "E_int_0_4,F_GreatFunc_1_8"
static inline  void _int_0_GreatFunc_1_TFirst__gen_block_func_0(TFirst * self,aet_generic_E _aetGenNewParamPrefix_a)
 {
E a= *((E *)_aetGenNewParamPrefix_a);
 ACompareFunc compareFunc = ( ACompareFunc ) cmptcs ;
  lastcompare ( & a , & b ) ;
 }
 * FuncPointerCall记录的是函数指针
 * ref代表的是变量 compareFunc
 * 如果有实始值判断是不是类变量并且类型是泛型
 */
void func_pointer_add(FuncPointer *self,tree call)
{
   if(!atCompileGenericBlock(self)  || !call)
      return;
   if(TREE_CODE(call)!=CALL_EXPR)
      return;
   tree fn=CALL_EXPR_FN(call);
   if(TREE_CODE(fn)!=VAR_DECL && TREE_CODE(fn)!=NOP_EXPR)
      return;
   tree type =TREE_TYPE(fn);
   if(!(TREE_CODE(type)==POINTER_TYPE && TREE_CODE(TREE_TYPE(type))==FUNCTION_TYPE))
      return;
   tree cmpref = NULL;
   if(TREE_CODE(fn)==VAR_DECL){
      n_debug("func_pointer_add 11 函数指针变量的初始值\n");
      //ACompareFunc lastcompare =cmptcs;
      //E tysw = 5;
      //lastcompare(&a,&tysw);
      tree init = DECL_INITIAL(fn);
      aet_print_tree(init);
      if(!init)
         return ;
      if(!(TREE_CODE(init)==NOP_EXPR && TREE_CODE(TREE_OPERAND(init,0))==COMPONENT_REF))
         return;
      cmpref = TREE_OPERAND(init,0);
   }else if(TREE_CODE(fn)==NOP_EXPR){
      n_debug("func_pointer_add 22 类型F的类变量转为函数指针然后直接调用。\n");
      //((ACompareFunc)cmptcs)(&a,&tysw);
      cmpref = TREE_OPERAND(fn,0);
   }else{
      aet_print_tree_skip_debug(call);
      n_error("在 func_pointer_add 还未支持的类型");
      return;
   }
   //2.获取非直接引用的类
   tree indirect = TREE_OPERAND(cmpref,0);
   tree op0 = TREE_OPERAND(indirect,0);
   if(TREE_CODE(op0)==PARM_DECL && !strcmp(IDENTIFIER_POINTER(DECL_NAME(op0)),"self")){
      char *sysName  = class_util_get_class_name(TREE_TYPE(op0));
      tree field = TREE_OPERAND(cmpref,1);
      VarEntity *entity = var_mgr_get_var(var_mgr_get(),sysName, IDENTIFIER_POINTER(DECL_NAME(field)));
      //3.判断变量是不是final$类型,如果是，隐藏一个语法规制就是从构造函数来给final变量赋值的。
      if(!entity || !entity->isFinal)
         return;
      //4.查找类变量类型是F ,F泛型声明对应的定义位置 F-->GreatFunc
      int index=getDirective(cmpref);
      if(index<0)
         return;
      if(findFuncPointerCall(self,current_function_decl,fn))
         return;
      n_debug("func_pointer_add 33 加入函数指针调用 index:%d\n",index);
      int dsc = 0;
      Directive **directive=cloneDirective(&dsc);
      FuncPointerCall *item=n_slice_new(FuncPointerCall);
      item->blockFunc = current_function_decl;
      item->funcPointer = fn;//可能是泛型块中的局部变量，也可能是类型是F的类中函数指针变量
      item->directive = directive;
      item->unitCount = dsc;
      item->genDeclPos = index;//类声明的变量类型是F,F在泛型模型中的序号
      item->entity = entity;
      n_ptr_array_add(self->collectFuncPointer,item);
   }
}

typedef struct _WalkData{
    tree  funcPointer;
    tree funcDecl;
}WalkData;

static  vec<tree, va_gc> *createParm(tree callExpr)
{
   vec<tree, va_gc> *parmVec;
   parmVec = make_tree_vector ();
   int i = 0;
   tree arg;
   call_expr_arg_iterator iter;
   FOR_EACH_CALL_EXPR_ARG (arg, iter, callExpr){
      vec_safe_push (parmVec, arg);
      i++;
   }
   return parmVec;
}

/**
 * 把self->xxx() 替换成 yyy()
 * 如果方法是private$才能替换，否则不允许，因为子类可能重载覆盖该方法.
 * 语义是在父类中调用了子类覆盖的方法
 */
static tree replace_cb (tree *tp, int *walk_subtrees, void *data)
{
   WalkData *dp = (WalkData *)data;
   tree t = *tp;
   if (TYPE_P (t))
      *walk_subtrees = 0;
  // else if (TREE_CODE (t) == BIND_EXPR){
      //walk_tree (&BIND_EXPR_BODY (t), replace_cb, data, NULL);
   else if(TREE_CODE(t)==CALL_EXPR){
      tree func=CALL_EXPR_FN(t);
      if(func==dp->funcPointer){
         vec<tree, va_gc> *parms=createParm(t);
         tree newCallExpr = c_build_function_call_vec (EXPR_LOCATION(t), vNULL,dp->funcDecl,parms, NULL);
         n_debug("funcpointer.c 成功替换函数指针调用为函数调用 %s\n",IDENTIFIER_POINTER(DECL_NAME(dp->funcDecl)));
         *tp=newCallExpr;
         release_tree_vector (parms);
      }
   }
   return NULL_TREE;
}


/**
 * 围绕泛型块函数
 * "E_int_0_4,F_GreatFunc_1_8"
static inline  void _int_0_GreatFunc_1_TFirst__gen_block_func_0(TFirst * self,aet_generic_E _aetGenNewParamPrefix_a)
 {
E a= *((E *)_aetGenNewParamPrefix_a);
 ACompareFunc lastcompare = ( ACompareFunc ) cmptcs ;
  lastcompare ( & a , & b ) ;
 }
 * FuncPointerCall记录的是函数指针
 */
void  func_pointer_optimize(FuncPointer *self)
{
   n_debug("func_pointer_optimize 00 优化数量:%d\n",self->collectFuncPointer->len);
   if(self->collectFuncPointer->len==0)
      return;
   int i,j;
   for(i=0;i<self->collectFuncPointer->len;i++){
      FuncPointerCall *item = n_ptr_array_index(self->collectFuncPointer,i);
      bool isModified = walk_tree (&DECL_SAVED_TREE (item->blockFunc),
            walk_modified, item->funcPointer, NULL);
      n_debug("func_pointer_optimize 11 modify:%d\n",isModified);
      if(isModified)
         continue;
      ClassFunc *matchFuncs[20];//适合的构造函数有那些
      int genPos[20];  //每个构造函数中F参数所在的位置
      int matchCount = getMatchCtor(item,matchFuncs,genPos);
      n_debug("func_pointer_optimize 22 mathcCount:%d\n",matchCount);
      //查找整个编译单元中调用构造函数时，传递的参数
      tree ret=NULL_TREE;//从构造函数中找出的函数指针
      for(j=0;j<matchCount;j++){
         tree funcpointer =  catchCtorFuncPointer(item,matchFuncs[j],genPos[j]);
         if(funcpointer && !ret){
            ret = funcpointer;
         }else if(funcpointer && ret){
            if(ret!=funcpointer){
               printf("说明有两个不同的函数指针 %p %p\n",funcpointer,ret);
               ret = NULL_TREE;
               break;
            }
         }
      }
      if(ret){
         //查找当前编译单元，ret是不是一个函数定义
         //add_expr-->function_decl
         tree fndecl = TREE_OPERAND(ret,0);
         tree body = DECL_SAVED_TREE (fndecl);
         if(body){
            //可以替换了，说明函数体就在本编译单元。
            WalkData data={item->funcPointer,fndecl};
            walk_tree (&DECL_SAVED_TREE(item->blockFunc), replace_cb, &data, NULL);
         }
      }
   }
}

FuncPointer  *func_pointer_get()
{
   static FuncPointer *singleton = NULL;
   if (!singleton){
       singleton =n_slice_alloc0 (sizeof(FuncPointer));
       funcPointerInit(singleton);
       singleton->parser = aet_parser_get();
   }
   return singleton;
}


