#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic_internal.h"

static ValueType infer_expression_type(
    SemanticInfo *info,
    const ParseNode *expr,
    Scope *scope);

static const ParseNode *function_parameters(const ParseNode *function_def)
{
    return semantic_expect_child(function_def, 1, NODE_PARAMETERS);
}

static ValueType function_return_type(SemanticInfo *info, const ParseNode *function_def)
{
    if (function_def->child_count >= 3 && function_def->children[2]->kind == NODE_RETURN_TYPE) {
        const ParseNode *type_node = semantic_expect_child(function_def->children[2], 0, NODE_TYPE);
        return semantic_parse_type_node(info, type_node);
    }

    return TYPE_NONE;
}

static size_t parameter_count(const ParseNode *parameters)
{
    if (parameters->child_count == 1 && semantic_is_epsilon_node(parameters->children[0])) {
        return 0;
    }
    return parameters->child_count;
}

static ValueType parameter_type(SemanticInfo *info, const ParseNode *parameter)
{
    return semantic_parse_type_node(info, semantic_expect_child(parameter, 0, NODE_TYPE));
}

static int is_print_call(const ParseNode *call)
{
    const ParseNode *callee = semantic_expect_child(call, 0, NODE_PRIMARY);

    return callee->value != NULL && strcmp(callee->value, "print") == 0;
}

static int is_builtin_name(const char *name)
{
    return strcmp(name, "list_int") == 0 ||
        strcmp(name, "list_append") == 0 ||
        strcmp(name, "list_get") == 0 ||
        strcmp(name, "list_len") == 0 ||
        strcmp(name, "list_set") == 0;
}

static int is_ref_variable_expression(const ParseNode *expr)
{
    if (expr->kind != NODE_EXPRESSION || expr->child_count != 1) {
        return 0;
    }

    return expr->children[0]->kind == NODE_PRIMARY &&
        expr->children[0]->token_type == TOKEN_IDENTIFIER;
}

static int is_ref_variable_node(const ParseNode *node)
{
    return node != NULL &&
        node->kind == NODE_PRIMARY &&
        node->token_type == TOKEN_IDENTIFIER;
}

static const ParseNode *simple_statement_target(const ParseNode *simple_stmt)
{
    if (simple_stmt == NULL || simple_stmt->child_count == 0) {
        semantic_error("malformed simple statement");
    }
    return simple_stmt->children[0];
}

static void expect_argument_count(const char *name, const ParseNode *arguments, size_t expected)
{
    size_t actual = arguments->child_count;

    if (actual == 1 && semantic_is_epsilon_node(arguments->children[0])) {
        actual = 0;
    }

    if (actual != expected) {
        semantic_error("function '%s' expects %zu arguments", name, expected);
    }
}

static ValueType infer_builtin_call_type(
    SemanticInfo *info,
    const char *name,
    const ParseNode *call,
    const ParseNode *arguments,
    Scope *scope)
{
    (void)call;

    if (strcmp(name, "list_int") == 0) {
        expect_argument_count(name, arguments, 0);
        semantic_record_node_type(info, call, TYPE_LIST_INT);
        return TYPE_LIST_INT;
    }

    if (strcmp(name, "list_append") == 0) {
        expect_argument_count(name, arguments, 2);
        if (infer_expression_type(info, arguments->children[0], scope) != TYPE_LIST_INT) {
            semantic_error("function 'list_append' argument 1 expects list[int]");
        }
        if (!is_ref_variable_expression(arguments->children[0])) {
            semantic_error("function 'list_append' argument 1 must be a variable");
        }
        if (infer_expression_type(info, arguments->children[1], scope) != TYPE_INT) {
            semantic_error("function 'list_append' argument 2 expects int");
        }
        semantic_record_node_type(info, call, TYPE_NONE);
        return TYPE_NONE;
    }

    if (strcmp(name, "list_get") == 0) {
        expect_argument_count(name, arguments, 2);
        if (infer_expression_type(info, arguments->children[0], scope) != TYPE_LIST_INT) {
            semantic_error("function 'list_get' argument 1 expects list[int]");
        }
        if (!is_ref_variable_expression(arguments->children[0])) {
            semantic_error("function 'list_get' argument 1 must be a variable");
        }
        if (infer_expression_type(info, arguments->children[1], scope) != TYPE_INT) {
            semantic_error("function 'list_get' argument 2 expects int");
        }
        semantic_record_node_type(info, call, TYPE_INT);
        return TYPE_INT;
    }

    if (strcmp(name, "list_len") == 0) {
        expect_argument_count(name, arguments, 1);
        if (infer_expression_type(info, arguments->children[0], scope) != TYPE_LIST_INT) {
            semantic_error("function 'list_len' argument 1 expects list[int]");
        }
        if (!is_ref_variable_expression(arguments->children[0])) {
            semantic_error("function 'list_len' argument 1 must be a variable");
        }
        semantic_record_node_type(info, call, TYPE_INT);
        return TYPE_INT;
    }

    if (strcmp(name, "list_set") == 0) {
        expect_argument_count(name, arguments, 3);
        if (infer_expression_type(info, arguments->children[0], scope) != TYPE_LIST_INT) {
            semantic_error("function 'list_set' argument 1 expects list[int]");
        }
        if (!is_ref_variable_expression(arguments->children[0])) {
            semantic_error("function 'list_set' argument 1 must be a variable");
        }
        if (infer_expression_type(info, arguments->children[1], scope) != TYPE_INT) {
            semantic_error("function 'list_set' argument 2 expects int");
        }
        if (infer_expression_type(info, arguments->children[2], scope) != TYPE_INT) {
            semantic_error("function 'list_set' argument 3 expects int");
        }
        semantic_record_node_type(info, call, TYPE_NONE);
        return TYPE_NONE;
    }

    semantic_error("unknown builtin '%s'", name);
    return TYPE_NONE;
}

static ValueType infer_call_type(
    SemanticInfo *info,
    const ParseNode *call,
    Scope *scope)
{
    const ParseNode *callee = semantic_expect_child(call, 0, NODE_PRIMARY);
    const ParseNode *arguments = semantic_expect_child(call, 1, NODE_ARGUMENTS);

    if (is_print_call(call)) {
        if (arguments->child_count == 1 && semantic_is_epsilon_node(arguments->children[0])) {
            semantic_record_node_type(info, call, TYPE_NONE);
            return TYPE_NONE;
        }
        if (arguments->child_count != 1) {
            semantic_error("print expects zero or one argument");
        }

        ValueType arg_type = infer_expression_type(info, arguments->children[0], scope);
        if (arg_type == TYPE_NONE) {
            semantic_error("print cannot print None");
        }
        if (semantic_type_is_ref(arg_type)) {
            semantic_error("print does not support %s yet", semantic_type_name(arg_type));
        }

        semantic_record_node_type(info, call, TYPE_NONE);
        return TYPE_NONE;
    }

    if (is_builtin_name(callee->value)) {
        return infer_builtin_call_type(info, callee->value, call, arguments, scope);
    }

    FunctionInfo *fn = semantic_find_function(info->functions, callee->value);
    if (fn == NULL) {
        semantic_error("unknown function '%s'", callee->value);
    }

    if (arguments->child_count == 1 && semantic_is_epsilon_node(arguments->children[0])) {
        if (fn->param_count != 0) {
            semantic_error("function '%s' expects %zu arguments", fn->name, fn->param_count);
        }
        semantic_record_node_type(info, call, fn->return_type);
        return fn->return_type;
    }

    if (arguments->child_count != fn->param_count) {
        semantic_error("function '%s' expects %zu arguments", fn->name, fn->param_count);
    }

    for (size_t i = 0; i < arguments->child_count; i++) {
        ValueType actual = infer_expression_type(info, arguments->children[i], scope);
        if (!semantic_is_assignable(fn->param_types[i], actual)) {
            semantic_error("function '%s' argument %zu expects %s but got %s",
                fn->name, i + 1,
                semantic_type_name(fn->param_types[i]),
                semantic_type_name(actual));
        }
        if (semantic_type_is_ref(fn->param_types[i]) &&
            semantic_type_is_ref(actual) &&
            !is_ref_variable_expression(arguments->children[i])) {
            semantic_error("function '%s' argument %zu must be a variable for %s values",
                fn->name, i + 1, semantic_type_name(actual));
        }
    }

    semantic_record_node_type(info, call, fn->return_type);
    return fn->return_type;
}

static ValueType infer_primary_type(
    SemanticInfo *info,
    const ParseNode *node,
    Scope *scope)
{
    ValueType type;

    if (node->kind == NODE_LIST_LITERAL) {
        for (size_t i = 0; i < node->child_count; i++) {
            ValueType item_type = infer_expression_type(info, node->children[i], scope);

            if (item_type != TYPE_INT) {
                semantic_error("list[int] literal elements must be int");
            }
        }

        semantic_record_node_type(info, node, TYPE_LIST_INT);
        return TYPE_LIST_INT;
    }

    if (node->kind == NODE_INDEX) {
        ValueType container_type = infer_primary_type(info, node->children[0], scope);
        ValueType index_type = infer_expression_type(info, node->children[1], scope);

        if (container_type != TYPE_LIST_INT) {
            semantic_error("indexing requires list[int] but got %s",
                semantic_type_name(container_type));
        }
        if (!is_ref_variable_node(node->children[0])) {
            semantic_error("indexed list access must use a variable base");
        }
        if (index_type != TYPE_INT) {
            semantic_error("list[int] index must be int");
        }

        semantic_record_node_type(info, node, TYPE_INT);
        return TYPE_INT;
    }

    if (node->kind == NODE_CALL) {
        type = infer_call_type(info, node, scope);
        semantic_record_node_type(info, node, type);
        return type;
    }

    if (node->kind == NODE_EXPRESSION) {
        type = infer_expression_type(info, node, scope);
        semantic_record_node_type(info, node, type);
        return type;
    }

    if (node->kind != NODE_PRIMARY || node->value == NULL) {
        semantic_error("unsupported primary expression");
    }

    switch (node->token_type) {
        case TOKEN_NUMBER:
            type = strchr(node->value, '.') != NULL ? TYPE_FLOAT : TYPE_INT;
            break;
        case TOKEN_STRING:
            type = TYPE_STR;
            break;
        case TOKEN_CHAR:
            type = TYPE_CHAR;
            break;
        case TOKEN_KEYWORD:
            if (strcmp(node->value, "True") == 0 || strcmp(node->value, "False") == 0) {
                type = TYPE_BOOL;
                break;
            }
            semantic_error("unexpected keyword '%s' in expression", node->value);
            break;
        case TOKEN_IDENTIFIER: {
            VariableBinding *var = semantic_find_variable(scope, node->value);
            if (var == NULL) {
                semantic_error("unknown variable '%s'", node->value);
            }
            type = var->type;
            break;
        }
        default:
            semantic_error("unsupported primary token");
    }

    semantic_record_node_type(info, node, type);
    return type;
}

static int is_equality_operator(const char *op)
{
    return strcmp(op, "==") == 0 || strcmp(op, "!=") == 0;
}

static int is_comparison_operator(const char *op)
{
    return strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
        strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0;
}

static int is_arithmetic_operator(const char *op)
{
    return strcmp(op, "+") == 0 || strcmp(op, "-") == 0 ||
        strcmp(op, "*") == 0 || strcmp(op, "/") == 0;
}

static int is_boolean_operator(const char *op)
{
    return strcmp(op, "and") == 0 || strcmp(op, "or") == 0;
}

static void typecheck_comparison_operands(ValueType lhs_type, ValueType rhs_type, const char *op)
{
    if (semantic_type_is_union(lhs_type) || semantic_type_is_union(rhs_type)) {
        semantic_error("operator '%s' does not support union operands yet", op);
    }
    if (semantic_type_is_ref(lhs_type) || semantic_type_is_ref(rhs_type)) {
        semantic_error("operator '%s' does not support %s operands",
            op,
            semantic_type_is_ref(lhs_type) ? semantic_type_name(lhs_type) : semantic_type_name(rhs_type));
    }

    if (is_comparison_operator(op)) {
        if (!semantic_is_numeric_type(lhs_type) || !semantic_is_numeric_type(rhs_type)) {
            semantic_error("comparison '%s' requires numeric operands", op);
        }
        return;
    }

    if (is_equality_operator(op)) {
        if (lhs_type == TYPE_STR || rhs_type == TYPE_STR) {
            semantic_error("str equality is not supported yet");
        }
        if (!(semantic_is_assignable(lhs_type, rhs_type) || semantic_is_assignable(rhs_type, lhs_type))) {
            semantic_error("operator '%s' requires comparable operands", op);
        }
        return;
    }

    semantic_error("unsupported operator '%s'", op);
}

static ValueType infer_expression_type(
    SemanticInfo *info,
    const ParseNode *expr,
    Scope *scope)
{
    const ParseNode *operator_node;
    ValueType lhs_type;
    ValueType rhs_type;

    if (expr->kind != NODE_EXPRESSION || expr->child_count == 0) {
        semantic_error("malformed expression");
    }

    if (expr->child_count == 1) {
        ValueType type = infer_primary_type(info, expr->children[0], scope);

        semantic_record_node_type(info, expr, type);
        return type;
    }

    if (expr->child_count == 2) {
        operator_node = semantic_expect_child(expr, 0, NODE_OPERATOR);
        rhs_type = infer_primary_type(info, expr->children[1], scope);

        if (strcmp(operator_node->value, "not") == 0) {
            if (rhs_type != TYPE_BOOL) {
                semantic_error("operator 'not' requires bool operands");
            }
            semantic_record_node_type(info, expr, TYPE_BOOL);
            return TYPE_BOOL;
        }

        if (semantic_type_is_union(rhs_type)) {
            semantic_error("operator '%s' does not support union operands yet", operator_node->value);
        }

        if (!is_arithmetic_operator(operator_node->value)) {
            semantic_error("unsupported unary operator '%s'", operator_node->value);
        }
        if (!semantic_is_numeric_type(rhs_type)) {
            semantic_error("operator '%s' requires numeric operands", operator_node->value);
        }

        semantic_record_node_type(info, expr, rhs_type);
        return rhs_type;
    }

    if (expr->child_count > 3) {
        for (size_t i = 1; i < expr->child_count; i += 2) {
            ValueType chain_lhs = infer_primary_type(info, expr->children[i - 1], scope);
            const ParseNode *chain_op = semantic_expect_child(expr, i, NODE_OPERATOR);
            ValueType chain_rhs = infer_primary_type(info, expr->children[i + 1], scope);

            if (!(is_comparison_operator(chain_op->value) || is_equality_operator(chain_op->value))) {
                semantic_error("unsupported operator '%s' in comparison chain", chain_op->value);
            }
            typecheck_comparison_operands(chain_lhs, chain_rhs, chain_op->value);
        }

        semantic_record_node_type(info, expr, TYPE_BOOL);
        return TYPE_BOOL;
    }

    if (expr->child_count != 3) {
        semantic_error("malformed expression");
    }

    lhs_type = infer_primary_type(info, expr->children[0], scope);
    operator_node = semantic_expect_child(expr, 1, NODE_OPERATOR);
    rhs_type = infer_primary_type(info, expr->children[2], scope);

    if (is_arithmetic_operator(operator_node->value)) {
        ValueType result_type;

        if (semantic_type_is_union(lhs_type) || semantic_type_is_union(rhs_type)) {
            semantic_error("operator '%s' does not support union operands yet", operator_node->value);
        }
        if (!semantic_is_numeric_type(lhs_type) || !semantic_is_numeric_type(rhs_type)) {
            semantic_error("operator '%s' requires numeric operands", operator_node->value);
        }
        if (strcmp(operator_node->value, "/") == 0 || lhs_type == TYPE_FLOAT || rhs_type == TYPE_FLOAT) {
            result_type = TYPE_FLOAT;
        } else {
            result_type = TYPE_INT;
        }

        semantic_record_node_type(info, expr, result_type);
        return result_type;
    }

    if (is_boolean_operator(operator_node->value)) {
        if (lhs_type != TYPE_BOOL || rhs_type != TYPE_BOOL) {
            semantic_error("operator '%s' requires bool operands", operator_node->value);
        }

        semantic_record_node_type(info, expr, TYPE_BOOL);
        return TYPE_BOOL;
    }

    if (is_comparison_operator(operator_node->value) || is_equality_operator(operator_node->value)) {
        typecheck_comparison_operands(lhs_type, rhs_type, operator_node->value);
        semantic_record_node_type(info, expr, TYPE_BOOL);
        return TYPE_BOOL;
    }

    semantic_error("unsupported operator '%s'", operator_node->value);
    return TYPE_NONE;
}

static int is_type_assignment(const ParseNode *statement_tail)
{
    return statement_tail->child_count == 4 &&
        statement_tail->children[0]->kind == NODE_COLON;
}

static const ParseNode *simple_statement_tail(const ParseNode *simple_stmt)
{
    return semantic_expect_child(simple_stmt, 1, NODE_STATEMENT_TAIL);
}

static const ParseNode *statement_tail_expression(const ParseNode *statement_tail)
{
    if (is_type_assignment(statement_tail)) {
        return semantic_expect_child(statement_tail, 3, NODE_EXPRESSION);
    }
    return semantic_expect_child(statement_tail, 1, NODE_EXPRESSION);
}

static const ParseNode *statement_tail_type_node(const ParseNode *statement_tail)
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

    ValueType expr_type = infer_expression_type(info, return_stmt->children[0], scope);
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
        infer_expression_type(info, semantic_expect_child(first_child, 0, NODE_EXPRESSION), scope);
        return;
    }

    if (simple_stmt->child_count != 2) {
        semantic_error("malformed simple statement");
    }

    const ParseNode *target = simple_statement_target(simple_stmt);
    const ParseNode *statement_tail = simple_statement_tail(simple_stmt);
    const ParseNode *expr = statement_tail_expression(statement_tail);
    ValueType expr_type = infer_expression_type(info, expr, scope);

    if (target->kind == NODE_INDEX) {
        ValueType container_type = infer_primary_type(info, target->children[0], scope);
        ValueType index_type = infer_expression_type(info, target->children[1], scope);

        if (is_type_assignment(statement_tail)) {
            semantic_error("indexed assignment cannot use a type annotation");
        }
        if (container_type != TYPE_LIST_INT) {
            semantic_error("indexed assignment requires list[int] but got %s",
                semantic_type_name(container_type));
        }
        if (!is_ref_variable_node(target->children[0])) {
            semantic_error("indexed assignment must use a variable base");
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

    if (is_type_assignment(statement_tail)) {
        ValueType declared = semantic_parse_type_node(info, statement_tail_type_node(statement_tail));
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
    ValueType condition_type = infer_expression_type(info, if_stmt->children[0], scope);

    if (condition_type != TYPE_BOOL) {
        semantic_error("if condition must be bool");
    }

    typecheck_branch_suite(info, if_stmt->children[2], scope, current_function);

    for (size_t i = 3; i < if_stmt->child_count; i++) {
        const ParseNode *branch = if_stmt->children[i];

        if (branch->kind == NODE_ELIF_CLAUSE) {
            ValueType elif_type = infer_expression_type(info, branch->children[0], scope);
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
    ValueType condition_type = infer_expression_type(info, while_stmt->children[0], scope);

    if (condition_type != TYPE_BOOL) {
        semantic_error("while condition must be bool");
    }

    typecheck_branch_suite(info, while_stmt->children[2], scope, current_function);
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

        const ParseNode *parameters = function_parameters(payload);
        size_t count = parameter_count(parameters);

        FunctionInfo *fn = malloc(sizeof(FunctionInfo));
        if (fn == NULL) {
            perror("malloc");
            exit(1);
        }

        fn->name = name;
        fn->return_type = function_return_type(info, payload);
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
            fn->param_types[j] = parameter_type(info, semantic_expect_child(parameters, j, NODE_PARAMETER));
        }
    }
}

static void typecheck_function(SemanticInfo *info, const ParseNode *function_def, Scope *global_scope)
{
    const ParseNode *parameters = function_parameters(function_def);
    const ParseNode *suite = function_def->children[function_def->child_count - 1];
    Scope local_scope = {0};
    FunctionContext current = {0};

    local_scope.parent = global_scope;
    current.name = semantic_expect_child(function_def, 0, NODE_PRIMARY)->value;
    current.return_type = function_return_type(info, function_def);

    if (!(parameters->child_count == 1 && semantic_is_epsilon_node(parameters->children[0]))) {
        for (size_t i = 0; i < parameters->child_count; i++) {
            const ParseNode *param = semantic_expect_child(parameters, i, NODE_PARAMETER);
            semantic_bind_variable(&local_scope, param->value, parameter_type(info, param));
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
