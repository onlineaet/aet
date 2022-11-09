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
#include "toplev.h"

#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "c-family/c-common.h"
#include "gimple-expr.h"

#include "c/c-tree.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "c/gimple-parser.h"
#include "../libcpp/include/cpplib.h"

#include "aetinfo.h"
#include "c-aet.h"
#include "c/c-parser.h"
#include "aetutils.h"
#include "aetprinttree.h"
#include "classmgr.h"



unsigned int aet_utils_create_hash(const uchar *str,size_t len)
{
   const uchar *cur;
   unsigned int hash = HT_HASHSTEP (0, *str);
   cur=str+1;
   while (ISIDNUM (*cur)){
        hash = HT_HASHSTEP (hash, *cur);
        cur++;
   }
   hash = HT_HASHFINISH (hash, len);
   return hash;
}

void aet_utils_create_in_class_iface_var(char *ifaceName,char *result)
{
	char temp[255];
    sprintf(temp,"iface%s",ifaceName);
	int hash=aet_utils_create_hash(temp,strlen(temp));
	sprintf(result,"%s%u",temp,hash);
}

void aet_utils_create_super_field_name(char *className,char *mangle,char *result)
{
	sprintf(result,"super_%s_%s",className,mangle);
}

void aet_utils_create_finalize_name(char *className,char *result)
{
	char temp[255];
    sprintf(temp,"~%s",className);
	int hash=aet_utils_create_hash(temp,strlen(temp));
	sprintf(result,"%s_finalize_%u",className,hash);
}

nboolean  aet_utils_is_finalize_name(char *className,char *funcName)
{
	char temp[255];
	aet_utils_create_finalize_name(className,temp);
	return strcmp(temp,funcName)==0;
}

void  aet_utils_create_unref_name(char *className,char *funcName)
{
	char temp[255];
    sprintf(temp,"unref%s",className);
	int hash=aet_utils_create_hash(temp,strlen(temp));
	sprintf(funcName,"%s_unref_%u",className,hash);
}

nboolean  aet_utils_is_unref_name(char *className,char *funcName)
{
	char temp[255];
	aet_utils_create_unref_name(className,temp);
	return strcmp(temp,funcName)==0;
}

static unsigned int calc_hash (const unsigned char *str, size_t len)
{
  size_t n = len;
  unsigned int r = 0;
  while (n--)
    r = HT_HASHSTEP (r, *str++);
  return HT_HASHFINISH (r, len);
}

/*
 * lex.c中每个字符算一个hash 必须与lex.c保持一致。
 * aet_utils_create_identifier中创建hash的方法与symtab.c中的不一样。
 * "_aet_magic$_123"字符串用calc_hash创建与lex.c方法创建的hash不一样。所以生成的tree不一样。
 * 在lookup_field中就找不到对应的field。也一能用
 * get_identifier_with_length(str,strlen(str));
 * get_identifier (str);
 * 他们所用的hashtable与parser_in中的也不一样。
 * 把symtab.c中的calc_hash移到aet_utils_create_identifier解决问题。在lex.c中创建hash的地方有三处。
  unsigned int hash = HT_HASHSTEP (0, *base);
  cur = base + 1;
  while (ISIDNUM (*cur))
    {
      hash = HT_HASHSTEP (hash, *cur);
      cur++;
    }
  len = cur - base;
  hash = HT_HASHFINISH (hash, len);

    unsigned int hash = HT_HASHSTEP (0, *cur);
  ++cur;
  while (ISIDNUM (*cur))
    {
      hash = HT_HASHSTEP (hash, *cur);
      ++cur;
    }
  hash = HT_HASHFINISH (hash, cur - base);
 */
tree aet_utils_create_identifier(const uchar *str,size_t len)
{
   cpp_hashnode *result;
   unsigned int hash= calc_hash(str,len);
   result = CPP_HASHNODE (ht_lookup_with_hash (parse_in->hash_table,
						  (const unsigned char *)str, len, hash, HT_ALLOC));
   return HT_IDENT_TO_GCC_IDENT (HT_NODE (result));
}

c_token      *aet_utils_create_typedef_token(c_token *token,location_t start_loc)
{
	  token->type = CPP_KEYWORD;
	  token->id_kind=C_ID_NONE;
      token->keyword = RID_TYPEDEF;
      token->pragma_kind = PRAGMA_NONE;
      token->value=aet_utils_create_identifier("typedef",7);
      C_SET_RID_CODE(token->value,RID_TYPEDEF);
      TREE_LANG_FLAG_0 (token->value) = 1;
 	  token->location = start_loc;
      return token;
}

c_token      *aet_utils_create_struct_token(c_token *token,location_t start_loc)
{
	  token->type = CPP_KEYWORD;
	  token->id_kind=C_ID_NONE;
      token->keyword = RID_STRUCT;
      token->pragma_kind = PRAGMA_NONE;
      token->value=aet_utils_create_identifier("struct",6);
      C_SET_RID_CODE(token->value,RID_STRUCT);
      TREE_LANG_FLAG_0 (token->value) = 1;
 	  token->location = start_loc;
      return token;
}

c_token      *aet_utils_create_return_token(c_token *token,location_t start_loc)
{
	  token->type = CPP_KEYWORD;
	  token->id_kind=C_ID_NONE;
      token->keyword = RID_RETURN;
      token->pragma_kind = PRAGMA_NONE;
      token->value =aet_utils_create_identifier("return",6);
      C_SET_RID_CODE(token->value,RID_RETURN);
      TREE_LANG_FLAG_0 (token->value) = 1;
 	  token->location = start_loc;
      return token;
}


c_token      *aet_utils_create_if_token(c_token *token,location_t start_loc)
{
      token->type = CPP_KEYWORD;
      token->id_kind=C_ID_NONE;
      token->keyword = RID_IF;
      token->pragma_kind = PRAGMA_NONE;
      token->value = aet_utils_create_identifier("if",2);
      C_SET_RID_CODE(token->value,RID_IF);
      TREE_LANG_FLAG_0 (token->value) = 1;
      token->location = start_loc;
      return token;
}

c_token      *aet_utils_create_sizeof_token(c_token *token,location_t start_loc)
{
	  token->type = CPP_KEYWORD;
	  token->id_kind=C_ID_NONE;
      token->keyword = RID_SIZEOF;
      token->pragma_kind = PRAGMA_NONE;
      token->value = aet_utils_create_identifier("sizeof",6);
      C_SET_RID_CODE(token->value,RID_SIZEOF);
      TREE_LANG_FLAG_0 (token->value) = 1;
 	  token->location = start_loc;
      return token;
}

c_token      *aet_utils_create_static_token(c_token *token,location_t start_loc)
{
	  token->type = CPP_KEYWORD;
	  token->id_kind=C_ID_NONE;
      token->keyword = RID_STATIC;
      token->pragma_kind = PRAGMA_NONE;
      token->value = aet_utils_create_identifier("static",6);
      C_SET_RID_CODE(token->value,RID_STATIC);
      TREE_LANG_FLAG_0 (token->value) = 1;
 	  token->location = start_loc;
      return token;
}

c_token      *aet_utils_create_char_token(c_token *token,location_t start_loc)
{
	  token->type = CPP_KEYWORD;
	  token->id_kind=C_ID_NONE;
      token->keyword = RID_CHAR;
      token->pragma_kind = PRAGMA_NONE;
      token->value=aet_utils_create_identifier("char",4);
      C_SET_RID_CODE(token->value,RID_CHAR);
      TREE_LANG_FLAG_0 (token->value) = 1;
 	  token->location = start_loc;
      return token;
}

c_token      *aet_utils_create_int_token(c_token *token,location_t start_loc)
{
	  token->type = CPP_KEYWORD;
	  token->id_kind=C_ID_NONE;
      token->keyword = RID_INT;
      token->pragma_kind = PRAGMA_NONE;
      token->value=aet_utils_create_identifier("int",3);
      C_SET_RID_CODE(token->value,RID_INT);
      TREE_LANG_FLAG_0 (token->value) = 1;
 	  token->location = start_loc;
      return token;
}

/**
 * 创建private$
 */
void  aet_utils_create_private$_token(c_token *token,location_t start_loc)
{
     token->type = CPP_KEYWORD;
     token->id_kind=C_ID_NONE;
     token->keyword = RID_AET_PRIVATE;
     token->pragma_kind = PRAGMA_NONE;
     token->value=aet_utils_create_identifier("private$",8);
     C_SET_RID_CODE(token->value,RID_AET_PRIVATE);
     TREE_LANG_FLAG_0 (token->value) = 1;
     token->location = start_loc;
     return token;
}

c_token       *aet_utils_create_super_token(c_token *token,location_t start_loc)
{
	  token->type = CPP_KEYWORD;
	  token->id_kind=C_ID_NONE;
      token->keyword = RID_AET_SUPER;
      token->pragma_kind = PRAGMA_NONE;
      token->value=aet_utils_create_identifier("super$",6);
      C_SET_RID_CODE(token->value,RID_AET_SUPER);
      TREE_LANG_FLAG_0 (token->value) = 1;
	  token->location = start_loc;
      return token;
}

c_token       *aet_utils_create_aet_goto_token(c_token *token,location_t start_loc)
{
	  token->type = CPP_KEYWORD;
	  token->id_kind=C_ID_NONE;
      token->keyword = RID_AET_GOTO;
      token->pragma_kind = PRAGMA_NONE;
      token->value=aet_utils_create_identifier(RID_AET_GOTO_STR,strlen(RID_AET_GOTO_STR));
      C_SET_RID_CODE(token->value,RID_AET_GOTO);
      TREE_LANG_FLAG_0 (token->value) = 1;
	  token->location = start_loc;
      return token;
}

c_token       *aet_utils_create_void_token(c_token *token,location_t start_loc)
{
	  token->type = CPP_KEYWORD;
	  token->id_kind=C_ID_NONE;
	  token->keyword = RID_VOID;
	  token->pragma_kind = PRAGMA_NONE;
	  token->value=aet_utils_create_identifier("void",4);
	  C_SET_RID_CODE(token->value,RID_VOID);
	  TREE_LANG_FLAG_0 (token->value) = 1;
	  token->location = start_loc;
	  return token;
}



void aet_utils_copy_token(c_token *src,c_token *dest)
{
	 tree value=src->value;
	 memcpy(dest,src,sizeof(c_token));
	 dest->location=src->location;
	 dest->value=value;
}

void aet_utils_create_token(c_token *token,enum cpp_ttype type,uchar *str,int len)
{
   token->type = type;
   if(type==CPP_NAME){
      token->value =aet_utils_create_identifier(str,len);
	  token->id_kind = C_ID_ID;
   }else{
	  token->id_kind=C_ID_NONE;
   }
   token->keyword = RID_MAX;
   token->pragma_kind = PRAGMA_NONE;
}

void  aet_utils_create_number_token(c_token *token,int value)
{
	   token->type = CPP_NUMBER;
	   token->id_kind=C_ID_NONE;
	   token->keyword = RID_MAX;
	   token->pragma_kind = PRAGMA_NONE;
	   unsigned HOST_WIDE_INT w;
	   w=value;
	   token->value=build_int_cst (integer_type_node, w);
}

tree aet_utils_create_ident(uchar *str)
{
   return aet_utils_create_identifier(str,strlen(str));
}


static int tempFuncCount=0;
/**
 * 创建临时的函数名
 * 如:_A12test_ARandom11pointerHash4
 */
tree aet_utils_create_temp_func_name(char *className,char *orgiName)
{
	NString *newName=n_string_new("_A");
	n_string_append_printf(newName,"%d",strlen(className));
	n_string_append(newName,className);
	n_string_append_printf(newName,"%d",strlen(orgiName));
	n_string_append(newName,orgiName);
	tempFuncCount++;
	n_string_append_printf(newName,"%d",tempFuncCount);
	tree tree=aet_utils_create_ident(newName->str);
	n_string_free(newName,TRUE);
	return tree;
}

/**
 * 从aet_utils_create_temp_func_name创建的临时函数名中取出类名和函数名
 * srcFuncName 临时函数名。
 * className:从srcFuncName取出的类名存入className
 * funcName:从srcFuncName取出的类名存入funcName
 * 如:_A12test_ARandom11pointerHash4
 * className=test_ARandom;
 * funcName=pointerHash;
 */
int aet_utils_get_orgi_func_and_class_name(char *srcFuncName,char *className,char *funcName)
{
	if(srcFuncName[0]!='_')
		return 0;
	if(srcFuncName[1]!='A')
		return 0;
    if(!n_ascii_isdigit(srcFuncName[2]))
    	return 0;
    char classNameLen[strlen(srcFuncName)];
    int i=2;
    int count=0;
    while(n_ascii_isdigit(srcFuncName[i])){
    	classNameLen[count++]=srcFuncName[i];
        i++;
    }
    classNameLen[count]='\0';
    int len=atoi((const char*)classNameLen);
    memcpy(className,srcFuncName+i,len);
    className[len]='\0';
    i+=len;
    count=0;
    while(n_ascii_isdigit(srcFuncName[i])){
    	classNameLen[count++]=srcFuncName[i];
        i++;
    }
    classNameLen[count]='\0';
    len=atoi((const char*)classNameLen);
    memcpy(funcName,srcFuncName+i,len);
    funcName[len]='\0';
    return len;
}


void aet_utils_create_string_token(c_token *token,uchar *str,int len)
{
     tree value = build_string (len, (const char *) str);
     token->type = CPP_STRING;
     token->value = value;
}

/**
 * 在class声明最后加入一个初始化方法
 * Abc_init_$12345678@_Abc
 */
char* aet_utils_create_init_method(char *className)
{
   char buffer[256];
   sprintf(buffer,"%s_%s_%s",className,AET_INIT_GLOBAL_METHOD_STRING,className);
   char *re=n_strdup(buffer);
   return re;
}

/**
 * 加入额外的代码，并编译，同时把当前的buffer的状态及数据保存，
 * 当编完额外的代码，恢复。恢复是通过在文件directives.c 中的 _cpp_pop_buffer 启动的
 */
int aet_utils_add_token(cpp_reader *pfile, const char *str,size_t len)
{
  return aet_utils_add_token_with_location(pfile,str,len,input_location);
}

int aet_utils_add_token_with_location(cpp_reader *pfile, const char *str,size_t len,location_t loc)
{
   int tokenFrom= aet_utils_in_micro();
   if(tokenFrom==FROM_MACRO){
       n_error("准备编译新加的代码时，发现正在编译宏符号。退出!，源代码:%s\n",str);
       return 0;
   }
   char *nbuf;
   nbuf=n_strdup(str);
   len=strlen(nbuf);
   void *restoreData=aet_utils_create_restore_location_data(pfile,loc);
   cpp_buffer *newBuffer=cpp_push_buffer (pfile, (uchar *) nbuf, len, /* from_stage3 */ true);
   aet_utils_write_cpp_buffer(newBuffer,restoreData);
   _cpp_clean_line (pfile);
   pfile->cur_token = _cpp_temp_token (pfile);
   cpp_token *token = _cpp_lex_direct (pfile);
  //printf("加入新符号 00 ---%s %d %s, %s, %d\n", (char *)str,len,__FILE__, __FUNCTION__, __LINE__);
  if (pfile->context->tokens_kind == TOKENS_KIND_EXTENDED){
      /* We are tracking tokens resulting from macro expansion.
     Create a macro line map and generate a virtual location for
     the token resulting from the expansion of the built-in
     macro.  */
//      location_t *virt_locs = NULL;
//      _cpp_buff *token_buf = tokens_buff_new (pfile, 1, &virt_locs);
//      const line_map_macro * map =linemap_enter_macro (pfile->line_table, node, loc, 1);
//      tokens_buff_add_token (token_buf, virt_locs, token,
//               pfile->line_table->builtin_location,
//               pfile->line_table->builtin_location,
//              map, /*macro_token_index=*/0);
//      push_extended_tokens_context (pfile, node, token_buf, virt_locs,(const cpp_token **)token_buf->base,1);
     // printf("加入新符号 11 11---%s %d %s, %s, %d\n", (char *)str,len,__FILE__, __FUNCTION__, __LINE__);

  }else{
     // printf("加入新符号 22 33---%s %d %s, %s, %d\n", (char *)str,len,__FILE__, __FUNCTION__, __LINE__);
     _cpp_push_token_context (pfile, NULL, token, 1);
  }
  return 1;
}


static char *getIdent(c_token *token)
{
  return IDENTIFIER_POINTER(token->value);
}

static char *getLiteral(c_token *token)
{
	tree value=token->value;
	if(TREE_CODE(value)==STRING_CST){
		return TREE_STRING_POINTER(value);
	}else if(TREE_CODE(value)==INTEGER_CST){
		char buffer[100];
		tree type=TREE_TYPE(value);
   	    wide_int result=wi::to_wide(value);
        if(TYPE_UNSIGNED (type)){
            nuint64 re=result.to_shwi();
	       sprintf(buffer, "%llu", re);
        }else{
           nint64 re=result.to_shwi();
           sprintf(buffer, "%lld", re);
        }
        char *str=n_strdup(buffer);
        return str;
	}else if(TREE_CODE(value)==REAL_CST){
	   char buffer[32];
	   real_to_decimal (buffer, TREE_REAL_CST_PTR (value), sizeof (buffer), 0, true);
	   char *str=n_strdup(buffer);
	   return str;
	}else if(TREE_CODE(value)==FIXED_CST){
	   char buffer[32];
	   fixed_to_decimal (buffer, TREE_FIXED_CST_PTR (value), sizeof (buffer));
	   char *str=n_strdup(buffer);
	   return str;
	}
	return NULL;
}

static char *getKeyword(c_token *token)
{
	int i;
    for (i = 0; i < num_c_common_reswords; i++){
  	   const c_common_resword *resword = &c_common_reswords[i];
	   if (token->keyword==resword->rid){
		  return resword->word;
	   }
    }
    return NULL;
}
#define NONE ""

char *aet_utils_convert_token_to_string(c_token *token)
{
   int type=token->type;
   switch(type){
	  case CPP_EQ: return "=";
	  case CPP_NOT: return		"!";
	  case CPP_GREATER: return		">";
	  case CPP_LESS:  return		"<";
	  case CPP_PLUS:  return		"+";
	  case CPP_MINUS:  return		"-";
	  case CPP_MULT:  return		"*";
	  case CPP_DIV:  return		"/";
	  case CPP_MOD:  return		"%";
	  case CPP_AND:  return		"&";
	  case CPP_OR:  return		"|";
	  case CPP_XOR:  return		"^";
	  case CPP_RSHIFT:  return		">>";
	  case CPP_LSHIFT:  return		"<<";
	  case CPP_COMPL:  return		"~";
	  case CPP_AND_AND:  return		"&&";
	  case CPP_OR_OR:  return		"||";
	  case CPP_QUERY:  return		"?";
	  case CPP_COLON:  return		":";
	  case CPP_COMMA:  return		",";
	  case CPP_OPEN_PAREN:  return	"(";
	  case CPP_CLOSE_PAREN:  return	")";
	  case CPP_EOF:          return	NONE;
	  case CPP_EQ_EQ:  return		"==";
	  case CPP_NOT_EQ:  return		"!=";
	  case CPP_GREATER_EQ: return	">=";
	  case CPP_LESS_EQ:  return		"<=";
	  case CPP_SPACESHIP:  return		"<=>";
	  case CPP_PLUS_EQ:  return		"+=";
	  case CPP_MINUS_EQ:  return		"-=";
	  case CPP_MULT_EQ:  return		"*=";
	  case CPP_DIV_EQ:  return		"/=";
	  case CPP_MOD_EQ:  return		"%=";
	  case CPP_AND_EQ:  return		"&=";	/* bit ops */
	  case CPP_OR_EQ:  return		"|=";
	  case CPP_XOR_EQ:  return		"^=";
	  case CPP_RSHIFT_EQ:  return		">>=";
	  case CPP_LSHIFT_EQ:  return		"<<=";
	  case CPP_HASH:  return		"#";	/* digraphs */
	  case CPP_PASTE:  return		"##";
	  case CPP_OPEN_SQUARE:  return	"[";
	  case CPP_CLOSE_SQUARE:  return	"]";
	  case CPP_OPEN_BRACE:  return	"{";
	  case CPP_CLOSE_BRACE:  return	"}";
	  case CPP_SEMICOLON:  return		";";	/* structure */
	  case CPP_ELLIPSIS:  return		"...";
	  case CPP_PLUS_PLUS: return		"++";	/* increment */
	  case CPP_MINUS_MINUS:  return	"--";
	  case CPP_DEREF:  return		"->";	/* accessors */
	  case CPP_DOT:  return		".";
	  case CPP_SCOPE:  return		"::";
	  case CPP_DEREF_STAR:  return	"->*";
	  case CPP_DOT_STAR:  return		".*";
	  case CPP_ATSIGN:  return		"@";  /* used in Objective-C */
	  case CPP_NAME:  return	getIdent(token);	 /* word */
	  case CPP_AT_NAME:return	getIdent(token);	 /* @word - Objective-C */
	  case CPP_NUMBER:return		getLiteral(token); /* 34_be+ta  */

	  case CPP_CHAR:  return		getLiteral(token);  /* 'char' */
	  case CPP_WCHAR:  return		getLiteral(token); /* L'char' */
	  case CPP_CHAR16:  return		getLiteral(token);  /* u'char' */
	  case CPP_CHAR32:  return		getLiteral(token);  /* U'char' */
	  case CPP_UTF8CHAR:  return	getLiteral(token);  /* u8'char' */
	  case CPP_OTHER:  return		getLiteral(token);  /* stray punctuation */

	  case CPP_STRING:  return		getLiteral(token);  /* "string" */
	  case CPP_WSTRING:  return		getLiteral(token);  /* L"string" */
	  case CPP_STRING16:  return	getLiteral(token);  /* u"string" */
	  case CPP_STRING32 : return	getLiteral(token);  /* U"string" */
	  case CPP_UTF8STRING:  return	getLiteral(token);  /* u8"string" */
	  case CPP_OBJC_STRING:  return	getLiteral(token);  /* @"string" - Objective-C */
	  case CPP_HEADER_NAME:  return	getLiteral(token);  /* <stdio.h> in #include */

	  case CPP_CHAR_USERDEF:  return	getLiteral(token);  /* 'char'_suffix - C++-0x */
	  case CPP_WCHAR_USERDEF:  return	getLiteral(token);  /* L'char'_suffix - C++-0x */
	  case CPP_CHAR16_USERDEF:  return	getLiteral(token);  /* u'char'_suffix - C++-0x */
	  case CPP_CHAR32_USERDEF:  return	getLiteral(token);  /* U'char'_suffix - C++-0x */
	  case CPP_UTF8CHAR_USERDEF : return	getLiteral(token);  /* u8'char'_suffix - C++-0x */
	  case CPP_STRING_USERDEF:  return	getLiteral(token);  /* "string"_suffix - C++-0x */
	  case CPP_WSTRING_USERDEF:  return	getLiteral(token);  /* L"string"_suffix - C++-0x */
	  case CPP_STRING16_USERDEF:  return	getLiteral(token);  /* u"string"_suffix - C++-0x */
	  case CPP_STRING32_USERDEF:  return	getLiteral(token);  /* U"string"_suffix - C++-0x */
	  case CPP_UTF8STRING_USERDEF:  return getLiteral(token);  /* u8"string"_suffix - C++-0x */

	  case CPP_COMMENT:  return		getLiteral(token);  /* Only if output comments.  */
					 /* SPELL_LITERAL happens to DTRT.  */
	  case CPP_MACRO_ARG:  return		NONE;	 /* Macro argument.  */
	  case CPP_PRAGMA:  return		NONE;	 /* Only for deferred pragmas.  */
	  case CPP_PRAGMA_EOL:  return	NONE;	 /* End-of-line for deferred pragmas.  */
	  case CPP_PADDING:  return		NONE;	 /* Whitespace for -E.	*/
	  case CPP_KEYWORD:return getKeyword(token);
   }
   return NULL;
}

/**
 * 创建枚举的类型名
 */
char *aet_utils_create_enum_type_name(char *sysName,char *userName)
{
	char temp[255];
	int hash=aet_utils_create_hash(userName,strlen(userName));
	sprintf(temp,"_e0%d%s%d%s%u",strlen(sysName),sysName,strlen(userName),userName,hash);
	return n_strdup(temp);
}

/**
 * 创建枚举元素名
 */
char *aet_utils_create_enum_element_name(char *sysName,char *origElement)
{
	char temp[255];
	int hash=aet_utils_create_hash(origElement,strlen(origElement));
	sprintf(temp,"_e1%d%s%d%s%u",strlen(sysName),sysName,strlen(origElement),origElement,hash);
	return n_strdup(temp);
}

/**
 * 获取keyword类型的keyword的字符串
 */
char   *aet_utils_get_keyword_string(c_token *token)
{
    return getKeyword(token);
}

