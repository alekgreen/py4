#include <stdio.h>
#include "lexer.h"

int main(void)
{
    FILE *fp = fopen("example0.p4", "r");
    if (!fp) {
        perror("fopen");
        return 1;
    }

    lexer(fp);
    fclose(fp);
    return 0;
}
