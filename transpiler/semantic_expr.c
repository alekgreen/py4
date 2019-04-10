#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic_internal.h"

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

const ParseNode *semantic_function_parameters(const ParseNode *function_def)
{
    return semantic_expect_child(function_def, 1, NODE_PARAMETERS);
}

ValueType semantic_function_return_type(SemanticInfo *info, const ParseNode *function_def)
{
    if (function_def->child_count >= 3 && function_def->children[2]->kind == NODE_RETURN_TYPE) {
        const ParseNode *type_node = semantic_expect_child(function_def->children[2], 0, NODE_TYPE);
        return semantic_parse_type_node(info, type_node);
    }

    return TYPE_NONE;
}

size_t semantic_parameter_count(const ParseNode *parameters)
{
    if (parameters->child_count == 1 && semantic_is_epsilon_node(parameters->children[0])) {
        return 0;
    }
    return parameters->child_count;
}

ValueType semantic_parameter_type(SemanticInfo *info, const ParseNode *parameter)
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
    return strcmp(name, "len") == 0 ||
        strcmp(name, "list_int") == 0 ||
        strcmp(name, "list_append") == 0 ||
        strcmp(name, "list_get") == 0 ||
        strcmp(name, "list_len") == 0 ||
        strcmp(name, "list_set") == 0;
}

static ValueType infer_method_call_type(
    SemanticInfo *info,
    const ParseNode *call,
    Scope *scope)
{
    const ParseNode *receiver;
    const ParseNode *method = semantic_expect_child(call, 1, NODE_PRIMARY);
    const ParseNode *arguments = semantic_expect_child(call, 2, NODE_ARGUMENTS);
    ValueType receiver_type;

    if (call == NULL || call->child_count != 3) {
        semantic_error("malformed method call");
    }

    receiver = call->children[0];
    receiver_type = semantic_infer_primary_type(info, receiver, scope);

    if (receiver_type != TYPE_LIST_INT) {
        semantic_error("method '%s' requires list[int] receiver", method->value);
    }

    if (strcmp(method->value, "append") == 0) {
        expect_argument_count(method->value, arguments, 1);
        if (semantic_infer_expression_type(info, arguments->children[0], scope) != TYPE_INT) {
            semantic_error("method 'append' expects int argument");
        }
        semantic_record_node_type(info, call, TYPE_NONE);
        return TYPE_NONE;
    }

    if (strcmp(method->value, "pop") == 0) {
        expect_argument_count(method->value, arguments, 0);
        semantic_record_node_type(info, call, TYPE_INT);
        return TYPE_INT;
    }

    if (strcmp(method->value, "clear") == 0) {
        expect_argument_count(method->value, arguments, 0);
        semantic_record_node_type(info, call, TYPE_NONE);
        return TYPE_NONE;
    }

    if (strcmp(method->value, "copy") == 0) {
        expect_argument_count(method->value, arguments, 0);
        semantic_record_node_type(info, call, TYPE_LIST_INT);
        return TYPE_LIST_INT;
    }

    semantic_error("unknown list[int] method '%s'", method->value);
    return TYPE_NONE;
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
        if (semantic_infer_expression_type(info, arguments->children[0], scope) != TYPE_LIST_INT) {
            semantic_error("function 'list_append' argument 1 expects list[int]");
        }
        if (semantic_infer_expression_type(info, arguments->children[1], scope) != TYPE_INT) {
            semantic_error("function 'list_append' argument 2 expects int");
        }
        semantic_record_node_type(info, call, TYPE_NONE);
        return TYPE_NONE;
    }

    if (strcmp(name, "list_get") == 0) {
        expect_argument_count(name, arguments, 2);
        if (semantic_infer_expression_type(info, arguments->children[0], scope) != TYPE_LIST_INT) {
            semantic_error("function 'list_get' argument 1 expects list[int]");
        }
        if (semantic_infer_expression_type(info, arguments->children[1], scope) != TYPE_INT) {
            semantic_error("function 'list_get' argument 2 expects int");
        }
        semantic_record_node_type(info, call, TYPE_INT);
        return TYPE_INT;
    }

    if (strcmp(name, "list_len") == 0 || strcmp(name, "len") == 0) {
        expect_argument_count(name, arguments, 1);
        if (semantic_infer_expression_type(info, arguments->children[0], scope) != TYPE_LIST_INT) {
            semantic_error("function '%s' argument 1 expects list[int]", name);
        }
        semantic_record_node_type(info, call, TYPE_INT);
        return TYPE_INT;
    }

    if (strcmp(name, "list_set") == 0) {
        expect_argument_count(name, arguments, 3);
        if (semantic_infer_expression_type(info, arguments->children[0], scope) != TYPE_LIST_INT) {
            semantic_error("function 'list_set' argument 1 expects list[int]");
        }
        if (semantic_infer_expression_type(info, arguments->children[1], scope) != TYPE_INT) {
            semantic_error("function 'list_set' argument 2 expects int");
        }
        if (semantic_infer_expression_type(info, arguments->children[2], scope) != TYPE_INT) {
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

        ValueType arg_type = semantic_infer_expression_type(info, arguments->children[0], scope);
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
        ValueType actual = semantic_infer_expression_type(info, arguments->children[i], scope);
        if (!semantic_is_assignable(fn->param_types[i], actual)) {
            semantic_error("function '%s' argument %zu expects %s but got %s",
                fn->name, i + 1,
                semantic_type_name(fn->param_types[i]),
                semantic_type_name(actual));
        }
    }

    semantic_record_node_type(info, call, fn->return_type);
    return fn->return_type;
}

ValueType semantic_infer_primary_type(
    SemanticInfo *info,
    const ParseNode *node,
    Scope *scope)
{
    ValueType type;

    if (node->kind == NODE_LIST_LITERAL) {
        for (size_t i = 0; i < node->child_count; i++) {
            ValueType item_type = semantic_infer_expression_type(info, node->children[i], scope);

            if (item_type != TYPE_INT) {
                semantic_error("list[int] literal elements must be int");
            }
        }

        semantic_record_node_type(info, node, TYPE_LIST_INT);
        return TYPE_LIST_INT;
    }

    if (node->kind == NODE_METHOD_CALL) {
        type = infer_method_call_type(info, node, scope);
        semantic_record_node_type(info, node, type);
        return type;
    }

    if (node->kind == NODE_INDEX) {
        ValueType container_type = semantic_infer_primary_type(info, node->children[0], scope);
        ValueType index_type = semantic_infer_expression_type(info, node->children[1], scope);

        if (container_type != TYPE_LIST_INT) {
            semantic_error("indexing requires list[int] but got %s",
                semantic_type_name(container_type));
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
        type = semantic_infer_expression_type(info, node, scope);
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

ValueType semantic_infer_expression_type(
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
        ValueType type = semantic_infer_primary_type(info, expr->children[0], scope);

        semantic_record_node_type(info, expr, type);
        return type;
    }

    if (expr->child_count == 2) {
        operator_node = semantic_expect_child(expr, 0, NODE_OPERATOR);
        rhs_type = semantic_infer_primary_type(info, expr->children[1], scope);

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
            ValueType chain_lhs = semantic_infer_primary_type(info, expr->children[i - 1], scope);
            const ParseNode *chain_op = semantic_expect_child(expr, i, NODE_OPERATOR);
            ValueType chain_rhs = semantic_infer_primary_type(info, expr->children[i + 1], scope);

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

    lhs_type = semantic_infer_primary_type(info, expr->children[0], scope);
    operator_node = semantic_expect_child(expr, 1, NODE_OPERATOR);
    rhs_type = semantic_infer_primary_type(info, expr->children[2], scope);

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
