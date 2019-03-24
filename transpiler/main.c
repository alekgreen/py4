#include <stdio.h>
#include <string.h>

#include "codegen.h"
#include "lexer.h"
#include "parse.h"
#include "semantic.h"

int main(int argc, char **argv)
{
    const char *input_path = "example0.p4";
    int show_tokens = 0;
    int show_tree = 0;
    FILE *fp;
    TokenStream *ts;
    ParseNode *root;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0) {
            show_tokens = 1;
        } else if (strcmp(argv[i], "--tree") == 0) {
            show_tree = 1;
        } else {
            input_path = argv[i];
        }
    }

    fp = fopen(input_path, "r");

    if (!fp) {
        perror("fopen");
        return 1;
    }

    ts = lexer(fp);

    if (show_tokens) {
        debug_print_ts(ts);
        reset_ts(ts);
    }

    root = parse(ts);
    if (show_tree) {
        print_tree(root, 0);
    }

    typecheck_program(root);
    emit_c_program(stdout, root);

    free_tree(root);
    free_token_stream(ts);

    fclose(fp);
    return 0;
}
