//
//  char_stream.h
//  TORA
//
//  Created by Nial on 01/09/2015.
//  Copyright (c) 2015 Nial. All rights reserved.
//

#ifndef __tora__char_stream__
#define __tora__char_stream__

#include <stdio.h>
#include <stdbool.h>

typedef struct {
    uint64_t pos;
    uint64_t line;
    uint64_t col;
    unsigned long length;
    const char *contents;
} TORAInputStream;

TORAInputStream *input_stream_from_file_contents(const char *filename);
char input_stream_next_char(TORAInputStream *stream);
char input_stream_peek(TORAInputStream *stream);
bool input_stream_eof(TORAInputStream *stream);
void free_input_stream(TORAInputStream *stream);

#endif /* defined(__tora__char_stream__) */
