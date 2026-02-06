#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h> 


extern int start_program_shell(int argc, char *argv[]); 

int main(int argc, char *argv[])
{
    return start_program_shell(argc, argv);
}

tokenlist *new_tokenlist(void) {
    tokenlist *tokens = (tokenlist *)malloc(sizeof(tokenlist));
    if (!tokens) { perror("Failed to allocate tokenlist"); exit(EXIT_FAILURE); }
    
    tokens->size = 0;
    tokens->items = (char **)malloc(sizeof(char *)); 
    if (!tokens->items) { perror("Failed to allocate token array"); exit(EXIT_FAILURE); }
    tokens->items[0] = NULL; 
    return tokens;
}

void add_token(tokenlist *tokens, char *item) {
    size_t i = tokens->size;

    tokens->items = (char **)realloc(tokens->items, (i + 2) * sizeof(char *)); 
    if (!tokens->items) { perror("Failed to reallocate token array"); exit(EXIT_FAILURE); }
    
    tokens->items[i] = (char *)malloc(strlen(item) + 1);
    if (!tokens->items[i]) { perror("Failed to allocate token string"); exit(EXIT_FAILURE); }
    strcpy(tokens->items[i], item);

    tokens->items[i + 1] = NULL; 

    tokens->size += 1;
}

tokenlist *get_tokens(char *input) {
    char *buf = (char *)malloc(strlen(input) + 1);
    if (!buf) { perror("Failed to allocate buffer"); exit(EXIT_FAILURE); }
    strcpy(buf, input); 
    
    tokenlist *tokens_list = new_tokenlist();
    
    char *tok = strtok(buf, " ");
    while (tok != NULL)
    {
        add_token(tokens_list, tok);
        tok = strtok(NULL, " ");
    }
    
    free(buf); //free the duplicate input buffer
    
    return tokens_list;
}
void free_tokens(tokenlist *tokens) {
    if (!tokens) return;
    for (size_t i = 0; i < tokens->size; i++) {
        if (tokens->items[i]) {
            free(tokens->items[i]);
        }
    }
    free(tokens->items);
    free(tokens);
}