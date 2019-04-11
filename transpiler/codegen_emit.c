#include <stdio.h>
#include <stdlib.h>

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
            char *base = codegen_primary_to_c_string(ctx, target->children[0]);
            char *index = codegen_expression_to_c_string(ctx, target->children[1]);
            char *value = codegen_expression_to_c_string(ctx, expr);

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

    codegen_emit_indent(ctx);
    fprintf(ctx->out, "while (%s)\n", cond);
    free(cond);
    codegen_emit_indent(ctx);
    fputs("{\n", ctx->out);
    ctx->indent_level++;
    codegen_push_cleanup_scope(ctx);
    codegen_emit_suite(ctx, while_stmt->children[2]);
    codegen_pop_cleanup_scope(ctx);
    ctx->indent_level--;
    codegen_emit_indent(ctx);
    fputs("}\n", ctx->out);
}

static void emit_for_statement(CodegenContext *ctx, const ParseNode *for_stmt)
{
    const ParseNode *target = codegen_expect_child(for_stmt, 0, NODE_PRIMARY);
    const ParseNode *iterable = codegen_expect_child(for_stmt, 1, NODE_EXPRESSION);
    const ParseNode *suite = codegen_expect_child(for_stmt, 3, NODE_SUITE);
    ValueType iterable_type = semantic_type_of(ctx->semantic, iterable);
    char *iterable_expr;
    char *iterable_name;
    char *iterable_type_name;
    char *index_name;
    int iterable_owned;

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

    codegen_emit_indent(ctx);
    fprintf(ctx->out,
        "for (int %s = 0; %s < py4_list_int_len(%s); %s++)\n",
        index_name, index_name, iterable_name, index_name);
    codegen_emit_indent(ctx);
    fputs("{\n", ctx->out);
    ctx->indent_level++;
    codegen_emit_indent(ctx);
    fprintf(ctx->out, "int %s = py4_list_int_get(%s, %s);\n",
        target->value, iterable_name, index_name);
    codegen_push_cleanup_scope(ctx);
    codegen_emit_suite(ctx, suite);
    codegen_pop_cleanup_scope(ctx);
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
                char *base = codegen_primary_to_c_string(ctx, name->children[0]);
                char *index = codegen_expression_to_c_string(ctx, name->children[1]);
                char *value = codegen_expression_to_c_string(ctx, expr);

                codegen_emit_indent(ctx);
                fprintf(ctx->out, "py4_list_int_set(%s, %s, %s);\n", base, index, value);
                free(base);
                free(index);
                free(value);
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

    fputs("static Py4ListInt *py4_list_int_from_values(size_t count, const int *values)\n{\n", ctx->out);
    fputs("    Py4ListInt *list = py4_list_int_new();\n", ctx->out);
    fputs("    py4_list_int_ensure_capacity(list, count);\n", ctx->out);
    fputs("    for (size_t i = 0; i < count; i++) {\n", ctx->out);
    fputs("        list->items[i] = values[i];\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    list->len = count;\n", ctx->out);
    fputs("    return list;\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static int py4_list_int_get(Py4ListInt *list, int index)\n{\n", ctx->out);
    fputs("    py4_list_int_bounds_check(list, index);\n", ctx->out);
    fputs("    return list->items[index];\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static void py4_list_int_set(Py4ListInt *list, int index, int value)\n{\n", ctx->out);
    fputs("    py4_list_int_bounds_check(list, index);\n", ctx->out);
    fputs("    list->items[index] = value;\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static int py4_list_int_pop(Py4ListInt *list)\n{\n", ctx->out);
    fputs("    if (list == NULL) {\n", ctx->out);
    fputs("        fprintf(stderr, \"Runtime error: list[int] is null\\n\");\n", ctx->out);
    fputs("        exit(1);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    if (list->len == 0) {\n", ctx->out);
    fputs("        fprintf(stderr, \"Runtime error: pop from empty list[int]\\n\");\n", ctx->out);
    fputs("        exit(1);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    list->len--;\n", ctx->out);
    fputs("    return list->items[list->len];\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static void py4_list_int_clear(Py4ListInt *list)\n{\n", ctx->out);
    fputs("    if (list == NULL) {\n", ctx->out);
    fputs("        fprintf(stderr, \"Runtime error: list[int] is null\\n\");\n", ctx->out);
    fputs("        exit(1);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    list->len = 0;\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static Py4ListInt *py4_list_int_copy(Py4ListInt *list)\n{\n", ctx->out);
    fputs("    if (list == NULL) {\n", ctx->out);
    fputs("        fprintf(stderr, \"Runtime error: list[int] is null\\n\");\n", ctx->out);
    fputs("        exit(1);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    return py4_list_int_from_values(list->len, list->items);\n", ctx->out);
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
