#ifndef SEMANTIC_H
#define SEMANTIC_H

#include "parse.h"

typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_STRING,
    TYPE_NONE
} ValueType;

typedef struct SemanticInfo SemanticInfo;

SemanticInfo *analyze_program(const ParseNode *root);
void free_semantic_info(SemanticInfo *info);
ValueType semantic_type_of(const SemanticInfo *info, const ParseNode *node);
const char *semantic_type_name(ValueType type);

#endif
