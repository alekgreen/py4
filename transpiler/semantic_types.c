#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "semantic_internal.h"

typedef struct {
    ValueType id;
    size_t element_count;
    ValueType element_types[MAX_TUPLE_ELEMENTS];
    char name[128];
} TupleTypeInfo;

typedef struct {
    ValueType id;
    const char *name;
    const ParseNode *node;
    size_t field_count;
    const char *field_names[MAX_CLASS_FIELDS];
    ValueType field_types[MAX_CLASS_FIELDS];
} ClassTypeInfo;

static TupleTypeInfo TUPLE_TYPES[MAX_TUPLE_TYPES];
static size_t TUPLE_TYPE_COUNT = 0;
static ClassTypeInfo CLASS_TYPES[MAX_CLASS_TYPES];
static size_t CLASS_TYPE_COUNT = 0;

static const TupleTypeInfo *find_tuple_type(ValueType type)
{
    for (size_t i = 0; i < TUPLE_TYPE_COUNT; i++) {
        if (TUPLE_TYPES[i].id == type) {
            return &TUPLE_TYPES[i];
        }
    }

    semantic_error("unknown tuple type id %u", type);
    return NULL;
}

static ClassTypeInfo *find_class_type(ValueType type)
{
    for (size_t i = 0; i < CLASS_TYPE_COUNT; i++) {
        if (CLASS_TYPES[i].id == type) {
            return &CLASS_TYPES[i];
        }
    }

    semantic_error("unknown class type id %u", type);
    return NULL;
}

static int tuple_elements_equal(const TupleTypeInfo *info, const ValueType *elements, size_t count)
{
    if (info->element_count != count) {
        return 0;
    }

    for (size_t i = 0; i < count; i++) {
        if (info->element_types[i] != elements[i]) {
            return 0;
        }
    }

    return 1;
}

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
    if (semantic_type_is_tuple(type)) {
        return find_tuple_type(type)->name;
    }
    if (semantic_type_is_class(type)) {
        return find_class_type(type)->name;
    }

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
        case TYPE_LIST_STR: return "list[str]";
        case TYPE_DICT_STR_STR: return "dict[str, str]";
        default: return "unknown";
    }
}

static ValueType parse_named_type_atom(const char *name)
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
    if (strcmp(name, "list[str]") == 0) {
        return TYPE_LIST_STR;
    }
    if (strcmp(name, "dict[str, str]") == 0) {
        return TYPE_DICT_STR_STR;
    }

    return 0;
}

static ValueType parse_type_atom_node(SemanticInfo *info, const ParseNode *node)
{
    ValueType elements[MAX_TUPLE_ELEMENTS];
    ValueType named_type;

    if (node == NULL || node->kind != NODE_PRIMARY || node->value == NULL) {
        semantic_error("malformed type atom");
    }

    if (node->child_count == 0) {
        named_type = parse_named_type_atom(node->value);
        if (named_type != 0) {
            return named_type;
        }

        named_type = semantic_find_class_type(node->value);
        if (named_type != 0) {
            return named_type;
        }

        semantic_error_at_node(node, "unsupported type '%s'", node->value);
    }

    if (node->child_count < 2) {
        semantic_error_at_node(node, "tuple types must have at least two elements");
    }
    if (node->child_count > MAX_TUPLE_ELEMENTS) {
        semantic_error_at_node(node, "tuple types support at most %d elements", MAX_TUPLE_ELEMENTS);
    }

    for (size_t i = 0; i < node->child_count; i++) {
        elements[i] = parse_type_atom_node(info, semantic_expect_child(node, i, NODE_PRIMARY));
    }

    return semantic_make_tuple_type(elements, node->child_count);
}

int semantic_type_contains(ValueType type, ValueType member)
{
    if (semantic_type_is_tuple(type) || semantic_type_is_tuple(member) ||
        semantic_type_is_class(type) || semantic_type_is_class(member)) {
        return type == member;
    }
    return member != 0 && (type & member) == member;
}

int semantic_type_is_union(ValueType type)
{
    if (semantic_type_is_tuple(type) || semantic_type_is_class(type) || (type & ~TYPE_ATOMIC_MASK) != 0) {
        return 0;
    }
    return count_type_bits(type) > 1;
}

int semantic_type_is_tuple(ValueType type)
{
    return type >= TYPE_TUPLE_BASE && type < TYPE_CLASS_BASE;
}

int semantic_type_is_class(ValueType type)
{
    return type >= TYPE_CLASS_BASE;
}

int semantic_type_is_ref(ValueType type)
{
    return !semantic_type_is_tuple(type) && (type == TYPE_LIST_INT ||
        type == TYPE_LIST_FLOAT ||
        type == TYPE_LIST_BOOL ||
        type == TYPE_LIST_CHAR ||
        type == TYPE_LIST_STR ||
        type == TYPE_DICT_STR_STR);
}

int semantic_type_needs_management(ValueType type)
{
    if (semantic_type_is_ref(type)) {
        return 1;
    }

    if (semantic_type_is_tuple(type)) {
        for (size_t i = 0; i < semantic_tuple_element_count(type); i++) {
            if (semantic_type_needs_management(semantic_tuple_element_type(type, i))) {
                return 1;
            }
        }
        return 0;
    }

    if (semantic_type_is_class(type)) {
        for (size_t i = 0; i < semantic_class_field_count(type); i++) {
            if (semantic_type_needs_management(semantic_class_field_type(type, i))) {
                return 1;
            }
        }
        return 0;
    }

    return 0;
}

int semantic_type_is_list(ValueType type)
{
    return type == TYPE_LIST_INT ||
        type == TYPE_LIST_FLOAT ||
        type == TYPE_LIST_BOOL ||
        type == TYPE_LIST_CHAR ||
        type == TYPE_LIST_STR;
}

int semantic_type_is_dict(ValueType type)
{
    return type == TYPE_DICT_STR_STR;
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
        case TYPE_LIST_STR:
            return TYPE_STR;
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
        TYPE_LIST_CHAR,
        TYPE_LIST_STR,
        TYPE_DICT_STR_STR
    };

    if (semantic_type_is_tuple(type) || semantic_type_is_class(type)) {
        return type_member_name(type);
    }

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
        TYPE_LIST_CHAR,
        TYPE_LIST_STR,
        TYPE_DICT_STR_STR
    };

    if (target == 0 || value == 0) {
        return 0;
    }

    if (semantic_type_is_tuple(target) || semantic_type_is_tuple(value) ||
        semantic_type_is_class(target) || semantic_type_is_class(value)) {
        return target == value;
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

ValueType semantic_make_tuple_type(const ValueType *elements, size_t element_count)
{
    TupleTypeInfo *entry;
    size_t used = 0;

    if (element_count < 2) {
        semantic_error("tuple types must have at least two elements");
    }
    if (element_count > MAX_TUPLE_ELEMENTS) {
        semantic_error("tuple types support at most %d elements", MAX_TUPLE_ELEMENTS);
    }

    for (size_t i = 0; i < element_count; i++) {
        if (semantic_type_is_union(elements[i])) {
            semantic_error("tuple elements cannot be union types yet");
        }
        if (elements[i] == TYPE_NONE) {
            semantic_error("tuple elements cannot be None");
        }
    }

    for (size_t i = 0; i < TUPLE_TYPE_COUNT; i++) {
        if (tuple_elements_equal(&TUPLE_TYPES[i], elements, element_count)) {
            return TUPLE_TYPES[i].id;
        }
    }

    if (TUPLE_TYPE_COUNT >= MAX_TUPLE_TYPES) {
        semantic_error("too many tuple types in one program");
    }

    entry = &TUPLE_TYPES[TUPLE_TYPE_COUNT];
    entry->id = TYPE_TUPLE_BASE + (ValueType)TUPLE_TYPE_COUNT;
    entry->element_count = element_count;
    for (size_t i = 0; i < element_count; i++) {
        entry->element_types[i] = elements[i];
    }

    entry->name[used++] = '(';
    for (size_t i = 0; i < element_count; i++) {
        const char *part = semantic_type_name(elements[i]);
        size_t part_len = strlen(part);

        if (used + part_len + 3 >= sizeof(entry->name)) {
            semantic_error("tuple type name is too long");
        }
        memcpy(entry->name + used, part, part_len);
        used += part_len;
        if (i + 1 < element_count) {
            entry->name[used++] = ',';
        }
    }
    entry->name[used++] = ')';
    entry->name[used] = '\0';

    TUPLE_TYPE_COUNT++;
    return entry->id;
}

size_t semantic_tuple_element_count(ValueType type)
{
    return find_tuple_type(type)->element_count;
}

ValueType semantic_tuple_element_type(ValueType type, size_t index)
{
    const TupleTypeInfo *info = find_tuple_type(type);

    if (index >= info->element_count) {
        semantic_error("tuple index out of bounds for %s", semantic_type_name(type));
    }
    return info->element_types[index];
}

size_t semantic_tuple_type_count(void)
{
    return TUPLE_TYPE_COUNT;
}

ValueType semantic_tuple_type_at(size_t index)
{
    if (index >= TUPLE_TYPE_COUNT) {
        semantic_error("tuple type index out of bounds");
    }
    return TUPLE_TYPES[index].id;
}

ValueType semantic_find_class_type(const char *name)
{
    for (size_t i = 0; i < CLASS_TYPE_COUNT; i++) {
        if (strcmp(CLASS_TYPES[i].name, name) == 0) {
            return CLASS_TYPES[i].id;
        }
    }
    return 0;
}

ValueType semantic_register_class(const ParseNode *class_def)
{
    const ParseNode *name_node;
    const char *name;
    ValueType builtin_type;
    ClassTypeInfo *entry;

    if (class_def == NULL || class_def->kind != NODE_CLASS_DEF) {
        semantic_error("malformed class definition");
    }

    name_node = semantic_expect_child(class_def, 0, NODE_PRIMARY);
    name = name_node->value;
    if (semantic_find_class_type(name) != 0) {
        semantic_error_at_node(name_node, "duplicate class '%s'", name);
    }

    builtin_type = parse_named_type_atom(name);
    if (builtin_type != 0) {
        semantic_error_at_node(name_node, "class name '%s' conflicts with built-in type", name);
    }

    if (CLASS_TYPE_COUNT >= MAX_CLASS_TYPES) {
        semantic_error("too many classes in one program");
    }

    entry = &CLASS_TYPES[CLASS_TYPE_COUNT];
    entry->id = TYPE_CLASS_BASE + (ValueType)CLASS_TYPE_COUNT;
    entry->name = name;
    entry->node = class_def;
    entry->field_count = 0;
    CLASS_TYPE_COUNT++;
    return entry->id;
}

void semantic_define_class_fields(SemanticInfo *info, const ParseNode *class_def)
{
    ClassTypeInfo *entry;
    ValueType class_type;

    class_type = semantic_find_class_type(semantic_expect_child(class_def, 0, NODE_PRIMARY)->value);
    if (class_type == 0) {
        semantic_error("class definition missing from registry");
    }
    entry = find_class_type(class_type);

    for (size_t i = 2; i < class_def->child_count; i++) {
        const ParseNode *field = class_def->children[i];
        const ParseNode *type_node;
        ValueType field_type;

        if (field->kind == NODE_FUNCTION_DEF) {
            continue;
        }
        if (field->kind != NODE_FIELD_DECL) {
            semantic_error("malformed class definition");
        }

        type_node = semantic_expect_child(field, 0, NODE_TYPE);
        field_type = semantic_parse_type_node(info, type_node);

        if (entry->field_count >= MAX_CLASS_FIELDS) {
            semantic_error_at_node(field, "class '%s' supports at most %d fields", entry->name, MAX_CLASS_FIELDS);
        }
        for (size_t j = 0; j < entry->field_count; j++) {
            if (strcmp(entry->field_names[j], field->value) == 0) {
                semantic_error_at_node(field, "duplicate field '%s' in class '%s'", field->value, entry->name);
            }
        }
        if (semantic_type_is_union(field_type)) {
            semantic_error_at_node(field, "class fields cannot use union types yet");
        }
        if (field_type == TYPE_NONE) {
            semantic_error_at_node(field, "class fields cannot use None");
        }

        entry->field_names[entry->field_count] = field->value;
        entry->field_types[entry->field_count] = field_type;
        entry->field_count++;
        semantic_record_node_type(info, field, field_type);
    }
}

size_t semantic_class_type_count(void)
{
    return CLASS_TYPE_COUNT;
}

ValueType semantic_class_type_at(size_t index)
{
    if (index >= CLASS_TYPE_COUNT) {
        semantic_error("class type index out of bounds");
    }
    return CLASS_TYPES[index].id;
}

const char *semantic_class_name(ValueType type)
{
    return find_class_type(type)->name;
}

size_t semantic_class_field_count(ValueType type)
{
    return find_class_type(type)->field_count;
}

const char *semantic_class_field_name(ValueType type, size_t index)
{
    ClassTypeInfo *info = find_class_type(type);

    if (index >= info->field_count) {
        semantic_error("class field index out of bounds for %s", info->name);
    }
    return info->field_names[index];
}

ValueType semantic_class_field_type(ValueType type, size_t index)
{
    ClassTypeInfo *info = find_class_type(type);

    if (index >= info->field_count) {
        semantic_error("class field index out of bounds for %s", info->name);
    }
    return info->field_types[index];
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
        ValueType atom = parse_type_atom_node(info, member);

        if ((semantic_type_is_tuple(atom) || semantic_type_is_tuple(type)) &&
            (type != 0 || type_node->child_count > 1)) {
            semantic_error_at_node(type_node, "tuple types cannot be used inside a union yet");
        }
        if ((semantic_type_is_class(atom) || semantic_type_is_class(type)) &&
            (type != 0 || type_node->child_count > 1)) {
            semantic_error_at_node(type_node, "class types cannot be used inside a union yet");
        }

        if (semantic_type_contains(type, atom)) {
            semantic_error_at_node(member, "duplicate type '%s' in union", member->value);
        }

        semantic_record_node_type(info, member, atom);
        type |= atom;
    }

    if ((semantic_type_contains(type, TYPE_LIST_INT) ||
         semantic_type_contains(type, TYPE_LIST_FLOAT) ||
         semantic_type_contains(type, TYPE_LIST_BOOL) ||
         semantic_type_contains(type, TYPE_LIST_CHAR) ||
         semantic_type_contains(type, TYPE_LIST_STR)) &&
        semantic_type_is_union(type)) {
        semantic_error_at_node(type_node, "list types cannot be used inside a union yet");
    }
    if (semantic_type_contains(type, TYPE_DICT_STR_STR) && semantic_type_is_union(type)) {
        semantic_error_at_node(type_node, "dict types cannot be used inside a union yet");
    }
    if (semantic_type_is_union(type)) {
        for (size_t i = 0; i < type_node->child_count; i++) {
            ValueType member_type = parse_type_atom_node(info, type_node->children[i]);
            if (semantic_type_is_tuple(member_type)) {
                semantic_error_at_node(type_node, "tuple types cannot be used inside a union yet");
            }
            if (semantic_type_is_class(member_type)) {
                semantic_error_at_node(type_node, "class types cannot be used inside a union yet");
            }
        }
    }

    semantic_record_node_type(info, type_node, type);
    return type;
}

int semantic_builtin_returns_owned_ref(const char *name)
{
    return strcmp(name, "list_int") == 0 ||
        strcmp(name, "list_float") == 0 ||
        strcmp(name, "list_bool") == 0 ||
        strcmp(name, "list_char") == 0 ||
        strcmp(name, "list_str") == 0 ||
        strcmp(name, "dict_str_str") == 0;
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

MethodInfo *semantic_find_method(MethodInfo *methods, ValueType owner_type, const char *name)
{
    for (MethodInfo *method = methods; method != NULL; method = method->next) {
        if (method->owner_type == owner_type && strcmp(method->name, name) == 0) {
            return method;
        }
    }
    return NULL;
}

const char *semantic_method_c_name(const SemanticInfo *info, ValueType owner_type, const char *method_name)
{
    MethodInfo *method = semantic_find_method(info->methods, owner_type, method_name);

    if (method == NULL) {
        semantic_error("unknown method '%s' on %s", method_name, semantic_type_name(owner_type));
    }
    return method->c_name;
}

size_t semantic_method_arity(const SemanticInfo *info, ValueType owner_type, const char *method_name)
{
    MethodInfo *method = semantic_find_method(info->methods, owner_type, method_name);

    if (method == NULL) {
        semantic_error("unknown method '%s' on %s", method_name, semantic_type_name(owner_type));
    }
    return method->param_count > 0 ? method->param_count - 1 : 0;
}

ValueType semantic_method_parameter_type(
    const SemanticInfo *info,
    ValueType owner_type,
    const char *method_name,
    size_t index)
{
    MethodInfo *method = semantic_find_method(info->methods, owner_type, method_name);

    if (method == NULL) {
        semantic_error("unknown method '%s' on %s", method_name, semantic_type_name(owner_type));
    }
    if (index + 1 >= method->param_count) {
        semantic_error("method parameter index out of bounds");
    }
    return method->param_types[index + 1];
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
