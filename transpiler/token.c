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
    if (ts->count >= ts->capacity){
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
    return make_token(TOKEN_EOF, "");
}

Token peek_ts(TokenStream *ts)
{
    if (ts->pos < ts->count)
        return ts->data[ts->pos];
    return make_token(TOKEN_EOF, "");
}

Token expect(TokenStream *ts, TokenType type)
{
    Token t = get_from_ts(ts);
    if (t.type != type) {
        fprintf(stderr, "Parse error: expected %d, got %d (%s)\n", type, t.type, t.value);
        exit(1);
    }
    return t;
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

const char *token_type_to_str(TokenType type)
{
    switch (type) {
        case TOKEN_KEYWORD:    return "KEYWORD";
        case TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TOKEN_SYMBOL:     return "SYMBOL";
        case TOKEN_NUMBER:     return "NUMBER";
        case TOKEN_STRING:     return "STRING";
        case TOKEN_CHAR:       return "CHAR";
        case TOKEN_OPERATOR:   return "OPERATOR";
        case TOKEN_ASSIGN:     return "ASSIGN";
        case TOKEN_PLUS:       return "PLUS";
        case TOKEN_MINUS:      return "MINUS";
        case TOKEN_PUNCT:      return "PUNCT";
        case TOKEN_COLON:      return "COLON";
        case TOKEN_UNKNOWN:    return "UNKNOWN";
        case TOKEN_INDENT:     return "INDENT";
        case TOKEN_DEDENT:     return "DEDENT";
        case TOKEN_NEWLINE:    return "NEWLINE";
        case TOKEN_LPAREN:     return "LPAREN";
        case TOKEN_RPAREN:     return "RPAREN";
        case TOKEN_COMMA:      return "COMMA";
        case TOKEN_RARROW:     return "RARROW";
        case TOKEN_NULL:       return "NULL";
        case TOKEN_EOF:        return "EOF";
        default:               return "UNSET";
    }
}

void print_token(Token tok)
{
    printf("(%s, \"%s\")\n", token_type_to_str(tok.type), tok.value);
}

void reset_ts(TokenStream *ts) { ts->pos = 0; }

void debug_print_ts(const TokenStream *ts)
{
    for (int i = 0; i < ts->count; ++i) {
        print_token(ts->data[i]);
    }
}
