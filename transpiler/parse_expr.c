#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse_internal.h"

static ParseNode *parse_ARGUMENTS(TokenStream *ts);
static ParseNode *parse_LIST_LITERAL(TokenStream *ts);
static ParseNode *parse_TYPE_ATOM(TokenStream *ts);
static ParseNode *parse_OR(TokenStream *ts);
static ParseNode *parse_AND(TokenStream *ts);
static ParseNode *parse_NOT(TokenStream *ts);
static ParseNode *parse_COMPARISON(TokenStream *ts);
static ParseNode *parse_TERM(TokenStream *ts);
static ParseNode *parse_FACTOR(TokenStream *ts);
static ParseNode *parse_UNARY(TokenStream *ts);
static ParseNode *parse_PRIMARY(TokenStream *ts);

static ParseNode *wrap_expression(ParseNode *node)
{
    ParseNode *expr;

    if (node->kind == NODE_EXPRESSION) {
        return node;
    }

    expr = create_node(NODE_EXPRESSION, TOKEN_NULL, NULL);
    add_child(expr, node);
    return expr;
}

static ParseNode *make_unary_expression(Token op, ParseNode *operand)
{
    ParseNode *expr = create_node(NODE_EXPRESSION, TOKEN_NULL, NULL);

    add_child(expr, create_node(NODE_OPERATOR, op.type, op.value));
    add_child(expr, operand);
    return expr;
}

static ParseNode *make_binary_expression(ParseNode *lhs, Token op, ParseNode *rhs)
{
    ParseNode *expr = create_node(NODE_EXPRESSION, TOKEN_NULL, NULL);

    add_child(expr, lhs);
    add_child(expr, create_node(NODE_OPERATOR, op.type, op.value));
    add_child(expr, rhs);
    return expr;
}

static int matches_operator(Token tok, const char *value)
{
    return parse_is_operator_token(tok) && strcmp(tok.value, value) == 0;
}

static int matches_keyword_operator(Token tok, const char *value)
{
    return tok.type == TOKEN_KEYWORD && strcmp(tok.value, value) == 0;
}

static int is_comparison_operator_token(Token tok)
{
    return matches_operator(tok, "==") || matches_operator(tok, "!=") ||
        matches_operator(tok, "<") || matches_operator(tok, ">") ||
        matches_operator(tok, "<=") || matches_operator(tok, ">=");
}

ParseNode *parse_TYPE(TokenStream *ts)
{
    ParseNode *node = create_node(NODE_TYPE, TOKEN_NULL, NULL);

    add_child(node, parse_TYPE_ATOM(ts));
    while (parse_is_pipe_token(peek_ts(ts))) {
        get_from_ts(ts);
        add_child(node, parse_TYPE_ATOM(ts));
    }

    return node;
}

static ParseNode *parse_TYPE_ATOM(TokenStream *ts)
{
    ParseNode *node;
    Token type_tok = get_from_ts(ts);

    if (!parse_is_type_token(type_tok)) {
        parse_error(type_tok, "expected type name");
    }

    if (parse_is_keyword_token(type_tok, "list")) {
        Token member_tok;
        char *type_name;

        expect(ts, TOKEN_LBRACKET);
        member_tok = get_from_ts(ts);
        if (!parse_is_type_token(member_tok)) {
            parse_error(member_tok, "expected type name inside list[...]");
        }
        if (strcmp(member_tok.value, "int") != 0 &&
            strcmp(member_tok.value, "float") != 0 &&
            strcmp(member_tok.value, "bool") != 0 &&
            strcmp(member_tok.value, "char") != 0) {
            parse_error(member_tok, "only list[int], list[float], list[bool], and list[char] are supported right now");
        }
        expect(ts, TOKEN_RBRACKET);

        type_name = parse_join_type_name("list", member_tok.value);
        node = create_node(NODE_PRIMARY, TOKEN_KEYWORD, type_name);
        free(type_name);
        return node;
    }

    return create_node(NODE_PRIMARY, type_tok.type, type_tok.value);
}

ParseNode *parse_ASSIGN_TARGET(TokenStream *ts)
{
    ParseNode *node;
    ParseNode *index_node;
    Token tok = expect(ts, TOKEN_IDENTIFIER);

    node = create_node(NODE_PRIMARY, tok.type, tok.value);
    while (peek_ts(ts).type == TOKEN_LBRACKET) {
        expect(ts, TOKEN_LBRACKET);
        index_node = create_node(NODE_INDEX, TOKEN_NULL, NULL);
        add_child(index_node, node);
        add_child(index_node, parse_EXPRESSION(ts));
        expect(ts, TOKEN_RBRACKET);
        node = index_node;
    }

    return node;
}

ParseNode *parse_EXPRESSION(TokenStream *ts)
{
    return wrap_expression(parse_OR(ts));
}

static ParseNode *parse_OR(TokenStream *ts)
{
    ParseNode *node = parse_AND(ts);

    while (matches_keyword_operator(peek_ts(ts), "or")) {
        Token op = get_from_ts(ts);
        ParseNode *rhs = parse_AND(ts);

        node = make_binary_expression(node, op, rhs);
    }

    return node;
}

static ParseNode *parse_AND(TokenStream *ts)
{
    ParseNode *node = parse_NOT(ts);

    while (matches_keyword_operator(peek_ts(ts), "and")) {
        Token op = get_from_ts(ts);
        ParseNode *rhs = parse_NOT(ts);

        node = make_binary_expression(node, op, rhs);
    }

    return node;
}

static ParseNode *parse_NOT(TokenStream *ts)
{
    Token tok = peek_ts(ts);

    if (matches_keyword_operator(tok, "not")) {
        tok = get_from_ts(ts);
        return make_unary_expression(tok, parse_NOT(ts));
    }

    return parse_COMPARISON(ts);
}

static ParseNode *parse_COMPARISON(TokenStream *ts)
{
    ParseNode *binary;
    ParseNode *lhs_node;
    ParseNode *op_node;
    ParseNode *rhs_node;
    ParseNode *lhs = parse_TERM(ts);
    ParseNode *node = create_node(NODE_EXPRESSION, TOKEN_NULL, NULL);

    add_child(node, lhs);
    while (is_comparison_operator_token(peek_ts(ts))) {
        Token op = get_from_ts(ts);
        ParseNode *rhs = parse_TERM(ts);

        add_child(node, create_node(NODE_OPERATOR, op.type, op.value));
        add_child(node, rhs);
    }

    if (node->child_count == 1) {
        ParseNode *only_child = node->children[0];

        node->children[0] = NULL;
        free_tree(node);
        return only_child;
    }

    if (node->child_count != 3) {
        return node;
    }

    lhs_node = node->children[0];
    op_node = node->children[1];
    rhs_node = node->children[2];
    binary = create_node(NODE_EXPRESSION, TOKEN_NULL, NULL);
    add_child(binary, lhs_node);
    add_child(binary, op_node);
    add_child(binary, rhs_node);
    free(node->children);
    free(node->value);
    free(node);
    return binary;
}

static ParseNode *parse_TERM(TokenStream *ts)
{
    ParseNode *node = parse_FACTOR(ts);

    while (peek_ts(ts).type == TOKEN_PLUS || peek_ts(ts).type == TOKEN_MINUS) {
        Token op = get_from_ts(ts);
        ParseNode *rhs = parse_FACTOR(ts);

        node = make_binary_expression(node, op, rhs);
    }

    return node;
}

static ParseNode *parse_FACTOR(TokenStream *ts)
{
    ParseNode *node = parse_UNARY(ts);

    while (matches_operator(peek_ts(ts), "*") || matches_operator(peek_ts(ts), "/")) {
        Token op = get_from_ts(ts);
        ParseNode *rhs = parse_UNARY(ts);

        node = make_binary_expression(node, op, rhs);
    }

    return node;
}

static ParseNode *parse_UNARY(TokenStream *ts)
{
    Token tok = peek_ts(ts);

    if (tok.type == TOKEN_PLUS || tok.type == TOKEN_MINUS) {
        tok = get_from_ts(ts);
        return make_unary_expression(tok, parse_UNARY(ts));
    }

    return parse_PRIMARY(ts);
}

static ParseNode *parse_PRIMARY(TokenStream *ts)
{
    ParseNode *base;
    Token tok = peek_ts(ts);

    if (tok.type == TOKEN_NUMBER || tok.type == TOKEN_STRING || tok.type == TOKEN_CHAR ||
        parse_is_bool_literal(tok)) {
        tok = get_from_ts(ts);
        return create_node(NODE_PRIMARY, tok.type, tok.value);
    }

    if (tok.type == TOKEN_IDENTIFIER) {
        tok = get_from_ts(ts);
        base = create_node(NODE_PRIMARY, tok.type, tok.value);
    } else if (tok.type == TOKEN_LPAREN) {
        expect(ts, TOKEN_LPAREN);
        base = parse_EXPRESSION(ts);
        expect(ts, TOKEN_RPAREN);
    } else if (tok.type == TOKEN_LBRACKET) {
        base = parse_LIST_LITERAL(ts);
    } else {
        parse_error(tok, "expected expression");
        return NULL;
    }

    while (1) {
        if (peek_ts(ts).type == TOKEN_LPAREN) {
            ParseNode *call;

            if (base->kind != NODE_PRIMARY || base->token_type != TOKEN_IDENTIFIER) {
                parse_error(peek_ts(ts), "call target must be an identifier");
            }

            call = create_node(NODE_CALL, TOKEN_NULL, NULL);
            add_child(call, base);
            expect(ts, TOKEN_LPAREN);
            add_child(call, parse_ARGUMENTS(ts));
            expect(ts, TOKEN_RPAREN);
            base = call;
            continue;
        }

        if (peek_ts(ts).type == TOKEN_DOT) {
            ParseNode *method_call;
            Token method_name;

            expect(ts, TOKEN_DOT);
            method_name = expect(ts, TOKEN_IDENTIFIER);
            method_call = create_node(NODE_METHOD_CALL, TOKEN_NULL, NULL);
            add_child(method_call, base);
            add_child(method_call, create_node(NODE_PRIMARY, method_name.type, method_name.value));
            expect(ts, TOKEN_LPAREN);
            add_child(method_call, parse_ARGUMENTS(ts));
            expect(ts, TOKEN_RPAREN);
            base = method_call;
            continue;
        }

        if (peek_ts(ts).type == TOKEN_LBRACKET) {
            ParseNode *index_node = create_node(NODE_INDEX, TOKEN_NULL, NULL);

            expect(ts, TOKEN_LBRACKET);
            add_child(index_node, base);
            add_child(index_node, parse_EXPRESSION(ts));
            expect(ts, TOKEN_RBRACKET);
            base = index_node;
            continue;
        }

        break;
    }

    return base;
}

static ParseNode *parse_LIST_LITERAL(TokenStream *ts)
{
    ParseNode *node = create_node(NODE_LIST_LITERAL, TOKEN_NULL, NULL);

    expect(ts, TOKEN_LBRACKET);
    if (peek_ts(ts).type == TOKEN_RBRACKET) {
        expect(ts, TOKEN_RBRACKET);
        return node;
    }

    while (1) {
        add_child(node, parse_EXPRESSION(ts));
        if (peek_ts(ts).type != TOKEN_COMMA) {
            break;
        }
        get_from_ts(ts);
    }

    expect(ts, TOKEN_RBRACKET);
    return node;
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
