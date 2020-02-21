#ifndef SEMANTIC_CODEGEN_INTERNAL_H
#define SEMANTIC_CODEGEN_INTERNAL_H

#include "semantic.h"

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

MethodInfo *semantic_methods(const SemanticInfo *info);
MethodInfo *semantic_find_method(MethodInfo *methods, ValueType owner_type, const char *name);

#endif
