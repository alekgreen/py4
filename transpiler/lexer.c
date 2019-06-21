#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "token.h"

#define INDENT_WIDTH 4
#define MAX_LINE_LEN 4096
#define MAX_INDENT_LEVELS 256

static const char *KEYWORDS[] = {
    "def",
    "return",
    "break",
    "continue",
    "import",
    "class",
    "if",
    "while",
    "for",
    "in",
    "elif",
    "else",
    "and",
    "or",
    "not",
    "int",
    "float",
    "bool",
    "char",
    "str",
    "string",
    "list",
    "dict",
    "None",
    "True",
    "False",
    NULL
};

static int fpeek(FILE *stream)
{
    int c = fgetc(stream);
    if (c != EOF) {
        ungetc(c, stream);
    }
    return c;
}

static int is_keyword(const char *lexeme)
{
    for (int i = 0; KEYWORDS[i] != NULL; i++) {
        if (strcmp(lexeme, KEYWORDS[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int is_letter(char c)
{
    return isalpha((unsigned char)c) || c == '_';
}

static int is_digit(char c)
{
    return isdigit((unsigned char)c);
}

static int is_blank_line(const char *line)
{
    while (*line != '\0') {
        if (*line == '#') {
            return 1;
        }
        if (*line != ' ' && *line != '\t' && *line != '\r' && *line != '\n') {
            return 0;
        }
        line++;
    }
    return 1;
}

static void trim_line_ending(char *line)
{
    size_t len = strlen(line);

    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[--len] = '\0';
    }
}

static void append_token(
    TokenStream *ts,
    TokenType type,
    const char *value,
    int line,
    int column,
    const char *line_text)
{
    add_token_to_ts(ts, make_token_at(type, value, ts->source_path, line, column, line_text));
}

static void add_indent_tokens(
    TokenStream *ts,
    int *indent_stack,
    int *depth,
    int indent,
    int line,
    const char *line_text)
{
    int current = indent_stack[*depth];

    if (indent > current) {
        if (*depth + 1 >= MAX_INDENT_LEVELS) {
            fprintf(stderr, "Lexer error: indentation nesting too deep\n");
            exit(1);
        }
        indent_stack[++(*depth)] = indent;
        append_token(ts, TOKEN_INDENT, "<INDENT>", line, 1, line_text);
        return;
    }

    while (indent < current) {
        (*depth)--;
        current = indent_stack[*depth];
        append_token(ts, TOKEN_DEDENT, "<DEDENT>", line, 1, line_text);
    }

    if (indent != current) {
        fprintf(stderr, "Lexer error: inconsistent dedent to column %d\n", indent);
        exit(1);
    }
}

static int consume_indent(const char **cursor)
{
    int indent = 0;

    while (**cursor == ' ' || **cursor == '\t') {
        if (**cursor == ' ') {
            indent++;
        } else {
            indent += INDENT_WIDTH;
        }
        (*cursor)++;
    }

    return indent;
}

static void push_buffered_token(
    TokenStream *ts,
    TokenType type,
    const char *start,
    size_t len,
    int line,
    int column,
    const char *line_text)
{
    char buffer[64];

    if (len >= sizeof(buffer)) {
        fprintf(stderr, "Lexer error: token too long\n");
        exit(1);
    }

    memcpy(buffer, start, len);
    buffer[len] = '\0';
    append_token(ts, type, buffer, line, column, line_text);
}

static void push_quoted_token(
    TokenStream *ts,
    TokenType type,
    const char **cursor,
    const char *line_start,
    int line,
    const char *line_text,
    char quote)
{
    char buffer[64];
    size_t len = 0;
    int column = (int)(*cursor - line_start) + 1;

    buffer[len++] = **cursor;
    (*cursor)++;

    while (**cursor != '\0' && **cursor != '\r' && **cursor != '\n') {
        if (len + 2 >= sizeof(buffer)) {
            fprintf(stderr, "Lexer error: string literal too long\n");
            exit(1);
        }

        buffer[len++] = **cursor;
        if (**cursor == '\\') {
            (*cursor)++;
            if (**cursor == '\0' || **cursor == '\r' || **cursor == '\n') {
                fprintf(stderr, "Lexer error: unterminated escape sequence\n");
                exit(1);
            }
            buffer[len++] = **cursor;
            (*cursor)++;
            continue;
        }
        if (**cursor == quote) {
            buffer[len] = '\0';
            (*cursor)++;
            append_token(ts, type, buffer, line, column, line_text);
            return;
        }
        (*cursor)++;
    }

    fprintf(stderr, "Lexer error: unterminated quoted literal\n");
    exit(1);
}

static void tokenize_line(
    TokenStream *ts,
    const char *line_start,
    const char *line_text,
    int line,
    const char *cursor)
{
    while (*cursor != '\0' && *cursor != '\r' && *cursor != '\n') {
        const char *start;
        int column;

        if (*cursor == '#') {
            break;
        }
        if (*cursor == ' ' || *cursor == '\t') {
            cursor++;
            continue;
        }

        if (is_letter(*cursor)) {
            char buffer[64];
            size_t len;

            start = cursor;
            column = (int)(start - line_start) + 1;
            cursor++;
            while (is_letter(*cursor) || is_digit(*cursor)) {
                cursor++;
            }
            len = (size_t)(cursor - start);
            if (len >= sizeof(buffer)) {
                fprintf(stderr, "Lexer error: token too long\n");
                exit(1);
            }
            memcpy(buffer, start, len);
            buffer[len] = '\0';
            append_token(ts,
                is_keyword(buffer) ? TOKEN_KEYWORD : TOKEN_IDENTIFIER,
                buffer,
                line,
                column,
                line_text);
            continue;
        }

        if (is_digit(*cursor)) {
            int seen_dot = 0;

            start = cursor;
            column = (int)(start - line_start) + 1;
            cursor++;
            while (is_digit(*cursor) || (!seen_dot && *cursor == '.' && is_digit(cursor[1]))) {
                if (*cursor == '.') {
                    seen_dot = 1;
                }
                cursor++;
            }
            push_buffered_token(ts,
                TOKEN_NUMBER,
                start,
                (size_t)(cursor - start),
                line,
                column,
                line_text);
            continue;
        }

        if (*cursor == '"') {
            push_quoted_token(ts, TOKEN_STRING, &cursor, line_start, line, line_text, '"');
            continue;
        }

        if (*cursor == '\'') {
            push_quoted_token(ts, TOKEN_CHAR, &cursor, line_start, line, line_text, '\'');
            continue;
        }

        column = (int)(cursor - line_start) + 1;
        if (*cursor == '-' && cursor[1] == '>') {
            append_token(ts, TOKEN_RARROW, "->", line, column, line_text);
            cursor += 2;
            continue;
        }
        if (*cursor == '=' && cursor[1] == '=') {
            append_token(ts, TOKEN_OPERATOR, "==", line, column, line_text);
            cursor += 2;
            continue;
        }
        if (*cursor == '!' && cursor[1] == '=') {
            append_token(ts, TOKEN_OPERATOR, "!=", line, column, line_text);
            cursor += 2;
            continue;
        }
        if (*cursor == '<' && cursor[1] == '=') {
            append_token(ts, TOKEN_OPERATOR, "<=", line, column, line_text);
            cursor += 2;
            continue;
        }
        if (*cursor == '>' && cursor[1] == '=') {
            append_token(ts, TOKEN_OPERATOR, ">=", line, column, line_text);
            cursor += 2;
            continue;
        }

        switch (*cursor) {
            case '=':
                append_token(ts, TOKEN_ASSIGN, "=", line, column, line_text);
                break;
            case '+':
                append_token(ts, TOKEN_PLUS, "+", line, column, line_text);
                break;
            case '-':
                append_token(ts, TOKEN_MINUS, "-", line, column, line_text);
                break;
            case '*':
                append_token(ts, TOKEN_OPERATOR, "*", line, column, line_text);
                break;
            case '/':
                append_token(ts, TOKEN_OPERATOR, "/", line, column, line_text);
                break;
            case '<':
                append_token(ts, TOKEN_OPERATOR, "<", line, column, line_text);
                break;
            case '>':
                append_token(ts, TOKEN_OPERATOR, ">", line, column, line_text);
                break;
            case '(':
                append_token(ts, TOKEN_LPAREN, "(", line, column, line_text);
                break;
            case ')':
                append_token(ts, TOKEN_RPAREN, ")", line, column, line_text);
                break;
            case '[':
                append_token(ts, TOKEN_LBRACKET, "[", line, column, line_text);
                break;
            case ']':
                append_token(ts, TOKEN_RBRACKET, "]", line, column, line_text);
                break;
            case ':':
                append_token(ts, TOKEN_COLON, ":", line, column, line_text);
                break;
            case ',':
                append_token(ts, TOKEN_COMMA, ",", line, column, line_text);
                break;
            case '.':
                append_token(ts, TOKEN_DOT, ".", line, column, line_text);
                break;
            case '|':
                append_token(ts, TOKEN_OPERATOR, "|", line, column, line_text);
                break;
            default: {
                char value[2] = { *cursor, '\0' };
                append_token(ts, TOKEN_SYMBOL, value, line, column, line_text);
                break;
            }
        }

        cursor++;
    }
}

Token next_token(FILE *fp)
{
    int c;
    char buffer[64];
    int i = 0;

    do {
        c = fgetc(fp);
    } while (c == ' ' || c == '\t');

    if (c == EOF) {
        return make_token(TOKEN_EOF, "");
    }
    if (c == '\n') {
        return make_token(TOKEN_NEWLINE, "\\n");
    }

    buffer[i++] = (char)c;

    if (is_letter((char)c)) {
        while (is_letter((char)fpeek(fp)) || is_digit((char)fpeek(fp))) {
            if (i + 1 >= (int)sizeof(buffer)) {
                fprintf(stderr, "Lexer error: token too long\n");
                exit(1);
            }
            buffer[i++] = (char)fgetc(fp);
        }
        buffer[i] = '\0';
        return make_token(is_keyword(buffer) ? TOKEN_KEYWORD : TOKEN_IDENTIFIER, buffer);
    }

    if (is_digit((char)c)) {
        int seen_dot = 0;

        while (is_digit((char)fpeek(fp)) ||
            (!seen_dot && fpeek(fp) == '.')) {
            if (fpeek(fp) == '.') {
                int dot = fgetc(fp);
                int next = fpeek(fp);

                if (!is_digit((char)next)) {
                    ungetc(dot, fp);
                    break;
                }
                seen_dot = 1;
                if (i + 1 >= (int)sizeof(buffer)) {
                    fprintf(stderr, "Lexer error: token too long\n");
                    exit(1);
                }
                buffer[i++] = (char)dot;
                continue;
            }
            if (i + 1 >= (int)sizeof(buffer)) {
                fprintf(stderr, "Lexer error: token too long\n");
                exit(1);
            }
            buffer[i++] = (char)fgetc(fp);
        }
        buffer[i] = '\0';
        return make_token(TOKEN_NUMBER, buffer);
    }

    if (c == '-' && fpeek(fp) == '>') {
        buffer[i++] = (char)fgetc(fp);
        buffer[i] = '\0';
        return make_token(TOKEN_RARROW, buffer);
    }
    if (c == '=' && fpeek(fp) == '=') {
        buffer[i++] = (char)fgetc(fp);
        buffer[i] = '\0';
        return make_token(TOKEN_OPERATOR, buffer);
    }

    buffer[i] = '\0';
    switch (c) {
        case '=': return make_token(TOKEN_ASSIGN, buffer);
        case '+': return make_token(TOKEN_PLUS, buffer);
        case '-': return make_token(TOKEN_MINUS, buffer);
        case '*':
        case '/':
        case '<':
        case '>': return make_token(TOKEN_OPERATOR, buffer);
        case '(': return make_token(TOKEN_LPAREN, buffer);
        case ')': return make_token(TOKEN_RPAREN, buffer);
        case '[': return make_token(TOKEN_LBRACKET, buffer);
        case ']': return make_token(TOKEN_RBRACKET, buffer);
        case ':': return make_token(TOKEN_COLON, buffer);
        case ',': return make_token(TOKEN_COMMA, buffer);
        case '.': return make_token(TOKEN_DOT, buffer);
        case '|': return make_token(TOKEN_OPERATOR, buffer);
        default: return make_token(TOKEN_SYMBOL, buffer);
    }
}

TokenStream *lexer(FILE *fp, const char *source_path)
{
    TokenStream *ts = create_token_stream(source_path);
    int indent_stack[MAX_INDENT_LEVELS] = {0};
    int depth = 0;
    char line[MAX_LINE_LEN];
    int line_number = 0;

    while (fgets(line, sizeof(line), fp) != NULL) {
        const char *cursor = line;
        const char *stored_line;
        int indent;
        int newline_column;

        if (strchr(line, '\n') == NULL && !feof(fp)) {
            fprintf(stderr, "Lexer error: line too long\n");
            exit(1);
        }

        line_number++;
        trim_line_ending(line);
        add_source_line(ts, line);
        stored_line = ts->source_lines[ts->line_count - 1];

        if (is_blank_line(cursor)) {
            append_token(ts, TOKEN_NEWLINE, "\\n", line_number, 1, stored_line);
            continue;
        }

        indent = consume_indent(&cursor);
        add_indent_tokens(ts, indent_stack, &depth, indent, line_number, stored_line);
        tokenize_line(ts, line, stored_line, line_number, cursor);
        newline_column = (int)strlen(line) + 1;
        append_token(ts, TOKEN_NEWLINE, "\\n", line_number, newline_column, stored_line);
    }

    while (depth > 0) {
        depth--;
        append_token(ts, TOKEN_DEDENT, "<DEDENT>", line_number, 1, "");
    }

    append_token(ts, TOKEN_EOF, "", line_number, 1, "");
    return ts;
}
