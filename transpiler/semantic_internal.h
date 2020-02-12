#ifndef SEMANTIC_INTERNAL_H
#define SEMANTIC_INTERNAL_H

#include "semantic.h"

typedef struct VariableBinding {
    const char *name;
    const char *module_name;
    ValueType type;
    struct VariableBinding *next;
} VariableBinding;

typedef struct Scope {
    VariableBinding *vars;
    const struct ModuleInfo *module;
    ValueType current_class_type;
    struct Scope *parent;
} Scope;

typedef struct FunctionInfo {
    const char *name;
    const char *module_name;
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

typedef struct ConstructorTargetInfo {
    const ParseNode *call;
    ValueType class_type;
    struct ConstructorTargetInfo *next;
} ConstructorTargetInfo;

typedef struct EnumVariantTargetInfo {
    const ParseNode *node;
    ValueType enum_type;
    size_t variant_index;
    struct EnumVariantTargetInfo *next;
} EnumVariantTargetInfo;

typedef struct MethodInfo {
    ValueType owner_type;
    ValueType source_owner_type;
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

typedef struct ImportBinding {
    const char *local_name;
    const char *module_name;
    const char *symbol_name;
    int is_module_import;
    struct ImportBinding *next;
} ImportBinding;

typedef struct GlobalBinding {
    const char *module_name;
    const char *name;
    ValueType type;
    struct GlobalBinding *next;
} GlobalBinding;

typedef struct GlobalTargetInfo {
    const ParseNode *node;
    const char *module_name;
    const char *name;
    struct GlobalTargetInfo *next;
} GlobalTargetInfo;

typedef struct InferredDeclTargetInfo {
    const ParseNode *node;
    struct InferredDeclTargetInfo *next;
} InferredDeclTargetInfo;

typedef struct ModuleInfo {
    const char *name;
    const char *path;
    const ParseNode *root;
    ImportBinding *imports;
    struct ModuleInfo *next;
} ModuleInfo;

struct SemanticInfo {
    FunctionInfo *functions;
    MethodInfo *methods;
    NodeTypeInfo *node_types;
    CallTargetInfo *call_targets;
    ConstructorTargetInfo *constructor_targets;
    EnumVariantTargetInfo *enum_variant_targets;
    ModuleInfo *modules;
    ModuleInfo *entry_module;
    GlobalBinding *globals;
    GlobalTargetInfo *global_targets;
    InferredDeclTargetInfo *inferred_decl_targets;
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
ValueType semantic_find_enum_type(const char *name);
ValueType semantic_find_native_type(const char *module_name, const char *name);
void semantic_register_native_type(const char *module_name, const ParseNode *type_def);
ValueType semantic_register_enum(const ParseNode *enum_def);
ValueType semantic_register_class(const ParseNode *class_def);
void semantic_define_class_fields(SemanticInfo *info, const ParseNode *class_def);
FunctionInfo *semantic_find_function(FunctionInfo *functions, const char *name);
FunctionInfo *semantic_find_function_by_node(FunctionInfo *functions, const ParseNode *node);
void semantic_record_call_target(SemanticInfo *info, const ParseNode *call, FunctionInfo *function);
FunctionInfo *semantic_resolved_call_target(const SemanticInfo *info, const ParseNode *call);
void semantic_record_constructor_target(SemanticInfo *info, const ParseNode *call, ValueType class_type);
ValueType semantic_resolved_constructor_target(const SemanticInfo *info, const ParseNode *call);
void semantic_record_enum_variant_target(
    SemanticInfo *info,
    const ParseNode *node,
    ValueType enum_type,
    size_t variant_index);
EnumVariantTargetInfo *semantic_find_enum_variant_target(const SemanticInfo *info, const ParseNode *node);
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
int json_from_string_type_supported(ValueType type);
int json_to_string_type_supported(ValueType type);
ModuleInfo *semantic_find_module_info(ModuleInfo *modules, const char *name);
int is_module_private_name(const char *name);
char *module_ref_string(const ParseNode *node);
const ModuleInfo *scope_module(Scope *scope);
ValueType scope_class_type(Scope *scope);
ImportBinding *find_module_binding_for_receiver(const ModuleInfo *module, const ParseNode *receiver);
GlobalBinding *resolve_visible_global(SemanticInfo *info, Scope *scope, const char *name);
ValueType resolve_visible_class_type(SemanticInfo *info, Scope *scope, const char *name);
ValueType resolve_visible_enum_type(SemanticInfo *info, Scope *scope, const char *name);
ValueType resolve_module_class_type(
    SemanticInfo *info,
    const ModuleInfo *current_module,
    const char *module_local_name,
    const char *class_name);
ValueType resolve_field_access_enum_type(SemanticInfo *info, Scope *scope, const ParseNode *base);
ValueType resolve_function_call_type(
    SemanticInfo *info,
    const ParseNode *call,
    const char *display_name,
    const char *function_name,
    const char *module_name,
    const ParseNode *arguments,
    Scope *scope);
GlobalBinding *semantic_find_global(GlobalBinding *globals, const char *module_name, const char *name);
void semantic_record_global_target(
    SemanticInfo *info,
    const ParseNode *node,
    const char *module_name,
    const char *name);
GlobalTargetInfo *semantic_find_global_target(const SemanticInfo *info, const ParseNode *node);
void semantic_record_inferred_declaration_target(SemanticInfo *info, const ParseNode *node);
int semantic_is_inferred_declaration_target(const SemanticInfo *info, const ParseNode *node);

#endif
