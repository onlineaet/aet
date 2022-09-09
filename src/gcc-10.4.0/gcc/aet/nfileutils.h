/*
   Copyright (C) 2022 guiyang wangyong co.,ltd.

   This program reference glib code.

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

#ifndef __N_FILEUTILS_H__
#define __N_FILEUTILS_H__

#include "nbase.h"
#include "nerror.h"

#define N_FILE_ERROR n_file_error_quark ()

typedef enum
{
  N_FILE_ERROR_EXIST,
  N_FILE_ERROR_ISDIR,
  N_FILE_ERROR_ACCES,
  N_FILE_ERROR_NAMETOOLONG,
  N_FILE_ERROR_NOENT,
  N_FILE_ERROR_NOTDIR,
  N_FILE_ERROR_NXIO,
  N_FILE_ERROR_NODEV,
  N_FILE_ERROR_ROFS,
  N_FILE_ERROR_TXTBSY,
  N_FILE_ERROR_FAULT,
  N_FILE_ERROR_LOOP,
  N_FILE_ERROR_NOSPC,
  N_FILE_ERROR_NOMEM,
  N_FILE_ERROR_MFILE,
  N_FILE_ERROR_NFILE,
  N_FILE_ERROR_BADF,
  N_FILE_ERROR_INVAL,
  N_FILE_ERROR_PIPE,
  N_FILE_ERROR_AGAIN,
  N_FILE_ERROR_INTR,
  N_FILE_ERROR_IO,
  N_FILE_ERROR_PERM,
  N_FILE_ERROR_NOSYS,
  N_FILE_ERROR_FAILED
} NFileError;

/* For backward-compat reasons, these are synced to an old
 * anonymous enum in libgnome. But don't use that enum
 * in new code.
 */
typedef enum
{
  N_FILE_TEST_IS_REGULAR    = 1 << 0,
  N_FILE_TEST_IS_SYMLINK    = 1 << 1,
  N_FILE_TEST_IS_DIR        = 1 << 2,
  N_FILE_TEST_IS_EXECUTABLE = 1 << 3,
  N_FILE_TEST_EXISTS        = 1 << 4
} NFileTest;


typedef enum
{
  N_FILE_SET_CONTENTS_NONE = 0,
  N_FILE_SET_CONTENTS_CONSISTENT = 1 << 0,
  N_FILE_SET_CONTENTS_DURABLE = 1 << 1,
  N_FILE_SET_CONTENTS_ONLY_EXISTING = 1 << 2
} NFileSetContentsFlags;

#define N_IS_DIR_SEPARATOR(c) ((c) == N_DIR_SEPARATOR)


NQuark     n_file_error_quark      (void);
/* So other code can generate a NFileError */

NFileError n_file_error_from_errno (nint err_no);
nboolean n_file_test         (const nchar  *filename,NFileTest     test);

nboolean n_file_get_contents (const nchar  *filename, nchar **contents,nsize *length,NError      **error);
nboolean n_file_set_contents (const nchar *filename,const nchar *contents,nssize length,NError       **error);

nboolean n_file_set_contents_full (const nchar *filename,const nchar *contents,nssize length,NFileSetContentsFlags   flags,
                                   int mode, NError **error);
nchar   *n_file_read_link    (const nchar  *filename,NError      **error);

/* Wrapper / workalike for mkdtemp() */

nchar   *n_mkdtemp            (nchar  *tmpl);
nchar   *n_mkdtemp_full       (nchar  *tmpl,nint  mode);

/* Wrapper / workalike for mkstemp() */

nint     n_mkstemp            (nchar        *tmpl);
nint     n_mkstemp_full       (nchar        *tmpl,nint  flags,nint  mode);

/* Wrappers for n_mkstemp and n_mkdtemp() */

nint     n_file_open_tmp      (const nchar  *tmpl,nchar **name_used,NError      **error);
nchar   *n_dir_make_tmp       (const nchar  *tmpl,NError      **error);


nchar   *n_build_path         (const nchar *separator,const nchar *first_element, ...);
nchar   *n_build_pathv        (const nchar  *separator,nchar **args) ;
nchar   *n_build_filename     (const nchar *first_element,...);
nchar   *n_build_filenamev    (nchar **args) ;
nchar   *n_build_filename_valist (const nchar  *first_element, va_list      *args) ;
nint     n_mkdir_with_parents (const nchar *pathname,nint mode);

nboolean     n_path_is_absolute (const nchar *file_name);
const nchar *n_path_skip_root   (const nchar *file_name);
nchar       *n_get_current_dir   (void);
nchar       *n_path_get_basename (const nchar *file_name) ;
nchar       *n_path_get_dirname  (const nchar *file_name) ;

/**
 * 获取规范文件名
 */
nchar       *n_canonicalize_filename (const nchar *filename,const nchar *relative_to) ;



#endif /* __N_FILEUTILS_H__ */



