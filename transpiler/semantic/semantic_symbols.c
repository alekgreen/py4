#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic_internal.h"

int semantic_builtin_returns_owned_ref(const char *name)
{
    return strcmp(name, "list_int") == 0 ||
        strcmp(name, "list_float") == 0 ||
        strcmp(name, "list_bool") == 0 ||
        strcmp(name, "list_char") == 0 ||
        strcmp(name, "list_str") == 0 ||
        strcmp(name, "dict_str_str") == 0;
}

FunctionInfo *semantic_find_function(FunctionInfo *functions, const char *name)
{
    for (FunctionInfo *fn = functions; fn != NULL; fn = fn->next) {
        if (strcmp(fn->name, name) == 0) {
            return fn;
        }
    }
    return NULL;
}

FunctionInfo *semantic_find_function_by_node(FunctionInfo *functions, const ParseNode *node)
{
    for (FunctionInfo *fn = functions; fn != NULL; fn = fn->next) {
        if (fn->node == node) {
            return fn;
        }
    }
    return NULL;
}

void semantic_record_function_capture(FunctionInfo *fn, const char *name, ValueType type)
{
    if (fn == NULL || name == NULL) {
        semantic_error("cannot record function capture without a target function");
    }

    for (size_t i = 0; i < fn->capture_count; i++) {
        if (strcmp(fn->capture_names[i], name) == 0) {
            if (fn->capture_types[i] != type) {
                semantic_error("closure capture '%s' changed from %s to %s",
                    name,
                    semantic_type_name(fn->capture_types[i]),
                    semantic_type_name(type));
            }
            return;
        }
    }

    if (fn->capture_count == fn->capture_capacity) {
        size_t next_capacity = fn->capture_capacity == 0 ? 4 : fn->capture_capacity * 2;
        const char **next_names = realloc(fn->capture_names, sizeof(const char *) * next_capacity);
        ValueType *next_types = realloc(fn->capture_types, sizeof(ValueType) * next_capacity);

        if (next_names == NULL || next_types == NULL) {
            perror("realloc");
            exit(1);
        }

        fn->capture_names = next_names;
        fn->capture_types = next_types;
        fn->capture_capacity = next_capacity;
    }

    fn->capture_names[fn->capture_count] = name;
    fn->capture_types[fn->capture_count] = type;
    fn->capture_count++;
}

void semantic_record_call_target(SemanticInfo *info, const ParseNode *call, FunctionInfo *function)
{
    CallTargetInfo *target;

    for (target = info->call_targets; target != NULL; target = target->next) {
        if (target->call == call) {
            target->function = function;
            return;
        }
    }

    target = malloc(sizeof(CallTargetInfo));
    if (target == NULL) {
        perror("malloc");
        exit(1);
    }

    target->call = call;
    target->function = function;
    target->next = info->call_targets;
    info->call_targets = target;
}

FunctionInfo *semantic_resolved_call_target(const SemanticInfo *info, const ParseNode *call)
{
    for (CallTargetInfo *target = info->call_targets; target != NULL; target = target->next) {
        if (target->call == call) {
            return target->function;
        }
    }
    return NULL;
}

const char *semantic_function_c_name(const SemanticInfo *info, const ParseNode *function_def)
{
    FunctionInfo *fn = semantic_find_function_by_node(info->functions, function_def);

    if (fn == NULL) {
        semantic_error("unknown function definition during code generation");
    }
    return fn->c_name;
}

const char *semantic_call_c_name(const SemanticInfo *info, const ParseNode *call)
{
    FunctionInfo *fn = semantic_resolved_call_target(info, call);

    if (fn == NULL) {
        semantic_error("unresolved function call during code generation");
    }
    return fn->c_name;
}

int semantic_has_call_target(const SemanticInfo *info, const ParseNode *call)
{
    return semantic_resolved_call_target(info, call) != NULL;
}

void semantic_record_constructor_target(SemanticInfo *info, const ParseNode *call, ValueType class_type)
{
    ConstructorTargetInfo *target;

    for (target = info->constructor_targets; target != NULL; target = target->next) {
        if (target->call == call) {
            target->class_type = class_type;
            return;
        }
    }

    target = malloc(sizeof(ConstructorTargetInfo));
    if (target == NULL) {
        perror("malloc");
        exit(1);
    }

    target->call = call;
    target->class_type = class_type;
    target->next = info->constructor_targets;
    info->constructor_targets = target;
}

ValueType semantic_resolved_constructor_target(const SemanticInfo *info, const ParseNode *call)
{
    for (ConstructorTargetInfo *target = info->constructor_targets; target != NULL; target = target->next) {
        if (target->call == call) {
            return target->class_type;
        }
    }
    return 0;
}

void semantic_record_enum_variant_target(
    SemanticInfo *info,
    const ParseNode *node,
    ValueType enum_type,
    size_t variant_index)
{
    EnumVariantTargetInfo *target;

    for (target = info->enum_variant_targets; target != NULL; target = target->next) {
        if (target->node == node) {
            target->enum_type = enum_type;
            target->variant_index = variant_index;
            return;
        }
    }

    target = malloc(sizeof(EnumVariantTargetInfo));
    if (target == NULL) {
        perror("malloc");
        exit(1);
    }

    target->node = node;
    target->enum_type = enum_type;
    target->variant_index = variant_index;
    target->next = info->enum_variant_targets;
    info->enum_variant_targets = target;
}

EnumVariantTargetInfo *semantic_find_enum_variant_target(const SemanticInfo *info, const ParseNode *node)
{
    for (EnumVariantTargetInfo *target = info->enum_variant_targets; target != NULL; target = target->next) {
        if (target->node == node) {
            return target;
        }
    }
    return NULL;
}

int semantic_enum_variant_for_node(
    const SemanticInfo *info,
    const ParseNode *node,
    ValueType *enum_type_out,
    size_t *variant_index_out)
{
    EnumVariantTargetInfo *target = semantic_find_enum_variant_target(info, node);

    if (target == NULL) {
        return 0;
    }
    if (enum_type_out != NULL) {
        *enum_type_out = target->enum_type;
    }
    if (variant_index_out != NULL) {
        *variant_index_out = target->variant_index;
    }
    return 1;
}

int semantic_match_case_is_wildcard(const ParseNode *expr)
{
    const ParseNode *pattern;

    if (expr == NULL || expr->kind != NODE_EXPRESSION || expr->child_count != 1) {
        return 0;
    }

    pattern = expr->children[0];
    return pattern->kind == NODE_PRIMARY &&
        pattern->token_type == TOKEN_IDENTIFIER &&
        pattern->value != NULL &&
        strcmp(pattern->value, "_") == 0;
}

int semantic_match_type_supported(ValueType type)
{
    return semantic_type_is_enum(type) ||
        type == TYPE_INT ||
        type == TYPE_CHAR ||
        type == TYPE_BOOL;
}

int semantic_match_type_has_finite_coverage(ValueType type)
{
    return semantic_type_is_enum(type) || type == TYPE_BOOL;
}

static const char *scalar_match_pattern_description(ValueType type)
{
    if (type == TYPE_INT) {
        return "an int literal or '_'";
    }
    if (type == TYPE_CHAR) {
        return "a char literal or '_'";
    }
    if (type == TYPE_BOOL) {
        return "'True', 'False', or '_'";
    }
    return "a compatible literal or '_'";
}

static int scalar_pattern_matches_type(const ParseNode *pattern, ValueType type)
{
    if (pattern == NULL || pattern->kind != NODE_PRIMARY || pattern->value == NULL) {
        return 0;
    }
    if (type == TYPE_INT) {
        return pattern->token_type == TOKEN_NUMBER && strchr(pattern->value, '.') == NULL;
    }
    if (type == TYPE_CHAR) {
        return pattern->token_type == TOKEN_CHAR;
    }
    if (type == TYPE_BOOL) {
        return pattern->token_type == TOKEN_KEYWORD &&
            (strcmp(pattern->value, "True") == 0 || strcmp(pattern->value, "False") == 0);
    }
    return 0;
}

static int scalar_patterns_equal(const ParseNode *lhs, const ParseNode *rhs, ValueType type)
{
    if (!scalar_pattern_matches_type(lhs, type) || !scalar_pattern_matches_type(rhs, type)) {
        return 0;
    }
    return strcmp(lhs->value, rhs->value) == 0;
}

void semantic_validate_enum_match_pattern(
    SemanticInfo *info,
    const ParseNode *pattern_expr,
    Scope *scope,
    ValueType scrutinee_type,
    const ParseNode *match_node,
    size_t case_index,
    unsigned char *seen_variants,
    unsigned char *seen_bool_values)
{
    if (pattern_expr->child_count != 1) {
        if (semantic_type_is_enum(scrutinee_type)) {
            semantic_error_at_node(pattern_expr, "match case must be an enum member or '_'");
        }
        semantic_error_at_node(pattern_expr, "match case must be %s",
            scalar_match_pattern_description(scrutinee_type));
    }

    if (semantic_type_is_enum(scrutinee_type)) {
        ValueType case_enum_type = 0;
        size_t variant_index = 0;

        if (semantic_infer_expression_type(info, pattern_expr, scope) != scrutinee_type) {
            semantic_error_at_node(pattern_expr, "match case must use members of enum '%s'",
                semantic_enum_name(scrutinee_type));
        }
        if (!semantic_enum_variant_for_node(info, pattern_expr->children[0], &case_enum_type, &variant_index)) {
            semantic_error_at_node(pattern_expr, "match case must be an enum member or '_'");
        }
        if (case_enum_type != scrutinee_type) {
            semantic_error_at_node(pattern_expr, "match case must use members of enum '%s'",
                semantic_enum_name(scrutinee_type));
        }
        if (seen_variants[variant_index]) {
            semantic_error_at_node(pattern_expr, "duplicate match case '%s'",
                semantic_enum_variant_name(scrutinee_type, variant_index));
        }
        seen_variants[variant_index] = 1;
        return;
    }

    {
        const ParseNode *pattern = pattern_expr->children[0];

        if (!scalar_pattern_matches_type(pattern, scrutinee_type)) {
            semantic_error_at_node(pattern_expr, "match case must be %s",
                scalar_match_pattern_description(scrutinee_type));
        }

        for (size_t i = 2; i < case_index; i++) {
            const ParseNode *previous_case = semantic_expect_child(match_node, i, NODE_MATCH_CASE);
            const ParseNode *previous_pattern_expr = semantic_expect_child(previous_case, 0, NODE_EXPRESSION);

            if (semantic_match_case_is_wildcard(previous_pattern_expr)) {
                continue;
            }
            if (scalar_patterns_equal(previous_pattern_expr->children[0], pattern, scrutinee_type)) {
                semantic_error_at_node(pattern_expr, "duplicate match case literal %s", pattern->value);
            }
        }

        if (scrutinee_type == TYPE_BOOL) {
            seen_bool_values[strcmp(pattern->value, "True") == 0 ? 1 : 0] = 1;
        }
    }
}

int semantic_match_is_exhaustive(
    ValueType type,
    const unsigned char *seen_variants,
    const unsigned char *seen_bool_values)
{
    if (semantic_type_is_enum(type)) {
        for (size_t i = 0; i < semantic_enum_variant_count(type); i++) {
            if (!seen_variants[i]) {
                return 0;
            }
        }
        return 1;
    }

    if (type == TYPE_BOOL) {
        return seen_bool_values[0] && seen_bool_values[1];
    }

    return 0;
}

void semantic_error_missing_match_cases(
    const ParseNode *node,
    ValueType type,
    const unsigned char *seen_variants,
    const unsigned char *seen_bool_values)
{
    char buffer[512];
    size_t used;

    if (semantic_type_is_enum(type)) {
        size_t missing_count = 0;

        used = (size_t)snprintf(
            buffer,
            sizeof(buffer),
            "non-exhaustive match expression for enum '%s'; missing cases ",
            semantic_enum_name(type));
        if (used >= sizeof(buffer)) {
            semantic_error_at_node(node, "non-exhaustive match expression for enum '%s'",
                semantic_enum_name(type));
        }

        for (size_t i = 0; i < semantic_enum_variant_count(type); i++) {
            int written;

            if (seen_variants[i]) {
                continue;
            }
            written = snprintf(
                buffer + used,
                sizeof(buffer) - used,
                "%s'%s'",
                missing_count == 0 ? "" : ", ",
                semantic_enum_variant_name(type, i));
            if (written < 0 || (size_t)written >= sizeof(buffer) - used) {
                semantic_error_at_node(node, "non-exhaustive match expression for enum '%s'",
                    semantic_enum_name(type));
            }
            used += (size_t)written;
            missing_count++;
        }

        semantic_error_at_node(node, "%s", buffer);
    }

    if (type == TYPE_BOOL) {
        used = (size_t)snprintf(
            buffer,
            sizeof(buffer),
            "non-exhaustive match expression for bool; missing cases ");
        if (used >= sizeof(buffer)) {
            semantic_error_at_node(node, "non-exhaustive match expression for bool");
        }
        if (!seen_bool_values[1]) {
            used += (size_t)snprintf(buffer + used, sizeof(buffer) - used, "'True'");
        }
        if (!seen_bool_values[0]) {
            (void)snprintf(
                buffer + used,
                sizeof(buffer) - used,
                "%s'False'",
                seen_bool_values[1] ? "" : ", ");
        }

        semantic_error_at_node(node, "%s", buffer);
    }

    semantic_error_at_node(node, "match expression for %s requires a wildcard case '_'",
        semantic_type_name(type));
}

ValueType semantic_call_constructor_type(const SemanticInfo *info, const ParseNode *call)
{
    return semantic_resolved_constructor_target(info, call);
}

size_t semantic_call_arity(const SemanticInfo *info, const ParseNode *call)
{
    FunctionInfo *fn = semantic_resolved_call_target(info, call);

    if (fn == NULL) {
        semantic_error("unresolved function call during code generation");
    }
    return fn->param_count;
}

ValueType semantic_call_parameter_type(const SemanticInfo *info, const ParseNode *call, size_t index)
{
    FunctionInfo *fn = semantic_resolved_call_target(info, call);

    if (fn == NULL) {
        semantic_error("unresolved function call during code generation");
    }
    if (index >= fn->param_count) {
        semantic_error("call parameter index out of range");
    }
    return fn->param_types[index];
}

int semantic_function_is_nested(const SemanticInfo *info, const ParseNode *function_def)
{
    FunctionInfo *fn = semantic_find_function_by_node(info->functions, function_def);

    if (fn == NULL) {
        return 0;
    }
    return fn->enclosing != NULL;
}

size_t semantic_function_capture_count(const SemanticInfo *info, const ParseNode *function_def)
{
    FunctionInfo *fn = semantic_find_function_by_node(info->functions, function_def);

    if (fn == NULL) {
        return 0;
    }
    return fn->capture_count;
}

const char *semantic_function_capture_name(const SemanticInfo *info, const ParseNode *function_def, size_t index)
{
    FunctionInfo *fn = semantic_find_function_by_node(info->functions, function_def);

    if (fn == NULL) {
        semantic_error("unknown function definition during code generation");
    }
    if (index >= fn->capture_count) {
        semantic_error("function capture index out of range");
    }
    return fn->capture_names[index];
}

ValueType semantic_function_capture_type(const SemanticInfo *info, const ParseNode *function_def, size_t index)
{
    FunctionInfo *fn = semantic_find_function_by_node(info->functions, function_def);

    if (fn == NULL) {
        semantic_error("unknown function definition during code generation");
    }
    if (index >= fn->capture_count) {
        semantic_error("function capture index out of range");
    }
    return fn->capture_types[index];
}

size_t semantic_call_capture_count(const SemanticInfo *info, const ParseNode *call)
{
    FunctionInfo *fn = semantic_resolved_call_target(info, call);

    if (fn == NULL) {
        semantic_error("unresolved function call during code generation");
    }
    return fn->capture_count;
}

const char *semantic_call_capture_name(const SemanticInfo *info, const ParseNode *call, size_t index)
{
    FunctionInfo *fn = semantic_resolved_call_target(info, call);

    if (fn == NULL) {
        semantic_error("unresolved function call during code generation");
    }
    if (index >= fn->capture_count) {
        semantic_error("call capture index out of range");
    }
    return fn->capture_names[index];
}

ModuleInfo *semantic_find_module_info(ModuleInfo *modules, const char *name)
{
    for (ModuleInfo *module = modules; module != NULL; module = module->next) {
        if (strcmp(module->name, name) == 0) {
            return module;
        }
    }
    return NULL;
}

const char *semantic_module_name_for_path(const SemanticInfo *info, const char *path)
{
    for (ModuleInfo *module = info->modules; module != NULL; module = module->next) {
        if (strcmp(module->path, path) == 0) {
            return module->name;
        }
    }
    return NULL;
}

static char *semantic_module_ref_string(const ParseNode *node)
{
    if (node == NULL) {
        return NULL;
    }

    if (node->kind == NODE_PRIMARY && node->token_type == TOKEN_IDENTIFIER && node->value != NULL) {
        size_t len = strlen(node->value) + 1;
        char *copy = malloc(len);

        if (copy == NULL) {
            perror("malloc");
            exit(1);
        }

        memcpy(copy, node->value, len);
        return copy;
    }

    if (node->kind == NODE_FIELD_ACCESS) {
        char *base = semantic_module_ref_string(node->children[0]);
        const ParseNode *field = semantic_expect_child(node, 1, NODE_PRIMARY);
        char *joined;
        size_t len;

        if (base == NULL || field->value == NULL) {
            free(base);
            return NULL;
        }

        len = strlen(base) + strlen(field->value) + 2;
        joined = malloc(len);
        if (joined == NULL) {
            perror("malloc");
            exit(1);
        }
        snprintf(joined, len, "%s.%s", base, field->value);
        free(base);
        return joined;
    }

    return NULL;
}

const char *semantic_module_name_for_receiver(const SemanticInfo *info, const ParseNode *receiver)
{
    const char *current_module_name;
    ModuleInfo *module;
    char *name;

    if (receiver == NULL || receiver->source_path == NULL) {
        return NULL;
    }

    current_module_name = semantic_module_name_for_path(info, receiver->source_path);
    if (current_module_name == NULL) {
        return NULL;
    }

    module = semantic_find_module_info(info->modules, current_module_name);
    if (module == NULL) {
        return NULL;
    }

    name = semantic_module_ref_string(receiver);
    if (name == NULL) {
        return NULL;
    }

    for (ImportBinding *binding = module->imports; binding != NULL; binding = binding->next) {
        if (binding->is_module_import && strcmp(binding->local_name, name) == 0) {
            free(name);
            return binding->module_name;
        }
    }

    free(name);
    return NULL;
}

GlobalBinding *semantic_find_global(GlobalBinding *globals, const char *module_name, const char *name)
{
    for (GlobalBinding *global = globals; global != NULL; global = global->next) {
        if (strcmp(global->module_name, module_name) == 0 && strcmp(global->name, name) == 0) {
            return global;
        }
    }
    return NULL;
}

void semantic_record_global_target(
    SemanticInfo *info,
    const ParseNode *node,
    const char *module_name,
    const char *name)
{
    GlobalTargetInfo *target;

    for (target = info->global_targets; target != NULL; target = target->next) {
        if (target->node == node) {
            return;
        }
    }

    target = malloc(sizeof(GlobalTargetInfo));
    if (target == NULL) {
        perror("malloc");
        exit(1);
    }

    target->node = node;
    target->module_name = module_name;
    target->name = name;
    target->next = info->global_targets;
    info->global_targets = target;
}

GlobalTargetInfo *semantic_find_global_target(const SemanticInfo *info, const ParseNode *node)
{
    for (GlobalTargetInfo *target = info->global_targets; target != NULL; target = target->next) {
        if (target->node == node) {
            return target;
        }
    }
    return NULL;
}

void semantic_record_inferred_declaration_target(SemanticInfo *info, const ParseNode *node)
{
    InferredDeclTargetInfo *target;

    for (target = info->inferred_decl_targets; target != NULL; target = target->next) {
        if (target->node == node) {
            return;
        }
    }

    target = malloc(sizeof(InferredDeclTargetInfo));
    if (target == NULL) {
        perror("malloc");
        exit(1);
    }

    target->node = node;
    target->next = info->inferred_decl_targets;
    info->inferred_decl_targets = target;
}

int semantic_is_inferred_declaration_target(const SemanticInfo *info, const ParseNode *node)
{
    for (InferredDeclTargetInfo *target = info->inferred_decl_targets; target != NULL; target = target->next) {
        if (target->node == node) {
            return 1;
        }
    }
    return 0;
}

const char *semantic_global_c_name(const SemanticInfo *info, const char *module_name, const char *name)
{
    static char buffer[256];
    char module_buffer[128];
    GlobalBinding *global = semantic_find_global(info->globals, module_name, name);

    if (global == NULL) {
        semantic_error("unknown global '%s' in module '%s'", name, module_name);
    }

    snprintf(module_buffer, sizeof(module_buffer), "%s", module_name);
    for (size_t i = 0; module_buffer[i] != '\0'; i++) {
        if (module_buffer[i] == '.') {
            module_buffer[i] = '_';
        }
    }

    snprintf(buffer, sizeof(buffer), "py4_global_%s_%s", module_buffer, name);
    return buffer;
}

const char *semantic_global_target_c_name(const SemanticInfo *info, const ParseNode *node)
{
    GlobalTargetInfo *target = semantic_find_global_target(info, node);

    if (target == NULL) {
        return NULL;
    }
    return semantic_global_c_name(info, target->module_name, target->name);
}

MethodInfo *semantic_find_method(MethodInfo *methods, ValueType owner_type, const char *name)
{
    for (MethodInfo *method = methods; method != NULL; method = method->next) {
        if (method->owner_type == owner_type && strcmp(method->name, name) == 0) {
            return method;
        }
    }
    return NULL;
}

MethodInfo *semantic_methods(const SemanticInfo *info)
{
    return info == NULL ? NULL : info->methods;
}

const char *semantic_method_c_name(const SemanticInfo *info, ValueType owner_type, const char *method_name)
{
    MethodInfo *method = semantic_find_method(info->methods, owner_type, method_name);

    if (method == NULL) {
        semantic_error("unknown method '%s' on %s", method_name, semantic_type_name(owner_type));
    }
    return method->c_name;
}

size_t semantic_method_arity(const SemanticInfo *info, ValueType owner_type, const char *method_name)
{
    MethodInfo *method = semantic_find_method(info->methods, owner_type, method_name);

    if (method == NULL) {
        semantic_error("unknown method '%s' on %s", method_name, semantic_type_name(owner_type));
    }
    return method->param_count > 0 ? method->param_count - 1 : 0;
}

ValueType semantic_method_parameter_type(
    const SemanticInfo *info,
    ValueType owner_type,
    const char *method_name,
    size_t index)
{
    MethodInfo *method = semantic_find_method(info->methods, owner_type, method_name);

    if (method == NULL) {
        semantic_error("unknown method '%s' on %s", method_name, semantic_type_name(owner_type));
    }
    if (index + 1 >= method->param_count) {
        semantic_error("method parameter index out of bounds");
    }
    return method->param_types[index + 1];
}

VariableBinding *semantic_find_variable(Scope *scope, const char *name)
{
    for (Scope *curr = scope; curr != NULL; curr = curr->parent) {
        for (VariableBinding *var = curr->vars; var != NULL; var = var->next) {
            if (strcmp(var->name, name) == 0) {
                return var;
            }
        }
    }
    return NULL;
}

void semantic_bind_variable(Scope *scope, const char *name, ValueType type)
{
    for (VariableBinding *var = scope->vars; var != NULL; var = var->next) {
        if (strcmp(var->name, name) == 0) {
            if (var->type != type) {
                semantic_error("cannot redefine '%s' from %s to %s",
                    name, semantic_type_name(var->type), semantic_type_name(type));
            }
            return;
        }
    }

    VariableBinding *binding = malloc(sizeof(VariableBinding));
    if (binding == NULL) {
        perror("malloc");
        exit(1);
    }

    binding->name = name;
    binding->module_name = scope->parent == NULL && scope->module != NULL ? scope->module->name : NULL;
    binding->type = type;
    binding->function = scope->function;
    binding->next = scope->vars;
    scope->vars = binding;
}

void semantic_free_scope_bindings(VariableBinding *vars)
{
    while (vars != NULL) {
        VariableBinding *next = vars->next;
        free(vars);
        vars = next;
    }
}
