#ifndef SEMANTIC_INTERNAL_H
#define SEMANTIC_INTERNAL_H

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
    char *c_name;
    ValueType return_type;
    size_t param_count;
    ValueType *param_types;
    int is_native;
    const ParseNode *node;
    struct FunctionInfo *next;
} FunctionInfo;

typedef struct CallTargetInfo {
    const ParseNode *call;
    FunctionInfo *function;
    struct CallTargetInfo *next;
} CallTargetInfo;

typedef struct MethodInfo {
    ValueType owner_type;
    const char *name;
    char *c_name;
    ValueType return_type;
    size_t param_count;
    ValueType *param_types;
    const ParseNode *node;
    struct MethodInfo *next;
} MethodInfo;

typedef struct {
    const char *name;
    ValueType return_type;
    int loop_depth;
} FunctionContext;

typedef struct NodeTypeInfo {
    const ParseNode *node;
    ValueType type;
    struct NodeTypeInfo *next;
} NodeTypeInfo;

struct SemanticInfo {
    FunctionInfo *functions;
    MethodInfo *methods;
    NodeTypeInfo *node_types;
    CallTargetInfo *call_targets;
};

void semantic_error(const char *message, ...);
void semantic_error_at_node(const ParseNode *node, const char *message, ...);
const ParseNode *semantic_expect_child(const ParseNode *node, size_t index, NodeKind kind);
const ParseNode *semantic_statement_payload(const ParseNode *statement);
int semantic_is_if_statement(const ParseNode *node);
int semantic_is_epsilon_node(const ParseNode *node);
int semantic_is_numeric_type(ValueType type);
int semantic_is_assignable(ValueType target, ValueType value);
int semantic_builtin_returns_owned_ref(const char *name);
void semantic_record_node_type(SemanticInfo *info, const ParseNode *node, ValueType type);
ValueType semantic_parse_type_node(SemanticInfo *info, const ParseNode *type_node);
ValueType semantic_find_class_type(const char *name);
ValueType semantic_register_class(const ParseNode *class_def);
void semantic_define_class_fields(SemanticInfo *info, const ParseNode *class_def);
FunctionInfo *semantic_find_function(FunctionInfo *functions, const char *name);
FunctionInfo *semantic_find_function_by_node(FunctionInfo *functions, const ParseNode *node);
void semantic_record_call_target(SemanticInfo *info, const ParseNode *call, FunctionInfo *function);
FunctionInfo *semantic_resolved_call_target(const SemanticInfo *info, const ParseNode *call);
MethodInfo *semantic_find_method(MethodInfo *methods, ValueType owner_type, const char *name);
VariableBinding *semantic_find_variable(Scope *scope, const char *name);
void semantic_bind_variable(Scope *scope, const char *name, ValueType type);
void semantic_free_scope_bindings(VariableBinding *vars);
ValueType semantic_infer_expression_type(SemanticInfo *info, const ParseNode *expr, Scope *scope);
ValueType semantic_infer_expression_type_with_hint(
    SemanticInfo *info,
    const ParseNode *expr,
    Scope *scope,
    ValueType expected_type);
ValueType semantic_infer_primary_type(SemanticInfo *info, const ParseNode *node, Scope *scope);
const ParseNode *semantic_function_parameters(const ParseNode *function_def);
ValueType semantic_function_return_type(SemanticInfo *info, const ParseNode *function_def);
size_t semantic_parameter_count(const ParseNode *parameters);
ValueType semantic_parameter_type(SemanticInfo *info, const ParseNode *parameter);
const ParseNode *semantic_simple_statement_target(const ParseNode *simple_stmt);
int semantic_is_type_assignment(const ParseNode *statement_tail);
const ParseNode *semantic_simple_statement_tail(const ParseNode *simple_stmt);
const ParseNode *semantic_statement_tail_expression(const ParseNode *statement_tail);
const ParseNode *semantic_statement_tail_type_node(const ParseNode *statement_tail);
int semantic_tuple_literal_index(const ParseNode *expr, size_t *index_out);

#endif
