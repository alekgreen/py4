#include <stdio.h>
#include <ctype.h>

void skip_spaces(char **input)
{
    if (!input || !*input) return;
    while (**input == ' ') (*input)++;
}

int parse_term(char **input)
{
    skip_spaces(input);
    const char *start = *input;
    while (isdigit(**input)) (*input)++;
    if (*input == start) return 0;
    return 1;
}

int parse_expr(char **input)
{
    if (!parse_term(input)) return 0;
    skip_spaces(input);
    while(**input == '+') {
        (*input)++;
        if (!parse_term(input)) return 0;
        skip_spaces(input);
    }
    return 1;
}

int main()
{
    char *tests[] = {
        "1+2+3",
        "42",
        "+3",
        "1+",
        "7+   8 +9",
        NULL
    };
    for (int i = 0; tests[i]; i++) {
        char *input = tests[i];
        printf("Parsing '%s' -> ", tests[i]);
        if (parse_expr(&input) && *input == '\0')
            printf("OK\n");
        else
            printf("FAIL ('%s')\n", input);
    }
    return 0;
}