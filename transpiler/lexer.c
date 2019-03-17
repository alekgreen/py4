// simple lexer

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
    }
    printf("\"%s\")\n", tok.value);
}

int fpeek(FILE *stream)
{
    int c = fgetc(stream);
    ungetc(c, stream);
    return c;
}

Token make_token(TokenType type, const char *value)
{
    Token t;
    t.type = type;
    strncpy(t.value, value, sizeof(t.value) - 1);
    t.value[sizeof(t.value) - 1] = '\0';
    return t;
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
        while (is_letter(fpeek(fp))) {
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

void lexer(FILE *fp)
{   
    Token *tokens = malloc(sizeof(Token) * 100);
    Token curr;
    int i = 0;
    while (1) {
        if (i >= 100) break;
        curr = next_token(fp);
        tokens[i] = curr;
        printToken(tokens[i]);
        i++;
        if (curr.type == TOKEN_EOF) break;
    }
    free(tokens);
}


int main() 
{
    FILE *fp;

    fp = fopen("example1.p4", "r");

    lexer(fp);

    fclose(fp);

    return 0;
}