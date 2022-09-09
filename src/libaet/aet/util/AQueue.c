
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "AQueue.h"


impl$ AQueue{

    AQueue(){
        list =new$ AList();
        length = 0;
   }

    aboolean   isEmpty(){
      return length==0;
    }

    auint length (){
      return length;
    }

    /**
     * 先进先出。
     */
    void push(apointer  data){
        list->addLast(data);
        self->length++;
    }

    /**
     * 数据加前面
     */
    void pushHead(apointer  data){
        list->addFirst(data);
        self->length++;
    }

    void push(apointer data,int n){
        if (n < 0 || (auint) n >= length){
           push(data);
           return;
        }
        list->insert(n,data);
        self->length++;
    }

    apointer pop(){
        if (length==0)
          return NULL;
        apointer data= list->removeFirst();
        self->length--;
        return data;
    }

    apointer pop(int n){
        if (n >= length || n<0)
          return NULL;
        apointer data= list->remove(n);
        self->length--;
        return data;
    }

    apointer popLast(){
        if (length==0)
          return NULL;
        apointer data= list->removeLast();
        self->length--;
        return data;
    }

    void foreach(AFunc func,apointer userData){
        a_return_if_fail (func != NULL);
        ListIterator *iter=list->initIter();
        apointer value=NULL;
        while(iter->hasNext(&value)){
            func (value, userData);
        }
    }

    aboolean remove(apointer data){
        aboolean re=list->remove(data);
        if(re)
            self->length--;
        return re;
    }

    auint removeAll(apointer data){
        auint count=list->removeAll(data);
        self->length-=count;
        return count;
    }

    apointer peek(){
      return list->getFirst();
    }

    apointer peekLast(){
      return list->getLast();
    }

    apointer peek (auint index){
       if (index >= length)
         return NULL;
       if(index<0)
         return NULL;
       return list->get(index);
    }

    void sort(ACompareDataFunc compareFunc,apointer userData){
      a_return_if_fail (compareFunc != NULL);
      list->sort(compareFunc,userData);
    }

    void clear(){
        list->clear();
        length=0;
    }

    void  clear(ADestroyNotify func){
        list->clear(func);
        length=0;
    }

    AQueue *clone(){
        AQueue *result = new$ AQueue();
        ListIterator *iter=list->initIter();
        apointer value=NULL;
        while(iter->hasNext(&value))
           result->push(value);
        return result;
    }

    aboolean find(aconstpointer data){
        return list->find(data);
    }
    aboolean find(aconstpointer data,ACompareFunc func){
        return list->find(data,func);
    }

    apointer peekLastLink(){
      return (apointer)list->last;
    }

    void insertSorted(apointer data,ACompareDataFunc func,apointer userData){
        list->insertSorted(data,func,userData);
        length++;
    }

    void reverse (){
      list->reverse();
    }

    int  indexOf(apointer data){
        return list->indexOf(data);
    }

   ~AQueue(){
	   if(list!=NULL){
          list->unref();
	   }
   }


};
