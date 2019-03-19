#ifndef PARSE_H
#define PARSE_H
#include "token.h"
#include "lexer.h"

typedef enum {
    NODE_S,
    NODE_T,
    NODE_STATEMENT,
    NODE_STATEMENT_TAIL,
    NODE_EXPRESSION,
    NODE_EXPRESSION_PRIME,
    NODE_TERMINAL,
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
ParseNode *parse_T(TokenStream *ts);
ParseNode *parse_STATEMENT(TokenStream *ts);
ParseNode *parse_STATEMENT_TAIL(TokenStream *ts);
ParseNode *parse_EXPRESSION(TokenStream *ts);
ParseNode *parse_EXPRESSION_PRIME(TokenStream *ts);
ParseNode *parse_TERMINAL(TokenStream *ts);
ParseNode *parse(TokenStream *ts);

#endif