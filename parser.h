//
//  parser.h
//  TORA
//
//  Created by Nial on 31/08/2015.
//  Copyright (c) 2015 Nial. All rights reserved.
//

#ifndef __tora__parser__
#define __tora__parser__

#include <stdio.h>
#include "tora.h"

typedef enum {
    TORAExpressionTypeAssign,
    TORAExpressionTypeBinary,
    TORAExpressionTypeBoolean,
    TORAExpressionTypeString,
    TORAExpressionTypeNumeric,
    TORAExpressionTypeVariable,
    TORAExpressionTypeCall,
    TORAExpressionTypeWhile,
    TORAExpressionTypeIfThenElse,
    TORAExpressionTypeLambda,
    TORAExpressionTypeProg,
    TORAExpressionTypeArray,
    TORAExpressionTypeArrayIndex,
    TORAExpressionTypeNegativeUnary,
    TORAExpressionTypeReturn
} TORAExpressionType;

typedef struct {
    TORAInstanceType instance_type;
    int ref_count;
    
    TORAExpressionType type;
} TORAParserUnknownExpression;

typedef struct
{
    TORAInstanceType instance_type;
    int ref_count;
    
    TORAExpressionType type;
    char *val;
} TORAParserStringExpression;

typedef struct
{
    TORAInstanceType instance_type;
    int ref_count;
    
    TORAExpressionType type;
    double val;
} TORAParserNumericExpression;

typedef struct
{
    TORAInstanceType instance_type;
    int ref_count;
    
    TORAExpressionType type;
    bool val;
} TORAParserBoolExpression;

typedef struct
{
    TORAInstanceType instance_type;
    int ref_count;
    
    TORAExpressionType type;
    char *val;
} TORAParserVariableExpression;

typedef struct
{
    TORAInstanceType instance_type;
    int ref_count;
    
    TORAExpressionType type;
    TORALinkedList *val;
} TORAParserProgExpression;

typedef struct
{
    TORAInstanceType instance_type;
    int ref_count;
    
    TORAExpressionType type;
    TORALinkedList *val;
} TORAParserArrayExpression;

typedef struct
{
    TORAInstanceType instance_type;
    int ref_count;
    
    TORAExpressionType type;
    void *array;
    void *index;
} TORAParserArrayIndexExpression;

typedef struct
{
    TORAInstanceType instance_type;
    int ref_count;
    
    TORAExpressionType type;
    char *op;
    void *left;
    void *right;
} TORAParserAssignOrBinaryExpression;

typedef struct
{
    TORAInstanceType instance_type;
    int ref_count;
    
    TORAExpressionType type;
    void *condition;
    void *then;
    void *el;
} TORAParserIfThenElseExpression;

typedef struct
{
    TORAInstanceType instance_type;
    int ref_count;
    
    TORAExpressionType type;
    void *condition;
    void *body;
} TORAParserWhileExpression;

typedef struct
{
    TORAInstanceType instance_type;
    int ref_count;
    
    TORAExpressionType type;
    void *func;
    TORALinkedList *arguments;
} TORAParserCallExpression;

typedef struct
{
    TORAInstanceType instance_type;
    int ref_count;
    
    TORAExpressionType type;
    TORALinkedList *arguments;
    char *name;
    void *body;
} TORAParserLambdaExpression;

typedef struct
{
    TORAInstanceType instance_type;
    int ref_count;
    
    TORAExpressionType type;
    void *expression;
} TORAParserNegativeUnaryExpression;

typedef struct
{
    TORAInstanceType instance_type;
    int ref_count;
    
    TORAExpressionType type;
    void *expression;
} TORAParserReturnExpression;

TORAParserProgExpression *parse_top_level(TORATokenStream *token_stream);
void DEBUG_EXPRESSION(void *expression, int level);

TORAParserNumericExpression *new_numeric_expression(double val);
TORAParserBoolExpression *new_boolean_expression(bool val);
TORAParserProgExpression *new_prog_expression(TORALinkedList *list);
TORAParserArrayExpression *new_array_expression(TORALinkedList *list);
TORAParserArrayIndexExpression *new_array_index_expression(void *index, void *val);
TORAParserLambdaExpression *new_function_expression(char *name, void *body, TORALinkedList *arguments);
TORAParserCallExpression *new_call_expression(void *func, TORALinkedList *arguments);
TORAParserWhileExpression *new_while_expression(void *condition, void *body);
TORAParserIfThenElseExpression *new_if_expression(void *condition, void *then, void *el);
TORAParserAssignOrBinaryExpression *new_assign_expression(char *op, void *left, void *right);
TORAParserAssignOrBinaryExpression *new_binary_expression(char *op, void *left, void *right);
TORAParserNegativeUnaryExpression *new_negative_unary_expression(void *expression_to_negate);
TORAParserStringExpression *new_string_expression(char *val);
TORAParserVariableExpression *new_variable_expression(char *val);
TORAParserReturnExpression *new_return_expression(void *val);

#endif /* defined(__tora__parser__) */
