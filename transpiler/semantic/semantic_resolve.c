#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic_internal.h"

static char *semantic_dup_string(const char *value)
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

int is_module_private_name(const char *name)
{
    return name != NULL && name[0] == '_' && (name[1] == '\0' || name[1] != '_');
}

char *module_ref_string(const ParseNode *node)
{
    if (node == NULL) {
        return NULL;
    }

    if (node->kind == NODE_PRIMARY && node->token_type == TOKEN_IDENTIFIER && node->value != NULL) {
        return semantic_dup_string(node->value);
    }

    if (node->kind == NODE_FIELD_ACCESS) {
        char *base = module_ref_string(node->children[0]);
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

int json_from_string_type_supported(ValueType type)
{
    if (type == TYPE_INT || type == TYPE_FLOAT || type == TYPE_BOOL || type == TYPE_STR) {
        return 1;
    }
    if (type == TYPE_CHAR) {
        return 0;
    }
    if (semantic_type_is_tuple(type)) {
        for (size_t i = 0; i < semantic_tuple_element_count(type); i++) {
            if (!json_from_string_type_supported(semantic_tuple_element_type(type, i))) {
                return 0;
            }
        }
        return 1;
    }
    if (semantic_type_is_class(type)) {
        for (size_t i = 0; i < semantic_class_field_count(type); i++) {
            if (!json_from_string_type_supported(semantic_class_field_type(type, i))) {
                return 0;
            }
        }
        return 1;
    }
    if (semantic_type_is_optional(type)) {
        return json_from_string_type_supported(semantic_optional_base_type(type));
    }
    if (semantic_type_is_list(type)) {
        return json_from_string_type_supported(semantic_list_element_type(type));
    }
    if (semantic_type_is_dict(type)) {
        return semantic_dict_key_type(type) == TYPE_STR &&
            json_from_string_type_supported(semantic_dict_value_type(type));
    }
    return 0;
}

int json_to_string_type_supported(ValueType type)
{
    ValueType json_value_type = semantic_find_native_type("json", "Value");

    if (type == json_value_type) {
        return 1;
    }
    return json_from_string_type_supported(type);
}

const ModuleInfo *scope_module(Scope *scope)
{
    while (scope != NULL) {
        if (scope->module != NULL) {
            return scope->module;
        }
        scope = scope->parent;
    }
    return NULL;
}

const FunctionInfo *scope_function(Scope *scope)
{
    while (scope != NULL) {
        if (scope->function != NULL) {
            return scope->function;
        }
        scope = scope->parent;
    }
    return NULL;
}

ValueType scope_class_type(Scope *scope)
{
    while (scope != NULL) {
        if (scope->current_class_type != 0) {
            return scope->current_class_type;
        }
        scope = scope->parent;
    }
    return 0;
}

static ImportBinding *find_import_binding(const ModuleInfo *module, const char *local_name, int module_import_only)
{
    ImportBinding *binding;

    if (module == NULL) {
        return NULL;
    }

    for (binding = module->imports; binding != NULL; binding = binding->next) {
        if (strcmp(binding->local_name, local_name) != 0) {
            continue;
        }
        if (module_import_only && !binding->is_module_import) {
            continue;
        }
        if (!module_import_only && binding->is_module_import) {
            continue;
        }
        return binding;
    }
    return NULL;
}

ImportBinding *find_module_binding_for_receiver(const ModuleInfo *module, const ParseNode *receiver)
{
    char *name = module_ref_string(receiver);
    ImportBinding *binding;

    if (name == NULL) {
        return NULL;
    }

    binding = find_import_binding(module, name, 1);
    free(name);
    return binding;
}

GlobalBinding *resolve_visible_global(SemanticInfo *info, Scope *scope, const char *name)
{
    const ModuleInfo *current_module = scope_module(scope);
    ImportBinding *import_binding;
    GlobalBinding *binding;

    if (current_module == NULL) {
        return NULL;
    }

    binding = semantic_find_global(info->globals, current_module->name, name);
    if (binding != NULL) {
        return binding;
    }

    import_binding = find_import_binding(current_module, name, 0);
    if (import_binding == NULL) {
        return NULL;
    }
    if (is_module_private_name(import_binding->symbol_name)) {
        semantic_error_at_node(scope->module->root, "name '%s' is private to module '%s'",
            import_binding->symbol_name,
            import_binding->module_name);
    }

    return semantic_find_global(info->globals, import_binding->module_name, import_binding->symbol_name);
}

ValueType resolve_visible_class_type(SemanticInfo *info, Scope *scope, const char *name)
{
    const ModuleInfo *current_module = scope_module(scope);
    ImportBinding *import_binding;
    ValueType class_type;

    (void) info;

    class_type = semantic_find_class_type(name);
    if (class_type == 0 || current_module == NULL) {
        return class_type;
    }

    for (size_t i = 0; i < current_module->root->child_count; i++) {
        const ParseNode *payload = semantic_statement_payload(current_module->root->children[i]);

        if (payload->kind == NODE_CLASS_DEF &&
            strcmp(semantic_expect_child(payload, 0, NODE_PRIMARY)->value, name) == 0) {
            return class_type;
        }
    }

    import_binding = find_import_binding(current_module, name, 0);
    if (import_binding != NULL && import_binding->symbol_name != NULL &&
        strcmp(import_binding->symbol_name, name) == 0) {
        if (is_module_private_name(import_binding->symbol_name)) {
            semantic_error_at_node(scope->module->root, "name '%s' is private to module '%s'",
                import_binding->symbol_name,
                import_binding->module_name);
        }
        return class_type;
    }

    return 0;
}

ValueType resolve_visible_enum_type(SemanticInfo *info, Scope *scope, const char *name)
{
    const ModuleInfo *current_module = scope_module(scope);
    ImportBinding *import_binding;
    ValueType enum_type;

    (void) info;

    enum_type = semantic_find_enum_type(name);
    if (enum_type == 0 || current_module == NULL) {
        return enum_type;
    }

    for (size_t i = 0; i < current_module->root->child_count; i++) {
        const ParseNode *payload = semantic_statement_payload(current_module->root->children[i]);

        if (payload->kind == NODE_ENUM_DEF &&
            strcmp(semantic_expect_child(payload, 0, NODE_PRIMARY)->value, name) == 0) {
            return enum_type;
        }
    }

    import_binding = find_import_binding(current_module, name, 0);
    if (import_binding != NULL && import_binding->symbol_name != NULL &&
        strcmp(import_binding->symbol_name, name) == 0) {
        if (is_module_private_name(import_binding->symbol_name)) {
            semantic_error_at_node(scope->module->root, "name '%s' is private to module '%s'",
                import_binding->symbol_name,
                import_binding->module_name);
        }
        return enum_type;
    }

    return 0;
}

ValueType resolve_module_class_type(
    SemanticInfo *info,
    const ModuleInfo *current_module,
    const char *module_local_name,
    const char *class_name)
{
    ImportBinding *module_binding;
    ModuleInfo *target_module;
    ValueType class_type;

    if (current_module == NULL) {
        return 0;
    }

    module_binding = find_import_binding(current_module, module_local_name, 1);
    if (module_binding == NULL) {
        return 0;
    }

    target_module = semantic_find_module_info(info->modules, module_binding->module_name);
    if (target_module == NULL) {
        return 0;
    }
    if (is_module_private_name(class_name)) {
        semantic_error_at_node(current_module->root, "name '%s' is private to module '%s'",
            class_name,
            target_module->name);
    }

    for (size_t i = 0; i < target_module->root->child_count; i++) {
        const ParseNode *payload = semantic_statement_payload(target_module->root->children[i]);

        if (payload->kind == NODE_CLASS_DEF &&
            strcmp(semantic_expect_child(payload, 0, NODE_PRIMARY)->value, class_name) == 0) {
            class_type = semantic_find_class_type(class_name);
            return class_type;
        }
    }

    return 0;
}

static ValueType resolve_module_enum_type(
    SemanticInfo *info,
    const ModuleInfo *current_module,
    const char *module_local_name,
    const char *enum_name)
{
    ImportBinding *module_binding;
    ModuleInfo *target_module;
    ValueType enum_type;

    if (current_module == NULL) {
        return 0;
    }

    module_binding = find_import_binding(current_module, module_local_name, 1);
    if (module_binding == NULL) {
        return 0;
    }

    target_module = semantic_find_module_info(info->modules, module_binding->module_name);
    if (target_module == NULL) {
        return 0;
    }
    if (is_module_private_name(enum_name)) {
        semantic_error_at_node(current_module->root, "name '%s' is private to module '%s'",
            enum_name,
            target_module->name);
    }

    for (size_t i = 0; i < target_module->root->child_count; i++) {
        const ParseNode *payload = semantic_statement_payload(target_module->root->children[i]);

        if (payload->kind == NODE_ENUM_DEF &&
            strcmp(semantic_expect_child(payload, 0, NODE_PRIMARY)->value, enum_name) == 0) {
            enum_type = semantic_find_enum_type(enum_name);
            return enum_type;
        }
    }

    return 0;
}

ValueType resolve_field_access_enum_type(SemanticInfo *info, Scope *scope, const ParseNode *base)
{
    const ModuleInfo *current_module = scope_module(scope);

    if (base == NULL) {
        return 0;
    }
    if (base->kind == NODE_PRIMARY && base->value != NULL) {
        return resolve_visible_enum_type(info, scope, base->value);
    }
    if (base->kind == NODE_FIELD_ACCESS && base->child_count == 2 && current_module != NULL) {
        const ParseNode *module_receiver = base->children[0];
        const ParseNode *enum_name = semantic_expect_child(base, 1, NODE_PRIMARY);
        ImportBinding *module_binding = find_module_binding_for_receiver(current_module, module_receiver);

        if (module_binding == NULL) {
            return 0;
        }
        return resolve_module_enum_type(info, current_module, module_binding->local_name, enum_name->value);
    }
    return 0;
}

static int function_matches_module(FunctionInfo *fn, const char *module_name)
{
    return fn->module_name != NULL && strcmp(fn->module_name, module_name) == 0;
}

static int function_visible_lexically(FunctionInfo *fn, const FunctionInfo *current_function)
{
    if (fn->enclosing == NULL || current_function == NULL) {
        return 0;
    }

    return fn == current_function || current_function->node == fn->enclosing->node;
}

static int has_visible_lexical_function_named(
    SemanticInfo *info,
    const FunctionInfo *current_function,
    const char *name)
{
    if (current_function == NULL) {
        return 0;
    }

    for (FunctionInfo *fn = info->functions; fn != NULL; fn = fn->next) {
        if (strcmp(fn->name, name) != 0) {
            continue;
        }
        if (function_visible_lexically(fn, current_function)) {
            return 1;
        }
    }

    return 0;
}

static int function_visible_for_call(
    FunctionInfo *fn,
    const ModuleInfo *current_module,
    ImportBinding *import_binding,
    const char *module_name,
    const FunctionInfo *current_function,
    int lexical_only)
{
    if (fn->enclosing != NULL) {
        if (module_name != NULL) {
            return 0;
        }
        return function_visible_lexically(fn, current_function);
    }
    if (lexical_only) {
        return 0;
    }
    if (module_name != NULL) {
        return function_matches_module(fn, module_name);
    }
    if (current_module == NULL) {
        return 1;
    }
    if (function_matches_module(fn, current_module->name)) {
        return 1;
    }
    return import_binding != NULL &&
        function_matches_module(fn, import_binding->module_name) &&
        strcmp(fn->name, import_binding->symbol_name) == 0;
}

static void format_actual_argument_types(
    SemanticInfo *info,
    const ParseNode *arguments,
    Scope *scope,
    char *buffer,
    size_t buffer_size)
{
    size_t used = 0;

    buffer[0] = '\0';
    if (arguments->child_count == 1 && semantic_is_epsilon_node(arguments->children[0])) {
        return;
    }

    for (size_t i = 0; i < arguments->child_count; i++) {
        ValueType actual = semantic_infer_expression_type(info, arguments->children[i], scope);

        if (i != 0) {
            used += (size_t)snprintf(buffer + used, buffer_size - used, ", ");
        }
        used += (size_t)snprintf(buffer + used, buffer_size - used, "%s",
            semantic_type_name(actual));
    }
}

static void append_function_signature(
    FunctionInfo *fn,
    const char *display_name,
    char *buffer,
    size_t buffer_size,
    int *first)
{
    size_t used = strlen(buffer);

    if (!*first) {
        used += (size_t)snprintf(buffer + used, buffer_size - used, "; ");
    }

    used += (size_t)snprintf(buffer + used, buffer_size - used, "%s(", display_name);
    for (size_t i = 0; i < fn->param_count; i++) {
        if (i != 0) {
            used += (size_t)snprintf(buffer + used, buffer_size - used, ", ");
        }
        used += (size_t)snprintf(buffer + used, buffer_size - used, "%s",
            semantic_type_name(fn->param_types[i]));
    }
    snprintf(buffer + used, buffer_size - used, ")");
    *first = 0;
}

static void format_candidate_signatures(
    SemanticInfo *info,
    const char *display_name,
    const char *function_name,
    const ModuleInfo *current_module,
    const FunctionInfo *current_function,
    ImportBinding *import_binding,
    const char *module_name,
    size_t arity_filter,
    int require_arity,
    char *buffer,
    size_t buffer_size)
{
    int first = 1;
    int lexical_only = module_name == NULL &&
        has_visible_lexical_function_named(info, current_function, function_name);

    buffer[0] = '\0';
    for (FunctionInfo *fn = info->functions; fn != NULL; fn = fn->next) {
        if (strcmp(fn->name, function_name) != 0) {
            if (import_binding == NULL || strcmp(fn->name, import_binding->symbol_name) != 0) {
                continue;
            }
        }
        if (!function_visible_for_call(fn, current_module, import_binding, module_name, current_function, lexical_only)) {
            continue;
        }
        if (require_arity && fn->param_count != arity_filter) {
            continue;
        }
        append_function_signature(fn, display_name, buffer, buffer_size, &first);
    }
}

ValueType resolve_function_call_type(
    SemanticInfo *info,
    const ParseNode *call,
    const char *display_name,
    const char *function_name,
    const char *module_name,
    const ParseNode *arguments,
    Scope *scope)
{
    size_t actual_count = arguments->child_count;
    FunctionInfo *best = NULL;
    int best_score = 0;
    int ambiguous = 0;
    char actual_types[256];
    char candidates[512];
    int saw_name = 0;
    int saw_arity = 0;
    size_t arity_match_count = 0;
    FunctionInfo *sole_arity_match = NULL;
    const ModuleInfo *current_module = scope_module(scope);
    const FunctionInfo *current_function = scope_function(scope);
    ImportBinding *import_binding = module_name == NULL && current_module != NULL
        ? find_import_binding(current_module, function_name, 0)
        : NULL;
    int lexical_only = module_name == NULL &&
        has_visible_lexical_function_named(info, current_function, function_name);

    if (module_name != NULL && is_module_private_name(function_name)) {
        semantic_error_at_node(call, "name '%s' is private to module '%s'",
            function_name,
            module_name);
    }
    if (module_name == NULL && import_binding != NULL &&
        is_module_private_name(import_binding->symbol_name)) {
        semantic_error_at_node(call, "name '%s' is private to module '%s'",
            import_binding->symbol_name,
            import_binding->module_name);
    }

    if (arguments->child_count == 1 && semantic_is_epsilon_node(arguments->children[0])) {
        actual_count = 0;
    }

    if (module_name != NULL &&
        strcmp(module_name, "json") == 0 &&
        strcmp(function_name, "to_string") == 0) {
        ValueType arg_type;
        ValueType json_value_type = semantic_find_native_type("json", "Value");

        if (actual_count != 1) {
            semantic_error_at_node(arguments, "function '%s' expects %d arguments", "to_string", 1);
        }
        arg_type = semantic_infer_expression_type(info, arguments->children[0], scope);
        if (arg_type == json_value_type) {
            semantic_record_node_type(info, call, TYPE_STR);
            return TYPE_STR;
        }
        if (!json_to_string_type_supported(arg_type)) {
            semantic_error_at_node(arguments->children[0],
                "json.to_string(%s) currently supports int, float, bool, str, tuples, list[...], dict[str, ...], classes, and optionals",
                semantic_type_name(arg_type));
        }

        semantic_record_node_type(info, call, TYPE_STR);
        return TYPE_STR;
    }

    for (FunctionInfo *fn = info->functions; fn != NULL; fn = fn->next) {
        int score = 0;
        int compatible = 1;

        if (strcmp(fn->name, function_name) != 0) {
            if (import_binding == NULL || strcmp(fn->name, import_binding->symbol_name) != 0) {
                continue;
            }
        }
        if (!function_visible_for_call(fn, current_module, import_binding, module_name, current_function, lexical_only)) {
            continue;
        }

        saw_name = 1;
        if (fn->param_count != actual_count) {
            continue;
        }
        saw_arity = 1;
        arity_match_count++;
        sole_arity_match = fn;

        for (size_t i = 0; i < actual_count; i++) {
            ValueType actual = semantic_infer_expression_type_with_hint(
                info,
                arguments->children[i],
                scope,
                fn->param_types[i]);

            if (!semantic_is_assignable(fn->param_types[i], actual)) {
                compatible = 0;
                break;
            }
            if (actual == fn->param_types[i]) {
                continue;
            }
            if (actual == TYPE_INT && fn->param_types[i] == TYPE_FLOAT) {
                score += 1;
                continue;
            }
            score += 1;
        }

        if (!compatible) {
            continue;
        }

        if (best == NULL || score < best_score) {
            best = fn;
            best_score = score;
            ambiguous = 0;
        } else if (score == best_score) {
            ambiguous = 1;
        }
    }

    if (!saw_name) {
        if (module_name != NULL) {
            semantic_error_at_node(call, "unknown module function '%s'", display_name);
        }
        semantic_error_at_node(call, "unknown function '%s'", display_name);
    }
    if (best == NULL) {
        if (!saw_arity) {
            format_candidate_signatures(
                info,
                display_name,
                function_name,
                current_module,
                current_function,
                import_binding,
                module_name,
                0,
                0,
                candidates,
                sizeof(candidates));
            semantic_error_at_node(call, "no overload of function '%s' expects %zu arguments; candidates: %s",
                display_name, actual_count, candidates);
        }
        if (arity_match_count == 1 && sole_arity_match != NULL) {
            for (size_t i = 0; i < actual_count; i++) {
                ValueType actual = semantic_infer_expression_type_with_hint(
                    info,
                    arguments->children[i],
                    scope,
                    sole_arity_match->param_types[i]);
                if (!semantic_is_assignable(sole_arity_match->param_types[i], actual)) {
                    semantic_error_at_node(arguments->children[i],
                        "function '%s' argument %zu expects %s but got %s",
                        display_name,
                        i + 1,
                        semantic_type_name(sole_arity_match->param_types[i]),
                        semantic_type_name(actual));
                }
            }
        }
        format_actual_argument_types(info, arguments, scope, actual_types, sizeof(actual_types));
        format_candidate_signatures(
                info,
                display_name,
                function_name,
                current_module,
                current_function,
                import_binding,
                module_name,
                actual_count,
                1,
                candidates,
            sizeof(candidates));
        semantic_error_at_node(call, "no overload of function '%s' matches argument types (%s); candidates: %s",
            display_name, actual_count == 0 ? "" : actual_types, candidates);
    }
    if (ambiguous) {
        format_actual_argument_types(info, arguments, scope, actual_types, sizeof(actual_types));
        format_candidate_signatures(
            info,
            display_name,
            function_name,
            current_module,
            current_function,
            import_binding,
            module_name,
            actual_count,
            1,
            candidates,
            sizeof(candidates));
        semantic_error_at_node(call, "call to function '%s' is ambiguous for argument types (%s); candidates: %s",
            display_name, actual_count == 0 ? "" : actual_types, candidates);
    }

    for (size_t i = 0; i < actual_count; i++) {
        ValueType actual = semantic_infer_expression_type_with_hint(
            info,
            arguments->children[i],
            scope,
            best->param_types[i]);
        if (!semantic_is_assignable(best->param_types[i], actual)) {
            semantic_error_at_node(arguments->children[i], "function '%s' argument %zu expects %s but got %s",
                display_name, i + 1,
                semantic_type_name(best->param_types[i]),
                semantic_type_name(actual));
        }
    }

    semantic_record_call_target(info, call, best);
    semantic_record_node_type(info, call, best->return_type);
    return best->return_type;
}
