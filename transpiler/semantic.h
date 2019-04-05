#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "parse.h"

typedef unsigned int ValueType;

enum {
    TYPE_INT = 1u << 0,
    TYPE_FLOAT = 1u << 1,
    TYPE_BOOL = 1u << 2,
    TYPE_CHAR = 1u << 3,
    TYPE_STR = 1u << 4,
    TYPE_NONE = 1u << 5,
    TYPE_LIST_INT = 1u << 6
};

typedef struct SemanticInfo SemanticInfo;

SemanticInfo *analyze_program(const ParseNode *root);
void free_semantic_info(SemanticInfo *info);
ValueType semantic_type_of(const SemanticInfo *info, const ParseNode *node);
const char *semantic_type_name(ValueType type);
int semantic_type_contains(ValueType type, ValueType member);
int semantic_type_is_union(ValueType type);
int semantic_type_is_ref(ValueType type);

#endif
