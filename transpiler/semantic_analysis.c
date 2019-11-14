#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic_internal.h"

const ParseNode *semantic_simple_statement_target(const ParseNode *simple_stmt)
{
    if (simple_stmt == NULL || simple_stmt->child_count == 0) {
        semantic_error("malformed simple statement");
    }
    return simple_stmt->children[0];
}

int semantic_is_type_assignment(const ParseNode *statement_tail)
{
    return statement_tail->child_count == 4 &&
        statement_tail->children[0]->kind == NODE_COLON;
}

const ParseNode *semantic_simple_statement_tail(const ParseNode *simple_stmt)
{
    return semantic_expect_child(simple_stmt, 1, NODE_STATEMENT_TAIL);
}

const ParseNode *semantic_statement_tail_expression(const ParseNode *statement_tail)
{
    if (semantic_is_type_assignment(statement_tail)) {
        return semantic_expect_child(statement_tail, 3, NODE_EXPRESSION);
    }
    return semantic_expect_child(statement_tail, 1, NODE_EXPRESSION);
}

const ParseNode *semantic_statement_tail_type_node(const ParseNode *statement_tail)
{
    return semantic_expect_child(statement_tail, 1, NODE_TYPE);
}

static int typecheck_statement(
    SemanticInfo *info,
    const ParseNode *statement,
    Scope *scope,
    FunctionContext *current_function,
    int allow_function_defs);
static void collect_classes(const ParseNode *root);
static void validate_class_definitions(SemanticInfo *info, const ParseNode *root);
static void collect_methods(SemanticInfo *info, const ParseNode *root);
static void collect_functions(SemanticInfo *info, const ParseNode *root);
static void collect_module_bindings(SemanticInfo *info, const LoadedProgram *program);
static void collect_global_bindings(SemanticInfo *info, const LoadedProgram *program);
static void typecheck_function(SemanticInfo *info, const ParseNode *function_def, Scope *global_scope);
static void typecheck_class_methods(SemanticInfo *info, const ParseNode *class_def, Scope *global_scope);

static char *dup_string(const char *value)
{
    size_t len;
    char *copy;

    len = strlen(value) + 1;
    copy = malloc(len);
    if (copy == NULL) {
        perror("malloc");
        exit(1);
    }
    memcpy(copy, value, len);
    return copy;
}

static char *module_name_from_path(const char *path)
{
    const char *basename;
    size_t module_len;
    char *name;

    if (path == NULL) {
        return dup_string("");
    }

    basename = strrchr(path, '/');
    basename = basename == NULL ? path : basename + 1;
    module_len = strlen(basename);
    if (module_len >= 3 && strcmp(basename + module_len - 3, ".p4") == 0) {
        module_len -= 3;
    }
    name = malloc(module_len + 1);
    if (name == NULL) {
        perror("malloc");
        exit(1);
    }
    memcpy(name, basename, module_len);
    name[module_len] = '\0';
    return name;
}

static ModuleInfo *module_info_for_path(SemanticInfo *info, const char *path)
{
    for (ModuleInfo *module = info->modules; module != NULL; module = module->next) {
        if (strcmp(module->path, path) == 0) {
            return module;
        }
    }
    return NULL;
}

static ImportBinding *create_import_binding(
    const char *local_name,
    const char *module_name,
    const char *symbol_name,
    int is_module_import)
{
    ImportBinding *binding = malloc(sizeof(ImportBinding));

    if (binding == NULL) {
        perror("malloc");
        exit(1);
    }

    binding->local_name = dup_string(local_name);
    binding->module_name = dup_string(module_name);
    binding->symbol_name = symbol_name != NULL ? dup_string(symbol_name) : NULL;
    binding->is_module_import = is_module_import;
    binding->next = NULL;
    return binding;
}

static ModuleInfo *create_module_info(const LoadedModule *module)
{
    ModuleInfo *info = malloc(sizeof(ModuleInfo));

    if (info == NULL) {
        perror("malloc");
        exit(1);
    }

    info->name = dup_string(module->name);
    info->path = dup_string(module->path);
    info->root = module->root;
    info->imports = NULL;
    info->next = NULL;
    return info;
}

static void add_import_binding(ModuleInfo *module, ImportBinding *binding)
{
    ImportBinding *existing;

    for (existing = module->imports; existing != NULL; existing = existing->next) {
        if (strcmp(existing->local_name, binding->local_name) == 0) {
            semantic_error_at_node(module->root, "duplicate import binding '%s' in module '%s'",
                binding->local_name, module->name);
        }
    }

    binding->next = module->imports;
    module->imports = binding;
}

static void add_global_binding(SemanticInfo *info, const char *module_name, const char *name, ValueType type)
{
    GlobalBinding *binding;

    if (semantic_find_global(info->globals, module_name, name) != NULL) {
        semantic_error("duplicate global binding '%s' in module '%s'", name, module_name);
    }

    binding = malloc(sizeof(GlobalBinding));
    if (binding == NULL) {
        perror("malloc");
        exit(1);
    }

    binding->module_name = dup_string(module_name);
    binding->name = dup_string(name);
    binding->type = type;
    binding->next = info->globals;
    info->globals = binding;
}

static void append_text(char *buffer, size_t size, size_t *length, const char *text)
{
    size_t remaining;
    int written;

    if (*length >= size) {
        semantic_error("generated function name is too long");
    }

    remaining = size - *length;
    written = snprintf(buffer + *length, remaining, "%s", text);
    if (written < 0 || (size_t) written >= remaining) {
        semantic_error("generated function name is too long");
    }
    *length += (size_t) written;
}

static void append_identifier_text(char *buffer, size_t size, size_t *length, const char *text)
{
    for (size_t i = 0; text[i] != '\0'; i++) {
        char piece[2];

        piece[0] = text[i] == '.' ? '_' : text[i];
        piece[1] = '\0';
        append_text(buffer, size, length, piece);
    }
}

static void append_type_mangle(char *buffer, size_t size, size_t *length, ValueType type)
{
    if (semantic_type_is_tuple(type)) {
        append_text(buffer, size, length, "tuple");
        for (size_t i = 0; i < semantic_tuple_element_count(type); i++) {
            append_text(buffer, size, length, "_");
            append_type_mangle(buffer, size, length, semantic_tuple_element_type(type, i));
        }
        return;
    }

    if (semantic_type_is_class(type)) {
        append_text(buffer, size, length, "class_");
        append_text(buffer, size, length, semantic_class_name(type));
        return;
    }

    if (semantic_type_is_native(type)) {
        append_text(buffer, size, length, "native_");
        append_identifier_text(buffer, size, length, semantic_native_type_module(type));
        append_text(buffer, size, length, "_");
        append_identifier_text(buffer, size, length, semantic_native_type_name(type));
        return;
    }

    if (semantic_type_is_union(type)) {
        static const ValueType ordered_members[] = {
            TYPE_INT, TYPE_FLOAT, TYPE_BOOL, TYPE_CHAR, TYPE_STR, TYPE_NONE
        };

        append_text(buffer, size, length, "union");
        for (size_t i = 0; i < sizeof(ordered_members) / sizeof(ordered_members[0]); i++) {
            if (!semantic_type_contains(type, ordered_members[i])) {
                continue;
            }
            append_text(buffer, size, length, "_");
            append_type_mangle(buffer, size, length, ordered_members[i]);
        }
        return;
    }

    switch (type) {
        case TYPE_INT: append_text(buffer, size, length, "int"); return;
        case TYPE_FLOAT: append_text(buffer, size, length, "float"); return;
        case TYPE_BOOL: append_text(buffer, size, length, "bool"); return;
        case TYPE_CHAR: append_text(buffer, size, length, "char"); return;
        case TYPE_STR: append_text(buffer, size, length, "str"); return;
        case TYPE_NONE: append_text(buffer, size, length, "none"); return;
        case TYPE_LIST_INT: append_text(buffer, size, length, "list_int"); return;
        case TYPE_LIST_FLOAT: append_text(buffer, size, length, "list_float"); return;
        case TYPE_LIST_BOOL: append_text(buffer, size, length, "list_bool"); return;
        case TYPE_LIST_CHAR: append_text(buffer, size, length, "list_char"); return;
        case TYPE_LIST_STR: append_text(buffer, size, length, "list_str"); return;
        default:
            if (semantic_type_is_dict(type)) {
                append_text(buffer, size, length, "dict_");
                append_type_mangle(buffer, size, length, semantic_dict_key_type(type));
                append_text(buffer, size, length, "_");
                append_type_mangle(buffer, size, length, semantic_dict_value_type(type));
                return;
            }
            if (semantic_type_is_class(type)) {
                append_text(buffer, size, length, "class_");
                append_text(buffer, size, length, semantic_class_name(type));
                return;
            }
            if (semantic_type_is_native(type)) {
                append_text(buffer, size, length, "native_");
                append_text(buffer, size, length, semantic_native_type_module(type));
                append_text(buffer, size, length, "_");
                append_text(buffer, size, length, semantic_native_type_name(type));
                return;
            }
            if (semantic_type_is_optional(type)) {
                append_text(buffer, size, length, "optional_");
                append_type_mangle(buffer, size, length, semantic_optional_base_type(type));
                return;
            }
            semantic_error("unsupported function overload type %s", semantic_type_name(type));
    }
}

static void collect_native_types(SemanticInfo *info, const ParseNode *root)
{
    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = semantic_statement_payload(root->children[i]);
        ModuleInfo *module;

        if (payload->kind != NODE_NATIVE_TYPE_DEF) {
            continue;
        }

        module = module_info_for_path(info, payload->source_path);
        if (module == NULL) {
            semantic_error_at_node(payload->children[0], "native type '%s' is missing module path information",
                semantic_expect_child(payload, 0, NODE_PRIMARY)->value);
        }
        semantic_register_native_type(module->name, payload);
    }
}

static int same_function_signature(
    FunctionInfo *fn,
    const char *module_name,
    size_t param_count,
    const ValueType *param_types)
{
    if (strcmp(fn->module_name, module_name) != 0) {
        return 0;
    }
    if (fn->param_count != param_count) {
        return 0;
    }
    for (size_t i = 0; i < param_count; i++) {
        if (fn->param_types[i] != param_types[i]) {
            return 0;
        }
    }
    return 1;
}

static char *build_function_c_name(
    const ParseNode *function_def,
    const char *module_name,
    const char *name,
    size_t param_count,
    const ValueType *param_types,
    int is_native)
{
    char buffer[512];
    size_t length = 0;

    if (!is_native && strcmp(name, "main") == 0) {
        return dup_string("main");
    }

    if (is_native) {
        const char *path = function_def->source_path != NULL ? function_def->source_path : "";
        const char *basename = strrchr(path, '/');
        size_t module_len;

        basename = basename == NULL ? path : basename + 1;
        module_len = strlen(basename);
        if (module_len >= 3 && strcmp(basename + module_len - 3, ".p4") == 0) {
            module_len -= 3;
        }
        if (module_len == 0) {
            semantic_error_at_node(function_def->children[0], "native function '%s' is missing module path information", name);
        }

        append_text(buffer, sizeof(buffer), &length, "py4_stdlib_");
        {
            char module_name[256];
            if (module_len >= sizeof(module_name)) {
                semantic_error("native module name is too long");
            }
            memcpy(module_name, basename, module_len);
            module_name[module_len] = '\0';
            append_identifier_text(buffer, sizeof(buffer), &length, module_name);
        }
        append_text(buffer, sizeof(buffer), &length, "_");
        append_identifier_text(buffer, sizeof(buffer), &length, name);
    } else {
        append_text(buffer, sizeof(buffer), &length, "py4_fn_");
        append_identifier_text(buffer, sizeof(buffer), &length, module_name);
        append_text(buffer, sizeof(buffer), &length, "_");
        append_identifier_text(buffer, sizeof(buffer), &length, name);
    }

    append_text(buffer, sizeof(buffer), &length, "__");
    if (param_count == 0) {
        append_text(buffer, sizeof(buffer), &length, "void");
    } else {
        for (size_t i = 0; i < param_count; i++) {
            if (i != 0) {
                append_text(buffer, sizeof(buffer), &length, "_");
            }
            append_type_mangle(buffer, sizeof(buffer), &length, param_types[i]);
        }
    }

    return dup_string(buffer);
}

static int tuple_target_matches_type(const ParseNode *target, ValueType type)
{
    if (target->kind == NODE_PRIMARY) {
        return 1;
    }

    if (target->kind != NODE_TUPLE_TARGET || !semantic_type_is_tuple(type)) {
        return 0;
    }
    if (target->child_count != semantic_tuple_element_count(type)) {
        return 0;
    }

    for (size_t i = 0; i < target->child_count; i++) {
        if (!tuple_target_matches_type(target->children[i], semantic_tuple_element_type(type, i))) {
            return 0;
        }
    }
    return 1;
}

static void bind_tuple_target_declaration(
    SemanticInfo *info,
    const ParseNode *target,
    Scope *scope,
    ValueType tuple_type)
{
    if (target->kind == NODE_PRIMARY) {
        semantic_bind_variable(scope, target->value, tuple_type);
        semantic_record_node_type(info, target, tuple_type);
        return;
    }

    semantic_record_node_type(info, target, tuple_type);
    for (size_t i = 0; i < target->child_count; i++) {
        bind_tuple_target_declaration(
            info,
            target->children[i],
            scope,
            semantic_tuple_element_type(tuple_type, i));
    }
}

static void check_tuple_target_assignment(
    SemanticInfo *info,
    const ParseNode *target,
    Scope *scope,
    ValueType tuple_type,
    const ParseNode *expr)
{
    if (target->kind == NODE_PRIMARY) {
        VariableBinding *var = semantic_find_variable(scope, target->value);

        if (var == NULL) {
            semantic_error_at_node(target, "assignment to undeclared variable '%s'", target->value);
        }
        if (!semantic_is_assignable(var->type, tuple_type)) {
            semantic_error_at_node(expr, "cannot assign %s to %s '%s'",
                semantic_type_name(tuple_type),
                semantic_type_name(var->type),
                target->value);
        }
        semantic_record_node_type(info, target, var->type);
        return;
    }

    semantic_record_node_type(info, target, tuple_type);
    for (size_t i = 0; i < target->child_count; i++) {
        check_tuple_target_assignment(
            info,
            target->children[i],
            scope,
            semantic_tuple_element_type(tuple_type, i),
            expr);
    }
}

static int typecheck_branch_suite(
    SemanticInfo *info,
    const ParseNode *suite,
    Scope *parent_scope,
    FunctionContext *current_function)
{
    Scope branch_scope = {0};
    int returns = 0;

    branch_scope.parent = parent_scope;
    branch_scope.module = parent_scope->module;
    branch_scope.current_class_type = parent_scope->current_class_type;
    if (!(suite->child_count == 1 && semantic_is_epsilon_node(suite->children[0]))) {
        for (size_t i = 0; i < suite->child_count; i++) {
            if (!returns) {
                returns = typecheck_statement(info, suite->children[i], &branch_scope, current_function, 0);
            } else {
                (void)typecheck_statement(info, suite->children[i], &branch_scope, current_function, 0);
            }
        }
    }

    semantic_free_scope_bindings(branch_scope.vars);
    return returns;
}

static void typecheck_return_statement(
    SemanticInfo *info,
    const ParseNode *return_stmt,
    Scope *scope,
    FunctionContext *current_function)
{
    if (current_function == NULL || current_function->name == NULL) {
        semantic_error_at_node(return_stmt, "return is only valid inside a function");
    }

    if (return_stmt->child_count != 1) {
        semantic_error("malformed return statement");
    }

    if (semantic_is_epsilon_node(return_stmt->children[0])) {
        if (!semantic_type_contains(current_function->return_type, TYPE_NONE)) {
            semantic_error_at_node(return_stmt, "function '%s' must return %s",
                current_function->name,
                semantic_type_name(current_function->return_type));
        }
        return;
    }

    ValueType expr_type = semantic_infer_expression_type_with_hint(
        info,
        return_stmt->children[0],
        scope,
        current_function->return_type);
    if (!semantic_is_assignable(current_function->return_type, expr_type)) {
        semantic_error_at_node(return_stmt->children[0], "function '%s' returns %s but got %s",
            current_function->name,
            semantic_type_name(current_function->return_type),
            semantic_type_name(expr_type));
    }
}

static int typecheck_simple_statement(
    SemanticInfo *info,
    const ParseNode *simple_stmt,
    Scope *scope,
    FunctionContext *current_function)
{
    const ParseNode *first_child = simple_stmt->children[0];

    if (first_child->kind == NODE_RETURN_STATEMENT) {
        typecheck_return_statement(info, first_child, scope, current_function);
        return 1;
    }

    if (first_child->kind == NODE_BREAK_STATEMENT) {
        if (current_function == NULL || current_function->loop_depth <= 0) {
            semantic_error_at_node(first_child, "break is only valid inside a loop");
        }
        return 0;
    }

    if (first_child->kind == NODE_CONTINUE_STATEMENT) {
        if (current_function == NULL || current_function->loop_depth <= 0) {
            semantic_error_at_node(first_child, "continue is only valid inside a loop");
        }
        return 0;
    }

    if (first_child->kind == NODE_ASSERT_STATEMENT) {
        ValueType condition_type;

        if (first_child->child_count != 2) {
            semantic_error("malformed assert statement");
        }

        condition_type = semantic_infer_expression_type(info, first_child->children[0], scope);
        if (condition_type != TYPE_BOOL) {
            semantic_error_at_node(first_child->children[0], "assert condition must be bool");
        }

        if (!semantic_is_epsilon_node(first_child->children[1])) {
            ValueType message_type = semantic_infer_expression_type(info, first_child->children[1], scope);

            if (!semantic_is_assignable(TYPE_STR, message_type)) {
                semantic_error_at_node(first_child->children[1], "assert message must be str");
            }
        }

        return 0;
    }

    if (first_child->kind == NODE_EXPRESSION_STATEMENT) {
        semantic_infer_expression_type(info, semantic_expect_child(first_child, 0, NODE_EXPRESSION), scope);
        return 0;
    }

    if (simple_stmt->child_count != 2) {
        semantic_error("malformed simple statement");
    }

    const ParseNode *target = semantic_simple_statement_target(simple_stmt);
    const ParseNode *statement_tail = semantic_simple_statement_tail(simple_stmt);
    const ParseNode *expr = semantic_statement_tail_expression(statement_tail);
    ValueType expr_type;

    if (target->kind == NODE_INDEX) {
        ValueType container_type = semantic_infer_primary_type(info, target->children[0], scope);
        ValueType index_type = semantic_infer_expression_type(info, target->children[1], scope);
        ValueType element_type;
        expr_type = semantic_infer_expression_type(info, expr, scope);

        if (semantic_is_type_assignment(statement_tail)) {
            semantic_error_at_node(target, "indexed assignment cannot use a type annotation");
        }
        if (semantic_type_is_tuple(container_type)) {
            semantic_error_at_node(target, "tuple elements are immutable");
        }
        if (semantic_type_is_dict(container_type)) {
            ValueType key_type = semantic_dict_key_type(container_type);
            ValueType value_type = semantic_dict_value_type(container_type);

            if (!semantic_is_assignable(key_type, index_type)) {
                semantic_error_at_node(target->children[1], "dict index must be %s",
                    semantic_type_name(key_type));
            }
            if (!semantic_is_assignable(value_type, expr_type)) {
                semantic_error_at_node(expr, "cannot assign %s to %s dict value",
                    semantic_type_name(expr_type),
                    semantic_type_name(value_type));
            }
            semantic_record_node_type(info, target, value_type);
            return 0;
        }
        if (!semantic_type_is_list(container_type)) {
            semantic_error_at_node(target->children[0], "indexed assignment requires list or dict but got %s",
                semantic_type_name(container_type));
        }
        if (index_type != TYPE_INT) {
            semantic_error_at_node(target->children[1], "list index must be int");
        }
        element_type = semantic_list_element_type(container_type);
        if (!semantic_is_assignable(element_type, expr_type)) {
            semantic_error_at_node(expr, "cannot assign %s to %s element",
                semantic_type_name(expr_type),
                semantic_type_name(element_type));
        }

        semantic_record_node_type(info, target, element_type);
        return 0;
    }

    if (target->kind == NODE_FIELD_ACCESS) {
        ValueType field_type;

        if (semantic_is_type_assignment(statement_tail)) {
            semantic_error_at_node(target, "field assignment cannot use a type annotation");
        }

        field_type = semantic_infer_primary_type(info, target, scope);
        expr_type = semantic_infer_expression_type_with_hint(info, expr, scope, field_type);
        if (!semantic_is_assignable(field_type, expr_type)) {
            semantic_error_at_node(expr, "cannot assign %s to %s field '%s'",
                semantic_type_name(expr_type),
                semantic_type_name(field_type),
                target->children[1]->value);
        }

        semantic_record_node_type(info, target, field_type);
        return 0;
    }

    if (target->kind == NODE_TUPLE_TARGET) {
        ValueType tuple_type;

        if (semantic_is_type_assignment(statement_tail)) {
            tuple_type = semantic_parse_type_node(info, semantic_statement_tail_type_node(statement_tail));
            if (!semantic_type_is_tuple(tuple_type)) {
                semantic_error_at_node(target, "tuple destructuring requires a tuple type annotation");
            }
            if (target->child_count != semantic_tuple_element_count(tuple_type)) {
                semantic_error_at_node(target,
                    "tuple target expects %zu elements but got %zu",
                    target->child_count,
                    semantic_tuple_element_count(tuple_type));
            }
            if (!tuple_target_matches_type(target, tuple_type)) {
                semantic_error_at_node(target,
                    "tuple target shape does not match %s",
                    semantic_type_name(tuple_type));
            }

            expr_type = semantic_infer_expression_type_with_hint(info, expr, scope, tuple_type);
            if (!semantic_is_assignable(tuple_type, expr_type)) {
                semantic_error_at_node(expr, "cannot assign %s to %s tuple target",
                    semantic_type_name(expr_type),
                    semantic_type_name(tuple_type));
            }

            bind_tuple_target_declaration(info, target, scope, tuple_type);
            return 0;
        }

        expr_type = semantic_infer_expression_type(info, expr, scope);
        if (!semantic_type_is_tuple(expr_type)) {
            semantic_error_at_node(expr, "tuple destructuring requires a tuple value");
        }
        if (target->child_count != semantic_tuple_element_count(expr_type)) {
            semantic_error_at_node(expr,
                "tuple target expects %zu elements but got %zu",
                target->child_count,
                semantic_tuple_element_count(expr_type));
        }
        if (!tuple_target_matches_type(target, expr_type)) {
            semantic_error_at_node(expr,
                "tuple target shape does not match %s",
                semantic_type_name(expr_type));
        }

        check_tuple_target_assignment(info, target, scope, expr_type, expr);
        return 0;
    }

    if (semantic_is_type_assignment(statement_tail)) {
        ValueType declared = semantic_parse_type_node(info, semantic_statement_tail_type_node(statement_tail));
        expr_type = semantic_infer_expression_type_with_hint(info, expr, scope, declared);
        if (!semantic_is_assignable(declared, expr_type)) {
            semantic_error_at_node(expr, "cannot assign %s to %s '%s'",
                semantic_type_name(expr_type),
                semantic_type_name(declared),
                target->value);
        }
        semantic_bind_variable(scope, target->value, declared);
        semantic_record_node_type(info, target, declared);
        return 0;
    }

    VariableBinding *var = semantic_find_variable(scope, target->value);
    if (var == NULL) {
        expr_type = semantic_infer_expression_type(info, expr, scope);
        if (expr_type == TYPE_NONE) {
            semantic_error_at_node(expr,
                "cannot infer type for '%s' from None; add a type annotation",
                target->value);
        }
        if (semantic_type_is_list(expr_type) &&
            expr->child_count == 1 &&
            expr->children[0]->kind == NODE_LIST_LITERAL &&
            expr->children[0]->child_count == 0) {
            semantic_error_at_node(expr,
                "cannot infer type for '%s' from empty list literal; add a type annotation",
                target->value);
        }
        semantic_bind_variable(scope, target->value, expr_type);
        semantic_record_inferred_declaration_target(info, target);
        semantic_record_node_type(info, target, expr_type);
        return 0;
    }

    expr_type = semantic_infer_expression_type_with_hint(info, expr, scope, var->type);
    if (!semantic_is_assignable(var->type, expr_type)) {
        semantic_error_at_node(expr, "cannot assign %s to %s '%s'",
            semantic_type_name(expr_type),
            semantic_type_name(var->type),
            target->value);
    }

    if (var->module_name != NULL) {
        semantic_record_global_target(info, target, var->module_name, var->name);
    }
    semantic_record_node_type(info, target, var->type);
    return 0;
}

static int typecheck_suite(
    SemanticInfo *info,
    const ParseNode *suite,
    Scope *scope,
    FunctionContext *current_function)
{
    int returns = 0;

    if (suite->child_count == 1 && semantic_is_epsilon_node(suite->children[0])) {
        return 0;
    }

    for (size_t i = 0; i < suite->child_count; i++) {
        if (!returns) {
            returns = typecheck_statement(info, suite->children[i], scope, current_function, 0);
        } else {
            (void)typecheck_statement(info, suite->children[i], scope, current_function, 0);
        }
    }

    return returns;
}

static int typecheck_if_statement(
    SemanticInfo *info,
    const ParseNode *if_stmt,
    Scope *scope,
    FunctionContext *current_function)
{
    ValueType condition_type = semantic_infer_expression_type(info, if_stmt->children[0], scope);
    int if_returns;
    int saw_else = 0;

    if (condition_type != TYPE_BOOL) {
        semantic_error_at_node(if_stmt->children[0], "if condition must be bool");
    }

    if_returns = typecheck_branch_suite(info, if_stmt->children[2], scope, current_function);

    for (size_t i = 3; i < if_stmt->child_count; i++) {
        const ParseNode *branch = if_stmt->children[i];

        if (branch->kind == NODE_ELIF_CLAUSE) {
            ValueType elif_type = semantic_infer_expression_type(info, branch->children[0], scope);
            int branch_returns;

            if (elif_type != TYPE_BOOL) {
                semantic_error_at_node(branch->children[0], "elif condition must be bool");
            }
            branch_returns = typecheck_branch_suite(info, branch->children[2], scope, current_function);
            if_returns = if_returns && branch_returns;
            continue;
        }

        if (branch->kind == NODE_ELSE_CLAUSE) {
            int else_returns;

            saw_else = 1;
            else_returns = typecheck_branch_suite(info, branch->children[1], scope, current_function);
            return if_returns && else_returns;
        }

        semantic_error("malformed if statement");
    }

    return saw_else ? if_returns : 0;
}

static int typecheck_while_statement(
    SemanticInfo *info,
    const ParseNode *while_stmt,
    Scope *scope,
    FunctionContext *current_function)
{
    ValueType condition_type = semantic_infer_expression_type(info, while_stmt->children[0], scope);

    if (condition_type != TYPE_BOOL) {
        semantic_error_at_node(while_stmt->children[0], "while condition must be bool");
    }

    if (current_function == NULL) {
        semantic_error("internal error: loop context missing for while");
    }

    current_function->loop_depth++;
    typecheck_branch_suite(info, while_stmt->children[2], scope, current_function);
    current_function->loop_depth--;
    return 0;
}

static int range_argument_count(const ParseNode *arguments)
{
    if (arguments->child_count == 1 && semantic_is_epsilon_node(arguments->children[0])) {
        return 0;
    }
    return (int)arguments->child_count;
}

static int is_range_expression(const ParseNode *expr)
{
    const ParseNode *call;
    const ParseNode *callee;

    if (expr == NULL || expr->kind != NODE_EXPRESSION || expr->child_count != 1) {
        return 0;
    }

    call = expr->children[0];
    if (call->kind != NODE_CALL || call->child_count != 2) {
        return 0;
    }

    callee = semantic_expect_child(call, 0, NODE_PRIMARY);
    return callee->token_type == TOKEN_IDENTIFIER &&
        callee->value != NULL &&
        strcmp(callee->value, "range") == 0;
}

static void typecheck_range_expression(
    SemanticInfo *info,
    const ParseNode *expr,
    Scope *scope)
{
    const ParseNode *call = expr->children[0];
    const ParseNode *arguments = semantic_expect_child(call, 1, NODE_ARGUMENTS);
    int arg_count = range_argument_count(arguments);

    if (arg_count < 1 || arg_count > 3) {
        semantic_error_at_node(expr, "function 'range' expects 1 to 3 arguments");
    }

    for (int i = 0; i < arg_count; i++) {
        ValueType arg_type = semantic_infer_expression_type(info, arguments->children[i], scope);

        if (arg_type != TYPE_INT) {
            semantic_error_at_node(arguments->children[i], "function 'range' argument %d expects int", i + 1);
        }
    }
}

static int typecheck_for_statement(
    SemanticInfo *info,
    const ParseNode *for_stmt,
    Scope *scope,
    FunctionContext *current_function)
{
    Scope loop_scope = {0};
    const ParseNode *target;
    const ParseNode *iterable;
    ValueType target_type;
    ValueType iterable_type;

    target = for_stmt->children[0];
    if (target == NULL || (target->kind != NODE_PRIMARY && target->kind != NODE_TUPLE_TARGET)) {
        semantic_error("malformed for loop target");
    }
    iterable = semantic_expect_child(for_stmt, 1, NODE_EXPRESSION);

    if (is_range_expression(iterable)) {
        typecheck_range_expression(info, iterable, scope);
        target_type = TYPE_INT;
    } else {
        iterable_type = semantic_infer_expression_type(info, iterable, scope);
        if (!semantic_type_is_list(iterable_type) && !semantic_type_is_dict(iterable_type)) {
            semantic_error_at_node(iterable, "for loop iterable must be list, dict, or range but got %s",
                semantic_type_name(iterable_type));
        }
        target_type = semantic_type_is_dict(iterable_type)
            ? semantic_dict_key_type(iterable_type)
            : semantic_list_element_type(iterable_type);
    }

    if (current_function == NULL) {
        semantic_error("internal error: loop context missing for for");
    }

    loop_scope.parent = scope;
    loop_scope.module = scope->module;
    loop_scope.current_class_type = scope->current_class_type;
    if (target->kind == NODE_TUPLE_TARGET) {
        if (!semantic_type_is_tuple(target_type)) {
            semantic_error_at_node(iterable, "tuple for target requires a tuple element type");
        }
        if (target->child_count != semantic_tuple_element_count(target_type)) {
            semantic_error_at_node(iterable,
                "tuple for target expects %zu elements but got %zu",
                target->child_count,
                semantic_tuple_element_count(target_type));
        }
        if (!tuple_target_matches_type(target, target_type)) {
            semantic_error_at_node(iterable,
                "tuple for target shape does not match %s",
                semantic_type_name(target_type));
        }
        bind_tuple_target_declaration(info, target, &loop_scope, target_type);
    } else {
        semantic_bind_variable(&loop_scope, target->value, target_type);
        semantic_record_node_type(info, target, target_type);
    }
    current_function->loop_depth++;
    typecheck_suite(info, for_stmt->children[3], &loop_scope, current_function);
    current_function->loop_depth--;
    semantic_free_scope_bindings(loop_scope.vars);
    return 0;
}

static int typecheck_with_statement(
    SemanticInfo *info,
    const ParseNode *with_stmt,
    Scope *scope,
    FunctionContext *current_function)
{
    Scope with_scope = {0};
    const ParseNode *resource_expr;
    const ParseNode *target;
    ValueType resource_type;
    ValueType file_type = semantic_find_native_type("io", "File");

    resource_expr = semantic_expect_child(with_stmt, 0, NODE_EXPRESSION);
    target = semantic_expect_child(with_stmt, 1, NODE_PRIMARY);
    resource_type = semantic_infer_expression_type(info, resource_expr, scope);

    if (file_type == 0 || resource_type != file_type) {
        semantic_error_at_node(resource_expr, "with expression must be io.File but got %s",
            semantic_type_name(resource_type));
    }

    with_scope.parent = scope;
    with_scope.module = scope->module;
    with_scope.current_class_type = scope->current_class_type;
    semantic_bind_variable(&with_scope, target->value, resource_type);
    semantic_record_node_type(info, target, resource_type);
    typecheck_suite(info, with_stmt->children[3], &with_scope, current_function);
    semantic_free_scope_bindings(with_scope.vars);
    return 0;
}

static void collect_classes(const ParseNode *root)
{
    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = semantic_statement_payload(root->children[i]);

        if (payload->kind == NODE_CLASS_DEF) {
            semantic_register_class(payload);
        }
    }
}

static void validate_class_definitions(SemanticInfo *info, const ParseNode *root)
{
    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = semantic_statement_payload(root->children[i]);

        if (payload->kind == NODE_CLASS_DEF) {
            semantic_define_class_fields(info, payload);
        }
    }
}

static char *build_method_c_name(ValueType owner_type, const char *method_name)
{
    const char *class_name = semantic_class_name(owner_type);
    size_t len = strlen(class_name) + strlen(method_name) + 2;
    char *name = malloc(len);

    if (name == NULL) {
        perror("malloc");
        exit(1);
    }

    snprintf(name, len, "%s_%s", class_name, method_name);
    return name;
}

static void collect_methods(SemanticInfo *info, const ParseNode *root)
{
    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = semantic_statement_payload(root->children[i]);
        ValueType owner_type;

        if (payload->kind != NODE_CLASS_DEF) {
            continue;
        }

        owner_type = semantic_find_class_type(semantic_expect_child(payload, 0, NODE_PRIMARY)->value);
        for (size_t j = 2; j < payload->child_count; j++) {
            const ParseNode *member = payload->children[j];
            const ParseNode *parameters;
            size_t count;
            MethodInfo *method;
            const char *name;

            if (member->kind != NODE_FUNCTION_DEF) {
                continue;
            }

            name = semantic_expect_child(member, 0, NODE_PRIMARY)->value;
            if (semantic_find_method(info->methods, owner_type, name) != NULL) {
                semantic_error_at_node(member->children[0], "duplicate method '%s' on class '%s'",
                    name,
                    semantic_class_name(owner_type));
            }
            for (size_t k = 0; k < semantic_class_field_count(owner_type); k++) {
                if (strcmp(semantic_class_field_name(owner_type, k), name) == 0) {
                    semantic_error_at_node(member->children[0], "method '%s' conflicts with field on class '%s'",
                        name,
                        semantic_class_name(owner_type));
                }
            }

            parameters = semantic_function_parameters(member);
            count = semantic_parameter_count(parameters);
            if (count == 0) {
                semantic_error_at_node(member->children[0], "method '%s' on class '%s' must declare self",
                    name,
                    semantic_class_name(owner_type));
            }
            if (semantic_parameter_type(info, semantic_expect_child(parameters, 0, NODE_PARAMETER)) != owner_type) {
                semantic_error_at_node(parameters->children[0], "method '%s' must use self: %s as first parameter",
                    name,
                    semantic_class_name(owner_type));
            }
            if (strcmp(name, "__init__") == 0 &&
                semantic_function_return_type(info, member) != TYPE_NONE) {
                semantic_error_at_node(member->children[0], "__init__ on class '%s' must return None",
                    semantic_class_name(owner_type));
            }

            method = malloc(sizeof(MethodInfo));
            if (method == NULL) {
                perror("malloc");
                exit(1);
            }
            method->owner_type = owner_type;
            method->name = name;
            method->c_name = build_method_c_name(owner_type, name);
            method->return_type = semantic_function_return_type(info, member);
            method->param_count = count;
            method->param_types = count > 0 ? malloc(sizeof(ValueType) * count) : NULL;
            method->node = member;
            method->next = info->methods;
            info->methods = method;

            if (count > 0 && method->param_types == NULL) {
                perror("malloc");
                exit(1);
            }
            for (size_t k = 0; k < count; k++) {
                method->param_types[k] = semantic_parameter_type(info, semantic_expect_child(parameters, k, NODE_PARAMETER));
            }
        }
    }
}

static void collect_functions(SemanticInfo *info, const ParseNode *root)
{
    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = semantic_statement_payload(root->children[i]);
        const ParseNode *parameters;
        size_t count;
        ValueType *param_types;
        const char *name;
        char *module_name;
        FunctionInfo *existing;
        FunctionInfo *fn;

        if (payload->kind != NODE_FUNCTION_DEF && payload->kind != NODE_NATIVE_FUNCTION_DEF) {
            continue;
        }

        name = semantic_expect_child(payload, 0, NODE_PRIMARY)->value;
        {
            ModuleInfo *module = module_info_for_path(info, payload->source_path);
            module_name = module != NULL ? dup_string(module->name) : module_name_from_path(payload->source_path);
        }
        if (semantic_find_class_type(name) != 0) {
            semantic_error_at_node(payload->children[0], "function name '%s' conflicts with class", name);
        }

        parameters = semantic_function_parameters(payload);
        count = semantic_parameter_count(parameters);
        if (strcmp(name, "main") == 0) {
            if (payload->kind == NODE_NATIVE_FUNCTION_DEF) {
                semantic_error_at_node(payload->children[0], "main cannot be declared as native");
            }
            if (count != 0 || semantic_function_return_type(info, payload) != TYPE_NONE) {
                semantic_error_at_node(payload->children[0], "main must have no parameters and return None");
            }
            if (semantic_find_function(info->functions, name) != NULL) {
                semantic_error_at_node(payload->children[0], "duplicate function 'main'");
            }
        }

        param_types = count > 0 ? malloc(sizeof(ValueType) * count) : NULL;
        if (count > 0 && param_types == NULL) {
            perror("malloc");
            exit(1);
        }
        for (size_t j = 0; j < count; j++) {
            param_types[j] = semantic_parameter_type(info, semantic_expect_child(parameters, j, NODE_PARAMETER));
        }
        for (existing = info->functions; existing != NULL; existing = existing->next) {
            if (strcmp(existing->name, name) != 0) {
                continue;
            }
            if (same_function_signature(existing, module_name, count, param_types)) {
                semantic_error_at_node(payload->children[0], "duplicate overload '%s' with the same parameter types", name);
            }
        }

        fn = malloc(sizeof(FunctionInfo));
        if (fn == NULL) {
            perror("malloc");
            exit(1);
        }

        fn->name = name;
        fn->module_name = module_name;
        fn->c_name = build_function_c_name(payload, module_name, name, count, param_types, payload->kind == NODE_NATIVE_FUNCTION_DEF);
        fn->return_type = semantic_function_return_type(info, payload);
        fn->param_count = count;
        fn->param_types = param_types;
        fn->is_native = payload->kind == NODE_NATIVE_FUNCTION_DEF;
        fn->node = payload;
        fn->next = info->functions;
        info->functions = fn;
    }
}

static void collect_module_bindings(SemanticInfo *info, const LoadedProgram *program)
{
    ModuleInfo *tail = NULL;

    for (size_t i = 0; i < program->module_count; i++) {
        const LoadedModule *loaded = &program->modules[i];
        ModuleInfo *module = create_module_info(loaded);

        if (info->modules == NULL) {
            info->modules = module;
        } else {
            tail->next = module;
        }
        tail = module;

        if (i == program->entry_index) {
            info->entry_module = module;
        }

        for (size_t j = 0; j < loaded->root->child_count; j++) {
            const ParseNode *statement = loaded->root->children[j];
            const ParseNode *payload;
            const ParseNode *module_name;
            const ParseNode *imported_name;
            const ParseNode *alias_name;

            if (statement == NULL || statement->kind != NODE_STATEMENT || statement->child_count != 1) {
                continue;
            }

            payload = statement->children[0];
            if (payload->kind != NODE_IMPORT_STATEMENT || payload->child_count == 0) {
                continue;
            }

            module_name = payload->children[0];
            if (strcmp(payload->value, "import") == 0) {
                alias_name = payload->child_count > 1 ? payload->children[1] : NULL;
                add_import_binding(module,
                    create_import_binding(
                        alias_name != NULL ? alias_name->value : module_name->value,
                        module_name->value,
                        NULL,
                        1));
            } else {
                imported_name = payload->child_count > 1 ? payload->children[1] : NULL;
                alias_name = payload->child_count > 2 ? payload->children[2] : NULL;
                add_import_binding(module,
                    create_import_binding(
                        alias_name != NULL ? alias_name->value : imported_name->value,
                        module_name->value,
                        imported_name->value,
                        0));
            }
        }
    }
}

static void collect_global_bindings(SemanticInfo *info, const LoadedProgram *program)
{
    for (size_t module_index = 0; module_index < program->module_count; module_index++) {
        const LoadedModule *module = &program->modules[module_index];
        Scope module_scope = {0};

        module_scope.module = semantic_find_module_info(info->modules, module->name);

        for (size_t i = 0; i < module->root->child_count; i++) {
            const ParseNode *statement = module->root->children[i];
            const ParseNode *payload = semantic_statement_payload(statement);
            const ParseNode *target;
            const ParseNode *statement_tail;
            ValueType target_type;

            if (payload->kind != NODE_SIMPLE_STATEMENT || payload->child_count != 2) {
                continue;
            }

            target = semantic_simple_statement_target(payload);
            if (target->kind != NODE_PRIMARY) {
                continue;
            }

            statement_tail = semantic_simple_statement_tail(payload);
            if (semantic_is_type_assignment(statement_tail)) {
                target_type = semantic_parse_type_node(info, semantic_statement_tail_type_node(statement_tail));
            } else {
                const ParseNode *expr = semantic_statement_tail_expression(statement_tail);

                target_type = semantic_infer_expression_type(info, expr, &module_scope);
                if (target_type == TYPE_NONE) {
                    semantic_error_at_node(expr,
                        "cannot infer type for '%s' from None; add a type annotation",
                        target->value);
                }
                if (semantic_type_is_list(target_type) &&
                    expr->child_count == 1 &&
                    expr->children[0]->kind == NODE_LIST_LITERAL &&
                    expr->children[0]->child_count == 0) {
                    semantic_error_at_node(expr,
                        "cannot infer type for '%s' from empty list literal; add a type annotation",
                        target->value);
                }
            }

            add_global_binding(info, module->name, target->value, target_type);
            semantic_bind_variable(&module_scope, target->value, target_type);
        }

        semantic_free_scope_bindings(module_scope.vars);
    }
}

static void typecheck_function(SemanticInfo *info, const ParseNode *function_def, Scope *global_scope)
{
    const ParseNode *parameters = semantic_function_parameters(function_def);
    const ParseNode *suite = function_def->children[function_def->child_count - 1];
    Scope local_scope = {0};
    FunctionContext current = {0};

    local_scope.parent = global_scope;
    local_scope.module = global_scope->module;
    local_scope.current_class_type = global_scope->current_class_type;
    current.name = semantic_expect_child(function_def, 0, NODE_PRIMARY)->value;
    current.return_type = semantic_function_return_type(info, function_def);
    current.loop_depth = 0;

    if (!(parameters->child_count == 1 && semantic_is_epsilon_node(parameters->children[0]))) {
        for (size_t i = 0; i < parameters->child_count; i++) {
            const ParseNode *param = semantic_expect_child(parameters, i, NODE_PARAMETER);
            semantic_bind_variable(&local_scope, param->value, semantic_parameter_type(info, param));
        }
    }

    {
        int returns = typecheck_suite(info, suite, &local_scope, &current);

        if (current.return_type != TYPE_NONE && !returns) {
            semantic_error_at_node(function_def->children[0], "function '%s' must return %s on all paths",
                current.name, semantic_type_name(current.return_type));
        }
    }

    semantic_free_scope_bindings(local_scope.vars);
}

static void typecheck_class_methods(SemanticInfo *info, const ParseNode *class_def, Scope *global_scope)
{
    Scope class_scope = *global_scope;
    class_scope.current_class_type = semantic_find_class_type(semantic_expect_child(class_def, 0, NODE_PRIMARY)->value);

    for (size_t i = 2; i < class_def->child_count; i++) {
        if (class_def->children[i]->kind == NODE_NATIVE_FUNCTION_DEF) {
            semantic_error_at_node(class_def->children[i]->children[0], "native methods are not supported");
        }
        if (class_def->children[i]->kind == NODE_FUNCTION_DEF) {
            typecheck_function(info, class_def->children[i], &class_scope);
        }
    }
}

static int typecheck_statement(
    SemanticInfo *info,
    const ParseNode *statement,
    Scope *scope,
    FunctionContext *current_function,
    int allow_function_defs)
{
    const ParseNode *payload = semantic_statement_payload(statement);

    if (payload->kind == NODE_FUNCTION_DEF) {
        if (!allow_function_defs) {
            semantic_error_at_node(payload->children[0], "nested function definitions are not supported");
        }
        typecheck_function(info, payload, scope);
        return 0;
    }

    if (payload->kind == NODE_NATIVE_FUNCTION_DEF) {
        if (!allow_function_defs) {
            semantic_error_at_node(payload->children[0], "native function declarations are only supported at module scope");
        }
        return 0;
    }

    if (payload->kind == NODE_NATIVE_TYPE_DEF) {
        if (!allow_function_defs) {
            semantic_error_at_node(payload->children[0], "native type declarations are only supported at module scope");
        }
        return 0;
    }

    if (payload->kind == NODE_CLASS_DEF) {
        semantic_error_at_node(payload->children[0], "class definitions are only supported at module scope");
    }

    if (payload->kind == NODE_IMPORT_STATEMENT) {
        semantic_error_at_node(payload, "imports are only supported at module scope");
    }

    if (payload->kind == NODE_WHILE_STATEMENT) {
        return typecheck_while_statement(info, payload, scope, current_function);
    }

    if (payload->kind == NODE_FOR_STATEMENT) {
        return typecheck_for_statement(info, payload, scope, current_function);
    }

    if (payload->kind == NODE_WITH_STATEMENT) {
        return typecheck_with_statement(info, payload, scope, current_function);
    }

    if (semantic_is_if_statement(payload)) {
        return typecheck_if_statement(info, payload, scope, current_function);
    }

    if (payload->kind != NODE_SIMPLE_STATEMENT) {
        semantic_error("unsupported statement node");
    }

    return typecheck_simple_statement(info, payload, scope, current_function);
}

SemanticInfo *analyze_program(const LoadedProgram *program)
{
    const ParseNode *root;
    Scope global_scope = {0};
    SemanticInfo *info;

    if (program == NULL) {
        semantic_error("expected loaded program");
    }
    root = program->emission_root;
    if (root == NULL || root->kind != NODE_S) {
        semantic_error("expected program root");
    }

    info = calloc(1, sizeof(SemanticInfo));
    if (info == NULL) {
        perror("calloc");
        exit(1);
    }

    collect_module_bindings(info, program);
    collect_native_types(info, root);
    collect_classes(root);
    validate_class_definitions(info, root);
    collect_global_bindings(info, program);
    collect_methods(info, root);
    collect_functions(info, root);

    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = semantic_statement_payload(root->children[i]);
        FunctionContext module_context = {0};
        ModuleInfo *module = module_info_for_path(info, payload->source_path);

        global_scope.module = module;

        if (payload->kind == NODE_IMPORT_STATEMENT) {
            semantic_error_at_node(payload, "imports should be resolved before semantic analysis");
        } else if (payload->kind == NODE_CLASS_DEF) {
            typecheck_class_methods(info, payload, &global_scope);
            continue;
        } else if (payload->kind == NODE_NATIVE_TYPE_DEF) {
            continue;
        } else if (payload->kind == NODE_FUNCTION_DEF) {
            typecheck_function(info, payload, &global_scope);
        } else if (payload->kind == NODE_NATIVE_FUNCTION_DEF) {
            continue;
        } else {
            module_context.loop_depth = 0;
            typecheck_statement(info, root->children[i], &global_scope, &module_context, 0);
        }
    }

    semantic_free_scope_bindings(global_scope.vars);
    return info;
}

void free_semantic_info(SemanticInfo *info)
{
    NodeTypeInfo *node_types;
    FunctionInfo *functions;

    if (info == NULL) {
        return;
    }

    node_types = info->node_types;
    while (node_types != NULL) {
        NodeTypeInfo *next = node_types->next;
        free(node_types);
        node_types = next;
    }

    functions = info->functions;
    while (functions != NULL) {
        FunctionInfo *next = functions->next;
        free((char *) functions->module_name);
        free(functions->c_name);
        free(functions->param_types);
        free(functions);
        functions = next;
    }

    {
        CallTargetInfo *call_targets = info->call_targets;
        while (call_targets != NULL) {
            CallTargetInfo *next = call_targets->next;
            free(call_targets);
            call_targets = next;
        }
    }

    {
        ConstructorTargetInfo *constructor_targets = info->constructor_targets;
        while (constructor_targets != NULL) {
            ConstructorTargetInfo *next = constructor_targets->next;
            free(constructor_targets);
            constructor_targets = next;
        }
    }

    {
        ModuleInfo *modules = info->modules;
        while (modules != NULL) {
            ModuleInfo *next_module = modules->next;
            ImportBinding *bindings = modules->imports;

            while (bindings != NULL) {
                ImportBinding *next_binding = bindings->next;
                free((char *) bindings->local_name);
                free((char *) bindings->module_name);
                free((char *) bindings->symbol_name);
                free(bindings);
                bindings = next_binding;
            }

            free((char *) modules->name);
            free((char *) modules->path);
            free(modules);
            modules = next_module;
        }
    }

    {
        InferredDeclTargetInfo *targets = info->inferred_decl_targets;
        while (targets != NULL) {
            InferredDeclTargetInfo *next = targets->next;
            free(targets);
            targets = next;
        }
    }

    {
        GlobalBinding *globals = info->globals;
        while (globals != NULL) {
            GlobalBinding *next = globals->next;
            free((char *) globals->module_name);
            free((char *) globals->name);
            free(globals);
            globals = next;
        }
    }

    {
        GlobalTargetInfo *targets = info->global_targets;
        while (targets != NULL) {
            GlobalTargetInfo *next = targets->next;
            free(targets);
            targets = next;
        }
    }

    {
        MethodInfo *methods = info->methods;
        while (methods != NULL) {
            MethodInfo *next = methods->next;
            free(methods->c_name);
            free(methods->param_types);
            free(methods);
            methods = next;
        }
    }

    free(info);
}
