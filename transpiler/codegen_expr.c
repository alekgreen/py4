#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen_internal.h"

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
        codegen_error("failed to format generated C");
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
        strcmp(name, "list_int") == 0 ||
        strcmp(name, "list_float") == 0 ||
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

static const char *list_runtime_prefix(ValueType list_type)
{
    switch (list_type) {
        case TYPE_LIST_INT:
            return "py4_list_int";
        case TYPE_LIST_FLOAT:
            return "py4_list_float";
        default:
            codegen_error("%s is not a supported list type", semantic_type_name(list_type));
            return "";
    }
}

char *codegen_type_to_c_string(ValueType type)
{
    char buffer[MAX_NAME_LEN];

    if (!semantic_type_is_union(type)) {
        switch (type) {
            case TYPE_INT: return dup_printf("int");
            case TYPE_FLOAT: return dup_printf("double");
            case TYPE_BOOL: return dup_printf("bool");
            case TYPE_CHAR: return dup_printf("char");
            case TYPE_STR: return dup_printf("const char *");
            case TYPE_NONE: return dup_printf("void");
            case TYPE_LIST_INT: return dup_printf("Py4ListInt *");
            case TYPE_LIST_FLOAT: return dup_printf("Py4ListFloat *");
        }
    }

    codegen_build_union_base_name(buffer, sizeof(buffer), type);
    return dup_printf("%s", buffer);
}

char *codegen_next_temp_name(CodegenContext *ctx)
{
    return dup_printf("py4_tmp_%d", ctx->temp_counter++);
}

void codegen_push_cleanup_scope(CodegenContext *ctx)
{
    if (ctx->cleanup_scope_count >= MAX_SCOPE_DEPTH) {
        codegen_error("cleanup scope nesting too deep");
    }
    ctx->cleanup_scope_starts[ctx->cleanup_scope_count++] = ctx->ref_local_count;
}

void codegen_emit_ref_incref(CodegenContext *ctx, ValueType type, const char *name)
{
    switch (type) {
        case TYPE_LIST_INT:
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "py4_list_int_incref(%s);\n", name);
            return;
        case TYPE_LIST_FLOAT:
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "py4_list_float_incref(%s);\n", name);
            return;
        default:
            codegen_error("unsupported refcounted type %s", semantic_type_name(type));
    }
}

void codegen_emit_ref_decref(CodegenContext *ctx, ValueType type, const char *name)
{
    switch (type) {
        case TYPE_LIST_INT:
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "py4_list_int_decref(%s);\n", name);
            return;
        case TYPE_LIST_FLOAT:
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "py4_list_float_decref(%s);\n", name);
            return;
        default:
            codegen_error("unsupported refcounted type %s", semantic_type_name(type));
    }
}

void codegen_register_ref_local(CodegenContext *ctx, const char *name, ValueType type)
{
    if (!semantic_type_is_ref(type)) {
        return;
    }

    if (ctx->ref_local_count >= MAX_REF_LOCALS) {
        codegen_error("too many live refcounted locals");
    }

    ctx->ref_locals[ctx->ref_local_count].name = name;
    ctx->ref_locals[ctx->ref_local_count].type = type;
    ctx->ref_local_count++;
}

void codegen_emit_live_ref_cleanup(CodegenContext *ctx)
{
    for (size_t i = ctx->ref_local_count; i > 0; i--) {
        codegen_emit_ref_decref(ctx, ctx->ref_locals[i - 1].type, ctx->ref_locals[i - 1].name);
    }
}

void codegen_pop_cleanup_scope(CodegenContext *ctx)
{
    size_t start;

    if (ctx->cleanup_scope_count == 0) {
        codegen_error("cleanup scope stack underflow");
    }

    start = ctx->cleanup_scope_starts[--ctx->cleanup_scope_count];
    for (size_t i = ctx->ref_local_count; i > start; i--) {
        codegen_emit_ref_decref(ctx, ctx->ref_locals[i - 1].type, ctx->ref_locals[i - 1].name);
    }
    ctx->ref_local_count = start;
}

int codegen_expression_is_owned_ref(CodegenContext *ctx, const ParseNode *expr)
{
    const ParseNode *child;

    if (!semantic_type_is_ref(semantic_type_of(ctx->semantic, expr))) {
        return 0;
    }
    if (expr->kind != NODE_EXPRESSION || expr->child_count != 1) {
        return 0;
    }

    child = expr->children[0];
    return child->kind == NODE_CALL || child->kind == NODE_LIST_LITERAL || child->kind == NODE_METHOD_CALL;
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

static int node_is_owned_ref_value(const ParseNode *node)
{
    if (node == NULL) {
        return 0;
    }
    if (node->kind == NODE_CALL || node->kind == NODE_LIST_LITERAL || node->kind == NODE_METHOD_CALL) {
        return 1;
    }
    if (node->kind == NODE_EXPRESSION && node->child_count == 1) {
        return node_is_owned_ref_value(node->children[0]);
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
    if (!node_is_owned_ref_value(node)) {
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
    const ParseNode *function_def = NULL;
    const ParseNode *parameters = NULL;
    ValueType return_type = semantic_type_of(ctx->semantic, call);
    char *arg_parts[32];
    ValueType cleanup_types[32];
    char *cleanup_names[32];
    size_t arg_count = 0;
    size_t cleanup_count = 0;
    char *args;
    char *result;

    if (is_list_builtin_name(callee->value)) {
        if (strcmp(callee->value, "list_int") == 0) {
            return dup_printf("py4_list_int_new()");
        }
        if (strcmp(callee->value, "list_float") == 0) {
            return dup_printf("py4_list_float_new()");
        }
    } else {
        function_def = codegen_find_function_definition(ctx->root, callee->value);
        if (function_def == NULL) {
            codegen_error("unknown function '%s' during code generation", callee->value);
        }
        parameters = codegen_function_parameters(function_def);
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
                 strcmp(callee->value, "list_set") == 0) && i == 0) {
                target_type = arg_type;
            } else if ((strcmp(callee->value, "list_append") == 0 && i == 1) ||
                (strcmp(callee->value, "list_set") == 0 && i == 2)) {
                target_type = semantic_list_element_type(semantic_type_of(ctx->semantic, arguments->children[0]));
            }
        } else {
            const ParseNode *parameter = codegen_expect_child(parameters, i, NODE_PARAMETER);
            const ParseNode *type_node = codegen_expect_child(parameter, 0, NODE_TYPE);
            target_type = semantic_type_of(ctx->semantic, type_node);
        }

        if (semantic_type_is_ref(target_type) && semantic_type_is_ref(arg_type)) {
            int needs_cleanup = 0;
            char *part = materialize_ref_node(ctx, arg_node, target_type, 0, &needs_cleanup);

            arg_parts[i] = part;
            if (needs_cleanup) {
                cleanup_names[cleanup_count] = dup_printf("%s", part);
                cleanup_types[cleanup_count] = target_type;
                cleanup_count++;
            }
            continue;
        }

        arg_parts[i] = is_list_builtin_name(callee->value)
            ? codegen_expression_to_c_string(ctx, arg_node)
            : codegen_wrapped_expression_to_c_string(ctx, arg_node, target_type);
    }

    args = dup_printf("");
    for (size_t i = 0; i < arg_count; i++) {
        char *joined = i == 0 ? dup_printf("%s", arg_parts[i]) : dup_printf("%s, %s", args, arg_parts[i]);
        free(args);
        args = joined;
    }

    if (strcmp(callee->value, "list_append") == 0) {
        const char *prefix = list_runtime_prefix(semantic_type_of(ctx->semantic, arguments->children[0]));
        result = dup_printf("%s_append(%s)", prefix, args);
    } else if (strcmp(callee->value, "list_get") == 0) {
        const char *prefix = list_runtime_prefix(semantic_type_of(ctx->semantic, arguments->children[0]));
        result = dup_printf("%s_get(%s)", prefix, args);
    } else if (strcmp(callee->value, "list_len") == 0 || strcmp(callee->value, "len") == 0) {
        const char *prefix = list_runtime_prefix(semantic_type_of(ctx->semantic, arguments->children[0]));
        result = dup_printf("%s_len(%s)", prefix, args);
    } else if (strcmp(callee->value, "list_set") == 0) {
        const char *prefix = list_runtime_prefix(semantic_type_of(ctx->semantic, arguments->children[0]));
        result = dup_printf("%s_set(%s)", prefix, args);
    } else {
        result = dup_printf("%s(%s)", callee->value, args);
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
        return dup_printf("(void)0");
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
    ValueType receiver_type;
    int needs_cleanup = 0;
    char *receiver_name;
    char *result;

    if (call == NULL || call->child_count != 3) {
        codegen_error("malformed method call");
    }

    receiver = call->children[0];
    receiver_type = semantic_type_of(ctx->semantic, receiver);
    receiver_name = materialize_ref_node(ctx, receiver, receiver_type, 1, &needs_cleanup);

    if (!is_list_method_name(method->value)) {
        free(receiver_name);
        codegen_error("unknown method '%s' during code generation", method->value);
    }

    if (strcmp(method->value, "append") == 0) {
        const char *prefix = list_runtime_prefix(receiver_type);
        char *value = codegen_wrapped_expression_to_c_string(
            ctx,
            arguments->children[0],
            semantic_list_element_type(receiver_type));
        result = dup_printf("%s_append(%s, %s)", prefix, receiver_name, value);
        free(value);
    } else if (strcmp(method->value, "pop") == 0) {
        const char *prefix = list_runtime_prefix(receiver_type);
        result = dup_printf("%s_pop(%s)", prefix, receiver_name);
    } else if (strcmp(method->value, "clear") == 0) {
        const char *prefix = list_runtime_prefix(receiver_type);
        result = dup_printf("%s_clear(%s)", prefix, receiver_name);
    } else if (strcmp(method->value, "copy") == 0) {
        const char *prefix = list_runtime_prefix(receiver_type);
        result = dup_printf("%s_copy(%s)", prefix, receiver_name);
    } else {
        free(receiver_name);
        codegen_error("unknown method '%s' during code generation", method->value);
    }

    if (needs_cleanup) {
        ValueType return_type = semantic_type_of(ctx->semantic, call);

        if (return_type == TYPE_NONE) {
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s;\n", result);
            free(result);
            codegen_emit_ref_decref(ctx, receiver_type, receiver_name);
            free(receiver_name);
            return dup_printf("(void)0");
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
    if (primary->kind == NODE_CALL) {
        return call_to_c_string(ctx, primary);
    }

    if (primary->kind == NODE_METHOD_CALL) {
        return method_call_to_c_string(ctx, primary);
    }

    if (primary->kind == NODE_LIST_LITERAL) {
        ValueType list_type = semantic_type_of(ctx->semantic, primary);
        ValueType element_type = semantic_list_element_type(list_type);
        const char *prefix = list_runtime_prefix(list_type);

        if (primary->child_count == 0) {
            return dup_printf("%s_new()", prefix);
        }

        {
            char *values = dup_printf("");
            char *result;

            for (size_t i = 0; i < primary->child_count; i++) {
                char *item = codegen_wrapped_expression_to_c_string(ctx, primary->children[i], element_type);
                char *joined = i == 0 ? dup_printf("%s", item) : dup_printf("%s, %s", values, item);

                free(values);
                free(item);
                values = joined;
            }

            result = dup_printf("%s_from_values(%zu, (%s[]){%s})",
                prefix,
                primary->child_count,
                element_type == TYPE_FLOAT ? "double" : "int",
                values);
            free(values);
            return result;
        }
    }

    if (primary->kind == NODE_INDEX) {
        ValueType base_type = semantic_type_of(ctx->semantic, primary->children[0]);
        ValueType element_type = semantic_list_element_type(base_type);
        const char *prefix = list_runtime_prefix(base_type);
        int needs_cleanup = 0;
        char *base = materialize_ref_node(ctx, primary->children[0], base_type, 1, &needs_cleanup);
        char *index = codegen_expression_to_c_string(ctx, primary->children[1]);
        char *call_text = dup_printf("%s_get(%s, %s)", prefix, base, index);
        char *result;

        if (needs_cleanup) {
            char *result_name = codegen_next_temp_name(ctx);
            char *type_name = codegen_type_to_c_string(element_type);

            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s %s = %s;\n", type_name, result_name, call_text);
            codegen_emit_ref_decref(ctx, base_type, base);
            free(type_name);
            result = result_name;
        } else {
            result = dup_printf("%s", call_text);
        }

        free(base);
        free(index);
        free(call_text);
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
            return dup_printf("true");
        }
        if (strcmp(primary->value, "False") == 0) {
            return dup_printf("false");
        }
    }

    return dup_printf("%s", primary->value);
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

    result = dup_printf("");
    for (size_t i = 1; i < expr->child_count; i += 2) {
        const ParseNode *op = codegen_expect_child(expr, i, NODE_OPERATOR);
        char *piece = dup_printf("(%s %s %s)",
            temp_names[(i - 1) / 2],
            map_operator(op->value),
            temp_names[(i + 1) / 2]);
        char *joined = i == 1 ? dup_printf("%s", piece) : dup_printf("%s && %s", result, piece);

        free(result);
        free(piece);
        result = joined;
    }

    {
        char *wrapped = dup_printf("(%s)", result);
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
        result = dup_printf("(%s%s)", map_operator(operator_node->value), rhs);
        free(rhs);
        return result;
    }

    if (expr->child_count != 3) {
        codegen_error("malformed expression node");
    }

    lhs = codegen_primary_to_c_string(ctx, expr->children[0]);
    operator_node = codegen_expect_child(expr, 1, NODE_OPERATOR);
    rhs = codegen_primary_to_c_string(ctx, expr->children[2]);
    result = dup_printf("(%s %s %s)", lhs, map_operator(operator_node->value), rhs);
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

    if (semantic_type_is_union(target_type)) {
        if (semantic_type_is_union(expr_type)) {
            if (expr_type == target_type) {
                return codegen_expression_to_c_string(ctx, expr);
            }

            codegen_build_union_convert_name(helper_name, sizeof(helper_name), expr_type, target_type);
            inner = codegen_expression_to_c_string(ctx, expr);
            result = dup_printf("%s(%s)", helper_name, inner);
            free(inner);
            return result;
        }

        {
            char ctor_name[MAX_NAME_LEN];

            codegen_build_union_ctor_name(ctor_name, sizeof(ctor_name),
                target_type,
                codegen_resolve_union_member_type(target_type, expr_type));
            if (expr_type == TYPE_NONE) {
                return dup_printf("%s()", ctor_name);
            }

            inner = codegen_expression_to_c_string(ctx, expr);
            result = dup_printf("%s(%s)", ctor_name, inner);
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
