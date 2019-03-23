#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen.h"

typedef struct {
    FILE *out;
    int indent_level;
    int has_user_main;
    int has_top_level_simple_statements;
} CodegenContext;

static void codegen_error(const char *message, ...)
{
    va_list args;

    fprintf(stderr, "Codegen error: ");
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(1);
}

static void emit_indent(CodegenContext *ctx)
{
    for (int i = 0; i < ctx->indent_level; i++) {
        fputs("    ", ctx->out);
    }
}

static const ParseNode *expect_child(const ParseNode *node, size_t index, NodeKind kind)
{
    if (node == NULL || index >= node->child_count) {
        codegen_error("malformed AST");
    }
    if (node->children[index]->kind != kind) {
        codegen_error("unexpected AST node kind");
    }
    return node->children[index];
}

static int is_type_assignment(const ParseNode *statement_tail)
{
    return statement_tail->child_count == 4 &&
        statement_tail->children[0]->kind == NODE_COLON;
}

static int is_epsilon_node(const ParseNode *node)
{
    return node->kind == NODE_EPSILON;
}

static int is_print_call_expr(const ParseNode *expr)
{
    const ParseNode *call;
    const ParseNode *callee;

    if (expr->kind != NODE_EXPRESSION || expr->child_count != 1) {
        return 0;
    }

    call = expr->children[0];
    if (call->kind != NODE_CALL || call->child_count == 0) {
        return 0;
    }

    callee = call->children[0];
    return callee->kind == NODE_PRIMARY &&
        callee->value != NULL &&
        strcmp(callee->value, "print") == 0;
}

static const char *map_type_name(const ParseNode *type_node)
{
    if (type_node == NULL || type_node->value == NULL) {
        return "void";
    }
    if (strcmp(type_node->value, "int") == 0) {
        return "int";
    }
    if (strcmp(type_node->value, "None") == 0) {
        return "void";
    }
    return type_node->value;
}

static const ParseNode *statement_payload(const ParseNode *statement)
{
    if (statement->kind != NODE_STATEMENT || statement->child_count != 1) {
        codegen_error("malformed statement node");
    }
    return statement->children[0];
}

static const ParseNode *simple_statement_name(const ParseNode *simple_stmt)
{
    return expect_child(simple_stmt, 0, NODE_PRIMARY);
}

static const ParseNode *simple_statement_tail(const ParseNode *simple_stmt)
{
    return expect_child(simple_stmt, 1, NODE_STATEMENT_TAIL);
}

static const ParseNode *statement_tail_expression(const ParseNode *statement_tail)
{
    if (is_type_assignment(statement_tail)) {
        return expect_child(statement_tail, 3, NODE_EXPRESSION);
    }
    return expect_child(statement_tail, 1, NODE_EXPRESSION);
}

static void emit_expression(CodegenContext *ctx, const ParseNode *expr);

static void emit_arguments(CodegenContext *ctx, const ParseNode *arguments)
{
    int first = 1;

    if (arguments->child_count == 1 && is_epsilon_node(arguments->children[0])) {
        return;
    }

    for (size_t i = 0; i < arguments->child_count; i++) {
        if (!first) {
            fputs(", ", ctx->out);
        }
        emit_expression(ctx, arguments->children[i]);
        first = 0;
    }
}

static void emit_call(CodegenContext *ctx, const ParseNode *call)
{
    const ParseNode *callee = expect_child(call, 0, NODE_PRIMARY);
    const ParseNode *arguments = expect_child(call, 1, NODE_ARGUMENTS);

    fputs(callee->value, ctx->out);
    fputc('(', ctx->out);
    emit_arguments(ctx, arguments);
    fputc(')', ctx->out);
}

static void emit_primary(CodegenContext *ctx, const ParseNode *primary)
{
    if (primary->kind == NODE_CALL) {
        emit_call(ctx, primary);
        return;
    }

    if (primary->kind == NODE_EXPRESSION) {
        emit_expression(ctx, primary);
        return;
    }

    if (primary->kind != NODE_PRIMARY || primary->value == NULL) {
        codegen_error("unsupported primary node");
    }

    fputs(primary->value, ctx->out);
}

static void emit_expression(CodegenContext *ctx, const ParseNode *expr)
{
    if (expr->kind != NODE_EXPRESSION || expr->child_count == 0) {
        codegen_error("malformed expression node");
    }

    if (expr->child_count == 1) {
        emit_primary(ctx, expr->children[0]);
        return;
    }

    fputc('(', ctx->out);
    for (size_t i = 0; i < expr->child_count; i++) {
        const ParseNode *child = expr->children[i];

        if (child->kind == NODE_OPERATOR) {
            fprintf(ctx->out, " %s ", child->value);
        } else {
            emit_primary(ctx, child);
        }
    }
    fputc(')', ctx->out);
}

static void emit_print_statement(CodegenContext *ctx, const ParseNode *expr)
{
    const ParseNode *call = expr->children[0];
    const ParseNode *arguments = expect_child(call, 1, NODE_ARGUMENTS);

    emit_indent(ctx);
    if (arguments->child_count == 1 && is_epsilon_node(arguments->children[0])) {
        fputs("printf(\"\\n\");\n", ctx->out);
        return;
    }

    if (arguments->child_count != 1) {
        codegen_error("print currently supports exactly one argument");
    }

    fputs("printf(\"%d\\n\", ", ctx->out);
    emit_expression(ctx, arguments->children[0]);
    fputs(");\n", ctx->out);
}

static void emit_expression_statement(CodegenContext *ctx, const ParseNode *expr_stmt)
{
    const ParseNode *expr = expect_child(expr_stmt, 0, NODE_EXPRESSION);

    if (is_print_call_expr(expr)) {
        emit_print_statement(ctx, expr);
        return;
    }

    emit_indent(ctx);
    emit_expression(ctx, expr);
    fputs(";\n", ctx->out);
}

static void emit_local_simple_statement(CodegenContext *ctx, const ParseNode *simple_stmt)
{
    const ParseNode *first_child = simple_stmt->children[0];

    if (first_child->kind == NODE_EXPRESSION_STATEMENT) {
        emit_expression_statement(ctx, first_child);
        return;
    }

    if (simple_stmt->child_count != 2) {
        codegen_error("malformed simple statement");
    }

    const ParseNode *name = simple_statement_name(simple_stmt);
    const ParseNode *statement_tail = simple_statement_tail(simple_stmt);
    const ParseNode *expr = statement_tail_expression(statement_tail);

    emit_indent(ctx);
    if (is_type_assignment(statement_tail)) {
        const ParseNode *type_node = expect_child(statement_tail, 1, NODE_PRIMARY);

        fprintf(ctx->out, "%s %s = ", map_type_name(type_node), name->value);
    } else {
        fprintf(ctx->out, "%s = ", name->value);
    }
    emit_expression(ctx, expr);
    fputs(";\n", ctx->out);
}

static void emit_statement(CodegenContext *ctx, const ParseNode *statement, int allow_function_defs);

static void emit_suite(CodegenContext *ctx, const ParseNode *suite)
{
    if (suite->child_count == 1 && is_epsilon_node(suite->children[0])) {
        emit_indent(ctx);
        fputs("/* empty */\n", ctx->out);
        return;
    }

    for (size_t i = 0; i < suite->child_count; i++) {
        emit_statement(ctx, suite->children[i], 0);
    }
}

static const char *function_return_type(const ParseNode *function_def)
{
    if (function_def->child_count >= 3 && function_def->children[2]->kind == NODE_RETURN_TYPE) {
        const ParseNode *type_node = expect_child(function_def->children[2], 0, NODE_PRIMARY);
        return map_type_name(type_node);
    }
    return "void";
}

static int is_main_function(const ParseNode *function_def)
{
    const ParseNode *name = expect_child(function_def, 0, NODE_PRIMARY);

    return strcmp(name->value, "main") == 0;
}

static const ParseNode *function_parameters(const ParseNode *function_def)
{
    return expect_child(function_def, 1, NODE_PARAMETERS);
}

static const ParseNode *function_suite(const ParseNode *function_def)
{
    return function_def->children[function_def->child_count - 1];
}

static void emit_parameter_list(CodegenContext *ctx, const ParseNode *parameters, int is_c_main)
{
    if (is_c_main) {
        fputs("void", ctx->out);
        return;
    }

    if (parameters->child_count == 1 && is_epsilon_node(parameters->children[0])) {
        fputs("void", ctx->out);
        return;
    }

    for (size_t i = 0; i < parameters->child_count; i++) {
        const ParseNode *param = parameters->children[i];

        if (i > 0) {
            fputs(", ", ctx->out);
        }
        fprintf(ctx->out, "int %s", param->value);
    }
}

static void emit_function_signature(CodegenContext *ctx, const ParseNode *function_def, int prototype_only)
{
    const ParseNode *name = expect_child(function_def, 0, NODE_PRIMARY);
    const ParseNode *parameters = function_parameters(function_def);
    int is_c_main = is_main_function(function_def);
    const char *return_type = function_return_type(function_def);

    if (is_c_main) {
        fputs("int main(", ctx->out);
    } else {
        fprintf(ctx->out, "%s %s(", return_type, name->value);
    }
    emit_parameter_list(ctx, parameters, is_c_main);
    fputc(')', ctx->out);
    if (prototype_only) {
        fputs(";\n", ctx->out);
    }
}

static void emit_function_definition(CodegenContext *ctx, const ParseNode *function_def)
{
    int is_c_main = is_main_function(function_def);
    const char *return_type = function_return_type(function_def);

    emit_function_signature(ctx, function_def, 0);
    fputs("\n{\n", ctx->out);
    ctx->indent_level++;

    if (is_c_main && ctx->has_top_level_simple_statements) {
        emit_indent(ctx);
        fputs("py4_module_init();\n", ctx->out);
    }

    emit_suite(ctx, function_suite(function_def));

    if (is_c_main || strcmp(return_type, "int") == 0) {
        emit_indent(ctx);
        fputs("return 0;\n", ctx->out);
    }

    ctx->indent_level--;
    fputs("}\n", ctx->out);
}

static void emit_statement(CodegenContext *ctx, const ParseNode *statement, int allow_function_defs)
{
    const ParseNode *payload = statement_payload(statement);

    if (payload->kind == NODE_FUNCTION_DEF) {
        if (!allow_function_defs) {
            codegen_error("nested function definitions are not supported in C output");
        }
        emit_function_definition(ctx, payload);
        return;
    }

    if (payload->kind != NODE_SIMPLE_STATEMENT) {
        codegen_error("unsupported statement node");
    }

    emit_local_simple_statement(ctx, payload);
}

static void collect_program_state(CodegenContext *ctx, const ParseNode *root)
{
    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = statement_payload(root->children[i]);

        if (payload->kind == NODE_FUNCTION_DEF) {
            if (is_main_function(payload)) {
                ctx->has_user_main = 1;
            }
        } else if (payload->kind == NODE_SIMPLE_STATEMENT) {
            ctx->has_top_level_simple_statements = 1;
        }
    }
}

static void emit_global_declaration(CodegenContext *ctx, const ParseNode *simple_stmt)
{
    const ParseNode *statement_tail = simple_statement_tail(simple_stmt);

    if (!is_type_assignment(statement_tail)) {
        return;
    }

    const ParseNode *name = simple_statement_name(simple_stmt);
    const ParseNode *type_node = expect_child(statement_tail, 1, NODE_PRIMARY);

    fprintf(ctx->out, "%s %s;\n", map_type_name(type_node), name->value);
}

static void emit_global_declarations(CodegenContext *ctx, const ParseNode *root)
{
    int wrote_any = 0;

    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = statement_payload(root->children[i]);

        if (payload->kind == NODE_SIMPLE_STATEMENT) {
            if (payload->child_count == 2 &&
                payload->children[0]->kind == NODE_PRIMARY &&
                is_type_assignment(payload->children[1])) {
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

    if (ctx->has_top_level_simple_statements) {
        fputs("static void py4_module_init(void);\n", ctx->out);
        wrote_any = 1;
    }

    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = statement_payload(root->children[i]);

        if (payload->kind == NODE_FUNCTION_DEF) {
            emit_function_signature(ctx, payload, 1);
            wrote_any = 1;
        }
    }

    if (!ctx->has_user_main && ctx->has_top_level_simple_statements) {
        fputs("int main(void);\n", ctx->out);
        wrote_any = 1;
    }

    if (wrote_any) {
        fputc('\n', ctx->out);
    }
}

static void emit_module_init(CodegenContext *ctx, const ParseNode *root)
{
    if (!ctx->has_top_level_simple_statements) {
        return;
    }

    fputs("static void py4_module_init(void)\n{\n", ctx->out);
    ctx->indent_level++;
    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = statement_payload(root->children[i]);

        if (payload->kind == NODE_SIMPLE_STATEMENT) {
            const ParseNode *name;
            const ParseNode *statement_tail;
            const ParseNode *expr;

            if (payload->children[0]->kind == NODE_EXPRESSION_STATEMENT) {
                emit_expression_statement(ctx, payload->children[0]);
                continue;
            }

            name = simple_statement_name(payload);
            statement_tail = simple_statement_tail(payload);
            expr = statement_tail_expression(statement_tail);

            emit_indent(ctx);
            fprintf(ctx->out, "%s = ", name->value);
            emit_expression(ctx, expr);
            fputs(";\n", ctx->out);
        }
    }
    ctx->indent_level--;
    fputs("}\n\n", ctx->out);
}

static void emit_top_level_functions(CodegenContext *ctx, const ParseNode *root)
{
    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = statement_payload(root->children[i]);

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
    if (ctx->has_top_level_simple_statements) {
        emit_indent(ctx);
        fputs("py4_module_init();\n", ctx->out);
    }
    emit_indent(ctx);
    fputs("return 0;\n", ctx->out);
    ctx->indent_level--;
    fputs("}\n", ctx->out);
}

void emit_c_program(FILE *out, const ParseNode *root)
{
    CodegenContext ctx = {0};

    if (root == NULL || root->kind != NODE_S) {
        codegen_error("expected program root");
    }

    ctx.out = out;
    collect_program_state(&ctx, root);

    fputs("#include <stdio.h>\n\n", out);
    emit_global_declarations(&ctx, root);
    emit_function_prototypes(&ctx, root);
    emit_module_init(&ctx, root);
    emit_top_level_functions(&ctx, root);
    emit_auto_main(&ctx);
}
