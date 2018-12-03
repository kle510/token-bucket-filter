#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>


typedef struct myLinkedListElem {
    void *obj;
    struct myLinkedListElem *next;
    struct myLinkedListElem *prev;
} LinkedListElem;

typedef struct myLinkedList {
    int num_members;
    LinkedListElem anchor;

    int  (*Length)(struct myLinkedList *);
    int  (*Empty)(struct myLinkedList *);

    int  (*Append)(struct myLinkedList *, void*);
    int  (*Prepend)(struct myLinkedList *, void*);
    void (*Unlink)(struct myLinkedList *, LinkedListElem*);
    void (*UnlinkAll)(struct myLinkedList *);
    int  (*InsertBefore)(struct myLinkedList *, void*, LinkedListElem*);
    int  (*InsertAfter)(struct myLinkedList *, void*, LinkedListElem*);

    LinkedListElem *(*First)(struct myLinkedList *);
    LinkedListElem *(*Last)(struct myLinkedList *);
    LinkedListElem *(*Next)(struct myLinkedList *, LinkedListElem *cur);
    LinkedListElem *(*Prev)(struct myLinkedList *, LinkedListElem *cur);

    LinkedListElem *(*Find)(struct myLinkedList *, void *obj);
} LinkedList;



int  LinkedListLength(LinkedList*);
int  LinkedListEmpty(LinkedList*);

int  LinkedListAppend(LinkedList*, void*);
int  LinkedListPrepend(LinkedList*, void*);
void LinkedListUnlink(LinkedList*, LinkedListElem*);
void LinkedListUnlinkAll(LinkedList*);
int  LinkedListInsertAfter(LinkedList*, void*, LinkedListElem*);
int  LinkedListInsertBefore(LinkedList*, void*, LinkedListElem*);

LinkedListElem *LinkedListFirst(LinkedList*);
LinkedListElem *LinkedListLast(LinkedList*);
LinkedListElem *LinkedListNext(LinkedList*, LinkedListElem*);
LinkedListElem *LinkedListPrev(LinkedList*, LinkedListElem*);

LinkedListElem *LinkedListFind(LinkedList*, void*);


int LinkedListInit(LinkedList*);

int  LinkedListLength(LinkedList *list){
    return list->num_members;
}

int  LinkedListEmpty(LinkedList *list){

    //if list is empty, return true
    if (list->anchor.next == &list->anchor && 
        list->anchor.prev == &list->anchor){
        return 1;
    }else{ // list nonempty, return false
        return 0;
    }
}

int  LinkedListAppend(LinkedList *list, void *obj){

    LinkedListElem *elem = malloc(sizeof(LinkedListElem));

    //if empty, add obj to the list
    if (LinkedListEmpty(list)){

        elem->obj = obj;

        elem->next = &list->anchor;
        elem->prev = &list->anchor;

        list->anchor.next = elem;
        list->anchor.prev = elem;

        list->num_members++;

    }else if (!LinkedListEmpty(list)){ //if not empty, add obj after last

        elem->obj = obj;
        elem->prev = list->anchor.prev;
        list->anchor.prev->next = elem;
        elem->next = &list->anchor;
        list->anchor.prev = elem;
        list->num_members++;
        
    }else{
        return 0;
    }
    return 1;
}

int  LinkedListPrepend(LinkedList *list, void *obj){

    LinkedListElem *elem = malloc(sizeof(LinkedListElem));

    //if empty, add obj to the list
    if (LinkedListEmpty(list)){

        elem->obj = obj;

        elem->next = &list->anchor;
        elem->prev = &list->anchor;

        list->anchor.next = elem;
        list->anchor.prev = elem;

        list->num_members++;


    }else if (!LinkedListEmpty(list)){ //if not empty, add obj before last

        elem->obj = obj;
        elem->next = list->anchor.next;
        list->anchor.next->prev = elem;
        elem->prev = &list->anchor;
        list->anchor.next = elem;
        list->num_members++;

    }else{
        return 0;
    }
    return 1;
}

void LinkedListUnlink(LinkedList *list, LinkedListElem *elem){

    elem->next->prev = elem->prev;
    elem->prev->next = elem->next;
    list->num_members--;

    elem->prev = NULL;
    elem->next = NULL;
}

void LinkedListUnlinkAll(LinkedList *list){
    LinkedListElem *elem=NULL;

    for (elem=LinkedListFirst(list); elem != NULL; elem=LinkedListNext(list, elem)) {
        elem->next->prev = elem->prev;
        elem->prev->next = elem->next;
        list->num_members--;
    }
}

int  LinkedListInsertAfter(LinkedList *list, void* obj, LinkedListElem *elem){

    if (elem == NULL){
        if (LinkedListAppend(list, obj)){
            return 1;
        }
    }
    else{ //elem is not null, so find it

        LinkedListElem *new_elem = malloc(sizeof(LinkedListElem));

        new_elem->obj = obj;
        new_elem->next = elem->next;
        new_elem->prev = elem;

        new_elem->next->prev = new_elem;
        new_elem->prev->next = new_elem;

        list->num_members++;

        return 1;
    }
    return 0;
}

int  LinkedListInsertBefore(LinkedList *list, void* obj, LinkedListElem *elem){

    if (elem == NULL){
        if (LinkedListPrepend(list, obj)){
            return 1;
        }
    }
    else{ //elem is not null, so find it

        LinkedListElem *new_elem = malloc(sizeof(LinkedListElem));

        new_elem->obj = obj;
        new_elem->next = elem;
        new_elem->prev = elem->prev;

        new_elem->next->prev = new_elem;
        new_elem->prev->next = new_elem;

        list->num_members++;

        return 1;
    }
    return 0;
}

LinkedListElem *LinkedListFirst(LinkedList *list){

    if (LinkedListEmpty(list)){
        return NULL;
    }
    return list->anchor.next;
}

LinkedListElem *LinkedListLast(LinkedList *list){

    if (LinkedListEmpty(list)){
        return NULL;
    }
    return list->anchor.prev;
}

LinkedListElem *LinkedListNext(LinkedList *list, LinkedListElem *elem){

    //if last element
    if (elem->next == &list->anchor){
        return NULL;
    }

    return elem->next;
}

LinkedListElem *LinkedListPrev(LinkedList *list, LinkedListElem* elem){

    //if first element
    if (elem->prev == &list->anchor){
        return NULL;
    }

    return elem->prev;
}

LinkedListElem *LinkedListFind(LinkedList *list, void *obj){

    LinkedListElem *elem=NULL;

    for (elem=LinkedListFirst(list); elem != NULL; elem=LinkedListNext(list, elem)) {
        if (elem->obj == obj){
            return elem;
        }
    }
    return NULL;
}

int LinkedListInit(LinkedList *list){
    (*list).num_members = 0;

    list->anchor.next = &list->anchor;
    list->anchor.prev = &list->anchor;

    return 1;
}

