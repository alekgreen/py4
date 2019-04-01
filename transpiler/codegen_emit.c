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
        }
    }

    codegen_build_union_base_name(buffer, sizeof(buffer), type);
    return dup_printf("%s", buffer);
}

static char *next_temp_name(CodegenContext *ctx)
{
    return dup_printf("py4_tmp_%d", ctx->temp_counter++);
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
    const ParseNode *function_def = codegen_find_function_definition(ctx->root, callee->value);
    const ParseNode *parameters;
    char *result;

    if (arguments->child_count == 1 && codegen_is_epsilon_node(arguments->children[0])) {
        return dup_printf("");
    }

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
    char *args = arguments_to_c_string(ctx, call, arguments);
    char *result = dup_printf("%s(%s)", callee->value, args);

    free(args);
    return result;
}

static char *primary_to_c_string(CodegenContext *ctx, const ParseNode *primary)
{
    if (primary->kind == NODE_CALL) {
        return call_to_c_string(ctx, primary);
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
    }

    free(arg_text);
}

static void emit_expression_statement(CodegenContext *ctx, const ParseNode *expr_stmt)
{
    const ParseNode *expr = codegen_expect_child(expr_stmt, 0, NODE_EXPRESSION);
    char *expr_text;

    if (codegen_is_print_call_expr(expr)) {
        emit_print_statement(ctx, expr);
        return;
    }

    expr_text = expression_to_c_string(ctx, expr);
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
        char *expr_text = wrapped_expression_to_c_string(ctx, return_stmt->children[0], ctx->current_function_return_type);
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
        const ParseNode *name = codegen_simple_statement_name(simple_stmt);
        const ParseNode *statement_tail = codegen_simple_statement_tail(simple_stmt);
        const ParseNode *expr = codegen_statement_tail_expression(statement_tail);
        ValueType target_type;
        char *expr_text;

        if (codegen_is_type_assignment(statement_tail)) {
            target_type = semantic_type_of(ctx->semantic, codegen_statement_tail_type_node(statement_tail));
        } else {
            target_type = semantic_type_of(ctx->semantic, name);
        }

        expr_text = wrapped_expression_to_c_string(ctx, expr, target_type);
        codegen_emit_indent(ctx);
        if (codegen_is_type_assignment(statement_tail)) {
            codegen_emit_type_name(ctx, target_type);
            fprintf(ctx->out, " %s = %s;\n", name->value, expr_text);
        } else {
            fprintf(ctx->out, "%s = %s;\n", name->value, expr_text);
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
    codegen_emit_suite(ctx, if_stmt->children[2]);
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
            codegen_emit_suite(ctx, branch->children[2]);
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
            codegen_emit_suite(ctx, branch->children[1]);
            ctx->indent_level--;
            codegen_emit_indent(ctx);
            fputs("}", ctx->out);
            continue;
        }

        codegen_error("malformed if statement");
    }

    fputc('\n', ctx->out);
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

    if (is_c_main && ctx->has_top_level_executable_statements) {
        codegen_emit_indent(ctx);
        fputs("py4_module_init();\n", ctx->out);
    }

    codegen_emit_suite(ctx, codegen_function_suite(function_def));

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

    const ParseNode *name = codegen_simple_statement_name(simple_stmt);
    type = semantic_type_of(ctx->semantic, codegen_statement_tail_type_node(statement_tail));
    codegen_emit_type_name(ctx, type);
    fprintf(ctx->out, " %s;\n", name->value);
}

static void emit_global_declarations(CodegenContext *ctx, const ParseNode *root)
{
    int wrote_any = 0;

    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = codegen_statement_payload(root->children[i]);

        if (payload->kind == NODE_SIMPLE_STATEMENT) {
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

        if (payload->kind == NODE_FUNCTION_DEF) {
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

        if (payload->kind == NODE_SIMPLE_STATEMENT) {
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

            name = codegen_simple_statement_name(payload);
            statement_tail = codegen_simple_statement_tail(payload);
            expr = codegen_statement_tail_expression(statement_tail);
            target_type = semantic_type_of(ctx->semantic, name);
            expr_text = wrapped_expression_to_c_string(ctx, expr, target_type);

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

        if (payload->kind == NODE_FUNCTION_DEF) {
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

    fputs("#include <stdbool.h>\n#include <stdio.h>\n\n", out);
    codegen_emit_union_runtime(&ctx);
    emit_global_declarations(&ctx, root);
    emit_function_prototypes(&ctx, root);
    emit_module_init(&ctx, root);
    emit_top_level_functions(&ctx, root);
    emit_auto_main(&ctx);
}
