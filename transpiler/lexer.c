#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "token.h"

const char *KEYWORDS[] = {
    "def",
    "int",
    "print",
    "None",
    NULL
};
const char *OPERATORS[] = { "+", "-", "*", "/", "=", "!", "==", "!=", "<", ">", "<=", ">=", NULL };
const char *PUNCTS[]    = { "(", ")", "{", "}", "[", "]", ":", ",", ".", NULL };

int fpeek(FILE *stream)
{
    int c = fgetc(stream);
    ungetc(c, stream);
    return c;
}

int is_keyword(const char *lexeme)
{
    for (int i = 0; KEYWORDS[i] != NULL; i++){
        if (strcmp(lexeme, KEYWORDS[i]) == 0)
            return 1;
    }
    return 0;
}

int is_operator(const char *lexeme)
{
    for (int i = 0; OPERATORS[i] != NULL; i++){
        if (strcmp(lexeme, OPERATORS[i]) == 0)
            return 1;
    }
    return 0;
}

int is_puncts(const char *lexeme)
{
    for (int i = 0; PUNCTS[i] != NULL; i++){
        if (strcmp(lexeme, PUNCTS[i]) == 0)
            return 1;
    }
    return 0;
}

int is_special_char(char c)
{
    return strchr("+-*/=(){}[];,<>!:", c) != NULL;
}

int is_letter(char c)
{
    return (isalpha((unsigned char)c) || c == '_');
}

int is_digit(char c)
{
    return isdigit((unsigned char)c);
}

Token next_token(FILE *fp)
{
    int c;

    do {
        c = fgetc(fp);
    } while (c == ' ');

    int i = 1;
    char buffer[64];
    buffer[0] = c;

    if (c == EOF) return make_token(TOKEN_EOF, "");
    if (c == '\n') return make_token(TOKEN_NEWLINE, "\\n");
    if (c == '\t') return make_token(TOKEN_INDENT, "\\t");

    if (is_letter(c)) {
        while (is_letter(fpeek(fp)) || is_digit(fpeek(fp))) {
            c = fgetc(fp);
            buffer[i] = c;
            i++;
        }
        buffer[i] = '\0';
        if (is_keyword(buffer))
            return make_token(TOKEN_KEYWORD, buffer);
        return make_token(TOKEN_IDENTIFIER, buffer);
    }

    if (is_digit(c)) {
        while (is_digit(fpeek(fp))) {
            c = fgetc(fp);
            buffer[i] = c;
            i++;
        }
        buffer[i] = '\0';
        return make_token(TOKEN_NUMBER, buffer);
    }

    if (is_special_char(c)) {
        switch (c) {
            case '=':
                if (fpeek(fp) == '=') {
                    c = fgetc(fp);
                    buffer[i] = c;
                    i++;
                    buffer[i] = '\0';
                    return make_token(TOKEN_OPERATOR, buffer);
                }
                buffer[i] = '\0';
                return make_token(TOKEN_ASSIGN, buffer);
            case '+':
                buffer[i] = '\0';
                return make_token(TOKEN_PLUS, buffer);
            case ':':
                buffer[i] = '\0';
                return make_token(TOKEN_COLON, buffer);
            case '(':
                buffer[i] = '\0';
                return make_token(TOKEN_LPAREN, buffer);
            case ')':
                buffer[i] = '\0';
                return make_token(TOKEN_RPAREN, buffer);
            case '-':
                if (fpeek(fp) == '>'){
                    c = fgetc(fp);
                    buffer[i] = c;
                    i++;
                    buffer[i] = '\0';
                    return make_token(TOKEN_RARROW, buffer);
                }
                buffer[i] = '\0';
                return make_token(TOKEN_MINUS, buffer);
            default:
                break;
        }
        
        
    }

    buffer[i] = '\0';
    
    return make_token(TOKEN_SYMBOL, buffer);
}

TokenStream *lexer(FILE *fp)
{   
    TokenStream *ts = create_token_stream();
    Token curr;
    while (1) {
        curr = next_token(fp);
        add_token_to_ts(ts, curr);
        if (curr.type == TOKEN_EOF) break;
    }
    // while (ts->pos < ts->count) {
    //     print_token(get_from_ts(ts));
    // }
    return ts;
}
