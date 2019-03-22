#include <stdio.h>
#include "lexer.h"
#include "parse.h"

int main(int argc, char **argv)
{
    const char *input_path = argc > 1 ? argv[1] : "example0.p4";
    FILE *fp = fopen(input_path, "r");

    if (!fp) {
        perror("fopen");
        return 1;
    }

    TokenStream *ts = lexer(fp);

    debug_print_ts(ts);
    reset_ts(ts);

    ParseNode *root = parse(ts);
    print_tree(root, 0);

    free_tree(root);
    free_token_stream(ts);

    fclose(fp);
    return 0;
}
