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
#include "cgraph.h"
#include "attribs.h"

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

#include "tree-pretty-print.h"
#include "tree-dump.h"
#include "langhooks.h"
#include "tree-iterator.h"
#include "c/c-tree.h"


#include "aetutils.h"
#include "classmgr.h"
#include "classinfo.h"
#include "aetinfo.h"
#include "aet-c-common.h"
#include "aet-typeck.h"
#include "aetprinttree.h"
#include "genericimpl.h"
#include "c-aet.h"
#include "aet-c-parser-header.h"
#include "classutil.h"
#include "newobject.h"
#include "genericutil.h"
#include "genericmodel.h"
#include "classparser.h"
#include "classimpl.h"


#define NONE ""

static void newObjectInit(NewObject *self)
{
    self->newStack=new_stack_new();
    self->newHeap=new_heap_new();
    self->newField=new_field_new();
    self->returnNewObject=FALSE;//??????return???????????? new$ AObject
}

static void c_parser_skip_to_end_of_parameter (c_parser *parser,NString *codes)
{
  unsigned nesting_depth = 0;
  while (true){
      c_token *token = c_parser_peek_token (parser);
      if ((token->type == CPP_COMMA || token->type == CPP_SEMICOLON)  && !nesting_depth)
          break;
      /* If we've run out of tokens, stop.  */
      if (token->type == CPP_EOF)
         return;
      if (token->type == CPP_PRAGMA_EOL && parser->in_pragma)
          return;
      if (token->type == CPP_OPEN_BRACE || token->type == CPP_OPEN_PAREN || token->type == CPP_OPEN_SQUARE)
             ++nesting_depth;
      else if (token->type == CPP_CLOSE_BRACE  || token->type == CPP_CLOSE_PAREN || token->type == CPP_CLOSE_SQUARE){
         if (nesting_depth-- == 0)
            break;
      }
      char *source=aet_utils_convert_token_to_string(token);
      if(strcmp(source,"new$")==0){
          n_string_append(codes,source);
          n_string_append(codes," ");
      }else{
          n_string_append(codes,source);
      }
      /* Consume this token.  */
      c_parser_consume_token (parser);
    }
    parser->error = false;
}

static nboolean checkCtor(NewObject *self,GenericModel **model,ClassInfo **classInfo)
{
   c_parser *parser=self->parser;
   c_token *token = c_parser_peek_token (parser);
   if(token->type!=CPP_NAME){
     error_at(token->location,"?????????new$???????????????????????????");
     return FALSE;
   }
   if(token->id_kind!=C_ID_TYPENAME){
      error_at(token->location,"?????????new$??????????????????????????????????????????%qs?????????????????????????????????????????????????????????include???????????????????????????",IDENTIFIER_POINTER(token->value));
      return FALSE;
   }
  ClassInfo *info=class_mgr_get_class_info(class_mgr_get(),IDENTIFIER_POINTER(token->value));
  if(info==NULL){
     error_at(token->location,"%qs??????????????????????????????",IDENTIFIER_POINTER(token->value));
     return FALSE;
  }
  *classInfo=info;
  if(class_info_is_abstract_class(info)){
     error_at(token->location,"?????????%qs???????????????????????????",info->className.userName);
     return FALSE;
  }
  if(class_info_is_interface(info)){
     error_at(token->location,"?????????%qs???????????????????????????",info->className.userName);
     return FALSE;
  }
  ClassName *className=&info->className;
  c_parser_consume_token (parser); //consume AObject
  if(c_parser_peek_token (parser)->type == CPP_LESS){
      ClassInfo *info=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
      if(!info->genericModel){
         error_at(token->location,"???%qs?????????????????????",className->userName);
         return FALSE;
      }
      GenericModel *genModel=generic_model_new(parser->isAet,GEN_FROM_OBJECT_NEW$);
      *model=genModel;
  }
  return TRUE;
}

static ClassName *getBelongClassName(NewObject *self)
{
     c_parser *parser=self->parser;
     ClassParser  *classPaser=class_parser_get();
     ClassImpl  *classImpl=class_impl_get();
     nboolean pasering=class_parser_is_parsering(classPaser);
     n_debug("getBelongClassName--- %d %p",parser->isAet,current_function_decl);
     if(parser->isAet){
          return classImpl->className;
     }else if(pasering && classPaser->currentClassName){
        n_debug("???????????? classparsering %s",classPaser->currentClassName->userName);
        return classPaser->currentClassName;
     }
     return NULL;
}

/**
 * ??????generic_impl_check_new$
 */
static nboolean genericCheck(NewObject *self,location_t loc,ClassName *varClassName,GenericModel *varGen)
{
    ClassName *belongClassName=getBelongClassName(self);
    GenericModel *belongGen=NULL;
    GenericModel *funcGen=NULL;
    if(belongClassName){
       ClassInfo *belongInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),belongClassName);
       belongGen= belongInfo->genericModel;
    }
    //printf("genericCheck---- %p\n",belongGen);
    if(current_function_decl){
        funcGen=c_aet_get_func_generics_model(current_function_decl);
    }
    if(varGen!=NULL){
        ClassInfo *varClassInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),varClassName);
        int c1=generic_model_get_count(varClassInfo->genericModel);
        int c2=generic_model_get_count(varGen);
        if(c1!=c2){
           error_at(loc,"???%qs?????????????????????????????????????????????",varClassName->userName);
           return FALSE;
        }
        if(generic_model_get_undefine_count(varGen)==0){
            return TRUE;
        }
        if(belongGen==NULL && funcGen==NULL){
               char *name=generic_model_get_first_decl_name(varGen);
               error_at(loc,"????????????%qs????????????%qs???????????????",varClassName->userName,name);
               return FALSE;
        }
        nboolean re=TRUE;
        if(belongGen && funcGen){
            //????????????????????????
            GenericModel *merge=generic_model_merge(belongGen,funcGen);
            re=generic_model_include_decl(merge,varGen);
            if(!re){
                error_at(loc,"????????????????????????????????????????????????%qs???????????????%qs???????????????",belongClassName->userName,IDENTIFIER_POINTER(DECL_NAME(current_function_decl)));
            }
            generic_model_free(merge);
//         re=generic_model_include_decl(belongGen,varGen);
//         if(!re){
//            re=generic_model_include_decl(funcGen,varGen);
//            if(!re){
//                if(belongClassName)
//                  error_at(loc,"????????????????????????????????????????????????%qs???????????????",belongClassName->userName);
//               else
//                  error_at(loc,"????????????????????????????????????????????????????????????");
//            }
//         }
        }else if(belongGen && !funcGen){
           re=generic_model_include_decl(belongGen,varGen);
           if(!re){
              error_at(loc,"????????????????????????????????????????????????%qs???????????????",belongClassName->userName);
           }
        }else if(!belongGen && funcGen){
            re=generic_model_include_decl(funcGen,varGen);
            if(!re)
               error_at(loc,"????????????????????????????????????????????????????????????");
        }else{
            re=FALSE;
            char *name=generic_model_get_first_decl_name(varGen);
            error_at(loc,"????????????%qs????????????%qs???????????????",varClassName->userName,name);
        }
        return re;
    }else{
        error_at(loc,"?????????????????????????????????????????????");
        return FALSE;
    }
}

static nboolean checkVar(NewObject *self,tree decl,GenericModel *genericsModel)
{
   if(TREE_CODE(decl)!=VAR_DECL && TREE_CODE(decl)!=FIELD_DECL)
      return FALSE;
   tree type=TREE_TYPE(decl);
   char *className=class_util_get_class_name(type);
   if(className==NULL)
      return FALSE;
   //???????????????????????????????????????????????????????????????
   c_parser *parser=self->parser;
   if (c_parser_next_token_is_keyword (parser, RID_AET_NEW)){
      c_parser_consume_token (parser);//consume new$
      c_token *token = c_parser_peek_token (parser);
      location_t ctorLoc=token->location;
      GenericModel *genericsDefineModel=NULL;
      ClassInfo *info=NULL;
      if(!checkCtor(self,&genericsDefineModel,&info))
          return FALSE;
      ClassName *className=&info->className;
      if(TREE_CODE(type)==RECORD_TYPE){
         char *sysName=class_util_get_class_name_by_record(type);
         if(strcmp(className->sysName,sysName)){
             char *varName=IDENTIFIER_POINTER(DECL_NAME(decl));
             error_at(token->location,"??????%qs????????????%qs???????????????%qs????????????",varName,sysName,className->userName);
             return FALSE;
         }
      }
      if(genericsDefineModel && genericsModel){
         nboolean equal=  generic_model_equal(genericsDefineModel,genericsModel);
         if(!equal){
             //error_at(token->location,"????????????????????????????????????????????????");
            // return FALSE;
             n_warning("????????????????????????");
         }
         nboolean ok=genericCheck(self,token->location,className,genericsDefineModel);
         if(!ok){
             return FALSE;
         }
      }else if(genericsDefineModel && !genericsModel){
         nboolean ok=genericCheck(self,token->location,className,genericsDefineModel);
         if(!ok){
             return FALSE;
         }
      }else if(!genericsDefineModel && genericsModel){
         nboolean ok=genericCheck(self,token->location,className,genericsModel);
         if(!ok){
             return FALSE;
         }
      }
      if(genericsDefineModel)
          c_aet_set_generics_model(decl,genericsDefineModel);
      else if(genericsModel)
          c_aet_set_generics_model(decl,genericsModel);
      if(c_parser_peek_token (parser)->type!=CPP_OPEN_PAREN){
          error_at(c_parser_peek_token (parser)->location,"???????????????????????????");
          return FALSE;
      }
      NString *codes=n_string_new("");
      c_parser_skip_to_end_of_parameter(parser,codes);
      n_string_insert(codes,0,info->className.userName);
      n_debug("?????????????????????????????????:%s\n",codes->str);
      tree ctorCodes=aet_utils_create_ident(codes->str);
      tree sysClassName=aet_utils_create_ident(info->className.sysName);
      c_aet_set_ctor(decl,ctorCodes,sysClassName,ctorLoc);
      n_string_free(codes,TRUE);
      return TRUE;
   }
   return FALSE;
}

/**
 * ?????????
 * Abc abc=new$ Abc();
 * new_object_parser??? Abc abc=new$ Abc();??????
 * Abc abc={0};new$ Abc();???c-parser??????finish_decl???
 * ????????????new_object_ctor
 */
void  new_object_parser(NewObject *self,tree decl,GenericModel *genericsModel)
{
      c_parser *parser=self->parser;
      nboolean check=checkVar(self,decl,genericsModel);
      tree type=TREE_TYPE(decl);
      if(check && TREE_CODE(type)==RECORD_TYPE){
          c_aet_set_decl_method(decl,CREATE_OBJECT_USE_STACK);
          int tokenCount=parser->tokens_avail;
          int i;
          for(i=tokenCount;i>0;i--){
             aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+4]);
          }
          parser->tokens_avail=tokenCount+4;
          aet_utils_create_token(&parser->tokens[0],CPP_OPEN_BRACE,"{",1);
          aet_utils_create_number_token(&parser->tokens[1],0);
          aet_utils_create_token(&parser->tokens[2],CPP_CLOSE_BRACE,"}",1);
          aet_utils_create_token(&parser->tokens[3],CPP_SEMICOLON,";",1);
          aet_print_token_in_parser("??????????????????????????? 11");
      }else if(check && TREE_CODE(type)==POINTER_TYPE){
          if (c_parser_peek_token (parser)->type==CPP_SEMICOLON){
               c_parser_consume_token (parser);//consume ";" new Abc();
          }
          c_aet_set_decl_method(decl,CREATE_OBJECT_USE_HEAP);
          int tokenCount=parser->tokens_avail;
          int i;
          for(i=tokenCount;i>0;i--){
             aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+2]);
          }
          parser->tokens_avail=tokenCount+2;
          aet_utils_create_number_token(&parser->tokens[0],0);
          aet_utils_create_token(&parser->tokens[1],CPP_SEMICOLON,";",1);
          aet_print_token_in_parser("?????????????????? 11");
      }
}

/**
 * Abc abc=new$ Abc();
 * ???c-paser??????
 */
nboolean  new_object_ctor(NewObject *self,tree decl)
{
    c_parser *parser=self->parser;
    int method=c_aet_get_create_method(decl);
    if(method!=CREATE_OBJECT_USE_STACK && method!=CREATE_OBJECT_USE_HEAP)
        return FALSE;
    if(method==CREATE_OBJECT_USE_STACK)
        new_stack_init_object(self->newStack,decl);
    else if(method==CREATE_OBJECT_USE_HEAP){
       new_heap_create_object(self->newHeap,decl);
    }
    return TRUE;
}

static nboolean  modifyUseStack(NewObject *self,tree decl)
{
   c_parser *parser=self->parser;
   tree type=TREE_TYPE(decl);
   char *className=class_util_get_class_name(type);
   if(className==NULL)
       return FALSE;
   int method=c_aet_get_create_method(decl);
   if(method!=CREATE_CLASS_METHOD_UNKNOWN){ //???????????????????????????
        error_at(input_location,"??????%qs??????????????????",className);
        return FALSE;
   }
   GenericModel *genericDefineModel=c_aet_get_generics_model(decl);
   c_parser_consume_token (parser);//consume =
   nboolean check=checkVar(self,decl,genericDefineModel);
   printf("modifyUseStack 00 checkVar %d\n",check);
   if(!check)
      return FALSE;
   new_stack_modify_object(self->newStack,decl);
   c_aet_set_modify_stack_new(decl,1);
   int tokenCount=parser->tokens_avail;
   int i;
   for(i=tokenCount;i>0;i--){
     aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+1]);
   }
   parser->tokens_avail=tokenCount+1;
   aet_utils_create_token(&parser->tokens[0],CPP_SEMICOLON,";",1);
   return TRUE;
}

static nboolean  modifyUseHeap(NewObject *self,tree decl)
{
   c_parser *parser=self->parser;
   tree type=TREE_TYPE(decl);
   char *className=class_util_get_class_name(type);
   if(className==NULL)
       return FALSE;
   GenericModel *genericDefineModel=c_aet_get_generics_model(decl);
   c_parser_consume_token (parser);//consume =
   nboolean check=checkVar(self,decl,genericDefineModel);
   if(!check)
      return FALSE;
   if (c_parser_peek_token (parser)->type==CPP_SEMICOLON){
       c_parser_consume_token (parser);//consume ";" new Abc();
   }
   new_heap_modify_object(self->newHeap,decl);
   return TRUE;
}

static nboolean modifyUseComponentRef(NewObject *self,tree decl)
{
   c_parser *parser=self->parser;
   tree type=TREE_TYPE(decl);
   if(TREE_CODE(type)!=RECORD_TYPE && TREE_CODE(type)!=POINTER_TYPE)
       return FALSE;
   char *sysClassName=class_util_get_class_name(type);
   n_debug("modifyUseComponentRef --- %s",sysClassName);
   if(sysClassName==NULL)
       return FALSE;
   tree field=TREE_OPERAND (decl, 1);
   if(TREE_CODE(field)!=FIELD_DECL)
       return FALSE;
   GenericModel *genericDefineModel=c_aet_get_generics_model(field);
   c_parser_consume_token (parser);//consume =
   nboolean check=checkVar(self,field,genericDefineModel);
   if(!check){
      n_warning("?????????????????????????????????%s",sysClassName);
      return FALSE;
   }
   if(TREE_CODE(type)==RECORD_TYPE){
       //?????????
       n_debug("modifyUseComponentRef ---????????? %s",sysClassName);
       new_field_modify_object(self->newField,decl);
       c_aet_set_modify_stack_new(decl,1);
   }else if(TREE_CODE(type)==POINTER_TYPE){
       if (c_parser_peek_token (parser)->type==CPP_SEMICOLON){
           c_parser_consume_token (parser);//consume ";" new Abc();
       }
       new_field_modify_object(self->newField,decl);
   }
   return TRUE;
}

/**
 * abc=new$ Abc();
 */
nboolean  new_object_modify(NewObject *self,tree decl)
{
   c_parser *parser=self->parser;
   if(TREE_CODE(decl)!=VAR_DECL && TREE_CODE(decl)!=COMPONENT_REF)
      return FALSE;
   if(TREE_CODE(decl)==VAR_DECL){
       tree type=TREE_TYPE(decl);
       if(TREE_CODE(type)==RECORD_TYPE){
           return modifyUseStack(self,decl);
       }else if(TREE_CODE(type)==POINTER_TYPE){
           return modifyUseHeap(self,decl);
       }
   }else if(TREE_CODE(decl)==COMPONENT_REF){
       return modifyUseComponentRef(self,decl);
   }
   return FALSE;
}

void    new_object_finish(NewObject *self,CreateClassMethod method,tree func)
{
    if(method==CREATE_OBJECT_METHOD_STACK)
      new_stack_finish(self->newStack);
    else if(method==CREATE_OBJECT_METHOD_HEAP || method==CREATE_OBJECT_METHOD_NO_DECL_HEAP)
      new_heap_finish(self->newHeap,method,func);
    else if(method==CREATE_OBJECT_METHOD_FIELD_STACK || method==CREATE_OBJECT_METHOD_FIELD_HEAP)
      new_field_finish(self->newField,method,func);
}

static nboolean isFromStack(NewObject *self)
{
    if(!current_function_decl)
        return FALSE;
    if(!self->returnNewObject)
        return FALSE;
    new_object_set_return_newobject(self,FALSE);
    tree  decl = DECL_RESULT (current_function_decl);
    tree type=TREE_TYPE(decl);
    if(TREE_CODE(type)!=RECORD_TYPE)
        return FALSE;
    return TRUE;
}

/**
 * ?????? new$ Abc();??????????????? new$ Abc()
 */
nboolean  new_object_parser_new$(NewObject *self)
{
   c_parser *parser=self->parser;
   c_parser_consume_token (parser);//consume new$
   c_token *token = c_parser_peek_token (parser);
   if(!current_function_decl){
      error_at(token->location,"????????????????????????????????????");
      return FALSE;
   }
   GenericModel *genericsDefine=NULL;
   ClassInfo *info=NULL;
   if(!checkCtor(self,&genericsDefine,&info))
      return FALSE;
   ClassName *className=&info->className;
   NString *codes=n_string_new("");
   c_parser_skip_to_end_of_parameter(parser,codes);
   n_string_insert(codes,0,info->className.userName);
   nboolean fromStack=isFromStack(self);
   n_debug("?????????????????????????????????:%s fromstack:%d\n",codes->str,fromStack);
   if(!fromStack)
     new_heap_create_object_no_decl(self->newHeap,className,genericsDefine,codes->str,self->isParserParmsState);
   else
     new_stack_create_object_no_name(self->newStack,className,genericsDefine,codes->str);
   n_string_free(codes,TRUE);
   aet_print_token(c_parser_peek_token (parser));
   return TRUE;
}

/**
 * ?????? return new$ Abc ?????????????????????????????????????????????
 */
void new_object_set_return_newobject(NewObject *self,nboolean returnNewObject)
{
    self->returnNewObject=returnNewObject;
}

/**
 *  ??????????????????????????????????????????
 */
void new_object_set_parser_parms_state(NewObject *self,nboolean isParserParmsState)
{
    self->isParserParmsState=isParserParmsState;
}


/**
 * ?????????new????????????????????????????????????
 * class Abc {
 *    static AObject *obj=new$ AObject();
 * }
 * ??????checkVar???decl???????????????????????????????????????
 */
char * new_object_parser_for_static(NewObject *self,tree decl,GenericModel *genericsModel)
{
      c_parser *parser=self->parser;
      nboolean check=checkVar(self,decl,genericsModel);
      tree type=TREE_TYPE(decl);
      char *codes=NULL;
      if(check && TREE_CODE(type)==RECORD_TYPE){
          n_debug("new_object_parser_for_static ??????????????????????????? 11???");
          //new_stack_init_object(self->newStack,decl);
      }else if(check && TREE_CODE(type)==POINTER_TYPE){
//        if (c_parser_peek_token (parser)->type==CPP_SEMICOLON){
//             c_parser_consume_token (parser);//consume ";" new Abc();
//        }
          codes=new_heap_create_object_for_static(self->newHeap,decl);
          n_debug("new_object_parser_for_static ?????????????????? 11 %s",codes);
      }
      return codes;

}


void   new_object_set_parser(NewObject *self,c_parser *parser)
{
      self->parser=parser;
      new_strategy_set_parser((NewStrategy*)self->newStack,parser);
      new_strategy_set_parser((NewStrategy*)self->newHeap,parser);
      new_strategy_set_parser((NewStrategy*)self->newField,parser);
}

NewObject *new_object_get()
{
    static NewObject *singleton = NULL;
    if (!singleton){
         singleton =n_slice_alloc0 (sizeof(NewObject));
         newObjectInit(singleton);
    }
    return singleton;
}
