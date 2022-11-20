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
//#include "parserstatic.h"
//#include "aet-c-parser-header.h"
//#include "classfunc.h"
//#include "genericutil.h"
//#include "funcmgr.h"
//#include "genericcall.h"
//#include "blockmgr.h"
//#include "genericquery.h"
//#include "parserhelp.h"
//#include "funccheck.h"
//#include "makefileparm.h"
#include "classutil.h"
//#include "middlefile.h"


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
  aet_print_tree_skip_debug(fieldtype);
  int ret=comptypes (lhs_main_type, fieldtype);
  if(ret==1)
      return TRUE;
  return find(lhs_main_type,fieldtype);

}



/**
 * 右边的参数与左边的比较。如果正确返回 init
 */
static int  compare(tree ltype, tree rtype)
{
  enum tree_code code = TREE_CODE (ltype);
  tree lmain= TYPE_MAIN_VARIANT (ltype);
  tree rmain= TYPE_MAIN_VARIANT (rtype);
  int rlcmpmain=comptypes (rmain,lmain);
  int rlcmptype=comptypes (rtype,ltype);
//  printf("comapre -----left_main_variant\n");
//  aet_print_tree_skip_debug(lmain);
//  printf("comapre -----right_main_variant\n\n");
//  aet_print_tree_skip_debug(rmain);
//  printf("\n\n");
//  printf("comapre -----ltype\n\n");
//  aet_print_tree_skip_debug(ltype);
//  printf("\n\n");
//  printf("comapre -----rtype\n\n");
//  aet_print_tree_skip_debug(rtype);

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





