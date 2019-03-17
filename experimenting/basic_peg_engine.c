#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef enum {
    P_LITERAL,
    P_SEQUENCE,
    P_CHOICE,
    P_ONE_OR_MORE,
    P_RULE_REF
} PegType;

typedef struct Peg {
    PegType type;
    const char *literal;
    struct Peg *a, *b, *sub;
} Peg;

typedef enum {
    AST_LITERAL,
    AST_ADD
} ASTType;

typedef struct AST {
    ASTType type;
    char *value;
    struct AST *left;
    struct AST *right;
} AST;

AST *match(Peg *peg, const char **input);
AST *make_ast(ASTType type, const char *val, AST *left, AST *right);
void print_ast(AST *node, int depth);

AST *make_ast(ASTType type, const char *val, AST *left, AST *right)
{
    AST *node = malloc(sizeof(AST));
    node->type = type;
    node->value = val ? strdup(val) : NULL; 
    node->left = left;
    node->right = right;
    return node;
}

AST *match(Peg *peg, const char **input)
{
    const char *start = *input;

    switch (peg->type) {
        case P_LITERAL:
            if (strncmp(*input, peg->literal, strlen(peg->literal)) == 0) {
                *input += strlen(peg->literal);
                return make_ast(AST_LITERAL, peg->literal, NULL, NULL);
            }
            return NULL;
        
        case P_SEQUENCE: {
            AST *left = match(peg->a, input);
            if (!left) { *input = start; return NULL; }
            AST *right = match(peg->b, input);
            if (!right) { *input = start; free(left); return NULL; }

            if (left->type == AST_LITERAL && strcmp(left->value, "+") == 0) {
                free(left->value);
                free(left);
                return right;
            }

            return make_ast(AST_ADD, "+", left, right);
        }

        case P_ONE_OR_MORE: {
            AST *head = match(peg->sub, input);
            if (!head) return NULL;
            AST *cur = head;
            while (1) {
                const char *save = *input;
                AST *next = match(peg->sub, input);
                if (!next) {*input = save; break;}
                cur = make_ast(AST_ADD, "+", cur, next);
            }
            return cur;
        }
    }
    return NULL;
}

void print_ast(AST *node, int depth) {
    if (!node) return;
    for (int i = 0; i < depth; i++) printf("  ");
    if (node->type == AST_LITERAL)
        printf("LITERAL(%s)\n", node->value);
    else if (node->type == AST_ADD)
        printf("ADD(%s)\n", node->value ? node->value : "+");
    print_ast(node->left, depth + 1);
    print_ast(node->right, depth + 1);
}

int main() {
    // Grammar: Expr <- "a" ("+" "a")*
    Peg lit_a   = { P_LITERAL, "a", NULL, NULL, NULL };
    Peg lit_plus= { P_LITERAL, "+", NULL, NULL, NULL };
    Peg plus_a_seq = { P_SEQUENCE, NULL, &lit_plus, &lit_a, NULL };
    Peg repeat_plus_a = { P_ONE_OR_MORE, NULL, NULL, NULL, &plus_a_seq };
    Peg expr_seq = { P_SEQUENCE, NULL, &lit_a, &repeat_plus_a, NULL };

    const char *input = "a+a+a+a+a";
    const char *p = input;

    AST *tree = match(&expr_seq, &p);

    if (tree && *p == '\0') {
        printf("OK\nAST:\n");
        print_ast(tree, 0);
    } else {
        printf("FAIL('%s')\n", p);
    }

    return 0;
}
