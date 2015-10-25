//
//  parser.c
//  TORA
//
//  Created by Nial on 31/08/2015.
//  Copyright (c) 2015 Nial. All rights reserved.
//

#include <stdlib.h>
#include <string.h>

#include "tora.h"

typedef void* (*TORAParserCallback) (TORATokenStream*);

bool parser_is_punctuation(TORATokenStream *token_stream, char *cmp);
void parser_skip_punctuation(TORATokenStream *token_stream, char *cmp);
bool parser_is_keyword(TORATokenStream *token_stream, char *cmp);
void parser_skip_keyword(TORATokenStream *token_stream, char *cmp);
bool parser_is_operator(TORATokenStream *token_stream, char *cmp);
void parser_skip_operator(TORATokenStream *token_stream, char *cmp);

void* parse_prog(TORATokenStream *token_stream);
void* parse_array(TORATokenStream *token_stream);
void* parse_array_lookup(TORATokenStream *token_stream, void *array);
void* parse_while(TORATokenStream *token_stream);
void* parse_expression(TORATokenStream *token_stream);
void* parse_expression_callback(TORATokenStream *token_stream);
void* parse_atom(TORATokenStream *token_stream);
void* parse_atom_callback(TORATokenStream *token_stream);
void* parse_call(TORATokenStream *token_stream, void *func);
void *parse_varname(TORATokenStream *token_stream);
void* parse_return(TORATokenStream *token_stream);

void* parser_maybe_binary(TORATokenStream *token_stream, void* left_expression, int precedence);
void* parser_maybe_call(TORATokenStream *token_stream, TORAParserCallback);
void* parser_maybe_array_lookup(TORATokenStream *token_stream, TORAParserCallback parse_function);

TORALinkedList* delimited(TORATokenStream *token_stream, char *start, char *stop, char *separator, TORAParserCallback parser_callback);;

TORAParserProgExpression *parse_top_level(TORATokenStream *token_stream)
{
    TORALinkedList *expression_list_head = NULL;
    TORALinkedList *expression_list_cur = NULL;
    
    while(!token_stream_eof(token_stream))
    {
        void* expression = parse_expression(token_stream);

        TORALinkedList *expression_item = tora_malloc(sizeof(TORALinkedList));
        if(!expression_item)
        {
            TORA_RUNTIME_EXCEPTION("Failed to malloc prog expression list item");
        }
        expression_item->instance_type = TORAInstanceTypeLinkedList;
        expression_item->ref_count = 0;
        
        expression_item->name = NULL;
        expression_item->value = tora_retain(expression);
        expression_item->next = NULL;
        
        if(expression_list_head == NULL)
        {
            expression_list_head = expression_item;
            expression_list_cur = expression_list_head;
        }
        else
        {
            expression_list_cur->next = expression_item;
            expression_list_cur = expression_item;
            
            tora_retain(expression_item);
        }
        
        if(!token_stream_eof(token_stream)) parser_skip_punctuation(token_stream, ";");
    }
    
    // Once we've generated our expression list we wrap it in a prog expression
    // and then send it along for interpretation
    if(expression_list_head)
    {
        return new_prog_expression(expression_list_head);
    }
    return NULL;
}
void* parse_expression(TORATokenStream *token_stream)
{
    return parser_maybe_call(token_stream, parse_expression_callback);
}
void* parse_expression_callback(TORATokenStream *token_stream)
{
    void *expression = parse_atom(token_stream);
    return parser_maybe_binary(token_stream, expression, 0);
}
void* parse_bool(TORATokenStream *token_stream)
{
    TORAToken *next_token = token_stream_next(token_stream);
    assert(next_token);
    assert(next_token->val);
    
    TORAParserBoolExpression *expression = tora_malloc(sizeof(TORAParserBoolExpression));
    if(!expression)
    {
        TORA_RUNTIME_EXCEPTION("Failed to malloc boolean expression");
    }
    
    expression->type = TORAExpressionTypeBoolean;
    expression->val = strcmp((char *)next_token->val, "true") == 0;
    
    return expression;
}
void* parse_varname(TORATokenStream *token_stream)
{
    TORAToken *token = token_stream_next(token_stream);
    assert(token);
    
    if(token->type != TORATokenTypeVariable)
    {
        TORA_PARSER_EXCEPTION(token_stream->input_stream->line, token_stream->input_stream->col, "Expected a variable token, received token of type: %d", token->type);
    }
    
    return new_variable_expression(token->val);
}
void* parse_function(TORATokenStream *token_stream)
{
    // Determine whether or not this is a named function
    TORAToken *next_token = token_stream_peek(token_stream);
    char *name = NULL;
    if(next_token->type == TORATokenTypeVariable)
    {
        next_token = token_stream_next(token_stream);
        if(next_token)
        {
            name = tora_strcpy(next_token->val);
        }
    }
    
    TORALinkedList *arguments = delimited(token_stream, "(", ")", ",", parse_varname);
    void *body = parse_expression(token_stream);
    void *function = new_function_expression(name, body, arguments);
    if(name)
    {
        tora_free(name);
    }
    
    return function;
}
void* parse_prog(TORATokenStream *token_stream)
{
    TORALinkedList *expression_list = delimited(token_stream, "{", "}", ";", parse_expression);
    return new_prog_expression(expression_list);
}
void* parse_array(TORATokenStream *token_stream)
{
    TORALinkedList *expression_list_head = NULL;
    TORALinkedList *expression_list_cur = NULL;
    
    bool first = true;
    parser_skip_punctuation(token_stream, "[");
    while(!token_stream_eof(token_stream))
    {
        // If we've reached our stop token bail out
        if(parser_is_punctuation(token_stream, "]")) break;
        
        // If this is our first iteration we need to skip over our start token
        if(first)
        {
            first = false;
        }
        else
        {
            parser_skip_punctuation(token_stream, ",");
        }
        
        // Make sure we haven't hit our stop token after skipping over our start token
        if(parser_is_punctuation(token_stream, "]")) break;
        
        void *key_expression = NULL;
        void *value_expression = parse_expression(token_stream);
        // Look for the existence of a : punctuation token to denote an
        // associative value assignment
        if(parser_is_punctuation(token_stream, ":"))
        {
            parser_skip_punctuation(token_stream, ":");
            
            // If this is an associative assignment, the first expression
            // (value_expression) actually represents our key
            key_expression = value_expression;
            value_expression = parse_expression(token_stream);
        }
        // If the array assignment is non-associative, we assign a numeric
        // index for the new item
        else
        {
            key_expression = new_numeric_expression(linked_list_length(expression_list_head));
        }
        
        // Create a basic linked-list to store all of our expressions
        TORALinkedList *expression_list_item = tora_malloc(sizeof(TORALinkedList));
        if(!expression_list_item)
        {
            TORA_RUNTIME_EXCEPTION("delimited failed to malloc linked list item");
        }
        expression_list_item->instance_type = TORAInstanceTypeLinkedList;
        expression_list_item->ref_count = 0;
        
        expression_list_item->name = tora_retain(key_expression);
        expression_list_item->value = tora_retain(value_expression);
        expression_list_item->next = NULL;
        
        if(expression_list_head == NULL)
        {
            expression_list_head = expression_list_item;
            expression_list_cur = expression_list_head;
        }
        else
        {
            expression_list_cur->next = expression_list_item;
            expression_list_cur = expression_list_item;
            
            tora_retain(expression_list_item);
        }
    }
    
    parser_skip_punctuation(token_stream, "]");
    return new_array_expression(expression_list_head);
}
void* parse_array_lookup(TORATokenStream *token_stream, void *array)
{
    // Since we can have multiple dimensions of array lookup, i.e:
    // array[0][1], we iterate through and adjust the left_expression
    // accordingly
    void *left_expression = array;
    while(parser_is_punctuation(token_stream, "["))
    {
        parser_skip_punctuation(token_stream, "[");
        void *index = parse_expression(token_stream);
        parser_skip_punctuation(token_stream, "]");
        
        left_expression = new_array_index_expression(index, left_expression);
    }
    
    return left_expression;
}
void* parse_while(TORATokenStream *token_stream)
{
    parser_skip_keyword(token_stream, "while");
    void *condition = parse_expression(token_stream);
    void *body = parse_expression(token_stream);
    
    return new_while_expression(condition, body);
}
void* parse_if(TORATokenStream *token_stream)
{
    parser_skip_keyword(token_stream, "if");
    void *condition = parse_expression(token_stream);
    void *then = parse_expression(token_stream);
    void *el = NULL;
    
    if(parser_is_keyword(token_stream, "else"))
    {
        token_stream_next(token_stream);
        el = parse_expression(token_stream);
    }
    
    return new_if_expression(condition, then, el);
}
void* parse_return(TORATokenStream *token_stream)
{
    parser_skip_keyword(token_stream, "return");
    
    void *expression = NULL;
    if(!parser_is_punctuation(token_stream, ";"))
    {
        expression = parse_expression(token_stream);
    }
    // If we don't explicitly provide an expression for our return
    // we'll treat it as a boolean false
    else
    {
        expression = new_boolean_expression(false);
    }
    return new_return_expression(expression);
}
void* parse_call(TORATokenStream *token_stream, void *func)
{
    return new_call_expression(func, delimited(token_stream, "(", ")", ",", parse_expression));
}
void* parse_atom(TORATokenStream *token_stream)
{
    return parser_maybe_call(token_stream, parse_atom_callback);
}
void* parse_atom_callback(TORATokenStream *token_stream)
{
    if(parser_is_punctuation(token_stream, "("))
    {
        token_stream_next(token_stream);
        void *expression = parse_expression(token_stream);
        parser_skip_punctuation(token_stream, ")");
        return expression;
    }
    if(parser_is_punctuation(token_stream, "{"))
    {
        return parse_prog(token_stream);
    }
    if(parser_is_punctuation(token_stream, "["))
    {
        return parse_array(token_stream);
    }
    if(parser_is_keyword(token_stream, "if"))
    {
        return parse_if(token_stream);
    }
    if(parser_is_keyword(token_stream, "while"))
    {
        return parse_while(token_stream);
    }
    if(parser_is_keyword(token_stream, "true") || parser_is_keyword(token_stream, "false"))
    {
        return parse_bool(token_stream);
    }
    if(parser_is_keyword(token_stream, "func"))
    {
        token_stream_next(token_stream);
        return parse_function(token_stream);
    }
    if(parser_is_keyword(token_stream, "return"))
    {
        return parse_return(token_stream);
    }
    if(parser_is_operator(token_stream, "-"))
    {
        token_stream_next(token_stream);
        
        TORAParserUnknownExpression *expression = (TORAParserUnknownExpression *)parse_expression(token_stream);
        
        // When handling unary negation we need to check whether the expression we're negating
        // is a binary expression, otherwise something like a = -1 + 1 will evaluate incorrectly as
        // a = -(1+1). As a result, we only negate the left-hand binary expression in these instances
        if(expression->type == TORAExpressionTypeBinary)
        {
            printf("NEGATIVE BINARY\n");
            TORAParserAssignOrBinaryExpression *binary_expression = (TORAParserAssignOrBinaryExpression *)expression;
            TORAParserNegativeUnaryExpression *unary_expression = new_negative_unary_expression(binary_expression->left);
            tora_release(binary_expression->left);
            binary_expression->left = tora_retain(unary_expression);
            return binary_expression;
        }
        else
        {
            return new_negative_unary_expression(expression);
        }
    }
    
    TORAToken *token = token_stream_next(token_stream);
    assert(token);
    
    switch(token->type)
    {
        case TORATokenTypeVariable:
            return new_variable_expression(token->val);
            break;
        case TORATokenTypeNumeric:
            return new_numeric_expression(token->numeric_val);
            break;
        case TORATokenTypeStr:
            return new_string_expression(token->val);
            break;
        default:
            TORA_PARSER_EXCEPTION(token_stream->input_stream->line,
                                  token_stream->input_stream->col,
                                  "Unexpected token encountered: %s", token->val);
            break;
    }
    
    return NULL;
}

// _maybe* functions
void *parser_maybe_call(TORATokenStream *token_stream, TORAParserCallback parse_function)
{
    void *result_expression = parse_function(token_stream);
    if(parser_is_punctuation(token_stream, "("))
    {
        return parse_call(token_stream, result_expression);
    }
    else if(parser_is_punctuation(token_stream, "["))
    {
        return parse_array_lookup(token_stream, result_expression);
    }
    
    return result_expression;
}
void* parser_maybe_binary(TORATokenStream *token_stream, void *left_expression, int precedence)
{
    if(parser_is_operator(token_stream, NULL))
    {
        TORAToken *token = token_stream_peek(token_stream);
        int his_precedence = get_operator_precendence((char *)token->val);
        if(his_precedence > precedence)
        {
            char *val = tora_strcpy((char *)token->val);
            token_stream_next(token_stream);
        
            void *right_expression = parser_maybe_binary(token_stream, parse_atom(token_stream), his_precedence);
            void *result = NULL;
            
            // If this is a = operator, treat it as an assignment
            if(strcmp(val, "=") == 0)
            {
                TORAParserAssignOrBinaryExpression *expression = new_assign_expression(val, left_expression, right_expression);
                result = parser_maybe_binary(token_stream, expression, precedence);
            }
            // Otherwise treat it as a binary expression
            else
            {
                TORAParserAssignOrBinaryExpression *expression = new_binary_expression(val, left_expression, right_expression);
                result = parser_maybe_binary(token_stream, expression, precedence);
            }
            
            // The op will be copied when assigning to the expression, so free our copy here
            tora_free(val);
            
            return result;
        }
    }
    
    return left_expression;
}

// Helpers
TORALinkedList* delimited(TORATokenStream *token_stream, char *start, char *stop, char *separator, TORAParserCallback parser_callback)
{
    TORALinkedList *expression_list_head = NULL;
    TORALinkedList *expression_list_cur = NULL;
    
    bool first = true;
    parser_skip_punctuation(token_stream, start);
    while(!token_stream_eof(token_stream))
    {
        // If we've reached our stop token bail out
        if(parser_is_punctuation(token_stream, stop)) break;
        
        // If this is our first iteration we need to skip over our start token
        if(first)
        {
            first = false;
        }
        else
        {
            parser_skip_punctuation(token_stream, separator);
        }
        
        // Make sure we haven't hit our stop token after skipping over our start token
        if(parser_is_punctuation(token_stream, stop)) break;
        
        // If not, this must be our first valid token, so try to parse it
        void *expression = parser_callback(token_stream);
        
        // Create a basic linked-list to store all of our expressions
        TORALinkedList *expression_list_item = tora_malloc(sizeof(TORALinkedList));
        if(!expression_list_item)
        {
            TORA_RUNTIME_EXCEPTION("delimited failed to malloc linked list item");
        }
        expression_list_item->instance_type = TORAInstanceTypeLinkedList;
        expression_list_item->ref_count = 0;
        
        expression_list_item->name = NULL;
        expression_list_item->value = tora_retain(expression);
        expression_list_item->next = NULL;
        
        if(expression_list_head == NULL)
        {
            expression_list_head = expression_list_item;
            expression_list_cur = expression_list_head;
        }
        else
        {
            expression_list_cur->next = expression_list_item;
            expression_list_cur = expression_list_item;
            
            tora_retain(expression_list_item);
        }
    }
    
    parser_skip_punctuation(token_stream, stop);
    return expression_list_head;
}
bool parser_is_punctuation(TORATokenStream *token_stream, char *cmp)
{
    assert(token_stream);
    
    TORAToken *token = token_stream_peek(token_stream);
    return (token && token->type == TORATokenTypePunctuation && (!cmp || strcmp((char *)token->val, cmp) == 0));
}
void parser_skip_punctuation(TORATokenStream *token_stream, char *cmp)
{
    if(parser_is_punctuation(token_stream, cmp))
    {
        token_stream_next(token_stream);
    }
    else
    {
        //TORAToken *token = token_stream_peek(token_stream);
        //TORA_PARSER_EXCEPTION("runtime", (int)token_stream->input_stream->line, "Expecting punctuation: %s, found: %s", cmp, token->val);
    }
}
bool parser_is_keyword(TORATokenStream *token_stream, char *cmp)
{
    TORAToken *token = token_stream_peek(token_stream);
    return (token && token->type == TORATokenTypeKeyword && (!cmp || strcmp((char *)token->val, cmp) == 0));
}
void parser_skip_keyword(TORATokenStream *token_stream, char *cmp)
{
    if(parser_is_keyword(token_stream, cmp))
    {
        token_stream_next(token_stream);
    }
    else
    {
        TORAToken *token = token_stream_peek(token_stream);
        TORA_PARSER_EXCEPTION(token_stream->input_stream->line,
                              token_stream->input_stream->col,
                              "Expecting keyword: %s, found: %s", cmp, token->val);
    }
}
bool parser_is_operator(TORATokenStream *token_stream, char *cmp)
{
    TORAToken *token = token_stream_peek(token_stream);
    return (token && token->type == TORATokenTypeOperation && (!cmp || strcmp((char *)token->val, cmp) == 0));
}
void parser_skip_operator(TORATokenStream *token_stream, char *cmp)
{
    if(parser_is_operator(token_stream, cmp))
    {
        token_stream_next(token_stream);
    }
    else
    {
        TORAToken *token = token_stream_peek(token_stream);
        TORA_PARSER_EXCEPTION(token_stream->input_stream->line,
                              token_stream->input_stream->col,
                              "Expecting operator: %s, found: %s", cmp, token->val);
    }
}

// Helpers
TORAParserProgExpression *new_prog_expression(TORALinkedList *list)
{
    TORAParserProgExpression *expression = tora_malloc(sizeof(TORAParserProgExpression));
    if(!expression)
    {
        TORA_RUNTIME_EXCEPTION("Failed to malloc space for prog expression");
    }
    expression->instance_type = TORAInstanceTypeExpression;
    expression->ref_count = 0;
    expression->type = TORAExpressionTypeProg;
    expression->val = tora_retain(list);
    
    return expression;
}
TORAParserArrayExpression *new_array_expression(TORALinkedList *list)
{
    TORAParserArrayExpression *expression = tora_malloc(sizeof(TORAParserArrayExpression));
    if(!expression)
    {
        TORA_RUNTIME_EXCEPTION("Failed to malloc space for array expression");
    }
    expression->instance_type = TORAInstanceTypeExpression;
    expression->ref_count = 0;
    expression->type = TORAExpressionTypeArray;
    expression->val = NULL;
    if(list)
    {
        expression->val = tora_retain(list);
    }
    
    return expression;
}
TORAParserArrayIndexExpression *new_array_index_expression(void *index, void *array)
{
    TORAParserArrayIndexExpression *expression = tora_malloc(sizeof(TORAParserArrayIndexExpression));
    if(!expression)
    {
        TORA_RUNTIME_EXCEPTION("Failed to malloc space for array index expression");
    }
    expression->instance_type = TORAInstanceTypeExpression;
    expression->ref_count = 0;
    
    expression->type = TORAExpressionTypeArrayIndex;
    expression->index = tora_retain(index);
    expression->array = tora_retain(array);
    
    return expression;
}
TORAParserLambdaExpression *new_function_expression(char *name, void *body, TORALinkedList *arguments)
{
    assert(body);
    
    TORAParserLambdaExpression *expression = tora_malloc(sizeof(TORAParserLambdaExpression));
    if(!expression)
    {
        TORA_RUNTIME_EXCEPTION("Failed to malloc space for function expression");
    }
    expression->instance_type = TORAInstanceTypeExpression;
    expression->type = TORAExpressionTypeLambda;
    expression->ref_count = 0;
    expression->arguments = tora_retain(arguments);
    expression->body = tora_retain(body);
    expression->name = tora_strcpy(name);
    
    return expression;
}
TORAParserCallExpression *new_call_expression(void *func, TORALinkedList *arguments)
{
    assert(func);
    
    TORAParserCallExpression *expression = tora_malloc(sizeof(TORAParserCallExpression));
    if(!expression)
    {
        TORA_RUNTIME_EXCEPTION("Failed to malloc space for call expression");
    }
    expression->instance_type = TORAInstanceTypeExpression;
    expression->type = TORAExpressionTypeCall;
    expression->ref_count = 0;
    expression->arguments = tora_retain(arguments);
    expression->func = tora_retain(func);
    
    return expression;
}
TORAParserWhileExpression *new_while_expression(void *condition, void *body)
{
    assert(condition);
    assert(body);
    
    TORAParserWhileExpression *expression = tora_malloc(sizeof(TORAParserWhileExpression));
    if(!expression)
    {
        TORA_RUNTIME_EXCEPTION("Failed to malloc space for while expression");
    }
    expression->instance_type = TORAInstanceTypeExpression;
    expression->type = TORAExpressionTypeWhile;
    expression->ref_count = 0;
    expression->condition = tora_retain(condition);
    expression->body = tora_retain(body);
    
    return expression;
}
TORAParserIfThenElseExpression *new_if_expression(void *condition, void *then, void *el)
{
    assert(condition);
    assert(then);
    
    TORAParserIfThenElseExpression *expression = tora_malloc(sizeof(TORAParserIfThenElseExpression));
    if(!expression)
    {
        TORA_RUNTIME_EXCEPTION("Failed to malloc space for if/then/else expression");
    }
    expression->instance_type = TORAInstanceTypeExpression;
    expression->type = TORAExpressionTypeIfThenElse;
    expression->ref_count = 0;
    expression->condition = tora_retain(condition);
    expression->el = tora_retain(el);
    expression->then = tora_retain(then);
    
    return expression;
}
TORAParserAssignOrBinaryExpression *new_assign_expression(char *op, void *left, void *right)
{
    assert(op);
    assert(left);
    assert(right);
    
    TORAParserAssignOrBinaryExpression *expression = tora_malloc(sizeof(TORAParserAssignOrBinaryExpression));
    if(!expression)
    {
        TORA_RUNTIME_EXCEPTION("Failed to malloc space for assignment expression");
    }
    expression->instance_type = TORAInstanceTypeExpression;
    expression->type = TORAExpressionTypeAssign;
    expression->ref_count = 0;
    expression->op = tora_strcpy(op);
    expression->left = tora_retain(left);
    expression->right = tora_retain(right);
    
    return expression;
}
TORAParserAssignOrBinaryExpression *new_binary_expression(char *op, void *left, void *right)
{
    assert(op);
    assert(left);
    assert(right);
    
    TORAParserAssignOrBinaryExpression *expression = tora_malloc(sizeof(TORAParserAssignOrBinaryExpression));
    if(!expression)
    {
        TORA_RUNTIME_EXCEPTION("Failed to malloc space for binary expression");
    }
    expression->instance_type = TORAInstanceTypeExpression;
    expression->type = TORAExpressionTypeBinary;
    expression->ref_count = 0;
    expression->op = tora_strcpy(op);
    expression->left = tora_retain(left);
    expression->right = tora_retain(right);
    
    return expression;
}
TORAParserNegativeUnaryExpression *new_negative_unary_expression(void *expression_to_negate)
{
    TORAParserNegativeUnaryExpression *expression = tora_malloc(sizeof(TORAParserNegativeUnaryExpression));
    if(!expression)
    {
        TORA_RUNTIME_EXCEPTION("Failed to malloc space for negative unary expression");
    }
    expression->instance_type = TORAInstanceTypeExpression;
    expression->type = TORAExpressionTypeNegativeUnary;
    expression->ref_count = 0;
    expression->expression = tora_retain(expression_to_negate);
    
    return expression;
}
TORAParserStringExpression *new_string_expression(char *val)
{
    TORAParserStringExpression *expression = tora_malloc(sizeof(TORAParserStringExpression));
    if(!expression)
    {
        TORA_RUNTIME_EXCEPTION("Failed to malloc space for string expression");
    }
    expression->instance_type = TORAInstanceTypeExpression;
    expression->type = TORAExpressionTypeString;
    expression->ref_count = 0;
    expression->val = tora_strcpy(val);
    return expression;
}
TORAParserVariableExpression *new_variable_expression(char *val)
{
    TORAParserVariableExpression *expression = tora_malloc(sizeof(TORAParserVariableExpression));
    if(!expression)
    {
        TORA_RUNTIME_EXCEPTION("Failed to malloc space for variable expression");
    }
    expression->instance_type = TORAInstanceTypeExpression;
    expression->type = TORAExpressionTypeVariable;
    expression->ref_count = 0;
    expression->val = tora_strcpy(val);
    return expression;
}
TORAParserNumericExpression *new_numeric_expression(double val)
{
    TORAParserNumericExpression *expression = tora_malloc(sizeof(TORAParserNumericExpression));
    if(!expression)
    {
        TORA_RUNTIME_EXCEPTION("Failed to malloc space for numeric expression");
    }
    expression->instance_type = TORAInstanceTypeExpression;
    expression->type = TORAExpressionTypeNumeric;
    expression->ref_count = 0;
    expression->val = val;
    return expression;
}
TORAParserBoolExpression *new_boolean_expression(bool val)
{
    TORAParserBoolExpression *expression = tora_malloc(sizeof(TORAParserBoolExpression));
    if(!expression)
    {
        TORA_RUNTIME_EXCEPTION("Failed to malloc space for boolean expression");
    }
    expression->instance_type = TORAInstanceTypeExpression;
    expression->type = TORAExpressionTypeBoolean;
    expression->ref_count = 0;
    expression->val = val;
    return expression;
}
TORAParserReturnExpression *new_return_expression(void *val)
{
    TORAParserReturnExpression *expression = tora_malloc(sizeof(TORAParserReturnExpression));
    if(!expression)
    {
        TORA_RUNTIME_EXCEPTION("Failed to malloc space for return expression");
    }
    expression->instance_type = TORAInstanceTypeExpression;
    expression->type = TORAExpressionTypeReturn;
    expression->ref_count = 0;
    expression->expression = tora_retain(val);
    return expression;
}

// Debug
void DEBUG_EXPRESSION(void *expression, int level)
{
    assert(expression);
    
    TORAParserUnknownExpression *unknown_expression = (TORAParserUnknownExpression *)expression;
    
    for(int i = 0; i < level; i++) printf("\t");
    printf("(%p ref_count: %i) {\n", expression, unknown_expression->ref_count);
    
    switch(unknown_expression->type)
    {
        case TORAExpressionTypeAssign:
        case TORAExpressionTypeBinary:
        {
            TORAParserAssignOrBinaryExpression *assign_expression = (TORAParserAssignOrBinaryExpression *)expression;
            
            for(int i = 0; i < level+1; i++) printf("\t");
            if(unknown_expression->type == TORAExpressionTypeAssign)
            {
                printf("type: assign\n");
            }
            else
            {
                printf("type: binary\n");
            }
            
            if(assign_expression->op != NULL)
            {
                for(int i = 0; i < level+1; i++) printf("\t");
                printf("operator: %s\n", assign_expression->op);
            }
            
            if(assign_expression->left)
            {
                for(int i = 0; i < level+1; i++) printf("\t");
                printf("left:\n");
                DEBUG_EXPRESSION(assign_expression->left, level+1);
            }
            
            if(assign_expression->right)
            {
                for(int i = 0; i < level+1; i++) printf("\t");
                printf("right:\n");
                DEBUG_EXPRESSION(assign_expression->right, level+1);
            }
        }
            break;
        case TORAExpressionTypeBoolean:
        {
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("type: boolean\n");
            
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("val: %s\n", ((TORAParserBoolExpression *)expression)->val ? "true" : "false");
        }
            break;
        case TORAExpressionTypeCall:
        {
            TORAParserCallExpression *call_expression = (TORAParserCallExpression *)unknown_expression;
            
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("type: call\n");
            
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("func:\n");
            DEBUG_EXPRESSION(call_expression->func, level+1);
            
            if(call_expression->arguments)
            {
                for(int i = 0; i < level+1; i++) printf("\t");
                printf("args:\n");
                DEBUG_LINKED_LIST(call_expression->arguments, level+1);
            }
        }
            break;
        case TORAExpressionTypeWhile:
        {
            TORAParserWhileExpression *if_expression = (TORAParserWhileExpression *)unknown_expression;
            
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("type: while\n");
            
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("condition:\n");
            
            for(int i = 0; i < level+1; i++) printf("\t");
            DEBUG_EXPRESSION(if_expression->condition, level+1);
            
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("body:\n");
            DEBUG_EXPRESSION(if_expression->body, level+1);
        }
            break;
        case TORAExpressionTypeIfThenElse:
        {
            TORAParserIfThenElseExpression *if_expression = (TORAParserIfThenElseExpression *)unknown_expression;
            
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("type: if\n");
            
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("condition:\n");
            
            for(int i = 0; i < level+1; i++) printf("\t");
            DEBUG_EXPRESSION(if_expression->condition, level+1);
            
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("then:\n");
            DEBUG_EXPRESSION(if_expression->then, level+1);
            
            if(if_expression->el)
            {
                for(int i = 0; i < level+1; i++) printf("\t");
                printf("else:\n");
                DEBUG_EXPRESSION(if_expression->el, level+1);
            }
        }
            break;
        case TORAExpressionTypeLambda:
        {
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("type: function\n");
            
            TORAParserLambdaExpression *function_expression = (TORAParserLambdaExpression *)expression;
            
            if(function_expression->name)
            {
                for(int i = 0; i < level+1; i++) printf("\t");
                printf("name: %s\n", function_expression->name);
            }
            
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("body: \n");
            DEBUG_EXPRESSION(function_expression->body, level+1);
            
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("arguments: \n");
            TORALinkedList *prog_expression_list = function_expression->arguments;
            while(prog_expression_list)
            {
                printf("%s", prog_expression_list->value);
                DEBUG_EXPRESSION(prog_expression_list->value, level+1);
                prog_expression_list = prog_expression_list->next;
            }
        }
            break;
        case TORAExpressionTypeProg:
        {
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("type: prog\n");
            
            TORAParserProgExpression *prog_expression = (TORAParserProgExpression *)expression;
            TORALinkedList *prog_expression_list = prog_expression->val;
            while(prog_expression_list)
            {
                DEBUG_EXPRESSION(prog_expression_list->value, level+1);
                prog_expression_list = prog_expression_list->next;
            }
            printf("\n}");
        }
            break;
        case TORAExpressionTypeReturn:
        {
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("type: return\n");
            
            TORAParserReturnExpression *return_expression = (TORAParserReturnExpression *)expression;
            DEBUG_EXPRESSION(return_expression->expression, level+1);
            printf("\n}");
        }
            break;
        case TORAExpressionTypeArray:
        {
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("type: array\n");
            
            TORAParserArrayExpression *prog_expression = (TORAParserArrayExpression *)expression;
            if(prog_expression->val)
            {
                TORALinkedList *prog_expression_list = prog_expression->val;
                while(prog_expression_list)
                {
                    if(prog_expression_list->value)
                    {
                        DEBUG_EXPRESSION(prog_expression_list->value, level+1);
                    }
                    prog_expression_list = prog_expression_list->next;
                }
                printf("\n}");
            }
        }
            break;
        case TORAExpressionTypeArrayIndex:
        {
            TORAParserArrayIndexExpression *index_expression = (TORAParserArrayIndexExpression *)expression;
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("type: array lookup\n");
            
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("array:\n");
            DEBUG_EXPRESSION(index_expression->array, level+1);
            
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("key:\n");
            DEBUG_EXPRESSION(index_expression->index, level+1);
            
        }
            break;
        case TORAExpressionTypeNegativeUnary:
        {
            TORAParserNegativeUnaryExpression *negative_expression = (TORAParserNegativeUnaryExpression *)expression;
            
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("type: negative unary\n");
            for(int i = 0; i < level+1; i++) printf("\t");
            DEBUG_EXPRESSION(negative_expression->expression, level+1);
        }
            break;
        case TORAExpressionTypeVariable:
        {
            TORAParserVariableExpression *variable_expression = (TORAParserVariableExpression *)expression;
            
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("type: variable\n");
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("val: %s\n", variable_expression->val);
        }
            break;
        case TORAExpressionTypeString:
        {
            TORAParserStringExpression *string_expression = (TORAParserStringExpression *)expression;
            
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("type: string\n");
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("val: %s\n", string_expression->val);
        }
            break;
        case TORAExpressionTypeNumeric:
        {
            TORAParserNumericExpression *numeric_expression = (TORAParserNumericExpression *)expression;
            
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("type: numeric\n");
            for(int i = 0; i < level+1; i++) printf("\t");
            printf("val: %f\n", numeric_expression->val);
        }
            break;
    }
    
    for(int i = 0; i < level; i++) printf("\t");
    printf("}\n");
}