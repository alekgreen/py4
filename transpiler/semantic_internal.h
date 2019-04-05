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

void semantic_error(const char *message, ...);
const ParseNode *semantic_expect_child(const ParseNode *node, size_t index, NodeKind kind);
const ParseNode *semantic_statement_payload(const ParseNode *statement);
int semantic_is_if_statement(const ParseNode *node);
int semantic_is_epsilon_node(const ParseNode *node);
int semantic_is_numeric_type(ValueType type);
int semantic_is_assignable(ValueType target, ValueType value);
int semantic_builtin_returns_owned_ref(const char *name);
void semantic_record_node_type(SemanticInfo *info, const ParseNode *node, ValueType type);
ValueType semantic_parse_type_node(SemanticInfo *info, const ParseNode *type_node);
FunctionInfo *semantic_find_function(FunctionInfo *functions, const char *name);
VariableBinding *semantic_find_variable(Scope *scope, const char *name);
void semantic_bind_variable(Scope *scope, const char *name, ValueType type);
void semantic_free_scope_bindings(VariableBinding *vars);

#endif
