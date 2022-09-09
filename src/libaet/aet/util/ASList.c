#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "ASList.h"



static inline SListNode *createNode(apointer data, SListNode *next)
{
    SListNode *node=a_slice_new(SListNode);
    node->data=data;
    node->next=next;
    return node;
}

static inline void freeNode(SListNode *node)
{
    node->data=NULL;
    node->next=NULL;
    a_slice_free(SListNode,node);
}

impl$ ASList{



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

  SListNode *getLastNode(){
      SListNode *node=head;
       if (node){
          while (node->next)
            node = node->next;
       }
       return node;
  }


	  /**
	   * 加在最后
	   * */
  void add(apointer value) {
      SListNode *last;
      SListNode *newNode = createNode(value,NULL);
      last = getLastNode();
      if (last){
        last->next = newNode;
      }else
        head= newNode;
      size++;
  }



  void addFirst(apointer value) {
     SListNode *node = createNode(value,NULL);   //节点对象
     node->next =head;
     head = node;
     size++;
  }

  void insert(int  position,apointer  data){
    if (position < 0)
      add(data);
    else if (position == 0)
      addFirst (data);
    else{
        SListNode *newList = createNode(data,NULL);   //节点对象
        SListNode *list=head;
        if (!list){
           newList->next = NULL;
           head=newList;
           size++;
           return;
        }
        SListNode *prev= NULL;
        SListNode *tmp = list;
        while ((position-- > 0) && tmp){
            prev = tmp;
            tmp = tmp->next;
        }
        newList->next = prev->next;
        prev->next = newList;
        size++;
    }
  }

  auint length() {
     return size;
  }

  static aboolean removeImpl(aconstpointer  data,aboolean all,int *count){
      SListNode *tmp = NULL;
      SListNode *tempHead=head;
      SListNode **previous_ptr = &tempHead;
      aboolean ok=FALSE;
      int removeCount=0;
      while (*previous_ptr){
        tmp = *previous_ptr;
        if (tmp->data == data){
            *previous_ptr = tmp->next;
            freeNode (tmp);
            size--;
            ok=TRUE;
            removeCount++;
            if (!all)
              break;
        }else{
            previous_ptr = &tmp->next;
        }
    }
    head=tempHead;
    if(count)
      *count=removeCount;
    return ok;
  }

  auint  removeAll(aconstpointer  data){
      int count=0;
      removeImpl(data,TRUE,&count);
      return count;
  }

  aboolean remove(aconstpointer data) {
      return removeImpl(data,FALSE,NULL);
  }

  static inline void removeNode(SListNode *link){
      SListNode *tempHead=head;
      SListNode *tmp = NULL;
      SListNode **previous_ptr = &tempHead;
      while (*previous_ptr){
          tmp = *previous_ptr;
          if (tmp == link){
              *previous_ptr = tmp->next;
              tmp->next = NULL;
              freeNode (tmp);
              size--;
              break;
          }
          previous_ptr = &tmp->next;
      }
      head=tempHead;
  }


  void reverse (){
    SListNode *prev = NULL;
    SListNode *list=head;
     while (list){
        SListNode *next = list->next;
        list->next = prev;
        prev = list;
        list = next;
     }
     head=prev;
  }

  apointer getFirst() {
      if (head == NULL)
          return NULL;
      return head->data;
  }

  apointer getLast() {
      if(head==NULL)
          return NULL;
      SListNode *last = head;
      while (last->next)
        last = last->next;
      return last->data;
  }

  apointer get(int n) {
      checkElementIndex(n);
      if(head==NULL)
          return NULL;
      SListNode *list = head;
      while (n-- > 0 && list)
        list = list->next;
      return list ? list->data : NULL;
  }

  int indexOf(apointer data) {
      int i=0;
      SListNode *list = head;
      while (list){
          if (list->data == data)
            return i;
          i++;
          list = list->next;
      }
      return -1;
  }

  void foreach (AFunc func,apointer  userData){
       SListNode *node=head;
       while (node){
           SListNode *next = node->next;
          (*func) (node->data, userData);
          node = next;
       }
  }

  SListNode *sortMerge(SListNode *l1,SListNode *l2,AFunc compareFunc,apointer  userData){
      SListNode list, *l;
      int cmp;
      l=&list;
      while (l1 && l2){
        cmp = ((ACompareDataFunc) compareFunc) (l1->data, l2->data, userData);
        if (cmp <= 0){
            l=l->next=l1;
            l1=l1->next;
        }else{
            l=l->next=l2;
            l2=l2->next;
        }
      }
      l->next= l1 ? l1 : l2;
      return list.next;
  }

  SListNode *sortReal(SListNode *list,AFunc compareFunc,apointer  userData){
      SListNode *l1, *l2;
      if (!list)
        return NULL;
      if (!list->next)
        return list;
      l1 = list;
      l2 = list->next;
      while ((l2 = l2->next) != NULL){
        if ((l2 = l2->next) == NULL)
            break;
        l1=l1->next;
      }
      l2 = l1->next;
      l1->next = NULL;
      return sortMerge (sortReal(list, compareFunc, userData),sortReal (l2, compareFunc, userData),compareFunc,userData);
  }

  void  sort(ACompareFunc compareFunc){
      head=sortReal (head, (AFunc) compareFunc, NULL);
  }

  void  sort(ACompareDataFunc compareFunc,apointer userData){
      head=sortReal (head, (AFunc) compareFunc, userData);
  }

  static SListNode* insertSortedReal(SListNode *list,apointer  data,AFunc func,apointer  user_data){
      SListNode *tmp_list = list;
      SListNode *prev_list = NULL;
      SListNode *new_list;
      int cmp;
      a_return_val_if_fail (func != NULL, list);
      size++;
      if (!list){
        new_list = createNode(data,NULL);
        return new_list;
      }

      cmp = ((ACompareDataFunc) func) (data, tmp_list->data, user_data);
      while ((tmp_list->next) && (cmp > 0)){
          prev_list = tmp_list;
          tmp_list = tmp_list->next;
          cmp = ((ACompareDataFunc) func) (data, tmp_list->data, user_data);
      }

      new_list = createNode(data,NULL);

      if ((!tmp_list->next) && (cmp > 0)){
         tmp_list->next = new_list;
         new_list->next = NULL;
         return list;
      }

      if (prev_list){
        prev_list->next = new_list;
        new_list->next = tmp_list;
        return list;
      }else{
        new_list->next = list;
        return new_list;
      }
  }

  void insertSorted(apointer data,ACompareFunc  func){
      printf("insertSorted ----\n");
     head=insertSortedReal(head,data,(AFunc)func,NULL);
  }

  void insertSorted(apointer data,ACompareDataFunc  func,apointer userData){
      head=insertSortedReal(head,data,(AFunc)func,userData);
  }

  aboolean find(aconstpointer data){
      SListNode *list=head;
      while (list){
        if (list->data == data)
            return TRUE;
        list = list->next;
      }
      return FALSE;
  }

  aboolean find(aconstpointer  data,ACompareFunc func){
    a_return_val_if_fail (func != NULL, FALSE);
    SListNode *list=head;
    while (list){
        if (!func(list->data, data))
          return TRUE;
        list = list->next;
    }
    return FALSE;
  }

   void     setDestroyFunc(ADestroyNotify func){
       destoryItemFunc=func;
   }


  ASList(ADestroyNotify fun){
    size=0;
    head=NULL;
    destoryItemFunc=fun;
  }

  ASList(){
    self(NULL);
  }

  ~ASList(){
	 if(destoryItemFunc!=NULL)
	    foreach((AFunc)destoryItemFunc, NULL);
	 SListNode * x=NULL;
     for (x = head; x != NULL; x = x->next) {
		 freeNode(x);
     }
  }


};
