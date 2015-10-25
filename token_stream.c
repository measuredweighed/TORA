//
//  token_stream.c
//  TORA
//
//  Created by Nial on 30/08/2015.
//  Copyright © 2015 Nial. All rights reserved.
//

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "tora.h"

// Token
TORAToken *token_create(TORATokenType type, void* val, double numeric_val);

// Helpers
char *char_as_string(char ch);

// Token stream
char *token_stream_read_while(TORATokenStream *token_stream, bool (*predicate)(char, char*));
char *token_stream_read_escaped(TORATokenStream *token_stream, char end);
TORAToken *token_stream_read_string(TORATokenStream *token_stream);
TORAToken *token_stream_read_number(TORATokenStream *token_stream);
TORAToken *token_stream_read_ident(TORATokenStream *token_stream);
void token_stream_skip_comment(TORATokenStream *token_stream);

int num_tok;

TORATokenStream *token_stream_from_input(TORAInputStream *input_stream)
{
    num_tok = 0;
    TORATokenStream *token_stream = tora_malloc(sizeof(TORATokenStream));
    if(token_stream)
    {
        token_stream->input_stream = input_stream;
        token_stream->current_token = NULL;
    }
    
    return token_stream;
}
TORAToken *token_stream_read_next(TORATokenStream *token_stream)
{
    assert(token_stream);
    assert(token_stream->input_stream);
    
    // Free our previously read token
    free_item_from_queue(&token_queue);
    
    TORAInputStream *input_stream = token_stream->input_stream;
    
    // Read until we hit a non-whitespace character
    char *whitespace = token_stream_read_while(token_stream, is_whitespace);
    tora_free(whitespace);
    
    if(input_stream_eof(input_stream))
    {
        return NULL;
    }
    
    char ch = input_stream_peek(input_stream);
    if(ch == '#')
    {
        token_stream_skip_comment(token_stream);
        return token_stream_read_next(token_stream);
    }
    else if(ch == '"')
    {
        return token_stream_read_string(token_stream);
    }
    else if(is_digit(ch, NULL))
    {
        return token_stream_read_number(token_stream);
    }
    else if(is_id_start(ch, NULL))
    {
        return token_stream_read_ident(token_stream);
    }
    else if(is_punctuation(ch, NULL))
    {
        char *str = char_as_string(input_stream_next_char(input_stream));
        if(str)
        {
            return token_create(TORATokenTypePunctuation, str, 0);
        }
        else
        {
            // TODO: handle error
            return NULL;
        }
    }
    else if(is_op_char(ch, NULL))
    {
        return token_create(TORATokenTypeOperation, token_stream_read_while(token_stream, is_op_char), 0);
    }
    
    TORA_PARSER_EXCEPTION(token_stream->input_stream->line, token_stream->input_stream->col, "Invalid character: %c", ch);
    
    return NULL;
}
void token_stream_skip_comment(TORATokenStream *token_stream)
{
    char *value = token_stream_read_while(token_stream, is_not_newline);
    tora_free(value);
}
TORAToken *token_stream_read_number(TORATokenStream *token_stream)
{
    char *value = token_stream_read_while(token_stream, is_numeric_value);
    if(value)
    {
        // Convert our string to a double representation
        double numeric_value = 0;
        sscanf(value, "%lf", &numeric_value);
        
        // Ditch the memory allocated while reading our string
        tora_free(value);
        
        // And finally return the numeric token
        return token_create(TORATokenTypeNumeric, NULL, numeric_value);
    }
    
    return NULL;
}
TORAToken *token_stream_read_string(TORATokenStream *token_stream)
{
    return token_create(TORATokenTypeStr, token_stream_read_escaped(token_stream, '"'), 0);
}
TORAToken *token_stream_read_ident(TORATokenStream *token_stream)
{
    char *id = token_stream_read_while(token_stream, is_id);
    if(!id)
    {
        return NULL;
    }
    
    bool keyword = is_keyword(id);
    return token_create(keyword ? TORATokenTypeKeyword : TORATokenTypeVariable, id, 0);
}
char *token_stream_read_escaped(TORATokenStream *token_stream, char end)
{
    assert(token_stream);
    assert(token_stream->input_stream);
    
    bool escaped = false;
    
    size_t pos = 0;
    size_t str_size = 256;
    char *str = tora_malloc(str_size*sizeof(char));
    if(!str)
    {
        TORA_RUNTIME_EXCEPTION("Failed to malloc space for escaped string read");
    }
    
    TORAInputStream *input_stream = token_stream->input_stream;
    
    // We need to move the input stream head position along by one, so we don't
    // get caught up on the opening " for this string
    input_stream_next_char(input_stream);
    
    while(!input_stream_eof(input_stream))
    {
        char ch = input_stream_next_char(input_stream);
        if(escaped)
        {
            str[pos] = ch;
            escaped = false;
            pos++;
        }
        else if(ch == '\\')
        {
            escaped = true;
        }
        else if(ch == end)
        {
            break;
        }
        else
        {
            str[pos] = ch;
            pos++;
        }
        
        if(pos >= str_size)
        {
            str_size *= 2;
            realloc(str, str_size);
        }
    }
    
    str[pos] = '\0';
    return str;
}
char *token_stream_read_while(TORATokenStream *token_stream, bool (*predicate)(char, char*))
{
    assert(token_stream);
    assert(token_stream->input_stream);
    
    size_t pos = 0;
    size_t str_size = 256;
    char *str = tora_malloc(str_size*sizeof(char));
    if(!str)
    {
        TORA_RUNTIME_EXCEPTION("Failed to malloc space for input stream read");
    }
    
    TORAInputStream *input_stream = token_stream->input_stream;
    while(!input_stream_eof(input_stream) && (*predicate)(input_stream_peek(input_stream), str))
    {
        str[pos] = input_stream_next_char(input_stream);
        pos++;
        
        if(pos >= str_size)
        {
            str_size *= 2;
            str = realloc(str, str_size);
        }
    }
    str[pos] = '\0'; // null terminate our string
    
    return str;
}
TORAToken *token_stream_next(TORATokenStream *token_stream)
{
    TORAToken *token = token_stream->current_token;
    token_stream->current_token = NULL;
    if(!token)
    {
        token = token_stream_read_next(token_stream);
    }
    
    return token;
}
TORAToken *token_stream_peek(TORATokenStream *token_stream)
{
    assert(token_stream);
    
    if(!token_stream->current_token)
    {
        token_stream->current_token = token_stream_read_next(token_stream);
    }
    return token_stream->current_token;
}
bool token_stream_eof(TORATokenStream *token_stream)
{
    return token_stream_peek(token_stream) == NULL;
}

// Token
TORAToken *token_create(TORATokenType type, void* val, double numeric_val)
{
    TORAToken *token = tora_malloc(sizeof(TORAToken));
    if(!token)
    {
        TORA_RUNTIME_EXCEPTION("Failed to malloc space for token");
    }
    token->instance_type = TORAInstanceTypeToken;
    token->ref_count = 0;
    
    token->type = type;
    token->val = val;
    token->numeric_val = numeric_val;
    
    // Add this item to our token queue
    queue_raw_item(&token_queue, NULL, token);
   
    num_tok++;
    return token;
}
void free_token(TORAToken *token)
{
    if(token->val)
    {
        tora_free(token->val);
        token->val = NULL;
    }
    tora_free(token);
    num_tok--;
}
void DEBUG_TOKEN(TORAToken *token)
{
    char type_str[256];
    switch(token->type)
    {
        case TORATokenTypeStr:
            strncpy(type_str, "string", 256);
            break;
        case TORATokenTypePunctuation:
            strncpy(type_str, "punctuation", 256);
            break;
        case TORATokenTypeOperation:
            strncpy(type_str, "op", 256);
            break;
        case TORATokenTypeNumeric:
            strncpy(type_str, "numeric", 256);
            break;
        case TORATokenTypeKeyword:
            strncpy(type_str, "keyword", 256);
            break;
        case TORATokenTypeVariable:
            strncpy(type_str, "variable", 256);
            break;
        case TORATokenTypeBoolean:
            strncpy(type_str, "boolean", 256);
            break;
        default:
            strncpy(type_str, "unknown", 256);
            break;
    }
    
    if(token->type == TORATokenTypeNumeric)
    {
        printf("{\n\ttype: %s,\n\tval: %f\n}\n", type_str, token->numeric_val);
    }
    else
    {
        printf("{\n\ttype: %s,\n\tval: %s\n}\n", type_str, (char*)token->val);
    }
}

// Helpers
void free_token_stream(TORATokenStream *stream)
{
    drain_queue(token_queue);
    tora_free(stream);
}
char *char_as_string(char ch)
{
    char *str = tora_malloc(2*sizeof(char));
    if(!str)
    {
        TORA_RUNTIME_EXCEPTION("Faled to malloc space for char_as_string conversion");
    }
    
    str[0] = ch;
    str[1] = '\0';
    return str;
}