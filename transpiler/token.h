#ifndef TOKEN_H
#define TOKEN_H

#include <stdio.h>

typedef enum {
    TOKEN_KEYWORD,
    TOKEN_IDENTIFIER,
    TOKEN_SYMBOL,
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_CHAR,
    TOKEN_OPERATOR,
    TOKEN_ASSIGN,
    TOKEN_PLUS, TOKEN_MINUS,
    TOKEN_PUNCT,
    TOKEN_COLON,
    TOKEN_UNKNOWN,
    TOKEN_INDENT,
    TOKEN_DEDENT,
    TOKEN_NEWLINE,
    TOKEN_LPAREN, TOKEN_RPAREN,
    TOKEN_COMMA,
    TOKEN_RARROW,
    TOKEN_NULL,
    TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    char value[64];
} Token;

typedef struct {
    Token* data;
    int count;
    int capacity;
    int pos;
} TokenStream;

Token make_token(TokenType type, const char *value);
TokenStream *create_token_stream(void);
void add_token_to_ts(TokenStream *ts, Token tok);
Token peek_ts(TokenStream *ts);
Token expect(TokenStream *ts, TokenType type);
Token get_from_ts(TokenStream *ts);
void advance_ts(TokenStream *ts);
void free_token_stream(TokenStream *ts);
const char *token_type_to_str(TokenType type);
void print_token(Token tok);
void reset_ts(TokenStream *ts);
void debug_print_ts(const TokenStream *ts);

#endif
