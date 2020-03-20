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
static ParseNode *parse_TUPLE_TARGET_ITEM(TokenStream *ts);
static ParseNode *parse_TYPE_ATOM(TokenStream *ts);
static char *build_type_node_name(const ParseNode *node);
static ParseNode *parse_OR(TokenStream *ts);
static ParseNode *parse_AND(TokenStream *ts);
static ParseNode *parse_NOT(TokenStream *ts);
static ParseNode *parse_COMPARISON(TokenStream *ts);
static ParseNode *parse_TERM(TokenStream *ts);
static ParseNode *parse_FACTOR(TokenStream *ts);
static ParseNode *parse_UNARY(TokenStream *ts);
static ParseNode *parse_PRIMARY(TokenStream *ts);
static ParseNode *parse_SLICE_BOUND(TokenStream *ts);

static int is_json_from_string_target(const ParseNode *node)
{
    const ParseNode *field;

    if (node == NULL || node->kind != NODE_FIELD_ACCESS || node->child_count != 2) {
        return 0;
    }

    field = node->children[1];
    return field != NULL &&
        field->kind == NODE_PRIMARY &&
        field->value != NULL &&
        strcmp(field->value, "from_string") == 0;
}

static void parse_skip_container_layout(TokenStream *ts)
{
    while (peek_ts(ts).type == TOKEN_NEWLINE ||
           peek_ts(ts).type == TOKEN_INDENT ||
           peek_ts(ts).type == TOKEN_DEDENT) {
        get_from_ts(ts);
    }
}

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

static ParseNode *parse_SLICE_BOUND(TokenStream *ts)
{
    if (peek_ts(ts).type == TOKEN_COLON || peek_ts(ts).type == TOKEN_RBRACKET) {
        return create_node(NODE_EPSILON, TOKEN_NULL, "epsilon");
    }
    return parse_EXPRESSION(ts);
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
        char *part = build_type_node_name(child);
        size_t value_len = strlen(part);

        if (used + value_len + 3 >= sizeof(buffer)) {
            free(part);
            parse_error_at_node(node, "tuple type name is too long");
        }
        memcpy(buffer + used, part, value_len);
        used += value_len;
        free(part);
        if (i + 1 < node->child_count) {
            buffer[used++] = ',';
        }
    }
    buffer[used++] = ')';
    buffer[used] = '\0';
    return parse_dup_string(buffer);
}

static char *build_type_node_name(const ParseNode *node)
{
    char buffer[256];
    size_t used = 0;

    if (node == NULL) {
        parse_error_at_node(node, "missing type annotation node");
    }
    if (node->kind == NODE_PRIMARY) {
        return parse_dup_string(node->value);
    }
    if (node->kind != NODE_TYPE) {
        parse_error_at_node(node, "expected a type annotation while building a type name");
    }

    for (size_t i = 0; i < node->child_count; i++) {
        char *part = build_type_node_name(node->children[i]);
        size_t value_len = strlen(part);

        if (used + value_len + 4 >= sizeof(buffer)) {
            free(part);
            parse_error_at_node(node, "type name is too long");
        }
        memcpy(buffer + used, part, value_len);
        used += value_len;
        free(part);
        if (i + 1 < node->child_count) {
            memcpy(buffer + used, " | ", 3);
            used += 3;
        }
    }
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
        ParseNode *member_node;
        char *member_name;
        char *type_name;

        expect(ts, TOKEN_LBRACKET);
        member_node = parse_TYPE(ts);
        if (member_node == NULL || member_node->child_count == 0) {
            parse_error(type_tok, "expected type name inside list[...]");
        }
        expect(ts, TOKEN_RBRACKET);

        member_name = build_type_node_name(member_node);
        type_name = parse_join_type_name("list", member_name);
        node = create_node(NODE_PRIMARY, TOKEN_KEYWORD, type_name);
        node->line = type_tok.line;
        node->column = type_tok.column;
        node->source_path = parse_dup_string(type_tok.path);
        node->source_line = parse_dup_string(type_tok.line_text);
        add_child(node, member_node);
        free(member_name);
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
        char *qualified_name = parse_dup_string(type_tok.value);

        while (peek_ts(ts).type == TOKEN_DOT) {
            Token member_tok;
            char *joined;
            size_t len;

            expect(ts, TOKEN_DOT);
            member_tok = expect(ts, TOKEN_IDENTIFIER);
            len = strlen(qualified_name) + strlen(member_tok.value) + 2;
            joined = malloc(len);
            if (joined == NULL) {
                perror("malloc");
                exit(1);
            }
            snprintf(joined, len, "%s.%s", qualified_name, member_tok.value);
            free(qualified_name);
            qualified_name = joined;
        }

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
    char *type_name;
    ParseNode *key_type;
    ParseNode *value_type;
    char *key_name;
    char *value_name;
    size_t len;

    expect(ts, TOKEN_LBRACKET);
    key_type = parse_TYPE(ts);
    expect(ts, TOKEN_COMMA);
    value_type = parse_TYPE(ts);
    expect(ts, TOKEN_RBRACKET);

    key_name = build_type_node_name(key_type);
    value_name = build_type_node_name(value_type);
    len = strlen("dict[") + strlen(key_name) + strlen(", ") + strlen(value_name) + strlen("]") + 1;
    type_name = malloc(len);
    if (type_name == NULL) {
        perror("malloc");
        exit(1);
    }
    snprintf(type_name, len, "dict[%s, %s]", key_name, value_name);
    node = create_node(NODE_PRIMARY, TOKEN_KEYWORD, type_name);
    node->line = dict_tok.line;
    node->column = dict_tok.column;
    node->source_path = parse_dup_string(dict_tok.path);
    node->source_line = parse_dup_string(dict_tok.line_text);
    add_child(node, key_type);
    add_child(node, value_type);
    free(key_name);
    free(value_name);
    free(type_name);
    return node;
}

static ParseNode *parse_TUPLE_TYPE(TokenStream *ts, Token lparen_tok)
{
    ParseNode *tuple_node = create_node_from_token(NODE_PRIMARY, lparen_tok);
    char *type_name;

    add_child(tuple_node, parse_TYPE(ts));
    if (peek_ts(ts).type != TOKEN_COMMA) {
        parse_error(peek_ts(ts), "tuple type must have at least two elements");
    }

    while (peek_ts(ts).type == TOKEN_COMMA) {
        get_from_ts(ts);
        add_child(tuple_node, parse_TYPE(ts));
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
            ParseNode *index_expr;

            expect(ts, TOKEN_LBRACKET);
            index_expr = parse_SLICE_BOUND(ts);
            index_node = create_node(
                peek_ts(ts).type == TOKEN_COLON ? NODE_SLICE : NODE_INDEX,
                TOKEN_NULL,
                NULL);
            add_child(index_node, node);
            add_child(index_node, index_expr);
            if (peek_ts(ts).type == TOKEN_COLON) {
                expect(ts, TOKEN_COLON);
                add_child(index_node, parse_SLICE_BOUND(ts));
            }
            expect(ts, TOKEN_RBRACKET);
            node = index_node;
            continue;
        }

        break;
    }

    if (node->kind == NODE_PRIMARY && peek_ts(ts).type == TOKEN_COMMA) {
        ParseNode *tuple_node = create_node_from_token(NODE_TUPLE_TARGET, tok);

        add_child(tuple_node, node);
        while (peek_ts(ts).type == TOKEN_COMMA) {
            get_from_ts(ts);
            add_child(tuple_node, parse_TUPLE_TARGET_ITEM(ts));
        }
        return tuple_node;
    }

    return node;
}

static ParseNode *parse_TUPLE_TARGET_ITEM(TokenStream *ts)
{
    Token look = peek_ts(ts);

    if (look.type == TOKEN_LPAREN) {
        look = expect(ts, TOKEN_LPAREN);
        return parse_TUPLE_TARGET(ts, look);
    }

    return create_node_from_token(NODE_PRIMARY, expect(ts, TOKEN_IDENTIFIER));
}

static ParseNode *parse_TUPLE_TARGET(TokenStream *ts, Token lparen_tok)
{
    ParseNode *node = create_node_from_token(NODE_TUPLE_TARGET, lparen_tok);

    add_child(node, parse_TUPLE_TARGET_ITEM(ts));
    if (peek_ts(ts).type != TOKEN_COMMA) {
        parse_error(peek_ts(ts), "tuple target must have at least two elements");
    }

    while (peek_ts(ts).type == TOKEN_COMMA) {
        get_from_ts(ts);
        add_child(node, parse_TUPLE_TARGET_ITEM(ts));
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
        parse_is_bool_literal(tok) || parse_is_keyword_token(tok, "None")) {
        tok = get_from_ts(ts);
        base = create_node_from_token(NODE_PRIMARY, tok);
    } else if (tok.type == TOKEN_IDENTIFIER) {
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
            if (is_json_from_string_target(base)) {
                ParseNode *typed_call = create_node(NODE_TYPED_CALL, TOKEN_NULL, NULL);

                expect(ts, TOKEN_LBRACKET);
                add_child(typed_call, base);
                add_child(typed_call, parse_TYPE(ts));
                expect(ts, TOKEN_RBRACKET);
                expect(ts, TOKEN_LPAREN);
                add_child(typed_call, parse_ARGUMENTS(ts));
                expect(ts, TOKEN_RPAREN);
                base = typed_call;
                continue;
            }

            ParseNode *index_node;
            ParseNode *index_expr;

            expect(ts, TOKEN_LBRACKET);
            index_expr = parse_SLICE_BOUND(ts);
            index_node = create_node(
                peek_ts(ts).type == TOKEN_COLON ? NODE_SLICE : NODE_INDEX,
                TOKEN_NULL,
                NULL);
            add_child(index_node, base);
            add_child(index_node, index_expr);
            if (peek_ts(ts).type == TOKEN_COLON) {
                expect(ts, TOKEN_COLON);
                add_child(index_node, parse_SLICE_BOUND(ts));
            }
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
    parse_skip_container_layout(ts);
    if (peek_ts(ts).type == TOKEN_RBRACKET) {
        expect(ts, TOKEN_RBRACKET);
        return node;
    }

    while (1) {
        add_child(node, parse_EXPRESSION(ts));
        parse_skip_container_layout(ts);
        if (peek_ts(ts).type != TOKEN_COMMA) {
            break;
        }
        get_from_ts(ts);
        parse_skip_container_layout(ts);
    }

    parse_skip_container_layout(ts);
    expect(ts, TOKEN_RBRACKET);
    return node;
}

static ParseNode *parse_DICT_LITERAL(TokenStream *ts)
{
    ParseNode *node = create_node(NODE_DICT_LITERAL, TOKEN_NULL, NULL);

    expect(ts, TOKEN_LBRACE);
    parse_skip_container_layout(ts);
    if (peek_ts(ts).type == TOKEN_RBRACE) {
        expect(ts, TOKEN_RBRACE);
        return node;
    }

    while (1) {
        add_child(node, parse_EXPRESSION(ts));
        expect(ts, TOKEN_COLON);
        parse_skip_container_layout(ts);
        add_child(node, parse_EXPRESSION(ts));
        parse_skip_container_layout(ts);
        if (peek_ts(ts).type != TOKEN_COMMA) {
            break;
        }
        get_from_ts(ts);
        parse_skip_container_layout(ts);
    }

    parse_skip_container_layout(ts);
    expect(ts, TOKEN_RBRACE);
    return node;
}

static ParseNode *parse_ARGUMENTS(TokenStream *ts)
{
    ParseNode *node = create_node(NODE_ARGUMENTS, TOKEN_NULL, NULL);

    parse_skip_container_layout(ts);
    if (peek_ts(ts).type == TOKEN_RPAREN) {
        add_child(node, create_node(NODE_EPSILON, TOKEN_NULL, "epsilon"));
        return node;
    }

    while (1) {
        add_child(node, parse_EXPRESSION(ts));
        parse_skip_container_layout(ts);
        if (peek_ts(ts).type != TOKEN_COMMA) {
            break;
        }
        get_from_ts(ts);
        parse_skip_container_layout(ts);
    }

    return node;
}
