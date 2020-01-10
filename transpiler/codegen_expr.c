#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen_internal.h"

static const char *map_operator(const char *op)
{
    if (strcmp(op, "and") == 0) {
        return "&&";
    }
    if (strcmp(op, "or") == 0) {
        return "||";
    }
    if (strcmp(op, "not") == 0) {
        return "!";
    }
    return op;
}

static int is_comparison_operator(const char *op)
{
    return strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 ||
        strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
        strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0;
}

static int is_chained_comparison_expr(const ParseNode *expr)
{
    if (expr->kind != NODE_EXPRESSION || expr->child_count <= 3 || (expr->child_count % 2) == 0) {
        return 0;
    }

    for (size_t i = 1; i < expr->child_count; i += 2) {
        const ParseNode *op = codegen_expect_child(expr, i, NODE_OPERATOR);
        if (!is_comparison_operator(op->value)) {
            return 0;
        }
    }

    return 1;
}

static int is_list_builtin_name(const char *name)
{
    return strcmp(name, "len") == 0 ||
        strcmp(name, "ord") == 0 ||
        strcmp(name, "chr") == 0 ||
        strcmp(name, "list_int") == 0 ||
        strcmp(name, "list_float") == 0 ||
        strcmp(name, "list_bool") == 0 ||
        strcmp(name, "list_char") == 0 ||
        strcmp(name, "list_str") == 0 ||
        strcmp(name, "dict_str_str") == 0 ||
        strcmp(name, "list_append") == 0 ||
        strcmp(name, "list_get") == 0 ||
        strcmp(name, "list_len") == 0 ||
        strcmp(name, "list_set") == 0;
}

static int is_list_method_name(const char *name)
{
    return strcmp(name, "append") == 0 ||
        strcmp(name, "pop") == 0 ||
        strcmp(name, "clear") == 0 ||
        strcmp(name, "copy") == 0;
}

static int is_dict_method_name(const char *name)
{
    return strcmp(name, "set") == 0 ||
        strcmp(name, "get") == 0 ||
        strcmp(name, "get_or") == 0 ||
        strcmp(name, "contains") == 0 ||
        strcmp(name, "keys") == 0 ||
        strcmp(name, "items") == 0 ||
        strcmp(name, "values") == 0 ||
        strcmp(name, "pop") == 0 ||
        strcmp(name, "clear") == 0 ||
        strcmp(name, "copy") == 0;
}

static int is_init_self_identifier(CodegenContext *ctx, const ParseNode *primary)
{
    return ctx->current_function_is_init &&
        primary != NULL &&
        primary->kind == NODE_PRIMARY &&
        primary->token_type == TOKEN_IDENTIFIER &&
        primary->value != NULL &&
        strcmp(primary->value, "self") == 0;
}

static int is_super_call_node(const ParseNode *node)
{
    const ParseNode *callee;
    const ParseNode *arguments;

    if (node == NULL || node->kind != NODE_CALL || node->child_count != 2) {
        return 0;
    }

    callee = codegen_expect_child(node, 0, NODE_PRIMARY);
    arguments = codegen_expect_child(node, 1, NODE_ARGUMENTS);
    return callee->token_type == TOKEN_IDENTIFIER &&
        callee->value != NULL &&
        strcmp(callee->value, "super") == 0 &&
        arguments->child_count == 1 &&
        codegen_is_epsilon_node(arguments->children[0]);
}

static char *codegen_build_base_self_value(CodegenContext *ctx, ValueType owner_type, ValueType base_type)
{
    size_t base_field_count = semantic_class_field_count(base_type);
    char *parts = codegen_dup_printf("");

    if (base_field_count == 0) {
        free(parts);
        return codegen_dup_printf("((%s){0})", semantic_class_name(base_type));
    }

    for (size_t i = 0; i < base_field_count; i++) {
        char *field_expr = ctx->current_function_is_init
            ? codegen_dup_printf("self->%s", semantic_class_field_name(owner_type, i))
            : codegen_dup_printf("self.%s", semantic_class_field_name(owner_type, i));
        char *joined = i == 0
            ? codegen_dup_printf("%s", field_expr)
            : codegen_dup_printf("%s, %s", parts, field_expr);

        free(parts);
        free(field_expr);
        parts = joined;
    }

    {
        char *result = codegen_dup_printf("((%s){%s})", semantic_class_name(base_type), parts);
        free(parts);
        return result;
    }
}

static void codegen_emit_base_self_writeback(
    CodegenContext *ctx,
    ValueType owner_type,
    ValueType base_type,
    const char *base_name)
{
    size_t base_field_count = semantic_class_field_count(base_type);

    for (size_t i = 0; i < base_field_count; i++) {
        codegen_emit_indent(ctx);
        if (ctx->current_function_is_init) {
            fprintf(ctx->out, "self->%s = %s.%s;\n",
                semantic_class_field_name(owner_type, i),
                base_name,
                semantic_class_field_name(base_type, i));
        } else {
            fprintf(ctx->out, "self.%s = %s.%s;\n",
                semantic_class_field_name(owner_type, i),
                base_name,
                semantic_class_field_name(base_type, i));
        }
    }
}

static int node_is_owned_ref_value(CodegenContext *ctx, const ParseNode *node);

char *codegen_next_temp_name(CodegenContext *ctx)
{
    return codegen_dup_printf("py4_tmp_%d", ctx->temp_counter++);
}

void codegen_push_cleanup_scope(CodegenContext *ctx)
{
    if (ctx->cleanup_scope_count >= MAX_SCOPE_DEPTH) {
        codegen_error("cleanup scope nesting too deep");
    }
    ctx->cleanup_scope_starts[ctx->cleanup_scope_count++] = ctx->managed_local_count;
}

void codegen_emit_value_retain(CodegenContext *ctx, ValueType type, const char *name)
{
    char helper_name[MAX_NAME_LEN];

    if (!semantic_type_needs_management(type)) {
        codegen_error("unsupported managed type %s", semantic_type_name(type));
    }

    if (semantic_type_is_ref(type)) {
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "%s_incref(%s);\n", codegen_ref_runtime_prefix(type), name);
        return;
    }

    if (semantic_type_is_tuple(type)) {
        codegen_build_tuple_retain_name(helper_name, sizeof(helper_name), type);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "%s(&%s);\n", helper_name, name);
        return;
    }

    if (semantic_type_is_class(type)) {
        codegen_build_class_retain_name(helper_name, sizeof(helper_name), type);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "%s(&%s);\n", helper_name, name);
        return;
    }

    if (semantic_type_is_optional(type)) {
        codegen_build_optional_retain_name(helper_name, sizeof(helper_name), type);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "%s(&%s);\n", helper_name, name);
        return;
    }

    codegen_error("unsupported managed type %s", semantic_type_name(type));
}

void codegen_emit_value_release(CodegenContext *ctx, ValueType type, const char *name)
{
    char helper_name[MAX_NAME_LEN];

    if (!semantic_type_needs_management(type)) {
        codegen_error("unsupported managed type %s", semantic_type_name(type));
    }

    if (semantic_type_is_ref(type)) {
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "%s_decref(%s);\n", codegen_ref_runtime_prefix(type), name);
        return;
    }

    if (semantic_type_is_tuple(type)) {
        codegen_build_tuple_release_name(helper_name, sizeof(helper_name), type);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "%s(&%s);\n", helper_name, name);
        return;
    }

    if (semantic_type_is_class(type)) {
        codegen_build_class_release_name(helper_name, sizeof(helper_name), type);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "%s(&%s);\n", helper_name, name);
        return;
    }

    if (semantic_type_is_optional(type)) {
        codegen_build_optional_release_name(helper_name, sizeof(helper_name), type);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "%s(&%s);\n", helper_name, name);
        return;
    }

    codegen_error("unsupported managed type %s", semantic_type_name(type));
}

void codegen_emit_ref_incref(CodegenContext *ctx, ValueType type, const char *name)
{
    if (!semantic_type_is_ref(type)) {
        codegen_error("unsupported refcounted type %s", semantic_type_name(type));
    }
    codegen_emit_value_retain(ctx, type, name);
}

void codegen_emit_ref_decref(CodegenContext *ctx, ValueType type, const char *name)
{
    if (!semantic_type_is_ref(type)) {
        codegen_error("unsupported refcounted type %s", semantic_type_name(type));
    }
    codegen_emit_value_release(ctx, type, name);
}

void codegen_register_ref_local(CodegenContext *ctx, const char *name, ValueType type)
{
    if (!semantic_type_needs_management(type)) {
        return;
    }

    if (ctx->managed_local_count >= MAX_REF_LOCALS) {
        codegen_error("too many live managed locals");
    }

    ctx->managed_locals[ctx->managed_local_count].name = name;
    ctx->managed_locals[ctx->managed_local_count].type = type;
    ctx->managed_local_count++;
}

void codegen_emit_live_ref_cleanup(CodegenContext *ctx)
{
    for (size_t i = ctx->managed_local_count; i > 0; i--) {
        codegen_emit_value_release(ctx, ctx->managed_locals[i - 1].type, ctx->managed_locals[i - 1].name);
    }
}

void codegen_pop_cleanup_scope(CodegenContext *ctx)
{
    size_t start;

    if (ctx->cleanup_scope_count == 0) {
        codegen_error("cleanup scope stack underflow");
    }

    start = ctx->cleanup_scope_starts[--ctx->cleanup_scope_count];
    for (size_t i = ctx->managed_local_count; i > start; i--) {
        codegen_emit_value_release(ctx, ctx->managed_locals[i - 1].type, ctx->managed_locals[i - 1].name);
    }
    ctx->managed_local_count = start;
}

int codegen_expression_is_owned_ref(CodegenContext *ctx, const ParseNode *expr)
{
    const ParseNode *child;

    if (!semantic_type_needs_management(semantic_type_of(ctx->semantic, expr))) {
        return 0;
    }
    if (expr->kind != NODE_EXPRESSION || expr->child_count != 1) {
        return 0;
    }

    child = expr->children[0];
    return node_is_owned_ref_value(ctx, child);
}

static int node_is_borrowed_ref_value(const ParseNode *node)
{
    if (node == NULL) {
        return 0;
    }
    if (node->kind == NODE_PRIMARY && node->token_type == TOKEN_IDENTIFIER) {
        return 1;
    }
    if (node->kind == NODE_EXPRESSION && node->child_count == 1) {
        return node_is_borrowed_ref_value(node->children[0]);
    }
    return 0;
}

static int node_is_owned_ref_value(CodegenContext *ctx, const ParseNode *node)
{
    if (node == NULL) {
        return 0;
    }
    if (node->kind == NODE_CALL ||
        node->kind == NODE_LIST_LITERAL ||
        node->kind == NODE_DICT_LITERAL ||
        node->kind == NODE_METHOD_CALL ||
        node->kind == NODE_TUPLE_LITERAL) {
        return 1;
    }
    if (node->kind == NODE_INDEX) {
        ValueType base_type = semantic_type_of(ctx->semantic, node->children[0]);

        return semantic_type_is_list(base_type) &&
            semantic_type_needs_management(semantic_list_element_type(base_type));
    }
    if (node->kind == NODE_EXPRESSION && node->child_count == 1) {
        return node_is_owned_ref_value(ctx, node->children[0]);
    }
    return 0;
}

char *codegen_expression_to_c_string(CodegenContext *ctx, const ParseNode *expr);
char *codegen_wrapped_expression_to_c_string(CodegenContext *ctx, const ParseNode *expr, ValueType target_type);
char *codegen_primary_to_c_string(CodegenContext *ctx, const ParseNode *primary);

static char *materialize_ref_node(
    CodegenContext *ctx,
    const ParseNode *node,
    ValueType type,
    int use_primary_emitter,
    int *needs_cleanup)
{
    char *expr_text;
    char *temp_name;
    char *type_name;

    expr_text = use_primary_emitter
        ? codegen_primary_to_c_string(ctx, node)
        : codegen_expression_to_c_string(ctx, node);

    if (node_is_borrowed_ref_value(node)) {
        *needs_cleanup = 0;
        return expr_text;
    }

    temp_name = codegen_next_temp_name(ctx);
    type_name = codegen_type_to_c_string(type);
    codegen_emit_indent(ctx);
    fprintf(ctx->out, "%s %s = %s;\n", type_name, temp_name, expr_text);
    if (!node_is_owned_ref_value(ctx, node)) {
        codegen_emit_ref_incref(ctx, type, temp_name);
    }

    free(type_name);
    free(expr_text);
    *needs_cleanup = 1;
    return temp_name;
}

static char *call_to_c_string(CodegenContext *ctx, const ParseNode *call)
{
    const ParseNode *callee = codegen_expect_child(call, 0, NODE_PRIMARY);
    const ParseNode *arguments = codegen_expect_child(call, 1, NODE_ARGUMENTS);
    const char *function_c_name = NULL;
    ValueType return_type = semantic_type_of(ctx->semantic, call);
    ValueType class_type = semantic_call_constructor_type(ctx->semantic, call);
    int class_uses_init = class_type != 0 && semantic_class_has_initializer(ctx->semantic, class_type);
    char *arg_parts[32];
    int arg_owned[32] = {0};
    ValueType cleanup_types[32];
    char *cleanup_names[32];
    size_t arg_count = 0;
    size_t cleanup_count = 0;
    char *args;
    char *result;

    if (is_list_builtin_name(callee->value)) {
        if (strcmp(callee->value, "list_int") == 0) {
            return codegen_list_new_call(TYPE_LIST_INT);
        }
        if (strcmp(callee->value, "list_float") == 0) {
            return codegen_list_new_call(TYPE_LIST_FLOAT);
        }
        if (strcmp(callee->value, "list_bool") == 0) {
            return codegen_list_new_call(TYPE_LIST_BOOL);
        }
        if (strcmp(callee->value, "list_char") == 0) {
            return codegen_list_new_call(TYPE_LIST_CHAR);
        }
        if (strcmp(callee->value, "list_str") == 0) {
            return codegen_list_new_call(TYPE_LIST_STR);
        }
        if (strcmp(callee->value, "dict_str_str") == 0) {
            return codegen_dict_new_call(semantic_make_dict_type(TYPE_STR, TYPE_STR));
        }
    } else {
        if (class_type == 0) {
            function_c_name = semantic_call_c_name(ctx->semantic, call);
        }
        if (function_c_name == NULL && class_type == 0) {
            codegen_error("unknown function '%s' during code generation", callee->value);
        }
    }

    if (!(arguments->child_count == 1 && codegen_is_epsilon_node(arguments->children[0]))) {
        arg_count = arguments->child_count;
    }

    for (size_t i = 0; i < arg_count; i++) {
        const ParseNode *arg_node = arguments->children[i];
        ValueType arg_type = semantic_type_of(ctx->semantic, arg_node);
        ValueType target_type = arg_type;

        if (is_list_builtin_name(callee->value)) {
            if ((strcmp(callee->value, "list_append") == 0 ||
                 strcmp(callee->value, "list_get") == 0 ||
                 strcmp(callee->value, "list_len") == 0 ||
                 strcmp(callee->value, "len") == 0 ||
                 strcmp(callee->value, "ord") == 0 ||
                 strcmp(callee->value, "chr") == 0 ||
                 strcmp(callee->value, "list_set") == 0) && i == 0) {
                target_type = arg_type;
            } else if ((strcmp(callee->value, "list_append") == 0 && i == 1) ||
                (strcmp(callee->value, "list_set") == 0 && i == 2)) {
                target_type = semantic_list_element_type(semantic_type_of(ctx->semantic, arguments->children[0]));
            } else if (strcmp(callee->value, "len") == 0 && i == 0) {
                target_type = arg_type;
            }
        } else if (function_c_name != NULL) {
            target_type = semantic_call_parameter_type(ctx->semantic, call, i);
        } else if (class_uses_init) {
            target_type = semantic_method_parameter_type(ctx->semantic, class_type, "__init__", i);
        } else if (class_type != 0) {
            target_type = semantic_class_field_type(class_type, i);
        }

        if (semantic_type_is_ref(target_type) && semantic_type_is_ref(arg_type)) {
            int needs_cleanup = 0;
            char *part = materialize_ref_node(ctx, arg_node, target_type, 0, &needs_cleanup);

            arg_parts[i] = part;
            arg_owned[i] = node_is_owned_ref_value(ctx, arg_node);
            if (needs_cleanup && !(class_type != 0 && arg_owned[i])) {
                cleanup_names[cleanup_count] = codegen_dup_printf("%s", part);
                cleanup_types[cleanup_count] = target_type;
                cleanup_count++;
            }
            continue;
        }

        if (semantic_type_needs_management(target_type) && semantic_type_needs_management(arg_type) &&
            node_is_owned_ref_value(ctx, arg_node) && class_type == 0) {
            char *temp_name = codegen_next_temp_name(ctx);
            char *type_name = codegen_type_to_c_string(target_type);
            char *part = codegen_wrapped_expression_to_c_string(ctx, arg_node, target_type);

            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s %s = %s;\n", type_name, temp_name, part);
            free(type_name);
            free(part);
            arg_parts[i] = temp_name;
            arg_owned[i] = 1;
            cleanup_names[cleanup_count] = codegen_dup_printf("%s", temp_name);
            cleanup_types[cleanup_count] = target_type;
            cleanup_count++;
            continue;
        }

        arg_parts[i] = is_list_builtin_name(callee->value)
            ? codegen_expression_to_c_string(ctx, arg_node)
            : codegen_wrapped_expression_to_c_string(ctx, arg_node, target_type);
        arg_owned[i] = node_is_owned_ref_value(ctx, arg_node);
    }

    args = codegen_dup_printf("");
    for (size_t i = 0; i < arg_count; i++) {
        char *joined = i == 0
            ? codegen_dup_printf("%s", arg_parts[i])
            : codegen_dup_printf("%s, %s", args, arg_parts[i]);
        free(args);
        args = joined;
    }

    if (strcmp(callee->value, "list_append") == 0) {
        result = codegen_list_unary_call(semantic_type_of(ctx->semantic, arguments->children[0]), "append", args);
    } else if (strcmp(callee->value, "list_get") == 0) {
        result = codegen_list_unary_call(semantic_type_of(ctx->semantic, arguments->children[0]), "get", args);
    } else if (strcmp(callee->value, "ord") == 0) {
        result = codegen_dup_printf("py4_ord(%s)", args);
    } else if (strcmp(callee->value, "chr") == 0) {
        result = codegen_dup_printf("py4_chr(%s)", args);
    } else if (strcmp(callee->value, "list_len") == 0 || strcmp(callee->value, "len") == 0) {
        ValueType container_type = semantic_type_of(ctx->semantic, arguments->children[0]);

        if (semantic_type_is_dict(container_type)) {
            result = codegen_dict_unary_call(container_type, "len", args);
        } else if (container_type == TYPE_STR) {
            result = codegen_dup_printf("((int)strlen(%s))", args);
        } else {
            result = codegen_list_unary_call(container_type, "len", args);
        }
    } else if (strcmp(callee->value, "list_set") == 0) {
        result = codegen_list_unary_call(semantic_type_of(ctx->semantic, arguments->children[0]), "set", args);
    } else if (class_uses_init) {
        char ctor_name[MAX_NAME_LEN];

        codegen_build_class_ctor_name(ctor_name, sizeof(ctor_name), class_type);
        result = codegen_dup_printf("%s(%s)", ctor_name, args);
    } else if (class_type != 0) {
        if (semantic_type_needs_management(class_type)) {
            char *temp_name = codegen_next_temp_name(ctx);
            char *type_name = codegen_type_to_c_string(class_type);

            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s %s = ((%s){%s});\n", type_name, temp_name, type_name, args);
            for (size_t i = 0; i < arg_count; i++) {
                ValueType field_type = semantic_class_field_type(class_type, i);

                if (!semantic_type_needs_management(field_type) || arg_owned[i]) {
                    continue;
                }
                {
                    char *field_name = codegen_dup_printf("%s.%s", temp_name, semantic_class_field_name(class_type, i));

                    codegen_emit_value_retain(ctx, field_type, field_name);
                    free(field_name);
                }
            }
            result = temp_name;
            free(type_name);
            free(args);
            for (size_t i = 0; i < arg_count; i++) {
                free(arg_parts[i]);
            }
            for (size_t i = cleanup_count; i > 0; i--) {
                codegen_emit_value_release(ctx, cleanup_types[i - 1], cleanup_names[i - 1]);
                free(cleanup_names[i - 1]);
            }
            return result;
        } else {
            char *type_name = codegen_type_to_c_string(class_type);
            result = codegen_dup_printf("((%s){%s})", type_name, args);
            free(type_name);
        }
    } else {
        result = codegen_dup_printf("%s(%s)", function_c_name, args);
    }

    free(args);
    for (size_t i = 0; i < arg_count; i++) {
        free(arg_parts[i]);
    }

    if (cleanup_count == 0) {
        return result;
    }

    if (return_type == TYPE_NONE) {
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "%s;\n", result);
        free(result);
        for (size_t i = cleanup_count; i > 0; i--) {
            codegen_emit_ref_decref(ctx, cleanup_types[i - 1], cleanup_names[i - 1]);
            free(cleanup_names[i - 1]);
        }
        return codegen_dup_printf("(void)0");
    }

    {
        char *result_name = codegen_next_temp_name(ctx);
        char *type_name = codegen_type_to_c_string(return_type);

        codegen_emit_indent(ctx);
        fprintf(ctx->out, "%s %s = %s;\n", type_name, result_name, result);
        free(type_name);
        free(result);
        for (size_t i = cleanup_count; i > 0; i--) {
            codegen_emit_ref_decref(ctx, cleanup_types[i - 1], cleanup_names[i - 1]);
            free(cleanup_names[i - 1]);
        }
        return result_name;
    }
}

static char *method_call_to_c_string(CodegenContext *ctx, const ParseNode *call)
{
    const ParseNode *receiver;
    const ParseNode *method = codegen_expect_child(call, 1, NODE_PRIMARY);
    const ParseNode *arguments = codegen_expect_child(call, 2, NODE_ARGUMENTS);
    const char *receiver_module_name;
    ValueType receiver_type;
    int needs_cleanup = 0;
    char *receiver_name;
    char *result;

    if (call == NULL || call->child_count != 3) {
        codegen_error("malformed method call");
    }

    receiver = call->children[0];
    receiver_module_name = semantic_module_name_for_receiver(ctx->semantic, receiver);
    if (receiver_module_name != NULL && strcmp(receiver_module_name, "json") == 0 &&
        strcmp(method->value, "to_string") == 0) {
        const ParseNode *arg_node = arguments->children[0];
        ValueType arg_type = semantic_type_of(ctx->semantic, arg_node);
        ValueType json_value_type = semantic_find_native_type("json", "Value");
        char *arg_text;
        char *call_text;
        int cleanup_needed = 0;

        if (arg_type == json_value_type) {
            arg_text = materialize_ref_node(ctx, arg_node, arg_type, 0, &cleanup_needed);
            call_text = codegen_dup_printf("py4_json_stringify_handle(%s)", arg_text);
        } else {
            char helper_name[MAX_NAME_LEN];
            if (semantic_type_needs_management(arg_type) && node_is_owned_ref_value(ctx, arg_node)) {
                char *temp_name = codegen_next_temp_name(ctx);
                char *type_name = codegen_type_to_c_string(arg_type);
                char *part = codegen_wrapped_expression_to_c_string(ctx, arg_node, arg_type);

                codegen_emit_indent(ctx);
                fprintf(ctx->out, "%s %s = %s;\n", type_name, temp_name, part);
                free(type_name);
                free(part);
                arg_text = temp_name;
                cleanup_needed = 2;
            } else {
                arg_text = codegen_wrapped_expression_to_c_string(ctx, arg_node, arg_type);
            }
            codegen_build_json_to_string_name(helper_name, sizeof(helper_name), arg_type);
            call_text = codegen_dup_printf("%s(%s)", helper_name, arg_text);
        }

        if (!cleanup_needed) {
            free(arg_text);
            return call_text;
        }

        {
            char *result_name = codegen_next_temp_name(ctx);

            codegen_emit_indent(ctx);
            fprintf(ctx->out, "const char *%s = %s;\n", result_name, call_text);
            if (cleanup_needed == 1) {
                codegen_emit_ref_decref(ctx, arg_type, arg_text);
            } else {
                codegen_emit_value_release(ctx, arg_type, arg_text);
            }
            free(call_text);
            free(arg_text);
            return result_name;
        }
    }
    if (semantic_has_call_target(ctx->semantic, call)) {
        ValueType return_type = semantic_type_of(ctx->semantic, call);
        size_t arg_count = (arguments->child_count == 1 && codegen_is_epsilon_node(arguments->children[0]))
            ? 0
            : arguments->child_count;
        ValueType cleanup_types[32];
        char *cleanup_names[32];
        char *arg_parts[32];
        size_t cleanup_count = 0;
        char *args = codegen_dup_printf("");

        for (size_t i = 0; i < arg_count; i++) {
            ValueType target_type = semantic_call_parameter_type(ctx->semantic, call, i);
            ValueType arg_type = semantic_type_of(ctx->semantic, arguments->children[i]);

            if (semantic_type_needs_management(target_type) &&
                semantic_type_needs_management(arg_type) &&
                node_is_owned_ref_value(ctx, arguments->children[i])) {
                char *temp_name = codegen_next_temp_name(ctx);
                char *type_name = codegen_type_to_c_string(target_type);
                char *part = codegen_wrapped_expression_to_c_string(ctx, arguments->children[i], target_type);

                codegen_emit_indent(ctx);
                fprintf(ctx->out, "%s %s = %s;\n", type_name, temp_name, part);
                free(type_name);
                free(part);
                cleanup_names[cleanup_count] = codegen_dup_printf("%s", temp_name);
                cleanup_types[cleanup_count] = target_type;
                cleanup_count++;
                arg_parts[i] = temp_name;
            } else {
                arg_parts[i] = codegen_wrapped_expression_to_c_string(ctx, arguments->children[i], target_type);
            }

            {
                char *joined = i == 0
                    ? codegen_dup_printf("%s", arg_parts[i])
                    : codegen_dup_printf("%s, %s", args, arg_parts[i]);
                free(args);
                args = joined;
            }
        }

        result = codegen_dup_printf("%s(%s)", semantic_call_c_name(ctx->semantic, call), args);
        free(args);
        for (size_t i = 0; i < arg_count; i++) {
            free(arg_parts[i]);
        }

        if (cleanup_count == 0) {
            return result;
        }
        if (return_type == TYPE_NONE) {
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s;\n", result);
            free(result);
            for (size_t i = cleanup_count; i > 0; i--) {
                codegen_emit_value_release(ctx, cleanup_types[i - 1], cleanup_names[i - 1]);
                free(cleanup_names[i - 1]);
            }
            return codegen_dup_printf("(void)0");
        }

        {
            char *result_name = codegen_next_temp_name(ctx);
            char *type_name = codegen_type_to_c_string(return_type);

            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s %s = %s;\n", type_name, result_name, result);
            free(type_name);
            free(result);
            for (size_t i = cleanup_count; i > 0; i--) {
                codegen_emit_value_release(ctx, cleanup_types[i - 1], cleanup_names[i - 1]);
                free(cleanup_names[i - 1]);
            }
            return result_name;
        }
    }

    {
        ValueType class_type = semantic_call_constructor_type(ctx->semantic, call);
        int class_uses_init = class_type != 0 && semantic_class_has_initializer(ctx->semantic, class_type);

        if (class_type != 0) {
            size_t arg_count = (arguments->child_count == 1 && codegen_is_epsilon_node(arguments->children[0]))
                ? 0
                : arguments->child_count;
            char *arg_parts[32];
            int arg_owned[32] = {0};
            ValueType cleanup_types[32];
            char *cleanup_names[32];
            size_t cleanup_count = 0;
            char *args = codegen_dup_printf("");

            for (size_t i = 0; i < arg_count; i++) {
                const ParseNode *arg_node = arguments->children[i];
                ValueType arg_type = semantic_type_of(ctx->semantic, arg_node);
                ValueType target_type = class_uses_init
                    ? semantic_method_parameter_type(ctx->semantic, class_type, "__init__", i)
                    : semantic_class_field_type(class_type, i);

                if (semantic_type_is_ref(target_type) && semantic_type_is_ref(arg_type)) {
                    int arg_needs_cleanup = 0;
                    char *part = materialize_ref_node(ctx, arg_node, target_type, 0, &arg_needs_cleanup);

                    arg_parts[i] = part;
                    arg_owned[i] = node_is_owned_ref_value(ctx, arg_node);
                    if (arg_needs_cleanup && !arg_owned[i]) {
                        cleanup_names[cleanup_count] = codegen_dup_printf("%s", part);
                        cleanup_types[cleanup_count] = target_type;
                        cleanup_count++;
                    }
                } else {
                    arg_parts[i] = codegen_wrapped_expression_to_c_string(ctx, arg_node, target_type);
                    arg_owned[i] = node_is_owned_ref_value(ctx, arg_node);
                }

                {
                    char *joined = i == 0
                        ? codegen_dup_printf("%s", arg_parts[i])
                        : codegen_dup_printf("%s, %s", args, arg_parts[i]);
                    free(args);
                    args = joined;
                }
            }

            if (class_uses_init) {
                char ctor_name[MAX_NAME_LEN];

                codegen_build_class_ctor_name(ctor_name, sizeof(ctor_name), class_type);
                result = codegen_dup_printf("%s(%s)", ctor_name, args);
                free(args);
                for (size_t i = 0; i < arg_count; i++) {
                    free(arg_parts[i]);
                }
                for (size_t i = cleanup_count; i > 0; i--) {
                    codegen_emit_value_release(ctx, cleanup_types[i - 1], cleanup_names[i - 1]);
                    free(cleanup_names[i - 1]);
                }
                return result;
            }

            if (semantic_type_needs_management(class_type)) {
                char *temp_name = codegen_next_temp_name(ctx);
                char *type_name = codegen_type_to_c_string(class_type);

                codegen_emit_indent(ctx);
                fprintf(ctx->out, "%s %s = ((%s){%s});\n", type_name, temp_name, type_name, args);
                for (size_t i = 0; i < arg_count; i++) {
                    ValueType field_type = semantic_class_field_type(class_type, i);

                    if (!semantic_type_needs_management(field_type) || arg_owned[i]) {
                        continue;
                    }
                    {
                        char *field_name = codegen_dup_printf("%s.%s", temp_name, semantic_class_field_name(class_type, i));

                        codegen_emit_value_retain(ctx, field_type, field_name);
                        free(field_name);
                    }
                }

                free(type_name);
                free(args);
                for (size_t i = 0; i < arg_count; i++) {
                    free(arg_parts[i]);
                }
                for (size_t i = cleanup_count; i > 0; i--) {
                    codegen_emit_value_release(ctx, cleanup_types[i - 1], cleanup_names[i - 1]);
                    free(cleanup_names[i - 1]);
                }
                return temp_name;
            } else {
                char *type_name = codegen_type_to_c_string(class_type);
                char *constructed = codegen_dup_printf("((%s){%s})", type_name, args);

                free(type_name);
                free(args);
                for (size_t i = 0; i < arg_count; i++) {
                    free(arg_parts[i]);
                }
                for (size_t i = cleanup_count; i > 0; i--) {
                    codegen_emit_ref_decref(ctx, cleanup_types[i - 1], cleanup_names[i - 1]);
                    free(cleanup_names[i - 1]);
                }
                return constructed;
            }
        }
    }

    if (is_super_call_node(receiver)) {
        ValueType owner_type = ctx->current_method_owner_type;
        ValueType base_type;
        size_t arg_count;
        ValueType return_type;
        ValueType cleanup_types[32];
        char *cleanup_names[32];
        size_t cleanup_count = 0;
        char *args;
        char *result;

        if (owner_type == 0) {
            codegen_error("super() can only be used inside class methods");
        }
        base_type = semantic_class_base_type(owner_type);
        if (base_type == 0) {
            codegen_error("class '%s' has no base class for super()",
                semantic_class_name(owner_type));
        }

        arg_count = (arguments->child_count == 1 && codegen_is_epsilon_node(arguments->children[0]))
            ? 0
            : arguments->child_count;
        return_type = semantic_type_of(ctx->semantic, call);
        args = codegen_dup_printf("");

        for (size_t i = 0; i < arg_count; i++) {
            ValueType target_type = semantic_method_parameter_type(ctx->semantic, base_type, method->value, i);
            ValueType arg_type = semantic_type_of(ctx->semantic, arguments->children[i]);
            char *arg;
            char *joined;

            if (semantic_type_needs_management(target_type) &&
                semantic_type_needs_management(arg_type) &&
                node_is_owned_ref_value(ctx, arguments->children[i])) {
                char *temp_name = codegen_next_temp_name(ctx);
                char *type_name = codegen_type_to_c_string(target_type);
                char *part = codegen_wrapped_expression_to_c_string(ctx, arguments->children[i], target_type);

                codegen_emit_indent(ctx);
                fprintf(ctx->out, "%s %s = %s;\n", type_name, temp_name, part);
                free(type_name);
                free(part);
                cleanup_names[cleanup_count] = codegen_dup_printf("%s", temp_name);
                cleanup_types[cleanup_count] = target_type;
                cleanup_count++;
                arg = temp_name;
            } else {
                arg = codegen_wrapped_expression_to_c_string(ctx, arguments->children[i], target_type);
            }

            joined = i == 0
                ? codegen_dup_printf("%s", arg)
                : codegen_dup_printf("%s, %s", args, arg);
            free(args);
            free(arg);
            args = joined;
        }

        if (strcmp(method->value, "__init__") == 0) {
            char *base_name = codegen_next_temp_name(ctx);
            char *base_expr = codegen_build_base_self_value(ctx, owner_type, base_type);

            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s %s = %s;\n",
                semantic_class_name(base_type),
                base_name,
                base_expr);
            free(base_expr);

            {
                char *call_args = arg_count == 0
                    ? codegen_dup_printf("&%s", base_name)
                    : codegen_dup_printf("&%s, %s", base_name, args);
                result = codegen_dup_printf("%s(%s)",
                    semantic_method_c_name(ctx->semantic, base_type, method->value),
                    call_args);
                free(call_args);
            }
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s;\n", result);
            codegen_emit_base_self_writeback(ctx, owner_type, base_type, base_name);
            for (size_t i = cleanup_count; i > 0; i--) {
                codegen_emit_value_release(ctx, cleanup_types[i - 1], cleanup_names[i - 1]);
                free(cleanup_names[i - 1]);
            }
            free(base_name);
            free(args);
            free(result);
            return codegen_dup_printf("(void)0");
        }

        {
            char *base_expr = codegen_build_base_self_value(ctx, owner_type, base_type);
            char *call_args = arg_count == 0
                ? codegen_dup_printf("%s", base_expr)
                : codegen_dup_printf("%s, %s", base_expr, args);
            result = codegen_dup_printf("%s(%s)",
                semantic_method_c_name(ctx->semantic, base_type, method->value),
                call_args);
            free(base_expr);
            free(call_args);
        }
        free(args);

        if (cleanup_count == 0) {
            return result;
        }
        if (return_type == TYPE_NONE) {
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s;\n", result);
            free(result);
            for (size_t i = cleanup_count; i > 0; i--) {
                codegen_emit_value_release(ctx, cleanup_types[i - 1], cleanup_names[i - 1]);
                free(cleanup_names[i - 1]);
            }
            return codegen_dup_printf("(void)0");
        }

        {
            char *result_name = codegen_next_temp_name(ctx);
            char *type_name = codegen_type_to_c_string(return_type);

            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s %s = %s;\n", type_name, result_name, result);
            free(type_name);
            free(result);
            for (size_t i = cleanup_count; i > 0; i--) {
                codegen_emit_value_release(ctx, cleanup_types[i - 1], cleanup_names[i - 1]);
                free(cleanup_names[i - 1]);
            }
            return result_name;
        }
    }

    receiver_type = semantic_type_of(ctx->semantic, receiver);
    if (receiver_type == TYPE_STR) {
        char *receiver_text = codegen_primary_to_c_string(ctx, receiver);
        char *receiver_name = codegen_next_temp_name(ctx);
        char *result_name = codegen_next_temp_name(ctx);
        ValueType return_type = semantic_type_of(ctx->semantic, call);
        char *result_type_name = codegen_type_to_c_string(return_type);

        codegen_emit_indent(ctx);
        fprintf(ctx->out, "const char *%s = %s;\n", receiver_name, receiver_text);

        if (strcmp(method->value, "starts_with") == 0) {
            char *arg = codegen_wrapped_expression_to_c_string(ctx, arguments->children[0], TYPE_STR);
            char *arg_name = codegen_next_temp_name(ctx);

            codegen_emit_indent(ctx);
            fprintf(ctx->out, "const char *%s = %s;\n", arg_name, arg);
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s %s = py4_str_starts_with(%s, %s);\n",
                result_type_name, result_name, receiver_name, arg_name);
            free(arg);
            free(arg_name);
        } else if (strcmp(method->value, "ends_with") == 0) {
            char *arg = codegen_wrapped_expression_to_c_string(ctx, arguments->children[0], TYPE_STR);
            char *arg_name = codegen_next_temp_name(ctx);

            codegen_emit_indent(ctx);
            fprintf(ctx->out, "const char *%s = %s;\n", arg_name, arg);
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s %s = py4_str_ends_with(%s, %s);\n",
                result_type_name, result_name, receiver_name, arg_name);
            free(arg);
            free(arg_name);
        } else if (strcmp(method->value, "find") == 0) {
            char *arg = codegen_wrapped_expression_to_c_string(ctx, arguments->children[0], TYPE_STR);
            char *arg_name = codegen_next_temp_name(ctx);

            codegen_emit_indent(ctx);
            fprintf(ctx->out, "const char *%s = %s;\n", arg_name, arg);
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s %s = py4_str_find(%s, %s);\n",
                result_type_name, result_name, receiver_name, arg_name);
            free(arg);
            free(arg_name);
        } else if (strcmp(method->value, "split") == 0) {
            char *arg = codegen_wrapped_expression_to_c_string(ctx, arguments->children[0], TYPE_STR);
            char *arg_name = codegen_next_temp_name(ctx);

            codegen_emit_indent(ctx);
            fprintf(ctx->out, "const char *%s = %s;\n", arg_name, arg);
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s %s = py4_str_split(%s, %s);\n",
                result_type_name, result_name, receiver_name, arg_name);
            free(arg);
            free(arg_name);
        } else if (strcmp(method->value, "replace") == 0) {
            char *old_text = codegen_wrapped_expression_to_c_string(ctx, arguments->children[0], TYPE_STR);
            char *new_text = codegen_wrapped_expression_to_c_string(ctx, arguments->children[1], TYPE_STR);
            char *old_name = codegen_next_temp_name(ctx);
            char *new_name = codegen_next_temp_name(ctx);

            codegen_emit_indent(ctx);
            fprintf(ctx->out, "const char *%s = %s;\n", old_name, old_text);
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "const char *%s = %s;\n", new_name, new_text);
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s %s = py4_str_replace(%s, %s, %s);\n",
                result_type_name, result_name, receiver_name, old_name, new_name);
            free(old_text);
            free(new_text);
            free(old_name);
            free(new_name);
        } else {
            free(receiver_text);
            free(receiver_name);
            free(result_name);
            free(result_type_name);
            codegen_error("unknown method '%s' during code generation", method->value);
        }

        free(receiver_text);
        free(receiver_name);
        free(result_type_name);
        return result_name;
    }

    if (semantic_type_is_class(receiver_type)) {
        size_t arg_count = (arguments->child_count == 1 && codegen_is_epsilon_node(arguments->children[0]))
            ? 0
            : arguments->child_count;
        ValueType return_type = semantic_type_of(ctx->semantic, call);
        int receiver_owned = node_is_owned_ref_value(ctx, receiver);
        ValueType cleanup_types[32];
        char *cleanup_names[32];
        size_t cleanup_count = 0;
        char *receiver_text = codegen_primary_to_c_string(ctx, receiver);
        char *args = codegen_dup_printf("%s", receiver_text);
        char *result;

        if (semantic_type_needs_management(receiver_type) && receiver_owned) {
            char *temp_name = codegen_next_temp_name(ctx);
            char *type_name = codegen_type_to_c_string(receiver_type);

            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s %s = %s;\n", type_name, temp_name, receiver_text);
            free(type_name);
            free(receiver_text);
            receiver_text = temp_name;
            free(args);
            args = codegen_dup_printf("%s", receiver_text);
            cleanup_names[cleanup_count] = codegen_dup_printf("%s", receiver_text);
            cleanup_types[cleanup_count] = receiver_type;
            cleanup_count++;
        }

        for (size_t i = 0; i < arg_count; i++) {
            ValueType target_type = semantic_method_parameter_type(ctx->semantic, receiver_type, method->value, i);
            ValueType arg_type = semantic_type_of(ctx->semantic, arguments->children[i]);
            char *arg;

            if (semantic_type_needs_management(target_type) &&
                semantic_type_needs_management(arg_type) &&
                node_is_owned_ref_value(ctx, arguments->children[i])) {
                char *temp_name = codegen_next_temp_name(ctx);
                char *type_name = codegen_type_to_c_string(target_type);
                char *part = codegen_wrapped_expression_to_c_string(ctx, arguments->children[i], target_type);

                codegen_emit_indent(ctx);
                fprintf(ctx->out, "%s %s = %s;\n", type_name, temp_name, part);
                free(type_name);
                free(part);
                cleanup_names[cleanup_count] = codegen_dup_printf("%s", temp_name);
                cleanup_types[cleanup_count] = target_type;
                cleanup_count++;
                arg = temp_name;
            } else {
                arg = codegen_wrapped_expression_to_c_string(ctx, arguments->children[i], target_type);
            }
            char *joined = codegen_dup_printf("%s, %s", args, arg);

            free(args);
            free(arg);
            args = joined;
        }

        result = codegen_dup_printf("%s(%s)",
            semantic_method_c_name(ctx->semantic, receiver_type, method->value),
            args);
        free(receiver_text);
        free(args);
        if (cleanup_count == 0) {
            return result;
        }
        if (return_type == TYPE_NONE) {
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s;\n", result);
            free(result);
            for (size_t i = cleanup_count; i > 0; i--) {
                codegen_emit_value_release(ctx, cleanup_types[i - 1], cleanup_names[i - 1]);
                free(cleanup_names[i - 1]);
            }
            return codegen_dup_printf("(void)0");
        }

        {
            char *result_name = codegen_next_temp_name(ctx);
            char *type_name = codegen_type_to_c_string(return_type);

            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s %s = %s;\n", type_name, result_name, result);
            free(type_name);
            free(result);
            for (size_t i = cleanup_count; i > 0; i--) {
                codegen_emit_value_release(ctx, cleanup_types[i - 1], cleanup_names[i - 1]);
                free(cleanup_names[i - 1]);
            }
            return result_name;
        }
    }

    receiver_name = materialize_ref_node(ctx, receiver, receiver_type, 1, &needs_cleanup);

    if (semantic_type_is_dict(receiver_type)) {
        ValueType key_type = semantic_dict_key_type(receiver_type);
        ValueType value_type = semantic_dict_value_type(receiver_type);

        if (!is_dict_method_name(method->value)) {
            free(receiver_name);
            codegen_error("unknown method '%s' during code generation", method->value);
        }

        if (strcmp(method->value, "set") == 0) {
            char *key = codegen_wrapped_expression_to_c_string(ctx, arguments->children[0], key_type);
            char *value = codegen_wrapped_expression_to_c_string(ctx, arguments->children[1], value_type);

            result = codegen_dict_ternary_call(receiver_type, "set", receiver_name, key, value);
            free(key);
            free(value);
        } else if (strcmp(method->value, "get") == 0) {
            char *key = codegen_wrapped_expression_to_c_string(ctx, arguments->children[0], key_type);

            result = codegen_dict_binary_call(receiver_type, "get", receiver_name, key);
            free(key);
        } else if (strcmp(method->value, "get_or") == 0) {
            char *key = codegen_wrapped_expression_to_c_string(ctx, arguments->children[0], key_type);
            char *fallback = codegen_wrapped_expression_to_c_string(ctx, arguments->children[1], value_type);

            result = codegen_dict_ternary_call(receiver_type, "get_or", receiver_name, key, fallback);
            free(key);
            free(fallback);
        } else if (strcmp(method->value, "contains") == 0) {
            char *key = codegen_wrapped_expression_to_c_string(ctx, arguments->children[0], key_type);

            result = codegen_dict_binary_call(receiver_type, "contains", receiver_name, key);
            free(key);
        } else if (strcmp(method->value, "keys") == 0) {
            result = codegen_dict_unary_call(receiver_type, "keys", receiver_name);
        } else if (strcmp(method->value, "values") == 0) {
            result = codegen_dict_unary_call(receiver_type, "values", receiver_name);
        } else if (strcmp(method->value, "items") == 0) {
            result = codegen_dict_unary_call(receiver_type, "items", receiver_name);
        } else if (strcmp(method->value, "pop") == 0) {
            char *key = codegen_wrapped_expression_to_c_string(ctx, arguments->children[0], key_type);

            result = codegen_dict_binary_call(receiver_type, "pop", receiver_name, key);
            free(key);
        } else if (strcmp(method->value, "clear") == 0) {
            result = codegen_dict_unary_call(receiver_type, "clear", receiver_name);
        } else if (strcmp(method->value, "copy") == 0) {
            result = codegen_dict_unary_call(receiver_type, "copy", receiver_name);
        } else {
            free(receiver_name);
            codegen_error("unknown method '%s' during code generation", method->value);
        }
    } else {
        if (!is_list_method_name(method->value)) {
            free(receiver_name);
            codegen_error("unknown method '%s' during code generation", method->value);
        }

        if (strcmp(method->value, "append") == 0) {
            char *value = codegen_wrapped_expression_to_c_string(
                ctx,
                arguments->children[0],
                semantic_list_element_type(receiver_type));
            result = codegen_list_binary_call(receiver_type, "append", receiver_name, value);
            free(value);
        } else if (strcmp(method->value, "pop") == 0) {
            result = codegen_list_unary_call(receiver_type, "pop", receiver_name);
        } else if (strcmp(method->value, "clear") == 0) {
            result = codegen_list_unary_call(receiver_type, "clear", receiver_name);
        } else if (strcmp(method->value, "copy") == 0) {
            result = codegen_list_unary_call(receiver_type, "copy", receiver_name);
        } else {
            free(receiver_name);
            codegen_error("unknown method '%s' during code generation", method->value);
        }
    }

    if (needs_cleanup) {
        ValueType return_type = semantic_type_of(ctx->semantic, call);

        if (return_type == TYPE_NONE) {
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s;\n", result);
            free(result);
            codegen_emit_ref_decref(ctx, receiver_type, receiver_name);
            free(receiver_name);
            return codegen_dup_printf("(void)0");
        }

        {
            char *result_name = codegen_next_temp_name(ctx);
            char *type_name = codegen_type_to_c_string(return_type);

            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s %s = %s;\n", type_name, result_name, result);
            free(type_name);
            free(result);
            codegen_emit_ref_decref(ctx, receiver_type, receiver_name);
            free(receiver_name);
            return result_name;
        }
    }

    free(receiver_name);
    return result;
}

char *codegen_primary_to_c_string(CodegenContext *ctx, const ParseNode *primary)
{
    const char *global_name = semantic_global_target_c_name(ctx->semantic, primary);

    if (global_name != NULL) {
        return codegen_dup_printf("%s", global_name);
    }

    if (is_init_self_identifier(ctx, primary)) {
        return codegen_dup_printf("(*self)");
    }

    if (primary->kind == NODE_CALL) {
        return call_to_c_string(ctx, primary);
    }

    if (primary->kind == NODE_TYPED_CALL) {
        ValueType target_type = semantic_type_of(ctx->semantic, primary);
        char helper_name[MAX_NAME_LEN];
        char *argument = codegen_wrapped_expression_to_c_string(ctx, primary->children[2]->children[0], TYPE_STR);
        char *result;

        codegen_build_json_from_string_name(helper_name, sizeof(helper_name), target_type);
        result = codegen_dup_printf("%s(%s)", helper_name, argument);
        free(argument);
        return result;
    }

    if (primary->kind == NODE_METHOD_CALL) {
        return method_call_to_c_string(ctx, primary);
    }

    if (primary->kind == NODE_FIELD_ACCESS) {
        ValueType enum_type;
        size_t variant_index;

        if (semantic_enum_variant_for_node(ctx->semantic, primary, &enum_type, &variant_index)) {
            char value_name[MAX_NAME_LEN];

            codegen_build_enum_value_name(value_name, sizeof(value_name), enum_type, variant_index);
            return codegen_dup_printf("%s", value_name);
        }

        char *base = codegen_primary_to_c_string(ctx, primary->children[0]);
        char *result = codegen_dup_printf("((%s).%s)", base, primary->children[1]->value);

        free(base);
        return result;
    }

    if (primary->kind == NODE_LIST_LITERAL) {
        ValueType list_type = semantic_type_of(ctx->semantic, primary);
        ValueType element_type = semantic_list_element_type(list_type);

        if (primary->child_count == 0) {
            return codegen_list_new_call(list_type);
        }

        {
            char *values = codegen_dup_printf("");
            char *result;

            for (size_t i = 0; i < primary->child_count; i++) {
                char *item = codegen_wrapped_expression_to_c_string(ctx, primary->children[i], element_type);
                char *joined = i == 0
                    ? codegen_dup_printf("%s", item)
                    : codegen_dup_printf("%s, %s", values, item);

                free(values);
                free(item);
                values = joined;
            }

            result = codegen_dup_printf("%s_from_values(%zu, (%s[]){%s})",
                codegen_list_runtime_prefix(list_type),
                primary->child_count,
                codegen_list_element_c_type(list_type),
                values);
            free(values);
            return result;
        }
    }

    if (primary->kind == NODE_DICT_LITERAL) {
        ValueType dict_type = semantic_type_of(ctx->semantic, primary);
        ValueType key_type = semantic_dict_key_type(dict_type);
        ValueType value_type = semantic_dict_value_type(dict_type);
        char *key_c_type = codegen_type_to_c_string(key_type);
        char *value_c_type = codegen_type_to_c_string(value_type);

        if (primary->child_count == 0) {
            free(key_c_type);
            free(value_c_type);
            return codegen_dict_new_call(dict_type);
        }

        {
            char *keys = codegen_dup_printf("");
            char *values = codegen_dup_printf("");
            char *result;

            for (size_t i = 0; i + 1 < primary->child_count; i += 2) {
                char *key = codegen_wrapped_expression_to_c_string(ctx, primary->children[i], key_type);
                char *value = codegen_wrapped_expression_to_c_string(ctx, primary->children[i + 1], value_type);
                char *joined_keys = i == 0
                    ? codegen_dup_printf("%s", key)
                    : codegen_dup_printf("%s, %s", keys, key);
                char *joined_values = i == 0
                    ? codegen_dup_printf("%s", value)
                    : codegen_dup_printf("%s, %s", values, value);

                free(keys);
                free(values);
                free(key);
                free(value);
                keys = joined_keys;
                values = joined_values;
            }

            result = codegen_dup_printf("%s_from_pairs(%zu, (const %s[]){%s}, (const %s[]){%s})",
                codegen_dict_runtime_prefix(dict_type),
                primary->child_count / 2,
                key_c_type,
                keys,
                value_c_type,
                values);
            free(keys);
            free(values);
            free(key_c_type);
            free(value_c_type);
            return result;
        }
    }

    if (primary->kind == NODE_TUPLE_LITERAL) {
        ValueType tuple_type = semantic_type_of(ctx->semantic, primary);
        size_t element_count = semantic_tuple_element_count(tuple_type);
        char *values = codegen_dup_printf("");
        char *result;
        char *type_name;

        for (size_t i = 0; i < element_count; i++) {
            ValueType element_type = semantic_tuple_element_type(tuple_type, i);
            char *item = codegen_wrapped_expression_to_c_string(ctx, primary->children[i], element_type);
            char *joined = i == 0
                ? codegen_dup_printf("%s", item)
                : codegen_dup_printf("%s, %s", values, item);

            free(values);
            free(item);
            values = joined;
        }

        type_name = codegen_type_to_c_string(tuple_type);
        if (semantic_type_needs_management(tuple_type)) {
            char *temp_name = codegen_next_temp_name(ctx);

            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s %s = ((%s){%s});\n", type_name, temp_name, type_name, values);
            for (size_t i = 0; i < element_count; i++) {
                ValueType element_type = semantic_tuple_element_type(tuple_type, i);

                if (!semantic_type_needs_management(element_type) || node_is_owned_ref_value(ctx, primary->children[i])) {
                    continue;
                }
                {
                    char *field_name = codegen_dup_printf("%s.item%zu", temp_name, i);

                    codegen_emit_value_retain(ctx, element_type, field_name);
                    free(field_name);
                }
            }
            result = temp_name;
        } else {
            result = codegen_dup_printf("((%s){%s})", type_name, values);
        }
        free(type_name);
        free(values);
        return result;
    }

    if (primary->kind == NODE_INDEX) {
        ValueType base_type = semantic_type_of(ctx->semantic, primary->children[0]);
        if (semantic_type_is_tuple(base_type)) {
            size_t tuple_index;
            char *base = codegen_primary_to_c_string(ctx, primary->children[0]);
            char *result;

            if (!semantic_tuple_literal_index(primary->children[1], &tuple_index)) {
                codegen_error("tuple index must be a non-negative integer literal");
            }

            result = codegen_dup_printf("((%s).item%zu)", base, tuple_index);
            free(base);
            return result;
        } else {
            ValueType element_type;
            int needs_cleanup = 0;
            char *base;
            char *index = codegen_expression_to_c_string(ctx, primary->children[1]);
            char *call_text;
            char *result;

            if (base_type == TYPE_STR) {
                base = codegen_primary_to_c_string(ctx, primary->children[0]);
                call_text = codegen_dup_printf("py4_str_get(%s, %s)", base, index);
                result = codegen_dup_printf("%s", call_text);
                free(base);
                free(index);
                free(call_text);
                return result;
            }

            base = materialize_ref_node(ctx, primary->children[0], base_type, 1, &needs_cleanup);
            if (semantic_type_is_dict(base_type)) {
                element_type = semantic_dict_value_type(base_type);
                call_text = codegen_dict_binary_call(base_type, "get", base, index);
            } else {
                element_type = semantic_list_element_type(base_type);
                call_text = codegen_list_binary_call(base_type, "get", base, index);
            }

            if (needs_cleanup) {
                char *result_name = codegen_next_temp_name(ctx);
                char *type_name = codegen_type_to_c_string(element_type);

                codegen_emit_indent(ctx);
                fprintf(ctx->out, "%s %s = %s;\n", type_name, result_name, call_text);
                codegen_emit_ref_decref(ctx, base_type, base);
                free(type_name);
                result = result_name;
            } else {
                result = codegen_dup_printf("%s", call_text);
            }

            free(base);
            free(index);
            free(call_text);
            return result;
        }
    }

    if (primary->kind == NODE_SLICE) {
        char *base = codegen_primary_to_c_string(ctx, primary->children[0]);
        char *start = codegen_is_epsilon_node(primary->children[1])
            ? codegen_dup_printf("0")
            : codegen_expression_to_c_string(ctx, primary->children[1]);
        char *end = codegen_is_epsilon_node(primary->children[2])
            ? codegen_dup_printf("((int)strlen(%s))", base)
            : codegen_expression_to_c_string(ctx, primary->children[2]);
        char *result = codegen_dup_printf("py4_str_slice(%s, %s, %s)", base, start, end);

        free(base);
        free(start);
        free(end);
        return result;
    }

    if (primary->kind == NODE_EXPRESSION) {
        return codegen_expression_to_c_string(ctx, primary);
    }

    if (primary->kind != NODE_PRIMARY || primary->value == NULL) {
        codegen_error("unsupported primary node");
    }

    if (primary->token_type == TOKEN_KEYWORD) {
        if (strcmp(primary->value, "True") == 0) {
            return codegen_dup_printf("true");
        }
        if (strcmp(primary->value, "False") == 0) {
            return codegen_dup_printf("false");
        }
    }

    return codegen_dup_printf("%s", primary->value);
}

static char *chain_comparison_to_c_string(CodegenContext *ctx, const ParseNode *expr)
{
    size_t operand_count = (expr->child_count + 1) / 2;
    char **temp_names = calloc(operand_count, sizeof(char *));
    char *result;

    if (temp_names == NULL) {
        perror("calloc");
        exit(1);
    }

    for (size_t i = 0; i < expr->child_count; i += 2) {
        const ParseNode *operand = expr->children[i];
        ValueType operand_type = semantic_type_of(ctx->semantic, operand);
        char *operand_code = codegen_primary_to_c_string(ctx, operand);
        char *temp_name = codegen_next_temp_name(ctx);
        char *type_name = codegen_type_to_c_string(operand_type);

        codegen_emit_indent(ctx);
        fprintf(ctx->out, "%s %s = %s;\n", type_name, temp_name, operand_code);
        temp_names[i / 2] = temp_name;

        free(type_name);
        free(operand_code);
    }

    result = codegen_dup_printf("");
    for (size_t i = 1; i < expr->child_count; i += 2) {
        const ParseNode *op = codegen_expect_child(expr, i, NODE_OPERATOR);
        char *piece = codegen_dup_printf("(%s %s %s)",
            temp_names[(i - 1) / 2],
            map_operator(op->value),
            temp_names[(i + 1) / 2]);
        char *joined = i == 1
            ? codegen_dup_printf("%s", piece)
            : codegen_dup_printf("%s && %s", result, piece);

        free(result);
        free(piece);
        result = joined;
    }

    {
        char *wrapped = codegen_dup_printf("(%s)", result);
        for (size_t i = 0; i < operand_count; i++) {
            free(temp_names[i]);
        }
        free(temp_names);
        free(result);
        return wrapped;
    }
}

char *codegen_expression_to_c_string(CodegenContext *ctx, const ParseNode *expr)
{
    const ParseNode *operator_node;
    char *lhs;
    char *rhs;
    char *result;

    if (expr->kind != NODE_EXPRESSION || expr->child_count == 0) {
        codegen_error("malformed expression node");
    }

    if (is_chained_comparison_expr(expr)) {
        return chain_comparison_to_c_string(ctx, expr);
    }

    if (expr->child_count == 1) {
        return codegen_primary_to_c_string(ctx, expr->children[0]);
    }

    if (expr->child_count == 2) {
        operator_node = codegen_expect_child(expr, 0, NODE_OPERATOR);
        rhs = codegen_primary_to_c_string(ctx, expr->children[1]);
        result = codegen_dup_printf("(%s%s)", map_operator(operator_node->value), rhs);
        free(rhs);
        return result;
    }

    if (expr->child_count != 3) {
        codegen_error("malformed expression node");
    }

    operator_node = codegen_expect_child(expr, 1, NODE_OPERATOR);
    if (strcmp(operator_node->value, "in") == 0) {
        ValueType rhs_type = semantic_type_of(ctx->semantic, expr->children[2]);
        int needs_cleanup = 0;
        char *lhs_text = codegen_primary_to_c_string(ctx, expr->children[0]);
        char *rhs_name = materialize_ref_node(ctx, expr->children[2], rhs_type, 1, &needs_cleanup);
        char *call_text = codegen_dict_binary_call(rhs_type, "contains", rhs_name, lhs_text);

        free(lhs_text);
        if (needs_cleanup) {
            char *result_name = codegen_next_temp_name(ctx);

            codegen_emit_indent(ctx);
            fprintf(ctx->out, "bool %s = %s;\n", result_name, call_text);
            codegen_emit_ref_decref(ctx, rhs_type, rhs_name);
            free(rhs_name);
            free(call_text);
            return result_name;
        }

        free(rhs_name);
        return call_text;
    }

    if (strcmp(operator_node->value, "+") == 0 &&
        semantic_type_of(ctx->semantic, expr->children[0]) == TYPE_STR &&
        semantic_type_of(ctx->semantic, expr->children[2]) == TYPE_STR) {
        lhs = codegen_primary_to_c_string(ctx, expr->children[0]);
        rhs = codegen_primary_to_c_string(ctx, expr->children[2]);
        result = codegen_dup_printf("py4_str_concat(%s, %s)", lhs, rhs);
        free(lhs);
        free(rhs);
        return result;
    }

    lhs = codegen_primary_to_c_string(ctx, expr->children[0]);
    rhs = codegen_primary_to_c_string(ctx, expr->children[2]);
    result = codegen_dup_printf("(%s %s %s)", lhs, map_operator(operator_node->value), rhs);
    free(lhs);
    free(rhs);
    return result;
}

char *codegen_wrapped_expression_to_c_string(CodegenContext *ctx, const ParseNode *expr, ValueType target_type)
{
    ValueType expr_type = semantic_type_of(ctx->semantic, expr);
    char helper_name[MAX_NAME_LEN];
    char *inner;
    char *result;

    if (semantic_type_is_optional(target_type)) {
        ValueType base_type = semantic_optional_base_type(target_type);
        char optional_name[MAX_NAME_LEN];

        codegen_build_optional_base_name(optional_name, sizeof(optional_name), target_type);
        if (expr_type == target_type) {
            return codegen_expression_to_c_string(ctx, expr);
        }
        if (expr_type == TYPE_NONE) {
            return codegen_dup_printf("((%s){ .is_none = true })", optional_name);
        }
        if (expr_type == base_type) {
            inner = codegen_wrapped_expression_to_c_string(ctx, expr, base_type);
            result = codegen_dup_printf("((%s){ .is_none = false, .value = %s })", optional_name, inner);
            free(inner);
            return result;
        }
    }

    if (semantic_type_is_union(target_type)) {
        if (semantic_type_is_union(expr_type)) {
            if (expr_type == target_type) {
                return codegen_expression_to_c_string(ctx, expr);
            }

            codegen_build_union_convert_name(helper_name, sizeof(helper_name), expr_type, target_type);
            inner = codegen_expression_to_c_string(ctx, expr);
            result = codegen_dup_printf("%s(%s)", helper_name, inner);
            free(inner);
            return result;
        }

        {
            char ctor_name[MAX_NAME_LEN];

            codegen_build_union_ctor_name(ctor_name, sizeof(ctor_name),
                target_type,
                codegen_resolve_union_member_type(target_type, expr_type));
            if (expr_type == TYPE_NONE) {
                return codegen_dup_printf("%s()", ctor_name);
            }

            inner = codegen_expression_to_c_string(ctx, expr);
            result = codegen_dup_printf("%s(%s)", ctor_name, inner);
            free(inner);
            return result;
        }
    }

    return codegen_expression_to_c_string(ctx, expr);
}

void codegen_emit_wrapped_expression(CodegenContext *ctx, const ParseNode *expr, ValueType target_type)
{
    char *text = codegen_wrapped_expression_to_c_string(ctx, expr, target_type);

    fputs(text, ctx->out);
    free(text);
}

void codegen_emit_expression(CodegenContext *ctx, const ParseNode *expr)
{
    char *text = codegen_expression_to_c_string(ctx, expr);

    fputs(text, ctx->out);
    free(text);
}
