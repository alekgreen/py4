#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "token.h"

TokenStream *create_token_stream(void)
{
    TokenStream *ts = malloc(sizeof(TokenStream));
    if (!ts) {
        perror("malloc TokenStream");
        exit(1);
    }
    ts->count = 0;
    ts->capacity = 128;
    ts->pos = 0;
    ts->data = malloc(sizeof(Token) * ts->capacity);
    if (!ts->data) {
        perror("malloc TokenStream.data");
        exit(1);
    }
    return ts;
}

Token make_token(TokenType type, const char *value)
{
    Token t = {0};
    t.type = type;
    if (value) {
        strncpy(t.value, value, sizeof(t.value) - 1);
        t.value[sizeof(t.value) - 1] = '\0';
    } else {
        t.value[0] = '\0';
    }
    return t;
}

void add_token_to_ts(TokenStream *ts, Token tok)
{
    if (ts->capacity >= ts->count){
        ts->capacity *= 2;
        ts->data = realloc(ts->data, sizeof(Token) * ts->capacity);
        if (!ts->data) {
            perror("realloc TokenStream.data");
            exit(1);
        }
    }
    ts->data[ts->count++] = tok;
}

Token get_from_ts(TokenStream *ts)
{   
    if (ts->pos < ts->count)
        return ts->data[ts->pos++];
    return make_token(TOKEN_NULL, "");
}

Token peek_ts(TokenStream *ts)
{
    if (ts->pos < ts->count)
        return ts->data[ts->pos];
    return make_token(TOKEN_NULL, "");
}

void advance_ts(TokenStream *ts)
{
    if (ts->pos < ts->count)
        ts->pos++;
}

void free_token_stream(TokenStream *ts)
{
    if (ts) {
        free(ts->data);
        free(ts);
    }
}

void printToken(Token tok)
{
    switch(tok.type) {
        case TOKEN_KEYWORD: printf("(KEYWORD, "); break;
        case TOKEN_IDENTIFIER: printf("(IDENTIFIER, "); break;
        case TOKEN_SYMBOL: printf("(SYMBOL, "); break;
        case TOKEN_NUMBER: printf("(NUMBER, "); break;
        case TOKEN_OPERATOR: printf("(OPERATOR, "); break;
        case TOKEN_ASSIGN: printf("(ASSIGN, "); break;
        case TOKEN_PLUS: printf("(PLUS, "); break;
        case TOKEN_MINUS: printf("(MINUS, "); break;
        case TOKEN_PUNCT: printf("(PUNCT, "); break;
        case TOKEN_COLON: printf("(COLON, "); break;
        case TOKEN_UNKNOWN: printf("(UNKNOWN, "); break;
        case TOKEN_INDENT: printf("(INDENT, "); break;
        case TOKEN_DEDENT: printf("(DEDENT, "); break;
        case TOKEN_NEWLINE: printf("(NEWLINE, "); break;
        case TOKEN_LPAREN: printf("(LPAREN, "); break;
        case TOKEN_RPAREN: printf("(RPAREN, "); break;
        case TOKEN_RARROW: printf("(RARROW, "); break;
        case TOKEN_EOF: printf("(EOF, "); break;
        case TOKEN_NULL: printf("(NULL, "); break;
    }
    printf("\"%s\")\n", tok.value);
}