#ifndef MODULE_LOADER_H
#define MODULE_LOADER_H

#include "parse.h"

ParseNode *load_program_from_entry(const char *input_path, int show_tokens);

#endif
