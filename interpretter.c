//
//  interpretter.c
//  TORA
//
//  Created by Nial on 03/09/2015.
//  Copyright (c) 2015 Nial. All rights reserved.
//

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "interpretter.h"

void *environment_set_var(TORAEnvironment *environment, TORAParserUnknownExpression *key, TORAParserUnknownExpression *val);
TORALinkedList *environment_get_var(TORAEnvironment *environment, char *name, bool search_parent_scope);

void *apply_op(char *op, TORAParserUnknownExpression *a, TORAParserUnknownExpression *b, TORAEnvironment *environment);
void *concat_expressions(TORAParserUnknownExpression *a, TORAParserUnknownExpression *b);
bool expressions_are_equal(TORAParserUnknownExpression *a, TORAParserUnknownExpression *b);
void *maybe_standard_lib_call(char *method, TORALinkedList *arguments, TORAEnvironment *environment);

// Helpers
bool expression_is_truthy(TORAParserUnknownExpression *exp);
TORALinkedList *get_array_value_for_key(TORAParserArrayExpression *array, TORAParserUnknownExpression *key);
TORAParserArrayExpression *get_array_for_index_expression(TORAParserArrayIndexExpression *index_expression, TORAEnvironment *environment);
void *string_representation_for_expression(void *expression);

TORAEnvironment *create_environment(TORAEnvironment *parent)
{
    TORAEnvironment *environment = tora_malloc(sizeof(TORAEnvironment));
    if(!environment)
    {
        TORA_RUNTIME_EXCEPTION("Failed to malloc space for environment");
    }
    environment->instance_type = TORAInstanceTypeEnvironment;
    environment->ref_count = 0;
    
    environment->parent = parent;
    environment->var_list_head = NULL;
    environment->var_list_tail = NULL;
    
    queue_raw_item(&environment_queue, NULL, environment);
    return environment;
}
void environment_define_var(TORAEnvironment *environment, char *name, void *val, bool local_declaration)
{
    assert(environment);
    assert(name);
    assert(val);
    
    // If a variable with this name already exists, set it's value and then bail out
    TORALinkedList *var = environment_get_var(environment, name, !local_declaration);
    if(var)
    {
        tora_release(var->value);
        var->value = tora_retain(val);
        return;
    }
    
    // Otherwise create a new link in our vars linked list and stash the value away
    TORALinkedList *item = tora_malloc(sizeof(TORALinkedList));
    if(!item)
    {
        TORA_RUNTIME_EXCEPTION("Failed to malloc space for environment variable list item");
    }
    item->instance_type = TORAInstanceTypeLinkedList;
    item->ref_count = 0;
   
    item->name = tora_retain(new_string_expression(name));
    item->value = tora_retain(val);
    item->next = NULL;
    
    // Assign this item as the head of our environment variables list if no other
    // variables have been declared, and likewise point the tail item towards it
    if(environment->var_list_head == NULL)
    {
        environment->var_list_head = item;
        environment->var_list_tail = item;
    }
    // If we already have an established list of variables add this to our tail
    // and set our tail to point to the new item
    else
    {
        environment->var_list_tail->next = item;
        environment->var_list_tail = item;
    }
    tora_retain(item);
}
void *environment_set_var(TORAEnvironment *environment, TORAParserUnknownExpression *variable, TORAParserUnknownExpression *val)
{
    assert(environment);
    assert(variable);
    assert(val);
    
    void *key = NULL;
    
    // We need to derive a 'name' (key) for this variable assignment. Depending on whether we've been passed
    // a variable on the left-side of the assignment, or an array index look-up we need to perform some alternative logic
    if(variable->type == TORAExpressionTypeVariable)
    {
        // If we're assigning to a standard variable, i.e: a = 1,
        // we just need to ask the left-side variable expression for it's 'val'
        // which in the case of the above example will be 'a'.
        key = ((TORAParserVariableExpression *)variable)->val;
    }
    // This is an array look-up
    else if(variable->type == TORAExpressionTypeArrayIndex)
    {
        // Assigning to an array index, i.e: a[2] = 1, is slightly more complex...
        TORAParserArrayIndexExpression *array_index_expression = (TORAParserArrayIndexExpression *)variable;
        TORAParserUnknownExpression *array_ref_expression = array_index_expression->array;
        if(array_ref_expression->type == TORAExpressionTypeVariable)
        {
            // key here relates to the lookup offset, i.e: a[2] name will = 2
            key = evaluate(array_index_expression->index, environment, NULL);

            // Next we use the look-up expression to grab the current defined value for the variable 'a'.
            TORAParserArrayExpression *array_expression = get_array_for_index_expression(array_index_expression, environment);
            if(array_expression)
            {
                // Then we use a helper function to grab a pointer to the value stored at the offset our
                // array index expression points to, i.e: 2.
                TORALinkedList *array_val = get_array_value_for_key(array_expression, key);
                if(array_val)
                {
                    tora_release(array_val->value);
                    array_val->value = tora_retain(val);
                }
                else
                {
                    TORALinkedList *new_array_val = tora_malloc(sizeof(TORALinkedList));
                    if(!new_array_val)
                    {
                        TORA_RUNTIME_EXCEPTION("Failed to malloc space for new array list item");
                    }
                    new_array_val->name = tora_retain(key);
                    new_array_val->value = tora_retain(val);
                    new_array_val->next = NULL;
                    new_array_val->ref_count = 0;
                    new_array_val->instance_type = TORAInstanceTypeLinkedList;
                    
                    TORALinkedList *cur_array_val = array_expression->val;
                    if(cur_array_val)
                    {
                        while(cur_array_val->next)
                        {
                            cur_array_val = cur_array_val->next;
                        }
                        cur_array_val->next = tora_retain(new_array_val);
                    }
                    else
                    {
                        array_expression->val = tora_retain(new_array_val);
                    }
                }
                
                // ... and finally we replace the entire linked list to avoid destroying the stored array value
                val = (TORAParserUnknownExpression *)array_expression;
            }
        }
        else
        {
            TORA_INTERPRETTER_EXCEPTION("Attempting array value set with invalid offset");
        }
    }
    // Undefined behaviour!
    else
    {
        TORA_INTERPRETTER_EXCEPTION("Attempting value set on invalid left-hand expression");
    }
    
    environment_define_var(environment, key, val, true);
    return val;
}
TORALinkedList *environment_get_var(TORAEnvironment *environment, char *name, bool search_parent_scope)
{
    assert(environment);
    assert(name);
    
    TORALinkedList *cur_item = environment->var_list_head;
    while(cur_item)
    {
        TORAParserStringExpression *key = (TORAParserStringExpression *)cur_item->name;
        if(key && key->type == TORAExpressionTypeString && strcmp(key->val, name) == 0)
        {
            return cur_item;
        }
        cur_item = cur_item->next;
    }
    
    if(environment->parent && search_parent_scope)
    {
        return environment_get_var(environment->parent, name, search_parent_scope);
    }
    return NULL;
}
void free_environment(TORAEnvironment *environment)
{
    assert(environment);
    
    if(environment->var_list_head)
    {
        tora_release(environment->var_list_head);
        environment->var_list_head = NULL;
    }
    environment->var_list_tail = NULL;
    
    tora_free(environment);
}

// Evaluator
void* evaluate(void *expression, TORAEnvironment *environment, bool *return_encountered)
{
    assert(expression);
    assert(environment);
    
    TORAParserUnknownExpression *unknown_expression = (TORAParserUnknownExpression *)expression;
    switch(unknown_expression->type)
    {
        case TORAExpressionTypeString:
        case TORAExpressionTypeBoolean:
        case TORAExpressionTypeNumeric:
            return expression;
            break;        
        case TORAExpressionTypeVariable:
        {
            TORAParserVariableExpression *variable_expression = (TORAParserVariableExpression *)expression;
            TORALinkedList *val = environment_get_var(environment, variable_expression->val, true);
            if(val)
            {
                return val->value;
            }
            else
            {
                return NULL;
                //TORA_INTERPRETTER_EXCEPTION("Undefined variable: %s", ((TORAParserVariableExpression *)expression)->val);
            }
        }
            break;
        
        case TORAExpressionTypeArrayIndex:
        {
            TORAParserArrayExpression *array_expression = get_array_for_index_expression((TORAParserArrayIndexExpression*)expression, environment);
            TORAParserUnknownExpression *key = evaluate(((TORAParserArrayIndexExpression*)expression)->index, environment, NULL);

            TORALinkedList *val = get_array_value_for_key(array_expression, key);
            if(val)
            {
                void *result = evaluate(val->value, environment, NULL);
                queue_raw_item(&interpretter_queue, NULL, result);
                return result;
            }
        }
            break;
            
        case TORAExpressionTypeAssign:
        {
            TORAParserAssignOrBinaryExpression *assign_expression = (TORAParserAssignOrBinaryExpression *)expression;
            TORAParserUnknownExpression *left_expression = (TORAParserUnknownExpression *)assign_expression->left;
            
            // Only allow us to assign values to variables or array look-ups
            if(left_expression->type != TORAExpressionTypeVariable &&
               left_expression->type != TORAExpressionTypeArrayIndex)
            {
                TORA_INTERPRETTER_EXCEPTION("Cannot assign to values of this type: %i", left_expression->type);
            }
            
            void *right_expression = evaluate(assign_expression->right, environment, NULL);
            return environment_set_var(environment, left_expression, right_expression);
        }
            break;

        case TORAExpressionTypeBinary:
        {
            TORAParserAssignOrBinaryExpression *binary_expression = (TORAParserAssignOrBinaryExpression *)expression;
            
            void *left = evaluate(binary_expression->left, environment, NULL);
            void *right = evaluate(binary_expression->right, environment, NULL);
            void *result = apply_op(binary_expression->op, left, right, environment);
            if(result)
            {
                queue_raw_item(&interpretter_queue, NULL, result);
            }
            return result;
        }
            break;
            
        case TORAExpressionTypeLambda:
        {
            TORAParserLambdaExpression *lamda_expression = (TORAParserLambdaExpression *)expression;
            if(lamda_expression->name)
            {
                environment_define_var(environment, lamda_expression->name, expression, true);
            }
            return expression;
        }
            break;
            
        case TORAExpressionTypeProg:
        {
            TORAParserProgExpression *prog_expression = (TORAParserProgExpression *)expression;
            TORALinkedList *cur_expression = prog_expression->val;
            void *val = NULL;
            
            bool return_encountered_in_prog = false;
            while(cur_expression)
            {
                val = evaluate(cur_expression->value, environment, &return_encountered_in_prog);
                if(val)
                {
                    queue_raw_item(&interpretter_queue, NULL, val);
                    
                    // Determine whether or not we should return from the prog
                    if(return_encountered && return_encountered_in_prog)
                    {
                        *return_encountered = return_encountered_in_prog;
                        return val;
                    }
                }
                
                // Otherwise continue parsing the prog
                cur_expression = cur_expression->next;
            }
            
            if(!val)
            {
                val = new_boolean_expression(false);
                queue_raw_item(&interpretter_queue, NULL, val);
            }
            return val;
        }
            break;
        case TORAExpressionTypeReturn:
        {
            TORAParserReturnExpression *return_expression = (TORAParserReturnExpression *)expression;
            
            // Signals to evaluate() callers up the chain that we've encountered a return node
            if(return_encountered) *return_encountered = true;
            return evaluate(return_expression->expression, environment, NULL);
        }
            break;
        case TORAExpressionTypeNegativeUnary:
        {
            TORAParserNegativeUnaryExpression *negative_expression = (TORAParserNegativeUnaryExpression *)expression;
            TORAParserUnknownExpression *val = (TORAParserUnknownExpression *)evaluate(negative_expression->expression, environment, NULL);
            if(val->type == TORAExpressionTypeNumeric)
            {
                ((TORAParserNumericExpression *)val)->val *= -1;
            }
            else
            {
                TORA_INTERPRETTER_EXCEPTION("Attempted to negate a non-numeric value");
            }
            return val;
        }
            break;
        case TORAExpressionTypeArray:
        {
            // Iterate over our array and evaluate each of it's values
            // This is necessary when returning arrays from functions
            // due to the fact that they can contain locally scoped variables
            TORALinkedList *cur = ((TORAParserArrayExpression *)expression)->val;
            while(cur)
            {
                void *old_value = cur->value;
                void *new_value = evaluate(cur->value, environment, NULL);
                cur->value = tora_retain(new_value);
                tora_release(old_value);
                cur = cur->next;
            } 
            return expression;
        }
            break;
            
        case TORAExpressionTypeCall:
        {
            TORAParserCallExpression *call_expression = (TORAParserCallExpression *)expression;
            TORAParserLambdaExpression *function_expression = (TORAParserLambdaExpression *)evaluate(call_expression->func, environment, NULL);
            
            // If our function call evalutes to a known function we can try to execute it...
            if(function_expression)
            {
                TORAEnvironment *new_environment = create_environment(environment);
                
                TORALinkedList *cur_argument = function_expression->arguments;
                TORALinkedList *cur_value = call_expression->arguments;
                while(cur_argument)
                {
                    void *val = evaluate(cur_value->value, environment, NULL);
                    
                    TORAParserVariableExpression *variable_expression = (TORAParserVariableExpression *)cur_argument->value;
                    environment_define_var(new_environment, variable_expression->val, val, true);

                    cur_argument = cur_argument->next;
                    cur_value = cur_value->next;
                }
                
                bool return_encountered_in_call = false;
                void *result = evaluate(function_expression->body, new_environment, &return_encountered_in_call);
                if(return_encountered && return_encountered_in_call)
                {
                    *return_encountered = return_encountered_in_call;
                }
                return result;
            }
            // Otherwise check whether this call is referencing something from our standard lib
            else
            {
                TORAParserVariableExpression *function_name_func = (TORAParserVariableExpression *)call_expression->func;
                if(function_name_func->type == TORAExpressionTypeVariable)
                {
                    return maybe_standard_lib_call(function_name_func->val, call_expression->arguments, environment);
                }
            }
            return NULL;
        }
            break;
        case TORAExpressionTypeWhile:
        {
            bool return_encountered_in_while = false;
            
            TORAParserWhileExpression *while_expression = (TORAParserWhileExpression *)expression;
            while(((TORAParserBoolExpression *)evaluate(while_expression->condition, environment, NULL))->val)
            {
                void *val = evaluate(while_expression->body, environment, &return_encountered_in_while);
                if(return_encountered && return_encountered_in_while)
                {
                    *return_encountered = return_encountered_in_while;
                    return val;
                }
            }
            
            return new_boolean_expression(false);
        }
            break;
        case TORAExpressionTypeIfThenElse:
        {
            TORAParserIfThenElseExpression *if_expression = (TORAParserIfThenElseExpression *)expression;
            TORAParserBoolExpression *condition = evaluate(if_expression->condition, environment, NULL);
            
            // If this statements condition evaluated true
            if(condition->val)
            {
                bool return_encountered_in_if_then = false;
                void *result = evaluate(if_expression->then, environment, &return_encountered_in_if_then);
                if(return_encountered && return_encountered_in_if_then)
                {
                    *return_encountered = return_encountered_in_if_then;
                }
                return result;
            }
            // Otherwise, execute our else block
            else if(if_expression->el)
            {
                bool return_encountered_in_if_then_else = false;
                void *result = evaluate(if_expression->el, environment, &return_encountered_in_if_then_else);
                if(return_encountered && return_encountered_in_if_then_else)
                {
                    *return_encountered = return_encountered_in_if_then_else;
                }
                return result;
            }
            else
            {
                return new_boolean_expression(false);
            }
        }
            break;
        default:
            break;
    }
    
    return NULL;
}
void *maybe_standard_lib_call(char *method, TORALinkedList *arguments, TORAEnvironment *environment)
{
    // Math functions
    if(strcmp(method, "sin") == 0)
    {
        TORAParserNumericExpression *numeric = (TORAParserNumericExpression *)evaluate(arguments->value, environment, NULL);
        void *result = new_numeric_expression(sin(numeric->val));
        queue_raw_item(&interpretter_queue, NULL, numeric);
        queue_raw_item(&interpretter_queue, NULL, result);
        return result;
    }
    else if(strcmp(method, "cos") == 0)
    {
        TORAParserNumericExpression *numeric = (TORAParserNumericExpression *)evaluate(arguments->value, environment, NULL);
        void *result = new_numeric_expression(cos(numeric->val));
        queue_raw_item(&interpretter_queue, NULL, numeric);
        queue_raw_item(&interpretter_queue, NULL, result);
        return result;
    }
    else if(strcmp(method, "tan") == 0)
    {
        TORAParserNumericExpression *numeric = (TORAParserNumericExpression *)evaluate(arguments->value, environment, NULL);
        void *result = new_numeric_expression(tan(numeric->val));
        queue_raw_item(&interpretter_queue, NULL, numeric);
        queue_raw_item(&interpretter_queue, NULL, result);
        return result;
    }
    else if(strcmp(method, "atan") == 0)
    {
        TORAParserNumericExpression *numeric = (TORAParserNumericExpression *)evaluate(arguments->value, environment, NULL);
        void *result = new_numeric_expression(atan(numeric->val));
        queue_raw_item(&interpretter_queue, NULL, numeric);
        queue_raw_item(&interpretter_queue, NULL, result);
        return result;
    }
    else if(strcmp(method, "log") == 0)
    {
        TORAParserNumericExpression *numeric = (TORAParserNumericExpression *)evaluate(arguments->value, environment, NULL);
        void *result = new_numeric_expression(log(numeric->val));
        queue_raw_item(&interpretter_queue, NULL, numeric);
        queue_raw_item(&interpretter_queue, NULL, result);
        return result;
    }
    else if(strcmp(method, "exp") == 0)
    {
        TORAParserNumericExpression *numeric = (TORAParserNumericExpression *)evaluate(arguments->value, environment, NULL);
        void *result = new_numeric_expression(exp(numeric->val));
        queue_raw_item(&interpretter_queue, NULL, numeric);
        queue_raw_item(&interpretter_queue, NULL, result);
        return result;
    }
    else if(strcmp(method, "round") == 0)
    {
        TORAParserNumericExpression *numeric = (TORAParserNumericExpression *)evaluate(arguments->value, environment, NULL);
        void *result = new_numeric_expression(round(numeric->val));
        queue_raw_item(&interpretter_queue, NULL, numeric);
        queue_raw_item(&interpretter_queue, NULL, result);
        return result;
    }
    else if(strcmp(method, "min") == 0)
    {
        TORAParserNumericExpression *a = (TORAParserNumericExpression *)evaluate(arguments->value, environment, NULL);
        TORAParserNumericExpression *b = (TORAParserNumericExpression *)evaluate(arguments->next->value, environment, NULL);
        void *result = new_numeric_expression(fmin(a->val, b->val));
        queue_raw_item(&interpretter_queue, NULL, a);
        queue_raw_item(&interpretter_queue, NULL, b);
        queue_raw_item(&interpretter_queue, NULL, result);
        return result;
    }
    else if(strcmp(method, "max") == 0)
    {
        TORAParserNumericExpression *a = (TORAParserNumericExpression *)evaluate(arguments->value, environment, NULL);
        TORAParserNumericExpression *b = (TORAParserNumericExpression *)evaluate(arguments->next->value, environment, NULL);
        void *result = new_numeric_expression(fmax(a->val, b->val));
        queue_raw_item(&interpretter_queue, NULL, a);
        queue_raw_item(&interpretter_queue, NULL, b);
        queue_raw_item(&interpretter_queue, NULL, result);
        return result;
    }
    
    // Collection functions
    else if(strcmp(method, "length") == 0)
    {
        TORAParserArrayExpression *array_expression = (TORAParserArrayExpression *)evaluate(arguments->value, environment, NULL);
        if(array_expression && array_expression->type == TORAExpressionTypeArray)
        {
            queue_raw_item(&interpretter_queue, NULL, array_expression);
            
            void *result = new_numeric_expression(linked_list_length(array_expression->val));
            queue_raw_item(&interpretter_queue, NULL, result);
            return result;
        }
        else
        {
            TORA_INTERPRETTER_EXCEPTION("Unable to determine collection length for invalid type: %i", array_expression->type);
        }
    }
    
    // Logging functions
    else if(strcmp(method, "print") == 0)
    {
        TORAParserStringExpression *string_expression = (TORAParserStringExpression *)evaluate(arguments->value, environment, NULL);
        if(string_expression)
        {
            queue_raw_item(&interpretter_queue, NULL, string_expression);
            
            if(string_expression->type == TORAExpressionTypeString)
            {
                printf("%s", string_expression->val);
            }
        }
    }
    else if(strcmp(method, "println") == 0)
    {
        TORAParserUnknownExpression *expression = (TORAParserUnknownExpression *)evaluate(arguments->value, environment, NULL);
        if(expression)
        {
            queue_raw_item(&interpretter_queue, NULL, expression);
            
            TORAParserStringExpression *string_expression = string_representation_for_expression(expression);
            if(string_expression)
            {
                printf("%s\n", string_expression->val);
                
                queue_raw_item(&interpretter_queue, NULL, string_expression);
            }
        }
    }
    return NULL;
}
void *apply_op(char *op,
               TORAParserUnknownExpression *a,
               TORAParserUnknownExpression *b,
               TORAEnvironment *environment)
{
    assert(op);
    assert(a);
    assert(b);
    
    TORAParserNumericExpression *num_a = (TORAParserNumericExpression *)a;
    TORAParserNumericExpression *num_b = (TORAParserNumericExpression *)b;
    
    void *result = NULL;
    if(strcmp(op, "+") == 0)
    {
        result = (void *)concat_expressions(a, b);
    }
    else if(strcmp(op, "-") == 0)
    {
        result = (void *)new_numeric_expression(num_a->val - num_b->val);
    }
    else if(strcmp(op, "*") == 0)
    {
        result = (void *)new_numeric_expression(num_a->val * num_b->val);
    }
    else if(strcmp(op, "/") == 0)
    {
        result = (void *)new_numeric_expression(num_a->val / num_b->val);
    }
    else if(strcmp(op, "%") == 0)
    {
        result = (void *)new_numeric_expression(fmod(num_a->val, num_b->val));
    }
    else if(strcmp(op, "&&") == 0)
    {
        TORAParserUnknownExpression *eval_a = (TORAParserUnknownExpression *)evaluate(a, environment, NULL);
        TORAParserUnknownExpression *eval_b = (TORAParserUnknownExpression *)evaluate(b, environment, NULL);
        bool val = expression_is_truthy(eval_a) && expression_is_truthy(eval_b);
        result = (void *)new_boolean_expression(val);
    }
    else if(strcmp(op, "||") == 0)
    {
        TORAParserUnknownExpression *eval_a = (TORAParserUnknownExpression *)evaluate(a, environment, NULL);
        TORAParserUnknownExpression *eval_b = (TORAParserUnknownExpression *)evaluate(b, environment, NULL);
        bool val = expression_is_truthy(eval_a) || expression_is_truthy(eval_b);
        result = (void *)new_boolean_expression(val);
    }
    else if(strcmp(op, "<") == 0)
    {
        result = (void *)new_boolean_expression(num_a->val < num_b->val);
    }
    else if(strcmp(op, ">") == 0)
    {
        result = (void *)new_boolean_expression(num_a->val > num_b->val);
    }
    else if(strcmp(op, "<=") == 0)
    {
        result = (void *)new_boolean_expression(num_a->val <= num_b->val);
    }
    else if(strcmp(op, ">=") == 0)
    {
        result = (void *)new_boolean_expression(num_a->val >= num_b->val);
    }
    else if(strcmp(op, "==") == 0)
    {
        result = (void *)new_boolean_expression(expressions_are_equal(a, b));
    }
    else if(strcmp(op, "!=") == 0)
    {
        result = (void *)new_boolean_expression(!expressions_are_equal(a, b));
    }
    
    if(result == NULL)
    {
        TORA_INTERPRETTER_EXCEPTION("apply_op call failed");
    }
    return result;
}
// Determines the equality of two expressions
bool expressions_are_equal(TORAParserUnknownExpression *a, TORAParserUnknownExpression *b)
{
    assert(a);
    assert(b);
    
    if(a->type == TORAExpressionTypeString)
    {
        TORAParserStringExpression *string_a = (TORAParserStringExpression *)a;
        TORAParserStringExpression *string_b = (TORAParserStringExpression *)b;
        
        // string/string equality check
        if(b->type == TORAExpressionTypeString)
        {
            return strcmp(string_a->val, string_b->val) == 0;
        }
    }
    else if(a->type == TORAExpressionTypeNumeric)
    {
        TORAParserNumericExpression *num_a = (TORAParserNumericExpression *)a;
        TORAParserNumericExpression *num_b = (TORAParserNumericExpression *)b;
        
        // numeric/numeric equality check
        if(b->type == TORAExpressionTypeNumeric)
        {
            return num_a->val == num_b->val;
        }
    }
    
    return false;
}
void *concat_expressions(TORAParserUnknownExpression *a, TORAParserUnknownExpression *b)
{
    assert(a);
    assert(b);
    
    TORAParserNumericExpression *num_a = (TORAParserNumericExpression *)a;
    TORAParserNumericExpression *num_b = (TORAParserNumericExpression *)b;
    
    TORAParserStringExpression *string_a = (TORAParserStringExpression *)a;
    TORAParserStringExpression *string_b = (TORAParserStringExpression *)b;
    
    void *result = NULL;
    if(a->type == TORAExpressionTypeString)
    {
        string_b = string_representation_for_expression(b);
        char *val = tora_malloc(strlen(string_a->val) + strlen(string_b->val) + 1);
        if(!val)
        {
            TORA_RUNTIME_EXCEPTION("Malloc failed during string concatenation");
        }
        strcpy(val, string_a->val);
        strcat(val, string_b->val);
        result = (void *)new_string_expression(val);
        queue_raw_item(&interpretter_queue, NULL, string_b);
        tora_free(val);
    }
    else if(a->type == TORAExpressionTypeNumeric)
    {
        // concat a numeric + numeric
        if(b->type == TORAExpressionTypeNumeric)
        {
            result = (void *)new_numeric_expression(num_a->val + num_b->val);
        }
        // concat a numeric + string
        else if(b->type == TORAExpressionTypeString)
        {
            result = (void *)new_numeric_expression(num_a->val + atof(string_b->val));
        }
        // undefined behaviour
        else
        {
            TORA_INTERPRETTER_EXCEPTION("Invalid concatination between a numeric and value of type: %i", b->type);
        }
    }
    else
    {
        TORA_INTERPRETTER_EXCEPTION("Invalid concatination between values of type: %i and %i", a->type, b->type);
    }
    
    return result;
}

void free_expression(void *expression)
{
    assert(expression);
    
    TORAParserUnknownExpression *unknown_expression = (TORAParserUnknownExpression *)expression;
    switch(unknown_expression->type)
    {
        case TORAExpressionTypeProg:
        {
            TORAParserProgExpression *prog_expression = (TORAParserProgExpression *)expression;
            tora_release(prog_expression->val);
            prog_expression->val = NULL;
        }
            break;
        case TORAExpressionTypeReturn:
        {
            TORAParserReturnExpression *return_expression = (TORAParserReturnExpression *)expression;
            tora_release(return_expression->expression);
            return_expression->expression = NULL;
        }
            break;
        case TORAExpressionTypeArray:
        {
            TORAParserArrayExpression *array_expression = (TORAParserArrayExpression *)expression;
            if(array_expression->val)
            {
                tora_release(array_expression->val);
                array_expression->val = NULL;
            }
        }
            break;
        case TORAExpressionTypeArrayIndex:
        {
            TORAParserArrayIndexExpression *array_expression = (TORAParserArrayIndexExpression *)expression;
            tora_release(array_expression->index);
            array_expression->index = NULL;
            
            tora_release(array_expression->array);
            array_expression->array = NULL;
        }
            break;
        case TORAExpressionTypeLambda:
        {
            TORAParserLambdaExpression *function_expression = (TORAParserLambdaExpression *)expression;
            if(function_expression->name)
            {
                tora_free(function_expression->name);
                function_expression->name = NULL;
            }
            
            if(function_expression->arguments)
            {
                tora_release(function_expression->arguments);
                function_expression->arguments = NULL;
            }
            
            tora_release(function_expression->body);
        }
            break;
        case TORAExpressionTypeCall:
        {
            TORAParserCallExpression *call_expression = (TORAParserCallExpression *)expression;
            tora_release(call_expression->func);
            call_expression->func = NULL;
            
            if(call_expression->arguments)
            {
                tora_release(call_expression->arguments);
                call_expression->arguments = NULL;
            }
        }
            break;
        case TORAExpressionTypeWhile:
        {
            TORAParserWhileExpression *if_expression = (TORAParserWhileExpression *)expression;
            tora_release(if_expression->condition);
            if_expression->condition = NULL;
            
            tora_release(if_expression->body);
            if_expression->body = NULL;
        }
            break;
        case TORAExpressionTypeIfThenElse:
        {
            TORAParserIfThenElseExpression *if_expression = (TORAParserIfThenElseExpression *)expression;
            tora_release(if_expression->condition);
            if_expression->condition = NULL;
            
            tora_release(if_expression->then);
            if_expression->then = NULL;
            
            if(if_expression->el)
            {
                tora_release(if_expression->el);
                if_expression->el = NULL;
            }
        }
            break;
        case TORAExpressionTypeAssign:
        case TORAExpressionTypeBinary:
        {
            TORAParserAssignOrBinaryExpression *assign_expression = (TORAParserAssignOrBinaryExpression *)expression;
            
            tora_release(assign_expression->left);
            assign_expression->left = NULL;
            
            tora_release(assign_expression->right);
            assign_expression->right = NULL;
            
            if(assign_expression->op)
            {
                tora_free(assign_expression->op);
                assign_expression->op = NULL;
            }
        }
            break;
        case TORAExpressionTypeNegativeUnary:
        {
            TORAParserNegativeUnaryExpression *negative_unary = (TORAParserNegativeUnaryExpression *)expression;
            tora_release(negative_unary->expression);
            negative_unary->expression = NULL;
        }
            break;
        case TORAExpressionTypeString:
        {
            TORAParserStringExpression *string_expression = (TORAParserStringExpression *)expression;
            if(string_expression->val)
            {
                tora_free(string_expression->val);
                string_expression->val = NULL;
            }
        }
            break;
        case TORAExpressionTypeVariable:
        {
            TORAParserVariableExpression *variable_expression = (TORAParserVariableExpression *)expression;
            if(variable_expression->val)
            {
                tora_free(variable_expression->val);
                variable_expression->val = NULL;
            }
        }
            break;
        default:
            // STUB
            break;
    }
    
    tora_free(expression);
}

// Helpers
void *string_representation_for_expression(void *expression)
{
    TORAParserUnknownExpression *unknown_expression = (TORAParserUnknownExpression *)expression;
    switch(unknown_expression->type)
    {
        case TORAExpressionTypeString:
            return expression;
            break;
        case TORAExpressionTypeNumeric:
        {
            char val[20];
            snprintf(val, 20, "%f", ((TORAParserNumericExpression *)expression)->val);
            return new_string_expression(val);
        }
        case TORAExpressionTypeArray:
        {
            char val[20];
            snprintf(val, 20, "ARRAY [len:%llu]", linked_list_length(((TORAParserArrayExpression *)expression)->val));
            return new_string_expression(val);
        }
        case TORAExpressionTypeBoolean:
        {
            TORAParserBoolExpression *bool_expression = (TORAParserBoolExpression *)expression;
            return new_string_expression(bool_expression->val ? "true" : "false");
        }
        default:
            return new_string_expression("???");
            break;
    }
    
    return NULL;
}
TORAParserArrayExpression *get_array_for_index_expression(TORAParserArrayIndexExpression *index_expression, TORAEnvironment *environment)
{
    assert(index_expression->array);
    assert(index_expression->type == TORAExpressionTypeArrayIndex);
    
    // In this instance array_expression will either be a variable will evaluate to an array expression
    TORAParserUnknownExpression *array_name_expression = (TORAParserUnknownExpression *)index_expression->array;
    TORAParserArrayExpression *array = NULL;
    if(array_name_expression->type == TORAExpressionTypeVariable)
    {
        TORALinkedList *val = environment_get_var(environment, ((TORAParserVariableExpression *)array_name_expression)->val, true);
        if(val)
        {
            array = (TORAParserArrayExpression *)val->value;
        }
        else
        {
            TORA_INTERPRETTER_EXCEPTION("Undefined array");
        }
    }
    else
    {
        array = evaluate(array_name_expression, environment, NULL);
    }
    
    // TODO: check array type
    return array;
}
TORALinkedList *get_array_value_for_key(TORAParserArrayExpression *array, TORAParserUnknownExpression *key)
{
    if(key->type != TORAExpressionTypeNumeric && key->type != TORAExpressionTypeString)
    {
        TORA_INTERPRETTER_EXCEPTION("Invalid array offset type: %d", key->type);
    }
    
    TORALinkedList *cur_array_val = array->val;
    while(cur_array_val)
    {
        if(cur_array_val->name && expressions_are_equal(cur_array_val->name, key))
        {
            return cur_array_val;
        }
        cur_array_val = cur_array_val->next;
    }
    
    return NULL;
}
// Determines whether an expression (evaluated or otherwise) represents a truthy value
bool expression_is_truthy(TORAParserUnknownExpression *exp)
{
    TORAParserBoolExpression *bool_exp = (TORAParserBoolExpression *)exp;
    return (bool_exp->type == TORAExpressionTypeBoolean && bool_exp->val);
}