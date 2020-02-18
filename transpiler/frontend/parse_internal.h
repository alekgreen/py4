#ifndef PARSE_INTERNAL_H
#define PARSE_INTERNAL_H

#include "parse.h"

char *parse_dup_string(const char *value);
char *parse_join_type_name(const char *container, const char *member);
int parse_is_keyword_token(Token tok, const char *value);
int parse_is_bool_literal(Token tok);
int parse_is_block_continuation(Token tok);
int parse_is_operator_token(Token tok);
int parse_is_type_token(Token tok);
int parse_is_pipe_token(Token tok);
void parse_error(Token tok, const char *message);
void parse_error_at_node(const ParseNode *node, const char *message);
Token parse_expect_keyword(TokenStream *ts, const char *value);
void parse_skip_newlines(TokenStream *ts);

ParseNode *parse_TYPE(TokenStream *ts);
ParseNode *parse_ASSIGN_TARGET(TokenStream *ts);
#endif
