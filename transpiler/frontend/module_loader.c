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
    LoadedProgram *program;
} LoadContext;

static void import_error(const char *path, const char *message, ...);
static void import_error_at_node(const ParseNode *node, const char *message, ...);

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
        import_error(NULL, "failed to build module path");
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

static char *dots_to_slashes(const char *name)
{
    char *copy = dup_string(name);

    for (char *p = copy; *p != '\0'; p++) {
        if (*p == '.') {
            *p = '/';
        }
    }
    return copy;
}

static void import_error(const char *path, const char *message, ...)
{
    va_list args;
    va_list copy;
    int needed;
    char *buffer;

    va_start(args, message);
    va_copy(copy, args);
    needed = vsnprintf(NULL, 0, message, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(args);
        print_basic_diagnostic(stderr, path, "Import error", "failed to format import error");
        exit(1);
    }

    buffer = malloc((size_t)needed + 1);
    if (buffer == NULL) {
        va_end(args);
        perror("malloc");
        exit(1);
    }

    vsnprintf(buffer, (size_t)needed + 1, message, args);
    va_end(args);
    print_basic_diagnostic(stderr, path, "Import error", buffer);
    free(buffer);
    exit(1);
}

static void import_error_at_node(const ParseNode *node, const char *message, ...)
{
    va_list args;
    va_list copy;
    int needed;
    char *buffer;

    va_start(args, message);
    va_copy(copy, args);
    needed = vsnprintf(NULL, 0, message, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(args);
        print_basic_diagnostic(
            stderr,
            node != NULL ? node->source_path : NULL,
            "Import error",
            "failed to format import error");
        exit(1);
    }

    buffer = malloc((size_t)needed + 1);
    if (buffer == NULL) {
        va_end(args);
        perror("malloc");
        exit(1);
    }

    vsnprintf(buffer, (size_t)needed + 1, message, args);
    va_end(args);
    print_source_diagnostic(
        stderr,
        node != NULL ? node->source_path : NULL,
        node != NULL ? node->line : 0,
        node != NULL ? node->column : 0,
        "Import error",
        buffer,
        node != NULL ? node->source_line : NULL);
    free(buffer);
    exit(1);
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

static const char *top_level_symbol_name(const ParseNode *statement)
{
    const ParseNode *payload;
    const ParseNode *symbol;

    if (statement == NULL || statement->kind != NODE_STATEMENT || statement->child_count != 1) {
        return NULL;
    }

    payload = statement->children[0];
    if (payload->kind != NODE_FUNCTION_DEF &&
        payload->kind != NODE_NATIVE_FUNCTION_DEF &&
        payload->kind != NODE_NATIVE_TYPE_DEF &&
        payload->kind != NODE_CLASS_DEF) {
        if (payload->kind == NODE_SIMPLE_STATEMENT &&
            payload->child_count == 2 &&
            payload->children[0]->kind == NODE_PRIMARY) {
            return payload->children[0]->value;
        }
        return NULL;
    }

    symbol = payload->children[0];
    return symbol != NULL ? symbol->value : NULL;
}

static int is_exported_top_level(const ParseNode *statement, const char *name)
{
    const char *symbol = top_level_symbol_name(statement);

    return symbol != NULL && strcmp(symbol, name) == 0;
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

static char *module_name_from_path(const char *path)
{
    char *copy = dup_string(path);
    char *trimmed = copy;
    size_t len;

    if (strncmp(trimmed, "./", 2) == 0) {
        trimmed += 2;
    }
    if (strncmp(trimmed, "stdlib/", 7) == 0) {
        trimmed += 7;
    }

    len = strlen(trimmed);
    if (len >= 3 && strcmp(trimmed + len - 3, ".p4") == 0) {
        trimmed[len - 3] = '\0';
    }
    len = strlen(trimmed);
    if (len >= 9 && strcmp(trimmed + len - 9, "/__init__") == 0) {
        trimmed[len - 9] = '\0';
    }

    for (char *p = trimmed; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '.';
        }
    }

    {
        char *result = dup_string(trimmed);
        free(copy);
        return result;
    }
}

static char *resolve_import_path(const char *current_path, const char *module_name)
{
    char *rel = dots_to_slashes(module_name);
    char *dir = dirname_of(current_path);
    char *stdlib_path = dup_printf("stdlib/%s.p4", rel);
    char *stdlib_pkg = dup_printf("stdlib/%s/__init__.p4", rel);
    char *search_dir = dup_string(dir);

    free(dir);
    while (search_dir != NULL) {
        char *resolved = dup_printf("%s/%s.p4", search_dir, rel);
        char *resolved_pkg = dup_printf("%s/%s/__init__.p4", search_dir, rel);
        char *next = NULL;

        if (file_exists(resolved)) {
            free(search_dir);
            free(rel);
            free(resolved_pkg);
            free(stdlib_path);
            free(stdlib_pkg);
            return resolved;
        }
        if (file_exists(resolved_pkg)) {
            free(search_dir);
            free(rel);
            free(resolved);
            free(stdlib_path);
            free(stdlib_pkg);
            return resolved_pkg;
        }

        if (strcmp(search_dir, ".") == 0 || strcmp(search_dir, "/") == 0) {
            free(resolved);
            free(resolved_pkg);
            free(search_dir);
            break;
        }

        next = dirname_of(search_dir);
        free(resolved);
        free(resolved_pkg);
        free(search_dir);
        search_dir = next;
    }

    if (file_exists(stdlib_path)) {
        free(rel);
        free(stdlib_pkg);
        return stdlib_path;
    }
    if (file_exists(stdlib_pkg)) {
        free(rel);
        free(stdlib_path);
        return stdlib_pkg;
    }

    free(rel);
    free(stdlib_path);
    return stdlib_pkg;
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

static LoadedModule *find_loaded_module(LoadContext *ctx, const char *path)
{
    for (size_t i = 0; i < ctx->program->module_count; i++) {
        if (strcmp(ctx->program->modules[i].path, path) == 0) {
            return &ctx->program->modules[i];
        }
    }
    return NULL;
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

static void add_loaded_module(LoadContext *ctx, const char *path, const char *module_name, ParseNode *root)
{
    LoadedProgram *program = ctx->program;

    if (program->module_count == program->module_capacity) {
        size_t next_capacity = program->module_capacity == 0 ? 8 : program->module_capacity * 2;
        LoadedModule *modules = realloc(program->modules, sizeof(LoadedModule) * next_capacity);

        if (modules == NULL) {
            perror("realloc");
            exit(1);
        }
        program->modules = modules;
        program->module_capacity = next_capacity;
    }

    if (module_name != NULL) {
        program->modules[program->module_count].name = dup_string(module_name);
    } else {
        program->modules[program->module_count].name = module_name_from_path(path);
    }
    program->modules[program->module_count].path = dup_string(path);
    program->modules[program->module_count].root = root;
    program->module_count++;
}

static ParseNode *parse_file(const char *path, int show_tokens)
{
    FILE *fp = fopen(path, "r");
    TokenStream *ts;
    ParseNode *root;

    if (fp == NULL) {
        import_error(path, "could not open '%s'", path);
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

static void load_file(LoadContext *ctx, const char *path, const char *module_name, int show_tokens);

static ParseNode *clone_tree(const ParseNode *node)
{
    ParseNode *copy;

    if (node == NULL) {
        return NULL;
    }

    copy = create_node(node->kind, node->token_type, node->value);
    free(copy->source_path);
    free(copy->source_line);
    copy->source_path = node->source_path != NULL ? dup_string(node->source_path) : NULL;
    copy->source_line = node->source_line != NULL ? dup_string(node->source_line) : NULL;
    copy->line = node->line;
    copy->column = node->column;
    for (size_t i = 0; i < node->child_count; i++) {
        add_child(copy, clone_tree(node->children[i]));
    }
    return copy;
}

static void load_import_from_node(LoadContext *ctx, const char *current_path, const ParseNode *import_stmt);

static ParseNode *make_alias_wrapper(const ParseNode *statement, const char *original_name, const char *alias_name)
{
    const ParseNode *payload;
    const ParseNode *parameters;
    ParseNode *statement_node;
    ParseNode *function_def;
    ParseNode *arguments;
    ParseNode *call;
    ParseNode *expr_stmt;
    ParseNode *simple_stmt;
    ParseNode *suite;
    ParseNode *inner_stmt;
    size_t param_count;

    if (statement == NULL || statement->kind != NODE_STATEMENT || statement->child_count != 1) {
        return NULL;
    }

    payload = statement->children[0];
    if (payload->kind != NODE_NATIVE_FUNCTION_DEF && payload->kind != NODE_FUNCTION_DEF) {
        return NULL;
    }

    statement_node = create_node(NODE_STATEMENT, TOKEN_NULL, NULL);
    function_def = create_node(NODE_FUNCTION_DEF, payload->token_type, NULL);
    function_def->source_path = dup_string(payload->source_path);
    function_def->source_line = dup_string(payload->source_line);
    function_def->line = payload->line;
    function_def->column = payload->column;

    add_child(function_def, create_node(NODE_PRIMARY, TOKEN_IDENTIFIER, alias_name));
    function_def->children[0]->source_path = dup_string(payload->children[0]->source_path);
    function_def->children[0]->source_line = dup_string(payload->children[0]->source_line);
    function_def->children[0]->line = payload->children[0]->line;
    function_def->children[0]->column = payload->children[0]->column;

    parameters = payload->children[1];
    {
        ParseNode *params_copy = create_node(NODE_PARAMETERS, TOKEN_NULL, NULL);
        param_count = parameters->child_count;
        for (size_t i = 0; i < param_count; i++) {
            ParseNode *param = parameters->children[i];

            if (param->kind == NODE_EPSILON) {
                add_child(params_copy, create_node(NODE_EPSILON, TOKEN_NULL, "epsilon"));
                continue;
            }

            {
                ParseNode *param_copy = create_node(NODE_PARAMETER, TOKEN_NULL, param->value);
                add_child(param_copy, clone_tree(param->children[0]));
                add_child(params_copy, param_copy);
            }
        }
        add_child(function_def, params_copy);
    }

    if (payload->child_count >= 3 && payload->children[2]->kind == NODE_RETURN_TYPE) {
        add_child(function_def, clone_tree(payload->children[2]));
    }

    suite = create_node(NODE_SUITE, TOKEN_NULL, NULL);
    inner_stmt = create_node(NODE_STATEMENT, TOKEN_NULL, NULL);
    simple_stmt = create_node(NODE_SIMPLE_STATEMENT, TOKEN_NULL, NULL);
    if (payload->child_count >= 3 &&
        payload->children[2]->kind == NODE_RETURN_TYPE &&
        payload->children[2]->child_count == 1 &&
        payload->children[2]->children[0]->kind == NODE_TYPE &&
        payload->children[2]->children[0]->child_count == 1 &&
        payload->children[2]->children[0]->children[0]->value != NULL &&
        strcmp(payload->children[2]->children[0]->children[0]->value, "None") != 0) {
        expr_stmt = create_node(NODE_RETURN_STATEMENT, TOKEN_NULL, NULL);
    } else {
        expr_stmt = create_node(NODE_EXPRESSION_STATEMENT, TOKEN_NULL, NULL);
    }

    call = create_node(NODE_CALL, TOKEN_NULL, NULL);
    add_child(call, create_node(NODE_PRIMARY, TOKEN_IDENTIFIER, original_name));
    arguments = create_node(NODE_ARGUMENTS, TOKEN_NULL, NULL);
    for (size_t i = 0; i < param_count; i++) {
        if (parameters->children[i]->kind == NODE_EPSILON) {
            add_child(arguments, create_node(NODE_EPSILON, TOKEN_NULL, "epsilon"));
            break;
        }
        add_child(arguments, create_node(NODE_EXPRESSION, TOKEN_NULL, NULL));
        add_child(arguments->children[i], create_node(NODE_PRIMARY, TOKEN_IDENTIFIER, parameters->children[i]->value));
    }
    if (param_count == 0) {
        add_child(arguments, create_node(NODE_EPSILON, TOKEN_NULL, "epsilon"));
    }
    add_child(call, arguments);

    if (expr_stmt->kind == NODE_RETURN_STATEMENT) {
        add_child(expr_stmt, create_node(NODE_EXPRESSION, TOKEN_NULL, NULL));
        add_child(expr_stmt->children[0], call);
    } else {
        add_child(expr_stmt, create_node(NODE_EXPRESSION, TOKEN_NULL, NULL));
        add_child(expr_stmt->children[0], call);
    }
    add_child(simple_stmt, expr_stmt);
    add_child(inner_stmt, simple_stmt);
    add_child(suite, inner_stmt);
    add_child(function_def, suite);
    add_child(statement_node, function_def);
    return statement_node;
}

static void import_module_symbol(
    LoadContext *ctx,
    const char *path,
    const char *module_name,
    const char *symbol_name,
    const char *alias_name,
    const ParseNode *import_node)
{
    ParseNode *root;
    int found = 0;

    if (!has_visited(ctx, path)) {
        mark_visited(ctx, path);
        root = parse_file(path, 0);
        if (root->kind != NODE_S) {
            import_error(path, "malformed module root for '%s'", path);
        }
        add_loaded_module(ctx, path, module_name, root);

        for (size_t i = 0; i < root->child_count; i++) {
            ParseNode *child = root->children[i];

            if (child == NULL || child->kind == NODE_EPSILON) {
                continue;
            }
            if (is_import_statement(child)) {
                load_import_from_node(ctx, path, child->children[0]);
            }
        }
    } else {
        LoadedModule *loaded = find_loaded_module(ctx, path);

        if (loaded == NULL) {
            import_error(path, "module tracking failed for '%s'", path);
        }
        root = loaded->root;
    }

    if (root->kind != NODE_S) {
        import_error(path, "malformed module root for '%s'", path);
    }

    for (size_t i = 0; i < root->child_count; i++) {
        ParseNode *child = root->children[i];

        if (child == NULL) {
            continue;
        }

        if (is_import_statement(child)) {
            continue;
        }

        if (is_exported_top_level(child, symbol_name)) {
            ParseNode *selected = child;
            found = 1;
            add_child(ctx->program->emission_root, clone_tree(selected));
            if (alias_name != NULL && strcmp(alias_name, symbol_name) != 0) {
                ParseNode *wrapper = make_alias_wrapper(selected, symbol_name, alias_name);

                if (wrapper == NULL) {
                    import_error_at_node(import_node, "unsupported alias target '%s'", symbol_name);
                }
                add_child(ctx->program->emission_root, wrapper);
            }
        }
    }

    if (!found) {
        import_error_at_node(import_node, "module '%s' has no export '%s'", path, symbol_name);
    }
}

static void load_import_from_node(LoadContext *ctx, const char *current_path, const ParseNode *import_stmt)
{
    const ParseNode *module_name = import_stmt->children[0];
    const ParseNode *imported_name = NULL;
    const ParseNode *alias_name = NULL;
    char *import_path = resolve_import_path(current_path, module_name->value);

    if (strcmp(import_stmt->value, "from") == 0) {
        imported_name = import_stmt->child_count > 1 ? import_stmt->children[1] : NULL;
        alias_name = import_stmt->child_count > 2 ? import_stmt->children[2] : NULL;
        import_module_symbol(
            ctx,
            import_path,
            module_name->value,
            imported_name->value,
            alias_name != NULL ? alias_name->value : NULL,
            import_stmt);
        free(import_path);
        return;
    }

    load_file(ctx, import_path, module_name->value, 0);
    free(import_path);
}

static void load_file(LoadContext *ctx, const char *path, const char *module_name, int show_tokens)
{
    ParseNode *root;

    if (has_visited(ctx, path)) {
        return;
    }

    mark_visited(ctx, path);
    root = parse_file(path, show_tokens);
    if (root->kind != NODE_S) {
        import_error(path, "malformed module root for '%s'", path);
    }

    add_loaded_module(ctx, path, module_name, root);

    for (size_t i = 0; i < root->child_count; i++) {
        ParseNode *child = root->children[i];
        if (child == NULL) {
            continue;
        }

        if (child->kind == NODE_EPSILON) {
            continue;
        }

        if (is_import_statement(child)) {
            load_import_from_node(ctx, path, child->children[0]);
            continue;
        }

        add_child(ctx->program->emission_root, clone_tree(child));
    }
}

LoadedProgram *load_program_from_entry(const char *input_path, int show_tokens)
{
    LoadContext ctx = {0};
    LoadedProgram *program = calloc(1, sizeof(LoadedProgram));

    if (program == NULL) {
        perror("calloc");
        exit(1);
    }
    program->emission_root = create_node(NODE_S, TOKEN_NULL, NULL);
    ctx.program = program;
    load_file(&ctx, input_path, NULL, show_tokens);

    if (program->module_count == 0) {
        import_error(input_path, "no modules were loaded");
    }

    for (size_t i = 0; i < program->module_count; i++) {
        if (strcmp(program->modules[i].path, input_path) == 0) {
            program->entry_index = i;
            break;
        }
    }

    if (program->emission_root->child_count == 0) {
        add_child(program->emission_root, create_node(NODE_EPSILON, TOKEN_NULL, "epsilon"));
    }

    for (size_t i = 0; i < ctx.visited_count; i++) {
        free(ctx.visited_paths[i]);
    }
    free(ctx.visited_paths);

    return program;
}

void free_loaded_program(LoadedProgram *program)
{
    if (program == NULL) {
        return;
    }

    for (size_t i = 0; i < program->module_count; i++) {
        free(program->modules[i].name);
        free(program->modules[i].path);
        free_tree(program->modules[i].root);
    }
    free(program->modules);
    free_tree(program->emission_root);
    free(program);
}
