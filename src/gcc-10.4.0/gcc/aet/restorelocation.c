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
#include "c/c-parser.h"

#include "aetinfo.h"
#include "c-aet.h"
#include "aetutils.h"
#include "aetprinttree.h"
#include "classmgr.h"

/**
 * 在编译时加入了一些原代码还有新增的token。
 * 当这些代码或token编译完后，要恢复到文件原来的位置。
 */

/**
 * 加入额外的代码，并编译，同时把当前的buffer的状态及数据保存，
 * 当编完额外的代码，恢复。恢复是通过在文件directives.c 中的 _cpp_pop_buffer 启动的
 */
/**
 * 加入新代码，并编译。
 * 编完后，要恢复原来的input_location
 * 加入新代码，与下列文件有关
 * libcpp/directives.c 中的 _cpp_pop_buffer  cpp_push_buffer
 * c-family/c-lex.c        fe_file_change
 * c-family/c-opts.c       cb_file_change
 * libcpp/lex.c           _cpp_get_fresh_line  lex_raw_string 跟踪文件内容时否到尾了。
 */
typedef struct _RestoreInputLocaton{
    location_t loc;
    int   order;
    unsigned char *line_base;
    location_t highest_location;
    location_t highest_line;
    unsigned char *cur;
    unsigned char *next_line;  /* Start of to-be-cleaned logical line.  */



           unsigned int max_column_hint;
          nboolean seen_line_directive;
          location_t builtin_location;
          /* The default value of range_bits in ordinary line maps.  */
          unsigned int default_range_bits;
          unsigned int num_optimized_ranges;
          unsigned int num_unoptimized_ranges;

          int reason;
          unsigned char sysp;
          unsigned int m_column_and_range_bits : 8;
          unsigned int m_range_bits : 8;
          linenum_type to_line;
          location_t included_from;
}RestoreInputLocaton;

static int addCodesCount=0;
static NPtrArray *depthArray=n_ptr_array_new();


static void restore(cpp_reader *pfile,RestoreInputLocaton *item)
{

      // printf("RestoreInputLocaton --- item %u order:%d %u line_base:%p %p cur:%p\n",
            //   item->loc,item->order,input_location,item->line_base,pfile->buffer->line_base,pfile->buffer->cur);
           if(!pfile->buffer){
               printf("pfile->buffer==NULL，编译完文件了。返回。\n");
               return;
           }

//input_location=item->loc;
//pfile->buffer->line_base=item->line_base;
//pfile->line_table->highest_location=item->highest_location;


           pfile->line_table->highest_line=item->highest_line;
           pfile->buffer->cur=item->cur;

           //pfile->buffer->need_line=1;


           pfile->buffer->next_line=item->next_line;



//           pfile->line_table->max_column_hint= item->max_column_hint;
//           pfile->line_table->seen_line_directive=item->seen_line_directive;
//           pfile->line_table->builtin_location=item->builtin_location;
//            /* The default value of range_bits in ordinary line maps.  */
//           pfile->line_table->default_range_bits =item->default_range_bits;
//           pfile->line_table->num_optimized_ranges=item->num_optimized_ranges;
//           pfile->line_table->num_unoptimized_ranges=item->num_unoptimized_ranges;

           line_maps *set=pfile->line_table;
           line_map_ordinary *map = LINEMAPS_LAST_ORDINARY_MAP (set);


//         map->reason=item->reason;
//         map->sysp=item->sysp;


           map->m_column_and_range_bits=item->m_column_and_range_bits;
           map->m_range_bits=item->m_range_bits;


//         map->to_line=item->to_line;
//         map->included_from=item->included_from;


}

static char *popBuffer_cb(const char *header,cpp_dir *dir)
{
    RestoreInputLocaton *restoreData=(RestoreInputLocaton *)dir->canonical_name;
    n_free(dir->name);
    dir->name=NULL;
    dir->len=0;
    dir->canonical_name=NULL;
    restore(parse_in,restoreData);
    cpp_stop_forcing_token_locations (parse_in);
    n_ptr_array_remove(depthArray,restoreData);
    n_slice_free(RestoreInputLocaton,restoreData);
    return NULL;
}

void aet_utils_write_cpp_buffer(cpp_buffer *newBuffer,void *restoreData)
{
    if(newBuffer->dir.name){
        n_error("newBuffer->dir->name不可能有内容:%s\n",newBuffer->dir.name);
    }
    if(newBuffer->dir.canonical_name){
        n_error("newBuffer->dir->canonical_name不可能有内容:%s\n",newBuffer->dir.canonical_name);
    }
    newBuffer->dir.name=n_strdup("aet");
    newBuffer->dir.len=3;
    newBuffer->dir.construct=popBuffer_cb;
    newBuffer->dir.canonical_name=(char*)restoreData;
   // printf("createBufferChange ---- %p %p\n",newBuffer->dir.name,newBuffer->dir.canonical_name);
}

void *aet_utils_create_restore_location_data(cpp_reader *pfile,location_t loc)
{
    RestoreInputLocaton *restore=(RestoreInputLocaton *)n_slice_new0(RestoreInputLocaton);
    restore->loc=loc;
    restore->order=addCodesCount++;
    restore->line_base=pfile->buffer->line_base;
    restore->highest_location=pfile->line_table->highest_location;
    restore->highest_line=pfile->line_table->highest_line;
    restore->cur=pfile->buffer->cur;
    restore->next_line=pfile->buffer->next_line;

    restore->max_column_hint=pfile->line_table->max_column_hint;
    restore->builtin_location=pfile->line_table->builtin_location;
    restore->seen_line_directive=pfile->line_table->seen_line_directive;
    /* The default value of range_bits in ordinary line maps.  */
    restore->default_range_bits=pfile->line_table->default_range_bits;
    restore->num_optimized_ranges=pfile->line_table->num_optimized_ranges;
    restore->num_unoptimized_ranges=pfile->line_table->num_unoptimized_ranges;

    line_maps *set=pfile->line_table;
    line_map_ordinary *map = LINEMAPS_LAST_ORDINARY_MAP (set);

    restore->reason=map->reason;
    restore->sysp=map->sysp;
    restore->m_column_and_range_bits=map->m_column_and_range_bits;
    restore->m_range_bits=map->m_range_bits;
    restore->to_line=map->to_line;
    restore->included_from=map->included_from;
    n_ptr_array_add(depthArray,restore);
   // printf("aet_utils_create_restore_location_data restoreData:%p\n",restore);
    return (void*)restore;
}


/* Return TRUE if we reached the end of the set of tokens stored in
   CONTEXT, FALSE otherwise.  */
static inline nboolean reached_end_of_context (cpp_context *context)
{
  if (context->tokens_kind == TOKENS_KIND_DIRECT)
      return FIRST (context).token == LAST (context).token;
  else if (context->tokens_kind == TOKENS_KIND_INDIRECT || context->tokens_kind == TOKENS_KIND_EXTENDED)
    return FIRST (context).ptoken == LAST (context).ptoken;
  else
    abort ();
}

/**
 * 如果不是在编译附加代码状态中，返回loc
 * 否则返回currentResotre中的loc变量
 */
location_t aet_utils_get_location(location_t loc)
{
    if(depthArray->len==0)
        return 0;
    RestoreInputLocaton *currenRestore=n_ptr_array_index(depthArray,depthArray->len-1);
    if(currenRestore!=NULL)
        return currenRestore->loc;
    return loc;
}

/**
 * 当前的token是处理宏状态中。
 */
int aet_utils_in_micro()
{
      cpp_reader *pfile=parse_in;
      cpp_context *context = pfile->context;
      int in_directive=pfile->state.in_directive;
      nboolean  end=reached_end_of_context(context);
      int state=FROM_SOURCE;
      if (!context->prev){
          state=FROM_SOURCE;
      }else if(!end){
          state=FROM_MACRO;
      }else{
          if (pfile->context->c.macro)
              state=FROM_MACRO;
          state=FROM_OTHER;
      }
      n_debug("restorelocaton.c aet_utils_in_micro tokenKind:%d context->prev:%p end:%d in_directive:%d pfile->context->c.macro:%p",
              context->tokens_kind,context->prev,end,in_directive,pfile->context->c.macro);
      return state;
}

/**
 * 如果在此状态，访问控制被忽略。
 */
nboolean aet_utils_compile_additional_code_status()
{
    return depthArray->len>0;
}

