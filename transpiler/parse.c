#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse_internal.h"

char *parse_dup_string(const char *value)
{
    char *copy;
    size_t len;

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

char *parse_join_type_name(const char *container, const char *member)
{
    char *name;
    size_t len = strlen(container) + strlen(member) + 3;

    name = malloc(len);
    if (name == NULL) {
        perror("malloc");
        exit(1);
    }

    snprintf(name, len, "%s[%s]", container, member);
    return name;
}

static void parse_set_node_location(
    ParseNode *node,
    const char *path,
    int line,
    int column,
    const char *source_line)
{
    if (node == NULL || path == NULL || line <= 0 || column <= 0) {
        return;
    }

    free(node->source_path);
    free(node->source_line);
    node->source_path = parse_dup_string(path);
    node->source_line = parse_dup_string(source_line);
    node->line = line;
    node->column = column;
}

int parse_is_keyword_token(Token tok, const char *value)
{
    return tok.type == TOKEN_KEYWORD && strcmp(tok.value, value) == 0;
}

int parse_is_bool_literal(Token tok)
{
    return parse_is_keyword_token(tok, "True") || parse_is_keyword_token(tok, "False");
}

int parse_is_block_continuation(Token tok)
{
    return parse_is_keyword_token(tok, "elif") || parse_is_keyword_token(tok, "else");
}

int parse_is_operator_token(Token tok)
{
    return tok.type == TOKEN_PLUS || tok.type == TOKEN_MINUS || tok.type == TOKEN_OPERATOR;
}

int parse_is_type_token(Token tok)
{
    return tok.type == TOKEN_KEYWORD || tok.type == TOKEN_IDENTIFIER;
}

int parse_is_pipe_token(Token tok)
{
    return tok.type == TOKEN_OPERATOR && strcmp(tok.value, "|") == 0;
}

void parse_error(Token tok, const char *message)
{
    print_token_diagnostic(stderr, tok, "Parse error", message);
    exit(1);
}

void parse_error_at_node(const ParseNode *node, const char *message)
{
    if (node != NULL && node->source_path != NULL && node->line > 0 && node->column > 0) {
        print_source_diagnostic(
            stderr,
            node->source_path,
            node->line,
            node->column,
            "Parse error",
            message,
            node->source_line);
    } else {
        fprintf(stderr, "Parse error: %s\n", message);
    }
    exit(1);
}

Token parse_expect_keyword(TokenStream *ts, const char *value)
{
    Token tok = expect(ts, TOKEN_KEYWORD);

    if (strcmp(tok.value, value) != 0) {
        parse_error(tok, "unexpected keyword");
    }

    return tok;
}

void parse_skip_newlines(TokenStream *ts)
{
    while (peek_ts(ts).type == TOKEN_NEWLINE) {
        get_from_ts(ts);
    }
}

static ParseNode *parse_statement_list(TokenStream *ts, TokenType terminator, NodeKind kind);
static ParseNode *parse_SIMPLE_STATEMENT(TokenStream *ts);
static ParseNode *parse_IMPORT_STATEMENT(TokenStream *ts);
static ParseNode *parse_RETURN_STATEMENT(TokenStream *ts);
static ParseNode *parse_BREAK_STATEMENT(TokenStream *ts);
static ParseNode *parse_CONTINUE_STATEMENT(TokenStream *ts);
static ParseNode *parse_IF_STATEMENT(TokenStream *ts);
static ParseNode *parse_WHILE_STATEMENT(TokenStream *ts);
static ParseNode *parse_FOR_STATEMENT(TokenStream *ts);
static ParseNode *parse_CLASS_DEF(TokenStream *ts);
static ParseNode *parse_FIELD_DECL(TokenStream *ts);
static ParseNode *parse_FUNCTION_DEF(TokenStream *ts);
static ParseNode *parse_NATIVE_FUNCTION_DEF(TokenStream *ts);
static ParseNode *parse_NATIVE_TYPE_DEF(TokenStream *ts);
static ParseNode *parse_NATIVE_DECL(TokenStream *ts);
static ParseNode *parse_SUITE(TokenStream *ts);
static ParseNode *parse_PARAMETERS(TokenStream *ts);
static ParseNode *parse_EXPRESSION_STATEMENT(TokenStream *ts);

ParseNode *create_node(NodeKind kind, TokenType token_type, const char *value)
{
    ParseNode *node = malloc(sizeof(ParseNode));

    if (node == NULL) {
        perror("malloc");
        exit(1);
    }

    node->kind = kind;
    node->token_type = token_type;
    node->value = parse_dup_string(value);
    node->source_path = NULL;
    node->source_line = NULL;
    node->line = 0;
    node->column = 0;
    node->children = NULL;
    node->child_count = 0;
    return node;
}

ParseNode *create_node_from_token(NodeKind kind, Token tok)
{
    ParseNode *node = create_node(kind, tok.type, tok.value);

    parse_set_node_location(node, tok.path, tok.line, tok.column, tok.line_text);
    return node;
}

void add_child(ParseNode *parent, ParseNode *child)
{
    ParseNode **children = realloc(parent->children,
        sizeof(ParseNode *) * (parent->child_count + 1));

    if (children == NULL) {
        perror("realloc");
        exit(1);
    }

    parent->children = children;
    parent->children[parent->child_count++] = child;
    if (parent->line == 0 && child != NULL && child->line > 0) {
        parse_set_node_location(
            parent,
            child->source_path,
            child->line,
            child->column,
            child->source_line);
    }
}

void free_tree(ParseNode *node)
{
    if (node == NULL) {
        return;
    }

    for (size_t i = 0; i < node->child_count; i++) {
        free_tree(node->children[i]);
    }

    free(node->children);
    free(node->value);
    free(node->source_path);
    free(node->source_line);
    free(node);
}

const char *node_kind_to_str(NodeKind kind)
{
    switch (kind) {
        case NODE_S:                    return "PROGRAM";
        case NODE_STATEMENT:            return "STATEMENT";
        case NODE_SIMPLE_STATEMENT:     return "SIMPLE_STATEMENT";
        case NODE_IMPORT_STATEMENT:     return "IMPORT_STATEMENT";
        case NODE_RETURN_STATEMENT:     return "RETURN_STATEMENT";
        case NODE_BREAK_STATEMENT:      return "BREAK_STATEMENT";
        case NODE_CONTINUE_STATEMENT:   return "CONTINUE_STATEMENT";
        case NODE_IF_STATEMENT:         return "IF_STATEMENT";
        case NODE_WHILE_STATEMENT:      return "WHILE_STATEMENT";
        case NODE_FOR_STATEMENT:        return "FOR_STATEMENT";
        case NODE_CLASS_DEF:            return "CLASS_DEF";
        case NODE_FIELD_DECL:           return "FIELD_DECL";
        case NODE_ELIF_CLAUSE:          return "ELIF_CLAUSE";
        case NODE_ELSE_CLAUSE:          return "ELSE_CLAUSE";
        case NODE_STATEMENT_TAIL:       return "STATEMENT_TAIL";
        case NODE_FUNCTION_DEF:         return "FUNCTION_DEF";
        case NODE_NATIVE_FUNCTION_DEF:  return "NATIVE_FUNCTION_DEF";
        case NODE_NATIVE_TYPE_DEF:      return "NATIVE_TYPE_DEF";
        case NODE_PARAMETERS:           return "PARAMETERS";
        case NODE_PARAMETER:            return "PARAMETER";
        case NODE_RETURN_TYPE:          return "RETURN_TYPE";
        case NODE_TYPE:                 return "TYPE";
        case NODE_SUITE:                return "SUITE";
        case NODE_EXPRESSION_STATEMENT: return "EXPRESSION_STATEMENT";
        case NODE_EXPRESSION:           return "EXPRESSION";
        case NODE_PRIMARY:              return "PRIMARY";
        case NODE_LIST_LITERAL:         return "LIST_LITERAL";
        case NODE_DICT_LITERAL:         return "DICT_LITERAL";
        case NODE_TUPLE_LITERAL:        return "TUPLE_LITERAL";
        case NODE_TUPLE_TARGET:         return "TUPLE_TARGET";
        case NODE_CALL:                 return "CALL";
        case NODE_METHOD_CALL:          return "METHOD_CALL";
        case NODE_FIELD_ACCESS:         return "FIELD_ACCESS";
        case NODE_INDEX:                return "INDEX";
        case NODE_ARGUMENTS:            return "ARGUMENTS";
        case NODE_COLON:                return "COLON";
        case NODE_ASSIGN:               return "ASSIGN";
        case NODE_OPERATOR:             return "OPERATOR";
        case NODE_EPSILON:              return "EPSILON";
        default:                        return "UNKNOWN";
    }
}

void print_tree(ParseNode *node, int depth)
{
    if (node == NULL) {
        return;
    }

    for (int i = 0; i < depth; i++) {
        printf(i == depth - 1 ? "├── " : "│   ");
    }

    printf("%s", node_kind_to_str(node->kind));
    if (node->value != NULL) {
        printf(" (%s, %s)", token_type_to_str(node->token_type), node->value);
    }
    printf("\n");

    for (size_t i = 0; i < node->child_count; i++) {
        print_tree(node->children[i], depth + 1);
    }
}

static ParseNode *parse_statement_list(TokenStream *ts, TokenType terminator, NodeKind kind)
{
    ParseNode *root = create_node(kind, TOKEN_NULL, NULL);

    parse_skip_newlines(ts);
    while (peek_ts(ts).type != terminator &&
        peek_ts(ts).type != TOKEN_EOF &&
        !parse_is_block_continuation(peek_ts(ts))) {
        add_child(root, parse_STATEMENT(ts));
        parse_skip_newlines(ts);
    }

    if (root->child_count == 0) {
        add_child(root, create_node(NODE_EPSILON, TOKEN_NULL, "epsilon"));
    }

    return root;
}

ParseNode *parse_S(TokenStream *ts)
{
    return parse_statement_list(ts, TOKEN_EOF, NODE_S);
}

ParseNode *parse_STATEMENT(TokenStream *ts)
{
    ParseNode *node = create_node(NODE_STATEMENT, TOKEN_NULL, NULL);

    if (parse_is_keyword_token(peek_ts(ts), "def")) {
        add_child(node, parse_FUNCTION_DEF(ts));
    } else if (parse_is_keyword_token(peek_ts(ts), "native")) {
        add_child(node, parse_NATIVE_DECL(ts));
    } else if (parse_is_keyword_token(peek_ts(ts), "import") || parse_is_keyword_token(peek_ts(ts), "from")) {
        add_child(node, parse_IMPORT_STATEMENT(ts));
    } else if (parse_is_keyword_token(peek_ts(ts), "if")) {
        add_child(node, parse_IF_STATEMENT(ts));
    } else if (parse_is_keyword_token(peek_ts(ts), "while")) {
        add_child(node, parse_WHILE_STATEMENT(ts));
    } else if (parse_is_keyword_token(peek_ts(ts), "for")) {
        add_child(node, parse_FOR_STATEMENT(ts));
    } else if (parse_is_keyword_token(peek_ts(ts), "class")) {
        add_child(node, parse_CLASS_DEF(ts));
    } else {
        add_child(node, parse_SIMPLE_STATEMENT(ts));
    }

    return node;
}

ParseNode *parse_STATEMENT_TAIL(TokenStream *ts)
{
    ParseNode *node = create_node(NODE_STATEMENT_TAIL, TOKEN_NULL, NULL);
    Token look = peek_ts(ts);
    Token tok;

    if (look.type == TOKEN_COLON) {
        tok = expect(ts, TOKEN_COLON);
        add_child(node, create_node_from_token(NODE_COLON, tok));
        add_child(node, parse_TYPE(ts));

        tok = expect(ts, TOKEN_ASSIGN);
        add_child(node, create_node_from_token(NODE_ASSIGN, tok));
        add_child(node, parse_EXPRESSION(ts));
        return node;
    }

    if (look.type == TOKEN_ASSIGN) {
        tok = expect(ts, TOKEN_ASSIGN);
        add_child(node, create_node_from_token(NODE_ASSIGN, tok));
        add_child(node, parse_EXPRESSION(ts));
        return node;
    }

    parse_error(look, "unexpected token in statement tail");
    return NULL;
}

static ParseNode *parse_IMPORT_STATEMENT(TokenStream *ts)
{
    Token first_tok = expect(ts, TOKEN_KEYWORD);
    ParseNode *node = create_node_from_token(NODE_IMPORT_STATEMENT, first_tok);
    Token first_name;
    ParseNode *module_node;

    first_name = expect(ts, TOKEN_IDENTIFIER);
    module_node = create_node_from_token(NODE_PRIMARY, first_name);
    while (peek_ts(ts).type == TOKEN_DOT) {
        Token next_name;
        char *joined;
        size_t len;

        expect(ts, TOKEN_DOT);
        next_name = expect(ts, TOKEN_IDENTIFIER);
        len = strlen(module_node->value) + strlen(next_name.value) + 2;
        joined = malloc(len);
        if (joined == NULL) {
            perror("malloc");
            exit(1);
        }
        snprintf(joined, len, "%s.%s", module_node->value, next_name.value);
        free(module_node->value);
        module_node->value = joined;
    }

    if (strcmp(first_tok.value, "import") == 0) {
        add_child(node, module_node);
        if (parse_is_keyword_token(peek_ts(ts), "as")) {
            Token alias_name;

            get_from_ts(ts);
            alias_name = expect(ts, TOKEN_IDENTIFIER);
            add_child(node, create_node_from_token(NODE_PRIMARY, alias_name));
        }
        return node;
    }

    if (strcmp(first_tok.value, "from") == 0) {
        Token imported_name;
        Token alias_name;

        add_child(node, module_node);
        parse_expect_keyword(ts, "import");
        imported_name = expect(ts, TOKEN_IDENTIFIER);
        add_child(node, create_node_from_token(NODE_PRIMARY, imported_name));
        if (parse_is_keyword_token(peek_ts(ts), "as")) {
            get_from_ts(ts);
            alias_name = expect(ts, TOKEN_IDENTIFIER);
            add_child(node, create_node_from_token(NODE_PRIMARY, alias_name));
        }
        return node;
    }

    free_tree(module_node);
    parse_error(first_tok, "unexpected keyword");
    return NULL;
}

static ParseNode *parse_WHILE_STATEMENT(TokenStream *ts)
{
    Token while_tok = parse_expect_keyword(ts, "while");
    ParseNode *node = create_node_from_token(NODE_WHILE_STATEMENT, while_tok);
    Token colon_tok;

    add_child(node, parse_EXPRESSION(ts));
    colon_tok = expect(ts, TOKEN_COLON);
    add_child(node, create_node_from_token(NODE_COLON, colon_tok));
    expect(ts, TOKEN_NEWLINE);
    expect(ts, TOKEN_INDENT);
    add_child(node, parse_SUITE(ts));
    expect(ts, TOKEN_DEDENT);
    return node;
}

static ParseNode *parse_FOR_STATEMENT(TokenStream *ts)
{
    Token for_tok = parse_expect_keyword(ts, "for");
    ParseNode *node = create_node_from_token(NODE_FOR_STATEMENT, for_tok);
    Token iterator_name;
    Token colon_tok;

    iterator_name = expect(ts, TOKEN_IDENTIFIER);
    add_child(node, create_node_from_token(NODE_PRIMARY, iterator_name));
    parse_expect_keyword(ts, "in");
    add_child(node, parse_EXPRESSION(ts));
    colon_tok = expect(ts, TOKEN_COLON);
    add_child(node, create_node_from_token(NODE_COLON, colon_tok));
    expect(ts, TOKEN_NEWLINE);
    expect(ts, TOKEN_INDENT);
    add_child(node, parse_SUITE(ts));
    expect(ts, TOKEN_DEDENT);
    return node;
}

static ParseNode *parse_FIELD_DECL(TokenStream *ts)
{
    Token field_tok = expect(ts, TOKEN_IDENTIFIER);
    ParseNode *node = create_node_from_token(NODE_FIELD_DECL, field_tok);

    expect(ts, TOKEN_COLON);
    add_child(node, parse_TYPE(ts));
    return node;
}

static ParseNode *parse_CLASS_DEF(TokenStream *ts)
{
    Token class_tok = parse_expect_keyword(ts, "class");
    ParseNode *node = create_node_from_token(NODE_CLASS_DEF, class_tok);
    Token name;
    Token colon_tok;

    name = expect(ts, TOKEN_IDENTIFIER);
    add_child(node, create_node_from_token(NODE_PRIMARY, name));

    colon_tok = expect(ts, TOKEN_COLON);
    add_child(node, create_node_from_token(NODE_COLON, colon_tok));
    expect(ts, TOKEN_NEWLINE);
    expect(ts, TOKEN_INDENT);
    parse_skip_newlines(ts);

    while (peek_ts(ts).type != TOKEN_DEDENT && peek_ts(ts).type != TOKEN_EOF) {
        if (parse_is_keyword_token(peek_ts(ts), "def")) {
            add_child(node, parse_FUNCTION_DEF(ts));
        } else if (parse_is_keyword_token(peek_ts(ts), "native")) {
            add_child(node, parse_NATIVE_DECL(ts));
        } else {
            add_child(node, parse_FIELD_DECL(ts));
        }
        if (peek_ts(ts).type == TOKEN_NEWLINE) {
            get_from_ts(ts);
        }
        parse_skip_newlines(ts);
    }

    if (node->child_count == 2) {
        parse_error_at_node(node, "class body must declare at least one field");
    }

    expect(ts, TOKEN_DEDENT);
    return node;
}

static ParseNode *parse_SIMPLE_STATEMENT(TokenStream *ts)
{
    ParseNode *node = create_node(NODE_SIMPLE_STATEMENT, TOKEN_NULL, NULL);
    Token first = peek_ts(ts);
    int start_pos = ts->pos;

    if (parse_is_keyword_token(first, "return")) {
        add_child(node, parse_RETURN_STATEMENT(ts));
        return node;
    }

    if (parse_is_keyword_token(first, "break")) {
        add_child(node, parse_BREAK_STATEMENT(ts));
        return node;
    }

    if (parse_is_keyword_token(first, "continue")) {
        add_child(node, parse_CONTINUE_STATEMENT(ts));
        return node;
    }

    if (first.type == TOKEN_IDENTIFIER || first.type == TOKEN_LPAREN) {
        ParseNode *target = parse_ASSIGN_TARGET(ts);
        Token look = peek_ts(ts);

        if ((target->kind == NODE_PRIMARY && (look.type == TOKEN_COLON || look.type == TOKEN_ASSIGN)) ||
            (target->kind == NODE_FIELD_ACCESS && (look.type == TOKEN_COLON || look.type == TOKEN_ASSIGN)) ||
            (target->kind == NODE_INDEX && look.type == TOKEN_ASSIGN) ||
            (target->kind == NODE_TUPLE_TARGET && (look.type == TOKEN_COLON || look.type == TOKEN_ASSIGN))) {
            add_child(node, target);
            add_child(node, parse_STATEMENT_TAIL(ts));
            return node;
        }

        free_tree(target);
        ts->pos = start_pos;
    }

    add_child(node, parse_EXPRESSION_STATEMENT(ts));
    return node;
}

static ParseNode *parse_RETURN_STATEMENT(TokenStream *ts)
{
    Token return_tok = parse_expect_keyword(ts, "return");
    ParseNode *node = create_node_from_token(NODE_RETURN_STATEMENT, return_tok);

    if (peek_ts(ts).type == TOKEN_NEWLINE || peek_ts(ts).type == TOKEN_DEDENT ||
        peek_ts(ts).type == TOKEN_EOF) {
        add_child(node, create_node(NODE_EPSILON, TOKEN_NULL, "epsilon"));
        return node;
    }

    add_child(node, parse_EXPRESSION(ts));
    return node;
}

static ParseNode *parse_BREAK_STATEMENT(TokenStream *ts)
{
    return create_node_from_token(NODE_BREAK_STATEMENT, parse_expect_keyword(ts, "break"));
}

static ParseNode *parse_CONTINUE_STATEMENT(TokenStream *ts)
{
    return create_node_from_token(NODE_CONTINUE_STATEMENT, parse_expect_keyword(ts, "continue"));
}

static ParseNode *parse_FUNCTION_DEF(TokenStream *ts)
{
    Token def_tok = parse_expect_keyword(ts, "def");
    ParseNode *node = create_node_from_token(NODE_FUNCTION_DEF, def_tok);
    Token name;
    Token arrow_tok;
    Token colon_tok;

    name = expect(ts, TOKEN_IDENTIFIER);
    add_child(node, create_node_from_token(NODE_PRIMARY, name));

    expect(ts, TOKEN_LPAREN);
    add_child(node, parse_PARAMETERS(ts));
    expect(ts, TOKEN_RPAREN);

    if (peek_ts(ts).type == TOKEN_RARROW) {
        ParseNode *return_type;

        arrow_tok = expect(ts, TOKEN_RARROW);
        return_type = create_node_from_token(NODE_RETURN_TYPE, arrow_tok);
        add_child(return_type, parse_TYPE(ts));
        add_child(node, return_type);
    }

    colon_tok = expect(ts, TOKEN_COLON);
    add_child(node, create_node_from_token(NODE_COLON, colon_tok));
    expect(ts, TOKEN_NEWLINE);
    expect(ts, TOKEN_INDENT);
    add_child(node, parse_SUITE(ts));
    expect(ts, TOKEN_DEDENT);
    return node;
}

static ParseNode *parse_NATIVE_FUNCTION_DEF(TokenStream *ts)
{
    Token native_tok = parse_expect_keyword(ts, "native");
    Token def_tok;
    ParseNode *node;
    Token name;
    Token arrow_tok;

    (void)native_tok;
    def_tok = parse_expect_keyword(ts, "def");
    node = create_node_from_token(NODE_NATIVE_FUNCTION_DEF, def_tok);

    name = expect(ts, TOKEN_IDENTIFIER);
    add_child(node, create_node_from_token(NODE_PRIMARY, name));

    expect(ts, TOKEN_LPAREN);
    add_child(node, parse_PARAMETERS(ts));
    expect(ts, TOKEN_RPAREN);

    if (peek_ts(ts).type == TOKEN_RARROW) {
        ParseNode *return_type;

        arrow_tok = expect(ts, TOKEN_RARROW);
        return_type = create_node_from_token(NODE_RETURN_TYPE, arrow_tok);
        add_child(return_type, parse_TYPE(ts));
        add_child(node, return_type);
    }

    return node;
}

static ParseNode *parse_NATIVE_TYPE_DEF(TokenStream *ts)
{
    Token native_tok = parse_expect_keyword(ts, "native");
    Token type_tok;
    ParseNode *node;
    Token name;

    (void)native_tok;
    type_tok = parse_expect_keyword(ts, "type");
    node = create_node_from_token(NODE_NATIVE_TYPE_DEF, type_tok);

    name = expect(ts, TOKEN_IDENTIFIER);
    add_child(node, create_node_from_token(NODE_PRIMARY, name));
    return node;
}

static ParseNode *parse_NATIVE_DECL(TokenStream *ts)
{
    if (ts->pos + 1 >= ts->count) {
        parse_error(peek_ts(ts), "expected native declaration");
    }

    if (parse_is_keyword_token(ts->data[ts->pos + 1], "def")) {
        return parse_NATIVE_FUNCTION_DEF(ts);
    }
    if (parse_is_keyword_token(ts->data[ts->pos + 1], "type")) {
        return parse_NATIVE_TYPE_DEF(ts);
    }

    parse_error(ts->data[ts->pos + 1], "expected 'def' or 'type' after native");
    return NULL;
}

static ParseNode *parse_IF_STATEMENT(TokenStream *ts)
{
    Token if_tok = parse_expect_keyword(ts, "if");
    ParseNode *node = create_node_from_token(NODE_IF_STATEMENT, if_tok);
    Token colon_tok;

    add_child(node, parse_EXPRESSION(ts));
    colon_tok = expect(ts, TOKEN_COLON);
    add_child(node, create_node_from_token(NODE_COLON, colon_tok));
    expect(ts, TOKEN_NEWLINE);
    expect(ts, TOKEN_INDENT);
    add_child(node, parse_SUITE(ts));
    expect(ts, TOKEN_DEDENT);

    while (parse_is_keyword_token(peek_ts(ts), "elif")) {
        Token elif_tok = parse_expect_keyword(ts, "elif");
        ParseNode *elif_node = create_node_from_token(NODE_ELIF_CLAUSE, elif_tok);

        add_child(elif_node, parse_EXPRESSION(ts));
        colon_tok = expect(ts, TOKEN_COLON);
        add_child(elif_node, create_node_from_token(NODE_COLON, colon_tok));
        expect(ts, TOKEN_NEWLINE);
        expect(ts, TOKEN_INDENT);
        add_child(elif_node, parse_SUITE(ts));
        expect(ts, TOKEN_DEDENT);
        add_child(node, elif_node);
    }

    if (parse_is_keyword_token(peek_ts(ts), "else")) {
        Token else_tok = parse_expect_keyword(ts, "else");
        ParseNode *else_node = create_node_from_token(NODE_ELSE_CLAUSE, else_tok);

        colon_tok = expect(ts, TOKEN_COLON);
        add_child(else_node, create_node_from_token(NODE_COLON, colon_tok));
        expect(ts, TOKEN_NEWLINE);
        expect(ts, TOKEN_INDENT);
        add_child(else_node, parse_SUITE(ts));
        expect(ts, TOKEN_DEDENT);
        add_child(node, else_node);
    }

    return node;
}

static ParseNode *parse_PARAMETERS(TokenStream *ts)
{
    ParseNode *node = create_node(NODE_PARAMETERS, TOKEN_NULL, NULL);

    if (peek_ts(ts).type == TOKEN_RPAREN) {
        add_child(node, create_node(NODE_EPSILON, TOKEN_NULL, "epsilon"));
        return node;
    }

    while (1) {
        ParseNode *param_node;
        Token param = expect(ts, TOKEN_IDENTIFIER);

        param_node = create_node_from_token(NODE_PARAMETER, param);
        expect(ts, TOKEN_COLON);
        add_child(param_node, parse_TYPE(ts));
        add_child(node, param_node);

        if (peek_ts(ts).type != TOKEN_COMMA) {
            break;
        }
        get_from_ts(ts);
    }

    return node;
}

static ParseNode *parse_SUITE(TokenStream *ts)
{
    return parse_statement_list(ts, TOKEN_DEDENT, NODE_SUITE);
}

static ParseNode *parse_EXPRESSION_STATEMENT(TokenStream *ts)
{
    ParseNode *node = create_node(NODE_EXPRESSION_STATEMENT, TOKEN_NULL, NULL);

    add_child(node, parse_EXPRESSION(ts));
    return node;
}

ParseNode *parse(TokenStream *ts)
{
    ParseNode *root = parse_S(ts);

    if (peek_ts(ts).type != TOKEN_EOF) {
        parse_error(peek_ts(ts), "unexpected tokens remaining after parse");
    }

    return root;
}
