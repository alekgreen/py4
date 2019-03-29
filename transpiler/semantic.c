#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic.h"

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
    ValueType *param_types;
    const ParseNode *node;
    struct FunctionInfo *next;
} FunctionInfo;

typedef struct {
    const char *name;
    ValueType return_type;
    int saw_return;
} FunctionContext;

typedef struct NodeTypeInfo {
    const ParseNode *node;
    ValueType type;
    struct NodeTypeInfo *next;
} NodeTypeInfo;

struct SemanticInfo {
    FunctionInfo *functions;
    NodeTypeInfo *node_types;
};

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

static int is_if_statement(const ParseNode *node)
{
    return node->kind == NODE_IF_STATEMENT;
}

static int is_epsilon_node(const ParseNode *node)
{
    return node->kind == NODE_EPSILON;
}

static int count_type_bits(ValueType type)
{
    int count = 0;

    while (type != 0) {
        count += (type & 1u) != 0;
        type >>= 1u;
    }

    return count;
}

int semantic_type_contains(ValueType type, ValueType member)
{
    return member != 0 && (type & member) == member;
}

int semantic_type_is_union(ValueType type)
{
    return count_type_bits(type) > 1;
}

static const char *type_member_name(ValueType type)
{
    switch (type) {
        case TYPE_INT: return "int";
        case TYPE_FLOAT: return "float";
        case TYPE_BOOL: return "bool";
        case TYPE_CHAR: return "char";
        case TYPE_STR: return "str";
        case TYPE_NONE: return "None";
        default: return "unknown";
    }
}

const char *semantic_type_name(ValueType type)
{
    static char buffers[8][96];
    static int next_buffer = 0;
    char *buffer;
    size_t used = 0;
    int first = 1;
    const ValueType ordered_types[] = {
        TYPE_INT,
        TYPE_FLOAT,
        TYPE_BOOL,
        TYPE_CHAR,
        TYPE_STR,
        TYPE_NONE
    };

    if (type == TYPE_INT || type == TYPE_FLOAT || type == TYPE_BOOL ||
        type == TYPE_CHAR || type == TYPE_STR || type == TYPE_NONE) {
        return type_member_name(type);
    }

    buffer = buffers[next_buffer];
    next_buffer = (next_buffer + 1) % 8;
    buffer[0] = '\0';

    for (size_t i = 0; i < sizeof(ordered_types) / sizeof(ordered_types[0]); i++) {
        if (!semantic_type_contains(type, ordered_types[i])) {
            continue;
        }

        used += snprintf(buffer + used, sizeof(buffers[0]) - used, "%s%s",
            first ? "" : " | ",
            type_member_name(ordered_types[i]));
        first = 0;
    }

    if (first) {
        snprintf(buffer, sizeof(buffers[0]), "unknown");
    }

    return buffer;
}

static int is_numeric_type(ValueType type)
{
    return type == TYPE_INT || type == TYPE_FLOAT;
}

static int is_assignable(ValueType target, ValueType value)
{
    const ValueType source_members[] = {
        TYPE_INT,
        TYPE_FLOAT,
        TYPE_BOOL,
        TYPE_CHAR,
        TYPE_STR,
        TYPE_NONE
    };

    if (target == 0 || value == 0) {
        return 0;
    }

    for (size_t i = 0; i < sizeof(source_members) / sizeof(source_members[0]); i++) {
        ValueType member = source_members[i];

        if (!semantic_type_contains(value, member)) {
            continue;
        }
        if (semantic_type_contains(target, member)) {
            continue;
        }
        if (member == TYPE_INT && semantic_type_contains(target, TYPE_FLOAT)) {
            continue;
        }
        return 0;
    }

    return 1;
}

static ValueType parse_type_atom_name(const char *name)
{
    if (strcmp(name, "int") == 0) {
        return TYPE_INT;
    }
    if (strcmp(name, "float") == 0) {
        return TYPE_FLOAT;
    }
    if (strcmp(name, "bool") == 0) {
        return TYPE_BOOL;
    }
    if (strcmp(name, "char") == 0) {
        return TYPE_CHAR;
    }
    if (strcmp(name, "str") == 0 || strcmp(name, "string") == 0) {
        return TYPE_STR;
    }
    if (strcmp(name, "None") == 0) {
        return TYPE_NONE;
    }

    semantic_error("unsupported type '%s'", name);
    return 0;
}

static void record_node_type(SemanticInfo *info, const ParseNode *node, ValueType type)
{
    for (NodeTypeInfo *curr = info->node_types; curr != NULL; curr = curr->next) {
        if (curr->node == node) {
            curr->type = type;
            return;
        }
    }

    NodeTypeInfo *entry = malloc(sizeof(NodeTypeInfo));
    if (entry == NULL) {
        perror("malloc");
        exit(1);
    }

    entry->node = node;
    entry->type = type;
    entry->next = info->node_types;
    info->node_types = entry;
}

ValueType semantic_type_of(const SemanticInfo *info, const ParseNode *node)
{
    for (NodeTypeInfo *curr = info->node_types; curr != NULL; curr = curr->next) {
        if (curr->node == node) {
            return curr->type;
        }
    }

    semantic_error("missing semantic type information");
    return 0;
}

static ValueType parse_type_node(SemanticInfo *info, const ParseNode *type_node)
{
    ValueType type = 0;

    if (type_node == NULL || type_node->kind != NODE_TYPE || type_node->child_count == 0) {
        semantic_error("malformed type annotation");
    }

    for (size_t i = 0; i < type_node->child_count; i++) {
        const ParseNode *member = expect_child(type_node, i, NODE_PRIMARY);
        ValueType atom = parse_type_atom_name(member->value);

        if (semantic_type_contains(type, atom)) {
            semantic_error("duplicate type '%s' in union", member->value);
        }

        record_node_type(info, member, atom);
        type |= atom;
    }

    record_node_type(info, type_node, type);
    return type;
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
    for (VariableBinding *var = scope->vars; var != NULL; var = var->next) {
        if (strcmp(var->name, name) == 0) {
            if (var->type != type) {
                semantic_error("cannot redefine '%s' from %s to %s",
                    name, semantic_type_name(var->type), semantic_type_name(type));
            }
            return;
        }
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
    SemanticInfo *info,
    const ParseNode *expr,
    Scope *scope);

static const ParseNode *function_parameters(const ParseNode *function_def)
{
    return expect_child(function_def, 1, NODE_PARAMETERS);
}

static ValueType function_return_type(SemanticInfo *info, const ParseNode *function_def)
{
    if (function_def->child_count >= 3 && function_def->children[2]->kind == NODE_RETURN_TYPE) {
        const ParseNode *type_node = expect_child(function_def->children[2], 0, NODE_TYPE);
        return parse_type_node(info, type_node);
    }

    return TYPE_NONE;
}

static size_t parameter_count(const ParseNode *parameters)
{
    if (parameters->child_count == 1 && is_epsilon_node(parameters->children[0])) {
        return 0;
    }
    return parameters->child_count;
}

static ValueType parameter_type(SemanticInfo *info, const ParseNode *parameter)
{
    return parse_type_node(info, expect_child(parameter, 0, NODE_TYPE));
}

static int is_print_call(const ParseNode *call)
{
    const ParseNode *callee = expect_child(call, 0, NODE_PRIMARY);

    return callee->value != NULL && strcmp(callee->value, "print") == 0;
}

static ValueType infer_call_type(
    SemanticInfo *info,
    const ParseNode *call,
    Scope *scope)
{
    const ParseNode *callee = expect_child(call, 0, NODE_PRIMARY);
    const ParseNode *arguments = expect_child(call, 1, NODE_ARGUMENTS);

    if (is_print_call(call)) {
        if (arguments->child_count == 1 && is_epsilon_node(arguments->children[0])) {
            record_node_type(info, call, TYPE_NONE);
            return TYPE_NONE;
        }
        if (arguments->child_count != 1) {
            semantic_error("print expects zero or one argument");
        }

        ValueType arg_type = infer_expression_type(info, arguments->children[0], scope);
        if (arg_type == TYPE_NONE) {
            semantic_error("print cannot print None");
        }

        record_node_type(info, call, TYPE_NONE);
        return TYPE_NONE;
    }

    FunctionInfo *fn = find_function(info->functions, callee->value);
    if (fn == NULL) {
        semantic_error("unknown function '%s'", callee->value);
    }

    if (arguments->child_count == 1 && is_epsilon_node(arguments->children[0])) {
        if (fn->param_count != 0) {
            semantic_error("function '%s' expects %zu arguments", fn->name, fn->param_count);
        }
        record_node_type(info, call, fn->return_type);
        return fn->return_type;
    }

    if (arguments->child_count != fn->param_count) {
        semantic_error("function '%s' expects %zu arguments", fn->name, fn->param_count);
    }

    for (size_t i = 0; i < arguments->child_count; i++) {
        ValueType actual = infer_expression_type(info, arguments->children[i], scope);
        if (!is_assignable(fn->param_types[i], actual)) {
            semantic_error("function '%s' argument %zu expects %s but got %s",
                fn->name, i + 1,
                semantic_type_name(fn->param_types[i]),
                semantic_type_name(actual));
        }
    }

    record_node_type(info, call, fn->return_type);
    return fn->return_type;
}

static ValueType infer_primary_type(
    SemanticInfo *info,
    const ParseNode *node,
    Scope *scope)
{
    ValueType type;

    if (node->kind == NODE_CALL) {
        type = infer_call_type(info, node, scope);
        record_node_type(info, node, type);
        return type;
    }

    if (node->kind == NODE_EXPRESSION) {
        type = infer_expression_type(info, node, scope);
        record_node_type(info, node, type);
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
            VariableBinding *var = find_variable(scope, node->value);
            if (var == NULL) {
                semantic_error("unknown variable '%s'", node->value);
            }
            type = var->type;
            break;
        }
        default:
            semantic_error("unsupported primary token");
    }

    record_node_type(info, node, type);
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

static ValueType infer_expression_type(
    SemanticInfo *info,
    const ParseNode *expr,
    Scope *scope)
{
    const char *op = NULL;
    ValueType type;

    if (expr->kind != NODE_EXPRESSION || expr->child_count == 0) {
        semantic_error("malformed expression");
    }

    type = infer_primary_type(info, expr->children[0], scope);
    if (expr->child_count == 1) {
        record_node_type(info, expr, type);
        return type;
    }

    for (size_t i = 1; i < expr->child_count; i++) {
        const ParseNode *child = expr->children[i];

        if (child->kind == NODE_OPERATOR) {
            op = child->value;
            continue;
        }

        ValueType rhs = infer_primary_type(info, child, scope);

        if (op == NULL) {
            semantic_error("missing operator in expression");
        }

        if (semantic_type_is_union(type) || semantic_type_is_union(rhs)) {
            semantic_error("operator '%s' does not support union operands yet", op);
        }

        if (is_arithmetic_operator(op)) {
            if (!is_numeric_type(type) || !is_numeric_type(rhs)) {
                semantic_error("operator '%s' requires numeric operands", op);
            }
            if (strcmp(op, "/") == 0 || type == TYPE_FLOAT || rhs == TYPE_FLOAT) {
                type = TYPE_FLOAT;
            } else {
                type = TYPE_INT;
            }
            continue;
        }

        if (is_comparison_operator(op)) {
            if (!is_numeric_type(type) || !is_numeric_type(rhs)) {
                semantic_error("comparison '%s' requires numeric operands", op);
            }
            type = TYPE_BOOL;
            continue;
        }

        if (is_equality_operator(op)) {
            if (type == TYPE_STR || rhs == TYPE_STR) {
                semantic_error("str equality is not supported yet");
            }
            if (!(is_assignable(type, rhs) || is_assignable(rhs, type))) {
                semantic_error("operator '%s' requires comparable operands", op);
            }
            type = TYPE_BOOL;
            continue;
        }

        semantic_error("unsupported operator '%s'", op);
    }

    record_node_type(info, expr, type);
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

static const ParseNode *statement_tail_type_node(const ParseNode *statement_tail)
{
    return expect_child(statement_tail, 1, NODE_TYPE);
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
    if (!(suite->child_count == 1 && is_epsilon_node(suite->children[0]))) {
        for (size_t i = 0; i < suite->child_count; i++) {
            typecheck_statement(info, suite->children[i], &branch_scope, current_function, 0);
        }
    }

    free_scope_bindings(branch_scope.vars);
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

    if (is_epsilon_node(return_stmt->children[0])) {
        if (!semantic_type_contains(current_function->return_type, TYPE_NONE)) {
            semantic_error("function '%s' must return %s",
                current_function->name,
                semantic_type_name(current_function->return_type));
        }
        return;
    }

    ValueType expr_type = infer_expression_type(info, return_stmt->children[0], scope);
    if (!is_assignable(current_function->return_type, expr_type)) {
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
        infer_expression_type(info, expect_child(first_child, 0, NODE_EXPRESSION), scope);
        return;
    }

    if (simple_stmt->child_count != 2) {
        semantic_error("malformed simple statement");
    }

    const ParseNode *name = simple_statement_name(simple_stmt);
    const ParseNode *statement_tail = simple_statement_tail(simple_stmt);
    const ParseNode *expr = statement_tail_expression(statement_tail);
    ValueType expr_type = infer_expression_type(info, expr, scope);

    if (is_type_assignment(statement_tail)) {
        ValueType declared = parse_type_node(info, statement_tail_type_node(statement_tail));
        if (!is_assignable(declared, expr_type)) {
            semantic_error("cannot assign %s to %s '%s'",
                semantic_type_name(expr_type),
                semantic_type_name(declared),
                name->value);
        }
        bind_variable(scope, name->value, declared);
        record_node_type(info, name, declared);
        return;
    }

    VariableBinding *var = find_variable(scope, name->value);
    if (var == NULL) {
        semantic_error("assignment to undeclared variable '%s'", name->value);
    }
    if (!is_assignable(var->type, expr_type)) {
        semantic_error("cannot assign %s to %s '%s'",
            semantic_type_name(expr_type),
            semantic_type_name(var->type),
            name->value);
    }

    record_node_type(info, name, var->type);
}

static void typecheck_suite(
    SemanticInfo *info,
    const ParseNode *suite,
    Scope *scope,
    FunctionContext *current_function)
{
    if (suite->child_count == 1 && is_epsilon_node(suite->children[0])) {
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

static void collect_functions(SemanticInfo *info, const ParseNode *root)
{
    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = statement_payload(root->children[i]);

        if (payload->kind != NODE_FUNCTION_DEF) {
            continue;
        }

        const char *name = expect_child(payload, 0, NODE_PRIMARY)->value;
        if (find_function(info->functions, name) != NULL) {
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
            fn->param_types[j] = parameter_type(info, expect_child(parameters, j, NODE_PARAMETER));
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
    current.name = expect_child(function_def, 0, NODE_PRIMARY)->value;
    current.return_type = function_return_type(info, function_def);

    if (!(parameters->child_count == 1 && is_epsilon_node(parameters->children[0]))) {
        for (size_t i = 0; i < parameters->child_count; i++) {
            const ParseNode *param = expect_child(parameters, i, NODE_PARAMETER);
            bind_variable(&local_scope, param->value, parameter_type(info, param));
        }
    }

    typecheck_suite(info, suite, &local_scope, &current);

    if (current.return_type != TYPE_NONE && !current.saw_return) {
        semantic_error("function '%s' must return %s",
            current.name, semantic_type_name(current.return_type));
    }

    free_scope_bindings(local_scope.vars);
}

static void typecheck_statement(
    SemanticInfo *info,
    const ParseNode *statement,
    Scope *scope,
    FunctionContext *current_function,
    int allow_function_defs)
{
    const ParseNode *payload = statement_payload(statement);

    if (payload->kind == NODE_FUNCTION_DEF) {
        if (!allow_function_defs) {
            semantic_error("nested function definitions are not supported");
        }
        typecheck_function(info, payload, scope);
        return;
    }

    if (is_if_statement(payload)) {
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
        const ParseNode *payload = statement_payload(root->children[i]);

        if (payload->kind == NODE_FUNCTION_DEF) {
            typecheck_function(info, payload, &global_scope);
        } else {
            typecheck_statement(info, root->children[i], &global_scope, NULL, 0);
        }
    }

    free_scope_bindings(global_scope.vars);
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
