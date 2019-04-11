#include <stdio.h>
#include <stdlib.h>

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

static void typecheck_statement(
    SemanticInfo *info,
    const ParseNode *statement,
    Scope *scope,
    FunctionContext *current_function,
    int allow_function_defs);

static void typecheck_branch_suite(
    SemanticInfo *info,
    const ParseNode *suite,
    Scope *parent_scope,
    FunctionContext *current_function)
{
    Scope branch_scope = {0};

    branch_scope.parent = parent_scope;
    if (!(suite->child_count == 1 && semantic_is_epsilon_node(suite->children[0]))) {
        for (size_t i = 0; i < suite->child_count; i++) {
            typecheck_statement(info, suite->children[i], &branch_scope, current_function, 0);
        }
    }

    semantic_free_scope_bindings(branch_scope.vars);
}

static void typecheck_return_statement(
    SemanticInfo *info,
    const ParseNode *return_stmt,
    Scope *scope,
    FunctionContext *current_function)
{
    if (current_function == NULL) {
        semantic_error("return is only valid inside a function");
    }

    current_function->saw_return = 1;

    if (return_stmt->child_count != 1) {
        semantic_error("malformed return statement");
    }

    if (semantic_is_epsilon_node(return_stmt->children[0])) {
        if (!semantic_type_contains(current_function->return_type, TYPE_NONE)) {
            semantic_error("function '%s' must return %s",
                current_function->name,
                semantic_type_name(current_function->return_type));
        }
        return;
    }

    ValueType expr_type = semantic_infer_expression_type(info, return_stmt->children[0], scope);
    if (!semantic_is_assignable(current_function->return_type, expr_type)) {
        semantic_error("function '%s' returns %s but got %s",
            current_function->name,
            semantic_type_name(current_function->return_type),
            semantic_type_name(expr_type));
    }
}

static void typecheck_simple_statement(
    SemanticInfo *info,
    const ParseNode *simple_stmt,
    Scope *scope,
    FunctionContext *current_function)
{
    const ParseNode *first_child = simple_stmt->children[0];

    if (first_child->kind == NODE_RETURN_STATEMENT) {
        typecheck_return_statement(info, first_child, scope, current_function);
        return;
    }

    if (first_child->kind == NODE_EXPRESSION_STATEMENT) {
        semantic_infer_expression_type(info, semantic_expect_child(first_child, 0, NODE_EXPRESSION), scope);
        return;
    }

    if (simple_stmt->child_count != 2) {
        semantic_error("malformed simple statement");
    }

    const ParseNode *target = semantic_simple_statement_target(simple_stmt);
    const ParseNode *statement_tail = semantic_simple_statement_tail(simple_stmt);
    const ParseNode *expr = semantic_statement_tail_expression(statement_tail);
    ValueType expr_type = semantic_infer_expression_type(info, expr, scope);

    if (target->kind == NODE_INDEX) {
        ValueType container_type = semantic_infer_primary_type(info, target->children[0], scope);
        ValueType index_type = semantic_infer_expression_type(info, target->children[1], scope);

        if (semantic_is_type_assignment(statement_tail)) {
            semantic_error("indexed assignment cannot use a type annotation");
        }
        if (container_type != TYPE_LIST_INT) {
            semantic_error("indexed assignment requires list[int] but got %s",
                semantic_type_name(container_type));
        }
        if (index_type != TYPE_INT) {
            semantic_error("list[int] index must be int");
        }
        if (expr_type != TYPE_INT) {
            semantic_error("cannot assign %s to list[int] element",
                semantic_type_name(expr_type));
        }

        semantic_record_node_type(info, target, TYPE_INT);
        return;
    }

    if (semantic_is_type_assignment(statement_tail)) {
        ValueType declared = semantic_parse_type_node(info, semantic_statement_tail_type_node(statement_tail));
        if (!semantic_is_assignable(declared, expr_type)) {
            semantic_error("cannot assign %s to %s '%s'",
                semantic_type_name(expr_type),
                semantic_type_name(declared),
                target->value);
        }
        semantic_bind_variable(scope, target->value, declared);
        semantic_record_node_type(info, target, declared);
        return;
    }

    VariableBinding *var = semantic_find_variable(scope, target->value);
    if (var == NULL) {
        semantic_error("assignment to undeclared variable '%s'", target->value);
    }
    if (!semantic_is_assignable(var->type, expr_type)) {
        semantic_error("cannot assign %s to %s '%s'",
            semantic_type_name(expr_type),
            semantic_type_name(var->type),
            target->value);
    }

    semantic_record_node_type(info, target, var->type);
}

static void typecheck_suite(
    SemanticInfo *info,
    const ParseNode *suite,
    Scope *scope,
    FunctionContext *current_function)
{
    if (suite->child_count == 1 && semantic_is_epsilon_node(suite->children[0])) {
        return;
    }

    for (size_t i = 0; i < suite->child_count; i++) {
        typecheck_statement(info, suite->children[i], scope, current_function, 0);
    }
}

static void typecheck_if_statement(
    SemanticInfo *info,
    const ParseNode *if_stmt,
    Scope *scope,
    FunctionContext *current_function)
{
    ValueType condition_type = semantic_infer_expression_type(info, if_stmt->children[0], scope);

    if (condition_type != TYPE_BOOL) {
        semantic_error("if condition must be bool");
    }

    typecheck_branch_suite(info, if_stmt->children[2], scope, current_function);

    for (size_t i = 3; i < if_stmt->child_count; i++) {
        const ParseNode *branch = if_stmt->children[i];

        if (branch->kind == NODE_ELIF_CLAUSE) {
            ValueType elif_type = semantic_infer_expression_type(info, branch->children[0], scope);
            if (elif_type != TYPE_BOOL) {
                semantic_error("elif condition must be bool");
            }
            typecheck_branch_suite(info, branch->children[2], scope, current_function);
            continue;
        }

        if (branch->kind == NODE_ELSE_CLAUSE) {
            typecheck_branch_suite(info, branch->children[1], scope, current_function);
            continue;
        }

        semantic_error("malformed if statement");
    }
}

static void typecheck_while_statement(
    SemanticInfo *info,
    const ParseNode *while_stmt,
    Scope *scope,
    FunctionContext *current_function)
{
    ValueType condition_type = semantic_infer_expression_type(info, while_stmt->children[0], scope);

    if (condition_type != TYPE_BOOL) {
        semantic_error("while condition must be bool");
    }

    typecheck_branch_suite(info, while_stmt->children[2], scope, current_function);
}

static void typecheck_for_statement(
    SemanticInfo *info,
    const ParseNode *for_stmt,
    Scope *scope,
    FunctionContext *current_function)
{
    Scope loop_scope = {0};
    const ParseNode *target;
    ValueType iterable_type;

    target = semantic_expect_child(for_stmt, 0, NODE_PRIMARY);
    iterable_type = semantic_infer_expression_type(info, for_stmt->children[1], scope);

    if (iterable_type != TYPE_LIST_INT) {
        semantic_error("for loop iterable must be list[int] but got %s",
            semantic_type_name(iterable_type));
    }

    loop_scope.parent = scope;
    semantic_bind_variable(&loop_scope, target->value, TYPE_INT);
    semantic_record_node_type(info, target, TYPE_INT);
    typecheck_suite(info, for_stmt->children[3], &loop_scope, current_function);
    semantic_free_scope_bindings(loop_scope.vars);
}

static void collect_functions(SemanticInfo *info, const ParseNode *root)
{
    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = semantic_statement_payload(root->children[i]);

        if (payload->kind != NODE_FUNCTION_DEF) {
            continue;
        }

        const char *name = semantic_expect_child(payload, 0, NODE_PRIMARY)->value;
        if (semantic_find_function(info->functions, name) != NULL) {
            semantic_error("duplicate function '%s'", name);
        }

        const ParseNode *parameters = semantic_function_parameters(payload);
        size_t count = semantic_parameter_count(parameters);

        FunctionInfo *fn = malloc(sizeof(FunctionInfo));
        if (fn == NULL) {
            perror("malloc");
            exit(1);
        }

        fn->name = name;
        fn->return_type = semantic_function_return_type(info, payload);
        fn->param_count = count;
        fn->param_types = count > 0 ? malloc(sizeof(ValueType) * count) : NULL;
        fn->node = payload;
        fn->next = info->functions;
        info->functions = fn;

        if (count > 0 && fn->param_types == NULL) {
            perror("malloc");
            exit(1);
        }

        for (size_t j = 0; j < count; j++) {
            fn->param_types[j] = semantic_parameter_type(info, semantic_expect_child(parameters, j, NODE_PARAMETER));
        }
    }
}

static void typecheck_function(SemanticInfo *info, const ParseNode *function_def, Scope *global_scope)
{
    const ParseNode *parameters = semantic_function_parameters(function_def);
    const ParseNode *suite = function_def->children[function_def->child_count - 1];
    Scope local_scope = {0};
    FunctionContext current = {0};

    local_scope.parent = global_scope;
    current.name = semantic_expect_child(function_def, 0, NODE_PRIMARY)->value;
    current.return_type = semantic_function_return_type(info, function_def);

    if (!(parameters->child_count == 1 && semantic_is_epsilon_node(parameters->children[0]))) {
        for (size_t i = 0; i < parameters->child_count; i++) {
            const ParseNode *param = semantic_expect_child(parameters, i, NODE_PARAMETER);
            semantic_bind_variable(&local_scope, param->value, semantic_parameter_type(info, param));
        }
    }

    typecheck_suite(info, suite, &local_scope, &current);

    if (current.return_type != TYPE_NONE && !current.saw_return) {
        semantic_error("function '%s' must return %s",
            current.name, semantic_type_name(current.return_type));
    }

    semantic_free_scope_bindings(local_scope.vars);
}

static void typecheck_statement(
    SemanticInfo *info,
    const ParseNode *statement,
    Scope *scope,
    FunctionContext *current_function,
    int allow_function_defs)
{
    const ParseNode *payload = semantic_statement_payload(statement);

    if (payload->kind == NODE_FUNCTION_DEF) {
        if (!allow_function_defs) {
            semantic_error("nested function definitions are not supported");
        }
        typecheck_function(info, payload, scope);
        return;
    }

    if (payload->kind == NODE_IMPORT_STATEMENT) {
        semantic_error("imports are only supported at module scope");
    }

    if (payload->kind == NODE_WHILE_STATEMENT) {
        typecheck_while_statement(info, payload, scope, current_function);
        return;
    }

    if (payload->kind == NODE_FOR_STATEMENT) {
        typecheck_for_statement(info, payload, scope, current_function);
        return;
    }

    if (semantic_is_if_statement(payload)) {
        typecheck_if_statement(info, payload, scope, current_function);
        return;
    }

    if (payload->kind != NODE_SIMPLE_STATEMENT) {
        semantic_error("unsupported statement node");
    }

    typecheck_simple_statement(info, payload, scope, current_function);
}

SemanticInfo *analyze_program(const ParseNode *root)
{
    Scope global_scope = {0};
    SemanticInfo *info;

    if (root == NULL || root->kind != NODE_S) {
        semantic_error("expected program root");
    }

    info = calloc(1, sizeof(SemanticInfo));
    if (info == NULL) {
        perror("calloc");
        exit(1);
    }

    collect_functions(info, root);

    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = semantic_statement_payload(root->children[i]);

        if (payload->kind == NODE_IMPORT_STATEMENT) {
            semantic_error("imports should be resolved before semantic analysis");
        } else if (payload->kind == NODE_FUNCTION_DEF) {
            typecheck_function(info, payload, &global_scope);
        } else {
            typecheck_statement(info, root->children[i], &global_scope, NULL, 0);
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
        free(functions->param_types);
        free(functions);
        functions = next;
    }

    free(info);
}
