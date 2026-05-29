/*
 * Copyright (C) 2022 , guiyang,wangyong co.,ltd.

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

#ifndef __GCC_AET_PARSER_H__
#define __GCC_AET_PARSER_H__

#include "nlib.h"


typedef struct _AetParser AetParser;
/* --- structures --- */
struct _AetParser
{
   c_parser *parser;
   bool isAet;
   bool isGenericState;
   bool isTestGenericBlockState;
   NString *includeCodes;
};

#define AET_PARSER_CONSUME_TOKEN \
      int i;\
        int avail = parser->tokens_avail-1;\
        for(i=3;i<avail;i++)\
           parser->tokens[i] = parser->tokens[i+1];

AetParser *aet_parser_get();
void aet_parser_set_parser(AetParser *self,c_parser *parser);

void aet_parser_c_parser_translation_unit (AetParser *self);
//解析 impl$ 时就进入了 aet状态
void aet_parser_set_enter(AetParser *self,bool enter);
nboolean aet_parser_is_aet(AetParser *self);
//进入了 generic 状态
void aet_parser_set_generic_state(AetParser *self,bool enter);
bool aet_parser_is_generic_state(AetParser *self);
void aet_parser_set_test_generic_block_state(AetParser *self,bool enter);
bool aet_parser_is_test_generic_block_state(AetParser *self);

//原型 disable_extension_diagnostics c-parser.cc
int aet_parser_disable_extension_diagnostics (void);
//原型 c_parser_expr_no_commas
struct c_expr aet_parser_c_parser_expr_no_commas (AetParser *self, struct c_expr *after,tree omp_atomic_lhs = NULL_TREE);
//原型 c_parser_declaration_or_fndef c-parser.cc
void aet_parser_c_parser_declaration_or_fndef (AetParser *self, bool, bool, bool,
                       bool, bool, bool,tree *, vec<c_token> *,
                       bool have_attrs = false,
                       tree attrs = NULL,
                       struct oacc_routine_data * = NULL,
                       bool * = NULL);

//原型 c_parser_initializer  c-parser.cc
struct c_expr aet_parser_c_parser_initializer (AetParser *self,tree decl);
//原型 restore_extension_diagnostics c-parser.cc
void aet_parser_restore_extension_diagnostics (AetParser *self,int flags);
//原型 c_parser_compound_statement c-parser.cc
tree aet_parser_c_parser_compound_statement (AetParser *self, location_t *endlocp);
//原型 c_parser_asm_definition c-parser.cc
void aet_parser_c_parser_asm_definition (AetParser *self);
//原型 c_parser_pragma c-parser.cc
bool aet_parser_c_parser_pragma (AetParser *self, int pc, bool *if_p,tree before_labels);
//原型 c_parser_skip_to_end_of_block_or_statement c-parser.cc
void  aet_parser_c_parser_skip_to_end_of_block_or_statement (AetParser *self);
//原型 c_parser_static_assert_declaration_no_semi c-parser.cc
void aet_parser_c_parser_static_assert_declaration_no_semi (AetParser *self);
//原型 c_parser_nth_token_starts_std_attributes c-parser.cc
bool aet_parser_c_parser_nth_token_starts_std_attributes (AetParser *self,unsigned int n);
//原型 c_parser_set_source_position_from_token c-parser.cc
void aet_parser_c_parser_set_source_position_from_token (c_token *token);
//原型 c_parser_gnu_attributes c-parser.cc
tree aet_parser_c_parser_gnu_attributes (AetParser *self);
//原型 c_parser_std_attribute_specifier_sequence c-parser.cc
tree aet_parser_c_parser_std_attribute_specifier_sequence (AetParser *self);

nboolean aet_parser_set_class_or_enum_type(AetParser *self,c_token *who);
//原型 c_parser_declarator c-parser.h c-parsr.cc
struct c_declarator * aet_parser_c_parser_declarator (AetParser *self, bool type_seen_p, c_dtr_syn kind,
           bool *seen_id);
//原型 c_parser_declspecs c-parser.h c-parser.cc
void aet_parser_c_parser_declspecs (AetParser *self, struct c_declspecs *specs,
          bool scspec_ok, bool typespec_ok, bool start_attr_ok,
          bool alignspec_ok, bool auto_type_ok,
          bool start_std_attr_ok, bool end_std_attr_ok,
          enum c_lookahead_kind la);

//原型 c_parser_expr_list c-parser.cc
vec<tree, va_gc> *aet_parser_c_parser_expr_list (AetParser *aetParser, bool convert_p, bool fold_p,
          vec<tree, va_gc> **p_orig_types,
          location_t *sizeof_arg_loc, tree *sizeof_arg,
          vec<location_t> *locations,
          unsigned int *literal_zero_mask);

void aet_parser_c_parser_declspecs_generic (AetParser *self,struct c_declspecs *specs);


#endif


