#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen_internal.h"

static void emit_union_constructor_call(CodegenContext *ctx, ValueType union_type, ValueType stored_type)
{
    char ctor_name[MAX_NAME_LEN];

    codegen_build_union_ctor_name(ctor_name, sizeof(ctor_name), union_type, stored_type);
    fputs(ctor_name, ctx->out);
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
    arg_text = codegen_expression_to_c_string(ctx, arguments->children[0]);

    codegen_emit_indent(ctx);
        if (semantic_type_is_union(arg_type)) {
            codegen_build_union_print_name(helper_name, sizeof(helper_name), arg_type);
            fprintf(ctx->out, "%s(%s);\n", helper_name, arg_text);
            free(arg_text);
            return;
        }
        if (semantic_type_is_tuple(arg_type)) {
            free(arg_text);
            codegen_error("print does not support %s yet", semantic_type_name(arg_type));
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
        case TYPE_LIST_FLOAT:
        case TYPE_LIST_BOOL:
        case TYPE_LIST_CHAR:
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

    expr_text = codegen_expression_to_c_string(ctx, expr);
    if (semantic_type_is_ref(expr_type) && codegen_expression_is_owned_ref(ctx, expr)) {
        char *temp_name = codegen_next_temp_name(ctx);
        char *type_name = codegen_type_to_c_string(expr_type);

        codegen_emit_indent(ctx);
        fprintf(ctx->out, "%s %s = %s;\n", type_name, temp_name, expr_text);
        free(type_name);
        free(expr_text);
        codegen_emit_ref_decref(ctx, expr_type, temp_name);
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
        codegen_emit_live_ref_cleanup(ctx);
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
            char *temp_name = codegen_next_temp_name(ctx);
            char *type_name = codegen_type_to_c_string(return_type);

            expr_text = codegen_wrapped_expression_to_c_string(ctx, expr, return_type);
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s %s = %s;\n", type_name, temp_name, expr_text);
            if (!codegen_expression_is_owned_ref(ctx, expr)) {
                codegen_emit_ref_incref(ctx, return_type, temp_name);
            }
            free(type_name);
            free(expr_text);
            codegen_emit_live_ref_cleanup(ctx);
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "return %s;\n", temp_name);
            free(temp_name);
            return;
        }

        expr_text = codegen_wrapped_expression_to_c_string(ctx, expr, return_type);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return %s;\n", expr_text);
        free(expr_text);
    }
}

static void codegen_push_loop(CodegenContext *ctx, int loop_id)
{
    if (ctx->loop_count >= MAX_LOOP_DEPTH) {
        codegen_error("loop nesting too deep");
    }
    ctx->loop_ids[ctx->loop_count++] = loop_id;
}

static void codegen_pop_loop(CodegenContext *ctx)
{
    if (ctx->loop_count == 0) {
        codegen_error("loop stack underflow");
    }
    ctx->loop_count--;
}

static void emit_loop_jump(CodegenContext *ctx, const char *keyword, int action)
{
    int loop_id;

    if (ctx->loop_count == 0) {
        codegen_error("%s is not valid outside a loop", keyword);
    }

    loop_id = ctx->loop_ids[ctx->loop_count - 1];
    codegen_emit_indent(ctx);
    fprintf(ctx->out, "py4_loop_action_%d = %d;\n", loop_id, action);
    codegen_emit_indent(ctx);
    fprintf(ctx->out, "goto py4_loop_cleanup_%d;\n", loop_id);
}

static void emit_tuple_destructuring_assignment(
    CodegenContext *ctx,
    const ParseNode *target,
    const ParseNode *statement_tail,
    const ParseNode *expr)
{
    ValueType tuple_type = codegen_is_type_assignment(statement_tail)
        ? semantic_type_of(ctx->semantic, codegen_statement_tail_type_node(statement_tail))
        : semantic_type_of(ctx->semantic, target);
    char *tuple_temp = codegen_next_temp_name(ctx);
    char *tuple_type_name = codegen_type_to_c_string(tuple_type);
    char *expr_text = codegen_wrapped_expression_to_c_string(ctx, expr, tuple_type);

    codegen_emit_indent(ctx);
    fprintf(ctx->out, "%s %s = %s;\n", tuple_type_name, tuple_temp, expr_text);

    for (size_t i = 0; i < target->child_count; i++) {
        const ParseNode *name = codegen_expect_child(target, i, NODE_PRIMARY);
        ValueType element_type = codegen_is_type_assignment(statement_tail)
            ? semantic_tuple_element_type(tuple_type, i)
            : semantic_type_of(ctx->semantic, name);
        char *field_expr = codegen_dup_printf("%s.item%zu", tuple_temp, i);

        codegen_emit_indent(ctx);
        if (codegen_is_type_assignment(statement_tail)) {
            codegen_emit_type_name(ctx, element_type);
            fprintf(ctx->out, " %s = %s;\n", name->value, field_expr);
        } else {
            fprintf(ctx->out, "%s = %s;\n", name->value, field_expr);
        }
        free(field_expr);
    }

    free(tuple_temp);
    free(tuple_type_name);
    free(expr_text);
}

static void emit_local_simple_statement(CodegenContext *ctx, const ParseNode *simple_stmt)
{
    const ParseNode *first_child = simple_stmt->children[0];

    if (first_child->kind == NODE_RETURN_STATEMENT) {
        emit_return_statement(ctx, first_child);
        return;
    }

    if (first_child->kind == NODE_BREAK_STATEMENT) {
        emit_loop_jump(ctx, "break", 1);
        return;
    }

    if (first_child->kind == NODE_CONTINUE_STATEMENT) {
        emit_loop_jump(ctx, "continue", 2);
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
            ValueType list_type = semantic_type_of(ctx->semantic, target->children[0]);
            ValueType element_type = semantic_list_element_type(list_type);
            char *base = codegen_primary_to_c_string(ctx, target->children[0]);
            char *index = codegen_expression_to_c_string(ctx, target->children[1]);
            char *value = codegen_wrapped_expression_to_c_string(ctx, expr, element_type);
            char *call = codegen_list_ternary_call(list_type, "set", base, index, value);

            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s;\n", call);
            free(base);
            free(index);
            free(value);
            free(call);
            return;
        }

        if (target->kind == NODE_TUPLE_TARGET) {
            emit_tuple_destructuring_assignment(ctx, target, statement_tail, expr);
            return;
        }

        if (codegen_is_type_assignment(statement_tail)) {
            target_type = semantic_type_of(ctx->semantic, codegen_statement_tail_type_node(statement_tail));
        } else {
            target_type = semantic_type_of(ctx->semantic, target);
        }

        if (semantic_type_is_ref(target_type)) {
            char *temp_name = codegen_next_temp_name(ctx);
            char *type_name = codegen_type_to_c_string(target_type);
            int is_owned = codegen_expression_is_owned_ref(ctx, expr);

            expr_text = codegen_wrapped_expression_to_c_string(ctx, expr, target_type);
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "%s %s = %s;\n", type_name, temp_name, expr_text);
            if (!is_owned) {
                codegen_emit_ref_incref(ctx, target_type, temp_name);
            }

            if (codegen_is_type_assignment(statement_tail)) {
                codegen_emit_indent(ctx);
                fprintf(ctx->out, "%s %s = %s;\n", type_name, target->value, temp_name);
                codegen_register_ref_local(ctx, target->value, target_type);
            } else {
                codegen_emit_ref_decref(ctx, target_type, target->value);
                codegen_emit_indent(ctx);
                fprintf(ctx->out, "%s = %s;\n", target->value, temp_name);
            }

            free(type_name);
            free(temp_name);
            free(expr_text);
            return;
        }

        expr_text = codegen_wrapped_expression_to_c_string(ctx, expr, target_type);
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
    char *cond = codegen_expression_to_c_string(ctx, if_stmt->children[0]);

    codegen_emit_indent(ctx);
    fprintf(ctx->out, "if (%s)\n", cond);
    free(cond);
    codegen_emit_indent(ctx);
    fputs("{\n", ctx->out);
    ctx->indent_level++;
    codegen_push_cleanup_scope(ctx);
    codegen_emit_suite(ctx, if_stmt->children[2]);
    codegen_pop_cleanup_scope(ctx);
    ctx->indent_level--;
    codegen_emit_indent(ctx);
    fputs("}", ctx->out);

    for (size_t i = 3; i < if_stmt->child_count; i++) {
        const ParseNode *branch = if_stmt->children[i];

        if (branch->kind == NODE_ELIF_CLAUSE) {
            char *elif_cond = codegen_expression_to_c_string(ctx, branch->children[0]);

            fprintf(ctx->out, " else if (%s)\n", elif_cond);
            free(elif_cond);
            codegen_emit_indent(ctx);
            fputs("{\n", ctx->out);
            ctx->indent_level++;
            codegen_push_cleanup_scope(ctx);
            codegen_emit_suite(ctx, branch->children[2]);
            codegen_pop_cleanup_scope(ctx);
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
            codegen_push_cleanup_scope(ctx);
            codegen_emit_suite(ctx, branch->children[1]);
            codegen_pop_cleanup_scope(ctx);
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
    char *cond = codegen_expression_to_c_string(ctx, while_stmt->children[0]);
    int loop_id = ctx->temp_counter++;

    codegen_emit_indent(ctx);
    fprintf(ctx->out, "while (%s)\n", cond);
    free(cond);
    codegen_emit_indent(ctx);
    fputs("{\n", ctx->out);
    ctx->indent_level++;
    codegen_emit_indent(ctx);
    fprintf(ctx->out, "int py4_loop_action_%d = 0;\n", loop_id);
    codegen_push_cleanup_scope(ctx);
    codegen_push_loop(ctx, loop_id);
    codegen_emit_suite(ctx, while_stmt->children[2]);
    codegen_pop_loop(ctx);
    codegen_emit_indent(ctx);
    fprintf(ctx->out, "py4_loop_cleanup_%d:\n", loop_id);
    codegen_pop_cleanup_scope(ctx);
    codegen_emit_indent(ctx);
    fprintf(ctx->out, "if (py4_loop_action_%d == 1) {\n", loop_id);
    ctx->indent_level++;
    codegen_emit_indent(ctx);
    fputs("break;\n", ctx->out);
    ctx->indent_level--;
    codegen_emit_indent(ctx);
    fputs("}\n", ctx->out);
    codegen_emit_indent(ctx);
    fprintf(ctx->out, "if (py4_loop_action_%d == 2) {\n", loop_id);
    ctx->indent_level++;
    codegen_emit_indent(ctx);
    fputs("continue;\n", ctx->out);
    ctx->indent_level--;
    codegen_emit_indent(ctx);
    fputs("}\n", ctx->out);
    ctx->indent_level--;
    codegen_emit_indent(ctx);
    fputs("}\n", ctx->out);
}

static int argument_count(const ParseNode *arguments)
{
    if (arguments->child_count == 1 && codegen_is_epsilon_node(arguments->children[0])) {
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

    callee = codegen_expect_child(call, 0, NODE_PRIMARY);
    return callee->token_type == TOKEN_IDENTIFIER &&
        callee->value != NULL &&
        strcmp(callee->value, "range") == 0;
}

static void emit_range_for_statement(CodegenContext *ctx, const ParseNode *for_stmt)
{
    const ParseNode *target = codegen_expect_child(for_stmt, 0, NODE_PRIMARY);
    const ParseNode *iterable = codegen_expect_child(for_stmt, 1, NODE_EXPRESSION);
    const ParseNode *call = iterable->children[0];
    const ParseNode *arguments = codegen_expect_child(call, 1, NODE_ARGUMENTS);
    const ParseNode *suite = codegen_expect_child(for_stmt, 3, NODE_SUITE);
    int arg_count = argument_count(arguments);
    char *start_expr;
    char *stop_expr;
    char *step_expr;
    char *start_name;
    char *stop_name;
    char *step_name;
    char *index_name;
    int loop_id = ctx->temp_counter++;

    if (arg_count < 1 || arg_count > 3) {
        codegen_error("function 'range' expects 1 to 3 arguments");
    }

    codegen_emit_indent(ctx);
    fputs("{\n", ctx->out);
    ctx->indent_level++;

    start_expr = arg_count >= 2
        ? codegen_expression_to_c_string(ctx, arguments->children[0])
        : NULL;
    stop_expr = arg_count == 1
        ? codegen_expression_to_c_string(ctx, arguments->children[0])
        : codegen_expression_to_c_string(ctx, arguments->children[1]);
    step_expr = arg_count == 3
        ? codegen_expression_to_c_string(ctx, arguments->children[2])
        : NULL;

    start_name = codegen_next_temp_name(ctx);
    stop_name = codegen_next_temp_name(ctx);
    step_name = codegen_next_temp_name(ctx);
    index_name = codegen_next_temp_name(ctx);

    codegen_emit_indent(ctx);
    fprintf(ctx->out, "int %s = %s;\n", start_name, arg_count >= 2 ? start_expr : "0");
    codegen_emit_indent(ctx);
    fprintf(ctx->out, "int %s = %s;\n", stop_name, stop_expr);
    codegen_emit_indent(ctx);
    fprintf(ctx->out, "int %s = %s;\n", step_name, arg_count == 3 ? step_expr : "1");
    codegen_emit_indent(ctx);
    fprintf(ctx->out, "if (%s == 0) {\n", step_name);
    ctx->indent_level++;
    codegen_emit_indent(ctx);
    fputs("fprintf(stderr, \"Runtime error: range() step cannot be zero\\n\");\n", ctx->out);
    codegen_emit_indent(ctx);
    fputs("exit(1);\n", ctx->out);
    ctx->indent_level--;
    codegen_emit_indent(ctx);
    fputs("}\n", ctx->out);

    codegen_emit_indent(ctx);
    fprintf(ctx->out,
        "for (int %s = %s; (%s > 0) ? (%s < %s) : (%s > %s); %s += %s)\n",
        index_name, start_name, step_name, index_name, stop_name, index_name, stop_name, index_name, step_name);
    codegen_emit_indent(ctx);
    fputs("{\n", ctx->out);
    ctx->indent_level++;
    codegen_emit_indent(ctx);
    fprintf(ctx->out, "int py4_loop_action_%d = 0;\n", loop_id);
    codegen_emit_indent(ctx);
    fprintf(ctx->out, "int %s = %s;\n", target->value, index_name);
    codegen_push_cleanup_scope(ctx);
    codegen_push_loop(ctx, loop_id);
    codegen_emit_suite(ctx, suite);
    codegen_pop_loop(ctx);
    codegen_emit_indent(ctx);
    fprintf(ctx->out, "py4_loop_cleanup_%d:\n", loop_id);
    codegen_pop_cleanup_scope(ctx);
    codegen_emit_indent(ctx);
    fprintf(ctx->out, "if (py4_loop_action_%d == 1) {\n", loop_id);
    ctx->indent_level++;
    codegen_emit_indent(ctx);
    fputs("break;\n", ctx->out);
    ctx->indent_level--;
    codegen_emit_indent(ctx);
    fputs("}\n", ctx->out);
    codegen_emit_indent(ctx);
    fprintf(ctx->out, "if (py4_loop_action_%d == 2) {\n", loop_id);
    ctx->indent_level++;
    codegen_emit_indent(ctx);
    fputs("continue;\n", ctx->out);
    ctx->indent_level--;
    codegen_emit_indent(ctx);
    fputs("}\n", ctx->out);
    ctx->indent_level--;
    codegen_emit_indent(ctx);
    fputs("}\n", ctx->out);
    ctx->indent_level--;
    codegen_emit_indent(ctx);
    fputs("}\n", ctx->out);

    free(start_expr);
    free(stop_expr);
    free(step_expr);
    free(start_name);
    free(stop_name);
    free(step_name);
    free(index_name);
}

static void emit_for_statement(CodegenContext *ctx, const ParseNode *for_stmt)
{
    const ParseNode *target = codegen_expect_child(for_stmt, 0, NODE_PRIMARY);
    const ParseNode *iterable = codegen_expect_child(for_stmt, 1, NODE_EXPRESSION);
    const ParseNode *suite = codegen_expect_child(for_stmt, 3, NODE_SUITE);
    ValueType iterable_type;
    ValueType element_type;
    char *iterable_expr;
    char *iterable_name;
    char *iterable_type_name;
    char *index_name;
    int iterable_owned;
    int loop_id = ctx->temp_counter++;

    if (is_range_expression(iterable)) {
        emit_range_for_statement(ctx, for_stmt);
        return;
    }

    iterable_type = semantic_type_of(ctx->semantic, iterable);
    element_type = semantic_list_element_type(iterable_type);
    iterable_expr = codegen_wrapped_expression_to_c_string(ctx, iterable, iterable_type);
    iterable_name = codegen_next_temp_name(ctx);
    iterable_type_name = codegen_type_to_c_string(iterable_type);
    index_name = codegen_next_temp_name(ctx);
    iterable_owned = codegen_expression_is_owned_ref(ctx, iterable);

    codegen_emit_indent(ctx);
    fputs("{\n", ctx->out);
    ctx->indent_level++;

    codegen_emit_indent(ctx);
    fprintf(ctx->out, "%s %s = %s;\n", iterable_type_name, iterable_name, iterable_expr);
    if (!iterable_owned) {
        codegen_emit_ref_incref(ctx, iterable_type, iterable_name);
    }

    {
        char *len_call = codegen_list_unary_call(iterable_type, "len", iterable_name);

        codegen_emit_indent(ctx);
        fprintf(ctx->out,
            "for (int %s = 0; %s < %s; %s++)\n",
            index_name, index_name, len_call, index_name);
        free(len_call);
    }
    codegen_emit_indent(ctx);
    fputs("{\n", ctx->out);
    ctx->indent_level++;
    codegen_emit_indent(ctx);
    fprintf(ctx->out, "int py4_loop_action_%d = 0;\n", loop_id);
    {
        char *get_call = codegen_list_binary_call(iterable_type, "get", iterable_name, index_name);

        codegen_emit_indent(ctx);
        codegen_emit_type_name(ctx, element_type);
        fprintf(ctx->out, " %s = %s;\n", target->value, get_call);
        free(get_call);
    }
    codegen_push_cleanup_scope(ctx);
    codegen_push_loop(ctx, loop_id);
    codegen_emit_suite(ctx, suite);
    codegen_pop_loop(ctx);
    codegen_emit_indent(ctx);
    fprintf(ctx->out, "py4_loop_cleanup_%d:\n", loop_id);
    codegen_pop_cleanup_scope(ctx);
    codegen_emit_indent(ctx);
    fprintf(ctx->out, "if (py4_loop_action_%d == 1) {\n", loop_id);
    ctx->indent_level++;
    codegen_emit_indent(ctx);
    fputs("break;\n", ctx->out);
    ctx->indent_level--;
    codegen_emit_indent(ctx);
    fputs("}\n", ctx->out);
    codegen_emit_indent(ctx);
    fprintf(ctx->out, "if (py4_loop_action_%d == 2) {\n", loop_id);
    ctx->indent_level++;
    codegen_emit_indent(ctx);
    fputs("continue;\n", ctx->out);
    ctx->indent_level--;
    codegen_emit_indent(ctx);
    fputs("}\n", ctx->out);
    ctx->indent_level--;
    codegen_emit_indent(ctx);
    fputs("}\n", ctx->out);
    codegen_emit_ref_decref(ctx, iterable_type, iterable_name);
    ctx->indent_level--;
    codegen_emit_indent(ctx);
    fputs("}\n", ctx->out);

    free(iterable_expr);
    free(iterable_name);
    free(iterable_type_name);
    free(index_name);
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
    codegen_push_cleanup_scope(ctx);

    if (is_c_main && ctx->has_top_level_executable_statements) {
        codegen_emit_indent(ctx);
        fputs("py4_module_init();\n", ctx->out);
    }

    codegen_emit_suite(ctx, codegen_function_suite(function_def));
    codegen_pop_cleanup_scope(ctx);

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

    if (payload->kind == NODE_FOR_STATEMENT) {
        emit_for_statement(ctx, payload);
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
    const ParseNode *name;

    if (!codegen_is_type_assignment(statement_tail)) {
        return;
    }

    name = codegen_simple_statement_target(simple_stmt);
    type = semantic_type_of(ctx->semantic, codegen_statement_tail_type_node(statement_tail));
    if (name->kind == NODE_TUPLE_TARGET) {
        for (size_t i = 0; i < name->child_count; i++) {
            const ParseNode *item = codegen_expect_child(name, i, NODE_PRIMARY);

            codegen_emit_type_name(ctx, semantic_tuple_element_type(type, i));
            fprintf(ctx->out, " %s;\n", item->value);
        }
    } else {
        codegen_emit_type_name(ctx, type);
        fprintf(ctx->out, " %s;\n", name->value);
    }
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
                (payload->children[0]->kind == NODE_PRIMARY || payload->children[0]->kind == NODE_TUPLE_TARGET) &&
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
            if (payload->children[0]->kind == NODE_BREAK_STATEMENT ||
                payload->children[0]->kind == NODE_CONTINUE_STATEMENT) {
                codegen_error("break and continue are not valid at module scope");
            }

            name = codegen_simple_statement_target(payload);
            statement_tail = codegen_simple_statement_tail(payload);
            expr = codegen_statement_tail_expression(statement_tail);
            if (name->kind == NODE_TUPLE_TARGET) {
                emit_tuple_destructuring_assignment(ctx, name, statement_tail, expr);
                continue;
            }
            if (name->kind == NODE_INDEX) {
                ValueType list_type = semantic_type_of(ctx->semantic, name->children[0]);
                ValueType element_type = semantic_list_element_type(list_type);
                char *base = codegen_primary_to_c_string(ctx, name->children[0]);
                char *index = codegen_expression_to_c_string(ctx, name->children[1]);
                char *value = codegen_wrapped_expression_to_c_string(ctx, expr, element_type);
                char *call = codegen_list_ternary_call(list_type, "set", base, index, value);

                codegen_emit_indent(ctx);
                fprintf(ctx->out, "%s;\n", call);
                free(base);
                free(index);
                free(value);
                free(call);
                continue;
            }

            target_type = semantic_type_of(ctx->semantic, name);
            expr_text = codegen_wrapped_expression_to_c_string(ctx, expr, target_type);

            if (semantic_type_is_ref(target_type)) {
                char *temp_name = codegen_next_temp_name(ctx);
                char *type_name = codegen_type_to_c_string(target_type);
                int is_owned = codegen_expression_is_owned_ref(ctx, expr);

                codegen_emit_indent(ctx);
                fprintf(ctx->out, "%s %s = %s;\n", type_name, temp_name, expr_text);
                if (!is_owned) {
                    codegen_emit_ref_incref(ctx, target_type, temp_name);
                }
                if (!codegen_is_type_assignment(statement_tail)) {
                    codegen_emit_ref_decref(ctx, target_type, name->value);
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
        } else if (payload->kind == NODE_WHILE_STATEMENT) {
            emit_while_statement(ctx, payload);
        } else if (payload->kind == NODE_FOR_STATEMENT) {
            emit_for_statement(ctx, payload);
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
    codegen_emit_container_runtime(&ctx);
    codegen_emit_tuple_runtime(&ctx);
    codegen_emit_union_runtime(&ctx);
    emit_global_declarations(&ctx, root);
    emit_function_prototypes(&ctx, root);
    emit_module_init(&ctx, root);
    emit_top_level_functions(&ctx, root);
    emit_auto_main(&ctx);
}
