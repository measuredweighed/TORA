//
//  token_stream.h
//  TORA
//
//  Created by Nial on 30/08/2015.
//  Copyright © 2015 Nial. All rights reserved.
//

#ifndef parser_h
#define parser_h

#include <stdio.h>
#include <stdbool.h>

#include "input_stream.h"

extern int num_tok;

typedef enum {
    TORATokenTypeStr,
    TORATokenTypeNumeric,
    TORATokenTypePunctuation,
    TORATokenTypeOperation,
    TORATokenTypeKeyword,
    TORATokenTypeVariable,
    TORATokenTypeBoolean
} TORATokenType;

typedef struct {
    TORAInstanceType instance_type;
    int ref_count;
    
    TORATokenType type;
    void *val;
    double numeric_val;
    bool bool_val;
} TORAToken;

typedef struct {
    TORAInputStream *input_stream;
    TORAToken *current_token;
} TORATokenStream;

TORATokenStream *token_stream_from_input(TORAInputStream *stream);
TORAToken *token_stream_read_next(TORATokenStream *token_stream);
TORAToken *token_stream_next(TORATokenStream *token_stream);
TORAToken *token_stream_peek(TORATokenStream *token_stream);
bool token_stream_eof(TORATokenStream *token_stream);
void free_token_stream(TORATokenStream *stream);
void free_token(TORAToken *token);

void DEBUG_TOKEN(TORAToken *token);

#endif /* parser_h */
