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
#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "c-family/c-common.h"
#include "langhooks.h"
#include "opts.h"

#include "c/c-tree.h"
#include "c-family/name-hint.h"
#include "c-family/known-headers.h"
#include "c-family/c-spellcheck.h"
#include "c-aet.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "c/gimple-parser.h"
#include "tree-iterator.h"
#include "../libcpp/include/cpplib.h"

#include "aet-c-parser-header.h"
#include "aetutils.h"
#include "genericimpl.h"
#include "aetinfo.h"
#include "varmgr.h"
#include "aetprinttree.h"
#include "aetprinttoken.h"
#include "classmgr.h"
#include "classutil.h"
#include "genericutil.h"
#include "classparser.h"
#include "classimpl.h"
#include "blockmgr.h"
#include "genericblock.h"
#include "genericinfo.h"
#include "funcmgr.h"
#include "makefileparm.h"
#include "middlefile.h"
#include "aetlib.h"


static void blockMgrInit(BlockMgr *self)
{
	self->lhs=NULL_TREE;
	self->infoCount=0;
	self->currentBlockName=NULL;
	self->blockFileName=NULL;
}


static inline void c_parser_check_literal_zero (c_parser *parser, unsigned *literal_zero_mask,unsigned int idx)
{
  if (idx >= HOST_BITS_PER_INT)
    return;
  c_token *tok = c_parser_peek_token (parser);
  switch (tok->type)
    {
    case CPP_NUMBER:
    case CPP_CHAR:
    case CPP_WCHAR:
    case CPP_CHAR16:
    case CPP_CHAR32:
    case CPP_UTF8CHAR:
      /* If a parameter is literal zero alone, remember it
	 for -Wmemset-transposed-args warning.  */
      if (integer_zerop (tok->value)
	  && !TREE_OVERFLOW (tok->value)
	  && (c_parser_peek_2nd_token (parser)->type == CPP_COMMA
	      || c_parser_peek_2nd_token (parser)->type == CPP_CLOSE_PAREN))
	*literal_zero_mask |= 1U << idx;
    default:
      break;
    }
}

static vec<tree, va_gc> *c_parser_expr_list (BlockMgr *self, bool convert_p, bool fold_p,
		    vec<tree, va_gc> **p_orig_types,location_t *sizeof_arg_loc, tree *sizeof_arg,
		    vec<location_t> *locations,unsigned int *literal_zero_mask)
{
   c_parser *parser = self->parser->parser;
    vec<tree, va_gc> *ret;
    vec<tree, va_gc> *orig_types;
    struct c_expr expr;
    unsigned int idx = 0;

    ret = make_tree_vector ();
    if (p_orig_types == NULL){
        n_debug("c_parser_expr_list 00 _orig_types == NULL");
       orig_types = NULL;
    }else{
        n_debug("c_parser_expr_list 11 orig_types = make_tree_vector ()");
       orig_types = make_tree_vector ();
    }

    if (literal_zero_mask)
       c_parser_check_literal_zero (parser, literal_zero_mask, 0);
    n_debug("c_parser_expr_list 22 开始赋值 convert_p:%d fold_p:%d",convert_p,fold_p);
    expr = aet_parser_c_parser_expr_no_commas/*!c_parser_expr_no_commas*/(self->parser, NULL);
    if (convert_p){
       expr = convert_lvalue_to_rvalue (expr.get_location (), expr, true, true);
    }
    if (fold_p){
       expr.value = c_fully_fold (expr.value, false, NULL);
    }
    ret->quick_push (expr.value);
    n_debug("c_parser_expr_list 33 开始赋值 ",get_tree_code_name(TREE_CODE(expr.value)));
    if (orig_types)
       orig_types->quick_push (expr.original_type);
    if (locations)
       locations->safe_push (expr.get_location ());
    if (sizeof_arg != NULL && expr.original_code == SIZEOF_EXPR){
       sizeof_arg[0] = c_last_sizeof_arg;
       sizeof_arg_loc[0] = c_last_sizeof_loc;
    }
    while (c_parser_next_token_is (parser, CPP_COMMA)){
       c_parser_consume_token (parser);
       if (literal_zero_mask)
	      c_parser_check_literal_zero (parser, literal_zero_mask, idx + 1);
       expr = aet_parser_c_parser_expr_no_commas/*!c_parser_expr_no_commas*/(self->parser, NULL);
       if (convert_p)
	      expr = convert_lvalue_to_rvalue (expr.get_location (), expr, true,true);
       if (fold_p)
	      expr.value = c_fully_fold (expr.value, false, NULL);
       vec_safe_push (ret, expr.value);
       n_debug("c_parser_expr_list 44 开始赋值  expr.value:%p %p",
    		   expr.value,TREE_TYPE(expr.value),get_tree_code_name(TREE_CODE(expr.value)));
       if (orig_types)
	      vec_safe_push (orig_types, expr.original_type);
       if (locations)
	      locations->safe_push (expr.get_location ());
       if (++idx < 3 && sizeof_arg != NULL && expr.original_code == SIZEOF_EXPR){
	      sizeof_arg[idx] = c_last_sizeof_arg;
	      sizeof_arg_loc[idx] = c_last_sizeof_loc;
	   }
    }
    if (orig_types)
      *p_orig_types = orig_types;
    return ret;
}

static void c_parser_skip_to_pragma_eol (c_parser *parser, bool error_if_not_eol = true)
{
  gcc_assert (parser->in_pragma);
  parser->in_pragma = false;

  if (error_if_not_eol && c_parser_peek_token (parser)->type != CPP_PRAGMA_EOL)
    c_parser_error (parser, "expected end of line");

  cpp_ttype token_type;
  do
    {
      c_token *token = c_parser_peek_token (parser);
      token_type = token->type;
      if (token_type == CPP_EOF)
	break;
      c_parser_consume_token (parser);
    }
  while (token_type != CPP_PRAGMA_EOL);

  parser->error = false;
}

static void c_parser_consume_pragma (c_parser *parser)
{
  gcc_assert (!parser->in_pragma);
  gcc_assert (parser->tokens_avail >= 1);
  gcc_assert (parser->tokens[0].type == CPP_PRAGMA);
  if (parser->tokens != &parser->tokens_buf[0])
    parser->tokens++;
  else if (parser->tokens_avail >= 2)
    {
      parser->tokens[0] = parser->tokens[1];
      if (parser->tokens_avail >= 3)
	parser->tokens[1] = parser->tokens[2];
    }
  parser->tokens_avail--;
  parser->in_pragma = true;
}

/**
 * 原型来自aetparser.h
 */
static void c_parser_skip_to_end_of_block_or_statement (c_parser *parser,NString *codes)
{
   unsigned nesting_depth = 0;
   bool save_error = parser->error;
   enum cpp_ttype previewType=0;
   while (true){
      c_token *token;
      /* Peek at the next token.  */
      token = c_parser_peek_token (parser);
      switch (token->type){
         case CPP_EOF:
            return;

         case CPP_PRAGMA_EOL:
            if (parser->in_pragma)
               return;
            break;

         case CPP_SEMICOLON:
            /* If the next token is a ';', we have reached the
            end of the statement.  */
            if (!nesting_depth){
               /* Consume the ';'.  */
               c_parser_consume_token (parser);
               goto finished;
            }
            break;

         case CPP_CLOSE_BRACE:
            /* If the next token is a non-nested '}', then we have
            reached the end of the current block.  */
            if (nesting_depth == 0 || --nesting_depth == 0){
               c_parser_consume_token (parser);
               goto finished;
            }
            break;

         case CPP_OPEN_BRACE:
            /* If it the next token is a '{', then we are entering a new
            block.  Consume the entire block.  */
            ++nesting_depth;
            break;

         case CPP_PRAGMA:
            /* If we see a pragma, consume the whole thing at once.  We
            have some safeguards against consuming pragmas willy-nilly.
            Normally, we'd expect to be here with parser->error set,
            which disables these safeguards.  But it's possible to get
            here for secondary error recovery, after parser->error has
            been cleared.  */
            c_parser_consume_pragma (parser);
            c_parser_skip_to_pragma_eol (parser);
            parser->error = save_error;
            continue;
         default:
            break;
      }
      char *source=aet_utils_convert_token_to_string(token);
      n_string_append(codes,source);
      if(previewType==CPP_CLOSE_PAREN && token->type==CPP_SEMICOLON){
         n_string_append(codes,"\n");
      }else{
         n_string_append(codes," ");
      }
      previewType=token->type;
      c_parser_consume_token (parser);
   }
finished:
   parser->error = false;
}

/**
 * 取源代码的一段
 */
static char *getBlockFromSourceCodes(location_t start,location_t end)
{
	expanded_location xs;
	xs = expand_location(start);
	int startRows=xs.line;
	int startPos= xs.column;
	xs = expand_location(end);
	int endRows=xs.line;
	int endPos= xs.column;
	cpp_reader *pfile=parse_in;
	cpp_buffer *buffer=pfile->buffer;
	const unsigned char *str=buffer->buf;        /* Entire character buffer.  */
	NString *codes=n_string_new(str);
	int startLoc=0;
	int endLoc=0;
	int i=1;
	for(i=1;i<startRows;i++){
	   int loc=n_string_indexof_from(codes,"\n",startLoc);
	   startLoc=loc+1;
	}
	for(i=1;i<endRows;i++){
	   int loc=n_string_indexof_from(codes,"\n",endLoc);
	   endLoc=loc+1;
	}
	if(endLoc<startLoc){
		endLoc=codes->len;
	}

	//roew is :115 28 117 5 2314 2316
	n_debug("roew is :%d %d %d %d %d %d",startRows,startPos,endRows,endPos,startLoc,endLoc);
	NString *blockCodes=n_string_substring_from(codes,startLoc,endLoc);
	n_string_free(codes,TRUE);
	n_debug("blockcodes is :%s",blockCodes->str);
	return n_string_free(blockCodes,FALSE);
}



static GenericInfo *getInfoAndCreate(BlockMgr *self,ClassName *className)
{
	int i;
	for(i=0;i<self->infoCount;i++){
		GenericInfo *info=self->genericInfos[i];
		if(generic_info_same(info,className))
			return info;
	}
    GenericInfo *info =generic_info_new(className);
    self->genericInfos[self->infoCount++]=info;
	return info;
}

static GenericInfo *getInfo(BlockMgr *self,ClassName *className)
{
	int i;
	for(i=0;i<self->infoCount;i++){
		GenericInfo *info=self->genericInfos[i];
		if(generic_info_same(info,className))
			return info;
	}
	return NULL;
}

/**
 * 把结构体的域与块函数匹配
 */
static void genBlockFuncCompileOver(BlockMgr *self)
{
   ClassImpl    *impl=class_impl_get();
   ClassName *className=impl->className;
   c_parser *parser=self->parser->parser;
   GenericInfo *info=NULL;
   int i;
   for(i=0;i<self->infoCount;i++){
      GenericInfo *item=self->genericInfos[i];
      ClassName *name=item->className;
      if(strcmp(name->sysName,className->sysName)==0){
         info=item;
         break;
      }
   }
   if(info==NULL){
      error_at(input_location,"在blockmgr发生不可知的错误！genBlockFuncCompileOver");
      n_error("报告此错误！");
      return;
   }
   if(self->currentBlockName==NULL){
      error_at(input_location,"在blockmgr发生不可知的错误！genBlockFuncCompileOver currentBlockName==NULL");
      n_error("报告此错误！");
      return;
   }
   GenericBlock *block=generic_info_get_block(info,self->currentBlockName);
   if(block==NULL || generic_block_is_compile(block)){
      error_at(input_location,"在blockmgr发生不可知的错误！block==NULL || generic_block_is_compile(block)");
      n_error("报告此错误！");
   }

   tree funcId=aet_utils_create_ident(self->currentBlockName);
   tree decl=lookup_name(funcId);
   printf("genBlockFuncCompileOver %s\n",self->currentBlockName);
   if(!aet_utils_valid_tree(decl)){
      error_at(input_location,"在blockmgr发生不可知的错误！函数%qs是空的",self->currentBlockName);
      n_error("报告此错误！");
   }
   //tree  field=generic_info_get_field(info,self->currentBlockName);
 //  generic_block_match_field_and_func(block,decl,field);
   generic_block_set_compile(block,TRUE);
   n_free(self->currentBlockName);
   self->currentBlockName=NULL;
}

/**
 * 编译泛型块文件
 */
static void compileFile(BlockMgr *self)
{
   //printf("generic_expand_create_generic_class 00 %s\n",in_fnames[0]);
   MakefileParm  *makefileParm=makefile_parm_get();
   FILE *fp=fopen(makefileParm->compileFileName,"r");
   char buffer[1024*500];
   int rev=fread(buffer,1,1024*500,fp);
   buffer[rev]='\0';
   fclose(fp);
   aet_utils_add_token(parse_in,buffer,rev);
}

/*
 * 解析关键字 aet_goto_compile$ RID_AET_GOTO_STR
 * 返回false c-parser也返回，否则c-parser继续处理。
 */
nboolean  block_mgr_parser_goto(BlockMgr *self,nboolean start_attr_ok,AetGotoTag re)
{
   c_parser *parser=self->parser->parser;
   if(re==GOTO_GENERIC_BLOCK_FUNC_TEST_COMPILE_START){
      aet_parser_set_test_generic_block_state(self->parser,TRUE);
      c_parser_consume_token (parser);//consume   RID_AET_GOTO
      c_parser_consume_token (parser);//consume   GOTO_GENERIC_BLOCK_FUNC_TEST_COMPILE_START
      return TRUE;
   }else if(re==GOTO_GENERIC_BLOCK_FUNC_TEST_COMPILE_END){
      aet_parser_set_test_generic_block_state(self->parser,FALSE);
      c_parser_consume_token (parser);//consume   RID_AET_GOTO
      c_parser_consume_token (parser);//consume   GOTO_GENERIC_BLOCK_FUNC_TEST_COMPILE_END
      n_info("测试泛型块函数结束了\n");
      genBlockFuncCompileOver(self);
      return FALSE;
   }else if(re==GOTO_READY_COMPILE_GENERIC_BLOCK_FUNC){//新加的 20025-11-10
        c_parser_consume_token (parser);//consume   RID_AET_GOTO
        c_parser_consume_token (parser);//consume   GOTO_READY_COMPILE_GENERIC_BLOCK_FUNC
        MakefileParm  *makefileParm=makefile_parm_get();
        n_info("到文件尾了，11 插入文件的内容%s %s\n", makefileParm->compileFileName,in_fnames[0]);
        compileFile(self);
        return FALSE;
   }else if(re==GOTO_ENTER_COMPILE_GENERIC_BLOCK_FUNC){
      c_parser_consume_token (parser);//consume   RID_AET_GOTO
      c_parser_consume_token (parser);//consume   GOTO_ENTER_COMPILE_GENERIC_BLOCK_FUNC
      generic_parser_enter(generic_parser_get());
      n_info("解析某个泛型定义，进入编译泛型块函数\n");
      return FALSE;
   }else{
      error_at(input_location,"在 block_mgr_parser_goto 不可知的错误！");
      n_error("报告此错误！");
   }
   return FALSE;
}

/**
 * 把块当成内部函数编译
 * 但是会引起 编译器内部错误：在 gimplify_expr 中，于 gimplify.c:14363
 * 在同一个函数内有块又调用泛型函数，testBlock会出错
 */
static void testBlock(BlockMgr *self,GenericBlock *block)
{
   char *source=generic_block_create_codes(block);
   if(self->currentBlockName!=NULL){
      n_free(self->currentBlockName);
      self->currentBlockName=NULL;
   }
   self->currentBlockName=n_strdup(generic_block_get_name(block));
   NString *codes=n_string_new("");
   n_string_append_printf(codes,"%s %d %s\n",RID_AET_GOTO_STR,GOTO_GENERIC_BLOCK_FUNC_TEST_COMPILE_START,source);
   n_string_append_printf(codes,"%s %d\n",RID_AET_GOTO_STR,GOTO_GENERIC_BLOCK_FUNC_TEST_COMPILE_END);
   printf("testBlock----- %s\n",codes->str);
   aet_utils_add_token(parse_in,codes->str,codes->len);
   n_string_free(codes,TRUE);
   n_free(source);
}


/**
 * 解析 genericblock$(a1){
 * }
 * 替换成以下调用表达式
 * ((Abc *)self->_generic_blocks_1234)->func_1(self,a1)
 */
struct c_expr  block_mgr_parser(BlockMgr *self)
{
   struct c_expr ret;
   c_parser *parser=self->parser->parser;
   if(!self->parser->isAet){
      c_parser_error (parser, "关键字genericblock$只能用在类实现中。%<;%>");
      c_parser_consume_token (parser);
      block_mgr_set_lhs(self,NULL_TREE);
      return ret;
   }
   ClassImpl  *classImpl=class_impl_get();
   ClassName *className=classImpl->className;
   ClassInfo *classInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
   tree currentFunc=current_function_decl;
   char *funcName=IDENTIFIER_POINTER(DECL_NAME(currentFunc));
   ClassFunc *classFunc=func_mgr_get_func(func_mgr_get(),currentFunc);
   nboolean isFuncGen=class_func_is_func_generic(classFunc);
   if(!class_info_is_generic_class(classInfo) && !isFuncGen){
      //是不是泛型函数
      error_at(input_location,"%qs泛型块只能用在泛型类或泛型函数中。",className->userName);
      c_parser_consume_token (parser);
      return ret;
   }
   c_parser_consume_token (parser);
   //作为参数解析
   location_t loc=   c_parser_peek_token (parser)->location;
   if (!c_parser_next_token_is (parser, CPP_OPEN_PAREN)){
      error_at(loc,"语法出错了：格式：genericblock$(){...};。");
      block_mgr_set_lhs(self,NULL_TREE);
      return ret;
   }
   vec<tree, va_gc> *exprlist;
   c_parser_consume_token(parser);
   if (c_parser_next_token_is (parser, CPP_CLOSE_PAREN))
      exprlist = make_tree_vector ();
   else
      exprlist = c_parser_expr_list(self, true, false, NULL,NULL,NULL,NULL,NULL);

   tree selftree=lookup_name(get_identifier("self"));
   vec_safe_insert(exprlist,0,selftree);//插入self参数到第一个位置
   c_parser_skip_until_found (parser, CPP_CLOSE_PAREN, "expected %<)%>");
   location_t startLoc=c_parser_peek_token (parser)->location;
   NString *body=n_string_new("");
   aet_print_token(c_parser_peek_token (parser));
   c_parser_skip_to_end_of_block_or_statement(parser,body);
   n_string_append(body,"\n");
   location_t endLoc=c_parser_peek_token (parser)->location;
   aet_print_token(c_parser_peek_token (parser));
   //c_parser_skip_until_found (parser, CPP_SEMICOLON, "expected %<;%>");//加这句话 int gen=genericblock(){};出问题，但genericblock(){};不会
   aet_print_token(c_parser_peek_token (parser));
   char *body1=getBlockFromSourceCodes(startLoc,endLoc);
   n_debug("block body 两种不同的源代码:%s\n 第二种： %s",body->str,body1);
   NString  *bodys=n_string_substring(body,1);//去除{
   n_string_free(body,TRUE);
   body=bodys;
   //生成名字和参数

   GenericInfo *ginfo=getInfoAndCreate(self,className);
   GenericBlock *block=generic_info_add_block(ginfo,self->lhs,exprlist,
         body->str,classFunc->mangleFunName,isFuncGen);
   class_func_add_generic_block(classFunc);
   generic_block_set_loc(block,startLoc,endLoc);
   generic_block_print(block);
   //testBlock(self,block); //在同一个函数内有块又调用泛型函数，testBlock会出错
   //生成如下表达式:
   //(1)非泛型函数内(*(setData_1_typedecl)self->_gen_blocks_array_897[0])(self,5);
   //(2)泛型函数(*(setData_1_typedecl)self->_gen_blocks_array_897[0])(self,5);
   tree value=  generic_block_get_call(block);
   n_string_free(body,TRUE);
   n_free(body1);
   block_mgr_set_lhs(self,NULL_TREE);
   ret.value= value;
   set_c_expr_source_range (&ret, startLoc,endLoc);
   ret.original_code = ERROR_MARK;
   ret.original_type = NULL;
   if (exprlist){
      release_tree_vector (exprlist);
   }
   return ret;
}

/**
 * int abc=genericblock$(){};
 * int abc 就是lhs,lhs是在c_parser_expr_no_commas生成的
 * rhs是 genericblock$(){};但不知道lhs是什么，在这里设后，
 * genericblock就知道返回的类型了
 * lhs只有generic_block创建call时调用，取的是lhs的TREE_TYPE(lhs);
 * 如果 return genericblock$(... lhs是当前函数的TREE_TYPE,进入generic_block再调用
 * TREE_TYPE(lsh)刚好是当前函数的rtn。
 */
void  block_mgr_set_lhs(BlockMgr *self,tree lhs)
{
   self->lhs=lhs;
}

int block_mgr_get_block_count(BlockMgr *self,ClassName *className)
{
	GenericInfo *info=getInfo(self,className);
	if(info==NULL){
		n_debug("找不到GenericInfo :%s",className->sysName);
		return 0;
	}
   return generic_info_get_block_count(info);
}

//整个编译单元是不有泛型块函数
nboolean  block_mgr_have_block(BlockMgr *self)
{
   return self->infoCount>0;
}

//类中函数有多少个泛型块
int   block_mgr_get_block_count_by_func(BlockMgr *self,ClassFunc *func)
{
   if(!func)
      return 0;
   GenericInfo *info=getInfo(self,func->className);
   if(info==NULL){
      return 0;
   }
   return generic_info_get_block_count_by_belong(info,func->mangleFunName);
}


/**
 * 保存块到xxx.block.o文件中
 * xxx是in_fnames[0]对应的输出文件名
 */
void block_mgr_save(BlockMgr *self)
{
   if(makefile_parm_is_second_compile(makefile_parm_get())){
      n_debug("blockmgr.c block_mgr_save.c 是第二次编译 %s 不需要写入任何信息。\n",in_fnames[0]);
      return;
   }
   char  *objfile=makefile_parm_get_object_file(makefile_parm_get());
 //  printf("block_mgr_save --- 没有genericinfo说明没有块:%d %s\n",self->infoCount,objfile);
   char newName[255];
   sprintf(newName,"%s.block_new.o",objfile);
   //如果没有泛型块，移走原来的块代码文件
   if(self->infoCount==0){
      remove(newName);//移走块代码文件
      return;
   }
   int i;
   NString *codes=n_string_new("");
   for(i=0;i<self->infoCount;i++){
      GenericInfo *info=self->genericInfos[i];
      char *re=generic_info_save(info);
      n_string_append(codes,re);
      n_free(re);
   }
   gcc_assert(codes->len>0);
   FILE *fp=fopen(newName,"w");
   int rx=fwrite(codes->str,1,codes->len,fp);
   fclose(fp);
   n_string_free(codes,TRUE);
   gcc_assert( self->blockFileName==NULL);
   self->blockFileName=n_strdup(newName);
   middle_file_modify(middle_file_get(),COMPILE_BLOCK);
}

/**
 * 所有的有GenericInfo和GenericBlock对象的文件内容全部合并成一个字符串
 */
static char * readLocaFile(char *localFileList)
{
   if(!localFileList || strlen(localFileList)==0)
      return NULL;
   nchar **items=n_strsplit(localFileList,"\n",-1);
   int length= n_strv_length(items);
   int i;
   NString *codes=n_string_new("");
   for(i=0;i<length;i++){
      char *fn=items[i];
      FILE *fp=fopen(fn,"r");
      if(fp){
         char buffer[1024*150];
         int rev=fread(buffer,1,1024*150,fp);
         if(rev>0){
            buffer[rev]='\0';
            n_string_append(codes,buffer);
            n_string_append(codes,"\n");
         }
         fclose(fp);
      }
   }
   n_strfreev(items);
   if(codes->len==0){
      n_string_free(codes,TRUE);
      return NULL;
   }
   return n_string_free(codes,FALSE);
}

/**
 * 调用该方法时正在编译middlefile.c
 * GENERIC_BLOCK_INDEX_FILE 保存的是有泛型块的文件名列表。
 * 这些文件名是在各个编译单元中创建的文件，名字如xxx_block.o
 * 在这些文件中存放字符串化GenericInfo和GenericBlock
 * 进入这里属于编译temp_func_track_45.c 主要靠gcc.cc中传递的参数获取在aetcollect中收集的数据
 * content的内容如下:
   class_block start:
     TFirst
     /home/sns/workspace/ai/src/debug/ai0.c 类实现所在文件
     1  是不是泛型类
     block start:
     _Z6TFirst7setdataEPN6TFirstE 块所在函数
     0  所在函数是不是泛型函数
     0  索引号
     void _TFirst__inner_generic_func_0(TFirst * self,int xe)
     {
        int f ;
     }
     block end:
     class_block end:
 */
void block_mgr_ready(BlockMgr *self)
{
   char *fileName = getenv("GCC_AET_BLOCK_LIST_PATH");
   if(fileName==NULL ||strlen(fileName)==0){
      return;
   }
   FILE *fp=fopen(fileName,"r");
   char *content = NULL;
   if(fp){
      char fileList[50*1024];
      int rev=fread(fileList,1,50*1024,fp);
      fclose(fp);
      fileList[rev]='\0';
      content = readLocaFile(fileList);
   }

   if(self->saveString){
      free(self->saveString);
      self->saveString=NULL;
   }
   //存放的是本项目所有的genericinfo
   NPtrArray *genInfoArrayFromLocal= generic_info_create_info(content);
   //从库中生成的genInfo
   NPtrArray *genInfoArrayFromLib=aet_lib_get_generic_info_and_block(aet_lib_get());
   self->saveString=content;
   if(self->outputArray){
      n_ptr_array_unref(self->outputArray);
      self->outputArray=NULL;
   }
   self->outputArray=n_ptr_array_new();
   int i;
   if(genInfoArrayFromLocal){
      for(i=0;i<genInfoArrayFromLocal->len;i++)
         n_ptr_array_add(self->outputArray,n_ptr_array_index(genInfoArrayFromLocal,i));
      n_ptr_array_unref(genInfoArrayFromLocal);
   }
   if(genInfoArrayFromLib){
      //printf("从库取内容 ---- %d\n",genInfoArrayFromLib->len);
      for(i=0;i<genInfoArrayFromLib->len;i++)
         n_ptr_array_add(self->outputArray,n_ptr_array_index(genInfoArrayFromLib,i));
      n_ptr_array_unref(genInfoArrayFromLib);
   }
}

char *block_mgr_get_save(BlockMgr *self)
{
   return self->saveString;
}

NPtrArray *block_mgr_get_output_generic_info(BlockMgr *self)
{
   return self->outputArray;
}

GenericInfo   *block_mgr_get_info (BlockMgr *self,ClassName *className)
{
   if(className==NULL)
      return NULL;
   GenericInfo *genericInfos[20];
   int infoCount;
   int i;
   for(i=0;i<self->infoCount;i++){
      GenericInfo *f=self->genericInfos[i];
      if(f!=NULL && !strcmp(f->className->sysName,className->sysName)){
         return f;
      }
   }
   return NULL;
}


BlockMgr *block_mgr_get()
{
   static BlockMgr *singleton = NULL;
   if (!singleton){
      singleton =n_slice_alloc0 (sizeof(BlockMgr));
      blockMgrInit(singleton);
      singleton->parser = aet_parser_get();
   }
   return singleton;
}


