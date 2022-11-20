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

#ifndef GCC_AET_UTILS_H
#define GCC_AET_UTILS_H

#include "nlib.h"
#include "c-aet.h"

tree           aet_utils_create_identifier(const uchar *str,size_t len);
tree           aet_utils_create_ident(uchar *str);
c_token       *aet_utils_create_typedef_token(c_token *token,location_t start_loc);
c_token       *aet_utils_create_struct_token(c_token *token,location_t start_loc);
c_token       *aet_utils_create_return_token(c_token *token,location_t start_loc);
c_token       *aet_utils_create_if_token(c_token *token,location_t start_loc);
c_token       *aet_utils_create_sizeof_token(c_token *token,location_t start_loc);
c_token       *aet_utils_create_void_token(c_token *token,location_t start_loc);
c_token       *aet_utils_create_static_token(c_token *token,location_t start_loc);
c_token       *aet_utils_create_char_token(c_token *token,location_t start_loc);
c_token       *aet_utils_create_int_token(c_token *token,location_t start_loc);
void           aet_utils_create_number_token(c_token *src,int value);
void           aet_utils_create_private$_token(c_token *token,location_t start_loc);

void           aet_utils_copy_token(c_token *src,c_token *dest);
void           aet_utils_create_token(c_token *token,enum cpp_ttype type,uchar *str,int len);
tree           aet_utils_create_temp_func_name(char *className,char *orgiName);
int            aet_utils_get_orgi_func_and_class_name(char *newName,char *className,char *funcName);
c_token       *aet_utils_create_super_token(c_token *token,location_t start_loc);
void           aet_utils_create_string_token(c_token *token,uchar *str,int len);
char*          aet_utils_create_init_method(char *className);
c_token       *aet_utils_create_aet_goto_token(c_token *token,location_t start_loc);


int            aet_utils_add_token(cpp_reader *pfile, const char *str,size_t len);
int            aet_utils_add_token_with_location(cpp_reader *pfile, const char *str,size_t len,location_t loc);
int            aet_utils_add_token_with_force(cpp_reader *pfile, const char *str,size_t len,location_t loc,nboolean force);

void          *aet_utils_create_restore_location_data(cpp_reader *pfile,location_t loc);
location_t     aet_utils_get_location(location_t loc);//classctor.c调用
void           aet_utils_write_cpp_buffer(cpp_buffer *newBuffer,void *restoreData);
unsigned int   aet_utils_create_hash(const uchar *str,size_t len);
void           aet_utils_create_in_class_iface_var(char *ifaceName,char *result);
void           aet_utils_create_super_field_name(char *className,char *mangle,char *result);
void           aet_utils_create_finalize_name(char *className,char *result);
nboolean       aet_utils_is_finalize_name(char *className,char *funcName);
void           aet_utils_create_unref_name(char *className,char *funcName);
nboolean       aet_utils_is_unref_name(char *className,char *funcName);

char          *aet_utils_convert_token_to_string(c_token *token);

static inline nboolean   aet_utils_valid_tree(tree value)
{
	return (value!=NULL && value!=NULL_TREE && value!=error_mark_node && value!=0xffffffffffffffff);
}

static inline void  aet_utils_print_loc(location_t loc)
{
	if(loc==UNKNOWN_LOCATION)
		printf("---------------------location UNKNOWN_LOCATION----------------");
	else{
	    expanded_location xloc;
        xloc = expand_location(loc);
	    printf("----------------位置数据 %s %d %d\n",xloc.file, xloc.line, xloc.column);
	}
}


/**
  * 得到当前时间的毫秒数
  */
static inline  nuint64 aet_utils_current_time_millis()
 {
	struct timeval tve;
	gettimeofday(&tve,NULL);
	return tve.tv_sec*1000+tve.tv_usec/1000;
 }

/**
 * 创建枚举的类型名
 */
char *aet_utils_create_enum_type_name(char *sysName,char *userName);
char *aet_utils_create_enum_element_name(char *sysName,char *origElement);

typedef enum{
    FROM_SOURCE,
    FROM_MACRO,
    FROM_OTHER,
}TokenFrom;

//token来自micro吗?
int      aet_utils_in_micro();
//是不是在编译附加代码状态
nboolean aet_utils_compile_additional_code_status();

char   *aet_utils_get_keyword_string(c_token *token);//获取keyword类型的keyword的字符串


#endif /* ! GCC_C_AET_H */
