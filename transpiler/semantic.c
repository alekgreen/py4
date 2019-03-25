#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic.h"

typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_NONE
} ValueType;

typedef struct VariableBinding {
    const char *name;
    ValueType type;
    struct VariableBinding *next;
} VariableBinding;

typedef struct Scope {
    VariableBinding *vars;
    struct Scope *parent;
} Scope;

typedef struct FunctionInfo {
    const char *name;
    ValueType return_type;
    size_t param_count;
    const ParseNode *node;
    struct FunctionInfo *next;
} FunctionInfo;

typedef struct {
    const char *name;
    ValueType return_type;
    int is_main;
    int saw_return;
} FunctionContext;

static void semantic_error(const char *message, ...)
{
    va_list args;

    fprintf(stderr, "Type error: ");
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(1);
}

static const ParseNode *expect_child(const ParseNode *node, size_t index, NodeKind kind)
{
    if (node == NULL || index >= node->child_count || node->children[index]->kind != kind) {
        semantic_error("malformed AST");
    }
    return node->children[index];
}

static const ParseNode *statement_payload(const ParseNode *statement)
{
    if (statement == NULL || statement->kind != NODE_STATEMENT || statement->child_count != 1) {
        semantic_error("malformed statement node");
    }
    return statement->children[0];
}

static ValueType parse_type_name(const char *name)
{
    if (strcmp(name, "int") == 0) {
        return TYPE_INT;
    }
    if (strcmp(name, "float") == 0) {
        return TYPE_FLOAT;
    }
    if (strcmp(name, "None") == 0) {
        return TYPE_NONE;
    }
    semantic_error("unsupported type '%s'", name);
    return TYPE_NONE;
}

static const char *type_name(ValueType type)
{
    switch (type) {
        case TYPE_INT: return "int";
        case TYPE_FLOAT: return "float";
        case TYPE_NONE: return "None";
        default: return "unknown";
    }
}

static int is_numeric_type(ValueType type)
{
    return type == TYPE_INT || type == TYPE_FLOAT;
}

static int is_assignable(ValueType target, ValueType value)
{
    if (target == value) {
        return 1;
    }
    return target == TYPE_FLOAT && value == TYPE_INT;
}

static int is_epsilon_node(const ParseNode *node)
{
    return node->kind == NODE_EPSILON;
}

static FunctionInfo *find_function(FunctionInfo *functions, const char *name)
{
    for (FunctionInfo *fn = functions; fn != NULL; fn = fn->next) {
        if (strcmp(fn->name, name) == 0) {
            return fn;
        }
    }
    return NULL;
}

static VariableBinding *find_variable(Scope *scope, const char *name)
{
    for (Scope *curr = scope; curr != NULL; curr = curr->parent) {
        for (VariableBinding *var = curr->vars; var != NULL; var = var->next) {
            if (strcmp(var->name, name) == 0) {
                return var;
            }
        }
    }
    return NULL;
}

static void bind_variable(Scope *scope, const char *name, ValueType type)
{
    VariableBinding *existing = NULL;

    for (VariableBinding *var = scope->vars; var != NULL; var = var->next) {
        if (strcmp(var->name, name) == 0) {
            existing = var;
            break;
        }
    }

    if (existing != NULL) {
        if (existing->type != type) {
            semantic_error("cannot redefine '%s' from %s to %s",
                name, type_name(existing->type), type_name(type));
        }
        return;
    }

    VariableBinding *binding = malloc(sizeof(VariableBinding));
    if (binding == NULL) {
        perror("malloc");
        exit(1);
    }
    binding->name = name;
    binding->type = type;
    binding->next = scope->vars;
    scope->vars = binding;
}

static void free_scope_bindings(VariableBinding *vars)
{
    while (vars != NULL) {
        VariableBinding *next = vars->next;
        free(vars);
        vars = next;
    }
}

static ValueType infer_expression_type(
    const ParseNode *expr,
    Scope *scope,
    FunctionInfo *functions);

static int is_print_call(const ParseNode *call)
{
    const ParseNode *callee = expect_child(call, 0, NODE_PRIMARY);

    return callee->value != NULL && strcmp(callee->value, "print") == 0;
}

static const ParseNode *function_parameter(const ParseNode *function_def, size_t index)
{
    const ParseNode *parameters = expect_child(function_def, 1, NODE_PARAMETERS);

    return expect_child(parameters, index, NODE_PARAMETER);
}

static ValueType parameter_type(const ParseNode *function_def, size_t index)
{
    const ParseNode *param = function_parameter(function_def, index);
    const ParseNode *type_node = expect_child(param, 0, NODE_PRIMARY);

    return parse_type_name(type_node->value);
}

static ValueType infer_call_type(
    const ParseNode *call,
    Scope *scope,
    FunctionInfo *functions)
{
    const ParseNode *callee = expect_child(call, 0, NODE_PRIMARY);
    const ParseNode *arguments = expect_child(call, 1, NODE_ARGUMENTS);

    if (is_print_call(call)) {
        if (arguments->child_count == 1 && is_epsilon_node(arguments->children[0])) {
            return TYPE_NONE;
        }
        if (arguments->child_count != 1) {
            semantic_error("print expects zero or one argument");
        }
        if (!is_numeric_type(infer_expression_type(arguments->children[0], scope, functions))) {
            semantic_error("print argument must be numeric");
        }
        return TYPE_NONE;
    }

    FunctionInfo *fn = find_function(functions, callee->value);
    if (fn == NULL) {
        semantic_error("unknown function '%s'", callee->value);
    }

    if (arguments->child_count == 1 && is_epsilon_node(arguments->children[0])) {
        if (fn->param_count != 0) {
            semantic_error("function '%s' expects %zu arguments", fn->name, fn->param_count);
        }
        return fn->return_type;
    }

    if (arguments->child_count != fn->param_count) {
        semantic_error("function '%s' expects %zu arguments", fn->name, fn->param_count);
    }

    for (size_t i = 0; i < arguments->child_count; i++) {
        ValueType expected = parameter_type(fn->node, i);
        ValueType actual = infer_expression_type(arguments->children[i], scope, functions);

        if (!is_assignable(expected, actual)) {
            semantic_error("function '%s' argument %zu expects %s but got %s",
                fn->name, i + 1, type_name(expected), type_name(actual));
        }
    }

    return fn->return_type;
}

static ValueType infer_primary_type(
    const ParseNode *node,
    Scope *scope,
    FunctionInfo *functions)
{
    if (node->kind == NODE_PRIMARY) {
        if (node->token_type == TOKEN_NUMBER) {
            return strchr(node->value, '.') != NULL ? TYPE_FLOAT : TYPE_INT;
        }
        if (node->token_type == TOKEN_IDENTIFIER) {
            VariableBinding *var = find_variable(scope, node->value);
            if (var == NULL) {
                semantic_error("unknown variable '%s'", node->value);
            }
            return var->type;
        }
    }

    if (node->kind == NODE_CALL) {
        return infer_call_type(node, scope, functions);
    }

    if (node->kind == NODE_EXPRESSION) {
        return infer_expression_type(node, scope, functions);
    }

    semantic_error("unsupported primary expression");
    return TYPE_NONE;
}

static ValueType infer_expression_type(
    const ParseNode *expr,
    Scope *scope,
    FunctionInfo *functions)
{
    const char *op = NULL;
    ValueType type;

    if (expr->kind != NODE_EXPRESSION || expr->child_count == 0) {
        semantic_error("malformed expression");
    }

    type = infer_primary_type(expr->children[0], scope, functions);
    if (expr->child_count == 1) {
        return type;
    }

    for (size_t i = 1; i < expr->child_count; i++) {
        const ParseNode *child = expr->children[i];

        if (child->kind == NODE_OPERATOR) {
            op = child->value;
            continue;
        }

        ValueType rhs = infer_primary_type(child, scope, functions);
        if (!is_numeric_type(type) || !is_numeric_type(rhs)) {
            semantic_error("operators require numeric operands");
        }

        if (op != NULL &&
            (strcmp(op, "==") == 0 || strcmp(op, "!=") == 0 ||
             strcmp(op, "<") == 0 || strcmp(op, ">") == 0 ||
             strcmp(op, "<=") == 0 || strcmp(op, ">=") == 0)) {
            type = TYPE_INT;
        } else if (op != NULL && strcmp(op, "/") == 0) {
            type = TYPE_FLOAT;
        } else if (type == TYPE_FLOAT || rhs == TYPE_FLOAT) {
            type = TYPE_FLOAT;
        } else {
            type = TYPE_INT;
        }
    }

    return type;
}

static int is_type_assignment(const ParseNode *statement_tail)
{
    return statement_tail->child_count == 4 &&
        statement_tail->children[0]->kind == NODE_COLON;
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

static void typecheck_statement(
    const ParseNode *statement,
    Scope *scope,
    FunctionInfo *functions,
    FunctionContext *current_function,
    int allow_function_defs);

static void typecheck_return_statement(
    const ParseNode *return_stmt,
    Scope *scope,
    FunctionInfo *functions,
    FunctionContext *current_function)
{
    if (current_function == NULL) {
        semantic_error("return is only valid inside a function");
    }

    current_function->saw_return = 1;

    if (return_stmt->child_count != 1) {
        semantic_error("malformed return statement");
    }

    if (is_epsilon_node(return_stmt->children[0])) {
        if (current_function->return_type != TYPE_NONE) {
            semantic_error("function '%s' must return %s",
                current_function->name, type_name(current_function->return_type));
        }
        return;
    }

    ValueType expr_type = infer_expression_type(return_stmt->children[0], scope, functions);
    if (!is_assignable(current_function->return_type, expr_type)) {
        semantic_error("function '%s' returns %s but got %s",
            current_function->name,
            type_name(current_function->return_type),
            type_name(expr_type));
    }
}

static void typecheck_simple_statement(
    const ParseNode *simple_stmt,
    Scope *scope,
    FunctionInfo *functions,
    FunctionContext *current_function)
{
    const ParseNode *first_child = simple_stmt->children[0];

    if (first_child->kind == NODE_RETURN_STATEMENT) {
        typecheck_return_statement(first_child, scope, functions, current_function);
        return;
    }

    if (first_child->kind == NODE_EXPRESSION_STATEMENT) {
        infer_expression_type(expect_child(first_child, 0, NODE_EXPRESSION), scope, functions);
        return;
    }

    if (simple_stmt->child_count != 2) {
        semantic_error("malformed simple statement");
    }

    const ParseNode *name = simple_statement_name(simple_stmt);
    const ParseNode *statement_tail = simple_statement_tail(simple_stmt);
    const ParseNode *expr = statement_tail_expression(statement_tail);
    ValueType expr_type = infer_expression_type(expr, scope, functions);

    if (is_type_assignment(statement_tail)) {
        ValueType declared_type = parse_type_name(expect_child(statement_tail, 1, NODE_PRIMARY)->value);
        if (!is_assignable(declared_type, expr_type)) {
            semantic_error("cannot assign %s to %s '%s'",
                type_name(expr_type), type_name(declared_type), name->value);
        }
        bind_variable(scope, name->value, declared_type);
        return;
    }

    VariableBinding *var = find_variable(scope, name->value);
    if (var == NULL) {
        semantic_error("assignment to undeclared variable '%s'", name->value);
    }
    if (!is_assignable(var->type, expr_type)) {
        semantic_error("cannot assign %s to %s '%s'",
            type_name(expr_type), type_name(var->type), name->value);
    }
}

static void typecheck_suite(
    const ParseNode *suite,
    Scope *scope,
    FunctionInfo *functions,
    FunctionContext *current_function)
{
    if (suite->child_count == 1 && is_epsilon_node(suite->children[0])) {
        return;
    }

    for (size_t i = 0; i < suite->child_count; i++) {
        typecheck_statement(suite->children[i], scope, functions, current_function, 0);
    }
}

static size_t parameter_count(const ParseNode *parameters)
{
    if (parameters->child_count == 1 && is_epsilon_node(parameters->children[0])) {
        return 0;
    }
    return parameters->child_count;
}

static ValueType function_return_type(const ParseNode *function_def)
{
    if (function_def->child_count >= 3 && function_def->children[2]->kind == NODE_RETURN_TYPE) {
        return parse_type_name(expect_child(function_def->children[2], 0, NODE_PRIMARY)->value);
    }
    return TYPE_NONE;
}

static int is_main_function(const ParseNode *function_def)
{
    return strcmp(expect_child(function_def, 0, NODE_PRIMARY)->value, "main") == 0;
}

static void collect_functions(const ParseNode *root, FunctionInfo **functions)
{
    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = statement_payload(root->children[i]);

        if (payload->kind != NODE_FUNCTION_DEF) {
            continue;
        }

        const char *name = expect_child(payload, 0, NODE_PRIMARY)->value;
        if (find_function(*functions, name) != NULL) {
            semantic_error("duplicate function '%s'", name);
        }

        FunctionInfo *fn = malloc(sizeof(FunctionInfo));
        if (fn == NULL) {
            perror("malloc");
            exit(1);
        }

        fn->name = name;
        fn->return_type = function_return_type(payload);
        fn->param_count = parameter_count(expect_child(payload, 1, NODE_PARAMETERS));
        fn->node = payload;
        fn->next = *functions;
        *functions = fn;
    }
}

static void free_functions(FunctionInfo *functions)
{
    while (functions != NULL) {
        FunctionInfo *next = functions->next;
        free(functions);
        functions = next;
    }
}

static void typecheck_function(
    const ParseNode *function_def,
    Scope *global_scope,
    FunctionInfo *functions)
{
    const ParseNode *parameters = expect_child(function_def, 1, NODE_PARAMETERS);
    const ParseNode *suite = function_def->children[function_def->child_count - 1];
    Scope local_scope = {0};
    FunctionContext current = {0};

    local_scope.parent = global_scope;
    current.name = expect_child(function_def, 0, NODE_PRIMARY)->value;
    current.return_type = function_return_type(function_def);
    current.is_main = is_main_function(function_def);

    if (!(parameters->child_count == 1 && is_epsilon_node(parameters->children[0]))) {
        for (size_t i = 0; i < parameters->child_count; i++) {
            const ParseNode *param = expect_child(parameters, i, NODE_PARAMETER);
            ValueType param_type = parse_type_name(expect_child(param, 0, NODE_PRIMARY)->value);

            bind_variable(&local_scope, param->value, param_type);
        }
    }

    typecheck_suite(suite, &local_scope, functions, &current);

    if (current.return_type != TYPE_NONE && !current.saw_return) {
        semantic_error("function '%s' must return %s",
            current.name, type_name(current.return_type));
    }

    free_scope_bindings(local_scope.vars);
}

static void typecheck_statement(
    const ParseNode *statement,
    Scope *scope,
    FunctionInfo *functions,
    FunctionContext *current_function,
    int allow_function_defs)
{
    const ParseNode *payload = statement_payload(statement);

    if (payload->kind == NODE_FUNCTION_DEF) {
        if (!allow_function_defs) {
            semantic_error("nested function definitions are not supported");
        }
        typecheck_function(payload, scope, functions);
        return;
    }

    if (payload->kind != NODE_SIMPLE_STATEMENT) {
        semantic_error("unsupported statement node");
    }

    typecheck_simple_statement(payload, scope, functions, current_function);
}

void typecheck_program(const ParseNode *root)
{
    Scope global_scope = {0};
    FunctionInfo *functions = NULL;

    if (root == NULL || root->kind != NODE_S) {
        semantic_error("expected program root");
    }

    collect_functions(root, &functions);

    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = statement_payload(root->children[i]);

        if (payload->kind == NODE_FUNCTION_DEF) {
            typecheck_function(payload, &global_scope, functions);
        } else {
            typecheck_simple_statement(payload, &global_scope, functions, NULL);
        }
    }

    free_scope_bindings(global_scope.vars);
    free_functions(functions);
}
