#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic_internal.h"

static int count_type_bits(ValueType type)
{
    int count = 0;

    while (type != 0) {
        count += (type & 1u) != 0;
        type >>= 1u;
    }

    return count;
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
        case TYPE_LIST_INT: return "list[int]";
        case TYPE_LIST_FLOAT: return "list[float]";
        case TYPE_LIST_BOOL: return "list[bool]";
        case TYPE_LIST_CHAR: return "list[char]";
        default: return "unknown";
    }
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
    if (strcmp(name, "list[int]") == 0) {
        return TYPE_LIST_INT;
    }
    if (strcmp(name, "list[float]") == 0) {
        return TYPE_LIST_FLOAT;
    }
    if (strcmp(name, "list[bool]") == 0) {
        return TYPE_LIST_BOOL;
    }
    if (strcmp(name, "list[char]") == 0) {
        return TYPE_LIST_CHAR;
    }

    semantic_error("unsupported type '%s'", name);
    return 0;
}

int semantic_type_contains(ValueType type, ValueType member)
{
    return member != 0 && (type & member) == member;
}

int semantic_type_is_union(ValueType type)
{
    return count_type_bits(type) > 1;
}

int semantic_type_is_ref(ValueType type)
{
    return type == TYPE_LIST_INT ||
        type == TYPE_LIST_FLOAT ||
        type == TYPE_LIST_BOOL ||
        type == TYPE_LIST_CHAR;
}

int semantic_type_is_list(ValueType type)
{
    return type == TYPE_LIST_INT ||
        type == TYPE_LIST_FLOAT ||
        type == TYPE_LIST_BOOL ||
        type == TYPE_LIST_CHAR;
}

ValueType semantic_list_element_type(ValueType type)
{
    switch (type) {
        case TYPE_LIST_INT:
            return TYPE_INT;
        case TYPE_LIST_FLOAT:
            return TYPE_FLOAT;
        case TYPE_LIST_BOOL:
            return TYPE_BOOL;
        case TYPE_LIST_CHAR:
            return TYPE_CHAR;
        default:
            semantic_error("%s is not a list type", semantic_type_name(type));
            return TYPE_NONE;
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
        TYPE_NONE,
        TYPE_LIST_INT,
        TYPE_LIST_FLOAT,
        TYPE_LIST_BOOL,
        TYPE_LIST_CHAR
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

void semantic_error(const char *message, ...)
{
    va_list args;

    fprintf(stderr, "Type error: ");
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(1);
}

void semantic_error_at_node(const ParseNode *node, const char *message, ...)
{
    va_list args;
    va_list copy;
    int needed;
    char *buffer;

    if (node == NULL || node->source_path == NULL || node->line <= 0 || node->column <= 0) {
        va_start(args, message);
        fprintf(stderr, "Type error: ");
        vfprintf(stderr, message, args);
        fprintf(stderr, "\n");
        va_end(args);
        exit(1);
    }

    va_start(args, message);
    va_copy(copy, args);
    needed = vsnprintf(NULL, 0, message, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(args);
        semantic_error("failed to format type error");
    }

    buffer = malloc((size_t)needed + 1);
    if (buffer == NULL) {
        va_end(args);
        perror("malloc");
        exit(1);
    }

    vsnprintf(buffer, (size_t)needed + 1, message, args);
    va_end(args);

    print_source_diagnostic(
        stderr,
        node->source_path,
        node->line,
        node->column,
        "Type error",
        buffer,
        node->source_line);
    free(buffer);
    exit(1);
}

const ParseNode *semantic_expect_child(const ParseNode *node, size_t index, NodeKind kind)
{
    if (node == NULL || index >= node->child_count || node->children[index]->kind != kind) {
        semantic_error("malformed AST");
    }
    return node->children[index];
}

const ParseNode *semantic_statement_payload(const ParseNode *statement)
{
    if (statement == NULL || statement->kind != NODE_STATEMENT || statement->child_count != 1) {
        semantic_error("malformed statement node");
    }
    return statement->children[0];
}

int semantic_is_if_statement(const ParseNode *node)
{
    return node->kind == NODE_IF_STATEMENT;
}

int semantic_is_epsilon_node(const ParseNode *node)
{
    return node->kind == NODE_EPSILON;
}

int semantic_is_numeric_type(ValueType type)
{
    return type == TYPE_INT || type == TYPE_FLOAT;
}

int semantic_is_assignable(ValueType target, ValueType value)
{
    const ValueType source_members[] = {
        TYPE_INT,
        TYPE_FLOAT,
        TYPE_BOOL,
        TYPE_CHAR,
        TYPE_STR,
        TYPE_NONE,
        TYPE_LIST_INT,
        TYPE_LIST_FLOAT,
        TYPE_LIST_BOOL,
        TYPE_LIST_CHAR
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

void semantic_record_node_type(SemanticInfo *info, const ParseNode *node, ValueType type)
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

ValueType semantic_parse_type_node(SemanticInfo *info, const ParseNode *type_node)
{
    ValueType type = 0;

    if (type_node == NULL || type_node->kind != NODE_TYPE || type_node->child_count == 0) {
        semantic_error("malformed type annotation");
    }

    for (size_t i = 0; i < type_node->child_count; i++) {
        const ParseNode *member = semantic_expect_child(type_node, i, NODE_PRIMARY);
        ValueType atom = parse_type_atom_name(member->value);

        if (semantic_type_contains(type, atom)) {
            semantic_error_at_node(member, "duplicate type '%s' in union", member->value);
        }

        semantic_record_node_type(info, member, atom);
        type |= atom;
    }

    if ((semantic_type_contains(type, TYPE_LIST_INT) ||
         semantic_type_contains(type, TYPE_LIST_FLOAT) ||
         semantic_type_contains(type, TYPE_LIST_BOOL) ||
         semantic_type_contains(type, TYPE_LIST_CHAR)) &&
        semantic_type_is_union(type)) {
        semantic_error_at_node(type_node, "list types cannot be used inside a union yet");
    }

    semantic_record_node_type(info, type_node, type);
    return type;
}

int semantic_builtin_returns_owned_ref(const char *name)
{
    return strcmp(name, "list_int") == 0 ||
        strcmp(name, "list_float") == 0 ||
        strcmp(name, "list_bool") == 0 ||
        strcmp(name, "list_char") == 0;
}

FunctionInfo *semantic_find_function(FunctionInfo *functions, const char *name)
{
    for (FunctionInfo *fn = functions; fn != NULL; fn = fn->next) {
        if (strcmp(fn->name, name) == 0) {
            return fn;
        }
    }
    return NULL;
}

VariableBinding *semantic_find_variable(Scope *scope, const char *name)
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

void semantic_bind_variable(Scope *scope, const char *name, ValueType type)
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

void semantic_free_scope_bindings(VariableBinding *vars)
{
    while (vars != NULL) {
        VariableBinding *next = vars->next;
        free(vars);
        vars = next;
    }
}
