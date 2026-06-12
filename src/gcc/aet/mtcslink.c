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
#include "toplev.h"
#include "attribs.h"
#include "stor-layout.h"
#include "varasm.h"
#include "trans-mem.h"
#include "c-family/c-pragma.h"
#include "gcc-rich-location.h"
#include "opts.h"
#include "plugin.h"

#include "c/c-tree.h"
#include "../libcpp/internal.h"
#include "c/c-parser.h"
#include "../libcpp/include/cpplib.h"


#include "aet-c-parser-header.h"
#include "aetutils.h"
#include "aetprinttree.h"
#include "aetprinttoken.h"
#include "aetinfo.h"
#include "makefileparm.h"
#include "middlefile.h"
#include "aetmediator.h"
#include "mtcsparser.h"

#include "mtcslink.h"

#define LINK_START "MTCS LINK START:"
#define LINK_END   "MTCS LINK END:"

typedef struct _LinkInfo
{
   char *srcFile;
   char *mtcsLinkFile;
   int version;
   int isa;
   char *platName;
   char *funcNames[200];
   int funcNameCount;
}LinkInfo;

/**
 * 读取libdevice的内容
 */
static char* readLibDeviceFile(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        fprintf(stderr, "Error: cannot open %s\n", path);
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);

    char *buf = (char*)xmalloc(size + 1);
    if (!buf) {
        fclose(fp);
        return NULL;
    }
    fread(buf, 1, size, fp);
    buf[size] = 0;
    fclose(fp);
    return buf;
}

// helper: find last occurrence of needle in haystack between [start, end)
// returns pointer or NULL
static const char* rfind_substr_range(const char *start, const char *end, const char *needle)
{
    size_t nlen = strlen(needle);
    if (nlen == 0) return NULL;
    const char *p = end - nlen;
    while (p >= start) {
        if (memcmp(p, needle, nlen) == 0) return p;
        p--;
    }
    return NULL;
}

// helper: find last '}' before pos (return pointer to that '}' or NULL)
static const char* rfind_char_before(const char *begin, const char *pos, char ch)
{
    const char *p = pos - 1;
    while (p >= begin) {
        if (*p == ch) return p;
        p--;
    }
    return NULL;
}

// Main robust extractor:
// - ptx: whole PTX buffer (null-terminated)
// - funcname: function name to extract, e.g. "__nv_sinf"
// Returns malloc'd string with the function block (from start-of-line-of-.func to matching '}'),
// or NULL if not found / parse error. Caller must free().
static char* extract_function(const char* ptx, const char* funcname)
{
    if (!ptx || !funcname) return NULL;

    const char *name_pos = ptx;
    while (1) {
        // find next occurrence of funcname
        name_pos = strstr(name_pos, funcname);
        if (!name_pos)
           return NULL; // not found

        // Ensure the found occurrence is an identifier (word boundary)
        // check char before and after
        int before_ok = 1, after_ok = 1;
        if (name_pos > ptx) {
            char bc = *(name_pos - 1);
            if ( (n_ascii_isalnum((unsigned char)bc)) || bc == '_' || bc == '.')
               before_ok = 0;
        }
        char ac = *(name_pos + strlen(funcname));
        if (ac != '\0') {
            if ( (n_ascii_isalnum((unsigned char)ac)) || ac == '_' || ac == '.')
               after_ok = 0;
        }
        if (!before_ok || !after_ok) {
            // this occurrence is part of a larger token, skip it
            name_pos = name_pos + 1;
            continue;
        }

        // Find previous '}' before this name (to avoid matching a .func that belongs to earlier function)
        const char *prev_close = rfind_char_before(ptx, name_pos, '}');
        const char *search_start = prev_close ? prev_close + 1 : ptx;

        // In the region [search_start, name_pos), find the last ".func"
        const char *func_token = rfind_substr_range(search_start, name_pos, ".func");
        if (!func_token) {
            // No .func before this occurrence in the allowed region -> maybe malformed or this name is not a func declaration
            // Try to continue searching further occurrences of the funcname
            name_pos = name_pos + 1;
            continue;
        }

        // Expand start to include possible ".visible" or ".extern" prefix that is right before func_token on same line
        // We'll find the line start (previous newline) and use that as start_of_decl if it contains ".func"
        const char *line_start = func_token;
        while (line_start > ptx && *(line_start - 1) != '\n')
           line_start--;
        // But if there's a ".visible" or ".extern" immediately before .func separated by spaces, include it.
        // Example: ".visible .func" -> include ".visible "
        const char *maybe_vis = line_start;
        // check text between line_start and func_token for ".visible" or ".extern" tokens
        size_t prefix_len = func_token - line_start;
        if (prefix_len > 0) {
            // simple search for "visible" or "extern" earlier on the same line
            if (strstr(line_start, ".visible") == line_start
                  || strstr(line_start, ".extern") == line_start || strstr(line_start, ".weak") == line_start) {
                // keep line_start as-is
            } else {
                // If there are leading tokens with whitespace, try to include them up to first '.' before func_token
                // Find earliest '.' that starts a token on this line (but not inside params)
                const char *dot = line_start;
                const char *lastdot = NULL;
                while (dot < func_token) {
                    if (*dot == '.') lastdot = dot;
                    dot++;
                }
                if (lastdot && lastdot < func_token) {
                    // ensure lastdot is not inside parentheses by checking parentheses count between lastdot and func_token
                    int par = 0;
                    const char *q = lastdot;
                    while (q < func_token) {
                        if (*q == '(') par++;
                        else if (*q == ')') { if (par>0) par--; }
                        q++;
                    }
                    if (par == 0) {
                        // include from lastdot
                        line_start = lastdot;
                    } else {
                        // keep original line_start
                    }
                }
            }
        }

        const char *decl_start = line_start;

        // But ensure decl_start points to ".func" or ".visible" or ".extern"
        // If not, fall back to func_token
        if (strncmp(decl_start, ".func", 5) != 0 &&
            strncmp(decl_start, ".visible", 8) != 0 &&
            strncmp(decl_start, ".extern", 7) != 0 &&
            strncmp(decl_start, ".weak", 5) != 0) {
            decl_start = func_token;
        }

        // Now find the opening brace '{' that begins the function body, starting from decl_start
        const char *brace = strchr(decl_start, '{');
        if (!brace) {
            // maybe function is declared without body (extern); in that case we can try to extract up to a terminating semicolon
            // look for ';' after name_pos
            const char *semi = strchr(name_pos, ';');
            if (semi && semi > name_pos) {
                size_t len = semi - decl_start + 1;
                char *out = (char*)xmalloc(len + 1);
                if (!out) return NULL;
                memcpy(out, decl_start, len);
                out[len] = '\0';
                return out;
            } else {
                // no body and no semicolon => give up for this occurrence, try next occurrence
                name_pos = name_pos + 1;
                continue;
            }
        }

        // Find matching '}' (brace depth)
        int depth = 1;
        const char *q = brace + 1;
        while (*q && depth > 0) {
            if (*q == '{') depth++;
            else if (*q == '}') depth--;
            q++;
        }
        if (depth != 0) {
            // unmatched braces; fail this occurrence and try next
            name_pos = name_pos + 1;
            continue;
        }

        // Success: extract from decl_start to q (q points just after the '}')
        size_t out_len = (size_t)(q - decl_start);
        char *out = (char*)xmalloc(out_len + 1);
        if (!out) return NULL;
        memcpy(out, decl_start, out_len);
        out[out_len] = '\0';
        return out;
    }
}

/**
 * 从xxx_libdevice_yy.ptx中获取指定的函数代码。
 * 硬编码三个函数
 * .func  (.param .b64 func_retval0) __internal_trig_reduction_slowpathd
(
   .param .b64 __internal_trig_reduction_slowpathd_param_0,
   .param .b64 __internal_trig_reduction_slowpathd_param_1
)
;
.func  (.param .b64 func_retval0) __internal_accurate_pow
(
   .param .b64 __internal_accurate_pow_param_0,
   .param .b64 __internal_accurate_pow_param_1
)
;
.func  (.param .b64 func_retval0) __internal_lgamma_pos
 */
static char *catchLibDeviceCodes(char *libDeviceFile,NPtrArray *funcNameArray)
{
    // read full libdevice.ptx
   //fprintf(stderr,"catchLibDeviceCodes --- %s\n",libDeviceFile);
    char *libptx = readLibDeviceFile(libDeviceFile);
    if (!libptx)
       return NULL;
    //取出第一个.visible .func之前的所有代码
    char *str=strstr(libptx,".visible .func");
    int len=strlen(libptx)-strlen(str);
    char header[len+1];
    memcpy(header,libptx,len);
    header[len]='\0';
    NString *codes=n_string_new("");
    n_string_append(codes,header);
    n_string_append(codes,"\n\n");

    int i;
    for(i=0;i<funcNameArray->len;i++){
       char *func=n_ptr_array_index(funcNameArray,i);
       n_debug("mtcslink.c 重要 catchLibDeviceCodes %s %d\n",func,strlen(libptx));
       char *body = extract_function(libptx, func);
       n_string_append_printf(codes,"%s\n\n", body);
       free(body);
    }
    char *body = extract_function(libptx+len, "__internal_trig_reduction_slowpathd");
    n_string_append_printf(codes,"%s\n\n", body);
    free(body);
    body = extract_function(libptx+len, "__internal_accurate_pow");
    n_string_append_printf(codes,"%s\n\n", body);
    free(body);
    //需要加(变成__internal_lgamma_pos(,否则找不到函数定义，找到的是__nv_lgamma中调用的__internal_lgamma_pos
    body = extract_function(libptx+len, "__internal_lgamma_pos(");
    n_string_append_printf(codes,"%s\n\n", body);
    free(body);
    free(libptx);
    return n_string_free(codes,FALSE);
}

static void mtcsLinkInit(MtcsLink *self)
{
   self->collectMtcsLinkFile=NULL;
}

/**
 * 如果源文件的修改时间大于链文件的修改时间，移走LinkInfo
 */
static void removeByTime(NPtrArray *array)
{
   int i;
   for(i=0;i<array->len;i++){
      LinkInfo *info=n_ptr_array_index(array,i);
      NFile *src=n_file_new(info->srcFile);
      NFile *linkFile=n_file_new(info->mtcsLinkFile);
      if(n_file_get_last_modified(src)>n_file_get_last_modified(linkFile)){
         n_ptr_array_remove(array,info);
         i--;
      }
      n_file_unref(src);
      n_file_unref(linkFile);
   }
}

static  nboolean haveFuncName(NPtrArray *array,char *fn)
{
   int i;
   for(i=0;i<array->len;i++){
      char  *funcName=n_ptr_array_index(array,i);
      if(!strcmp(funcName,fn))
        return TRUE;
   }
   return FALSE;
}

/**
 * 获取libdevice系列文件所在的路径。
 * cuda_libdevice_80.ptx
 * 这些文件是由aet.mk安装到lib64目录。
 * 由ptx/nvvm_compile_libdevice.c生成
 */
static char *getLibDevicePath()
{
   char path[1024]={0};
   int ret = readlink("/proc/self/exe",path,sizeof(path)-1);
   if(ret == -1){
      int pid=getpid();
      char fileName[1024];
      sprintf(fileName,"/proc/%d/exe",pid);
      ret = readlink(fileName,path,sizeof(path)-1);
      if(ret==-1){
         //printf("--angian-- get exec name fail!!\n");
         return NULL;
      }
   }
   path[ret]= '\0';
   //获取当前执行文件所在的路径 /home/sns/gcc152/gccnvptx/libexec/gcc/x86_64-pc-linux-gnu/15.2.0/cc1
   //libexec
   NString *execPath=n_string_new(path);
   int pos=n_string_last_indexof(execPath,"/libexec/");
   NString *rootPath=n_string_substring_from(execPath,0,pos);
   n_string_append(rootPath,"/lib64/");
   n_string_free(execPath,TRUE);
   return n_string_free(rootPath,FALSE);
}

typedef struct _LibDeviceData
{
    int diff;
    NFile *file;
}LibDeviceData;

static nint compareLibDevice_cb(nconstpointer  cand1,nconstpointer  cand2)
{
   LibDeviceData *p1 = *((LibDeviceData **) cand1);
   LibDeviceData *p2 = *((LibDeviceData **) cand2);
    int a=p1->diff;
    int b=p2->diff;
    nint r= (a > b ? +1 : a == b ? 0 : -1);
    return r;
}

/**
 * 获取最接近isa的文件
 * 文件格式 cuda_libdevice_xx.ptx
 * xx是sm的版本号
 */
static NFile *getCudaFile(char *isa,NList *list)
{
   int isaNumber=atoi(isa);
   int len=n_list_length(list);
   int i;
   NPtrArray *array=n_ptr_array_new();
   for(i=0;i<len;i++){
      NFile *item=n_list_nth_data(list,i);
      char *name=n_file_get_name(item);
      if(startswith(name,"cuda_libdevice_")){
         NString *f=n_string_new(name);
         int last=n_string_last_indexof(f,".ptx");
         NString *sm=n_string_substring_from(f,0+strlen("cuda_libdevice_"),last);
         int v=atoi(sm->str);
         if(v-isaNumber>0){
            LibDeviceData data={v-isaNumber,item};
            n_ptr_array_add(array,&data);
         }
         n_string_free(f,TRUE);
         n_string_free(sm,TRUE);
      }
   }
   if(array->len>0){
      n_ptr_array_sort(array,compareLibDevice_cb);
      LibDeviceData *d=n_ptr_array_index(array,0);
      n_ptr_array_unref(array);
      return d->file;
   }
   return NULL;
}
/**
 * 在array中已经是统一平台和version,isa的LinkInfo
 * 去除重复的函数名
 */
static void createCodes(NPtrArray *array,NString *codes)
{
   int i;
   NPtrArray *onlyFuncNameArray=n_ptr_array_new();
   char *platName=NULL;
   int version;
   int isa;
   for(i=0;i<array->len;i++){
      LinkInfo *info=n_ptr_array_index(array,i);
      if(i==0){
         platName=info->platName;
         version=info->version;
         isa=info->isa;
      }
      //生成不重复的函数名
      int j;
      for(j=0;j<info->funcNameCount;j++){
         if(!haveFuncName(onlyFuncNameArray,info->funcNames[j])){
            n_ptr_array_add(onlyFuncNameArray,info->funcNames[j]);
         }
      }
   }

   if(onlyFuncNameArray->len>0){
      if(strcmp(platName,"cuda")==0){
         //cuda_libdevice_80.ptx文件安装在和libaet.so的一个目录
         char *computeVersion = aet_mediator_get_compute_version(aet_mediator_get(),platName,version,isa,(AetMediatorUser*)mtcs_parser_get());
         char *libDevicePath=getLibDevicePath();
         char fileName[512];
         sprintf(fileName,"%scuda_libdevice_%s.ptx",libDevicePath,computeVersion);
         //printf("生成libdevice代码----%s %d %d\n",computeVersion,version,isa);
         NFile *file=n_file_new(fileName);
         if(!n_file_exists(file)){
            NFile *path=n_file_new(libDevicePath);
            NList *list=n_file_list_files_to_list(path);
            NFile *ret=getCudaFile(computeVersion,list);
            n_file_unref(file);
            file=NULL;
            if(ret)
              file=n_file_new(n_file_get_absolute_path(ret));
            n_file_unref(path);
            n_list_free(list);
         }
         if(file==NULL){
            n_error("cuda平台设备函数文件:%s不存在。",fileName);
            return;
         }
         char *ptxcode=catchLibDeviceCodes(n_file_get_absolute_path(file),onlyFuncNameArray);
         char *varName=aet_mediator_get_asm_var_name(aet_mediator_get(),platName,version,isa,NULL,(AetMediatorUser*)mtcs_parser_get());
         n_string_append_printf(codes,"const char %s[]= R\"%%%(\n%s\n )%%%\";\n\n",varName,ptxcode);
         {
              char *aetDump=getenv ("GCC_AET_DUMP");
              if(aetDump && (!strcmp(aetDump,"true") || !strcmp(aetDump,"TRUE"))){
                 n_debug("mtcslink.c 测试 写入数学库到 matlib \n");
                 FILE *testFile=fopen("matlib.ptx","w");
                 fwrite(ptxcode,1,strlen(ptxcode),testFile);
                 fclose(testFile);
              }
         }
         n_file_unref(file);
         free(ptxcode);
         free(varName);
      }else{
         n_error("%s平台还未支持！",platName);
      }
   }
}

/**
 * 合并平台
 */
static NPtrArray *mergePlat(NPtrArray *array)
{
   //第一步，合并平台
   int i;
   NHashTable *platHash=n_hash_table_new_full (n_str_hash, n_str_equal,n_free, NULL);
   for(i=0;i<array->len;i++){
      LinkInfo *info=n_ptr_array_index(array,i);
      if(!n_hash_table_contains(platHash,info->platName)){
         NPtrArray *infoArray=n_ptr_array_sized_new(2);
         n_ptr_array_add(infoArray,info);
         n_hash_table_insert (platHash, n_strdup(info->platName),infoArray);
      }else{
         NPtrArray *infoArray=(NPtrArray *)n_hash_table_lookup(platHash,info->platName);
         n_ptr_array_add(infoArray,info);
      }
   }
   //第二步，合并vesion,isa
   //platArray 存放的是每个平台的对应的version,isa的linkinfo
   NPtrArray *platArray=n_ptr_array_new();
   NHashTableIter iter;
   npointer key, value;
   n_hash_table_iter_init(&iter, platHash);
   while (n_hash_table_iter_next(&iter, &key, &value)) {
      NPtrArray *infoArray = (NPtrArray *)value;
      NHashTable *versionIsaHash=n_hash_table_new_full (n_str_hash, n_str_equal,n_free, NULL);
      for(i=0;i<infoArray->len;i++){
         LinkInfo *info=n_ptr_array_index(infoArray,i);
         char vi[30];
         sprintf(vi,"%d_%d",info->version,info->isa);
         if(!n_hash_table_contains(versionIsaHash,vi)){
            NPtrArray *hashArray=n_ptr_array_sized_new(2);
            n_ptr_array_add(hashArray,info);
            n_hash_table_insert (versionIsaHash, n_strdup(vi),hashArray);
         }else{
            NPtrArray *hashArray=(NPtrArray *)n_hash_table_lookup(versionIsaHash,vi);
            n_ptr_array_add(hashArray,info);
         }
      }
      n_ptr_array_add(platArray,versionIsaHash);
   }
   n_hash_table_unref(platHash);
   return platArray;

}

/**
 * 根据 writeFuncNames 的写入格式。
 * mtcsLinkFileName xxx.mtcslink.o文件 xxx是源文件的输出文件
 */
static LinkInfo *createLinkInfo(char *content)
{
   nchar **items=n_strsplit(content,"\n",-1);
   int length= n_strv_length(items);
   int i;
   LinkInfo *info=n_slice_new0(LinkInfo);
   info->srcFile=n_strdup(items[0]);
   info->mtcsLinkFile=n_strdup(items[1]);
   info->version=atoi(items[2]);
   info->isa=atoi(items[3]);
   info->platName=n_strdup(items[4]);
   int count=0;
   for(i=5;i<length;i++)
      if(items[i]!=NULL && strlen(items[i])>1)
         info->funcNames[count++]=n_strdup(items[i]);

   info->funcNameCount=count;
   n_strfreev(items);
   return info;
}


/**
 * 从xxx.mtcslink.o文件的内容生成LinkInfo
 */
static void createLinkInfoArray(NPtrArray *array,char *buffer)
{
   if(buffer==NULL)
      return;
   char *c=buffer;
   while(strstr(c,LINK_START)){
      char *start=strstr(c,LINK_START);
      //printf("r0 is :%s\n",start);
      char *n=start+strlen(LINK_START)+1;//加1跳过 LINK_START 后的\n号
    //  printf("r1 is :%s\n",n);
      char *end=strstr(n,LINK_END);
      //printf("r2 is :%s\n",end);
      int len=strlen(n);
      int remain=strlen(end);
      char *ret=xmalloc(len-remain+1);
      memcpy(ret,n,len-remain);
      ret[len-remain]='\0';
      //printf("mtcslink.c createLinkInfoArray :%s\n",ret);
      LinkInfo *info=createLinkInfo(ret);
      n_ptr_array_add(array,info);
      free(ret);
      c = end+strlen(LINK_END);
   }
}

/**
 * fileName是 xxx.mtcslink.o xxx是源文件对应的输出o文件。
 * 从xxx.mtcslink.o文件生成LinkInfo数组。
 */
static NPtrArray * readFile(char *fileName)
{
   NPtrArray *array=n_ptr_array_new();
   FILE *fp=fopen(fileName,"r");
   //printf("mtcslink.c readFile  00 fileName:%s fp:%p\n",fileName,fp);
   if(fp){
      char buffer[1024*150];
      int rev=fread(buffer,1,1024*150,fp);
      fclose(fp);
      if(rev>0){
         buffer[rev]='\0';
         createLinkInfoArray(array,buffer);
      }
   }
   return array;
}

/**
 * 移走LinkInfo中的函数名。
 */
static void removeFuncName(LinkInfo *info)
{
   if(info==NULL)
      return;
   int i;
   for(i=0;i<info->funcNameCount;i++){
      n_free(info->funcNames[i]);
      info->funcNames[i]=NULL;
   }
   info->funcNameCount=0;
}

static void addFuncName(LinkInfo *info,char *newFuncNames)
{
   if(info==NULL)
      return;
   nchar **items=n_strsplit(newFuncNames,"\n",-1);
   int length= n_strv_length(items);
   int i;
   for(i=0;i<length;i++)
      info->funcNames[i]=n_strdup(items[i]);
   info->funcNameCount=length;
   n_strfreev(items);
}

static void appendLinkInfo(NString *content,LinkInfo *info)
{
   n_string_append(content,LINK_START);
   n_string_append(content,"\n");
   n_string_append(content,info->srcFile);
   n_string_append(content,"\n");
   n_string_append(content,info->mtcsLinkFile);
   n_string_append(content,"\n");
   n_string_append_printf(content,"%d\n",info->version);
   n_string_append_printf(content,"%d\n",info->isa);
   n_string_append(content,info->platName);
   n_string_append(content,"\n");
   int i;
   for(i=0;i<info->funcNameCount;i++){
      n_string_append(content,info->funcNames[i]);
      n_string_append(content,"\n");
   }
   n_string_append(content,LINK_END);
   n_string_append(content,"\n");
}

/**
 * 写入linkInfo到xxx.mtcslink.o文件
 */
static void writeFile(char *fileName,NPtrArray *linkArray)
{
   if(linkArray->len==0){
      remove(fileName);
      return;
   }
   FILE *fp=fopen(fileName,"w");
   if(fp){
      int i;
      NString *codes=n_string_new("");
      for(i=0;i<linkArray->len;i++){
         LinkInfo *info=n_ptr_array_index(linkArray,i);
         appendLinkInfo(codes,info);
      }
      fwrite(codes->str,1,codes->len,fp);
      n_string_free(codes,TRUE);
      fclose(fp);
   }
}

static LinkInfo *newLinkInfo(char *linkFileName,const char *linkFuncNames,int version,int isa,const char *platName)
{
   nchar **items=n_strsplit(linkFuncNames,"\n",-1);
   int length= n_strv_length(items);
   int i;
   LinkInfo *info=n_slice_new0(LinkInfo);
   info->srcFile=n_strdup(in_fnames[0]);
   info->mtcsLinkFile=n_strdup(linkFileName);
   info->version=version;
   info->isa=isa;
   info->platName=n_strdup(platName);
    for(i=0;i<length;i++)
       info->funcNames[i]=n_strdup(items[i]);
    info->funcNameCount=length;
    n_strfreev(items);
    return info;
}

static void freeLinkInfo_cb(LinkInfo *info)
{
   if(info==NULL)
      return;
   int i;
   n_free(info->srcFile);
   n_free(info->mtcsLinkFile);
   n_free(info->platName);
   for(i=0;i<info->funcNameCount;i++){
      n_free(info->funcNames[i]);
      info->funcNames[i]=NULL;
   }
   n_slice_free(LinkInfo,info);
}

/**
 * 从当前编译的文件中加入mtcs需要链接的函数。
 */
void  mtcs_link_add(MtcsLink *self,const char *linkFuncNames,int version,int isa,const char *platName)
{
   char fileName[512];
   char  *objfile=makefile_parm_get_object_file(makefile_parm_get());
   sprintf(fileName,"%s.mtcslink_new.o",objfile);
   NPtrArray *linkArray=readFile(fileName);
   int i;
   nboolean find=FALSE;
   for(i=0;i<linkArray->len;i++){
      LinkInfo *info=n_ptr_array_index(linkArray,i);
      //printf("mtcslink info---%d %s\n",i,linkFuncNames);
      if(strcmp(info->platName,platName)==0 && info->version==version && info->isa==isa){
         //有相同的平台 version,isa
         //用新的linkFuncNames覆盖老的
         //printf("在原有的文件中找到相同的平台:%s %s %d %d\n",fileName,platName,version,isa);
         if(linkFuncNames==NULL){
            n_ptr_array_remove(linkArray,info);
            freeLinkInfo_cb(info);
         }else{
            removeFuncName(info);
            addFuncName(info,linkFuncNames);
         }
         find=TRUE;
         break;
      }
   }
   if(find){
      writeFile(fileName,linkArray);
   }else{
      if(linkFuncNames!=NULL){
         LinkInfo *info=newLinkInfo(fileName,linkFuncNames,version,isa,platName);
         n_ptr_array_add(linkArray,info);
         writeFile(fileName,linkArray);
      }
   }

   if(linkArray->len==0){
      //printf("mtcs_link_add 11 不写入COMPILE_MTCS_LINK  :%s\n",fileName);
      remove(fileName);
   }else{
      //printf("mtcs_link_add 22 %s :%s\n",linkFuncNames,fileName);
      gcc_assert(self->collectMtcsLinkFile==NULL);
      self->collectMtcsLinkFile=n_strdup(fileName);
      middle_file_modify(middle_file_get(),COMPILE_MTCS_LINK);
   }
   n_ptr_array_set_free_func(linkArray,freeLinkInfo_cb);
   n_ptr_array_unref(linkArray);

}

/**
 * 链接外部数学库中的函数，
 * 例如:cuda的libdevice.ptx
 * 处于正在编译文件temp_func_track_45.c
 */
void  mtcs_link_link(MtcsLink *self)
{
   char *fileName = getenv("GCC_AET_MTCS_LINK_LIST_PATH");
   //printf("mtcs_link_link 进入 %s\n",fileName);
   if(fileName==NULL ||strlen(fileName)==0){
      return;
   }
   char compileFileName[255];
   sprintf(compileFileName,"%s.o",fileName);
   FILE *fp=fopen(fileName,"r");
   char fileList[10*1024];
   int rev=fread(fileList,1,10*1024,fp);
   fclose(fp);
   if(rev<=0){
      remove(compileFileName);
      return ;
   }
   fileList[rev]='\0';
   nchar **items=n_strsplit(fileList,"\n",-1);
   int length= n_strv_length(items);
   int i,j;
   NPtrArray *allLinkInfoArray=n_ptr_array_new_with_free_func(freeLinkInfo_cb);
   for(i=0;i<length;i++){
      char *fileName=items[i];
      NPtrArray *linkArray=readFile(fileName);//从文件的内容生成LinkInfo数组
      for(j=0;j<linkArray->len;j++){
         n_ptr_array_add(allLinkInfoArray,n_ptr_array_index(linkArray,j));
      }
      n_ptr_array_unref(linkArray);
   }
   n_strfreev(items);

   if(allLinkInfoArray->len==0){
      n_ptr_array_unref(allLinkInfoArray);
      remove(compileFileName);
      return;
   }
   //清除源文件时间大于xxx.mtcslink.o的linkInfo
   removeByTime(allLinkInfoArray);
   //合并LinkInfo
   NString *codes=n_string_new("");
   NPtrArray *platArray=mergePlat(allLinkInfoArray);
   //第三步，生成每个平台每个version,isa的引用的函数列表。
   //versionIsaHash 不重复杂
   for(i=0;i<platArray->len;i++){
      NHashTable *versionIsaHash=n_ptr_array_index(platArray,i);
      NHashTableIter iter;
        npointer key, value;
        n_hash_table_iter_init(&iter, versionIsaHash);
        while (n_hash_table_iter_next(&iter, &key, &value)) {
           //相同平台、version和isa的LinkInfo
           NPtrArray *infoArray = (NPtrArray *)value;
           createCodes(infoArray,codes);
        }
   }
   //printf("mtcslink.c mtcs_link_link file:%s str:%s\n",compileFileName,codes->str);
   if(codes->len==0){
      remove(compileFileName);
   }else{
      FILE *fp=fopen(compileFileName,"w");
      fwrite(codes->str,1,codes->len,fp);
      fclose(fp);
   }
   n_string_free(codes,TRUE);
}

MtcsLink  *mtcs_link_new()
{
   MtcsLink *self =n_slice_alloc0 (sizeof(MtcsLink));
   mtcsLinkInit(self);
   return self;
}



