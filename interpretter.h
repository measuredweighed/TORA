//
//  interpretter.h
//  TORA
//
//  Created by Nial on 03/09/2015.
//  Copyright (c) 2015 Nial. All rights reserved.
//

#ifndef __tora__interpretter__
#define __tora__interpretter__

#include <stdio.h>
#include "tora.h"

typedef struct TORAEnvironment TORAEnvironment;
struct TORAEnvironment {
    TORAInstanceType instance_type;
    int ref_count;
    
    TORAEnvironment *parent;
    TORALinkedList *var_list_head;
    TORALinkedList *var_list_tail;
};

TORAEnvironment *create_environment(TORAEnvironment *parent);
void* evaluate(void *expression, TORAEnvironment *environment, bool *return_encountered);

void free_expression(void *expression);
void free_environment(TORAEnvironment *enviroment);

#endif /* defined(__tora__interpretter__) */
