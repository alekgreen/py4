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
    ValueType base_type;
    size_t field_count;
    const char *field_names[MAX_CLASS_FIELDS];
    ValueType field_owner_types[MAX_CLASS_FIELDS];
    ValueType field_types[MAX_CLASS_FIELDS];
    int fields_defined;
    int defining_fields;
} ClassTypeInfo;

typedef struct {
    ValueType id;
    ValueType element_type;
    char name[128];
} ClassListTypeInfo;

typedef struct {
    ValueType id;
    char module_name[128];
    char name[128];
    char qualified_name[192];
    char c_type[128];
    char runtime_prefix[128];
    const ParseNode *node;
} NativeTypeInfo;

typedef struct {
    ValueType id;
    ValueType base_type;
    char name[192];
} OptionalTypeInfo;

typedef struct {
    ValueType id;
    ValueType key_type;
    ValueType value_type;
    char name[192];
} DictTypeInfo;

typedef struct {
    ValueType id;
    const char *name;
    const ParseNode *node;
    size_t variant_count;
    const char *variant_names[MAX_CLASS_FIELDS];
} EnumTypeInfo;

static TupleTypeInfo TUPLE_TYPES[MAX_TUPLE_TYPES];
static size_t TUPLE_TYPE_COUNT = 0;
static ClassTypeInfo CLASS_TYPES[MAX_CLASS_TYPES];
static size_t CLASS_TYPE_COUNT = 0;
static ClassListTypeInfo CLASS_LIST_TYPES[MAX_CLASS_LIST_TYPES];
static size_t CLASS_LIST_TYPE_COUNT = 0;
static NativeTypeInfo NATIVE_TYPES[MAX_NATIVE_TYPES];
static size_t NATIVE_TYPE_COUNT = 0;
static OptionalTypeInfo OPTIONAL_TYPES[MAX_OPTIONAL_TYPES];
static size_t OPTIONAL_TYPE_COUNT = 0;
static DictTypeInfo DICT_TYPES[MAX_DICT_TYPES];
static size_t DICT_TYPE_COUNT = 0;
static EnumTypeInfo ENUM_TYPES[MAX_ENUM_TYPES];
static size_t ENUM_TYPE_COUNT = 0;

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

static ClassListTypeInfo *find_class_list_type(ValueType type)
{
    for (size_t i = 0; i < CLASS_LIST_TYPE_COUNT; i++) {
        if (CLASS_LIST_TYPES[i].id == type) {
            return &CLASS_LIST_TYPES[i];
        }
    }

    semantic_error("unknown class list type id %u", type);
    return NULL;
}

static NativeTypeInfo *find_native_type_info(ValueType type)
{
    for (size_t i = 0; i < NATIVE_TYPE_COUNT; i++) {
        if (NATIVE_TYPES[i].id == type) {
            return &NATIVE_TYPES[i];
        }
    }

    semantic_error("unknown native type id %u", type);
    return NULL;
}

static OptionalTypeInfo *find_optional_type_info(ValueType type)
{
    for (size_t i = 0; i < OPTIONAL_TYPE_COUNT; i++) {
        if (OPTIONAL_TYPES[i].id == type) {
            return &OPTIONAL_TYPES[i];
        }
    }

    semantic_error("unknown optional type id %u", type);
    return NULL;
}

static DictTypeInfo *find_dict_type_info(ValueType type)
{
    for (size_t i = 0; i < DICT_TYPE_COUNT; i++) {
        if (DICT_TYPES[i].id == type) {
            return &DICT_TYPES[i];
        }
    }

    semantic_error("unknown dict type id %u", type);
    return NULL;
}

static EnumTypeInfo *find_enum_type_info(ValueType type)
{
    for (size_t i = 0; i < ENUM_TYPE_COUNT; i++) {
        if (ENUM_TYPES[i].id == type) {
            return &ENUM_TYPES[i];
        }
    }

    semantic_error("unknown enum type id %u", type);
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
    if (semantic_type_is_list(type) && type >= TYPE_CLASS_LIST_BASE) {
        return find_class_list_type(type)->name;
    }
    if (semantic_type_is_class(type)) {
        return find_class_type(type)->name;
    }
    if (semantic_type_is_enum(type)) {
        return find_enum_type_info(type)->name;
    }
    if (semantic_type_is_native(type)) {
        return find_native_type_info(type)->qualified_name;
    }
    if (semantic_type_is_optional(type)) {
        return find_optional_type_info(type)->name;
    }
    if (semantic_type_is_dict(type)) {
        return find_dict_type_info(type)->name;
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

    return 0;
}

ValueType semantic_find_native_type(const char *module_name, const char *name)
{
    for (size_t i = 0; i < NATIVE_TYPE_COUNT; i++) {
        if (strcmp(NATIVE_TYPES[i].name, name) != 0) {
            continue;
        }
        if (module_name != NULL && strcmp(NATIVE_TYPES[i].module_name, module_name) != 0) {
            continue;
        }
        return NATIVE_TYPES[i].id;
    }
    return 0;
}

ValueType semantic_find_enum_type(const char *name)
{
    for (size_t i = 0; i < ENUM_TYPE_COUNT; i++) {
        if (strcmp(ENUM_TYPES[i].name, name) == 0) {
            return ENUM_TYPES[i].id;
        }
    }
    return 0;
}

static ModuleInfo *module_info_for_type_node(SemanticInfo *info, const ParseNode *node)
{
    if (node == NULL || node->source_path == NULL) {
        return NULL;
    }

    for (ModuleInfo *module = info->modules; module != NULL; module = module->next) {
        if (strcmp(module->path, node->source_path) == 0) {
            return module;
        }
    }
    return NULL;
}

static ImportBinding *find_type_import_binding(const ModuleInfo *module, const char *local_name, int module_import_only)
{
    ImportBinding *binding;

    if (module == NULL) {
        return NULL;
    }

    for (binding = module->imports; binding != NULL; binding = binding->next) {
        if (strcmp(binding->local_name, local_name) != 0) {
            continue;
        }
        if (module_import_only && !binding->is_module_import) {
            continue;
        }
        if (!module_import_only && binding->is_module_import) {
            continue;
        }
        return binding;
    }
    return NULL;
}

static int is_module_private_name(const char *name)
{
    return name != NULL && name[0] == '_' && (name[1] == '\0' || name[1] != '_');
}

static int split_module_member_name(
    const char *qualified_name,
    char *module_name,
    size_t module_size,
    char *member_name,
    size_t member_size)
{
    const char *dot;
    size_t module_len;
    size_t member_len;

    if (qualified_name == NULL) {
        return 0;
    }

    dot = strrchr(qualified_name, '.');
    if (dot == NULL) {
        return 0;
    }

    module_len = (size_t)(dot - qualified_name);
    member_len = strlen(dot + 1);
    if (module_len == 0 || member_len == 0 ||
        module_len >= module_size || member_len >= member_size) {
        return 0;
    }

    memcpy(module_name, qualified_name, module_len);
    module_name[module_len] = '\0';
    memcpy(member_name, dot + 1, member_len + 1);
    return 1;
}

static int dict_key_type_supported(ValueType type)
{
    return type == TYPE_INT ||
        type == TYPE_BOOL ||
        type == TYPE_CHAR ||
        type == TYPE_STR ||
        semantic_type_is_enum(type);
}

static int dict_value_type_supported(ValueType type)
{
    return type == TYPE_INT ||
        type == TYPE_FLOAT ||
        type == TYPE_BOOL ||
        type == TYPE_CHAR ||
        type == TYPE_STR ||
        semantic_type_is_optional(type) ||
        semantic_type_is_list(type) ||
        semantic_type_is_dict(type) ||
        semantic_type_is_class(type) ||
        semantic_type_is_tuple(type) ||
        semantic_type_is_native(type) ||
        semantic_type_is_enum(type);
}

static const ParseNode *class_base_type_node(const ParseNode *class_def)
{
    if (class_def != NULL &&
        class_def->child_count > 1 &&
        class_def->children[1]->kind == NODE_TYPE) {
        return class_def->children[1];
    }
    return NULL;
}

static size_t class_member_start_index(const ParseNode *class_def)
{
    return class_base_type_node(class_def) != NULL ? 3 : 2;
}

static ValueType parse_type_atom_node(SemanticInfo *info, const ParseNode *node)
{
    ValueType elements[MAX_TUPLE_ELEMENTS];
    ValueType named_type;

    if (node == NULL || node->kind != NODE_PRIMARY || node->value == NULL) {
        semantic_error("malformed type atom");
    }

    if (node->child_count == 0) {
        const ModuleInfo *current_module = module_info_for_type_node(info, node);
        const char *dot = strchr(node->value, '.');
        size_t value_len = strlen(node->value);

        named_type = parse_named_type_atom(node->value);
        if (named_type != 0) {
            return named_type;
        }

        if (value_len > 6 && strncmp(node->value, "list[", 5) == 0 && node->value[value_len - 1] == ']') {
            char element_name[128];
            size_t element_len = value_len - 6;
            ValueType element_type = 0;

            if (element_len == 0 || element_len >= sizeof(element_name)) {
                semantic_error_at_node(node, "unsupported type '%s'", node->value);
            }

            memcpy(element_name, node->value + 5, element_len);
            element_name[element_len] = '\0';

                element_type = parse_named_type_atom(element_name);
                if (element_type == 0) {
                if (strchr(element_name, '.') != NULL) {
                    char module_local_name[128];
                    char class_name[128];
                    ImportBinding *binding;

                    if (!split_module_member_name(
                            element_name,
                            module_local_name,
                            sizeof(module_local_name),
                            class_name,
                            sizeof(class_name))) {
                        semantic_error_at_node(node, "unsupported type '%s'", node->value);
                    }

                    binding = find_type_import_binding(current_module, module_local_name, 1);
                    if (binding == NULL) {
                        semantic_error_at_node(node, "unknown module '%s' in type annotation", module_local_name);
                    }
                    if (is_module_private_name(class_name)) {
                        semantic_error_at_node(node, "name '%s' is private to module '%s'",
                            class_name,
                            binding->module_name);
                    }

                    element_type = semantic_find_class_type(class_name);
                    if (element_type == 0) {
                        element_type = semantic_find_enum_type(class_name);
                    }
                    if (element_type == 0) {
                        element_type = semantic_find_native_type(binding->module_name, class_name);
                    }
                } else {
                    element_type = semantic_find_class_type(element_name);
                    if (element_type == 0) {
                        element_type = semantic_find_enum_type(element_name);
                    }
                    if (element_type == 0 && current_module != NULL) {
                        element_type = semantic_find_native_type(current_module->name, element_name);
                    }
                    if (element_type != 0 && current_module != NULL) {
                        for (size_t i = 0; i < current_module->root->child_count; i++) {
                            const ParseNode *payload = semantic_statement_payload(current_module->root->children[i]);

                            if ((payload->kind == NODE_CLASS_DEF || payload->kind == NODE_ENUM_DEF ||
                                    payload->kind == NODE_NATIVE_TYPE_DEF) &&
                                strcmp(semantic_expect_child(payload, 0, NODE_PRIMARY)->value, element_name) == 0) {
                                return semantic_make_list_type(element_type);
                            }
                        }

                        {
                            ImportBinding *binding = find_type_import_binding(current_module, element_name, 0);

                            if (binding != NULL && binding->symbol_name != NULL &&
                                strcmp(binding->symbol_name, element_name) == 0) {
                                if (is_module_private_name(binding->symbol_name)) {
                                    semantic_error_at_node(node, "name '%s' is private to module '%s'",
                                        binding->symbol_name,
                                        binding->module_name);
                                }
                                return semantic_make_list_type(element_type);
                            }
                        }
                    }
                    if (element_type != 0 && current_module == NULL) {
                        return semantic_make_list_type(element_type);
                    }
                }
            }

            if (element_type == TYPE_INT || element_type == TYPE_FLOAT ||
                element_type == TYPE_BOOL || element_type == TYPE_CHAR ||
                element_type == TYPE_STR ||
                semantic_type_is_enum(element_type) ||
                semantic_type_is_class(element_type) ||
                semantic_type_is_dict(element_type) ||
                semantic_type_is_native(element_type)) {
                return semantic_make_list_type(element_type);
            }

            semantic_error_at_node(node, "unsupported type '%s'", node->value);
        }

        if (dot != NULL) {
            char module_local_name[128];
            char class_name[128];
            ImportBinding *binding;

            if (!split_module_member_name(
                    node->value,
                    module_local_name,
                    sizeof(module_local_name),
                    class_name,
                    sizeof(class_name))) {
                semantic_error_at_node(node, "unsupported type '%s'", node->value);
            }

            binding = find_type_import_binding(current_module, module_local_name, 1);
            if (binding == NULL) {
                semantic_error_at_node(node, "unknown module '%s' in type annotation", module_local_name);
            }
            if (is_module_private_name(class_name)) {
                semantic_error_at_node(node, "name '%s' is private to module '%s'",
                    class_name,
                    binding->module_name);
            }

            named_type = semantic_find_class_type(class_name);
            if (named_type == 0) {
                named_type = semantic_find_enum_type(class_name);
            }
            if (named_type == 0) {
                named_type = semantic_find_native_type(binding->module_name, class_name);
            }
            if (named_type != 0) {
                return named_type;
            }
        } else {
            named_type = semantic_find_class_type(node->value);
            if (named_type == 0) {
                named_type = semantic_find_enum_type(node->value);
            }
            if (named_type == 0 && current_module != NULL) {
                named_type = semantic_find_native_type(current_module->name, node->value);
            }
            if (named_type != 0 && current_module != NULL) {
                for (size_t i = 0; i < current_module->root->child_count; i++) {
                    const ParseNode *payload = semantic_statement_payload(current_module->root->children[i]);

                    if ((payload->kind == NODE_CLASS_DEF || payload->kind == NODE_ENUM_DEF ||
                            payload->kind == NODE_NATIVE_TYPE_DEF) &&
                        strcmp(semantic_expect_child(payload, 0, NODE_PRIMARY)->value, node->value) == 0) {
                        return named_type;
                    }
                }

                {
                    ImportBinding *binding = find_type_import_binding(current_module, node->value, 0);

                    if (binding != NULL && binding->symbol_name != NULL &&
                        strcmp(binding->symbol_name, node->value) == 0) {
                        if (is_module_private_name(binding->symbol_name)) {
                            semantic_error_at_node(node, "name '%s' is private to module '%s'",
                                binding->symbol_name,
                                binding->module_name);
                        }
                        return named_type;
                    }
                }
            }
            if (named_type != 0 && current_module == NULL) {
                return named_type;
            }
        }

        semantic_error_at_node(node, "unsupported type '%s'", node->value);
    }

    if (strncmp(node->value, "list[", 5) == 0) {
        ValueType element_type;

        if (node->child_count != 1) {
            semantic_error_at_node(node, "malformed list type '%s'", node->value);
        }
        if (node->children[0]->kind == NODE_TYPE) {
            element_type = semantic_parse_type_node(info, node->children[0]);
        } else {
            element_type = parse_type_atom_node(info, semantic_expect_child(node, 0, NODE_PRIMARY));
        }
        return semantic_make_list_type(element_type);
    }

    if (strncmp(node->value, "dict[", 5) == 0) {
        ValueType key_type;
        ValueType value_type;

        if (node->child_count != 2) {
            semantic_error_at_node(node, "malformed dict type '%s'", node->value);
        }

        if (node->children[0]->kind == NODE_TYPE) {
            key_type = semantic_parse_type_node(info, node->children[0]);
        } else {
            key_type = parse_type_atom_node(info, semantic_expect_child(node, 0, NODE_PRIMARY));
        }
        if (node->children[1]->kind == NODE_TYPE) {
            value_type = semantic_parse_type_node(info, node->children[1]);
        } else {
            value_type = parse_type_atom_node(info, semantic_expect_child(node, 1, NODE_PRIMARY));
        }
        return semantic_make_dict_type(key_type, value_type);
    }

    if (node->child_count < 2) {
        semantic_error_at_node(node, "tuple types must have at least two elements");
    }
    if (node->child_count > MAX_TUPLE_ELEMENTS) {
        semantic_error_at_node(node, "tuple types support at most %d elements", MAX_TUPLE_ELEMENTS);
    }

    for (size_t i = 0; i < node->child_count; i++) {
        if (node->children[i]->kind == NODE_TYPE) {
            elements[i] = semantic_parse_type_node(info, node->children[i]);
        } else {
            elements[i] = parse_type_atom_node(info, semantic_expect_child(node, i, NODE_PRIMARY));
        }
    }

    return semantic_make_tuple_type(elements, node->child_count);
}

int semantic_type_contains(ValueType type, ValueType member)
{
    if (semantic_type_is_optional(type)) {
        ValueType base_type = find_optional_type_info(type)->base_type;

        return member == TYPE_NONE || member == base_type;
    }
    if (semantic_type_is_tuple(type) || semantic_type_is_tuple(member) ||
        semantic_type_is_list(type) || semantic_type_is_list(member) ||
        semantic_type_is_dict(type) || semantic_type_is_dict(member) ||
        semantic_type_is_class(type) || semantic_type_is_class(member) ||
        semantic_type_is_enum(type) || semantic_type_is_enum(member) ||
        semantic_type_is_native(type) || semantic_type_is_native(member)) {
        return type == member;
    }
    return member != 0 && (type & member) == member;
}

int semantic_type_is_union(ValueType type)
{
    if (semantic_type_is_tuple(type) || semantic_type_is_list(type) ||
        semantic_type_is_dict(type) ||
        semantic_type_is_class(type) || semantic_type_is_enum(type) || semantic_type_is_native(type) ||
        semantic_type_is_optional(type) ||
        (type & ~TYPE_ATOMIC_MASK) != 0) {
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
    return type >= TYPE_CLASS_BASE && type < TYPE_CLASS_LIST_BASE;
}

int semantic_type_is_enum(ValueType type)
{
    return type >= TYPE_ENUM_BASE && type < TYPE_ENUM_BASE + MAX_ENUM_TYPES;
}

int semantic_type_is_native(ValueType type)
{
    return type >= TYPE_NATIVE_BASE && type < TYPE_NATIVE_BASE + MAX_NATIVE_TYPES;
}

int semantic_type_is_optional(ValueType type)
{
    return type >= TYPE_OPTIONAL_BASE && type < TYPE_OPTIONAL_BASE + MAX_OPTIONAL_TYPES;
}

int semantic_type_is_ref(ValueType type)
{
    return !semantic_type_is_tuple(type) &&
        (semantic_type_is_list(type) || semantic_type_is_dict(type) || semantic_type_is_native(type));
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

    if (semantic_type_is_optional(type)) {
        return semantic_type_needs_management(semantic_optional_base_type(type));
    }

    return 0;
}

int semantic_type_is_list(ValueType type)
{
    return type == TYPE_LIST_INT ||
        type == TYPE_LIST_FLOAT ||
        type == TYPE_LIST_BOOL ||
        type == TYPE_LIST_CHAR ||
        type == TYPE_LIST_STR ||
        (type >= TYPE_CLASS_LIST_BASE &&
            type < TYPE_CLASS_LIST_BASE + MAX_CLASS_LIST_TYPES);
}

int semantic_type_is_dict(ValueType type)
{
    return type >= TYPE_DICT_BASE && type < TYPE_DICT_BASE + MAX_DICT_TYPES;
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
            if (type >= TYPE_CLASS_LIST_BASE) {
                return find_class_list_type(type)->element_type;
            }
            semantic_error("%s is not a list type", semantic_type_name(type));
            return TYPE_NONE;
    }
}

ValueType semantic_dict_key_type(ValueType type)
{
    if (!semantic_type_is_dict(type)) {
        semantic_error("%s is not a dict type", semantic_type_name(type));
    }
    return find_dict_type_info(type)->key_type;
}

ValueType semantic_dict_value_type(ValueType type)
{
    if (!semantic_type_is_dict(type)) {
        semantic_error("%s is not a dict type", semantic_type_name(type));
    }
    return find_dict_type_info(type)->value_type;
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
        TYPE_LIST_STR
    };

    if (semantic_type_is_tuple(type) || semantic_type_is_class(type) ||
        semantic_type_is_enum(type) ||
        semantic_type_is_dict(type) || semantic_type_is_native(type)) {
        return type_member_name(type);
    }
    if (semantic_type_is_optional(type)) {
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
        TYPE_NONE
    };

    if (target == 0 || value == 0) {
        return 0;
    }

    if (semantic_type_is_optional(target)) {
        ValueType base_type = semantic_optional_base_type(target);

        return value == TYPE_NONE || value == base_type || value == target;
    }
    if (semantic_type_is_optional(value)) {
        return target == value;
    }

    if (semantic_type_is_tuple(target) || semantic_type_is_tuple(value) ||
        semantic_type_is_list(target) || semantic_type_is_list(value) ||
        semantic_type_is_dict(target) || semantic_type_is_dict(value) ||
        semantic_type_is_class(target) || semantic_type_is_class(value) ||
        semantic_type_is_enum(target) || semantic_type_is_enum(value) ||
        semantic_type_is_native(target) || semantic_type_is_native(value)) {
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

ValueType semantic_make_list_type(ValueType element_type)
{
    ClassListTypeInfo *entry;

    switch (element_type) {
        case TYPE_INT:
            return TYPE_LIST_INT;
        case TYPE_FLOAT:
            return TYPE_LIST_FLOAT;
        case TYPE_BOOL:
            return TYPE_LIST_BOOL;
        case TYPE_CHAR:
            return TYPE_LIST_CHAR;
        case TYPE_STR:
            return TYPE_LIST_STR;
        default:
            break;
    }

    if (!semantic_type_is_class(element_type) &&
        !semantic_type_is_enum(element_type) &&
        !semantic_type_is_optional(element_type) &&
        !semantic_type_is_native(element_type) &&
        !semantic_type_is_tuple(element_type) &&
        !semantic_type_is_list(element_type) &&
        !semantic_type_is_dict(element_type)) {
        semantic_error("list elements must currently be int, float, bool, char, str, enum, optional, tuple, list, dict, class, or native opaque values");
    }

    for (size_t i = 0; i < CLASS_LIST_TYPE_COUNT; i++) {
        if (CLASS_LIST_TYPES[i].element_type == element_type) {
            return CLASS_LIST_TYPES[i].id;
        }
    }

    if (CLASS_LIST_TYPE_COUNT >= MAX_CLASS_LIST_TYPES) {
        semantic_error("too many class list types in one program");
    }

    entry = &CLASS_LIST_TYPES[CLASS_LIST_TYPE_COUNT];
    entry->id = TYPE_CLASS_LIST_BASE + (ValueType)CLASS_LIST_TYPE_COUNT;
    entry->element_type = element_type;
    snprintf(entry->name, sizeof(entry->name), "list[%s]", semantic_type_name(element_type));
    CLASS_LIST_TYPE_COUNT++;
    return entry->id;
}

ValueType semantic_make_dict_type(ValueType key_type, ValueType value_type)
{
    DictTypeInfo *entry;

    if (!dict_key_type_supported(key_type)) {
        semantic_error("dict keys must currently be int, bool, char, str, or enum");
    }
    if (!dict_value_type_supported(value_type)) {
        semantic_error("dict values must currently be int, float, bool, char, str, enum values, optional values, list values, dict values, class values, tuple values, or native opaque values");
    }

    for (size_t i = 0; i < DICT_TYPE_COUNT; i++) {
        if (DICT_TYPES[i].key_type == key_type && DICT_TYPES[i].value_type == value_type) {
            return DICT_TYPES[i].id;
        }
    }

    if (DICT_TYPE_COUNT >= MAX_DICT_TYPES) {
        semantic_error("too many dict types in one program");
    }

    entry = &DICT_TYPES[DICT_TYPE_COUNT];
    entry->id = TYPE_DICT_BASE + (ValueType)DICT_TYPE_COUNT;
    entry->key_type = key_type;
    entry->value_type = value_type;
    snprintf(entry->name, sizeof(entry->name), "dict[%s, %s]",
        semantic_type_name(key_type),
        semantic_type_name(value_type));
    DICT_TYPE_COUNT++;
    return entry->id;
}

ValueType semantic_make_optional_type(ValueType base_type)
{
    OptionalTypeInfo *entry;

    if (base_type == TYPE_NONE ||
        semantic_type_is_union(base_type) ||
        semantic_type_is_optional(base_type)) {
        semantic_error("optional base type must be a concrete non-union type, got %s",
            semantic_type_name(base_type));
    }

    for (size_t i = 0; i < OPTIONAL_TYPE_COUNT; i++) {
        if (OPTIONAL_TYPES[i].base_type == base_type) {
            return OPTIONAL_TYPES[i].id;
        }
    }

    if (OPTIONAL_TYPE_COUNT >= MAX_OPTIONAL_TYPES) {
        semantic_error("too many optional types");
    }

    entry = &OPTIONAL_TYPES[OPTIONAL_TYPE_COUNT];
    entry->id = TYPE_OPTIONAL_BASE + (ValueType)OPTIONAL_TYPE_COUNT;
    entry->base_type = base_type;
    snprintf(entry->name, sizeof(entry->name), "%s | None", semantic_type_name(base_type));
    OPTIONAL_TYPE_COUNT++;
    return entry->id;
}

ValueType semantic_optional_base_type(ValueType type)
{
    if (!semantic_type_is_optional(type)) {
        semantic_error("%s is not an optional type", semantic_type_name(type));
    }
    return find_optional_type_info(type)->base_type;
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

size_t semantic_list_type_count(void)
{
    return 5 + CLASS_LIST_TYPE_COUNT;
}

ValueType semantic_list_type_at(size_t index)
{
    switch (index) {
        case 0: return TYPE_LIST_INT;
        case 1: return TYPE_LIST_FLOAT;
        case 2: return TYPE_LIST_BOOL;
        case 3: return TYPE_LIST_CHAR;
        case 4: return TYPE_LIST_STR;
        default:
            if (index - 5 >= CLASS_LIST_TYPE_COUNT) {
                semantic_error("list type index out of bounds");
            }
            return CLASS_LIST_TYPES[index - 5].id;
    }
}

size_t semantic_dict_type_count(void)
{
    return DICT_TYPE_COUNT;
}

ValueType semantic_dict_type_at(size_t index)
{
    if (index >= DICT_TYPE_COUNT) {
        semantic_error("dict type index out of bounds");
    }
    return DICT_TYPES[index].id;
}

size_t semantic_enum_type_count(void)
{
    return ENUM_TYPE_COUNT;
}

ValueType semantic_enum_type_at(size_t index)
{
    if (index >= ENUM_TYPE_COUNT) {
        semantic_error("enum type index out of bounds");
    }
    return ENUM_TYPES[index].id;
}

const char *semantic_enum_name(ValueType type)
{
    return find_enum_type_info(type)->name;
}

size_t semantic_enum_variant_count(ValueType type)
{
    return find_enum_type_info(type)->variant_count;
}

const char *semantic_enum_variant_name(ValueType type, size_t index)
{
    EnumTypeInfo *info = find_enum_type_info(type);

    if (index >= info->variant_count) {
        semantic_error("enum variant index out of bounds for %s", info->name);
    }
    return info->variant_names[index];
}

int semantic_enum_variant_index(ValueType type, const char *name, size_t *index_out)
{
    EnumTypeInfo *info = find_enum_type_info(type);

    for (size_t i = 0; i < info->variant_count; i++) {
        if (strcmp(info->variant_names[i], name) == 0) {
            if (index_out != NULL) {
                *index_out = i;
            }
            return 1;
        }
    }
    return 0;
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

void semantic_register_native_type(const char *module_name, const ParseNode *type_def)
{
    const ParseNode *name_node;
    const char *name;
    NativeTypeInfo *entry;

    if (type_def == NULL || type_def->kind != NODE_NATIVE_TYPE_DEF) {
        semantic_error("malformed native type definition");
    }

    name_node = semantic_expect_child(type_def, 0, NODE_PRIMARY);
    name = name_node->value;
    if (semantic_find_native_type(module_name, name) != 0) {
        semantic_error_at_node(name_node, "duplicate native type '%s' in module '%s'", name, module_name);
    }
    if (semantic_find_enum_type(name) != 0) {
        semantic_error_at_node(name_node, "native type name '%s' conflicts with enum", name);
    }
    if (parse_named_type_atom(name) != 0) {
        semantic_error_at_node(name_node, "native type name '%s' conflicts with built-in type", name);
    }
    if (NATIVE_TYPE_COUNT >= MAX_NATIVE_TYPES) {
        semantic_error("too many native types in one program");
    }

    entry = &NATIVE_TYPES[NATIVE_TYPE_COUNT];
    entry->id = TYPE_NATIVE_BASE + (ValueType)NATIVE_TYPE_COUNT;
    snprintf(entry->module_name, sizeof(entry->module_name), "%s", module_name);
    snprintf(entry->name, sizeof(entry->name), "%s", name);
    snprintf(entry->qualified_name, sizeof(entry->qualified_name), "%s.%s", module_name, name);
    if (strcmp(module_name, "io") == 0 && strcmp(name, "File") == 0) {
        snprintf(entry->c_type, sizeof(entry->c_type), "Py4IoFile");
        snprintf(entry->runtime_prefix, sizeof(entry->runtime_prefix), "py4_io_file");
    } else if (strcmp(module_name, "json") == 0 && strcmp(name, "Value") == 0) {
        snprintf(entry->c_type, sizeof(entry->c_type), "Py4JsonValue");
        snprintf(entry->runtime_prefix, sizeof(entry->runtime_prefix), "py4_json_value");
    } else {
        semantic_error_at_node(name_node, "unsupported native type '%s.%s'", module_name, name);
    }
    entry->node = type_def;
    NATIVE_TYPE_COUNT++;
}

ValueType semantic_register_enum(const ParseNode *enum_def)
{
    const ParseNode *name_node;
    const char *name;
    ValueType builtin_type;
    EnumTypeInfo *entry;

    if (enum_def == NULL || enum_def->kind != NODE_ENUM_DEF) {
        semantic_error("malformed enum definition");
    }

    name_node = semantic_expect_child(enum_def, 0, NODE_PRIMARY);
    name = name_node->value;
    if (semantic_find_enum_type(name) != 0) {
        semantic_error_at_node(name_node, "duplicate enum '%s'", name);
    }
    if (semantic_find_class_type(name) != 0) {
        semantic_error_at_node(name_node, "enum name '%s' conflicts with class", name);
    }

    builtin_type = parse_named_type_atom(name);
    if (builtin_type != 0) {
        semantic_error_at_node(name_node, "enum name '%s' conflicts with built-in type", name);
    }

    if (ENUM_TYPE_COUNT >= MAX_ENUM_TYPES) {
        semantic_error("too many enums in one program");
    }

    entry = &ENUM_TYPES[ENUM_TYPE_COUNT];
    entry->id = TYPE_ENUM_BASE + (ValueType)ENUM_TYPE_COUNT;
    entry->name = name;
    entry->node = enum_def;
    entry->variant_count = 0;

    for (size_t i = 2; i < enum_def->child_count; i++) {
        const ParseNode *member = enum_def->children[i];

        if (member->kind != NODE_ENUM_MEMBER) {
            semantic_error("malformed enum definition");
        }
        if (entry->variant_count >= MAX_CLASS_FIELDS) {
            semantic_error_at_node(member, "enum '%s' supports at most %d members", name, MAX_CLASS_FIELDS);
        }
        for (size_t j = 0; j < entry->variant_count; j++) {
            if (strcmp(entry->variant_names[j], member->value) == 0) {
                semantic_error_at_node(member, "duplicate enum member '%s' in enum '%s'", member->value, name);
            }
        }
        entry->variant_names[entry->variant_count++] = member->value;
    }

    ENUM_TYPE_COUNT++;
    return entry->id;
}

int semantic_class_has_initializer(const SemanticInfo *info, ValueType type)
{
    return semantic_find_method(info->methods, type, "__init__") != NULL;
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
    if (semantic_find_enum_type(name) != 0) {
        semantic_error_at_node(name_node, "class name '%s' conflicts with enum", name);
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
    entry->base_type = 0;
    entry->field_count = 0;
    entry->fields_defined = 0;
    entry->defining_fields = 0;
    CLASS_TYPE_COUNT++;
    return entry->id;
}

void semantic_define_class_fields(SemanticInfo *info, const ParseNode *class_def)
{
    ClassTypeInfo *entry;
    ValueType class_type;
    const ParseNode *base_node;

    class_type = semantic_find_class_type(semantic_expect_child(class_def, 0, NODE_PRIMARY)->value);
    if (class_type == 0) {
        semantic_error("class definition missing from registry");
    }
    entry = find_class_type(class_type);
    if (entry->fields_defined) {
        return;
    }
    if (entry->defining_fields) {
        semantic_error_at_node(class_def->children[0], "inheritance cycle detected for class '%s'", entry->name);
    }
    entry->defining_fields = 1;

    base_node = class_base_type_node(class_def);
    if (base_node != NULL) {
        ValueType base_type = semantic_parse_type_node(info, base_node);
        ClassTypeInfo *base_entry;

        if (!semantic_type_is_class(base_type)) {
            semantic_error_at_node(base_node, "base type for class '%s' must be a class", entry->name);
        }
        if (base_type == class_type) {
            semantic_error_at_node(base_node, "class '%s' cannot inherit from itself", entry->name);
        }

        base_entry = find_class_type(base_type);
        semantic_define_class_fields(info, base_entry->node);
        entry->base_type = base_type;

        for (size_t i = 0; i < base_entry->field_count; i++) {
            entry->field_names[entry->field_count] = base_entry->field_names[i];
            entry->field_owner_types[entry->field_count] = base_entry->field_owner_types[i];
            entry->field_types[entry->field_count] = base_entry->field_types[i];
            entry->field_count++;
        }
    }

    for (size_t i = class_member_start_index(class_def); i < class_def->child_count; i++) {
        const ParseNode *field = class_def->children[i];
        const ParseNode *type_node;
        ValueType field_type;

        if (field->kind == NODE_FUNCTION_DEF || field->kind == NODE_NATIVE_FUNCTION_DEF) {
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
        entry->field_owner_types[entry->field_count] = class_type;
        entry->field_types[entry->field_count] = field_type;
        entry->field_count++;
        semantic_record_node_type(info, field, field_type);
    }
    entry->defining_fields = 0;
    entry->fields_defined = 1;
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

ValueType semantic_class_base_type(ValueType type)
{
    return find_class_type(type)->base_type;
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

ValueType semantic_class_field_owner_type(ValueType type, size_t index)
{
    ClassTypeInfo *info = find_class_type(type);

    if (index >= info->field_count) {
        semantic_error("class field index out of bounds for %s", info->name);
    }
    return info->field_owner_types[index];
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

    if (type_node->child_count == 2) {
        ValueType first_type = parse_type_atom_node(info, semantic_expect_child(type_node, 0, NODE_PRIMARY));
        ValueType second_type = parse_type_atom_node(info, semantic_expect_child(type_node, 1, NODE_PRIMARY));
        ValueType base_type = 0;

        semantic_record_node_type(info, type_node->children[0], first_type);
        semantic_record_node_type(info, type_node->children[1], second_type);

        if (first_type == TYPE_NONE &&
            second_type != TYPE_NONE &&
            !semantic_type_is_union(second_type) &&
            !semantic_type_is_optional(second_type)) {
            base_type = second_type;
        } else if (second_type == TYPE_NONE &&
            first_type != TYPE_NONE &&
            !semantic_type_is_union(first_type) &&
            !semantic_type_is_optional(first_type)) {
            base_type = first_type;
        }

        if (base_type != 0) {
            type = semantic_make_optional_type(base_type);
            semantic_record_node_type(info, type_node, type);
            return type;
        }
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
        if ((semantic_type_is_enum(atom) || semantic_type_is_enum(type)) &&
            (type != 0 || type_node->child_count > 1)) {
            semantic_error_at_node(type_node, "enum types cannot be used inside a union yet");
        }
        if ((semantic_type_is_list(atom) || semantic_type_is_list(type)) &&
            (type != 0 || type_node->child_count > 1)) {
            semantic_error_at_node(type_node, "list types cannot be used inside a union yet");
        }
        if ((semantic_type_is_dict(atom) || semantic_type_is_dict(type)) &&
            (type != 0 || type_node->child_count > 1)) {
            semantic_error_at_node(type_node, "dict types cannot be used inside a union yet");
        }

        if (semantic_type_contains(type, atom)) {
            semantic_error_at_node(member, "duplicate type '%s' in union", member->value);
        }

        semantic_record_node_type(info, member, atom);
        type |= atom;
    }

    if (semantic_type_is_union(type)) {
        for (size_t i = 0; i < type_node->child_count; i++) {
            ValueType member_type = parse_type_atom_node(info, type_node->children[i]);
            if (semantic_type_is_tuple(member_type)) {
                semantic_error_at_node(type_node, "tuple types cannot be used inside a union yet");
            }
            if (semantic_type_is_dict(member_type)) {
                semantic_error_at_node(type_node, "dict types cannot be used inside a union yet");
            }
            if (semantic_type_is_class(member_type)) {
                semantic_error_at_node(type_node, "class types cannot be used inside a union yet");
            }
            if (semantic_type_is_enum(member_type)) {
                semantic_error_at_node(type_node, "enum types cannot be used inside a union yet");
            }
            if (semantic_type_is_native(member_type)) {
                semantic_error_at_node(type_node, "native opaque types cannot be used inside a union yet");
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

FunctionInfo *semantic_find_function_by_node(FunctionInfo *functions, const ParseNode *node)
{
    for (FunctionInfo *fn = functions; fn != NULL; fn = fn->next) {
        if (fn->node == node) {
            return fn;
        }
    }
    return NULL;
}

void semantic_record_call_target(SemanticInfo *info, const ParseNode *call, FunctionInfo *function)
{
    CallTargetInfo *target;

    for (target = info->call_targets; target != NULL; target = target->next) {
        if (target->call == call) {
            target->function = function;
            return;
        }
    }

    target = malloc(sizeof(CallTargetInfo));
    if (target == NULL) {
        perror("malloc");
        exit(1);
    }

    target->call = call;
    target->function = function;
    target->next = info->call_targets;
    info->call_targets = target;
}

FunctionInfo *semantic_resolved_call_target(const SemanticInfo *info, const ParseNode *call)
{
    for (CallTargetInfo *target = info->call_targets; target != NULL; target = target->next) {
        if (target->call == call) {
            return target->function;
        }
    }
    return NULL;
}

const char *semantic_function_c_name(const SemanticInfo *info, const ParseNode *function_def)
{
    FunctionInfo *fn = semantic_find_function_by_node(info->functions, function_def);

    if (fn == NULL) {
        semantic_error("unknown function definition during code generation");
    }
    return fn->c_name;
}

const char *semantic_call_c_name(const SemanticInfo *info, const ParseNode *call)
{
    FunctionInfo *fn = semantic_resolved_call_target(info, call);

    if (fn == NULL) {
        semantic_error("unresolved function call during code generation");
    }
    return fn->c_name;
}

int semantic_has_call_target(const SemanticInfo *info, const ParseNode *call)
{
    return semantic_resolved_call_target(info, call) != NULL;
}

void semantic_record_constructor_target(SemanticInfo *info, const ParseNode *call, ValueType class_type)
{
    ConstructorTargetInfo *target;

    for (target = info->constructor_targets; target != NULL; target = target->next) {
        if (target->call == call) {
            target->class_type = class_type;
            return;
        }
    }

    target = malloc(sizeof(ConstructorTargetInfo));
    if (target == NULL) {
        perror("malloc");
        exit(1);
    }

    target->call = call;
    target->class_type = class_type;
    target->next = info->constructor_targets;
    info->constructor_targets = target;
}

ValueType semantic_resolved_constructor_target(const SemanticInfo *info, const ParseNode *call)
{
    for (ConstructorTargetInfo *target = info->constructor_targets; target != NULL; target = target->next) {
        if (target->call == call) {
            return target->class_type;
        }
    }
    return 0;
}

void semantic_record_enum_variant_target(
    SemanticInfo *info,
    const ParseNode *node,
    ValueType enum_type,
    size_t variant_index)
{
    EnumVariantTargetInfo *target;

    for (target = info->enum_variant_targets; target != NULL; target = target->next) {
        if (target->node == node) {
            target->enum_type = enum_type;
            target->variant_index = variant_index;
            return;
        }
    }

    target = malloc(sizeof(EnumVariantTargetInfo));
    if (target == NULL) {
        perror("malloc");
        exit(1);
    }

    target->node = node;
    target->enum_type = enum_type;
    target->variant_index = variant_index;
    target->next = info->enum_variant_targets;
    info->enum_variant_targets = target;
}

EnumVariantTargetInfo *semantic_find_enum_variant_target(const SemanticInfo *info, const ParseNode *node)
{
    for (EnumVariantTargetInfo *target = info->enum_variant_targets; target != NULL; target = target->next) {
        if (target->node == node) {
            return target;
        }
    }
    return NULL;
}

int semantic_enum_variant_for_node(
    const SemanticInfo *info,
    const ParseNode *node,
    ValueType *enum_type_out,
    size_t *variant_index_out)
{
    EnumVariantTargetInfo *target = semantic_find_enum_variant_target(info, node);

    if (target == NULL) {
        return 0;
    }
    if (enum_type_out != NULL) {
        *enum_type_out = target->enum_type;
    }
    if (variant_index_out != NULL) {
        *variant_index_out = target->variant_index;
    }
    return 1;
}

ValueType semantic_call_constructor_type(const SemanticInfo *info, const ParseNode *call)
{
    return semantic_resolved_constructor_target(info, call);
}

size_t semantic_call_arity(const SemanticInfo *info, const ParseNode *call)
{
    FunctionInfo *fn = semantic_resolved_call_target(info, call);

    if (fn == NULL) {
        semantic_error("unresolved function call during code generation");
    }
    return fn->param_count;
}

ValueType semantic_call_parameter_type(const SemanticInfo *info, const ParseNode *call, size_t index)
{
    FunctionInfo *fn = semantic_resolved_call_target(info, call);

    if (fn == NULL) {
        semantic_error("unresolved function call during code generation");
    }
    if (index >= fn->param_count) {
        semantic_error("call parameter index out of range");
    }
    return fn->param_types[index];
}

ModuleInfo *semantic_find_module_info(ModuleInfo *modules, const char *name)
{
    for (ModuleInfo *module = modules; module != NULL; module = module->next) {
        if (strcmp(module->name, name) == 0) {
            return module;
        }
    }
    return NULL;
}

const char *semantic_module_name_for_path(const SemanticInfo *info, const char *path)
{
    for (ModuleInfo *module = info->modules; module != NULL; module = module->next) {
        if (strcmp(module->path, path) == 0) {
            return module->name;
        }
    }
    return NULL;
}

static char *semantic_module_ref_string(const ParseNode *node)
{
    if (node == NULL) {
        return NULL;
    }

    if (node->kind == NODE_PRIMARY && node->token_type == TOKEN_IDENTIFIER && node->value != NULL) {
        size_t len = strlen(node->value) + 1;
        char *copy = malloc(len);

        if (copy == NULL) {
            perror("malloc");
            exit(1);
        }

        memcpy(copy, node->value, len);
        return copy;
    }

    if (node->kind == NODE_FIELD_ACCESS) {
        char *base = semantic_module_ref_string(node->children[0]);
        const ParseNode *field = semantic_expect_child(node, 1, NODE_PRIMARY);
        char *joined;
        size_t len;

        if (base == NULL || field->value == NULL) {
            free(base);
            return NULL;
        }

        len = strlen(base) + strlen(field->value) + 2;
        joined = malloc(len);
        if (joined == NULL) {
            perror("malloc");
            exit(1);
        }
        snprintf(joined, len, "%s.%s", base, field->value);
        free(base);
        return joined;
    }

    return NULL;
}

const char *semantic_module_name_for_receiver(const SemanticInfo *info, const ParseNode *receiver)
{
    const char *current_module_name;
    ModuleInfo *module;
    char *name;

    if (receiver == NULL || receiver->source_path == NULL) {
        return NULL;
    }

    current_module_name = semantic_module_name_for_path(info, receiver->source_path);
    if (current_module_name == NULL) {
        return NULL;
    }

    module = semantic_find_module_info(info->modules, current_module_name);
    if (module == NULL) {
        return NULL;
    }

    name = semantic_module_ref_string(receiver);
    if (name == NULL) {
        return NULL;
    }

    for (ImportBinding *binding = module->imports; binding != NULL; binding = binding->next) {
        if (binding->is_module_import && strcmp(binding->local_name, name) == 0) {
            free(name);
            return binding->module_name;
        }
    }

    free(name);
    return NULL;
}

size_t semantic_native_type_count(void)
{
    return NATIVE_TYPE_COUNT;
}

ValueType semantic_native_type_at(size_t index)
{
    if (index >= NATIVE_TYPE_COUNT) {
        semantic_error("native type index out of bounds");
    }
    return NATIVE_TYPES[index].id;
}

size_t semantic_optional_type_count(void)
{
    return OPTIONAL_TYPE_COUNT;
}

ValueType semantic_optional_type_at(size_t index)
{
    if (index >= OPTIONAL_TYPE_COUNT) {
        semantic_error("optional type index out of bounds");
    }
    return OPTIONAL_TYPES[index].id;
}

const char *semantic_native_type_name(ValueType type)
{
    return find_native_type_info(type)->name;
}

const char *semantic_native_type_module(ValueType type)
{
    return find_native_type_info(type)->module_name;
}

const char *semantic_native_c_type(ValueType type)
{
    return find_native_type_info(type)->c_type;
}

const char *semantic_native_runtime_prefix(ValueType type)
{
    return find_native_type_info(type)->runtime_prefix;
}

GlobalBinding *semantic_find_global(GlobalBinding *globals, const char *module_name, const char *name)
{
    for (GlobalBinding *global = globals; global != NULL; global = global->next) {
        if (strcmp(global->module_name, module_name) == 0 && strcmp(global->name, name) == 0) {
            return global;
        }
    }
    return NULL;
}

void semantic_record_global_target(
    SemanticInfo *info,
    const ParseNode *node,
    const char *module_name,
    const char *name)
{
    GlobalTargetInfo *target;

    for (target = info->global_targets; target != NULL; target = target->next) {
        if (target->node == node) {
            return;
        }
    }

    target = malloc(sizeof(GlobalTargetInfo));
    if (target == NULL) {
        perror("malloc");
        exit(1);
    }

    target->node = node;
    target->module_name = module_name;
    target->name = name;
    target->next = info->global_targets;
    info->global_targets = target;
}

GlobalTargetInfo *semantic_find_global_target(const SemanticInfo *info, const ParseNode *node)
{
    for (GlobalTargetInfo *target = info->global_targets; target != NULL; target = target->next) {
        if (target->node == node) {
            return target;
        }
    }
    return NULL;
}

void semantic_record_inferred_declaration_target(SemanticInfo *info, const ParseNode *node)
{
    InferredDeclTargetInfo *target;

    for (target = info->inferred_decl_targets; target != NULL; target = target->next) {
        if (target->node == node) {
            return;
        }
    }

    target = malloc(sizeof(InferredDeclTargetInfo));
    if (target == NULL) {
        perror("malloc");
        exit(1);
    }

    target->node = node;
    target->next = info->inferred_decl_targets;
    info->inferred_decl_targets = target;
}

int semantic_is_inferred_declaration_target(const SemanticInfo *info, const ParseNode *node)
{
    for (InferredDeclTargetInfo *target = info->inferred_decl_targets; target != NULL; target = target->next) {
        if (target->node == node) {
            return 1;
        }
    }
    return 0;
}

const char *semantic_global_c_name(const SemanticInfo *info, const char *module_name, const char *name)
{
    static char buffer[256];
    char module_buffer[128];
    GlobalBinding *global = semantic_find_global(info->globals, module_name, name);

    if (global == NULL) {
        semantic_error("unknown global '%s' in module '%s'", name, module_name);
    }

    snprintf(module_buffer, sizeof(module_buffer), "%s", module_name);
    for (size_t i = 0; module_buffer[i] != '\0'; i++) {
        if (module_buffer[i] == '.') {
            module_buffer[i] = '_';
        }
    }

    snprintf(buffer, sizeof(buffer), "py4_global_%s_%s", module_buffer, name);
    return buffer;
}

const char *semantic_global_target_c_name(const SemanticInfo *info, const ParseNode *node)
{
    GlobalTargetInfo *target = semantic_find_global_target(info, node);

    if (target == NULL) {
        return NULL;
    }
    return semantic_global_c_name(info, target->module_name, target->name);
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
    binding->module_name = scope->parent == NULL && scope->module != NULL ? scope->module->name : NULL;
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
