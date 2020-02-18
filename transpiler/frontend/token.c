#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "token.h"

static char *dup_string(const char *value)
{
    size_t len;
    char *copy;

    if (value == NULL) {
        return NULL;
    }

    len = strlen(value) + 1;
    copy = malloc(len);
    if (copy == NULL) {
        perror("malloc");
        exit(1);
    }

    memcpy(copy, value, len);
    return copy;
}

TokenStream *create_token_stream(const char *source_path)
{
    TokenStream *ts = malloc(sizeof(TokenStream));
    if (!ts) {
        perror("malloc TokenStream");
        exit(1);
    }
    ts->count = 0;
    ts->capacity = 128;
    ts->pos = 0;
    ts->source_path = dup_string(source_path);
    ts->source_lines = NULL;
    ts->line_count = 0;
    ts->line_capacity = 0;
    ts->data = malloc(sizeof(Token) * ts->capacity);
    if (!ts->data) {
        perror("malloc TokenStream.data");
        exit(1);
    }
    return ts;
}

Token make_token(TokenType type, const char *value)
{
    return make_token_at(type, value, NULL, 0, 0, NULL);
}

Token make_token_at(
    TokenType type,
    const char *value,
    const char *path,
    int line,
    int column,
    const char *line_text)
{
    Token t = {0};
    t.type = type;
    t.path = path;
    t.line_text = line_text;
    t.line = line;
    t.column = column;
    if (value) {
        strncpy(t.value, value, sizeof(t.value) - 1);
        t.value[sizeof(t.value) - 1] = '\0';
    } else {
        t.value[0] = '\0';
    }
    return t;
}

void add_source_line(TokenStream *ts, const char *line_text)
{
    if (ts->line_count >= ts->line_capacity) {
        int next_capacity = ts->line_capacity == 0 ? 32 : ts->line_capacity * 2;
        char **lines = realloc(ts->source_lines, sizeof(char *) * next_capacity);

        if (lines == NULL) {
            perror("realloc TokenStream.source_lines");
            exit(1);
        }

        ts->source_lines = lines;
        ts->line_capacity = next_capacity;
    }

    ts->source_lines[ts->line_count++] = dup_string(line_text);
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
    char message[128];

    if (t.type != type) {
        snprintf(message, sizeof(message),
            "expected %s, got %s ('%s')",
            token_type_to_str(type),
            token_type_to_str(t.type),
            t.value);
        print_token_diagnostic(stderr, t, "Parse error", message);
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
        for (int i = 0; i < ts->line_count; i++) {
            free(ts->source_lines[i]);
        }
        free(ts->source_lines);
        free(ts->source_path);
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
        case TOKEN_LBRACKET:   return "LBRACKET";
        case TOKEN_RBRACKET:   return "RBRACKET";
        case TOKEN_LBRACE:     return "LBRACE";
        case TOKEN_RBRACE:     return "RBRACE";
        case TOKEN_COMMA:      return "COMMA";
        case TOKEN_DOT:        return "DOT";
        case TOKEN_RARROW:     return "RARROW";
        case TOKEN_NULL:       return "NULL";
        case TOKEN_EOF:        return "EOF";
        default:               return "UNSET";
    }
}

void print_token(Token tok)
{
    if (tok.line > 0) {
        printf("(%s, \"%s\", %s:%d:%d)\n",
            token_type_to_str(tok.type),
            tok.value,
            tok.path != NULL ? tok.path : "<unknown>",
            tok.line,
            tok.column);
    } else {
        printf("(%s, \"%s\")\n", token_type_to_str(tok.type), tok.value);
    }
}

void reset_ts(TokenStream *ts) { ts->pos = 0; }

void debug_print_ts(const TokenStream *ts)
{
    for (int i = 0; i < ts->count; ++i) {
        print_token(ts->data[i]);
    }
}

void print_source_diagnostic(
    FILE *stream,
    const char *path,
    int line,
    int column,
    const char *kind,
    const char *message,
    const char *line_text)
{
    int caret_column;

    if (path == NULL || line <= 0 || column <= 0) {
        print_basic_diagnostic(stream, path, kind, message);
        return;
    }

    fprintf(stream, "%s:%d:%d: %s: %s\n", path, line, column, kind, message);

    if (line_text == NULL || line_text[0] == '\0') {
        return;
    }

    fprintf(stream, "    %s\n", line_text);
    fprintf(stream, "    ");
    caret_column = column;
    if (caret_column < 1) {
        caret_column = 1;
    }
    for (int i = 1; i < caret_column; i++) {
        fputc(line_text[i - 1] == '\t' ? '\t' : ' ', stream);
    }
    fprintf(stream, "^\n");
}

void print_basic_diagnostic(FILE *stream, const char *path, const char *kind, const char *message)
{
    if (path != NULL && path[0] != '\0') {
        fprintf(stream, "%s: %s: %s\n", path, kind, message);
        return;
    }

    fprintf(stream, "%s: %s\n", kind, message);
}

void print_token_diagnostic(FILE *stream, Token tok, const char *kind, const char *message)
{
    print_source_diagnostic(
        stream,
        tok.path,
        tok.line,
        tok.column,
        kind,
        message,
        tok.line_text);
}
