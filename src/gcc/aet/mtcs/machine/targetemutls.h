/*
 * Copyright (C) 2026  zclei
 * This file is part of AET.

 * AET is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 3, or (at your option) any later
 * version.

 * AET is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.

 * You should have received a copy of the GNU General Public License
 * along with GCC Exception along with this program; see the file COPYING3.
 * If not see <http://www.gnu.org/licenses/>.
 * AET was originally developed  by the onlineaet@163.com
 */

#ifndef __GCC_TARGET_EMUTLS__
#define __GCC_TARGET_EMUTLS__

#include "../../nlib.h"
#include "machinetarget.h"

/**
  * emulated TLS"‌ 的缩写
  * 当目标平台不支持原生的线程局部存储（Thread Local Storage, TLS）特性时，
  * 模拟 TLS”（emulated TLS）的机制来实现 thread_local 变量。
  */
typedef struct _TargetEmutls TargetEmutls;
struct _TargetEmutls
{
    MachineTarget parent;
    //原型 targetm.emutls.get_address #define TARGET_EMUTLS_GET_ADDRESS "__builtin___emutls_get_address"
    char *get_address;
    //原型  targetm.emutls.register_common, #define TARGET_EMUTLS_REGISTER_COMMON "__builtin___emutls_register_common"
    char *register_common;
    //原型 targetm.emutls.debug_form_tls_address #define TARGET_EMUTLS_DEBUG_FORM_TLS_ADDRESS false
    bool debug_form_tls_address;
};

TargetEmutls *target_emutls_new(MtcsMode *mtcsMode);



#endif

