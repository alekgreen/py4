#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "lexer.h"
#include "module_loader.h"

typedef struct {
    char **visited_paths;
    size_t visited_count;
    size_t visited_capacity;
    ParseNode *merged_root;
} LoadContext;

static char *dup_string(const char *value)
{
    size_t len = strlen(value) + 1;
    char *copy = malloc(len);

    if (copy == NULL) {
        perror("malloc");
        exit(1);
    }

    memcpy(copy, value, len);
    return copy;
}

static char *dup_printf(const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int needed;
    char *buffer;

    va_start(args, fmt);
    va_copy(copy, args);
    needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(args);
        fprintf(stderr, "Import error: failed to build module path\n");
        exit(1);
    }

    buffer = malloc((size_t)needed + 1);
    if (buffer == NULL) {
        va_end(args);
        perror("malloc");
        exit(1);
    }

    vsnprintf(buffer, (size_t)needed + 1, fmt, args);
    va_end(args);
    return buffer;
}

static int file_exists(const char *path)
{
    FILE *fp = fopen(path, "r");

    if (fp == NULL) {
        return 0;
    }
    fclose(fp);
    return 1;
}

static int is_import_statement(const ParseNode *statement)
{
    return statement != NULL &&
        statement->kind == NODE_STATEMENT &&
        statement->child_count == 1 &&
        statement->children[0]->kind == NODE_IMPORT_STATEMENT;
}

static char *dirname_of(const char *path)
{
    const char *slash = strrchr(path, '/');

    if (slash == NULL) {
        return dup_string(".");
    }
    if (slash == path) {
        return dup_string("/");
    }

    return dup_printf("%.*s", (int)(slash - path), path);
}

static char *resolve_import_path(const char *current_path, const char *module_name)
{
    char *dir = dirname_of(current_path);
    char *resolved = dup_printf("%s/%s.p4", dir, module_name);
    char *stdlib_path = dup_printf("stdlib/%s.p4", module_name);

    free(dir);
    if (file_exists(resolved)) {
        free(stdlib_path);
        return resolved;
    }
    if (file_exists(stdlib_path)) {
        free(resolved);
        return stdlib_path;
    }

    free(stdlib_path);
    return resolved;
}

static int has_visited(const LoadContext *ctx, const char *path)
{
    for (size_t i = 0; i < ctx->visited_count; i++) {
        if (strcmp(ctx->visited_paths[i], path) == 0) {
            return 1;
        }
    }
    return 0;
}

static void mark_visited(LoadContext *ctx, const char *path)
{
    char **paths;

    if (has_visited(ctx, path)) {
        return;
    }

    if (ctx->visited_count == ctx->visited_capacity) {
        size_t next_capacity = ctx->visited_capacity == 0 ? 8 : ctx->visited_capacity * 2;
        paths = realloc(ctx->visited_paths, sizeof(char *) * next_capacity);
        if (paths == NULL) {
            perror("realloc");
            exit(1);
        }
        ctx->visited_paths = paths;
        ctx->visited_capacity = next_capacity;
    }

    ctx->visited_paths[ctx->visited_count++] = dup_string(path);
}

static ParseNode *parse_file(const char *path, int show_tokens)
{
    FILE *fp = fopen(path, "r");
    TokenStream *ts;
    ParseNode *root;

    if (fp == NULL) {
        fprintf(stderr, "Import error: could not open '%s'\n", path);
        exit(1);
    }

    ts = lexer(fp, path);
    if (show_tokens) {
        debug_print_ts(ts);
        reset_ts(ts);
    }

    root = parse(ts);

    free_token_stream(ts);
    fclose(fp);
    return root;
}

static void free_root_shell(ParseNode *root)
{
    if (root == NULL) {
        return;
    }

    free(root->children);
    free(root->value);
    free(root);
}

static void load_file(LoadContext *ctx, const char *path, int show_tokens)
{
    ParseNode *root;

    if (has_visited(ctx, path)) {
        return;
    }

    mark_visited(ctx, path);
    root = parse_file(path, show_tokens);
    if (root->kind != NODE_S) {
        fprintf(stderr, "Import error: malformed module root for '%s'\n", path);
        exit(1);
    }

    for (size_t i = 0; i < root->child_count; i++) {
        ParseNode *child = root->children[i];

        root->children[i] = NULL;
        if (child == NULL) {
            continue;
        }

        if (child->kind == NODE_EPSILON) {
            free_tree(child);
            continue;
        }

        if (is_import_statement(child)) {
            const ParseNode *import_stmt = child->children[0];
            const ParseNode *module_name = import_stmt->children[0];
            char *import_path = resolve_import_path(path, module_name->value);

            load_file(ctx, import_path, 0);
            free(import_path);
            free_tree(child);
            continue;
        }

        add_child(ctx->merged_root, child);
    }

    free_root_shell(root);
}

ParseNode *load_program_from_entry(const char *input_path, int show_tokens)
{
    LoadContext ctx = {0};

    ctx.merged_root = create_node(NODE_S, TOKEN_NULL, NULL);
    load_file(&ctx, input_path, show_tokens);

    if (ctx.merged_root->child_count == 0) {
        add_child(ctx.merged_root, create_node(NODE_EPSILON, TOKEN_NULL, "epsilon"));
    }

    for (size_t i = 0; i < ctx.visited_count; i++) {
        free(ctx.visited_paths[i]);
    }
    free(ctx.visited_paths);

    return ctx.merged_root;
}
