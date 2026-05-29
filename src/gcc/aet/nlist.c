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


#include "nlist.h"
#include "nslice.h"
#include "nmem.h"
#include "nlog.h"


#define _n_list_alloc()         n_slice_new (NList)
#define _n_list_alloc0()        n_slice_new0 (NList)
#define _n_list_free1(list)     n_slice_free (NList, list)


NList *n_list_alloc (void)
{
  return _n_list_alloc0 ();
}


void n_list_free (NList *list)
{
  n_slice_free_chain (NList, list, next);
}


void n_list_free_1 (NList *list)
{
  _n_list_free1 (list);
}

void n_list_free_full (NList          *list,NDestroyNotify  free_func)
{
  n_list_foreach (list, (NFunc) free_func, NULL);
  n_list_free (list);
}

NList *n_list_append (NList    *list,               npointer  data)
{
  NList *new_list;
  NList *last;
  
  new_list = _n_list_alloc ();
  new_list->data = data;
  new_list->next = NULL;
  
  if (list)
    {
      last = n_list_last (list);
      /* n_assert (last != NULL); */
      last->next = new_list;
      new_list->prev = last;

      return list;
    }
  else
    {
      new_list->prev = NULL;
      return new_list;
    }
}


NList *n_list_prepend (NList    *list,                npointer  data)
{
  NList *new_list;
  
  new_list = _n_list_alloc ();
  new_list->data = data;
  new_list->next = list;
  
  if (list)
    {
      new_list->prev = list->prev;
      if (list->prev)
        list->prev->next = new_list;
      list->prev = new_list;
    }
  else
    new_list->prev = NULL;
  
  return new_list;
}


NList *n_list_insert (NList    *list,
               npointer  data,
               nint      position)
{
  NList *new_list;
  NList *tmp_list;

  if (position < 0)
    return n_list_append (list, data);
  else if (position == 0)
    return n_list_prepend (list, data);

  tmp_list = n_list_nth (list, position);
  if (!tmp_list)
    return n_list_append (list, data);

  new_list = _n_list_alloc ();
  new_list->data = data;
  new_list->prev = tmp_list->prev;
  tmp_list->prev->next = new_list;
  new_list->next = tmp_list;
  tmp_list->prev = new_list;

  return list;
}


NList *n_list_insert_before_link (NList *list,NList *sibling,NList *link_)
{
  n_return_val_if_fail (link_ != NULL, list);
  n_return_val_if_fail (link_->prev == NULL, list);
  n_return_val_if_fail (link_->next == NULL, list);

  if (list == NULL)
    {
      n_return_val_if_fail (sibling == NULL, list);
      return link_;
    }
  else if (sibling != NULL)
    {
      link_->prev = sibling->prev;
      link_->next = sibling;
      sibling->prev = link_;
      if (link_->prev != NULL)
        {
          link_->prev->next = link_;
          return list;
        }
      else
        {
          n_return_val_if_fail (sibling == list, link_);
          return link_;
        }
    }
  else
    {
      NList *last;

      for (last = list; last->next != NULL; last = last->next) {}

      last->next = link_;
      last->next->prev = last;
      last->next->next = NULL;

      return list;
    }
}


NList *n_list_insert_before (NList    *list,NList    *sibling,npointer  data)
{
  if (list == NULL)
    {
      list = n_list_alloc ();
      list->data = data;
      n_return_val_if_fail (sibling == NULL, list);
      return list;
    }
  else if (sibling != NULL)
    {
      NList *node;

      node = _n_list_alloc ();
      node->data = data;
      node->prev = sibling->prev;
      node->next = sibling;
      sibling->prev = node;
      if (node->prev != NULL)
        {
          node->prev->next = node;
          return list;
        }
      else
        {
          n_return_val_if_fail (sibling == list, node);
          return node;
        }
    }
  else
    {
      NList *last;

      for (last = list; last->next != NULL; last = last->next) {}

      last->next = _n_list_alloc ();
      last->next->data = data;
      last->next->prev = last;
      last->next->next = NULL;

      return list;
    }
}


NList *n_list_concat (NList *list1,        NList *list2)
{
  NList *tmp_list;
  
  if (list2)
    {
      tmp_list = n_list_last (list1);
      if (tmp_list)
        tmp_list->next = list2;
      else
        list1 = list2;
      list2->prev = tmp_list;
    }
  
  return list1;
}

static inline NList *_n_list_remove_link (NList *list,NList *link)
{
  if (link == NULL)
    return list;

  if (link->prev)
    {
      if (link->prev->next == link)
        link->prev->next = link->next;
      else
        n_warning ("corrupted double-linked list detected");
    }
  if (link->next)
    {
      if (link->next->prev == link)
        link->next->prev = link->prev;
      else
        n_warning ("corrupted double-linked list detected");
    }

  if (link == list)
    list = list->next;

  link->next = NULL;
  link->prev = NULL;

  return list;
}


NList *n_list_remove (NList         *list,               nconstpointer  data)
{
  NList *tmp;

  tmp = list;
  while (tmp)
    {
      if (tmp->data != data)
        tmp = tmp->next;
      else
        {
          list = _n_list_remove_link (list, tmp);
          _n_list_free1 (tmp);

          break;
        }
    }
  return list;
}


NList *n_list_remove_all (NList         *list,
                   nconstpointer  data)
{
  NList *tmp = list;

  while (tmp)
    {
      if (tmp->data != data)
        tmp = tmp->next;
      else
        {
          NList *next = tmp->next;

          if (tmp->prev)
            tmp->prev->next = next;
          else
            list = next;
          if (next)
            next->prev = tmp->prev;

          _n_list_free1 (tmp);
          tmp = next;
        }
    }
  return list;
}


NList *n_list_remove_link (NList *list,NList *llink)
{
  return _n_list_remove_link (list, llink);
}


NList *n_list_delete_link (NList *list,
                    NList *link_)
{
  list = _n_list_remove_link (list, link_);
  _n_list_free1 (link_);

  return list;
}


NList *n_list_copy (NList *list)
{
  return n_list_copy_deep (list, NULL, NULL);
}


NList *n_list_copy_deep (NList     *list,
                  NCopyFunc  func,
                  npointer   user_data)
{
  NList *new_list = NULL;

  if (list)
    {
      NList *last;

      new_list = _n_list_alloc ();
      if (func)
        new_list->data = func (list->data, user_data);
      else
        new_list->data = list->data;
      new_list->prev = NULL;
      last = new_list;
      list = list->next;
      while (list)
        {
          last->next = _n_list_alloc ();
          last->next->prev = last;
          last = last->next;
          if (func)
            last->data = func (list->data, user_data);
          else
            last->data = list->data;
          list = list->next;
        }
      last->next = NULL;
    }

  return new_list;
}


NList *n_list_reverse (NList *list)
{
  NList *last;
  
  last = NULL;
  while (list)
    {
      last = list;
      list = last->next;
      last->next = last->prev;
      last->prev = list;
    }
  
  return last;
}


NList *n_list_nth (NList *list,
            nuint  n)
{
  while ((n-- > 0) && list)
    list = list->next;
  
  return list;
}


NList *n_list_nth_prev (NList *list,
                 nuint  n)
{
  while ((n-- > 0) && list)
    list = list->prev;
  
  return list;
}


npointer n_list_nth_data (NList *list,
                 nuint  n)
{
  while ((n-- > 0) && list)
    list = list->next;
  
  return list ? list->data : NULL;
}


NList *n_list_find (NList         *list,
             nconstpointer  data)
{
  while (list)
    {
      if (list->data == data)
        break;
      list = list->next;
    }
  
  return list;
}

NList *n_list_find_custom (NList         *list,
                    nconstpointer  data,
                    NCompareFunc   func)
{
  n_return_val_if_fail (func != NULL, list);

  while (list)
    {
      if (! func (list->data, data))
        return list;
      list = list->next;
    }

  return NULL;
}


nint n_list_position (NList *list,  NList *llink)
{
  nint i;

  i = 0;
  while (list)
    {
      if (list == llink)
        return i;
      i++;
      list = list->next;
    }

  return -1;
}


nint n_list_index (NList         *list, nconstpointer  data)
{
  nint i;

  i = 0;
  while (list)
    {
      if (list->data == data)
        return i;
      i++;
      list = list->next;
    }

  return -1;
}


NList *n_list_last (NList *list)
{
  if (list)
    {
      while (list->next)
        list = list->next;
    }
  
  return list;
}


NList *n_list_first (NList *list)
{
  if (list)
    {
      while (list->prev)
        list = list->prev;
    }
  
  return list;
}


nuint n_list_length (NList *list)
{
  nuint length;
  
  length = 0;
  while (list)
    {
      length++;
      list = list->next;
    }
  
  return length;
}


void n_list_foreach (NList    *list,
                NFunc     func,
                npointer  user_data)
{
  while (list)
    {
      NList *next = list->next;
      (*func) (list->data, user_data);
      list = next;
    }
}

static NList *n_list_insert_sorted_real (NList    *list,
                           npointer  data,
                           NFunc     func,
                           npointer  user_data)
{
  NList *tmp_list = list;
  NList *new_list;
  nint cmp;

  n_return_val_if_fail (func != NULL, list);
  
  if (!list) 
    {
      new_list = _n_list_alloc0 ();
      new_list->data = data;
      return new_list;
    }
  
  cmp = ((NCompareDataFunc) func) (data, tmp_list->data, user_data);

  while ((tmp_list->next) && (cmp > 0))
    {
      tmp_list = tmp_list->next;

      cmp = ((NCompareDataFunc) func) (data, tmp_list->data, user_data);
    }

  new_list = _n_list_alloc0 ();
  new_list->data = data;

  if ((!tmp_list->next) && (cmp > 0))
    {
      tmp_list->next = new_list;
      new_list->prev = tmp_list;
      return list;
    }
   
  if (tmp_list->prev)
    {
      tmp_list->prev->next = new_list;
      new_list->prev = tmp_list->prev;
    }
  new_list->next = tmp_list;
  tmp_list->prev = new_list;
 
  if (tmp_list == list)
    return new_list;
  else
    return list;
}


NList *n_list_insert_sorted (NList        *list,
                      npointer      data,
                      NCompareFunc  func)
{
  return n_list_insert_sorted_real (list, data, (NFunc) func, NULL);
}


NList *n_list_insert_sorted_with_data (NList *list,
                                npointer          data,
                                NCompareDataFunc  func,
                                npointer          user_data)
{
  return n_list_insert_sorted_real (list, data, (NFunc) func, user_data);
}

static NList *n_list_sort_merge (NList  *l1,NList     *l2, NFunc     compare_func,npointer  user_data)
{
  NList list, *l, *lprev;
  nint cmp;
  l = &list; 
  lprev = NULL;

  while (l1 && l2)
    {
      cmp = ((NCompareDataFunc) compare_func) (l1->data, l2->data, user_data);

      if (cmp <= 0)
        {
          l->next = l1;
          l1 = l1->next;
        } 
      else 
        {
          l->next = l2;
          l2 = l2->next;
        }
      l = l->next;
      l->prev = lprev; 
      lprev = l;
    }
  l->next = l1 ? l1 : l2;
  l->next->prev = l;

  return list.next;
}

static NList * n_list_sort_real (NList    *list, NFunc     compare_func,npointer  user_data)
{
  NList *l1, *l2;
  
  if (!list) 
    return NULL;
  if (!list->next) 
    return list;
  
  l1 = list; 
  l2 = list->next;

  while ((l2 = l2->next) != NULL)
    {
      if ((l2 = l2->next) == NULL) 
        break;
      l1 = l1->next;
    }
  l2 = l1->next; 
  l1->next = NULL; 

  return n_list_sort_merge (n_list_sort_real (list, compare_func, user_data),
                            n_list_sort_real (l2, compare_func, user_data),
                            compare_func,
                            user_data);
}

NList *n_list_sort (NList        *list,NCompareFunc  compare_func)
{
  return n_list_sort_real (list, (NFunc) compare_func, NULL);
}

NList * n_list_sort_with_data (NList            *list,NCompareDataFunc  compare_func,npointer          user_data)
{
  return n_list_sort_real (list, (NFunc) compare_func, user_data);
}

/**
 * n_clear_list: (skip)
 * @list_ptr: (not nullable): a #NList return location
 * @destroy: (nullable): the function to pass to n_list_free_full() or %NULL to not free elements
 *
 * Clears a pointer to a #NList, freeing it and, optionally, freeing its elements using @destroy.
 *
 * @list_ptr must be a valid pointer. If @list_ptr points to a null #NList, this does nothing.
 *
 * Since: 2.64
 */
void (n_clear_list) (NList   **list_ptr,  NDestroyNotify   destroy)
{
  NList *list;

  list = *list_ptr;
  if (list)
    {
      *list_ptr = NULL;

      if (destroy)
        n_list_free_full (list, destroy);
      else
        n_list_free (list);
    }
}
