#ifndef PARSE_H
#define PARSE_H

#include <stddef.h>

#include "token.h"
#include "lexer.h"

typedef enum {
    NODE_S,
    NODE_STATEMENT,
    NODE_SIMPLE_STATEMENT,
    NODE_STATEMENT_TAIL,
    NODE_FUNCTION_DEF,
    NODE_PARAMETERS,
    NODE_PARAMETER,
    NODE_RETURN_TYPE,
    NODE_SUITE,
    NODE_EXPRESSION_STATEMENT,
    NODE_EXPRESSION,
    NODE_PRIMARY,
    NODE_CALL,
    NODE_ARGUMENTS,
    NODE_COLON,
    NODE_ASSIGN,
    NODE_OPERATOR,
    NODE_EPSILON
} NodeKind;

typedef struct ParseNode {
    NodeKind kind;
    TokenType token_type;
    char *value;
    struct ParseNode **children;
    size_t child_count;
} ParseNode;

ParseNode *create_node(NodeKind kind, TokenType token_type, const char *value);
void add_child(ParseNode *parent, ParseNode *child);
void free_tree(ParseNode *node);
const char *node_kind_to_str(NodeKind kind);
void print_tree(ParseNode *node, int depth);

ParseNode *parse_S(TokenStream *ts);
ParseNode *parse_STATEMENT(TokenStream *ts);
ParseNode *parse_STATEMENT_TAIL(TokenStream *ts);
ParseNode *parse_EXPRESSION(TokenStream *ts);
ParseNode *parse(TokenStream *ts);

#endif
