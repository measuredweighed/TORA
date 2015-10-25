//
//  structures.c
//  TORA
//
//  Created by Nial on 29/09/2015.
//  Copyright © 2015 Nial. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "structures.h"
#include "token_stream.h"
#include "interpretter.h"

void tora_release_instance(TORAUnknownType *ptr);

// Memory management
int num_malloc;
int num_free;
void *tora_malloc(size_t size)
{
    void *ptr = malloc(size);
    if(ptr)
    {
        num_malloc++;
    }
    return ptr;
}
void tora_free(void *p)
{
    free(p);
    num_free++;
}
void *tora_retain(void *ptr)
{
    if(!ptr) return NULL;
    
    ((TORAUnknownType *)ptr)->ref_count++;
    return ptr;
}
void tora_release(void *ptr)
{
    if(!ptr) return;
    
    TORAUnknownType *ref_ptr = (TORAUnknownType *)ptr;
    //assert(ref_ptr->ref_count > 0);
    
    ref_ptr->ref_count--;
    if(ref_ptr->ref_count == 0)
    {
        tora_release_instance(ref_ptr);
    }
}
void tora_release_instance(TORAUnknownType *ptr)
{
    switch(ptr->instance_type)
    {
        case TORAInstanceTypeEnvironment:
            free_environment((TORAEnvironment *)ptr);
            break;
        case TORAInstanceTypeExpression:
            free_expression(ptr);
            break;
        case TORAInstanceTypeToken:
            free_token((TORAToken *)ptr);
            break;
        case TORAInstanceTypeGeneric:
            tora_free(ptr);
            break;
        case TORAInstanceTypeLinkedList:
            free_linked_list((TORALinkedList *)ptr);
            break;
    }
}

// Linked list
void free_linked_list(TORALinkedList *list)
{
    assert(list);

    TORALinkedList *cur = list;
    while(cur)
    {
        TORALinkedList *next = cur->next;
        if(cur->name)
        {
            tora_release(cur->name);
            cur->name = NULL;
        }
        
        if(cur->value)
        {
            tora_release(cur->value);
            cur->value = NULL;
        }
        cur->next = NULL;
        
        tora_free(cur);
        cur = next;
    }
}
uint64_t linked_list_length(TORALinkedList *list)
{
    uint64_t length = 0;
    TORALinkedList *cur_item = list;
    while(cur_item)
    {
        length++;
        cur_item = cur_item->next;
    }
    return length;
}

// Queue
void queue_item(TORALinkedList **queue, TORALinkedList *list_item)
{
    assert(list_item);
    
    // If our queue is empty, assign this item as it's first occupant
    if(*queue == NULL)
    {
        *queue = list_item;
    }
    // Otherwise iterate to the end of the queue and add it there
    else
    {
        TORALinkedList *queue_item = *queue;
        while(queue_item->next != NULL)
        {
            queue_item = queue_item->next;
        }
        queue_item->next = list_item;
    }
    
    tora_retain(list_item);
}
// Works identically to queue_item, but accepts a non-TORALinkedList item
// and wraps it silently before adding it to the target queue
void queue_raw_item(TORALinkedList **queue, char *name, void *item)
{
    assert(item);
    
    TORALinkedList *link_item = tora_malloc(sizeof(TORALinkedList));
    if(!link_item)
    {
        TORA_RUNTIME_EXCEPTION("Failed to malloc linked list item for queue insertion");
    }
    link_item->instance_type = TORAInstanceTypeLinkedList;
    link_item->ref_count = 0;
    
    link_item->name = name ? tora_retain(new_string_expression(name)) : NULL;
    link_item->value = tora_retain(item);
    link_item->next = NULL;
    
    queue_item(queue, link_item);
}
TORALinkedList* pop_item_from_queue(TORALinkedList **queue)
{
    TORALinkedList *item_to_pop = *queue;
    if(*queue != NULL)
    {
        *queue = (*queue)->next;
        item_to_pop->next = NULL;
    }
    
    return item_to_pop;
}
void free_item_from_queue(TORALinkedList **queue)
{
    TORALinkedList *item = pop_item_from_queue(&(*queue));
    if(item)
    {
        tora_release(item);
    }
}
void drain_queue(TORALinkedList *queue)
{
    if(!queue) return;
    
    tora_release(queue);
}

// Debug helpers
void DEBUG_LINKED_LIST(TORALinkedList *list, int level)
{
    assert(list);
    
    TORALinkedList *cur = list;
    while(cur)
    {
        for(int i = 0; i < level; i++) printf("\t");
        printf("{\n");
        
        if(cur->name)
        {
            for(int i = 0; i < level; i++) printf("\t");
            printf("\tname:\n");
            DEBUG_EXPRESSION(cur->name, level+1);
        }
        
        for(int i = 0; i < level; i++) printf("\t");
        printf("\tval:\n");
        DEBUG_EXPRESSION(cur->value, level+1);
        
        for(int i = 0; i < level; i++) printf("\t");
        printf("}\n");
        cur = cur->next;
    }
}