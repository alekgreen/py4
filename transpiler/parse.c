#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parse.h"


ParseNode *create_node(NodeKind kind, TokenType token_type, const char *value)
{
    ParseNode *node = malloc(sizeof(ParseNode));
    node->kind = kind;
    node->token_type = token_type;
    node->value = value ? strdup(value) : NULL;
    node->children = NULL;
    node->child_count = 0;
    return node;
}

void add_child(ParseNode *parent, ParseNode *child)
{
    parent->children = realloc(parent->children, sizeof(ParseNode*) * (parent->child_count + 1));
    parent->children[parent->child_count++] = child;
}

void free_tree(ParseNode *n)
{
    if (!n) return;
    for (size_t i = 0; i < n->child_count; i++)
        free_tree(n->children[i]);
    free(n->children);
    if (n->value) free (n->value);
    free(n);
}

void skip_newlines(TokenStream *ts) {
    while (peek_ts(ts).type == TOKEN_NEWLINE) get_from_ts(ts);
}

const char *node_kind_to_str(NodeKind kind)
{
    switch (kind) {
        case NODE_S:                return "S";
        case NODE_T:                return "T";
        case NODE_STATEMENT:        return "STATEMENT";
        case NODE_STATEMENT_TAIL:   return "STATEMENT_TAIL";
        case NODE_EXPRESSION:       return "EXPRESSION";
        case NODE_EXPRESSION_PRIME: return "EXPRESSION'";
        case NODE_TERMINAL:         return "TERMINAL";
        case NODE_COLON:            return "COLON";
        case NODE_ASSIGN:           return "ASSIGN";
        case NODE_OPERATOR:         return "OPERATOR";
        case NODE_EPSILON:          return "ε";
        default:                    return "UNKNOWN";
    }
}

void print_tree(ParseNode *n, int depth)
{
    if (!n) return;
    for (int i = 0; i < depth; i++) {
        if (i == depth - 1)
            printf("├── ");
        else
            printf("│   ");
    }
    printf("%s", node_kind_to_str(n->kind));
    if (n->value) printf(" (%s, %s)", token_type_to_str(n->token_type), n->value);
    printf("\n");
    for (size_t i = 0; i < n->child_count; i++)
        print_tree(n->children[i], depth + 1);
}

ParseNode *parse_S(TokenStream *ts)
{   
    skip_newlines(ts);
    ParseNode *root = create_node(NODE_S, TOKEN_NULL, NULL);

    if (peek_ts(ts).type == TOKEN_EOF)
        return create_node(NODE_EPSILON, TOKEN_NULL, "ε");

    while (peek_ts(ts).type == TOKEN_IDENTIFIER) {
        add_child(root, parse_STATEMENT(ts));

        if (peek_ts(ts).type == TOKEN_NEWLINE)
            get_from_ts(ts);
        else
            break;
    }
    return root;
}

ParseNode *parse_STATEMENT(TokenStream *ts)
{
    ParseNode *n = create_node(NODE_STATEMENT, TOKEN_NULL, NULL);
    Token id = expect(ts, TOKEN_IDENTIFIER);
    add_child(n, create_node(NODE_TERMINAL, id.type, id.value));
    add_child(n, parse_STATEMENT_TAIL(ts));
    return n;
}

ParseNode *parse_STATEMENT_TAIL(TokenStream *ts)
{
    ParseNode *n = create_node(NODE_STATEMENT_TAIL, TOKEN_NULL, NULL);
    Token look = peek_ts(ts);
    if (look.type == TOKEN_COLON) {
        expect(ts, TOKEN_COLON);
        add_child(n, create_node(NODE_COLON, TOKEN_COLON, ":"));
        Token type_tok = expect(ts, TOKEN_KEYWORD);
        add_child(n, create_node(NODE_TERMINAL, type_tok.type, type_tok.value));
        expect(ts, TOKEN_ASSIGN);
        add_child(n, create_node(NODE_ASSIGN, TOKEN_ASSIGN, "="));
        add_child(n, parse_EXPRESSION(ts));
    } else if (look.type == TOKEN_ASSIGN) {
        expect(ts, TOKEN_ASSIGN);
        add_child(n, create_node(NODE_ASSIGN, TOKEN_ASSIGN, "="));
        add_child(n, parse_EXPRESSION(ts));
    } else {
        fprintf(stderr, "Parse error in STATEMENT_TAIL: unexpected token %s\n", look.value);
        exit(1);
    }
    return n;
}

ParseNode *parse_EXPRESSION(TokenStream *ts)
{
    ParseNode *n = create_node(NODE_EXPRESSION, TOKEN_NULL, NULL);
    add_child(n, parse_TERMINAL(ts));
    add_child(n, parse_EXPRESSION_PRIME(ts));
    return n;
}

ParseNode *parse_EXPRESSION_PRIME(TokenStream *ts)
{
    Token look = peek_ts(ts);
    if (look.type == TOKEN_PLUS || look.type == TOKEN_MINUS || look.type == TOKEN_OPERATOR) {
        ParseNode *n = create_node(NODE_EXPRESSION_PRIME, TOKEN_NULL, NULL);
        Token op = get_from_ts(ts);
        add_child(n, create_node(NODE_OPERATOR, op.type, op.value));
        add_child(n, parse_TERMINAL(ts));
        add_child(n, parse_EXPRESSION_PRIME(ts));
        return n;
    }
    return create_node(NODE_EPSILON, TOKEN_NULL, "ε");
}

ParseNode *parse_TERMINAL(TokenStream *ts)
{
    Token t = peek_ts(ts);
    ParseNode *n = create_node(NODE_TERMINAL, TOKEN_NULL, NULL);
    if (t.type == TOKEN_NUMBER || t.type == TOKEN_IDENTIFIER) {
        t = get_from_ts(ts);
        n->value = strdup(t.value);
        return n;
    }
    fprintf(stderr, "Parse error: expected TERMINAL but got %s\n", t.value);
    exit(1);
}

ParseNode *parse(TokenStream *ts)
{
    ParseNode *root = parse_S(ts);
    Token look = peek_ts(ts);
    if (look.type != TOKEN_EOF) {
        fprintf(stderr, "Warning: tokens remaining after parse\n");
    }
    return root;
}
