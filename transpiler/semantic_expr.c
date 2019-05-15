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
        semantic_error_at_node(arguments, "function '%s' expects %zu arguments", name, expected);
    }
}

static int is_direct_list_literal_expression(const ParseNode *expr)
{
    return expr != NULL &&
        expr->kind == NODE_EXPRESSION &&
        expr->child_count == 1 &&
        expr->children[0]->kind == NODE_LIST_LITERAL;
}

static int is_direct_tuple_literal_expression(const ParseNode *expr)
{
    return expr != NULL &&
        expr->kind == NODE_EXPRESSION &&
        expr->child_count == 1 &&
        expr->children[0]->kind == NODE_TUPLE_LITERAL;
}

static int tuple_element_type_supported(ValueType type)
{
    return type == TYPE_INT ||
        type == TYPE_FLOAT ||
        type == TYPE_BOOL ||
        type == TYPE_CHAR ||
        type == TYPE_STR ||
        semantic_type_is_tuple(type);
}

static ValueType infer_class_constructor_type(
    SemanticInfo *info,
    ValueType class_type,
    const ParseNode *call,
    const ParseNode *arguments,
    Scope *scope)
{
    size_t field_count = semantic_class_field_count(class_type);

    if (arguments->child_count == 1 && semantic_is_epsilon_node(arguments->children[0])) {
        if (field_count != 0) {
            semantic_error_at_node(call, "constructor '%s' expects %zu arguments",
                semantic_class_name(class_type),
                field_count);
        }
        semantic_record_node_type(info, call, class_type);
        return class_type;
    }

    if (arguments->child_count != field_count) {
        semantic_error_at_node(call, "constructor '%s' expects %zu arguments",
            semantic_class_name(class_type),
            field_count);
    }

    for (size_t i = 0; i < field_count; i++) {
        ValueType field_type = semantic_class_field_type(class_type, i);
        ValueType actual = semantic_infer_expression_type_with_hint(
            info,
            arguments->children[i],
            scope,
            field_type);

        if (!semantic_is_assignable(field_type, actual)) {
            semantic_error_at_node(arguments->children[i],
                "constructor '%s' field '%s' expects %s but got %s",
                semantic_class_name(class_type),
                semantic_class_field_name(class_type, i),
                semantic_type_name(field_type),
                semantic_type_name(actual));
        }
    }

    semantic_record_node_type(info, call, class_type);
    return class_type;
}

static ValueType infer_list_literal_with_hint(
    SemanticInfo *info,
    const ParseNode *literal,
    Scope *scope,
    ValueType expected_type)
{
    ValueType element_type = semantic_list_element_type(expected_type);

    for (size_t i = 0; i < literal->child_count; i++) {
        ValueType item_type = semantic_infer_expression_type(info, literal->children[i], scope);

        if (!semantic_is_assignable(element_type, item_type)) {
            semantic_error_at_node(literal->children[i], "cannot assign %s to %s list literal element",
                semantic_type_name(item_type),
                semantic_type_name(element_type));
        }
    }

    semantic_record_node_type(info, literal, expected_type);
    return expected_type;
}

static ValueType infer_tuple_literal_with_hint(
    SemanticInfo *info,
    const ParseNode *literal,
    Scope *scope,
    ValueType expected_type)
{
    size_t expected_count = semantic_tuple_element_count(expected_type);

    if (literal->child_count != expected_count) {
        semantic_error_at_node(literal,
            "tuple literal for %s expects %zu elements but got %zu",
            semantic_type_name(expected_type),
            expected_count,
            literal->child_count);
    }

    for (size_t i = 0; i < literal->child_count; i++) {
        ValueType expected_element = semantic_tuple_element_type(expected_type, i);
        ValueType actual = semantic_infer_expression_type_with_hint(
            info,
            literal->children[i],
            scope,
            expected_element);

        if (!semantic_is_assignable(expected_element, actual)) {
            semantic_error_at_node(literal->children[i],
                "tuple element %zu expects %s but got %s",
                i,
                semantic_type_name(expected_element),
                semantic_type_name(actual));
        }
    }

    semantic_record_node_type(info, literal, expected_type);
    return expected_type;
}

int semantic_tuple_literal_index(const ParseNode *expr, size_t *index_out)
{
    const ParseNode *primary;
    char *end = NULL;
    long index;

    if (expr == NULL || expr->kind != NODE_EXPRESSION || expr->child_count != 1) {
        return 0;
    }

    primary = expr->children[0];
    if (primary->kind != NODE_PRIMARY || primary->token_type != TOKEN_NUMBER) {
        return 0;
    }
    if (strchr(primary->value, '.') != NULL) {
        return 0;
    }

    index = strtol(primary->value, &end, 10);
    if (end == NULL || *end != '\0' || index < 0) {
        return 0;
    }

    *index_out = (size_t)index;
    return 1;
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
        strcmp(name, "list_float") == 0 ||
        strcmp(name, "list_bool") == 0 ||
        strcmp(name, "list_char") == 0 ||
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
    ValueType element_type;

    if (call == NULL || call->child_count != 3) {
        semantic_error("malformed method call");
    }

    receiver = call->children[0];
    receiver_type = semantic_infer_primary_type(info, receiver, scope);
    if (!semantic_type_is_list(receiver_type)) {
        semantic_error_at_node(receiver, "method '%s' requires list receiver", method->value);
    }
    element_type = semantic_list_element_type(receiver_type);

    if (strcmp(method->value, "append") == 0) {
        ValueType arg_type;

        expect_argument_count(method->value, arguments, 1);
        arg_type = semantic_infer_expression_type_with_hint(
            info,
            arguments->children[0],
            scope,
            element_type);
        if (!semantic_is_assignable(element_type, arg_type)) {
            semantic_error_at_node(arguments->children[0], "method 'append' expects %s argument", semantic_type_name(element_type));
        }
        semantic_record_node_type(info, call, TYPE_NONE);
        return TYPE_NONE;
    }

    if (strcmp(method->value, "pop") == 0) {
        expect_argument_count(method->value, arguments, 0);
        semantic_record_node_type(info, call, element_type);
        return element_type;
    }

    if (strcmp(method->value, "clear") == 0) {
        expect_argument_count(method->value, arguments, 0);
        semantic_record_node_type(info, call, TYPE_NONE);
        return TYPE_NONE;
    }

    if (strcmp(method->value, "copy") == 0) {
        expect_argument_count(method->value, arguments, 0);
        semantic_record_node_type(info, call, receiver_type);
        return receiver_type;
    }

    semantic_error_at_node(method, "unknown list method '%s'", method->value);
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

    if (strcmp(name, "list_float") == 0) {
        expect_argument_count(name, arguments, 0);
        semantic_record_node_type(info, call, TYPE_LIST_FLOAT);
        return TYPE_LIST_FLOAT;
    }

    if (strcmp(name, "list_bool") == 0) {
        expect_argument_count(name, arguments, 0);
        semantic_record_node_type(info, call, TYPE_LIST_BOOL);
        return TYPE_LIST_BOOL;
    }

    if (strcmp(name, "list_char") == 0) {
        expect_argument_count(name, arguments, 0);
        semantic_record_node_type(info, call, TYPE_LIST_CHAR);
        return TYPE_LIST_CHAR;
    }

    if (strcmp(name, "list_append") == 0) {
        ValueType list_type;
        ValueType arg_type;

        expect_argument_count(name, arguments, 2);
        list_type = semantic_infer_expression_type(info, arguments->children[0], scope);
        if (!semantic_type_is_list(list_type)) {
            semantic_error_at_node(arguments->children[0], "function 'list_append' argument 1 expects list");
        }
        arg_type = semantic_infer_expression_type_with_hint(
            info,
            arguments->children[1],
            scope,
            semantic_list_element_type(list_type));
        if (!semantic_is_assignable(semantic_list_element_type(list_type), arg_type)) {
            semantic_error_at_node(arguments->children[1], "function 'list_append' argument 2 expects %s",
                semantic_type_name(semantic_list_element_type(list_type)));
        }
        semantic_record_node_type(info, call, TYPE_NONE);
        return TYPE_NONE;
    }

    if (strcmp(name, "list_get") == 0) {
        ValueType list_type;

        expect_argument_count(name, arguments, 2);
        list_type = semantic_infer_expression_type(info, arguments->children[0], scope);
        if (!semantic_type_is_list(list_type)) {
            semantic_error_at_node(arguments->children[0], "function 'list_get' argument 1 expects list");
        }
        if (semantic_infer_expression_type(info, arguments->children[1], scope) != TYPE_INT) {
            semantic_error_at_node(arguments->children[1], "function 'list_get' argument 2 expects int");
        }
        semantic_record_node_type(info, call, semantic_list_element_type(list_type));
        return semantic_list_element_type(list_type);
    }

    if (strcmp(name, "list_len") == 0 || strcmp(name, "len") == 0) {
        expect_argument_count(name, arguments, 1);
        if (!semantic_type_is_list(semantic_infer_expression_type(info, arguments->children[0], scope))) {
            semantic_error_at_node(arguments->children[0], "function '%s' argument 1 expects list", name);
        }
        semantic_record_node_type(info, call, TYPE_INT);
        return TYPE_INT;
    }

    if (strcmp(name, "list_set") == 0) {
        ValueType list_type;
        ValueType value_type;

        expect_argument_count(name, arguments, 3);
        list_type = semantic_infer_expression_type(info, arguments->children[0], scope);
        if (!semantic_type_is_list(list_type)) {
            semantic_error_at_node(arguments->children[0], "function 'list_set' argument 1 expects list");
        }
        if (semantic_infer_expression_type(info, arguments->children[1], scope) != TYPE_INT) {
            semantic_error_at_node(arguments->children[1], "function 'list_set' argument 2 expects int");
        }
        value_type = semantic_infer_expression_type_with_hint(
            info,
            arguments->children[2],
            scope,
            semantic_list_element_type(list_type));
        if (!semantic_is_assignable(semantic_list_element_type(list_type), value_type)) {
            semantic_error_at_node(arguments->children[2], "function 'list_set' argument 3 expects %s",
                semantic_type_name(semantic_list_element_type(list_type)));
        }
        semantic_record_node_type(info, call, TYPE_NONE);
        return TYPE_NONE;
    }

    semantic_error_at_node(call, "unknown builtin '%s'", name);
    return TYPE_NONE;
}

static ValueType infer_call_type(
    SemanticInfo *info,
    const ParseNode *call,
    Scope *scope)
{
    const ParseNode *callee = semantic_expect_child(call, 0, NODE_PRIMARY);
    const ParseNode *arguments = semantic_expect_child(call, 1, NODE_ARGUMENTS);
    ValueType class_type;

    if (is_print_call(call)) {
        if (arguments->child_count == 1 && semantic_is_epsilon_node(arguments->children[0])) {
            semantic_record_node_type(info, call, TYPE_NONE);
            return TYPE_NONE;
        }
        if (arguments->child_count != 1) {
            semantic_error_at_node(call, "print expects zero or one argument");
        }

        ValueType arg_type = semantic_infer_expression_type(info, arguments->children[0], scope);
        if (arg_type == TYPE_NONE) {
            semantic_error_at_node(arguments->children[0], "print cannot print None");
        }
        if (semantic_type_is_ref(arg_type)) {
            semantic_error_at_node(arguments->children[0], "print does not support %s yet", semantic_type_name(arg_type));
        }
        if (semantic_type_is_class(arg_type)) {
            semantic_error_at_node(arguments->children[0], "print does not support %s yet", semantic_type_name(arg_type));
        }

        semantic_record_node_type(info, call, TYPE_NONE);
        return TYPE_NONE;
    }

    if (is_builtin_name(callee->value)) {
        return infer_builtin_call_type(info, callee->value, call, arguments, scope);
    }

    class_type = semantic_find_class_type(callee->value);
    if (class_type != 0) {
        return infer_class_constructor_type(info, class_type, call, arguments, scope);
    }

    FunctionInfo *fn = semantic_find_function(info->functions, callee->value);
    if (fn == NULL) {
        semantic_error_at_node(callee, "unknown function '%s'", callee->value);
    }

    if (arguments->child_count == 1 && semantic_is_epsilon_node(arguments->children[0])) {
        if (fn->param_count != 0) {
            semantic_error_at_node(call, "function '%s' expects %zu arguments", fn->name, fn->param_count);
        }
        semantic_record_node_type(info, call, fn->return_type);
        return fn->return_type;
    }

    if (arguments->child_count != fn->param_count) {
        semantic_error_at_node(call, "function '%s' expects %zu arguments", fn->name, fn->param_count);
    }

    for (size_t i = 0; i < arguments->child_count; i++) {
        ValueType actual = semantic_infer_expression_type_with_hint(
            info,
            arguments->children[i],
            scope,
            fn->param_types[i]);
        if (!semantic_is_assignable(fn->param_types[i], actual)) {
            semantic_error_at_node(arguments->children[i], "function '%s' argument %zu expects %s but got %s",
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
        int saw_float = 0;
        int saw_bool = 0;
        int saw_char = 0;
        int saw_int = 0;

        for (size_t i = 0; i < node->child_count; i++) {
            ValueType item_type = semantic_infer_expression_type(info, node->children[i], scope);

            if (item_type == TYPE_FLOAT) {
                saw_float = 1;
                continue;
            }
            if (item_type == TYPE_BOOL) {
                saw_bool = 1;
                continue;
            }
            if (item_type == TYPE_CHAR) {
                saw_char = 1;
                continue;
            }
            if (item_type == TYPE_INT) {
                saw_int = 1;
                continue;
            }
            semantic_error_at_node(node->children[i], "list literals currently support only int, float, bool, and char elements");
        }

        if ((saw_float && saw_bool) || (saw_bool && saw_int)) {
            semantic_error_at_node(node, "list literals cannot mix bool elements with numeric elements");
        }
        if (saw_char && (saw_int || saw_float || saw_bool)) {
            semantic_error_at_node(node, "list literals cannot mix char elements with non-char elements");
        }
        if (saw_bool && node->child_count > 0) {
            semantic_record_node_type(info, node, TYPE_LIST_BOOL);
            return TYPE_LIST_BOOL;
        }
        if (saw_char && node->child_count > 0) {
            semantic_record_node_type(info, node, TYPE_LIST_CHAR);
            return TYPE_LIST_CHAR;
        }

        semantic_record_node_type(info, node, saw_float ? TYPE_LIST_FLOAT : TYPE_LIST_INT);
        return saw_float ? TYPE_LIST_FLOAT : TYPE_LIST_INT;
    }

    if (node->kind == NODE_TUPLE_LITERAL) {
        ValueType element_types[MAX_TUPLE_ELEMENTS];

        if (node->child_count < 2) {
            semantic_error_at_node(node, "tuple literals must have at least two elements");
        }
        if (node->child_count > MAX_TUPLE_ELEMENTS) {
            semantic_error_at_node(node, "tuple literals support at most %d elements", MAX_TUPLE_ELEMENTS);
        }

        for (size_t i = 0; i < node->child_count; i++) {
            ValueType item_type = semantic_infer_expression_type(info, node->children[i], scope);

            if (!tuple_element_type_supported(item_type)) {
                semantic_error_at_node(node->children[i],
                    "tuple elements currently support only int, float, bool, char, and str");
            }
            element_types[i] = item_type;
        }

        type = semantic_make_tuple_type(element_types, node->child_count);
        semantic_record_node_type(info, node, type);
        return type;
    }

    if (node->kind == NODE_METHOD_CALL) {
        type = infer_method_call_type(info, node, scope);
        semantic_record_node_type(info, node, type);
        return type;
    }

    if (node->kind == NODE_FIELD_ACCESS) {
        const ParseNode *base = node->children[0];
        const ParseNode *field = semantic_expect_child(node, 1, NODE_PRIMARY);
        ValueType base_type = semantic_infer_primary_type(info, base, scope);

        if (!semantic_type_is_class(base_type)) {
            semantic_error_at_node(base, "field access requires class value but got %s",
                semantic_type_name(base_type));
        }

        for (size_t i = 0; i < semantic_class_field_count(base_type); i++) {
            if (strcmp(semantic_class_field_name(base_type, i), field->value) == 0) {
                type = semantic_class_field_type(base_type, i);
                semantic_record_node_type(info, field, type);
                semantic_record_node_type(info, node, type);
                return type;
            }
        }

        semantic_error_at_node(field, "class '%s' has no field '%s'",
            semantic_class_name(base_type),
            field->value);
    }

    if (node->kind == NODE_INDEX) {
        ValueType container_type = semantic_infer_primary_type(info, node->children[0], scope);
        ValueType index_type = semantic_infer_expression_type(info, node->children[1], scope);
        size_t tuple_index;

        if (semantic_type_is_tuple(container_type)) {
            if (!semantic_tuple_literal_index(node->children[1], &tuple_index)) {
                semantic_error_at_node(node->children[1], "tuple index must be a non-negative integer literal");
            }
            if (tuple_index >= semantic_tuple_element_count(container_type)) {
                semantic_error_at_node(node->children[1], "tuple index %zu is out of bounds", tuple_index);
            }

            semantic_record_node_type(info, node, semantic_tuple_element_type(container_type, tuple_index));
            return semantic_tuple_element_type(container_type, tuple_index);
        }

        if (!semantic_type_is_list(container_type)) {
            semantic_error_at_node(node->children[0], "indexing requires list or tuple but got %s",
                semantic_type_name(container_type));
        }
        if (index_type != TYPE_INT) {
            semantic_error_at_node(node->children[1], "list index must be int");
        }

        semantic_record_node_type(info, node, semantic_list_element_type(container_type));
        return semantic_list_element_type(container_type);
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
            semantic_error_at_node(node, "unexpected keyword '%s' in expression", node->value);
            break;
        case TOKEN_IDENTIFIER: {
            VariableBinding *var = semantic_find_variable(scope, node->value);
            if (var == NULL) {
                semantic_error_at_node(node, "unknown variable '%s'", node->value);
            }
            type = var->type;
            break;
        }
        default:
            semantic_error_at_node(node, "unsupported primary token");
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

static void typecheck_comparison_operands(
    const ParseNode *operator_node,
    ValueType lhs_type,
    ValueType rhs_type,
    const char *op)
{
    if (semantic_type_is_union(lhs_type) || semantic_type_is_union(rhs_type)) {
        semantic_error_at_node(operator_node, "operator '%s' does not support union operands yet", op);
    }
    if (semantic_type_is_tuple(lhs_type) || semantic_type_is_tuple(rhs_type)) {
        semantic_error_at_node(operator_node, "operator '%s' does not support tuple operands", op);
    }
    if (semantic_type_is_class(lhs_type) || semantic_type_is_class(rhs_type)) {
        semantic_error_at_node(operator_node, "operator '%s' does not support class operands", op);
    }
    if (semantic_type_is_ref(lhs_type) || semantic_type_is_ref(rhs_type)) {
        semantic_error_at_node(operator_node, "operator '%s' does not support %s operands",
            op,
            semantic_type_is_ref(lhs_type) ? semantic_type_name(lhs_type) : semantic_type_name(rhs_type));
    }

    if (is_comparison_operator(op)) {
        if (!semantic_is_numeric_type(lhs_type) || !semantic_is_numeric_type(rhs_type)) {
            semantic_error_at_node(operator_node, "comparison '%s' requires numeric operands", op);
        }
        return;
    }

    if (is_equality_operator(op)) {
        if (lhs_type == TYPE_STR || rhs_type == TYPE_STR) {
            semantic_error_at_node(operator_node, "str equality is not supported yet");
        }
        if (!(semantic_is_assignable(lhs_type, rhs_type) || semantic_is_assignable(rhs_type, lhs_type))) {
            semantic_error_at_node(operator_node, "operator '%s' requires comparable operands", op);
        }
        return;
    }

    semantic_error_at_node(operator_node, "unsupported operator '%s'", op);
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
                semantic_error_at_node(operator_node, "operator 'not' requires bool operands");
            }
            semantic_record_node_type(info, expr, TYPE_BOOL);
            return TYPE_BOOL;
        }

        if (semantic_type_is_union(rhs_type)) {
            semantic_error_at_node(operator_node, "operator '%s' does not support union operands yet", operator_node->value);
        }
        if (semantic_type_is_tuple(rhs_type)) {
            semantic_error_at_node(operator_node, "operator '%s' does not support tuple operands", operator_node->value);
        }
        if (semantic_type_is_class(rhs_type)) {
            semantic_error_at_node(operator_node, "operator '%s' does not support class operands", operator_node->value);
        }

        if (!is_arithmetic_operator(operator_node->value)) {
            semantic_error_at_node(operator_node, "unsupported unary operator '%s'", operator_node->value);
        }
        if (!semantic_is_numeric_type(rhs_type)) {
            semantic_error_at_node(operator_node, "operator '%s' requires numeric operands", operator_node->value);
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
                semantic_error_at_node(chain_op, "unsupported operator '%s' in comparison chain", chain_op->value);
            }
            typecheck_comparison_operands(chain_op, chain_lhs, chain_rhs, chain_op->value);
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
            semantic_error_at_node(operator_node, "operator '%s' does not support union operands yet", operator_node->value);
        }
        if (!semantic_is_numeric_type(lhs_type) || !semantic_is_numeric_type(rhs_type)) {
            semantic_error_at_node(operator_node, "operator '%s' requires numeric operands", operator_node->value);
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
            semantic_error_at_node(operator_node, "operator '%s' requires bool operands", operator_node->value);
        }

        semantic_record_node_type(info, expr, TYPE_BOOL);
        return TYPE_BOOL;
    }

    if (is_comparison_operator(operator_node->value) || is_equality_operator(operator_node->value)) {
        typecheck_comparison_operands(operator_node, lhs_type, rhs_type, operator_node->value);
        semantic_record_node_type(info, expr, TYPE_BOOL);
        return TYPE_BOOL;
    }

    semantic_error_at_node(operator_node, "unsupported operator '%s'", operator_node->value);
    return TYPE_NONE;
}

ValueType semantic_infer_expression_type_with_hint(
    SemanticInfo *info,
    const ParseNode *expr,
    Scope *scope,
    ValueType expected_type)
{
    ValueType type;

    if (semantic_type_is_list(expected_type) && is_direct_list_literal_expression(expr)) {
        type = infer_list_literal_with_hint(info, expr->children[0], scope, expected_type);
        semantic_record_node_type(info, expr, type);
        return type;
    }

    if (semantic_type_is_tuple(expected_type) && is_direct_tuple_literal_expression(expr)) {
        type = infer_tuple_literal_with_hint(info, expr->children[0], scope, expected_type);
        semantic_record_node_type(info, expr, type);
        return type;
    }

    return semantic_infer_expression_type(info, expr, scope);
}
