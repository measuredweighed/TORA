//
//  structures.h
//  TORA
//
//  Created by Nial on 29/09/2015.
//  Copyright © 2015 Nial. All rights reserved.
//

#ifndef structures_h
#define structures_h

#include <stdio.h>
#include <stdbool.h>

typedef enum {
    TORAInstanceTypeToken,
    TORAInstanceTypeExpression,
    TORAInstanceTypeEnvironment,
    TORAInstanceTypeLinkedList,
    TORAInstanceTypeGeneric
} TORAInstanceType;

typedef struct {
    TORAInstanceType instance_type;
    int ref_count;
} TORAUnknownType;

typedef struct TORALinkedList TORALinkedList;
struct TORALinkedList {
    TORAInstanceType instance_type;
    int ref_count;
    
    void *name;
    void *value;
    TORALinkedList *next;
};

extern int num_malloc;
extern int num_free;

// Memory management
void* tora_malloc(size_t size);
void tora_free(void *p);
char* tora_strcpy(char *str);
void* tora_retain(void *ptr);
void tora_release(void *ptr);

// Linked list
void free_linked_list(TORALinkedList *list);
uint64_t linked_list_length(TORALinkedList *list);

// Queue
void queue_item(TORALinkedList **queue, TORALinkedList *item);
void queue_raw_item(TORALinkedList **queue, char *name, void *item);
TORALinkedList* pop_item_from_queue(TORALinkedList **queue);
void free_item_from_queue(TORALinkedList **queue);
void drain_queue(TORALinkedList *queue);

// Debug helpers
void DEBUG_LINKED_LIST(TORALinkedList *list, int level);

#endif /* structures_h */
