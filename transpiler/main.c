#include <stdio.h>
#include <string.h>

#include "codegen.h"
#include "module_loader.h"
#include "parse.h"
#include "semantic.h"

int main(int argc, char **argv)
{
    const char *input_path = "example0.p4";
    int show_tokens = 0;
    int show_tree = 0;
    SemanticInfo *semantic;
    LoadedProgram *program;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0) {
            show_tokens = 1;
        } else if (strcmp(argv[i], "--tree") == 0) {
            show_tree = 1;
        } else {
            input_path = argv[i];
        }
    }

    program = load_program_from_entry(input_path, show_tokens);
    if (show_tree) {
        print_tree(program->emission_root, 0);
    }

    semantic = analyze_program(program);
    emit_c_program(stdout, program, semantic);

    free_semantic_info(semantic);
    free_loaded_program(program);
    return 0;
}
