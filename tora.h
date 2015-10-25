
//
//  config.h
//  TORA
//
//  Created by Nial on 30/08/2015.
//  Copyright © 2015 Nial. All rights reserved.
//

#ifndef tora_h
#define tora_h

#include <stdbool.h>
#include <regex.h>
#include <assert.h>

#include "lib/e4c_lite.h"
#include "structures.h"
#include "input_stream.h"
#include "token_stream.h"
#include "parser.h"
#include "interpretter.h"

E4C_DECLARE_EXCEPTION(ParserException);
E4C_DECLARE_EXCEPTION(InterpretterException);

#define TORA_UNUSED(expr) (void)(expr);
#define TORA_RUNTIME_EXCEPTION(fmt, ...) e4c_throw(&RuntimeException, __FILE__, __LINE__, tora_variadic_string(fmt, ##__VA_ARGS__))
#define TORA_PARSER_EXCEPTION(line, pos, fmt, ...) tora_parser_exception(line, pos, tora_variadic_string(fmt, ##__VA_ARGS__))
#define TORA_INTERPRETTER_EXCEPTION(fmt, ...) e4c_throw(&InterpretterException, __FILE__, __LINE__, tora_variadic_string(fmt, ##__VA_ARGS__))

#define TORA_NUM_KEYWORDS 7
#define TORA_NUM_OPERATORS 15

typedef struct {
    char *op;
    int level;
} TORAOperatorPrecendence;

extern const TORAOperatorPrecendence tora_operator_precedence[];
extern const char *tora_keywords[];

extern TORALinkedList *token_queue;
extern TORALinkedList *environment_queue;
extern TORALinkedList *interpretter_queue;

extern regex_t digit_regex;
extern regex_t op_regex;
extern regex_t id_regex;
extern regex_t id_start_regex;
extern regex_t punctuation_regex;
extern regex_t whitespace_regex;
extern regex_t not_newline_regex;

bool tora_init();
void tora_shutdown();

// Error handling
void tora_parser_exception(uint64_t line, uint64_t col, char *msg);
char *tora_variadic_string(const char *fmt, ...);

// Helpers
int get_operator_precendence(char *cmp);
bool is_keyword(char *keyword);
bool is_digit(char ch, char *value);
bool is_numeric_value(char ch, char *value);
bool is_id_start(char ch, char *value);
bool is_id(char ch, char *value);
bool is_op_char(char ch, char *value);
bool is_punctuation(char ch, char *value);
bool is_whitespace(char ch, char *value);
bool is_not_newline(char ch, char *value);

#endif /* tora_h */
