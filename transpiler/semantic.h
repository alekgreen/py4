#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "parse.h"

typedef unsigned int ValueType;

#define TYPE_ATOMIC_MASK ((1u << 12) - 1u)
#define TYPE_TUPLE_BASE (1u << 16)
#define TYPE_CLASS_BASE (1u << 24)
#define MAX_TUPLE_TYPES 64
#define MAX_TUPLE_ELEMENTS 8
#define MAX_CLASS_TYPES 64
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
    TYPE_LIST_STR = 1u << 10,
    TYPE_DICT_STR_STR = 1u << 11
};

typedef struct SemanticInfo SemanticInfo;

SemanticInfo *analyze_program(const ParseNode *root);
void free_semantic_info(SemanticInfo *info);
ValueType semantic_type_of(const SemanticInfo *info, const ParseNode *node);
const char *semantic_type_name(ValueType type);
int semantic_type_contains(ValueType type, ValueType member);
int semantic_type_is_union(ValueType type);
int semantic_type_is_tuple(ValueType type);
int semantic_type_is_class(ValueType type);
int semantic_type_is_ref(ValueType type);
int semantic_type_needs_management(ValueType type);
int semantic_type_is_list(ValueType type);
int semantic_type_is_dict(ValueType type);
ValueType semantic_list_element_type(ValueType type);
ValueType semantic_make_tuple_type(const ValueType *elements, size_t element_count);
size_t semantic_tuple_element_count(ValueType type);
ValueType semantic_tuple_element_type(ValueType type, size_t index);
size_t semantic_tuple_type_count(void);
ValueType semantic_tuple_type_at(size_t index);
int semantic_tuple_literal_index(const ParseNode *expr, size_t *index_out);
size_t semantic_class_type_count(void);
ValueType semantic_class_type_at(size_t index);
const char *semantic_class_name(ValueType type);
size_t semantic_class_field_count(ValueType type);
const char *semantic_class_field_name(ValueType type, size_t index);
ValueType semantic_class_field_type(ValueType type, size_t index);
ValueType semantic_find_class_type(const char *name);
const char *semantic_method_c_name(const SemanticInfo *info, ValueType owner_type, const char *method_name);
size_t semantic_method_arity(const SemanticInfo *info, ValueType owner_type, const char *method_name);
ValueType semantic_method_parameter_type(
    const SemanticInfo *info,
    ValueType owner_type,
    const char *method_name,
    size_t index);

#endif
