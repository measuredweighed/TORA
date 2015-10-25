//
//  input_stream.c
//  TORA
//
//  Created by Nial on 01/09/2015.
//  Copyright (c) 2015 Nial. All rights reserved.
//

#include <stdlib.h>
#include <inttypes.h>

#include "tora.h"

char *get_file_contents(const char *filename, unsigned long *length);

// Input stream
TORAInputStream *input_stream_from_file_contents(const char *filename)
{
    unsigned long contents_length = 0;
    
    const char *contents = get_file_contents(filename, &contents_length);
    if(contents)
    {
        TORAInputStream *stream = tora_malloc(sizeof(TORAInputStream));
        if(stream)
        {
            stream->pos = 0;
            stream->line = 1;
            stream->col = 0;
            stream->contents = contents;
            stream->length = contents_length;
            
            return stream;
        }
    }
    return NULL;
}

// Internal logic
char input_stream_next_char(TORAInputStream *stream)
{
    assert(stream);
    assert(stream->contents);
    
    char next_char = stream->contents[stream->pos];
    if(next_char == '\n')
    {
        stream->line++;
        stream->col = 0;
    }
    else
    {
        stream->col++;
    }
    stream->pos++;
    
    return next_char;
}
char input_stream_peek(TORAInputStream *stream)
{
    return stream->contents[stream->pos];
}
bool input_stream_eof(TORAInputStream *stream)
{
    return stream->pos >= stream->length;
}

// Helpers
void free_input_stream(TORAInputStream *stream)
{
    tora_free((void *)stream->contents);
    tora_free(stream);
}
char *get_file_contents(const char *filename, unsigned long *length)
{
    char *buffer = NULL;
    
    FILE *fp = fopen(filename, "rb");
    if(fp)
    {
        fseek(fp, 0, SEEK_END);
        *length = (unsigned long)ftell(fp);
        fseek(fp, 0, SEEK_SET);
        
        buffer = tora_malloc(*length * sizeof(char));
        if(!buffer)
        {
            TORA_RUNTIME_EXCEPTION("Failed to malloc space for source file contents");
        }
        fread(buffer, 1, *length, fp);
        fclose(fp);
    }
    
    return buffer;
}