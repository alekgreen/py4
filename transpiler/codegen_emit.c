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
    return strcmp(name, "list_int") == 0 ||
        strcmp(name, "list_append") == 0 ||
        strcmp(name, "list_get") == 0 ||
        strcmp(name, "list_len") == 0 ||
        strcmp(name, "list_set") == 0;
}

static char *type_to_c_string(ValueType type)
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
        }
    }

    codegen_build_union_base_name(buffer, sizeof(buffer), type);
    return dup_printf("%s", buffer);
}

static char *next_temp_name(CodegenContext *ctx)
{
    return dup_printf("py4_tmp_%d", ctx->temp_counter++);
}

static void push_cleanup_scope(CodegenContext *ctx)
{
    if (ctx->cleanup_scope_count >= MAX_SCOPE_DEPTH) {
        codegen_error("cleanup scope nesting too deep");
    }
    ctx->cleanup_scope_starts[ctx->cleanup_scope_count++] = ctx->ref_local_count;
}

static void emit_ref_incref(CodegenContext *ctx, ValueType type, const char *name)
{
    switch (type) {
        case TYPE_LIST_INT:
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "py4_list_int_incref(%s);\n", name);
            return;
        default:
            codegen_error("unsupported refcounted type %s", semantic_type_name(type));
    }
}

static void emit_ref_decref(CodegenContext *ctx, ValueType type, const char *name)
{
    switch (type) {
        case TYPE_LIST_INT:
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "py4_list_int_decref(%s);\n", name);
            return;
        default:
            codegen_error("unsupported refcounted type %s", semantic_type_name(type));
    }
}

static void register_ref_local(CodegenContext *ctx, const char *name, ValueType type)
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

static void emit_live_ref_cleanup(CodegenContext *ctx)
{
    for (size_t i = ctx->ref_local_count; i > 0; i--) {
        emit_ref_decref(ctx, ctx->ref_locals[i - 1].type, ctx->ref_locals[i - 1].name);
    }
}

static void pop_cleanup_scope(CodegenContext *ctx)
{
    size_t start;

    if (ctx->cleanup_scope_count == 0) {
        codegen_error("cleanup scope stack underflow");
    }

    start = ctx->cleanup_scope_starts[--ctx->cleanup_scope_count];
    for (size_t i = ctx->ref_local_count; i > start; i--) {
        emit_ref_decref(ctx, ctx->ref_locals[i - 1].type, ctx->ref_locals[i - 1].name);
    }
    ctx->ref_local_count = start;
}

static int expression_is_owned_ref(CodegenContext *ctx, const ParseNode *expr)
{
    const ParseNode *child;

    if (!semantic_type_is_ref(semantic_type_of(ctx->semantic, expr))) {
        return 0;
    }
    if (expr->kind != NODE_EXPRESSION || expr->child_count != 1) {
        return 0;
    }

    child = expr->children[0];
    if (child->kind != NODE_CALL) {
        return 0;
    }

    return 1;
}

static void emit_union_constructor_call(CodegenContext *ctx, ValueType union_type, ValueType stored_type)
{
    char ctor_name[MAX_NAME_LEN];

    codegen_build_union_ctor_name(ctor_name, sizeof(ctor_name), union_type, stored_type);
    fputs(ctor_name, ctx->out);
}

static char *expression_to_c_string(CodegenContext *ctx, const ParseNode *expr);
static char *wrapped_expression_to_c_string(CodegenContext *ctx, const ParseNode *expr, ValueType target_type);

static char *arguments_to_c_string(CodegenContext *ctx, const ParseNode *call, const ParseNode *arguments)
{
    const ParseNode *callee = codegen_expect_child(call, 0, NODE_PRIMARY);
    const ParseNode *function_def;
    const ParseNode *parameters;
    char *result;

    if (arguments->child_count == 1 && codegen_is_epsilon_node(arguments->children[0])) {
        return dup_printf("");
    }

    if (is_list_builtin_name(callee->value)) {
        result = dup_printf("");
        for (size_t i = 0; i < arguments->child_count; i++) {
            char *arg = expression_to_c_string(ctx, arguments->children[i]);
            char *joined = i == 0 ? dup_printf("%s", arg) : dup_printf("%s, %s", result, arg);

            free(result);
            free(arg);
            result = joined;
        }
        return result;
    }

    function_def = codegen_find_function_definition(ctx->root, callee->value);
    if (function_def == NULL) {
        codegen_error("unknown function '%s' during code generation", callee->value);
    }

    parameters = codegen_function_parameters(function_def);
    result = dup_printf("");
    for (size_t i = 0; i < arguments->child_count; i++) {
        const ParseNode *parameter = codegen_expect_child(parameters, i, NODE_PARAMETER);
        const ParseNode *type_node = codegen_expect_child(parameter, 0, NODE_TYPE);
        ValueType target_type = semantic_type_of(ctx->semantic, type_node);
        char *arg = wrapped_expression_to_c_string(ctx, arguments->children[i], target_type);
        char *joined = i == 0 ? dup_printf("%s", arg) : dup_printf("%s, %s", result, arg);

        free(result);
        free(arg);
        result = joined;
    }

    return result;
}

static char *call_to_c_string(CodegenContext *ctx, const ParseNode *call)
{
    const ParseNode *callee = codegen_expect_child(call, 0, NODE_PRIMARY);
    const ParseNode *arguments = codegen_expect_child(call, 1, NODE_ARGUMENTS);
    char *args;
    char *result;

    if (is_list_builtin_name(callee->value)) {
        if (strcmp(callee->value, "list_int") == 0) {
            return dup_printf("py4_list_int_new()");
        }

        args = arguments_to_c_string(ctx, call, arguments);
        if (strcmp(callee->value, "list_append") == 0) {
            result = dup_printf("py4_list_int_append(%s)", args);
        } else if (strcmp(callee->value, "list_get") == 0) {
            result = dup_printf("py4_list_int_get(%s)", args);
        } else if (strcmp(callee->value, "list_len") == 0) {
            result = dup_printf("py4_list_int_len(%s)", args);
        } else if (strcmp(callee->value, "list_set") == 0) {
            result = dup_printf("py4_list_int_set(%s)", args);
        } else {
            free(args);
            codegen_error("unknown builtin '%s' during code generation", callee->value);
        }
        free(args);
        return result;
    }

    args = arguments_to_c_string(ctx, call, arguments);
    result = dup_printf("%s(%s)", callee->value, args);
    free(args);
    return result;
}

static char *primary_to_c_string(CodegenContext *ctx, const ParseNode *primary)
{
    if (primary->kind == NODE_CALL) {
        return call_to_c_string(ctx, primary);
    }

    if (primary->kind == NODE_INDEX) {
        char *base = primary_to_c_string(ctx, primary->children[0]);
        char *index = expression_to_c_string(ctx, primary->children[1]);
        char *result = dup_printf("py4_list_int_get(%s, %s)", base, index);

        free(base);
        free(index);
        return result;
    }

    if (primary->kind == NODE_EXPRESSION) {
        return expression_to_c_string(ctx, primary);
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
        char *operand_code = primary_to_c_string(ctx, operand);
        char *temp_name = next_temp_name(ctx);
        char *type_name = type_to_c_string(operand_type);

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

static char *expression_to_c_string(CodegenContext *ctx, const ParseNode *expr)
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
        return primary_to_c_string(ctx, expr->children[0]);
    }

    if (expr->child_count == 2) {
        operator_node = codegen_expect_child(expr, 0, NODE_OPERATOR);
        rhs = primary_to_c_string(ctx, expr->children[1]);
        result = dup_printf("(%s%s)", map_operator(operator_node->value), rhs);
        free(rhs);
        return result;
    }

    if (expr->child_count != 3) {
        codegen_error("malformed expression node");
    }

    lhs = primary_to_c_string(ctx, expr->children[0]);
    operator_node = codegen_expect_child(expr, 1, NODE_OPERATOR);
    rhs = primary_to_c_string(ctx, expr->children[2]);
    result = dup_printf("(%s %s %s)", lhs, map_operator(operator_node->value), rhs);
    free(lhs);
    free(rhs);
    return result;
}

static char *wrapped_expression_to_c_string(CodegenContext *ctx, const ParseNode *expr, ValueType target_type)
{
    ValueType expr_type = semantic_type_of(ctx->semantic, expr);
    char helper_name[MAX_NAME_LEN];
    char *inner;
    char *result;

    if (semantic_type_is_union(target_type)) {
        if (semantic_type_is_union(expr_type)) {
            if (expr_type == target_type) {
                return expression_to_c_string(ctx, expr);
            }

            codegen_build_union_convert_name(helper_name, sizeof(helper_name), expr_type, target_type);
            inner = expression_to_c_string(ctx, expr);
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

            inner = expression_to_c_string(ctx, expr);
            result = dup_printf("%s(%s)", ctor_name, inner);
            free(inner);
            return result;
        }
    }

    return expression_to_c_string(ctx, expr);
}

void codegen_emit_wrapped_expression(CodegenContext *ctx, const ParseNode *expr, ValueType target_type)
{
    char *text = wrapped_expression_to_c_string(ctx, expr, target_type);

    fputs(text, ctx->out);
    free(text);
}

void codegen_emit_expression(CodegenContext *ctx, const ParseNode *expr)
{
    char *text = expression_to_c_string(ctx, expr);

    fputs(text, ctx->out);
    free(text);
}

static void emit_print_statement(CodegenContext *ctx, const ParseNode *expr)
{
    const ParseNode *call = expr->children[0];
    const ParseNode *arguments = codegen_expect_child(call, 1, NODE_ARGUMENTS);
    ValueType arg_type;
    char helper_name[MAX_NAME_LEN];
    char *arg_text;

    if (arguments->child_count == 1 && codegen_is_epsilon_node(arguments->children[0])) {
        codegen_emit_indent(ctx);
        fputs("printf(\"\\n\");\n", ctx->out);
        return;
    }

    if (arguments->child_count != 1) {
        codegen_error("print currently supports exactly one argument");
    }

    arg_type = semantic_type_of(ctx->semantic, arguments->children[0]);
    arg_text = expression_to_c_string(ctx, arguments->children[0]);

    codegen_emit_indent(ctx);
    if (semantic_type_is_union(arg_type)) {
        codegen_build_union_print_name(helper_name, sizeof(helper_name), arg_type);
        fprintf(ctx->out, "%s(%s);\n", helper_name, arg_text);
        free(arg_text);
        return;
    }

    switch (arg_type) {
        case TYPE_INT:
            fprintf(ctx->out, "printf(\"%%d\\n\", %s);\n", arg_text);
            break;
        case TYPE_FLOAT:
            fprintf(ctx->out, "printf(\"%%g\\n\", (double)(%s));\n", arg_text);
            break;
        case TYPE_BOOL:
            fprintf(ctx->out, "printf(\"%%s\\n\", (%s) ? \"True\" : \"False\");\n", arg_text);
            break;
        case TYPE_CHAR:
            fprintf(ctx->out, "printf(\"%%c\\n\", %s);\n", arg_text);
            break;
        case TYPE_STR:
            fprintf(ctx->out, "printf(\"%%s\\n\", %s);\n", arg_text);
            break;
        case TYPE_NONE:
            free(arg_text);
            codegen_error("cannot print None");
            return;
        case TYPE_LIST_INT:
            free(arg_text);
            codegen_error("print does not support %s yet", semantic_type_name(arg_type));
            return;
    }

    free(arg_text);
}

static void emit_expression_statement(CodegenContext *ctx, const ParseNode *expr_stmt)
{
    const ParseNode *expr = codegen_expect_child(expr_stmt, 0, NODE_EXPRESSION);
    ValueType expr_type = semantic_type_of(ctx->semantic, expr);
    char *expr_text;

    if (codegen_is_print_call_expr(expr)) {
        emit_print_statement(ctx, expr);
        return;
    }

    expr_text = expression_to_c_string(ctx, expr);
    if (semantic_type_is_ref(expr_type) && expression_is_owned_ref(ctx, expr)) {
        char *temp_name = next_temp_name(ctx);
        char *type_name = type_to_c_string(expr_type);

        codegen_emit_indent(ctx);
        fprintf(ctx->out, "%s %s = %s;\n", type_name, temp_name, expr_text);
        free(type_name);
        free(expr_text);
        emit_ref_decref(ctx, expr_type, temp_name);
        free(temp_name);
        return;
    }

    codegen_emit_indent(ctx);
    fprintf(ctx->out, "%s;\n", expr_text);
    free(expr_text);
}

static void emit_return_statement(CodegenContext *ctx, const ParseNode *return_stmt)
{
    if (return_stmt->child_count != 1) {
        codegen_error("malformed return statement");
    }

    if (codegen_is_epsilon_node(return_stmt->children[0])) {
        emit_live_ref_cleanup(ctx);
        codegen_emit_indent(ctx);
        if (ctx->current_function_is_main) {
            fputs("return 0;\n", ctx->out);
        } else if (semantic_type_is_union(ctx->current_function_return_type)) {
            fputs("return ", ctx->out);
            emit_union_constructor_call(ctx, ctx->current_function_return_type, TYPE_NONE);
            fputs("();\n", ctx->out);
        } else {
            fputs("return;\n", ctx->out);
        }
        return;
    }

    {
        const ParseNode *expr = return_stmt->children[0];
        ValueType return_type = ctx->current_function_return_type;
        char *expr_text;

        if (semantic_type_is_ref(return_type)) {
            char *temp_name = next_temp_name(ctx);
            char *type_name = type_to_c_string(return_type);

            expr_text = wrapped_expression_to_c_string(ctx, expr, return_type);
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s %s = %s;\n", type_name, temp_name, expr_text);
            if (!expression_is_owned_ref(ctx, expr)) {
                emit_ref_incref(ctx, return_type, temp_name);
            }
            free(type_name);
            free(expr_text);
            emit_live_ref_cleanup(ctx);
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "return %s;\n", temp_name);
            free(temp_name);
            return;
        }

        expr_text = wrapped_expression_to_c_string(ctx, expr, return_type);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return %s;\n", expr_text);
        free(expr_text);
    }
}

static void emit_local_simple_statement(CodegenContext *ctx, const ParseNode *simple_stmt)
{
    const ParseNode *first_child = simple_stmt->children[0];

    if (first_child->kind == NODE_RETURN_STATEMENT) {
        emit_return_statement(ctx, first_child);
        return;
    }

    if (first_child->kind == NODE_EXPRESSION_STATEMENT) {
        emit_expression_statement(ctx, first_child);
        return;
    }

    if (simple_stmt->child_count != 2) {
        codegen_error("malformed simple statement");
    }

    {
        const ParseNode *target = codegen_simple_statement_target(simple_stmt);
        const ParseNode *statement_tail = codegen_simple_statement_tail(simple_stmt);
        const ParseNode *expr = codegen_statement_tail_expression(statement_tail);
        ValueType target_type;
        char *expr_text;

        if (target->kind == NODE_INDEX) {
            char *base = primary_to_c_string(ctx, target->children[0]);
            char *index = expression_to_c_string(ctx, target->children[1]);
            char *value = expression_to_c_string(ctx, expr);

            codegen_emit_indent(ctx);
            fprintf(ctx->out, "py4_list_int_set(%s, %s, %s);\n", base, index, value);
            free(base);
            free(index);
            free(value);
            return;
        }

        if (codegen_is_type_assignment(statement_tail)) {
            target_type = semantic_type_of(ctx->semantic, codegen_statement_tail_type_node(statement_tail));
        } else {
            target_type = semantic_type_of(ctx->semantic, target);
        }

        if (semantic_type_is_ref(target_type)) {
            char *temp_name = next_temp_name(ctx);
            char *type_name = type_to_c_string(target_type);
            int is_owned = expression_is_owned_ref(ctx, expr);

            expr_text = wrapped_expression_to_c_string(ctx, expr, target_type);
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s %s = %s;\n", type_name, temp_name, expr_text);
            if (!is_owned) {
                emit_ref_incref(ctx, target_type, temp_name);
            }

            if (codegen_is_type_assignment(statement_tail)) {
                codegen_emit_indent(ctx);
                fprintf(ctx->out, "%s %s = %s;\n", type_name, target->value, temp_name);
                register_ref_local(ctx, target->value, target_type);
            } else {
                emit_ref_decref(ctx, target_type, target->value);
                codegen_emit_indent(ctx);
                fprintf(ctx->out, "%s = %s;\n", target->value, temp_name);
            }

            free(type_name);
            free(temp_name);
            free(expr_text);
            return;
        }

        expr_text = wrapped_expression_to_c_string(ctx, expr, target_type);
        codegen_emit_indent(ctx);
        if (codegen_is_type_assignment(statement_tail)) {
            codegen_emit_type_name(ctx, target_type);
            fprintf(ctx->out, " %s = %s;\n", target->value, expr_text);
        } else {
            fprintf(ctx->out, "%s = %s;\n", target->value, expr_text);
        }
        free(expr_text);
    }
}

static void emit_if_statement(CodegenContext *ctx, const ParseNode *if_stmt)
{
    char *cond = expression_to_c_string(ctx, if_stmt->children[0]);

    codegen_emit_indent(ctx);
    fprintf(ctx->out, "if (%s)\n", cond);
    free(cond);
    codegen_emit_indent(ctx);
    fputs("{\n", ctx->out);
    ctx->indent_level++;
    push_cleanup_scope(ctx);
    codegen_emit_suite(ctx, if_stmt->children[2]);
    pop_cleanup_scope(ctx);
    ctx->indent_level--;
    codegen_emit_indent(ctx);
    fputs("}", ctx->out);

    for (size_t i = 3; i < if_stmt->child_count; i++) {
        const ParseNode *branch = if_stmt->children[i];

        if (branch->kind == NODE_ELIF_CLAUSE) {
            char *elif_cond = expression_to_c_string(ctx, branch->children[0]);

            fprintf(ctx->out, " else if (%s)\n", elif_cond);
            free(elif_cond);
            codegen_emit_indent(ctx);
            fputs("{\n", ctx->out);
            ctx->indent_level++;
            push_cleanup_scope(ctx);
            codegen_emit_suite(ctx, branch->children[2]);
            pop_cleanup_scope(ctx);
            ctx->indent_level--;
            codegen_emit_indent(ctx);
            fputs("}", ctx->out);
            continue;
        }

        if (branch->kind == NODE_ELSE_CLAUSE) {
            fputs(" else\n", ctx->out);
            codegen_emit_indent(ctx);
            fputs("{\n", ctx->out);
            ctx->indent_level++;
            push_cleanup_scope(ctx);
            codegen_emit_suite(ctx, branch->children[1]);
            pop_cleanup_scope(ctx);
            ctx->indent_level--;
            codegen_emit_indent(ctx);
            fputs("}", ctx->out);
            continue;
        }

        codegen_error("malformed if statement");
    }

    fputc('\n', ctx->out);
}

static void emit_while_statement(CodegenContext *ctx, const ParseNode *while_stmt)
{
    char *cond = expression_to_c_string(ctx, while_stmt->children[0]);

    codegen_emit_indent(ctx);
    fprintf(ctx->out, "while (%s)\n", cond);
    free(cond);
    codegen_emit_indent(ctx);
    fputs("{\n", ctx->out);
    ctx->indent_level++;
    push_cleanup_scope(ctx);
    codegen_emit_suite(ctx, while_stmt->children[2]);
    pop_cleanup_scope(ctx);
    ctx->indent_level--;
    codegen_emit_indent(ctx);
    fputs("}\n", ctx->out);
}

void codegen_emit_suite(CodegenContext *ctx, const ParseNode *suite)
{
    if (suite->child_count == 1 && codegen_is_epsilon_node(suite->children[0])) {
        codegen_emit_indent(ctx);
        fputs("/* empty */\n", ctx->out);
        return;
    }

    for (size_t i = 0; i < suite->child_count; i++) {
        codegen_emit_statement(ctx, suite->children[i], 0);
    }
}

static void emit_parameter_list(CodegenContext *ctx, const ParseNode *parameters, int is_c_main)
{
    if (is_c_main) {
        fputs("void", ctx->out);
        return;
    }

    if (parameters->child_count == 1 && codegen_is_epsilon_node(parameters->children[0])) {
        fputs("void", ctx->out);
        return;
    }

    for (size_t i = 0; i < parameters->child_count; i++) {
        const ParseNode *param = parameters->children[i];
        const ParseNode *type_node = codegen_expect_child(param, 0, NODE_TYPE);

        if (i > 0) {
            fputs(", ", ctx->out);
        }
        codegen_emit_type_name(ctx, semantic_type_of(ctx->semantic, type_node));
        fprintf(ctx->out, " %s", param->value);
    }
}

static void emit_function_signature(CodegenContext *ctx, const ParseNode *function_def, int prototype_only)
{
    const ParseNode *name = codegen_expect_child(function_def, 0, NODE_PRIMARY);
    const ParseNode *parameters = codegen_function_parameters(function_def);
    int is_c_main = codegen_is_main_function(function_def);
    ValueType return_type = codegen_function_return_type(ctx, function_def);

    if (is_c_main) {
        fputs("int main(", ctx->out);
    } else {
        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", name->value);
    }
    emit_parameter_list(ctx, parameters, is_c_main);
    fputc(')', ctx->out);
    if (prototype_only) {
        fputs(";\n", ctx->out);
    }
}

static void emit_function_definition(CodegenContext *ctx, const ParseNode *function_def)
{
    int is_c_main = codegen_is_main_function(function_def);
    ValueType return_type = codegen_function_return_type(ctx, function_def);
    int prev_is_main = ctx->current_function_is_main;
    ValueType prev_return_type = ctx->current_function_return_type;

    emit_function_signature(ctx, function_def, 0);
    fputs("\n{\n", ctx->out);
    ctx->indent_level++;
    ctx->current_function_is_main = is_c_main;
    ctx->current_function_return_type = return_type;
    push_cleanup_scope(ctx);

    if (is_c_main && ctx->has_top_level_executable_statements) {
        codegen_emit_indent(ctx);
        fputs("py4_module_init();\n", ctx->out);
    }

    codegen_emit_suite(ctx, codegen_function_suite(function_def));
    pop_cleanup_scope(ctx);

    if (is_c_main) {
        codegen_emit_indent(ctx);
        fputs("return 0;\n", ctx->out);
    }

    ctx->current_function_is_main = prev_is_main;
    ctx->current_function_return_type = prev_return_type;
    ctx->indent_level--;
    fputs("}\n", ctx->out);
}

void codegen_emit_statement(CodegenContext *ctx, const ParseNode *statement, int allow_function_defs)
{
    const ParseNode *payload = codegen_statement_payload(statement);

    if (payload->kind == NODE_FUNCTION_DEF) {
        if (!allow_function_defs) {
            codegen_error("nested function definitions are not supported in C output");
        }
        emit_function_definition(ctx, payload);
        return;
    }

    if (payload->kind == NODE_WHILE_STATEMENT) {
        emit_while_statement(ctx, payload);
        return;
    }

    if (payload->kind == NODE_IMPORT_STATEMENT) {
        codegen_error("imports should be resolved before C code generation");
    }

    if (payload->kind == NODE_IF_STATEMENT) {
        emit_if_statement(ctx, payload);
        return;
    }

    if (payload->kind != NODE_SIMPLE_STATEMENT) {
        codegen_error("unsupported statement node");
    }

    emit_local_simple_statement(ctx, payload);
}

static void emit_global_declaration(CodegenContext *ctx, const ParseNode *simple_stmt)
{
    const ParseNode *statement_tail = codegen_simple_statement_tail(simple_stmt);
    ValueType type;

    if (!codegen_is_type_assignment(statement_tail)) {
        return;
    }

    const ParseNode *name = codegen_simple_statement_target(simple_stmt);
    type = semantic_type_of(ctx->semantic, codegen_statement_tail_type_node(statement_tail));
    codegen_emit_type_name(ctx, type);
    fprintf(ctx->out, " %s;\n", name->value);
}

static void emit_global_declarations(CodegenContext *ctx, const ParseNode *root)
{
    int wrote_any = 0;

    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = codegen_statement_payload(root->children[i]);

        if (payload->kind == NODE_IMPORT_STATEMENT) {
            codegen_error("imports should be resolved before C code generation");
        } else if (payload->kind == NODE_SIMPLE_STATEMENT) {
            if (payload->child_count == 2 &&
                payload->children[0]->kind == NODE_PRIMARY &&
                codegen_is_type_assignment(payload->children[1])) {
                emit_global_declaration(ctx, payload);
                wrote_any = 1;
            }
        }
    }

    if (wrote_any) {
        fputc('\n', ctx->out);
    }
}

static void emit_function_prototypes(CodegenContext *ctx, const ParseNode *root)
{
    int wrote_any = 0;

    if (ctx->has_top_level_executable_statements) {
        fputs("static void py4_module_init(void);\n", ctx->out);
        wrote_any = 1;
    }

    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = codegen_statement_payload(root->children[i]);

        if (payload->kind == NODE_IMPORT_STATEMENT) {
            codegen_error("imports should be resolved before C code generation");
        } else if (payload->kind == NODE_FUNCTION_DEF) {
            emit_function_signature(ctx, payload, 1);
            wrote_any = 1;
        }
    }

    if (!ctx->has_user_main && ctx->has_top_level_executable_statements) {
        fputs("int main(void);\n", ctx->out);
        wrote_any = 1;
    }

    if (wrote_any) {
        fputc('\n', ctx->out);
    }
}

static void emit_module_init(CodegenContext *ctx, const ParseNode *root)
{
    if (!ctx->has_top_level_executable_statements) {
        return;
    }

    fputs("static void py4_module_init(void)\n{\n", ctx->out);
    ctx->indent_level++;
    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = codegen_statement_payload(root->children[i]);

        if (payload->kind == NODE_IMPORT_STATEMENT) {
            codegen_error("imports should be resolved before C code generation");
        } else if (payload->kind == NODE_SIMPLE_STATEMENT) {
            const ParseNode *name;
            const ParseNode *statement_tail;
            const ParseNode *expr;
            ValueType target_type;
            char *expr_text;

            if (payload->children[0]->kind == NODE_EXPRESSION_STATEMENT) {
                emit_expression_statement(ctx, payload->children[0]);
                continue;
            }

            if (payload->children[0]->kind == NODE_RETURN_STATEMENT) {
                codegen_error("return is not valid at module scope");
            }

            name = codegen_simple_statement_target(payload);
            statement_tail = codegen_simple_statement_tail(payload);
            expr = codegen_statement_tail_expression(statement_tail);
            if (name->kind == NODE_INDEX) {
                char *base = primary_to_c_string(ctx, name->children[0]);
                char *index = expression_to_c_string(ctx, name->children[1]);
                char *value = expression_to_c_string(ctx, expr);

                codegen_emit_indent(ctx);
                fprintf(ctx->out, "py4_list_int_set(%s, %s, %s);\n", base, index, value);
                free(base);
                free(index);
                free(value);
                continue;
            }

            target_type = semantic_type_of(ctx->semantic, name);
            expr_text = wrapped_expression_to_c_string(ctx, expr, target_type);

            if (semantic_type_is_ref(target_type)) {
                char *temp_name = next_temp_name(ctx);
                char *type_name = type_to_c_string(target_type);
                int is_owned = expression_is_owned_ref(ctx, expr);

                codegen_emit_indent(ctx);
                fprintf(ctx->out, "%s %s = %s;\n", type_name, temp_name, expr_text);
                if (!is_owned) {
                    emit_ref_incref(ctx, target_type, temp_name);
                }
                if (!codegen_is_type_assignment(statement_tail)) {
                    emit_ref_decref(ctx, target_type, name->value);
                }
                codegen_emit_indent(ctx);
                fprintf(ctx->out, "%s = %s;\n", name->value, temp_name);
                free(type_name);
                free(temp_name);
                free(expr_text);
                continue;
            }

            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s = %s;\n", name->value, expr_text);
            free(expr_text);
            continue;
        }

        if (payload->kind == NODE_IF_STATEMENT) {
            emit_if_statement(ctx, payload);
        }
    }
    ctx->indent_level--;
    fputs("}\n\n", ctx->out);
}

static void emit_top_level_functions(CodegenContext *ctx, const ParseNode *root)
{
    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = codegen_statement_payload(root->children[i]);

        if (payload->kind == NODE_IMPORT_STATEMENT) {
            codegen_error("imports should be resolved before C code generation");
        } else if (payload->kind == NODE_FUNCTION_DEF) {
            emit_function_definition(ctx, payload);
            fputc('\n', ctx->out);
        }
    }
}

static void emit_auto_main(CodegenContext *ctx)
{
    if (ctx->has_user_main) {
        return;
    }

    fputs("int main(void)\n{\n", ctx->out);
    ctx->indent_level++;
    if (ctx->has_top_level_executable_statements) {
        codegen_emit_indent(ctx);
        fputs("py4_module_init();\n", ctx->out);
    }
    codegen_emit_indent(ctx);
    fputs("return 0;\n", ctx->out);
    ctx->indent_level--;
    fputs("}\n", ctx->out);
}

static void emit_list_int_runtime(CodegenContext *ctx)
{
    fputs("typedef struct {\n", ctx->out);
    fputs("    int refcount;\n", ctx->out);
    fputs("    size_t len;\n", ctx->out);
    fputs("    size_t cap;\n", ctx->out);
    fputs("    int *items;\n", ctx->out);
    fputs("} Py4ListInt;\n\n", ctx->out);

    fputs("static void py4_list_int_bounds_check(Py4ListInt *list, int index)\n{\n", ctx->out);
    fputs("    if (list == NULL) {\n", ctx->out);
    fputs("        fprintf(stderr, \"Runtime error: list[int] is null\\n\");\n", ctx->out);
    fputs("        exit(1);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    if (index < 0 || (size_t)index >= list->len) {\n", ctx->out);
    fputs("        fprintf(stderr, \"Runtime error: list[int] index out of bounds\\n\");\n", ctx->out);
    fputs("        exit(1);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static Py4ListInt *py4_list_int_new(void)\n{\n", ctx->out);
    fputs("    Py4ListInt *list = calloc(1, sizeof(Py4ListInt));\n", ctx->out);
    fputs("    if (list == NULL) {\n", ctx->out);
    fputs("        perror(\"calloc\");\n", ctx->out);
    fputs("        exit(1);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    list->refcount = 1;\n", ctx->out);
    fputs("    return list;\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static void py4_list_int_incref(Py4ListInt *list)\n{\n", ctx->out);
    fputs("    if (list != NULL) {\n", ctx->out);
    fputs("        list->refcount++;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static void py4_list_int_decref(Py4ListInt *list)\n{\n", ctx->out);
    fputs("    if (list == NULL) {\n", ctx->out);
    fputs("        return;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    list->refcount--;\n", ctx->out);
    fputs("    if (list->refcount == 0) {\n", ctx->out);
    fputs("        free(list->items);\n", ctx->out);
    fputs("        free(list);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static void py4_list_int_ensure_capacity(Py4ListInt *list, size_t needed)\n{\n", ctx->out);
    fputs("    size_t new_cap;\n", ctx->out);
    fputs("    int *items;\n\n", ctx->out);
    fputs("    if (list == NULL) {\n", ctx->out);
    fputs("        fprintf(stderr, \"Runtime error: list[int] is null\\n\");\n", ctx->out);
    fputs("        exit(1);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    if (list->cap >= needed) {\n", ctx->out);
    fputs("        return;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    new_cap = list->cap == 0 ? 4 : list->cap * 2;\n", ctx->out);
    fputs("    while (new_cap < needed) {\n", ctx->out);
    fputs("        new_cap *= 2;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    items = realloc(list->items, sizeof(int) * new_cap);\n", ctx->out);
    fputs("    if (items == NULL) {\n", ctx->out);
    fputs("        perror(\"realloc\");\n", ctx->out);
    fputs("        exit(1);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    list->items = items;\n", ctx->out);
    fputs("    list->cap = new_cap;\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static void py4_list_int_append(Py4ListInt *list, int value)\n{\n", ctx->out);
    fputs("    py4_list_int_ensure_capacity(list, list->len + 1);\n", ctx->out);
    fputs("    list->items[list->len++] = value;\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static int py4_list_int_get(Py4ListInt *list, int index)\n{\n", ctx->out);
    fputs("    py4_list_int_bounds_check(list, index);\n", ctx->out);
    fputs("    return list->items[index];\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static void py4_list_int_set(Py4ListInt *list, int index, int value)\n{\n", ctx->out);
    fputs("    py4_list_int_bounds_check(list, index);\n", ctx->out);
    fputs("    list->items[index] = value;\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static int py4_list_int_len(Py4ListInt *list)\n{\n", ctx->out);
    fputs("    if (list == NULL) {\n", ctx->out);
    fputs("        fprintf(stderr, \"Runtime error: list[int] is null\\n\");\n", ctx->out);
    fputs("        exit(1);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    return (int)list->len;\n", ctx->out);
    fputs("}\n\n", ctx->out);
}

void emit_c_program(FILE *out, const ParseNode *root, const SemanticInfo *info)
{
    CodegenContext ctx = {0};

    if (root == NULL || root->kind != NODE_S) {
        codegen_error("expected program root");
    }

    ctx.out = out;
    ctx.root = root;
    ctx.semantic = info;
    codegen_collect_program_state(&ctx, root);

    fputs("#include <stdbool.h>\n#include <stdio.h>\n#include <stdlib.h>\n\n", out);
    emit_list_int_runtime(&ctx);
    codegen_emit_union_runtime(&ctx);
    emit_global_declarations(&ctx, root);
    emit_function_prototypes(&ctx, root);
    emit_module_init(&ctx, root);
    emit_top_level_functions(&ctx, root);
    emit_auto_main(&ctx);
}
