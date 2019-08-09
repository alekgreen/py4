#ifndef CODEGEN_INTERNAL_H
#define CODEGEN_INTERNAL_H

#include <stddef.h>
#include <stdio.h>

#include "codegen.h"

#define MAX_UNION_TYPES 64
#define MAX_UNION_CONVERSIONS 128
#define MAX_NAME_LEN 128
#define MAX_REF_LOCALS 256
#define MAX_SCOPE_DEPTH 128
#define MAX_LOOP_DEPTH 128

typedef struct {
    ValueType from_type;
    ValueType to_type;
} UnionConversion;

typedef struct {
    const char *name;
    ValueType type;
} ManagedLocal;

typedef struct {
    FILE *out;
    const ParseNode *root;
    const SemanticInfo *semantic;
    int indent_level;
    int temp_counter;
    int has_user_main;
    int has_top_level_executable_statements;
    int current_function_is_main;
    int current_function_is_init;
    ValueType current_function_return_type;
    ValueType union_types[MAX_UNION_TYPES];
    size_t union_type_count;
    ValueType printable_union_types[MAX_UNION_TYPES];
    size_t printable_union_type_count;
    UnionConversion conversions[MAX_UNION_CONVERSIONS];
    size_t conversion_count;
    ManagedLocal managed_locals[MAX_REF_LOCALS];
    size_t managed_local_count;
    size_t cleanup_scope_starts[MAX_SCOPE_DEPTH];
    size_t cleanup_scope_count;
    int loop_ids[MAX_LOOP_DEPTH];
    size_t loop_count;
} CodegenContext;

extern const ValueType CODEGEN_ORDERED_TYPES[];
extern const size_t CODEGEN_ORDERED_TYPE_COUNT;

void codegen_error(const char *message, ...);
void codegen_emit_indent(CodegenContext *ctx);
char *codegen_dup_printf(const char *fmt, ...);
const ParseNode *codegen_expect_child(const ParseNode *node, size_t index, NodeKind kind);
int codegen_is_type_assignment(const ParseNode *statement_tail);
int codegen_is_epsilon_node(const ParseNode *node);
int codegen_is_print_call_expr(const ParseNode *expr);
const ParseNode *codegen_statement_payload(const ParseNode *statement);
const ParseNode *codegen_simple_statement_target(const ParseNode *simple_stmt);
const ParseNode *codegen_simple_statement_tail(const ParseNode *simple_stmt);
const ParseNode *codegen_statement_tail_expression(const ParseNode *statement_tail);
const ParseNode *codegen_statement_tail_type_node(const ParseNode *statement_tail);
const ParseNode *codegen_function_parameters(const ParseNode *function_def);
const ParseNode *codegen_function_suite(const ParseNode *function_def);
int codegen_is_native_function(const ParseNode *function_def);
const char *codegen_function_c_name(CodegenContext *ctx, const ParseNode *function_def);
int codegen_is_main_function(const ParseNode *function_def);
ValueType codegen_function_return_type(CodegenContext *ctx, const ParseNode *function_def);
const ParseNode *codegen_find_function_definition(const ParseNode *root, const char *name);
const char *codegen_type_suffix(ValueType type);
const char *codegen_type_field(ValueType type);
void codegen_build_tuple_base_name(char *buffer, size_t size, ValueType type);
void codegen_build_tuple_print_name(char *buffer, size_t size, ValueType type);
void codegen_build_tuple_retain_name(char *buffer, size_t size, ValueType type);
void codegen_build_tuple_release_name(char *buffer, size_t size, ValueType type);
void codegen_build_class_print_name(char *buffer, size_t size, ValueType type);
void codegen_build_class_retain_name(char *buffer, size_t size, ValueType type);
void codegen_build_class_release_name(char *buffer, size_t size, ValueType type);
void codegen_build_class_ctor_name(char *buffer, size_t size, ValueType type);
void codegen_build_list_print_name(char *buffer, size_t size, ValueType type);
void codegen_build_dict_print_name(char *buffer, size_t size, ValueType type);
const char *codegen_ref_runtime_prefix(ValueType type);
const char *codegen_list_struct_name(ValueType type);
const char *codegen_list_runtime_prefix(ValueType type);
const char *codegen_list_element_c_type(ValueType type);
const char *codegen_dict_struct_name(ValueType type);
const char *codegen_dict_runtime_prefix(ValueType type);
char *codegen_list_new_call(ValueType type);
char *codegen_list_unary_call(ValueType type, const char *suffix, const char *arg);
char *codegen_list_binary_call(ValueType type, const char *suffix, const char *arg0, const char *arg1);
char *codegen_list_ternary_call(
    ValueType type,
    const char *suffix,
    const char *arg0,
    const char *arg1,
    const char *arg2);
char *codegen_dict_new_call(ValueType type);
char *codegen_dict_unary_call(ValueType type, const char *suffix, const char *arg);
char *codegen_dict_binary_call(ValueType type, const char *suffix, const char *arg0, const char *arg1);
char *codegen_dict_ternary_call(
    ValueType type,
    const char *suffix,
    const char *arg0,
    const char *arg1,
    const char *arg2);
void codegen_build_union_base_name(char *buffer, size_t size, ValueType type);
void codegen_build_union_tag_name(char *buffer, size_t size, ValueType type);
void codegen_build_union_enum_value_name(char *buffer, size_t size, ValueType union_type, ValueType member);
void codegen_build_union_ctor_name(char *buffer, size_t size, ValueType union_type, ValueType member);
void codegen_build_union_print_name(char *buffer, size_t size, ValueType union_type);
void codegen_build_union_convert_name(char *buffer, size_t size, ValueType from_type, ValueType to_type);
void codegen_emit_scalar_c_type(FILE *out, ValueType type);
void codegen_emit_type_name(CodegenContext *ctx, ValueType type);
void codegen_add_union_type(CodegenContext *ctx, ValueType type);
ValueType codegen_resolve_union_member_type(ValueType union_type, ValueType value_type);
void codegen_add_union_conversion(CodegenContext *ctx, ValueType from_type, ValueType to_type);
void codegen_add_printable_union_type(CodegenContext *ctx, ValueType type);
void codegen_collect_union_types_from_node(CodegenContext *ctx, const ParseNode *node);
void codegen_collect_required_conversions(CodegenContext *ctx, const ParseNode *node);
void codegen_emit_container_runtime(CodegenContext *ctx);
void codegen_emit_struct_types(CodegenContext *ctx);
void codegen_emit_tuple_runtime(CodegenContext *ctx);
void codegen_emit_class_types(CodegenContext *ctx);
void codegen_emit_union_runtime(CodegenContext *ctx);
void codegen_collect_program_state(CodegenContext *ctx, const ParseNode *root);
char *codegen_type_to_c_string(ValueType type);
char *codegen_next_temp_name(CodegenContext *ctx);
void codegen_push_cleanup_scope(CodegenContext *ctx);
void codegen_emit_ref_incref(CodegenContext *ctx, ValueType type, const char *name);
void codegen_emit_ref_decref(CodegenContext *ctx, ValueType type, const char *name);
void codegen_emit_value_retain(CodegenContext *ctx, ValueType type, const char *name);
void codegen_emit_value_release(CodegenContext *ctx, ValueType type, const char *name);
void codegen_register_ref_local(CodegenContext *ctx, const char *name, ValueType type);
void codegen_emit_live_ref_cleanup(CodegenContext *ctx);
void codegen_pop_cleanup_scope(CodegenContext *ctx);
int codegen_expression_is_owned_ref(CodegenContext *ctx, const ParseNode *expr);
char *codegen_primary_to_c_string(CodegenContext *ctx, const ParseNode *primary);
char *codegen_expression_to_c_string(CodegenContext *ctx, const ParseNode *expr);
char *codegen_wrapped_expression_to_c_string(CodegenContext *ctx, const ParseNode *expr, ValueType target_type);
void codegen_emit_expression(CodegenContext *ctx, const ParseNode *expr);
void codegen_emit_wrapped_expression(CodegenContext *ctx, const ParseNode *expr, ValueType target_type);
void codegen_emit_statement(CodegenContext *ctx, const ParseNode *statement, int allow_function_defs);
void codegen_emit_suite(CodegenContext *ctx, const ParseNode *suite);

#endif
