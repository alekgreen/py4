#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>

#include "parse.h"

void emit_c_program(FILE *out, const ParseNode *root);

#endif
