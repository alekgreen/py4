#ifndef MODULE_LOADER_H
#define MODULE_LOADER_H

#include "parse.h"

typedef struct {
    char *name;
    char *path;
    ParseNode *root;
} LoadedModule;

typedef struct {
    LoadedModule *modules;
    size_t module_count;
    size_t module_capacity;
    size_t entry_index;
    ParseNode *emission_root;
} LoadedProgram;

LoadedProgram *load_program_from_entry(const char *input_path, int show_tokens);
void free_loaded_program(LoadedProgram *program);

#endif
