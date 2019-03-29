#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse.h"

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

static Token peek_n(TokenStream *ts, int offset)
{
    int index = ts->pos + offset;

    if (index >= 0 && index < ts->count) {
        return ts->data[index];
    }
    return make_token(TOKEN_EOF, "");
}

static int is_keyword_token(Token tok, const char *value)
{
    return tok.type == TOKEN_KEYWORD && strcmp(tok.value, value) == 0;
}

static int is_bool_literal(Token tok)
{
    return is_keyword_token(tok, "True") || is_keyword_token(tok, "False");
}

static int is_block_continuation(Token tok)
{
    return is_keyword_token(tok, "elif") || is_keyword_token(tok, "else");
}

static int is_operator_token(Token tok)
{
    return tok.type == TOKEN_PLUS || tok.type == TOKEN_MINUS || tok.type == TOKEN_OPERATOR;
}

static int is_type_token(Token tok)
{
    return tok.type == TOKEN_KEYWORD || tok.type == TOKEN_IDENTIFIER;
}

static int is_pipe_token(Token tok)
{
    return tok.type == TOKEN_OPERATOR && strcmp(tok.value, "|") == 0;
}

static void parse_error(Token tok, const char *message)
{
    fprintf(stderr, "Parse error: %s near '%s'\n", message, tok.value);
    exit(1);
}

static Token expect_keyword(TokenStream *ts, const char *value)
{
    Token tok = expect(ts, TOKEN_KEYWORD);

    if (strcmp(tok.value, value) != 0) {
        parse_error(tok, "unexpected keyword");
    }

    return tok;
}

static void skip_newlines(TokenStream *ts)
{
    while (peek_ts(ts).type == TOKEN_NEWLINE) {
        get_from_ts(ts);
    }
}

static ParseNode *parse_SIMPLE_STATEMENT(TokenStream *ts);
static ParseNode *parse_RETURN_STATEMENT(TokenStream *ts);
static ParseNode *parse_IF_STATEMENT(TokenStream *ts);
static ParseNode *parse_FUNCTION_DEF(TokenStream *ts);
static ParseNode *parse_SUITE(TokenStream *ts);
static ParseNode *parse_PARAMETERS(TokenStream *ts);
static ParseNode *parse_ARGUMENTS(TokenStream *ts);
static ParseNode *parse_EXPRESSION_STATEMENT(TokenStream *ts);
static ParseNode *parse_TYPE(TokenStream *ts);
static ParseNode *parse_PRIMARY(TokenStream *ts);

ParseNode *create_node(NodeKind kind, TokenType token_type, const char *value)
{
    ParseNode *node = malloc(sizeof(ParseNode));

    if (node == NULL) {
        perror("malloc");
        exit(1);
    }

    node->kind = kind;
    node->token_type = token_type;
    node->value = dup_string(value);
    node->children = NULL;
    node->child_count = 0;
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
    free(node);
}

const char *node_kind_to_str(NodeKind kind)
{
    switch (kind) {
        case NODE_S:                    return "PROGRAM";
        case NODE_STATEMENT:            return "STATEMENT";
        case NODE_SIMPLE_STATEMENT:     return "SIMPLE_STATEMENT";
        case NODE_RETURN_STATEMENT:     return "RETURN_STATEMENT";
        case NODE_IF_STATEMENT:         return "IF_STATEMENT";
        case NODE_ELIF_CLAUSE:          return "ELIF_CLAUSE";
        case NODE_ELSE_CLAUSE:          return "ELSE_CLAUSE";
        case NODE_STATEMENT_TAIL:       return "STATEMENT_TAIL";
        case NODE_FUNCTION_DEF:         return "FUNCTION_DEF";
        case NODE_PARAMETERS:           return "PARAMETERS";
        case NODE_PARAMETER:            return "PARAMETER";
        case NODE_RETURN_TYPE:          return "RETURN_TYPE";
        case NODE_TYPE:                 return "TYPE";
        case NODE_SUITE:                return "SUITE";
        case NODE_EXPRESSION_STATEMENT: return "EXPRESSION_STATEMENT";
        case NODE_EXPRESSION:           return "EXPRESSION";
        case NODE_PRIMARY:              return "PRIMARY";
        case NODE_CALL:                 return "CALL";
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

    skip_newlines(ts);
    while (peek_ts(ts).type != terminator &&
        peek_ts(ts).type != TOKEN_EOF &&
        !is_block_continuation(peek_ts(ts))) {
        add_child(root, parse_STATEMENT(ts));
        skip_newlines(ts);
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

    if (is_keyword_token(peek_ts(ts), "def")) {
        add_child(node, parse_FUNCTION_DEF(ts));
    } else if (is_keyword_token(peek_ts(ts), "if")) {
        add_child(node, parse_IF_STATEMENT(ts));
    } else {
        add_child(node, parse_SIMPLE_STATEMENT(ts));
    }

    return node;
}

static ParseNode *parse_SIMPLE_STATEMENT(TokenStream *ts)
{
    ParseNode *node = create_node(NODE_SIMPLE_STATEMENT, TOKEN_NULL, NULL);
    Token first = peek_ts(ts);
    Token second = peek_n(ts, 1);

    if (is_keyword_token(first, "return")) {
        add_child(node, parse_RETURN_STATEMENT(ts));
        return node;
    }

    if (first.type == TOKEN_IDENTIFIER &&
        (second.type == TOKEN_COLON || second.type == TOKEN_ASSIGN)) {
        Token identifier = expect(ts, TOKEN_IDENTIFIER);
        add_child(node, create_node(NODE_PRIMARY, identifier.type, identifier.value));
        add_child(node, parse_STATEMENT_TAIL(ts));
        return node;
    }

    add_child(node, parse_EXPRESSION_STATEMENT(ts));
    return node;
}

static ParseNode *parse_RETURN_STATEMENT(TokenStream *ts)
{
    ParseNode *node = create_node(NODE_RETURN_STATEMENT, TOKEN_NULL, NULL);

    expect_keyword(ts, "return");
    if (peek_ts(ts).type == TOKEN_NEWLINE || peek_ts(ts).type == TOKEN_DEDENT ||
        peek_ts(ts).type == TOKEN_EOF) {
        add_child(node, create_node(NODE_EPSILON, TOKEN_NULL, "epsilon"));
        return node;
    }

    add_child(node, parse_EXPRESSION(ts));
    return node;
}

static ParseNode *parse_FUNCTION_DEF(TokenStream *ts)
{
    ParseNode *node = create_node(NODE_FUNCTION_DEF, TOKEN_NULL, NULL);
    Token name;

    expect_keyword(ts, "def");
    name = expect(ts, TOKEN_IDENTIFIER);
    add_child(node, create_node(NODE_PRIMARY, name.type, name.value));

    expect(ts, TOKEN_LPAREN);
    add_child(node, parse_PARAMETERS(ts));
    expect(ts, TOKEN_RPAREN);

    if (peek_ts(ts).type == TOKEN_RARROW) {
        ParseNode *return_type = create_node(NODE_RETURN_TYPE, TOKEN_NULL, NULL);

        expect(ts, TOKEN_RARROW);
        add_child(return_type, parse_TYPE(ts));
        add_child(node, return_type);
    }

    expect(ts, TOKEN_COLON);
    add_child(node, create_node(NODE_COLON, TOKEN_COLON, ":"));

    expect(ts, TOKEN_NEWLINE);
    expect(ts, TOKEN_INDENT);
    add_child(node, parse_SUITE(ts));
    expect(ts, TOKEN_DEDENT);

    return node;
}

static ParseNode *parse_IF_STATEMENT(TokenStream *ts)
{
    ParseNode *node = create_node(NODE_IF_STATEMENT, TOKEN_NULL, NULL);

    expect_keyword(ts, "if");
    add_child(node, parse_EXPRESSION(ts));
    expect(ts, TOKEN_COLON);
    add_child(node, create_node(NODE_COLON, TOKEN_COLON, ":"));
    expect(ts, TOKEN_NEWLINE);
    expect(ts, TOKEN_INDENT);
    add_child(node, parse_SUITE(ts));
    expect(ts, TOKEN_DEDENT);

    while (is_keyword_token(peek_ts(ts), "elif")) {
        ParseNode *elif_node = create_node(NODE_ELIF_CLAUSE, TOKEN_NULL, NULL);

        expect_keyword(ts, "elif");
        add_child(elif_node, parse_EXPRESSION(ts));
        expect(ts, TOKEN_COLON);
        add_child(elif_node, create_node(NODE_COLON, TOKEN_COLON, ":"));
        expect(ts, TOKEN_NEWLINE);
        expect(ts, TOKEN_INDENT);
        add_child(elif_node, parse_SUITE(ts));
        expect(ts, TOKEN_DEDENT);

        add_child(node, elif_node);
    }

    if (is_keyword_token(peek_ts(ts), "else")) {
        ParseNode *else_node = create_node(NODE_ELSE_CLAUSE, TOKEN_NULL, NULL);

        expect_keyword(ts, "else");
        expect(ts, TOKEN_COLON);
        add_child(else_node, create_node(NODE_COLON, TOKEN_COLON, ":"));
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
        Token param = expect(ts, TOKEN_IDENTIFIER);
        ParseNode *param_node = create_node(NODE_PARAMETER, param.type, param.value);

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

ParseNode *parse_STATEMENT_TAIL(TokenStream *ts)
{
    ParseNode *node = create_node(NODE_STATEMENT_TAIL, TOKEN_NULL, NULL);
    Token look = peek_ts(ts);

    if (look.type == TOKEN_COLON) {
        expect(ts, TOKEN_COLON);
        add_child(node, create_node(NODE_COLON, TOKEN_COLON, ":"));
        add_child(node, parse_TYPE(ts));

        expect(ts, TOKEN_ASSIGN);
        add_child(node, create_node(NODE_ASSIGN, TOKEN_ASSIGN, "="));
        add_child(node, parse_EXPRESSION(ts));
        return node;
    }

    if (look.type == TOKEN_ASSIGN) {
        expect(ts, TOKEN_ASSIGN);
        add_child(node, create_node(NODE_ASSIGN, TOKEN_ASSIGN, "="));
        add_child(node, parse_EXPRESSION(ts));
        return node;
    }

    parse_error(look, "unexpected token in statement tail");
    return NULL;
}

static ParseNode *parse_TYPE(TokenStream *ts)
{
    ParseNode *node = create_node(NODE_TYPE, TOKEN_NULL, NULL);
    Token type_tok;

    type_tok = get_from_ts(ts);
    if (!is_type_token(type_tok)) {
        parse_error(type_tok, "expected type name");
    }
    add_child(node, create_node(NODE_PRIMARY, type_tok.type, type_tok.value));

    while (is_pipe_token(peek_ts(ts))) {
        type_tok = get_from_ts(ts);
        (void)type_tok;
        type_tok = get_from_ts(ts);
        if (!is_type_token(type_tok)) {
            parse_error(type_tok, "expected type name after '|'");
        }
        add_child(node, create_node(NODE_PRIMARY, type_tok.type, type_tok.value));
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

ParseNode *parse_EXPRESSION(TokenStream *ts)
{
    ParseNode *node = create_node(NODE_EXPRESSION, TOKEN_NULL, NULL);

    add_child(node, parse_PRIMARY(ts));
    while (is_operator_token(peek_ts(ts))) {
        Token op = get_from_ts(ts);
        add_child(node, create_node(NODE_OPERATOR, op.type, op.value));
        add_child(node, parse_PRIMARY(ts));
    }

    return node;
}

static ParseNode *parse_PRIMARY(TokenStream *ts)
{
    Token tok = peek_ts(ts);

    if (tok.type == TOKEN_NUMBER || tok.type == TOKEN_STRING || tok.type == TOKEN_CHAR ||
        is_bool_literal(tok)) {
        tok = get_from_ts(ts);
        return create_node(NODE_PRIMARY, tok.type, tok.value);
    }

    if (tok.type == TOKEN_IDENTIFIER) {
        ParseNode *base;

        tok = get_from_ts(ts);
        base = create_node(NODE_PRIMARY, tok.type, tok.value);

        if (peek_ts(ts).type == TOKEN_LPAREN) {
            ParseNode *call = create_node(NODE_CALL, TOKEN_NULL, NULL);

            add_child(call, base);
            expect(ts, TOKEN_LPAREN);
            add_child(call, parse_ARGUMENTS(ts));
            expect(ts, TOKEN_RPAREN);
            return call;
        }

        return base;
    }

    if (tok.type == TOKEN_LPAREN) {
        expect(ts, TOKEN_LPAREN);
        ParseNode *expr = parse_EXPRESSION(ts);
        expect(ts, TOKEN_RPAREN);
        return expr;
    }

    parse_error(tok, "expected expression");
    return NULL;
}

static ParseNode *parse_ARGUMENTS(TokenStream *ts)
{
    ParseNode *node = create_node(NODE_ARGUMENTS, TOKEN_NULL, NULL);

    if (peek_ts(ts).type == TOKEN_RPAREN) {
        add_child(node, create_node(NODE_EPSILON, TOKEN_NULL, "epsilon"));
        return node;
    }

    while (1) {
        add_child(node, parse_EXPRESSION(ts));
        if (peek_ts(ts).type != TOKEN_COMMA) {
            break;
        }
        get_from_ts(ts);
    }

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
