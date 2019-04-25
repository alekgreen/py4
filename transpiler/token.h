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
    TOKEN_LBRACKET, TOKEN_RBRACKET,
    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_RARROW,
    TOKEN_NULL,
    TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    char value[64];
    const char *path;
    const char *line_text;
    int line;
    int column;
} Token;

typedef struct {
    Token* data;
    int count;
    int capacity;
    int pos;
    char *source_path;
    char **source_lines;
    int line_count;
    int line_capacity;
} TokenStream;

Token make_token(TokenType type, const char *value);
Token make_token_at(
    TokenType type,
    const char *value,
    const char *path,
    int line,
    int column,
    const char *line_text);
TokenStream *create_token_stream(const char *source_path);
void add_source_line(TokenStream *ts, const char *line_text);
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
void print_source_diagnostic(
    FILE *stream,
    const char *path,
    int line,
    int column,
    const char *kind,
    const char *message,
    const char *line_text);
void print_token_diagnostic(FILE *stream, Token tok, const char *kind, const char *message);

#endif
