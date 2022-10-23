#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "AKeyFile.h"
#include "KeyFileUtil.h"
#include "ASList.h"


struct _KeyFileKeyValuePair
{
  char *key;  /* 如果是空的，是注释 */
  char *value;
};

struct _KeyFileGroup
{
  const char *name;  /* 组名如果是空的，组里将是注释 */
  AList *keyValuePairList;
  AHashTable *lookup_map;
};

static void g_key_file_key_value_pair_free (KeyFileKeyValuePair *pair)
{
    if (pair != NULL){
      a_free (pair->key);
      a_free (pair->value);
      a_slice_free (KeyFileKeyValuePair, pair);
    }
}

impl$ AKeyFile{

    static void freeKeyFileKeyValuePair_cb(KeyFileKeyValuePair *item){
       if(item->key!=NULL){
           a_free(item->key);
       }
       if(item->value!=NULL){
           a_free(item->value);
       }
       a_slice_free(KeyFileKeyValuePair,item);
    }

    static void freeKeyFileGroup_cb(KeyFileGroup *item){
       if(item->name!=NULL){
           a_free(item->name);
       }
       if(item->lookup_map){
           item->lookup_map->unref();
        }
       if(item->keyValuePairList){
           item->keyValuePairList->unref();
       }
       a_slice_free(KeyFileGroup,item);
    }

    static void clear(){
       if (self->locales){
          a_strfreev (locales);
          locales = NULL;
       }
       if (strBuffer){
          strBuffer->unref();
          strBuffer = NULL;
       }

       if (groups != NULL){
           groups->unref();
           groups = NULL;
       }

       if (groupHash != NULL){
          groupHash->unref();
          groupHash = NULL;
       }
       if(startGroup){
          startGroup=NULL;
       }
       if(currentGroup){
          currentGroup=NULL;
       }
    }

    static void init(){
        currentGroup = a_slice_new0 (KeyFileGroup);
        currentGroup->keyValuePairList=new$ AList();
        groups = new$ AList();
        groups->add(currentGroup);
        groupHash =new$ AHashTable(AHashTable.strHash,AHashTable.strEqual,a_free,NULL);
        startGroup = NULL;
        strBuffer = new$ AString(128);
        list_separator = ';';
        flags = 0;
        locales =NULL;// a_strdupv ((gchar **)g_get_language_names ());
    }

    static void g_key_file_parse_comment (const char  *line,asize length){
        KeyFileKeyValuePair *pair;
        if (!(flags & KeyFileFlags.KEEP_COMMENTS))
          return;
        printf("g_key_file_parse_comment 00 %s self->currentGroup:%p groupName:%s\n",line,self->currentGroup,self->currentGroup->name);
        a_warn_if_fail (self->currentGroup != NULL);
        pair = a_slice_new (KeyFileKeyValuePair);
        pair->key = NULL;
        pair->value = a_strndup (line, length);
        printf("g_key_file_parse_comment 11 %s self->currentGroup:%p\n",line,currentGroup->keyValuePairList);
        currentGroup->keyValuePairList->add(pair);

    }

    static KeyFileGroup *g_key_file_lookup_group (const char *groupName){
        ListIterator *iter=groups->initIter();
        apointer value=NULL;
        while(iter->hasNext(&value)){
            KeyFileGroup *gp=(KeyFileGroup *)value;
            if(gp->name && !strcmp(gp->name,groupName))
                return gp;
        }
        return NULL;
    }

    /**
     * 加组，如果当前组是最后是注释，要移到新组的头部，作为组的注释。
     */
    static void g_key_file_add_group (const char *groupName){
      KeyFileGroup *group = g_key_file_lookup_group(groupName);
      if (group != NULL){
          if(!repeatGroup){
              a_error("有重复的组。%s\n",groupName);
              return;
          }
      }
      group = a_slice_new0 (KeyFileGroup);
      group->name = a_strdup (groupName);
      group->lookup_map = new$ AHashTable(AHashTable.strHash, AHashTable.strEqual,NULL,NULL);
      group->keyValuePairList=new$ AList(AKeyFile.freeKeyFileKeyValuePair_cb);//加的也是KeyFileKeyValuePair，所以不释放
      int i;
      //printf("加组时，当前组:%s %d group:%s\n",currentGroup->name,currentGroup->keyValuePairList->length(),groupName);
      for(i=currentGroup->keyValuePairList->length()-1;i>=0;i--){
          KeyFileKeyValuePair *pair=(KeyFileKeyValuePair *)currentGroup->keyValuePairList->get(i);
          //printf("isxxx iis :%d %s %s\n",i,pair->key,pair->value);
          if(!pair->key){
              printf("加组时，从当前组的最后把注释移到新组，变成新组的注释。i:%d value: %s len:%d\n",i,pair->value,currentGroup->keyValuePairList->length());
              group->keyValuePairList->addFirst(pair);
              currentGroup->keyValuePairList->remove(pair);
             // i--;
              printf("加组时，从当前组的最后把注释移到新组，xxx 变成新组的注释。i:%d value: %s len:%d\n",i,pair->value,currentGroup->keyValuePairList->length());

          }else{
              break;
          }
      }
      if(currentGroup->keyValuePairList->length()==0 && !currentGroup->name){
           printf("当前组没有内容从groups移走它--- name:%s\n",currentGroup->name);
           groups->remove(currentGroup);
           freeKeyFileGroup_cb(currentGroup);
           currentGroup=NULL;
      }

      groups->add(group);
      currentGroup = group;
      if (startGroup == NULL)
        startGroup = group;
      apointer value=groupHash->get(group->name);
      auint count=1;
      if(value!=NULL){
         count+=APOINTER_TO_UINT(value);
      }
      groupHash->put(a_strdup(group->name), AUINT_TO_POINTER(count));

    }

    static aboolean g_key_file_locale_is_interesting (const char *locale){
       asize i;
       if (flags & KeyFileFlags.KEEP_TRANSLATIONS)
         return TRUE;
       for (i = 0; locales[i] != NULL; i++){
          if (a_ascii_strcasecmp (locales[i], locale) == 0)
            return TRUE;
       }
       return FALSE;
    }

    static void     g_key_file_parse_group (const char  *line,asize length,AError  **error){
      const achar *group_name_start, *group_name_end;

      /* advance past opening '['
       */
      group_name_start = line + 1;
      group_name_end = line + length - 1;

      while (*group_name_end != ']')
        group_name_end--;

      char *groupName = a_strndup (group_name_start,
                              group_name_end - group_name_start);

      if (!KeyFileUtil.isGroupName(groupName)){
          a_error_printf(error, ERROR_DOMAIN,-1,"无效的组名：%s",groupName);
          a_free (groupName);
          return;
      }
      g_key_file_add_group (groupName);
      a_free (groupName);
    }

    static void g_key_file_add_key_value_pair (KeyFileGroup *group,KeyFileKeyValuePair *pair){
        //printf("g_key_file_add_key_value_pair 00--- %p %s %p\n",group->lookup_map,pair->key,pair);
        group->lookup_map->replace(pair->key, pair);
        //printf("g_key_file_add_key_value_pair 11--- %p %s %p\n",group->keyValuePairList,pair->key,pair);
        group->keyValuePairList->add(pair);
    }

    static void g_key_file_parse_key_value_pair (const char  *line,asize length,AError **error){
      char *key, *value, *key_end, *value_start, *locale;
      asize key_len, value_len;

      if (currentGroup == NULL || currentGroup->name == NULL){
          printf("g_key_file_parse_key_value_pair --文件内容并不是从组开始的- %s %d\n",line,length);
          a_error_set (error, ERROR_DOMAIN,-1,"文件内容并不是从组开始的。");
          return;
      }

      key_end = value_start = strchr (line, '=');
      a_warn_if_fail (key_end != NULL);
      key_end--;
      value_start++;
      //printf("g_key_file_parse_key_value_pair 00-- %s\n",key_end);

      /* Pull the key name from the line (chomping trailing whitespace)
       */
      while (a_ascii_isspace (*key_end))
        key_end--;

      key_len = key_end - line + 2;

      a_warn_if_fail (key_len <= length);
      //printf("g_key_file_parse_key_value_pair 11-- %s %s %d\n",key_end,line,key_len);
      key = a_strndup (line, key_len - 1);
      //printf("g_key_file_parse_key_value_pair 22-- %s %s %d\n",key,line,key_len);
      if (!KeyFileUtil.isKeyName(key)){
          a_error_printf (error, ERROR_DOMAIN,-1,"无效的key: %s", key);
          a_free (key);
          return;
      }

      /* Pull the value from the line (chugging leading whitespace)
       */
      while (a_ascii_isspace (*value_start))
        value_start++;
      //printf("g_key_file_parse_key_value_pair 33-- %s %s %d\n",key,line,key_len);
      value_len = line + length - value_start + 1;
      value = a_strndup (value_start, value_len);
      //printf("g_key_file_parse_key_value_pair 44-- %s %s %d\n",key,value,key_len);
      a_warn_if_fail (startGroup != NULL);
      if (currentGroup && currentGroup->name && strcmp(startGroup->name,currentGroup->name)== 0  && strcmp (key, "Encoding") == 0){
          if (a_ascii_strcasecmp (value, "UTF-8") != 0){
              char *value_utf8 = a_utf8_make_valid (value, value_len);
              a_error_printf(error, ERROR_DOMAIN,-1,"文件内容的编码%sKey不支持。", value_utf8);
              a_free (value_utf8);
              a_free (key);
              a_free (value);
              return;
          }
      }

      /* Is this key a translation? If so, is it one that we care about?
       */
      locale = KeyFileUtil.getLocal(key);
      //printf("local iss %s\n",locale);
      if (locale == NULL || g_key_file_locale_is_interesting (locale)){
          KeyFileKeyValuePair *pair;
          pair = a_slice_new (KeyFileKeyValuePair);
          pair->key = key;
          pair->value = value;
          g_key_file_add_key_value_pair (currentGroup, pair);
          //printf("local vvv wwwwidddss %s %s %d\n",key,value,strlen(value));
      }else{
          a_free (key);
          a_free (value);
      }
      a_free (locale);
    }
    static int ssdss=0;
    static void g_key_file_parse_line (const char  *line,asize  length,AError **error){
      AError *curErr = NULL;
      char *line_start;
      a_return_if_fail (line != NULL);
      line_start = (char *) line;
      while (a_ascii_isspace (*line_start))
        line_start++;
      //printf("g_key_file_parse_line --- line:%s\n",line);
     // printf("g_key_file_parse_line --- line_start:%s\n",line_start);

      if (KeyFileUtil.isComment(line_start)){
        // printf("g_key_file_parse_line --- isComment line:%s\n",line);
         g_key_file_parse_comment (line, length);
      }else if (KeyFileUtil.isGroup(line_start)){
          //printf("g_key_file_parse_line --- isGroup %d line:%s\n",ssdss++,line);
        g_key_file_parse_group (line_start,length - (line_start - line), &curErr);
      }else if (KeyFileUtil.isKeyValuePair(line_start)){
        // printf("g_key_file_parse_line --- isKeyValuePair line:%s\n",line);
        g_key_file_parse_key_value_pair (line_start,length - (line_start - line),&curErr);
      }else{
         // printf("g_key_file_parse_line --- 出错了 line:%s\n",line);
          achar *line_utf8 = a_utf8_make_valid (line, length);
          a_error_printf(error, ERROR_DOMAIN,-1,"键值对文件包含的行“%s”不是键值对、组或注释",line_utf8);
          a_free (line_utf8);
          return;
      }
      if (curErr)
        a_error_transfer(error, curErr);
    }

    static void g_key_file_flush_parse_buffer (AError **error){
      AError *curErr = NULL;
      if (strBuffer->length() > 0){
          //printf("g_key_file_flush_parse_buffer ---00 %s %d\n",strBuffer->str,strBuffer->length);
          g_key_file_parse_line (strBuffer->getString(),strBuffer->length(),&curErr);
          //printf("g_key_file_flush_parse_buffer ---11 %d\n",strBuffer->length);
          strBuffer->erase (0, -1);
         // printf("g_key_file_flush_parse_buffer ---22 %d\n", strBuffer->length);
          if (curErr){
              a_error_transfer(error, curErr);
              return;
          }
      }
    }


    static void g_key_file_parse_data (const char *data,asize  length,AError **error){
      AError *curErr=NULL;
      asize i;
      a_return_if_fail (data != NULL || length == 0);
      i = 0;
      //strBuffer->getString()
      while (i < length){
          if (data[i] == '\n'){
              //printf("g_key_file_parse_data 换行 %d %s\n",strBuffer->length,strBuffer->str);
              if (strBuffer->length() > 0  && (strBuffer->getString()[strBuffer->length() - 1] == '\r')){
                   printf("g_key_file_parse_data 11 换行 %d\n",strBuffer->length());
                   strBuffer->erase (strBuffer->length() - 1,1);
              }

              /* When a newline is encountered flush the parse buffer so that the
               * line can be parsed.  Note that completely blank lines won't show
               * up in the parse buffer, so they get parsed directly.
               */
              if (strBuffer->length() > 0){
                  //printf("g_key_file_parse_data 22 换行 %d\n",strBuffer->length);
                  g_key_file_flush_parse_buffer (&curErr);
              }else{
                   //printf("g_key_file_parse_data 00 ----\n");
                   g_key_file_parse_comment ("", 1);
              }

              if (curErr){
                  a_error_transfer(error, curErr);
                  return;
              }
              i++;
          }else{
              const char *start;
              const char *end;
              asize lineLen;
              start = data + i;
              end = memchr (start, '\n', length - i);
              if (end == NULL)
                  end = data + length;
              lineLen = end - start;
              strBuffer->append(start, lineLen);
              //printf("g_key_file_parse_data 22 ----%d %s\n",lineLen,start);
              i += lineLen;
           }
        }
    }

    static KeyFileKeyValuePair *g_key_file_lookup_key_value_pair (KeyFileGroup *group,const char *key){
        return (KeyFileKeyValuePair *) group->lookup_map->get(key);
    }

    static void   set_not_found_key_error (const char *group_name,const char *key,AError  **error){
        a_error_printf(error, ERROR_DOMAIN,KeyFileError.KEY_NOT_FOUND,"Key file does not have key “%s” in group “%s”",key, group_name);
    }

     /**
      * 在组中获取key对应的值
      * 返回的值不能释放。
      */
    char *getValue (const char  *groupName,const char *key,AError **error){
      KeyFileGroup *group;
      KeyFileKeyValuePair *pair;
      char *value = NULL;
      a_return_val_if_fail (groupName != NULL, NULL);
      a_return_val_if_fail (key != NULL, NULL);
      group = g_key_file_lookup_group(groupName);
      if (!group){
          a_error_printf(error, ERROR_DOMAIN,-2,"Key file does not have group “%s”",groupName);
          return NULL;
      }
      pair = g_key_file_lookup_key_value_pair (group, key);
      if (pair)
        value = pair->value;
      else
        set_not_found_key_error (groupName, key, error);
      return value;
    }

    static char *g_key_file_parse_value_as_string (const char  *value,ASList *pieces,AError **error){
      char *string_value, *p, *q0, *q;
      string_value = a_new (char, strlen (value) + 1);
      p = (char *) value;
      q0 = q = string_value;
      while (*p){
          if (*p == '\\'){
              p++;
              switch (*p){
                case 's':
                  *q = ' ';
                  break;
                case 'n':
                  *q = '\n';
                  break;
                case 't':
                  *q = '\t';
                  break;
                case 'r':
                  *q = '\r';
                  break;
                case '\\':
                  *q = '\\';
                  break;
                case '\0':
                  a_error_set(error, ERROR_DOMAIN,-4,"Key file contains escape character at end of line");
                  break;
                default:
                  if (pieces && *p == self->list_separator)
                     *q = self->list_separator;
                  else{
                     *q++ = '\\';
                     *q = *p;
                     if (*error == NULL){
                       char sequence[3];
                       sequence[0] = '\\';
                       sequence[1] = *p;
                       sequence[2] = '\0';
                       a_error_printf(error, ERROR_DOMAIN,-4,"Key file contains escape sequence “%s”",sequence);
                     }
                   }
                   break;
               }//end switch
          }else{
               *q = *p;
               if (pieces && (*p == self->list_separator)){
                  // pieces->addFirst(a_strndup (q0, q - q0));
                   pieces->add(a_strndup (q0, q - q0));

                  q0 = q + 1;
               }
          }
          if (*p == '\0')
             break;
          q++;
          p++;
      }//end while

      *q = '\0';
      if (pieces){
        if (q0 < q)
          pieces->add(a_strndup (q0, q - q0));
      }
      return string_value;
    }

    /**
     * 获取组 key对应的变量
     */
    char *getString(const char  *groupName,const char *key,AError **error) {
      char *value, *string_value;
      AError *curErr=NULL;
      a_return_val_if_fail (groupName != NULL, NULL);
      a_return_val_if_fail (key != NULL, NULL);
      value = getValue(groupName, key, &curErr);
      if (curErr){
          a_error_transfer (error, curErr);
          return NULL;
      }
      if (!a_utf8_validate (value, -1, NULL)){
          achar *value_utf8 = a_utf8_make_valid (value, -1);
          a_error_printf(error, ERROR_DOMAIN,-4,"Key file contains key “%s” with value “%s” which is not UTF-8", key, value_utf8);
          a_free (value_utf8);
          return NULL;
      }
      string_value = g_key_file_parse_value_as_string (value, NULL,&curErr);
      if (curErr){
          if (a_error_matches (curErr, ERROR_DOMAIN,-3)){
              a_error_printf(error, ERROR_DOMAIN,-3,"Key file contains key “%s” which has a value that cannot be interpreted.", key);
              a_error_free (curErr);
          }else
            a_error_transfer(error, curErr);
      }
      return string_value;
    }

    char **getKeys(const char *groupName,asize *length,AError **error){
          KeyFileGroup *group;
          char **keys;
          asize i, num_keys;
          a_return_val_if_fail (groupName != NULL, NULL);
          group = g_key_file_lookup_group(groupName);
          if (!group){
              a_error_printf(error, ERROR_DOMAIN, KeyFileError.GROUP_NOT_FOUND,"Key file does not have group “%s”",groupName);
              return NULL;
          }
          num_keys = 0;
          apointer value=NULL;
          ListIterator *iter=group->keyValuePairList->initIter();
          while(iter->hasNext(&value)){
              KeyFileKeyValuePair *pair = (KeyFileKeyValuePair *)value;
              if(pair->key)//屏蔽掉注释
                   num_keys++;
          }
          keys = a_new (char *, num_keys + 1);
          i = 0;
          iter=group->keyValuePairList->initIter();
          while(iter->hasNext(&value)){
              KeyFileKeyValuePair *pair = (KeyFileKeyValuePair *)value;
              if(pair->key){
                  keys[i++] = a_strdup (pair->key);
              }
          }
          keys[num_keys] = NULL;
          if (length)
            *length = num_keys;
          return keys;
    }

    static ListNode *g_key_file_lookup_key_value_pair_node (KeyFileGroup  *group,const char *key){
          apointer value=NULL;
          ListIterator *iter=group->keyValuePairList->initIter();
          while(iter->hasNext(&value)){
              KeyFileKeyValuePair *pair = (KeyFileKeyValuePair *)value;
              if (pair->key && strcmp (pair->key, key) == 0){
                     return iter->getPrevNode();
              }
          }
          return NULL;
    }

    static ListNode *g_key_file_lookup_group_node (const char *groupName){
         apointer value=NULL;
         ListIterator *iter=groups->initIter();
         while(iter->hasNext(&value)){
             KeyFileGroup *group = (KeyFileGroup *)value;
             if (group && group->name && strcmp (group->name, groupName) == 0)
                 return iter->getPrevNode();
         }
         return NULL;
     }

    static char *g_key_file_parse_value_as_comment (char *value,aboolean is_final_line){
      char **lines;
      asize i;
      AString *string = new$ AString(512);
      lines = a_strsplit (value, "\n", 0);
      for (i = 0; lines[i] != NULL; i++){
          const char *line = lines[i];
          if (i != 0)
            string->append('\n');

          if (line[0] == '#')
            line++;
          string->append (line);
      }
      a_strfreev (lines);

      /* This function gets called once per line of a comment, but we don’t want
       * to add a trailing newline. */
      if (!is_final_line){
          printf("加一个换行符---- %p %s\n",string,string->getString());
          string->append('\n');
      }
      return string->unrefStr();
    }

    static char *getKeyComment(const char *groupName,const char *key,AError **error){
      KeyFileGroup *group;
      KeyFileKeyValuePair *pair;
      ListNode *key_node, *tmp;
      AString *string=NULL;
      char *comment;
      a_return_val_if_fail (KeyFileUtil.isGroupName(groupName), NULL);
      group= g_key_file_lookup_group(groupName);
      if (!group){
          a_error_printf(error, ERROR_DOMAIN, KeyFileError.GROUP_NOT_FOUND,"Key file does not have group “%s”",groupName ? groupName:"(null)");
          return NULL;
      }
      key_node = g_key_file_lookup_key_value_pair_node (group, key);
      if (key_node == NULL){
          set_not_found_key_error (group->name, key, error);
          return NULL;
      }
      tmp =AList.prev(key_node);
      while(tmp){
          KeyFileKeyValuePair *pair = (KeyFileKeyValuePair *)AList.nodeData(tmp);
          if(!pair->key){
              printf("这是注释--keyxxx- %s\n",pair->value);
              if (string == NULL)
                string = new$ AString(512);
              comment = g_key_file_parse_value_as_comment (pair->value,FALSE);
              string->insert (0,comment);
              a_free (comment);
          }else{
              break;
          }
          tmp =AList.prev(tmp);
      }

      if (string != NULL){
         string->erase(string->length()-1,-1);//去除最后一个换行符
         return string->unrefStr();
      }
      return NULL;
    }


    static char *getGroupComment (KeyFileGroup  *group,AError **error){
          char *comment;
          AString *string = NULL;
          ListIterator *iter=group->keyValuePairList->initIter();
          apointer value=NULL;
          while(iter->hasNext(&value)){
              KeyFileKeyValuePair *pair = (KeyFileKeyValuePair *)value;
              if(!pair->key){
                  printf("这是注释--- %s\n",pair->value);
                  if (string == NULL)
                    string = new$ AString(512);
                  comment = g_key_file_parse_value_as_comment (pair->value,FALSE);
                  string->append (comment);
                  a_free (comment);
              }else{
                  break;
              }
          }
          if (string != NULL){
             string->erase(string->length()-1,-1);//去除最后一个换行符
             return string->unrefStr();
          }
          return NULL;
    }


    char *getComment(const char *groupName,const char *key,AError **error){
          if (groupName != NULL && key != NULL)
            return getKeyComment(groupName, key, error);
          else if (groupName != NULL){
             KeyFileGroup *group = g_key_file_lookup_group (groupName);
             if (!group){
                 a_error_printf(error, ERROR_DOMAIN, KeyFileError.GROUP_NOT_FOUND,"Key file does not have group “%s”",groupName ? groupName:"(null)");
                 return NULL;
             }
             return getGroupComment (group, error);
          }else{
             KeyFileGroup *group = (KeyFileGroup *) groups->getFirst();
             return getGroupComment (group, error);
          }
    }



    static char *g_key_file_parse_comment_as_value (const char   *comment){
      char **lines;
      asize i;
      AString *string = new$ AString(512);
      lines = a_strsplit (comment, "\n", 0);
      for (i = 0; lines[i] != NULL; i++)
          string->appendPrintf("#%s%s", lines[i],lines[i + 1] == NULL? "" : "\n");
      a_strfreev (lines);
      return string->unrefStr();
    }


    static void  g_key_file_remove_key_value_pair_node (KeyFileGroup *group,ListNode *pairNode){
      KeyFileKeyValuePair *pair;
      pair = (KeyFileKeyValuePair *)AList.nodeData(pairNode);
      printf("移走前的大小:%d\n",group->keyValuePairList->length());
      group->keyValuePairList->removeNode(pairNode);
      printf("移走后的大小:%d\n",group->keyValuePairList->length());
      a_warn_if_fail (pair->value != NULL);
      g_key_file_key_value_pair_free (pair);
      AList.nodeFree(pairNode);
    }

    aboolean   setKeyComment(const char *groupName, const char *key,const char *comment,AError **error){
      KeyFileGroup *group;
      KeyFileKeyValuePair *pair;
      ListNode *key_node, *comment_node, *tmp;

      group = g_key_file_lookup_group (groupName);
      if (!group){
          a_error_printf(error, ERROR_DOMAIN, KeyFileError.GROUP_NOT_FOUND,"Key file does not have group “%s”",groupName ? groupName:"(null)");
         return FALSE;
      }

     /* 首先找到注释应该与之关联的键*/
      key_node = g_key_file_lookup_key_value_pair_node (group, key);
      if (key_node == NULL){
          set_not_found_key_error (group->name, key, error);
          return FALSE;
      }

      tmp =AList.prev(key_node);
      while(tmp){
           pair = (KeyFileKeyValuePair *)AList.nodeData(tmp);
           if(!pair->key){
               printf("移走的 这是注释--keyxxx- %s\n",pair->value);
               comment_node = tmp;
               tmp =AList.prev(tmp);
               g_key_file_remove_key_value_pair_node (group,comment_node);
           }else{
               break;
           }
       }

      if (comment == NULL)
        return TRUE;

      /* 现在可以加入新的注释了*/
      pair = a_slice_new (KeyFileKeyValuePair);
      pair->key = NULL;
      pair->value = g_key_file_parse_comment_as_value (comment);
      group->keyValuePairList->insert(pair,key_node);
      return TRUE;
    }



    static aboolean setComment(KeyFileGroup *group,const char  *comment,AError **error) {
          int i;
          int length=group->keyValuePairList->length();
          for(i=0;i<group->keyValuePairList->length();i++){
              KeyFileKeyValuePair *pair = (KeyFileKeyValuePair *)group->keyValuePairList->get(i);
              if(!pair->key){
                  group->keyValuePairList->remove(pair);
                  g_key_file_key_value_pair_free (pair);
                  i--;
                  continue;
              }
              break;
          }
          if(comment!=NULL){
              KeyFileKeyValuePair *pair = a_slice_new (KeyFileKeyValuePair);
              pair->key = NULL;
              pair->value = g_key_file_parse_comment_as_value (comment);
              group->keyValuePairList->addFirst(pair);
          }
          return TRUE;
    }

    /**
     * 移走注释
     */
    aboolean removeComment(const char *groupName,const char *key,AError **error){
       if (groupName != NULL && key != NULL)
          return setKeyComment (groupName, key, NULL, error);
       else if (groupName != NULL){
         a_return_val_if_fail (KeyFileUtil.isGroupName(groupName), FALSE);
         KeyFileGroup *group = g_key_file_lookup_group (groupName);
         if (!group){
             a_error_printf(error, ERROR_DOMAIN, KeyFileError.GROUP_NOT_FOUND,"Key file does not have group “%s”",groupName ? groupName:"(null)");
             return FALSE;
         }
         return setComment(group,NULL,error);
       }else{
         KeyFileGroup *group = (KeyFileGroup *)groups->getFirst();
         if(group==NULL)
             return FALSE;
         return setComment(group,NULL,error);
       }
    }

    /**
     * 设新注释
     */
    aboolean  setComment(const char *groupName,const char *key,const char  *comment,AError **error){
        if (groupName != NULL && key != NULL)
           return setKeyComment (groupName, key, comment, error);
        else if (groupName != NULL){
          a_return_val_if_fail (KeyFileUtil.isGroupName(groupName), FALSE);
          KeyFileGroup *group = g_key_file_lookup_group (groupName);
          if (!group){
              a_error_printf(error, ERROR_DOMAIN, KeyFileError.GROUP_NOT_FOUND,"Key file does not have group “%s”",groupName ? groupName:"(null)");
              return FALSE;
           }
           return setComment(group,comment,error);
        }else{
           KeyFileGroup *group = (KeyFileGroup *)groups->getFirst();
           if(group==NULL)
              return FALSE;
           return setComment(group,comment,error);
        }
    }
    /**
     * 从文件中读取key value
     */
    static aboolean loadFromFile(int fd,KeyFileFlags  flags,AError **error){
         AError *curError = NULL;
         assize bytes_read;
         struct stat stat_buf;
         char read_buf[4096];
         char old_separator;
         if (fstat (fd, &stat_buf) < 0){
             int errsv = errno;
             a_error_set(error, ERROR_DOMAIN,errsv,AString.errToStr(errsv));
             return FALSE;
         }

         if (!S_ISREG (stat_buf.st_mode)){
             a_error_set (error, ERROR_DOMAIN,-1,"不是正规文件。");
             return FALSE;
         }

         old_separator = self->list_separator;
         clear();
         init();
         self->list_separator = old_separator;
         self->flags = flags;
         do{
             int errsv;
             bytes_read = read(fd, read_buf, 4096);
             errsv = errno;
             if (bytes_read == 0)  /* End of File */
               break;
             if (bytes_read < 0){
                 if (errsv == EINTR || errsv == EAGAIN)
                   continue;
                 a_error_set(error, ERROR_DOMAIN,errsv,AString.errToStr(errsv));
                 return FALSE;
             }
             g_key_file_parse_data (read_buf, bytes_read,&curError);
         }while (!curError);
         if (curError){
             a_error_transfer(error, curError);
             return FALSE;
         }

         g_key_file_flush_parse_buffer (&curError);
         if (curError){
             a_error_transfer (error, curError);
             return FALSE;
         }
         return TRUE;
    }

    static int g_key_file_parse_value_as_integer (const char *value,AError **error){
      char *eof_int;
      long long_value;
      int int_value;
      int errsv;
      errno = 0;
      long_value = strtol (value, &eof_int, 10);
      errsv = errno;
      if (*value == '\0' || (*eof_int != '\0' && !a_ascii_isspace(*eof_int))){
          char *value_utf8 = a_utf8_make_valid (value, -1);
          a_error_printf(error, ERROR_DOMAIN, KeyFileError.INVALID_VALUE,"Value “%s” cannot be interpreted as a number.", value_utf8);
          a_free (value_utf8);
          return 0;
      }

      int_value = long_value;
      if (int_value != long_value || errsv == ERANGE){
          char *value_utf8 = a_utf8_make_valid (value, -1);
          a_error_printf(error, ERROR_DOMAIN, KeyFileError.INVALID_VALUE,"Integer value “%s” out of range.", value_utf8);
          a_free (value_utf8);
          return 0;
      }
      return int_value;
    }

    /**
     * 是否有数据
     */
    aboolean isEmpty(){
        return groups->length()==0;
    }

    /**
     * 返回组数量
     */
    auint  getGroupCount(){
        return groups->length();
    }

    /**
     * 第一个组的组名是不是指定的groupName
     */
    aboolean isFirstGroup(const char *groupName){
        if(groups->length()==0)
            return FALSE;
        KeyFileGroup *gp=(KeyFileGroup *)groups->get(0);
        return (gp->name && strcmp(gp->name,groupName)==0);
    }

    /**
     * 把关键字对应的值转化成整形。并返回整形
     */
    int  getInt(const char  *groupName,const char *key,AError **error){
      AError *curErr=NULL;
      char *value;
      int int_value;
      a_return_val_if_fail (groupName != NULL, -1);
      a_return_val_if_fail (key != NULL, -1);
      value = getValue(groupName, key, &curErr);
      if (curErr){
          a_error_transfer(error, curErr);
          return 0;
      }
      int_value = g_key_file_parse_value_as_integer (value, &curErr);
      if (curErr){
          if (a_error_matches (curErr,ERROR_DOMAIN,KeyFileError.INVALID_VALUE)){
              a_error_printf(error, ERROR_DOMAIN,KeyFileError.INVALID_VALUE,
                           "Key file contains key “%s” in group “%s” which has a value that cannot be interpreted.",
                             key, groupName);
              a_error_free (curErr);
          }else
            a_error_transfer(error, curErr);
      }
      return int_value;
    }

   char **getGroups(asize *length){
          int num_groups = groups->length();
          a_return_val_if_fail (num_groups > 0, NULL);
          char **groupArray = a_new (char *, num_groups+1);
          ListIterator *iter=groups->initIter();
          apointer value=NULL;
          int i=0;
          while(iter->hasNext(&value)){
              KeyFileGroup *group=(KeyFileGroup *)value;
              a_warn_if_fail (group->name != NULL);
              groupArray[i++] = a_strdup (group->name);
          }
          groupArray[i] = NULL;
          if (length)
            *length = i;
          return groupArray;
    }

   aboolean removeGroup(const char *groupName,AError  **error){
     a_return_val_if_fail (groupName != NULL, FALSE);
     KeyFileGroup *group=g_key_file_lookup_group (groupName);
     if (!group){
       a_error_printf(error, ERROR_DOMAIN, KeyFileError.GROUP_NOT_FOUND,"Key file does not have group “%s”",groupName);
       return FALSE;
     }
     if (group->name){
        apointer value=groupHash->get(group->name);
        if(value!=NULL){
           auint count=APOINTER_TO_UINT(value);
           if(count==1){
             groupHash->remove(group->name);
           }else{
             groupHash->put(a_strdup(group->name), AUINT_TO_POINTER(count-1));
           }
        }
     }

     if (currentGroup == group){
         if (groups->length()>1){
             KeyFileGroup *item = (KeyFileGroup *) groups->getLast();
             if(item==currentGroup){
                 currentGroup=(KeyFileGroup *) groups->getFirst();
             }else{
                 currentGroup=item;
             }
         }else
             currentGroup = NULL;
     }

     if(startGroup == group){
        if (groups->length()>1){
             KeyFileGroup *item = (KeyFileGroup *) groups->getFirst();
             if(item==startGroup){
                 startGroup=(KeyFileGroup *) groups->get(1);
             }else{
                 startGroup=item;
             }
         }else
             startGroup = NULL;
     }
     groups->remove(group);
     freeKeyFileGroup_cb(group);
     return TRUE;
   }

//    aboolean removeGroup(const char *groupName,AError  **error){
//        ListNode *groupNode;
//        a_return_val_if_fail (groupName != NULL, FALSE);
//        groupNode = g_key_file_lookup_group_node (groupName);
//        if (!groupNode){
//            a_error_printf(error, ERROR_DOMAIN, KeyFileError.GROUP_NOT_FOUND,"Key file does not have group “%s”",groupName);
//            return FALSE;
//        }
//        g_key_file_remove_group_node (groupNode);
//        AList.nodeFree(groupNode);
//        return TRUE;
//    }


    AKeyFile(const char *file,AError **error){
           int fd;
           int errsv;
           if(file==NULL){
               a_error_set(error, ERROR_DOMAIN,-1,"文件名是空的。");
               return;
           }
           a_return_val_if_fail (file != NULL, NULL);
           fd = open (file, O_RDONLY, 0);
           errsv = errno;
           if (fd == -1){
               a_warning("文件打不开。%s %s\n",file,AString.errToStr(errsv));
               a_error_set (error, ERROR_DOMAIN,errsv,AString.errToStr(errsv));
               return;
           }
           AError *kerr=NULL;
           repeatGroup=TRUE;
           loadFromFile(fd, KeyFileFlags.FILE_NONE, &kerr);
           close (fd);
           if (kerr){
               a_error_transfer(error, kerr);
               return ;
           }
     }

     AKeyFile(const char *data,asize length,KeyFileFlags flags,AError **error){
           AError *curErrr = NULL;
           if(data==NULL)
               return;
           if (length == (asize)-1)
             length = strlen (data);
           repeatGroup=TRUE;
           clear();
           init();
           self->flags = flags;
           g_key_file_parse_data (data, length, &curErrr);
           if (curErrr){
               a_error_transfer(error, curErrr);
               return;
           }
           g_key_file_flush_parse_buffer (&curErrr);
           if (curErrr){
               a_error_transfer (error, curErrr);
               return FALSE;
           }
     }



     ~AKeyFile(){
        clear();
      }


};
