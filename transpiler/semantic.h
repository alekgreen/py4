#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "parse.h"
#include "module_loader.h"

typedef unsigned int ValueType;

#define TYPE_ATOMIC_MASK ((1u << 12) - 1u)
#define TYPE_TUPLE_BASE (1u << 16)
#define MAX_TUPLE_TYPES 64
#define MAX_TUPLE_ELEMENTS 8
#define TYPE_CLASS_BASE (TYPE_TUPLE_BASE + MAX_TUPLE_TYPES)
#define MAX_CLASS_TYPES 64
#define TYPE_CLASS_LIST_BASE (TYPE_CLASS_BASE + MAX_CLASS_TYPES)
#define MAX_CLASS_LIST_TYPES 64
#define TYPE_NATIVE_BASE (TYPE_CLASS_LIST_BASE + MAX_CLASS_LIST_TYPES)
#define MAX_NATIVE_TYPES 16
#define TYPE_OPTIONAL_BASE (TYPE_NATIVE_BASE + MAX_NATIVE_TYPES)
#define MAX_OPTIONAL_TYPES 64
#define TYPE_DICT_BASE (TYPE_OPTIONAL_BASE + MAX_OPTIONAL_TYPES)
#define MAX_DICT_TYPES 64
#define MAX_CLASS_FIELDS 32

enum {
    TYPE_INT = 1u << 0,
    TYPE_FLOAT = 1u << 1,
    TYPE_BOOL = 1u << 2,
    TYPE_CHAR = 1u << 3,
    TYPE_STR = 1u << 4,
    TYPE_NONE = 1u << 5,
    TYPE_LIST_INT = 1u << 6,
    TYPE_LIST_FLOAT = 1u << 7,
    TYPE_LIST_BOOL = 1u << 8,
    TYPE_LIST_CHAR = 1u << 9,
    TYPE_LIST_STR = 1u << 10
};

typedef struct SemanticInfo SemanticInfo;

SemanticInfo *analyze_program(const LoadedProgram *program);
void free_semantic_info(SemanticInfo *info);
ValueType semantic_type_of(const SemanticInfo *info, const ParseNode *node);
const char *semantic_type_name(ValueType type);
int semantic_type_contains(ValueType type, ValueType member);
int semantic_type_is_union(ValueType type);
int semantic_type_is_tuple(ValueType type);
int semantic_type_is_class(ValueType type);
int semantic_type_is_native(ValueType type);
int semantic_type_is_optional(ValueType type);
int semantic_type_is_ref(ValueType type);
int semantic_type_needs_management(ValueType type);
int semantic_type_is_list(ValueType type);
int semantic_type_is_dict(ValueType type);
ValueType semantic_list_element_type(ValueType type);
ValueType semantic_make_dict_type(ValueType key_type, ValueType value_type);
ValueType semantic_dict_key_type(ValueType type);
ValueType semantic_dict_value_type(ValueType type);
ValueType semantic_make_tuple_type(const ValueType *elements, size_t element_count);
size_t semantic_tuple_element_count(ValueType type);
ValueType semantic_tuple_element_type(ValueType type, size_t index);
size_t semantic_tuple_type_count(void);
ValueType semantic_tuple_type_at(size_t index);
int semantic_tuple_literal_index(const ParseNode *expr, size_t *index_out);
size_t semantic_list_type_count(void);
ValueType semantic_list_type_at(size_t index);
ValueType semantic_make_list_type(ValueType element_type);
size_t semantic_dict_type_count(void);
ValueType semantic_dict_type_at(size_t index);
size_t semantic_class_type_count(void);
ValueType semantic_class_type_at(size_t index);
const char *semantic_class_name(ValueType type);
ValueType semantic_class_base_type(ValueType type);
size_t semantic_class_field_count(ValueType type);
const char *semantic_class_field_name(ValueType type, size_t index);
ValueType semantic_class_field_owner_type(ValueType type, size_t index);
ValueType semantic_class_field_type(ValueType type, size_t index);
ValueType semantic_find_class_type(const char *name);
size_t semantic_native_type_count(void);
ValueType semantic_find_native_type(const char *module_name, const char *name);
ValueType semantic_native_type_at(size_t index);
const char *semantic_native_type_name(ValueType type);
const char *semantic_native_type_module(ValueType type);
const char *semantic_native_c_type(ValueType type);
const char *semantic_native_runtime_prefix(ValueType type);
size_t semantic_optional_type_count(void);
ValueType semantic_optional_type_at(size_t index);
ValueType semantic_make_optional_type(ValueType base_type);
ValueType semantic_optional_base_type(ValueType type);
int semantic_class_has_initializer(const SemanticInfo *info, ValueType type);
ValueType semantic_call_constructor_type(const SemanticInfo *info, const ParseNode *call);
const char *semantic_module_name_for_path(const SemanticInfo *info, const char *path);
const char *semantic_module_name_for_receiver(const SemanticInfo *info, const ParseNode *receiver);
const char *semantic_function_c_name(const SemanticInfo *info, const ParseNode *function_def);
const char *semantic_call_c_name(const SemanticInfo *info, const ParseNode *call);
int semantic_has_call_target(const SemanticInfo *info, const ParseNode *call);
size_t semantic_call_arity(const SemanticInfo *info, const ParseNode *call);
ValueType semantic_call_parameter_type(const SemanticInfo *info, const ParseNode *call, size_t index);
const char *semantic_global_c_name(const SemanticInfo *info, const char *module_name, const char *name);
const char *semantic_global_target_c_name(const SemanticInfo *info, const ParseNode *node);
const char *semantic_method_c_name(const SemanticInfo *info, ValueType owner_type, const char *method_name);
size_t semantic_method_arity(const SemanticInfo *info, ValueType owner_type, const char *method_name);
ValueType semantic_method_parameter_type(
    const SemanticInfo *info,
    ValueType owner_type,
    const char *method_name,
    size_t index);
int semantic_is_inferred_declaration_target(const SemanticInfo *info, const ParseNode *node);

#endif
