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
#include "aetutils.h"
#include "genericimpl.h"
#include "aetinfo.h"
#include "varmgr.h"
#include "aetprinttree.h"
#include "classmgr.h"
#include "aet-c-parser-header.h"
#include "classutil.h"
#include "genericutil.h"
#include "classparser.h"
#include "classimpl.h"
#include "blockmgr.h"
#include "genericblock.h"
#include "genericinfo.h"
#include "genericfile.h"
#include "funcmgr.h"
#include "genericexpand.h"
#include "genericcommon.h"
#include "classlib.h"


static void blockMgrInit(BlockMgr *self)
{
	self->lhs=NULL_TREE;
	self->infoCount=0;
	self->currentBlockName=NULL;
	self->collectNewObject=n_ptr_array_new();
	self->defineFuncCallArray=n_ptr_array_new();
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

static vec<tree, va_gc> *c_parser_expr_list (c_parser *parser, bool convert_p, bool fold_p,
		    vec<tree, va_gc> **p_orig_types,location_t *sizeof_arg_loc, tree *sizeof_arg,
		    vec<location_t> *locations,unsigned int *literal_zero_mask)
{
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
    n_debug("c_parser_expr_list 22 ???????????? convert_p:%d fold_p:%d",convert_p,fold_p);
    expr = c_c_parser_expr_no_commas (parser, NULL);
    if (convert_p){
       expr = convert_lvalue_to_rvalue (expr.get_location (), expr, true, true);
    }
    if (fold_p){
       expr.value = c_fully_fold (expr.value, false, NULL);
    }
    ret->quick_push (expr.value);
    n_debug("c_parser_expr_list 33 ???????????? ",get_tree_code_name(TREE_CODE(expr.value)));
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
       expr = c_c_parser_expr_no_commas (parser, NULL);
       if (convert_p)
	      expr = convert_lvalue_to_rvalue (expr.get_location (), expr, true,true);
       if (fold_p)
	      expr.value = c_fully_fold (expr.value, false, NULL);
       vec_safe_push (ret, expr.value);
       n_debug("c_parser_expr_list 44 ????????????  expr.value:%p %p",
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

static void c_parser_skip_to_end_of_block_or_statement (c_parser *parser,NString *codes)
{
  unsigned nesting_depth = 0;
  bool save_error = parser->error;
  enum cpp_ttype previewType=0;
  while (true)
    {
      c_token *token;

      /* Peek at the next token.  */
      token = c_parser_peek_token (parser);

      switch (token->type)
	{
	case CPP_EOF:
	  return;

	case CPP_PRAGMA_EOL:
	  if (parser->in_pragma)
	    return;
	  break;

	case CPP_SEMICOLON:
	  /* If the next token is a ';', we have reached the
	     end of the statement.  */
	  if (!nesting_depth)
	    {
	      /* Consume the ';'.  */
	      c_parser_consume_token (parser);
	      goto finished;
	    }
	  break;

	case CPP_CLOSE_BRACE:
	  /* If the next token is a non-nested '}', then we have
	     reached the end of the current block.  */
	  if (nesting_depth == 0 || --nesting_depth == 0)
	    {
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
 * ?????????????????????
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
	char *result=n_strdup(blockCodes->str);
	n_string_free(blockCodes,TRUE);
	return result;
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
 * ????????????????????????????????????
 */
static void genBlockFuncCompileOver(BlockMgr *self)
{
	ClassImpl    *impl=class_impl_get();
	ClassName *className=impl->className;
    c_parser *parser=self->parser;
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
	   error_at(input_location,"???blockmgr???????????????????????????genBlockFuncCompileOver");
	   n_error("??????????????????");
	   return;
	}
	if(self->currentBlockName==NULL){
	   error_at(input_location,"???blockmgr???????????????????????????genBlockFuncCompileOver currentBlockName==NULL");
	   n_error("??????????????????");
	   return;
	}
	GenericBlock *block=generic_info_get_block(info,self->currentBlockName);
	if(block==NULL || generic_block_is_compile(block)){
	   error_at(input_location,"???blockmgr???????????????????????????block==NULL || generic_block_is_compile(block)");
	   n_error("??????????????????");
	}

    tree funcId=aet_utils_create_ident(self->currentBlockName);
    tree decl=lookup_name(funcId);
    printf("genBlockFuncCompileOver %s\n",self->currentBlockName);
    if(!aet_utils_valid_tree(decl)){
	   error_at(input_location,"???blockmgr?????????????????????????????????%qs?????????",self->currentBlockName);
	   n_error("??????????????????");
    }
    tree  field=generic_info_get_field(info,self->currentBlockName);
    generic_block_match_field_and_func(block,decl,field);
   	generic_block_set_compile(block,TRUE);
   	n_free(self->currentBlockName);
   	self->currentBlockName=NULL;
}

/*
 * ??????????????? aet_goto_compile$ RID_AET_GOTO_STR
 * ??????false c-parser??????????????????c-parser???????????????
 */

nboolean  block_mgr_parser_goto(BlockMgr *self,nboolean start_attr_ok,AetGotoTag re)
{
	  c_parser *parser=self->parser;
      if(re==GOTO_GENERIC_BLOCK_FUNC_DECL){
    	  generic_expand_produce_set_block_func_decl(generic_expand_get(),start_attr_ok);
    	  return FALSE;
      }else if(re==GOTO_GENERIC_BLOCK_FUNC_TEST_COMPILE_START){
    	  parser->isTestGenericBlockState=TRUE;
    	  c_parser_consume_token (parser);//consume   RID_AET_GOTO
    	  c_parser_consume_token (parser);//consume   GOTO_GENERIC_BLOCK_FUNC_TEST_COMPILE_START
    	  return TRUE;
      }else if(re==GOTO_GENERIC_BLOCK_FUNC_TEST_COMPILE_END){
    	  parser->isTestGenericBlockState=FALSE;
    	  c_parser_consume_token (parser);//consume   RID_AET_GOTO
    	  c_parser_consume_token (parser);//consume   GOTO_GENERIC_BLOCK_FUNC_TEST_COMPILE_END
    	  n_info("??????????????????????????????\n");
    	  genBlockFuncCompileOver(self);
    	  return FALSE;
      }else if(re==GOTO_GENERIC_BLOCK_FUNC_DEFINE_COMPILE){
		  c_parser_consume_token (parser);//consume   RID_AET_GOTO
		  c_parser_consume_token (parser);//consume  GOTO_GENERIC_BLOCK_FUNC_DEFINE_COMPILE
		  n_info("??????????????????00 ???????????????????????????genericclass ?????????%s ????????????????????????????????????????????????\n",in_fnames[0]);
		  generic_expand_compile_decl(generic_expand_get());
		  return FALSE;
      }else if(re==GOTO_GENERIC_COMPILE_UNDEFINE_BLOCK_FUNC_DECL){
 		  c_parser_consume_token (parser);//consume   RID_AET_GOTO
 		  c_parser_consume_token (parser);//consume   GOTO_GENERIC_COMPILE_UNDEFINE_BLOCK_FUNC_DECL
 		  n_info("?????????????????????????????? ?????????????????????????????? ?????????%s\n",in_fnames[0]);
 		  generic_expand_compile_define(generic_expand_get());
    	  return FALSE;
      }else if(re==GOTO_GENERIC_BLOCK_FUNC_DEFINE_COND){
		  c_parser_consume_token (parser);//consume   RID_AET_GOTO
		  c_parser_consume_token (parser);//consume   GOTO_GENERIC_BLOCK_FUNC_DEFINE_COND
		  generic_expand_produce_cond_define(generic_expand_get());
		  n_info("????????????????????????\n");
		  return FALSE;
       }else{
		  error_at(input_location,"??? block_mgr_parser_goto ?????????????????????");
		  n_error("??????????????????");
      }
      return FALSE;
}

/**
 * ??????????????????????????????
 * ??????????????? ??????????????????????????? gimplify_expr ????????? gimplify.c:14363
 * ???????????????????????????????????????????????????testBlock?????????
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


static void addFuncGenParmInfoNULL(BlockMgr *self,int add)
{
	   c_parser *parser=self->parser;
	   int tokenCount=parser->tokens_avail;
	   int total=20+add;
	   if(tokenCount+total>AET_MAX_TOKEN){
			 error("token?????????");
			 return;
	   }

	   int i;
	   for(i=tokenCount;i>0;i--){
			aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+total]);
	   }
	   aet_utils_create_token(&parser->tokens[0],CPP_NAME,"self",4);
	   aet_utils_create_token(&parser->tokens[1],CPP_COMMA,",",1);
	   aet_utils_create_token(&parser->tokens[2],CPP_NAME,"createFuncGenParmInfo",strlen("createFuncGenParmInfo"));
	   aet_utils_create_token(&parser->tokens[3],CPP_OPEN_PAREN,"(",1);

	   aet_utils_create_token(&parser->tokens[4],CPP_OPEN_PAREN,"(",1);
	   aet_utils_create_token(&parser->tokens[5],CPP_OPEN_PAREN,"(",1);
	   aet_utils_create_void_token(&parser->tokens[6],input_location);
	   aet_utils_create_token(&parser->tokens[7],CPP_MULT,"*",1);
	   aet_utils_create_token(&parser->tokens[8],CPP_CLOSE_PAREN,")",1);
       aet_utils_create_number_token(&parser->tokens[9],0);
	   aet_utils_create_token(&parser->tokens[10],CPP_CLOSE_PAREN,")",1);

	   aet_utils_create_token(&parser->tokens[11],CPP_COMMA,",",1);

	   aet_utils_create_token(&parser->tokens[12],CPP_OPEN_PAREN,"(",1);
	   aet_utils_create_token(&parser->tokens[13],CPP_OPEN_PAREN,"(",1);
	   aet_utils_create_void_token(&parser->tokens[14],input_location);
	   aet_utils_create_token(&parser->tokens[15],CPP_MULT,"*",1);
	   aet_utils_create_token(&parser->tokens[16],CPP_CLOSE_PAREN,")",1);
	   aet_utils_create_number_token(&parser->tokens[17],0);
	   aet_utils_create_token(&parser->tokens[18],CPP_CLOSE_PAREN,")",1);

	   aet_utils_create_token(&parser->tokens[19],CPP_CLOSE_PAREN,")",1);
       if(add)
	      aet_utils_create_token(&parser->tokens[20],CPP_COMMA,",",1);

	  parser->tokens_avail=tokenCount+total;
	  aet_print_token_in_parser("addFuncGenParmInfoNULL----add:%d",add);
}

static void addFuncGenParmInfoHaveBlockInfo(BlockMgr *self,int add)
{
	   c_parser *parser=self->parser;
	   int tokenCount=parser->tokens_avail;
	   int total=15+add;
	   if(tokenCount+total>AET_MAX_TOKEN){
			 error("token?????????");
			 return;
	   }

	   int i;
	   for(i=tokenCount;i>0;i--){
			aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+total]);
	   }
	   aet_utils_create_token(&parser->tokens[0],CPP_NAME,"self",4);
	   aet_utils_create_token(&parser->tokens[1],CPP_COMMA,",",1);
	   aet_utils_create_token(&parser->tokens[2],CPP_NAME,"createFuncGenParmInfo",strlen("createFuncGenParmInfo"));
	   aet_utils_create_token(&parser->tokens[3],CPP_OPEN_PAREN,"(",1);

	   aet_utils_create_token(&parser->tokens[4],CPP_AND,"&",1);
	   aet_utils_create_token(&parser->tokens[5],CPP_NAME,FuncGenParmInfo_NAME,strlen(FuncGenParmInfo_NAME));

	   aet_utils_create_token(&parser->tokens[6],CPP_COMMA,",",1);

	   aet_utils_create_token(&parser->tokens[7],CPP_OPEN_PAREN,"(",1);
	   aet_utils_create_token(&parser->tokens[8],CPP_OPEN_PAREN,"(",1);
	   aet_utils_create_void_token(&parser->tokens[9],input_location);
	   aet_utils_create_token(&parser->tokens[10],CPP_MULT,"*",1);
	   aet_utils_create_token(&parser->tokens[11],CPP_CLOSE_PAREN,")",1);
	   aet_utils_create_number_token(&parser->tokens[12],0);
	   aet_utils_create_token(&parser->tokens[13],CPP_CLOSE_PAREN,")",1);

	   aet_utils_create_token(&parser->tokens[14],CPP_CLOSE_PAREN,")",1);
       if(add)
	      aet_utils_create_token(&parser->tokens[15],CPP_COMMA,",",1);

	  parser->tokens_avail=tokenCount+total;
	  aet_print_token_in_parser("addFuncGenParmInfoHaveBlockInfo ----add:%d",add);
}

static void addFuncGenParmInfoFromtempFgpi1234(BlockMgr *self,int add)
{
	   c_parser *parser=self->parser;
	   int tokenCount=parser->tokens_avail;
	   int total=3+add;
	   if(tokenCount+total>AET_MAX_TOKEN){
			 error("token?????????");
			 return;
	   }

	   int i;
	   for(i=tokenCount;i>0;i--){
			aet_utils_copy_token(&parser->tokens[i-1],&parser->tokens[i-1+total]);
	   }
	   aet_utils_create_token(&parser->tokens[0],CPP_NAME,"self",4);
	   aet_utils_create_token(&parser->tokens[1],CPP_COMMA,",",1);
	   aet_utils_create_token(&parser->tokens[2],CPP_NAME,AET_FUNC_GENERIC_PARM_NAME,strlen(AET_FUNC_GENERIC_PARM_NAME));
       if(add)
	      aet_utils_create_token(&parser->tokens[3],CPP_COMMA,",",1);
	   parser->tokens_avail=tokenCount+total;
	   aet_print_token_in_parser("addFuncGenParmInfoFromtempFgpi1234 ----add:%d",add);
}

static void createFuncGenParmInfoActual(BlockMgr *self,int add)
{
	tree id=aet_utils_create_ident(AET_FUNC_GENERIC_PARM_NAME);
	tree funcGenParmInfo=lookup_name(id);
	char *funcName=IDENTIFIER_POINTER(DECL_NAME(current_function_decl));
	if(aet_utils_valid_tree(funcGenParmInfo)){
		n_debug("blockmgr ?????????????????????????????????1???????????????????????? tempFgpi1234 belongFunc:%s %s",funcName,in_fnames[0]);
		addFuncGenParmInfoFromtempFgpi1234 (self,add);
	}else{
		tree id=aet_utils_create_ident(FuncGenParmInfo_NAME);
		tree fggb=lookup_name(id);
		if(aet_utils_valid_tree(fggb)){
		    n_debug("blockmgr ?????????????????????????????????2????????????.c??????????????? _inFileGlobalFuncGenParmInfo ?????????????????? &_inFileGlobalFuncGenParmInfo belongFunc:%s %s",
					funcName,in_fnames[0]);
			addFuncGenParmInfoHaveBlockInfo(self,add);

		}else{
		    n_debug("blockmgr ?????????????????????????????????3????????? tempFgpi1234???_inFileGlobalFuncGenParmInfo ?????????????????? belongFunc:%s %s",funcName,in_fnames[0]);
		   addFuncGenParmInfoNULL(self,add);
		}
	}
}
/**
 * ?????? genericblock$(a1){
 * }
 * ??????????????????????????????
 * ((Abc *)self->_generic_blocks_1234)->func_1(self,a1)
 */
struct c_expr  block_mgr_parser(BlockMgr *self)
{
    struct c_expr ret;
	c_parser *parser=self->parser;
    if(!parser->isAet){
	  c_parser_error (parser, "?????????genericblock$???????????????????????????%<;%>");
	  c_parser_consume_token (parser);
	  block_mgr_set_lhs(self,NULL_TREE);
	  return ret;
    }
	ClassImpl  *classImpl=class_impl_get();
    ClassName *className=classImpl->className;
    ClassInfo *classInfo=class_mgr_get_class_info_by_class_name(class_mgr_get(),className);
    tree currentFunc=current_function_decl;
	char *funcName=IDENTIFIER_POINTER(DECL_NAME(currentFunc));
	nboolean isFuncGen=func_mgr_is_generic_func(func_mgr_get(),className,funcName);
    if(!class_info_is_generic_class(classInfo) && !isFuncGen){
    	   //?????????????????????
		error_at(input_location,"%qs???????????????????????????????????????????????????",className->userName);
		c_parser_consume_token (parser);
		return ret;
    }
	c_parser_consume_token (parser);
    //??????????????????
	location_t loc=	c_parser_peek_token (parser)->location;
    if (!c_parser_next_token_is (parser, CPP_OPEN_PAREN)){
	   error_at(loc,"???????????????????????????genericblock$(){};???");
	   block_mgr_set_lhs(self,NULL_TREE);
	   return ret;
    }
    vec<tree, va_gc> *exprlist;
	c_parser_consume_token(parser);
	int add=c_parser_next_token_is (parser, CPP_CLOSE_PAREN)?0:1;
	createFuncGenParmInfoActual(self,add);
	exprlist = c_parser_expr_list (parser, true, false, NULL,NULL,NULL,NULL,NULL);
    c_parser_skip_until_found (parser, CPP_CLOSE_PAREN, "expected %<)%>");
    location_t startLoc=c_parser_peek_token (parser)->location;
    NString *body=n_string_new("");
    aet_print_token(c_parser_peek_token (parser));
    c_parser_skip_to_end_of_block_or_statement(parser,body);
    n_string_append(body,"\n");
    location_t endLoc=c_parser_peek_token (parser)->location;
    aet_print_token(c_parser_peek_token (parser));
    //c_parser_skip_until_found (parser, CPP_SEMICOLON, "expected %<;%>");//???????????? int gen=genericblock(){};???????????????genericblock(){};??????
    aet_print_token(c_parser_peek_token (parser));
    char *body1=getBlockFromSourceCodes(startLoc,endLoc);
    n_debug("block body ????????????????????????:%s\n ???????????? %s",body->str,body1);
    NString  *bodys=n_string_substring(body,1);//??????{
    n_string_free(body,TRUE);
    body=bodys;
    //?????????????????????

     GenericInfo *ginfo=getInfoAndCreate(self,className);
     GenericBlock *block=generic_info_add_block(ginfo,self->lhs,exprlist,body->str,funcName,isFuncGen);
     generic_block_set_loc(block,startLoc,endLoc);
     generic_block_print(block);
     //testBlock(self,block); //???????????????????????????????????????????????????testBlock?????????
     tree value=  generic_block_get_call(block);//?????????????????????:(* (setData_1_typedecl)self->_gen_blocks_array_897[0])(self,tempFgpi1234,5);
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
 * int abc ??????lhs,lhs??????c_parser_expr_no_commas?????????
 * rhs??? genericblock$(){};????????????lhs??????????????????????????????
 * genericblock???????????????????????????
 */
void  block_mgr_set_lhs(BlockMgr *self,tree lhs)
{
   self->lhs=lhs;
}

int block_mgr_get_block_count(BlockMgr *self,ClassName *className)
{
	GenericInfo *info=getInfo(self,className);
	if(info==NULL){
		n_debug("?????????GenericInfo :%s",className->sysName);
		return 0;
	}
   return generic_info_get_block_count(info);
}

/**
 * ?????????????????????
 * ??????
 * value[0] ?????????
 * value[1] ????????????
 */
char  **block_mgr_get_block_source(BlockMgr *self,ClassName *className,int index)
{
	GenericInfo *info=getInfo(self,className);
	if(info==NULL){
		n_debug("?????????GenericInfo :%s",className->sysName);
		return;
	}
    return generic_info_create_block_source_var_by_index(info,index);
}

/**
 * ??????aet_generic_block_xml.tmp?????????????????????
 */
BlockContent *block_mgr_read_block_content(BlockMgr *self,char *sysName)
{
	BlockContent *bc= generic_file_get_block_content(generic_file_get(),sysName);
	if(bc==NULL){
		//???.so???????????????
		bc=class_lib_get_block_content(class_lib_get(),sysName);
	}
	return bc;
}

////////////////////////////////////////new$ Abc<int>()---------------

/**
 * new$ Abc<int,char>();
 * ??????new$??????????????????????????????????????????genericinfo,???????????????collectNewObject
 */
void block_mgr_add_define_new_object(BlockMgr *self,ClassName *className,GenericModel *defines)
{
    c_parser *parser=self->parser;
    char *belongClass=GENERIC_UNKNOWN_STR;
    if(parser->isAet){
     	ClassImpl  *impl=class_impl_get();
    	GenericInfo *info=getInfoAndCreate(self,impl->className);
    	generic_info_add_define_obj(info,className->sysName,defines);
    	return;
    }
    if(defines==NULL){
    	n_info("??????block_mgr_add_define_new_object??? GenericModel *define????????????sysName:%s",className->sysName);
    	return;
    }
    GenericNewClass  *item=generic_new_class_new(className->sysName,defines);
    int i;
    nboolean find=generic_new_class_exists(self->collectNewObject,item);
    n_debug("new$???????????????????????????????????????????????????????????? %s ???????????????:%d",item->id,find);
    if(!find){
	    n_ptr_array_add(self->collectNewObject,item);
    }else{
    	generic_new_class_free(item);
    }
}

/**
 * ???????????????new?????????????????????????????????
 * ???????????????new$?????????????????????
 * ??????:
 * static LU *lu=new$ LU<int>();
 * impl$ Abc{
 *   setData(){
 *     IMatrix *tt=new$ IMatrix<int>();
 *   }
 * }
 * ???Abc ???????????????????????????
 */
int    block_mgr_get_define_newobj_count(BlockMgr *self,ClassName *className)
{
	GenericInfo *info=getInfo(self,className);
	if(info==NULL){
		n_debug("?????????GenericInfo :%s",className->sysName);
		return 0;
	}
	return generic_info_get_define_obj_count(info)+self->collectNewObject->len;
}

char   **block_mgr_get_define_newobj_source_codes(BlockMgr *self,ClassName *className)
{
	GenericInfo *info=getInfo(self,className);
	if(info==NULL){
		n_debug("?????????GenericInfo :%s",className->sysName);
		return NULL;
	}
	return generic_info_create_define_source_code(info,self->collectNewObject);
}

/**
 * ???????????? 0?????????????????? 1??????????????????????????????
 * ???impl$ Abc{
 * }???
 * ???????????????????????????????????????
 */
char   **block_mgr_get_undefine_newobj_source_codes(BlockMgr *self,ClassName *className)
{
	GenericInfo *info=getInfo(self,className);
	if(info==NULL){
		n_debug("?????????GenericInfo :%s",className->sysName);
		return NULL;
	}
    return generic_info_create_undefine_source_code(info);
}

void block_mgr_add_undefine_new_object(BlockMgr *self,ClassName *className,GenericModel *defines)
{
    c_parser *parser=self->parser;
    char *belongClassName="unknown";
    if(!parser->isAet){
    	n_error("block_mgr_add_undefine_new_object ??????aet??????");
    	return;
    }
	ClassImpl  *impl=class_impl_get();
	belongClassName=impl->className->sysName;
	GenericInfo *info=getInfoAndCreate(self,impl->className);
	generic_info_add_undefine_obj(info,className->sysName,defines);
}

/**
 * ????????????new???????????????????????????
 * ???:
 * com_ai_LU$WWtx$com_ai_IMatrix$WWtx$float0$WWtx$
 * com_ai_LU???com_ai_IMatrix???com_ai_LU???????????????
 * unknown$WWtx$com_ai_IMatrix$WWtx$int0$WWtx$
 * unknown?????????????????????????????????????????????
 */
NPtrArray *block_mgr_read_define_new_object(BlockMgr *self,char *belongSysName)
{
   NPtrArray *array=generic_file_get_define_new_object(generic_file_get(),belongSysName);
   if(array==NULL){
	   array=class_lib_get_define_obj(class_lib_get(),belongSysName);
   }
   return array;
}

/**
 * ??????new$??????????????????????????????
 */
static void  saveDefineNewObject(BlockMgr *self)
{
	n_info("???????????????????????????new$????????????(???????????????) ??????????????????:%d %s",self->collectNewObject->len,in_fnames[0]);
    if(self->collectNewObject->len==0){
        generic_file_save_define_new$(generic_file_get(),NULL);
    	return;
    }
	int i;
    NString *re=n_string_new("");
    for(i=0;i<self->collectNewObject->len;i++){
    	GenericNewClass *item=(GenericNewClass *)n_ptr_array_index(self->collectNewObject,i);
    	char *id=generic_new_class_get_id(item);
    	n_string_append(re,id);
    }
    generic_file_save_define_new$(generic_file_get(),re->str);
    n_string_free(re,TRUE);
}

/**
 * ????????????????????????????????????
 */
static void  saveOutFuncGenericCall(BlockMgr *self)
{
	n_info("????????????????????????????????????????????? ????????????????????????:%d",self->defineFuncCallArray->len);
    if(self->defineFuncCallArray->len==0){
    	generic_file_save_out_func_call(generic_file_get(),NULL);
    	return;
    }
	int i;
    NString *re=n_string_new("");
    for(i=0;i<self->defineFuncCallArray->len;i++){
    	GenericCallFunc *item=(GenericCallFunc *)n_ptr_array_index(self->defineFuncCallArray,i);
    	n_string_append(re,item->id);
    }
    generic_file_save_out_func_call(generic_file_get(),re->str);
    n_string_free(re,TRUE);
}


/**
 * ????????????????????????????????????????????????????????????????????????
 */
void  block_mgr_save_define_newobject_and_func_call(BlockMgr *self)
{
	n_debug("block_mgr_save_define_newobject_and_func_call--- %s",in_fnames[0]);
	saveDefineNewObject(self);
	saveOutFuncGenericCall(self);
}


/**
 * ???????????????impl????????????????????????????????????????????????????????????
 */
int    block_mgr_get_undefine_newobj_count(BlockMgr *self,ClassName *className)
{
	int i;
	for(i=0;i<self->infoCount;i++){
       GenericInfo *info=self->genericInfos[i];
       if(generic_info_same(info,className)){
    	   return generic_info_get_undefine_obj_count(info);
       }
	}
	return 0;
}


/**
 * ?????????NPtrArray??????GenericNewClass
 */
NPtrArray     *block_mgr_read_undefine_new_object(BlockMgr *self,char *belongSysName)
{
	   NPtrArray *array=generic_file_get_undefine_new_object(generic_file_get(),belongSysName);
	   if(array==NULL){
		   array=class_lib_get_undefine_obj(class_lib_get(),belongSysName);
	   }
       return array;
}



/**
 * ????????????????????????
 */
char  *block_mgr_read_file_class_located(BlockMgr *self,char *sysName)
{
	char  *ret=generic_file_get_file_class_located(generic_file_get(),sysName);
	if(ret==NULL){
		ret=class_lib_get_file_class_located(class_lib_get(),sysName);
	}
	return ret;
}

/**
 * ????????????????????????????????????????????????
 */
void block_mgr_add_undefine_func_call(BlockMgr *self,ClassName *className,ClassFunc *func,GenericModel *defines)
{
    c_parser *parser=self->parser;
    char *belongClassName="unknown";
    if(!parser->isAet){
    	n_error("block_mgr_add_undefine_func_call ??????aet??????");
    	return;
    }
	ClassImpl  *impl=class_impl_get();
	belongClassName=impl->className->sysName;
	GenericInfo *info=getInfoAndCreate(self,impl->className);
	generic_info_add_undefine_func_call(info,className,func->mangleFunName,defines);
}

int block_mgr_get_undefine_func_call_count(BlockMgr *self,ClassName *className)
{
	GenericInfo *info=getInfo(self,className);
	if(info==NULL){
		n_debug("?????????GenericInfo :%s",className->sysName);
		return 0;
	}
	return generic_info_get_undefine_func_call_count(info);
}

char  **block_mgr_get_undefine_func_call_source_codes(BlockMgr *self,ClassName *className)
{
	GenericInfo *info=getInfo(self,className);
	if(info==NULL){
		n_debug("?????????GenericInfo :%s",className->sysName);
		return NULL;
	}
    return generic_info_create_undefine_func_call_source_code(info);
}

NPtrArray     *block_mgr_read_undefine_func_call(BlockMgr *self,char *sysName)
{
	   NPtrArray *array=generic_file_get_undefine_func_call(generic_file_get(),sysName);
	   if(array==NULL){
		   array=class_lib_get_undefine_func_call(class_lib_get(),sysName);
	   }
	   return array;
}




void  block_mgr_add_define_func_call(BlockMgr *self,ClassName *className,ClassFunc *classFunc,GenericModel *defines)
{
	   c_parser *parser=self->parser;
	    char *belongClass=GENERIC_UNKNOWN_STR;
	    if(parser->isAet){
	     	ClassImpl  *impl=class_impl_get();
	    	GenericInfo *info=getInfoAndCreate(self,impl->className);
	    	generic_info_add_define_func_call(info,className,classFunc->mangleFunName,defines);
	    	return;
	    }
	    if(defines==NULL){
	    	n_info("??????block_mgr_add_define_new_object??? GenericModel *define????????????sysName:%s",className->sysName);
	    	return;
	    }
	    GenericCallFunc *func=generic_call_func_new(className,classFunc->mangleFunName,defines);
		int i;
		for(i=0;i<self->defineFuncCallArray->len;i++){
			GenericCallFunc *item=n_ptr_array_index(self->defineFuncCallArray,i);
			if(!strcmp(item->sysName,func->sysName) && !strcmp(item->funcName,func->funcName) && generic_model_equal(defines,item->model)){
				generic_call_func_free(func);
				return;
			}
		}
		n_ptr_array_add(self->defineFuncCallArray,func);
}

int   block_mgr_get_define_func_call_count(BlockMgr *self,ClassName *className)
{
	GenericInfo *info=getInfo(self,className);
	if(info==NULL){
		n_debug("?????????GenericInfo :%s",className->sysName);
		return 0;
	}
	return generic_info_get_define_func_call_count(info)+self->defineFuncCallArray->len;
}

char   **block_mgr_get_define_func_call_source_codes(BlockMgr *self,ClassName *className)
{
	GenericInfo *info=getInfo(self,className);
	if(info==NULL){
		n_debug("?????????GenericInfo :%s",className->sysName);
		return NULL;
	}
	return generic_info_create_define_func_call_source_code(info,self->defineFuncCallArray);
}

NPtrArray *block_mgr_read_define_func_call(BlockMgr *self,char *belongSysName)
{
   NPtrArray *array=generic_file_get_define_func_call(generic_file_get(),belongSysName);
   if(array==NULL){
	   array=class_lib_get_define_func_call(class_lib_get(),belongSysName);
   }
   return array;
}


void block_mgr_set_parser(BlockMgr *self,c_parser *parser)
{
   self->parser=parser;
}

BlockMgr *block_mgr_get()
{
	static BlockMgr *singleton = NULL;
	if (!singleton){
		 singleton =n_slice_alloc0 (sizeof(BlockMgr));
		 blockMgrInit(singleton);
	}
	return singleton;
}


