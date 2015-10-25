//
//  TORA.c
//  TORA
//
//  Created by Nial on 30/08/2015.
//  Copyright © 2015 Nial. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "tora.h"

E4C_DEFINE_EXCEPTION(ParserException, "Parser Exception.", RuntimeException);
E4C_DEFINE_EXCEPTION(InterpretterException, "Interpretter Exception.", RuntimeException);

regex_t digit_regex;
regex_t op_regex;
regex_t id_regex;
regex_t id_start_regex;
regex_t punctuation_regex;
regex_t whitespace_regex;
regex_t not_newline_regex;

TORALinkedList *token_queue = NULL;
TORALinkedList *environment_queue = NULL;
TORALinkedList *interpretter_queue = NULL;

const char *tora_keywords[] = {
    "if",
    "else",
    "func",
    "true",
    "false",
    "while",
    "return"
};

const TORAOperatorPrecendence tora_operator_precedence[] = {
    { "=", 1 },
    { "||", 2 },
    { "&&", 3 },
    { "<", 7 }, { ">", 7 }, { "<=", 7 }, { ">=", 7 }, { "==", 7 }, { "!=", 7 },
    { "+", 10 }, { "-", 10 },
    { "*", 20 }, { "/", 20 }, { "%", 20 }, { ":", 20 }
};

// General
bool tora_init()
{
    num_malloc = 0;
    num_free = 0;
    
    int r = 0;
    r = regcomp(&digit_regex, "[[:digit:]]", REG_EXTENDED|REG_ICASE);
    if (r)
    {
        return false;
    }
    
    r = regcomp(&op_regex, "\\+|-|\\*|/|%|=|&|\\||<|>|!", REG_EXTENDED|REG_ICASE);
    if (r)
    {
        return false;
    }
    
    r = regcomp(&punctuation_regex, ",|;|:|\\(|\\)|{|}|\\[|\\]", REG_EXTENDED|REG_ICASE);
    if (r)
    {
        return false;
    }
    
    r = regcomp(&whitespace_regex, "[ \n\t]", REG_EXTENDED|REG_NEWLINE|REG_ICASE);
    if (r)
    {
        return false;
    }
    
    r = regcomp(&not_newline_regex, "[^\n]", REG_EXTENDED|REG_NEWLINE|REG_ICASE);
    if (r)
    {
        return false;
    }
    
    // A regex to match the beginning of variable names
    r = regcomp(&id_start_regex, "[a-z_]", REG_EXTENDED|REG_ICASE);
    if (r)
    {
        return false;
    }
    
    // A regex to match the end of variable names
    r = regcomp(&id_regex, "[0-9]", REG_EXTENDED|REG_ICASE);
    if (r)
    {
        return false;
    }
    return true;
}
void tora_shutdown()
{
    regfree(&digit_regex);
    regfree(&op_regex);
    regfree(&punctuation_regex);
    regfree(&whitespace_regex);
    regfree(&not_newline_regex);
    regfree(&id_start_regex);
    regfree(&id_regex);
}

// String helpers
char *tora_strcpy(char *str)
{
    if(str == NULL) return NULL;
    
    char *ret = tora_malloc(strlen(str)+1);
    if(ret != NULL)
    {
        return strcpy(ret, str);
    }
    
    return NULL;
}

// Error handling
void tora_parser_exception(uint64_t line, uint64_t col, char *msg)
{
    char extra[255];
    snprintf(extra, 255, "[line %llu, col: %llu] %s", line, col, msg);
    e4c_throw(&ParserException, __FILE__, __LINE__, extra);
}
char *tora_variadic_string(const char *fmt, ...)
{
    int len = 0, size = 100;
    va_list ap;
    
    char *msg = NULL;
    if((msg = malloc((size_t)size)) == NULL)
    {
        return NULL;
    }
    
    char *tmp_msg = NULL;
    while(1)
    {
        va_start(ap, fmt);
        len = vsnprintf(msg, size, fmt, ap);
        va_end(ap);
        
        // If we've now malloc'd enough space, we can ditch out
        if(len > -1 && len < size) break;
        
        // Otherwise, try resizing our new_msg
        size = (len > -1) ? len+1 : size*2;
        if ((tmp_msg = realloc(msg, (size_t)size)) == NULL)
        {
            free(msg);
            return NULL;
        }
        else
        {
            msg = tmp_msg;
        }
    }
    
    return msg;
}

// Syntax helpers
int get_operator_precendence(char *cmp)
{
    int i = 0;
    for(i = 0; i < TORA_NUM_OPERATORS; i++)
    {
        const TORAOperatorPrecendence current_operator = tora_operator_precedence[i];
        if(!strcmp(current_operator.op, cmp))
        {
            return current_operator.level;
        }
    }
    
    return -1;
}
bool is_keyword(char *keyword)
{
    int i = 0;
    for(i = 0; i < TORA_NUM_KEYWORDS; i++)
    {
        if(strlen(keyword) == strlen(tora_keywords[i]) && strncmp(keyword, tora_keywords[i], strlen(keyword)) == 0)
        {
            return true;
        }
    }
    
    return false;
}
bool is_digit(char ch, char *value)
{
    TORA_UNUSED(value);
    char test_value[2] = {ch, '\0'};
    int r = regexec(&digit_regex, test_value, 0, NULL, 0);
    return r == 0;
}
bool is_numeric_value(char ch, char *value)
{
    assert(ch);
    assert(value);
    
    char test_value[2] = {ch, '\0'};
    
    // dots are valid for numeric values, but only if we don't already have one
    if(test_value[0] == '.')
    {
        char *existing_dot = strchr(value, '.');
        if(existing_dot)
        {
            return false;
        }
        return true;
    }
    else
    {
        return is_digit(ch, value);
    }
}
bool is_op_char(char ch, char *value)
{
    TORA_UNUSED(value);
    char test_value[2] = {ch, '\0'};
    int r = regexec(&op_regex, test_value, 0, NULL, 0);
    return r == 0;
}
bool is_id_start(char ch, char *value)
{
    TORA_UNUSED(value);
    char test_value[2] = {ch, '\0'};
    int r = regexec(&id_start_regex, test_value, 0, NULL, 0);
    return r == 0;
}
bool is_id(char ch, char *value)
{
    TORA_UNUSED(value);
    char test_value[2] = {ch, '\0'};
    return is_id_start(ch, value) || regexec(&id_regex, test_value, 0, NULL, 0) == 0;
}
bool is_punctuation(char ch, char *value)
{
    TORA_UNUSED(value);
    char test_value[2] = {ch, '\0'};
    int r = regexec(&punctuation_regex, test_value, 0, NULL, 0);
    return r == 0;
}
bool is_whitespace(char ch, char *value)
{
    TORA_UNUSED(value);
    char test_value[2] = {ch, '\0'};
    int r = regexec(&whitespace_regex, test_value, 0, NULL, 0);
    return r == 0;
}
bool is_not_newline(char ch, char *value)
{
    TORA_UNUSED(value);
    char test_value[2] = {ch, '\0'};
    int r = regexec(&not_newline_regex, test_value, 0, NULL, 0);
    return r == 0;
}