#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parse_internal.h"

static ParseNode *parse_ARGUMENTS(TokenStream *ts);
static ParseNode *parse_LIST_LITERAL(TokenStream *ts);
static ParseNode *parse_DICT_LITERAL(TokenStream *ts);
static ParseNode *parse_DICT_TYPE(TokenStream *ts, Token dict_tok);
static ParseNode *parse_TUPLE_TYPE(TokenStream *ts, Token lparen_tok);
static ParseNode *parse_TUPLE_LITERAL(TokenStream *ts, Token lparen_tok);
static ParseNode *parse_TUPLE_TARGET(TokenStream *ts, Token lparen_tok);
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
    ParseNode *expr = create_node_from_token(NODE_EXPRESSION, op);

    add_child(expr, create_node_from_token(NODE_OPERATOR, op));
    add_child(expr, operand);
    return expr;
}

static ParseNode *make_binary_expression(ParseNode *lhs, Token op, ParseNode *rhs)
{
    ParseNode *expr = create_node_from_token(NODE_EXPRESSION, op);

    add_child(expr, lhs);
    add_child(expr, create_node_from_token(NODE_OPERATOR, op));
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
        matches_operator(tok, "<=") || matches_operator(tok, ">=") ||
        matches_keyword_operator(tok, "in");
}

static char *build_tuple_type_name(ParseNode *node)
{
    char buffer[256];
    size_t used = 0;

    buffer[used++] = '(';
    for (size_t i = 0; i < node->child_count; i++) {
        const ParseNode *child = node->children[i];
        size_t value_len = strlen(child->value);

        if (used + value_len + 3 >= sizeof(buffer)) {
            parse_error_at_node(node, "tuple type name is too long");
        }
        memcpy(buffer + used, child->value, value_len);
        used += value_len;
        if (i + 1 < node->child_count) {
            buffer[used++] = ',';
        }
    }
    buffer[used++] = ')';
    buffer[used] = '\0';
    return parse_dup_string(buffer);
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
            strcmp(member_tok.value, "char") != 0 &&
            strcmp(member_tok.value, "str") != 0) {
            parse_error(member_tok, "only list[int], list[float], list[bool], list[char], and list[str] are supported right now");
        }
        expect(ts, TOKEN_RBRACKET);

        type_name = parse_join_type_name("list", member_tok.value);
        node = create_node(NODE_PRIMARY, TOKEN_KEYWORD, type_name);
        node->line = type_tok.line;
        node->column = type_tok.column;
        node->source_path = parse_dup_string(type_tok.path);
        node->source_line = parse_dup_string(type_tok.line_text);
        free(type_name);
        return node;
    }

    if (parse_is_keyword_token(type_tok, "dict")) {
        return parse_DICT_TYPE(ts, type_tok);
    }

    if (type_tok.type == TOKEN_LPAREN) {
        return parse_TUPLE_TYPE(ts, type_tok);
    }

    if (!parse_is_type_token(type_tok)) {
        parse_error(type_tok, "expected type name");
    }

    if (type_tok.type == TOKEN_IDENTIFIER && peek_ts(ts).type == TOKEN_DOT) {
        Token member_tok;
        size_t len;
        char *qualified_name;

        expect(ts, TOKEN_DOT);
        member_tok = expect(ts, TOKEN_IDENTIFIER);
        len = strlen(type_tok.value) + strlen(member_tok.value) + 2;
        qualified_name = malloc(len);
        if (qualified_name == NULL) {
            perror("malloc");
            exit(1);
        }
        snprintf(qualified_name, len, "%s.%s", type_tok.value, member_tok.value);
        node = create_node(NODE_PRIMARY, TOKEN_IDENTIFIER, qualified_name);
        node->line = type_tok.line;
        node->column = type_tok.column;
        node->source_path = parse_dup_string(type_tok.path);
        node->source_line = parse_dup_string(type_tok.line_text);
        free(qualified_name);
        return node;
    }

    return create_node_from_token(NODE_PRIMARY, type_tok);
}

static ParseNode *parse_DICT_TYPE(TokenStream *ts, Token dict_tok)
{
    ParseNode *node;
    Token key_tok;
    Token value_tok;
    char *type_name;
    size_t len;

    expect(ts, TOKEN_LBRACKET);
    key_tok = get_from_ts(ts);
    if (!parse_is_type_token(key_tok) || strcmp(key_tok.value, "str") != 0) {
        parse_error(key_tok, "only dict[str, str] is supported right now");
    }
    expect(ts, TOKEN_COMMA);
    value_tok = get_from_ts(ts);
    if (!parse_is_type_token(value_tok) || strcmp(value_tok.value, "str") != 0) {
        parse_error(value_tok, "only dict[str, str] is supported right now");
    }
    expect(ts, TOKEN_RBRACKET);

    len = strlen("dict[str, str]") + 1;
    type_name = malloc(len);
    if (type_name == NULL) {
        perror("malloc");
        exit(1);
    }
    memcpy(type_name, "dict[str, str]", len);
    node = create_node(NODE_PRIMARY, TOKEN_KEYWORD, type_name);
    node->line = dict_tok.line;
    node->column = dict_tok.column;
    node->source_path = parse_dup_string(dict_tok.path);
    node->source_line = parse_dup_string(dict_tok.line_text);
    free(type_name);
    return node;
}

static ParseNode *parse_TUPLE_TYPE(TokenStream *ts, Token lparen_tok)
{
    ParseNode *tuple_node = create_node_from_token(NODE_PRIMARY, lparen_tok);
    char *type_name;

    add_child(tuple_node, parse_TYPE_ATOM(ts));
    if (peek_ts(ts).type != TOKEN_COMMA) {
        parse_error(peek_ts(ts), "tuple type must have at least two elements");
    }

    while (peek_ts(ts).type == TOKEN_COMMA) {
        get_from_ts(ts);
        add_child(tuple_node, parse_TYPE_ATOM(ts));
    }

    expect(ts, TOKEN_RPAREN);

    type_name = build_tuple_type_name(tuple_node);
    free(tuple_node->value);
    tuple_node->value = type_name;
    return tuple_node;
}

ParseNode *parse_ASSIGN_TARGET(TokenStream *ts)
{
    ParseNode *node;
    Token tok = peek_ts(ts);

    if (tok.type == TOKEN_LPAREN) {
        tok = expect(ts, TOKEN_LPAREN);
        return parse_TUPLE_TARGET(ts, tok);
    }

    tok = expect(ts, TOKEN_IDENTIFIER);

    node = create_node_from_token(NODE_PRIMARY, tok);
    while (1) {
        if (peek_ts(ts).type == TOKEN_DOT) {
            ParseNode *field_node;
            Token field_name;

            expect(ts, TOKEN_DOT);
            field_name = expect(ts, TOKEN_IDENTIFIER);
            field_node = create_node(NODE_FIELD_ACCESS, TOKEN_NULL, NULL);
            add_child(field_node, node);
            add_child(field_node, create_node_from_token(NODE_PRIMARY, field_name));
            node = field_node;
            continue;
        }

        if (peek_ts(ts).type == TOKEN_LBRACKET) {
            ParseNode *index_node;

            expect(ts, TOKEN_LBRACKET);
            index_node = create_node(NODE_INDEX, TOKEN_NULL, NULL);
            add_child(index_node, node);
            add_child(index_node, parse_EXPRESSION(ts));
            expect(ts, TOKEN_RBRACKET);
            node = index_node;
            continue;
        }

        break;
    }

    return node;
}

static ParseNode *parse_TUPLE_TARGET(TokenStream *ts, Token lparen_tok)
{
    ParseNode *node = create_node_from_token(NODE_TUPLE_TARGET, lparen_tok);
    Token look = peek_ts(ts);

    if (look.type == TOKEN_LPAREN) {
        look = expect(ts, TOKEN_LPAREN);
        add_child(node, parse_TUPLE_TARGET(ts, look));
    } else {
        add_child(node, create_node_from_token(NODE_PRIMARY, expect(ts, TOKEN_IDENTIFIER)));
    }
    if (peek_ts(ts).type != TOKEN_COMMA) {
        parse_error(peek_ts(ts), "tuple target must have at least two elements");
    }

    while (peek_ts(ts).type == TOKEN_COMMA) {
        get_from_ts(ts);
        look = peek_ts(ts);
        if (look.type == TOKEN_LPAREN) {
            look = expect(ts, TOKEN_LPAREN);
            add_child(node, parse_TUPLE_TARGET(ts, look));
        } else {
            add_child(node, create_node_from_token(NODE_PRIMARY, expect(ts, TOKEN_IDENTIFIER)));
        }
    }

    expect(ts, TOKEN_RPAREN);
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

        add_child(node, create_node_from_token(NODE_OPERATOR, op));
        add_child(node, rhs);
        if (matches_keyword_operator(op, "in")) {
            break;
        }
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
        return create_node_from_token(NODE_PRIMARY, tok);
    }

    if (tok.type == TOKEN_IDENTIFIER) {
        tok = get_from_ts(ts);
        base = create_node_from_token(NODE_PRIMARY, tok);
    } else if (tok.type == TOKEN_LPAREN) {
        tok = expect(ts, TOKEN_LPAREN);
        base = parse_TUPLE_LITERAL(ts, tok);
    } else if (tok.type == TOKEN_LBRACKET) {
        base = parse_LIST_LITERAL(ts);
    } else if (tok.type == TOKEN_LBRACE) {
        base = parse_DICT_LITERAL(ts);
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
            ParseNode *member_access;
            Token member_name;

            expect(ts, TOKEN_DOT);
            member_name = expect(ts, TOKEN_IDENTIFIER);
            if (peek_ts(ts).type == TOKEN_LPAREN) {
                member_access = create_node(NODE_METHOD_CALL, TOKEN_NULL, NULL);
                add_child(member_access, base);
                add_child(member_access, create_node_from_token(NODE_PRIMARY, member_name));
                expect(ts, TOKEN_LPAREN);
                add_child(member_access, parse_ARGUMENTS(ts));
                expect(ts, TOKEN_RPAREN);
            } else {
                member_access = create_node(NODE_FIELD_ACCESS, TOKEN_NULL, NULL);
                add_child(member_access, base);
                add_child(member_access, create_node_from_token(NODE_PRIMARY, member_name));
            }
            base = member_access;
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

static ParseNode *parse_TUPLE_LITERAL(TokenStream *ts, Token lparen_tok)
{
    ParseNode *first;

    if (peek_ts(ts).type == TOKEN_RPAREN) {
        parse_error(peek_ts(ts), "expected expression");
    }

    first = parse_EXPRESSION(ts);
    if (peek_ts(ts).type != TOKEN_COMMA) {
        expect(ts, TOKEN_RPAREN);
        return first;
    }

    {
        ParseNode *tuple_node = create_node_from_token(NODE_TUPLE_LITERAL, lparen_tok);

        add_child(tuple_node, first);
        while (peek_ts(ts).type == TOKEN_COMMA) {
            get_from_ts(ts);
            add_child(tuple_node, parse_EXPRESSION(ts));
        }
        expect(ts, TOKEN_RPAREN);
        return tuple_node;
    }
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

static ParseNode *parse_DICT_LITERAL(TokenStream *ts)
{
    ParseNode *node = create_node(NODE_DICT_LITERAL, TOKEN_NULL, NULL);

    expect(ts, TOKEN_LBRACE);
    if (peek_ts(ts).type == TOKEN_RBRACE) {
        expect(ts, TOKEN_RBRACE);
        return node;
    }

    while (1) {
        add_child(node, parse_EXPRESSION(ts));
        expect(ts, TOKEN_COLON);
        add_child(node, parse_EXPRESSION(ts));
        if (peek_ts(ts).type != TOKEN_COMMA) {
            break;
        }
        get_from_ts(ts);
    }

    expect(ts, TOKEN_RBRACE);
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
