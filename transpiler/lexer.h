#ifndef LEXER_H
#define LEXER_H
#include "token.h"

Token next_token(FILE *fp);
TokenStream *lexer(FILE *fp);

#endif