//
//  main.c
//  TORA
//
//  Created by Nial on 30/08/2015.
//  Copyright © 2015 Nial. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include "tora.h"

int main(int argc, const char * argv[])
{
    try
    {
        if(argc < 2)
        {
            printf("Please provide a .tora file to parse!\n");
            exit(1);
        }

        if(!tora_init())
        {
            TORA_RUNTIME_EXCEPTION("Failed to initialise parser!");
        }
        
        // Create an input stream to begin parsing our document
        TORAInputStream *input_stream = input_stream_from_file_contents(argv[1]);
        if(!input_stream)
        {
            TORA_RUNTIME_EXCEPTION("Failed to parse input stream!");
        }
        
        // Create a token stream from our TORAInputStream
        TORATokenStream *token_stream = token_stream_from_input(input_stream);
        if(!token_stream)
        {
            free_input_stream(input_stream);
            
            TORA_RUNTIME_EXCEPTION("Failed to malloc input stream!");
        }
        
        // Parse the top level of our program and generate an AST
        TORAParserProgExpression *prog = parse_top_level(token_stream);
        if(!prog)
        {
            free_token_stream(token_stream);
            free_input_stream(input_stream);
            
            TORA_RUNTIME_EXCEPTION("Failed to generate expression tree!");
        }
        
        // Create a base environment for use when evaluating the AST
        TORAEnvironment *environment = create_environment(NULL);
        if(!environment)
        {
            free_expression(prog);
            free_token_stream(token_stream);
            free_input_stream(input_stream);
            
            TORA_RUNTIME_EXCEPTION("Failed to malloc root environment!");
        }
        
        // Evaluate the program!
        bool return_encountered = false;
        evaluate(prog, environment, &return_encountered);
        
        // Cleanup
        free_expression(prog);
        drain_queue(environment_queue);
        drain_queue(interpretter_queue);
        
        free_token_stream(token_stream);
        free_input_stream(input_stream);
        
        if(num_malloc != num_free)
        {
            printf("Num malloc'd blocks: %i, num freed: %i, lost: %i\n", num_malloc, num_free, num_malloc-num_free);
            printf("Num un-freed tokens: %i\n", num_tok);
        }
        
        // Teardown
        tora_shutdown();
    }
    catch(ParserException)
    {
        printf("Parser Exception: \"%s\"\n\tfile: %s\n\tline: %d\n", e4c.err.message, e4c.err.file, e4c.err.line);
    }
    catch(InterpretterException)
    {
        printf("Interpretter Exception: \"%s\"\n\tfile: %s\n\tline: %d\n", e4c.err.message, e4c.err.file, e4c.err.line);
    }
    catch(RuntimeException)
    {
        printf("Runtime Exception: \"%s\"\n\tfile: %s\n\tline: %d\n", e4c.err.message, e4c.err.file, e4c.err.line);
    }
    
    return 0;
}