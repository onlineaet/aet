#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "AList.h"




static inline ListNode *createNode(ListNode *prev, apointer element, ListNode *next)
{
    ListNode *node=a_slice_new(ListNode);
    node->data = element;
    node->next = next;
    node->prev = prev;
    return node;
}

#define freeNode(node)     a_slice_free (ListNode, node)

impl$ ListIterator{
    ListIterator(){
        node=NULL;
    }

    aboolean  hasNext(apointer *value){
      ListNode *node=self->node;
      while (node){
          ListNode *next = node->next;
          *value=node->data;
          self->prevNode=node;
          self->node = next;
          return TRUE;
      }
      return FALSE;
    }

    ListNode *getNode(){
        return node;
    }

    ListNode *getPrevNode(){
        return prevNode;
    }
};

impl$ AList{

  static ListNode *node(int index) {
	  int i;
	  if (index < (size >> 1)) {
	      ListNode *x = first;
		  for (i = 0; i < index; i++)
			  x = x->next;
		  return x;
	  } else {
	      ListNode * x = last;
		  for (i = size - 1; i > index; i--)
			  x = x->prev;
		  return x;
	  }
  }

  static void linkLast(apointer e) {
        ListNode *l = last;
        ListNode *newNode = createNode(l, e, NULL);
        self->last = newNode;
        if (l == NULL)
            self->first = newNode;
        else
            l->next = newNode;
        size++;
  }

  static void linkFirst(apointer e) {
        ListNode *f = first;
        ListNode *newNode =createNode(NULL, e, f);
        first = newNode;
        if (f == NULL)
            last = newNode;
        else
            f->prev = newNode;
        size++;
  }

  void linkBefore(apointer e, ListNode *succ) {
      ListNode *pred = succ->prev;
      ListNode *newNode = createNode(pred, e, succ);
      succ->prev = newNode;
      if (pred == NULL)
          first = newNode;
      else
          pred->next = newNode;
      size++;
  }
  /**
   * Unlinks non-null node x.
   */
  static apointer delLink(ListNode *x) {
      // assert x != null;
      apointer element = x->data;
      ListNode *next = x->next;
      ListNode *prev = x->prev;
      if (prev == NULL) {
          first = next;
      } else {
          prev->next = next;
          x->prev = NULL;
      }

      if (next == NULL) {
          last = prev;
      } else {
          next->prev = prev;
          x->next = NULL;
      }

      x->data = NULL;
      size--;
      return element;
  }

  static aboolean isElementIndex(int index) {
      return index >= 0 && index < size;
  }

  static aboolean isPositionIndex(int index) {
       return index >= 0 && index <= size;
   }

  static void checkElementIndex(int index) {
      if (!isElementIndex(index))
          a_error("索引不在范围内,index:%d 当前List的大小：%d",index,size);
  }

  static void checkPositionIndex(int index) {
      if (!isPositionIndex(index))
          a_error("索引不在范围内,index:%d 当前List的大小：%d",index,size);
  }

  static ListNode* insertSortedReal(apointer  data,AFunc func,apointer  user_data){
      ListNode *list=first;
      ListNode *tmp_list = list;
      ListNode *new_list;
      aint cmp;
      a_return_val_if_fail (func != NULL, tmp_list);
      if (size==0){
         add(data);
         return first;
      }
      size++;
     cmp = ((ACompareDataFunc) func) (data, tmp_list->data, user_data);
     while ((tmp_list->next) && (cmp > 0)){
        tmp_list = tmp_list->next;
        cmp = ((ACompareDataFunc) func) (data, tmp_list->data, user_data);
     }

    new_list = createNode(NULL,data,NULL);
    if ((!tmp_list->next) && (cmp > 0)){
        tmp_list->next = new_list;
        new_list->prev = tmp_list;
        return list;
    }

    if (tmp_list->prev){
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


  /**
   * 后加的在后面
   */
  void add(apointer value) {
	  linkLast(value);
  }

  void addFirst(apointer value) {
	 linkFirst(value);
  }

  void addLast(apointer value) {
	 linkLast(value);
  }

  auint length() {
     return size;
  }

  aboolean remove(apointer data) {
	 ListNode * x=NULL;
     if (data == NULL) {
	    for (x = first; x != NULL; x = x->next) {
		   if (x->data == NULL) {
			  delLink(x);
			  freeNode(x);
			  return TRUE;
		   }
	    }
     }else{
	    for (x = first; x != NULL; x = x->next) {
		   if (data==x->data) {
			  delLink(x);
			  freeNode(x);
			  return TRUE;
		   }
	    }
     }
     return FALSE;
  }

  /**
   * 从listnode节点移走data
   */
  auint  removeAll(apointer data){
      ListNode *tmp = first;
      int count=0;
      ListNode *list;
      while (tmp){
        if (tmp->data != data)
           tmp = tmp->next;
        else{
            ListNode *next = tmp->next;
            if (tmp->prev)
              tmp->prev->next = next;
            if (next)
              next->prev = tmp->prev;
            delLink(tmp);
            freeNode(tmp);
            count++;
            tmp = next;
         }
      }
      return count;
  }

  apointer remove(int index) {
      checkElementIndex(index);
      if(index<=0)
          return removeFirst();
      if(index>=size)
          return removeLast();
      ListNode * x=node(index);
      apointer data=delLink(x);
      if(x)
          freeNode(x);
      return data;
  }

  apointer removeFirst(){
      ListNode * node=first;
      if(!node)
          return NULL;
      apointer data=delLink(node);
      freeNode(node);
      return data;
  }

  apointer removeLast(){
      ListNode * node=last;
      if(!node)
          return NULL;
      apointer data=delLink(node);
      freeNode(node);
      return data;
  }


  apointer getFirst() {
      if (first == NULL)
          return NULL;
      return first->data;
  }


  apointer getLast() {
      if (last == NULL)
          return NULL;
      return last->data;
  }

  apointer get(int index) {
      checkElementIndex(index);
      if(index<=0 )
          return first->data;
      if(index>=size-1)
          return last->data;
      return node(index)->data;
  }

  int indexOf(apointer data) {
      int index = 0;
      ListNode *x;
      if (data == NULL) {
          for (x = first; x != NULL; x = x->next) {
              if (x->data == NULL)
                  return index;
              index++;
          }
      } else {
          for (x = first; x != NULL; x = x->next) {
              if (data==x->data)
                  return index;
              index++;
          }
      }
      return -1;
  }

  void insert(int index, apointer element) {
	  if (index < 0)
	    return add (element);
      else if (index == 0)
        return addFirst (element);
      else if (index >=size)
          return addLast (element);
      linkBefore(element, node(index));
  }

  void  insertSorted(apointer data,ACompareFunc  func){
      insertSorted (data, (AFunc) func, NULL);
  }

  void foreach (AFunc func,apointer  user_data){
	 ListNode *node=first;
     while (node){
         ListNode *next = node->next;
        (*func) (node->data, user_data);
        node = next;
     }
  }


  void insertSorted(apointer data,ACompareDataFunc  func,apointer userData){
       self->first= insertSortedReal (data, (AFunc) func, userData);
       ListNode *node=self->first;
       while (node){
           ListNode *next = node->next;
           apointer value=node->data;
           if(!next){
                //printf("最后一个元素是:%p %p %p\n",node,first->next,first->prev);
                last=node;
           }
           node = next;
       }
  }

  static ListNode *next(ListNode *node){
      return node==NULL?NULL:node->next;
  }
  static ListNode *prev(ListNode *node){
      return node==NULL?NULL:node->prev;
  }

  static apointer  nodeData(ListNode *node){
      return node==NULL?NULL:node->data;
  }

  static void  nodeFree(ListNode *node){
      a_slice_free(ListNode,node);
  }

  static ListNode *last (ListNode *node){
    if (node){
        while (node->next)
            node = node->next;
    }
    return node;
  }


  aboolean removeNode(ListNode *node){
      ListNode *x;
      aboolean find=FALSE;
      for (x = first; x != NULL; x = x->next) {
         if (x==node){
             find=TRUE;
             break;
         }
      }
      if(find){
          delLink(node);
      }
      return find;
  }

  void clear(){
      ListNode * x=NULL;
      for (x = first; x != NULL; x = x->next) {
          delLink(x);
          freeNode(x);
      }
     first=last=NULL;
     size=0;
  }

  void  clear(ADestroyNotify func){
      if(func!=NULL)
         foreach((AFunc)func, NULL);
      clear();
  }

  void setDestroyFunc(ADestroyNotify fun){
      destoryItemFunc=fun;
  }

  ListIterator *initIter(){
      if(iter==NULL)
          iter=new$ ListIterator();
      iter->node=first;
      iter->prevNode=first;
      return iter;
  }

  ListNode *sortMerge(ListNode *l1,ListNode *l2,AFunc compareFunc,apointer  userData){
    ListNode list, *l,*lprev;
    int cmp;
    l = &list;
    lprev = NULL;
    while (l1 && l2){
        cmp = ((ACompareDataFunc)compareFunc)(l1->data, l2->data, userData);
        if (cmp <= 0){
            l->next = l1;
            l1 = l1->next;
        }else{
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

  ListNode *sortReal(ListNode *list,AFunc compareFunc,apointer  userData){
      ListNode *l1, *l2;
      if (!list)
        return NULL;
      if (!list->next)
        return list;
      l1 = list;
      l2 = list->next;
      while ((l2 = l2->next) != NULL){
        if ((l2 = l2->next) == NULL)
            break;
        l1 = l1->next;
      }
      l2 = l1->next;
      l1->next = NULL;
      return sortMerge (sortReal(list, compareFunc, userData),sortReal (l2, compareFunc, userData),compareFunc,userData);
  }


  void sort(ACompareFunc compareFunc){
       self->first= sortReal (self->first, (AFunc)compareFunc, NULL);
       ListNode *node=self->first;
       while (node){
           ListNode *next = node->next;
           apointer value=node->data;
           if(!next){
                last=node;
           }
           node = next;
       }
  }

  void sort(ACompareDataFunc compareFunc,apointer userData){
      self->first= sortReal (self->first, (AFunc)compareFunc, userData);
      ListNode *node=self->first;
      while (node){
          ListNode *next = node->next;
          apointer value=node->data;
          if(!next){
               last=node;
          }
          node = next;
      }
  }

  aboolean find(aconstpointer data){
      ListNode *list=first;
      while (list){
        if (list->data == data)
            return TRUE;
        list = list->next;
      }
      return FALSE;
  }

  aboolean find(aconstpointer  data,ACompareFunc func){
    a_return_val_if_fail (func != NULL, FALSE);
    ListNode *list=first;
    while (list){
        if (!func(list->data, data))
          return TRUE;
        list = list->next;
    }
    return FALSE;
  }

 void reverse (){
    ListNode *myLast;
    ListNode *list=first;
    ListNode *firstOld=first;
    ListNode *lastOld=last;

    myLast = NULL;
    while (list){
        myLast = list;
        list = myLast->next;
        myLast->next = myLast->prev;
        myLast->prev = list;
    }
    first=lastOld;
    last=firstOld;
  }



  AList(ADestroyNotify fun){
    size=0;
    first=NULL;
    last=NULL;
    destoryItemFunc=fun;
  }

  AList(){
    size=0;
    first=NULL;
    last=NULL;
    destoryItemFunc=NULL;
  }

  ~AList(){
	 if(destoryItemFunc!=NULL)
	    foreach((AFunc)destoryItemFunc, NULL);
	 ListNode * x=NULL;
     for (x = first; x != NULL; x = x->next) {
		 delLink(x);
		 freeNode(x);
     }
  }


};
