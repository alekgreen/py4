#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdio.h>

#include "module_loader.h"
#include "semantic.h"

void emit_c_program(FILE *out, const LoadedProgram *program, const SemanticInfo *info);

#endif
