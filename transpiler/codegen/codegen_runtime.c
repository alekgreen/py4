#include <stdio.h>
#include <stdlib.h>

#include "codegen_internal.h"

static void emit_runtime_value_retain(CodegenContext *ctx, ValueType type, const char *expr)
{
    char helper_name[MAX_NAME_LEN];

    if (!semantic_type_needs_management(type)) {
        return;
    }
    if (semantic_type_is_ref(type)) {
        fprintf(ctx->out, "    %s_incref(%s);\n", codegen_ref_runtime_prefix(type), expr);
        return;
    }
    if (semantic_type_is_class(type)) {
        codegen_build_class_retain_name(helper_name, sizeof(helper_name), type);
        fprintf(ctx->out, "    %s(&%s);\n", helper_name, expr);
        return;
    }
    if (semantic_type_is_tuple(type)) {
        codegen_build_tuple_retain_name(helper_name, sizeof(helper_name), type);
        fprintf(ctx->out, "    %s(&%s);\n", helper_name, expr);
        return;
    }
    if (semantic_type_is_optional(type)) {
        codegen_build_optional_retain_name(helper_name, sizeof(helper_name), type);
        fprintf(ctx->out, "    %s(&%s);\n", helper_name, expr);
        return;
    }
    codegen_error("unsupported managed list element type %s", semantic_type_name(type));
}

static void emit_runtime_value_release(CodegenContext *ctx, ValueType type, const char *expr)
{
    char helper_name[MAX_NAME_LEN];

    if (!semantic_type_needs_management(type)) {
        return;
    }
    if (semantic_type_is_ref(type)) {
        fprintf(ctx->out, "        %s_decref(%s);\n", codegen_ref_runtime_prefix(type), expr);
        return;
    }
    if (semantic_type_is_class(type)) {
        codegen_build_class_release_name(helper_name, sizeof(helper_name), type);
        fprintf(ctx->out, "        %s(&%s);\n", helper_name, expr);
        return;
    }
    if (semantic_type_is_tuple(type)) {
        codegen_build_tuple_release_name(helper_name, sizeof(helper_name), type);
        fprintf(ctx->out, "        %s(&%s);\n", helper_name, expr);
        return;
    }
    if (semantic_type_is_optional(type)) {
        codegen_build_optional_release_name(helper_name, sizeof(helper_name), type);
        fprintf(ctx->out, "        %s(&%s);\n", helper_name, expr);
        return;
    }
    codegen_error("unsupported managed list element type %s", semantic_type_name(type));
}

static void emit_runtime_value_print(CodegenContext *ctx, ValueType type, const char *expr)
{
    char helper_name[MAX_NAME_LEN];

    if (semantic_type_is_class(type)) {
        codegen_build_class_print_name(helper_name, sizeof(helper_name), type);
        fprintf(ctx->out, "            %s(%s);\n", helper_name, expr);
        return;
    }
    if (semantic_type_is_list(type)) {
        codegen_build_list_print_name(helper_name, sizeof(helper_name), type);
        fprintf(ctx->out, "            %s(%s);\n", helper_name, expr);
        return;
    }
    if (semantic_type_is_dict(type)) {
        codegen_build_dict_print_name(helper_name, sizeof(helper_name), type);
        fprintf(ctx->out, "            %s(%s);\n", helper_name, expr);
        return;
    }
    if (semantic_type_is_tuple(type)) {
        codegen_build_tuple_print_name(helper_name, sizeof(helper_name), type);
        fprintf(ctx->out, "            %s(%s);\n", helper_name, expr);
        return;
    }
    if (semantic_type_is_optional(type)) {
        ValueType base_type = semantic_optional_base_type(type);
        char *value_expr = codegen_dup_printf("%s.value", expr);

        fprintf(ctx->out, "            if (%s.is_none) {\n", expr);
        fprintf(ctx->out, "                printf(\"None\");\n");
        fprintf(ctx->out, "            } else {\n");
        emit_runtime_value_print(ctx, base_type, value_expr);
        fprintf(ctx->out, "            }\n");
        free(value_expr);
        return;
    }
    if (semantic_type_is_native(type)) {
        fprintf(ctx->out, "            printf(\"<%s.%s>\");\n",
            semantic_native_type_module(type),
            semantic_native_type_name(type));
        return;
    }
    if (semantic_type_is_enum(type)) {
        char helper_name[MAX_NAME_LEN];

        codegen_build_enum_print_name(helper_name, sizeof(helper_name), type);
        fprintf(ctx->out, "            %s(%s);\n", helper_name, expr);
        return;
    }

    switch (type) {
        case TYPE_INT:
            fprintf(ctx->out, "            printf(\"%%d\", %s);\n", expr);
            return;
        case TYPE_FLOAT:
            fprintf(ctx->out, "            printf(\"%%g\", %s);\n", expr);
            return;
        case TYPE_BOOL:
            fprintf(ctx->out, "            printf(\"%%s\", %s ? \"True\" : \"False\");\n", expr);
            return;
        case TYPE_CHAR:
            fprintf(ctx->out, "            printf(\"%%c\", %s);\n", expr);
            return;
        case TYPE_STR:
            fprintf(ctx->out, "            printf(\"%%s\", %s);\n", expr);
            return;
        default:
            codegen_error("unsupported list print type %s", semantic_type_name(type));
    }
}

static void emit_list_runtime(CodegenContext *ctx, ValueType list_type)
{
    char struct_name[MAX_NAME_LEN];
    char prefix[MAX_NAME_LEN];
    char type_name[MAX_NAME_LEN];
    const char *item_c_type = codegen_list_element_c_type(list_type);
    ValueType element_type = semantic_list_element_type(list_type);

    snprintf(struct_name, sizeof(struct_name), "%s", codegen_list_struct_name(list_type));
    snprintf(prefix, sizeof(prefix), "%s", codegen_list_runtime_prefix(list_type));
    snprintf(type_name, sizeof(type_name), "%s", semantic_type_name(list_type));

    fprintf(ctx->out, "typedef struct %s {\n", struct_name);
    fprintf(ctx->out, "    int refcount;\n");
    fprintf(ctx->out, "    size_t len;\n");
    fprintf(ctx->out, "    size_t cap;\n");
    fprintf(ctx->out, "    %s *items;\n", item_c_type);
    fprintf(ctx->out, "} %s;\n\n", struct_name);

    fprintf(ctx->out, "static void %s_bounds_check(%s *list, int index)\n{\n", prefix, struct_name);
    fprintf(ctx->out, "    if (list == NULL) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: %s is null\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    if (index < 0 || (size_t)index >= list->len) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: %s index out of bounds\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static %s *%s_new(void)\n{\n", struct_name, prefix);
    fprintf(ctx->out, "    %s *list = calloc(1, sizeof(%s));\n", struct_name, struct_name);
    fprintf(ctx->out, "    if (list == NULL) {\n");
    fprintf(ctx->out, "        perror(\"calloc\");\n");
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    list->refcount = 1;\n");
    fprintf(ctx->out, "    return list;\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static void %s_incref(%s *list)\n{\n", prefix, struct_name);
    fprintf(ctx->out, "    if (list != NULL) {\n");
    fprintf(ctx->out, "        list->refcount++;\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static void %s_decref(%s *list)\n{\n", prefix, struct_name);
    fprintf(ctx->out, "    if (list == NULL) {\n");
    fprintf(ctx->out, "        return;\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    list->refcount--;\n");
    fprintf(ctx->out, "    if (list->refcount == 0) {\n");
    if (semantic_type_needs_management(element_type)) {
        fprintf(ctx->out, "        for (size_t i = 0; i < list->len; i++) {\n");
        emit_runtime_value_release(ctx, element_type, "list->items[i]");
        fprintf(ctx->out, "        }\n");
    }
    fprintf(ctx->out, "        free(list->items);\n");
    fprintf(ctx->out, "        free(list);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static void %s_ensure_capacity(%s *list, size_t needed)\n{\n", prefix, struct_name);
    fprintf(ctx->out, "    size_t new_cap;\n");
    fprintf(ctx->out, "    %s *items;\n\n", item_c_type);
    fprintf(ctx->out, "    if (list == NULL) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: %s is null\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    if (list->cap >= needed) {\n");
    fprintf(ctx->out, "        return;\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    new_cap = list->cap == 0 ? 4 : list->cap * 2;\n");
    fprintf(ctx->out, "    while (new_cap < needed) {\n");
    fprintf(ctx->out, "        new_cap *= 2;\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    items = realloc(list->items, sizeof(%s) * new_cap);\n", item_c_type);
    fprintf(ctx->out, "    if (items == NULL) {\n");
    fprintf(ctx->out, "        perror(\"realloc\");\n");
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    list->items = items;\n");
    fprintf(ctx->out, "    list->cap = new_cap;\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static void %s_append(%s *list, %s value)\n{\n", prefix, struct_name, item_c_type);
    fprintf(ctx->out, "    %s_ensure_capacity(list, list->len + 1);\n", prefix);
    fprintf(ctx->out, "    list->items[list->len++] = value;\n");
    if (semantic_type_needs_management(element_type)) {
        emit_runtime_value_retain(ctx, element_type, "list->items[list->len - 1]");
    }
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static %s *%s_from_values(size_t count, %s *values)\n{\n",
        struct_name, prefix, item_c_type);
    fprintf(ctx->out, "    %s *list = %s_new();\n", struct_name, prefix);
    fprintf(ctx->out, "    %s_ensure_capacity(list, count);\n", prefix);
    fprintf(ctx->out, "    for (size_t i = 0; i < count; i++) {\n");
    fprintf(ctx->out, "        list->items[i] = values[i];\n");
    if (semantic_type_needs_management(element_type)) {
        emit_runtime_value_retain(ctx, element_type, "list->items[i]");
    }
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    list->len = count;\n");
    fprintf(ctx->out, "    return list;\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static %s %s_get(%s *list, int index)\n{\n", item_c_type, prefix, struct_name);
    fprintf(ctx->out, "    %s_bounds_check(list, index);\n", prefix);
    if (semantic_type_needs_management(element_type)) {
        fprintf(ctx->out, "    %s value = list->items[index];\n", item_c_type);
        if (semantic_type_is_ref(element_type)) {
            fprintf(ctx->out, "    %s_incref(value);\n", codegen_ref_runtime_prefix(element_type));
        } else if (semantic_type_is_class(element_type)) {
            char helper_name[MAX_NAME_LEN];

            codegen_build_class_retain_name(helper_name, sizeof(helper_name), element_type);
            fprintf(ctx->out, "    %s(&value);\n", helper_name);
        } else if (semantic_type_is_optional(element_type)) {
            char helper_name[MAX_NAME_LEN];

            codegen_build_optional_retain_name(helper_name, sizeof(helper_name), element_type);
            fprintf(ctx->out, "    %s(&value);\n", helper_name);
        } else {
            char helper_name[MAX_NAME_LEN];

            codegen_build_tuple_retain_name(helper_name, sizeof(helper_name), element_type);
            fprintf(ctx->out, "    %s(&value);\n", helper_name);
        }
        fprintf(ctx->out, "    return value;\n");
        fprintf(ctx->out, "}\n\n");
    } else {
    fprintf(ctx->out, "    return list->items[index];\n");
    fprintf(ctx->out, "}\n\n");
    }

    fprintf(ctx->out, "static void %s_set(%s *list, int index, %s value)\n{\n",
        prefix, struct_name, item_c_type);
    fprintf(ctx->out, "    %s_bounds_check(list, index);\n", prefix);
    if (semantic_type_needs_management(element_type)) {
        emit_runtime_value_release(ctx, element_type, "list->items[index]");
    }
    fprintf(ctx->out, "    list->items[index] = value;\n");
    if (semantic_type_needs_management(element_type)) {
        emit_runtime_value_retain(ctx, element_type, "list->items[index]");
    }
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static %s %s_pop(%s *list)\n{\n", item_c_type, prefix, struct_name);
    fprintf(ctx->out, "    if (list == NULL) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: %s is null\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    if (list->len == 0) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: pop from empty %s\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    list->len--;\n");
    fprintf(ctx->out, "    return list->items[list->len];\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static void %s_clear(%s *list)\n{\n", prefix, struct_name);
    fprintf(ctx->out, "    if (list == NULL) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: %s is null\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    if (semantic_type_needs_management(element_type)) {
        fprintf(ctx->out, "    for (size_t i = 0; i < list->len; i++) {\n");
        emit_runtime_value_release(ctx, element_type, "list->items[i]");
        fprintf(ctx->out, "    }\n");
    }
    fprintf(ctx->out, "    list->len = 0;\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static %s *%s_copy(%s *list)\n{\n", struct_name, prefix, struct_name);
    fprintf(ctx->out, "    if (list == NULL) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: %s is null\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    return %s_from_values(list->len, list->items);\n", prefix);
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static int %s_len(%s *list)\n{\n", prefix, struct_name);
    fprintf(ctx->out, "    if (list == NULL) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: %s is null\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    return (int)list->len;\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static void py4_print_%s(%s *value)\n{\n",
        codegen_type_suffix(list_type),
        struct_name);
    fprintf(ctx->out, "    printf(\"[\");\n");
    fprintf(ctx->out, "    if (value != NULL) {\n");
    fprintf(ctx->out, "        for (size_t i = 0; i < value->len; i++) {\n");
    fprintf(ctx->out, "            if (i > 0) {\n");
    fprintf(ctx->out, "                printf(\", \");\n");
    fprintf(ctx->out, "            }\n");
    emit_runtime_value_print(ctx, element_type, "value->items[i]");
    fprintf(ctx->out, "        }\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    printf(\"]\");\n");
    fprintf(ctx->out, "}\n\n");
}

static void emit_dict_key_compare(
    CodegenContext *ctx,
    ValueType key_type,
    const char *lhs,
    const char *rhs)
{
    if (key_type == TYPE_STR) {
        fprintf(ctx->out, "strcmp(%s, %s) == 0", lhs, rhs);
        return;
    }
    fprintf(ctx->out, "%s == %s", lhs, rhs);
}

static void emit_dict_runtime(CodegenContext *ctx, ValueType dict_type)
{
    ValueType key_type = semantic_dict_key_type(dict_type);
    ValueType value_type = semantic_dict_value_type(dict_type);
    ValueType item_types[2] = {key_type, value_type};
    ValueType keys_list_type = semantic_make_list_type(key_type);
    ValueType tuple_type = semantic_make_tuple_type(item_types, 2);
    ValueType values_list_type = semantic_make_list_type(value_type);
    ValueType items_list_type = semantic_make_list_type(tuple_type);
    char struct_name[MAX_NAME_LEN];
    char prefix[MAX_NAME_LEN];
    char type_name[MAX_NAME_LEN];
    char keys_struct_name[MAX_NAME_LEN];
    char values_struct_name[MAX_NAME_LEN];
    char items_struct_name[MAX_NAME_LEN];
    char keys_prefix[MAX_NAME_LEN];
    char values_prefix[MAX_NAME_LEN];
    char items_prefix[MAX_NAME_LEN];
    char tuple_name[MAX_NAME_LEN];
    char *key_c_type = codegen_type_to_c_string(key_type);
    char *value_c_type = codegen_type_to_c_string(value_type);

    snprintf(struct_name, sizeof(struct_name), "%s", codegen_dict_struct_name(dict_type));
    snprintf(prefix, sizeof(prefix), "%s", codegen_dict_runtime_prefix(dict_type));
    snprintf(type_name, sizeof(type_name), "%s", semantic_type_name(dict_type));

    snprintf(keys_struct_name, sizeof(keys_struct_name), "%s", codegen_list_struct_name(keys_list_type));
    snprintf(values_struct_name, sizeof(values_struct_name), "%s", codegen_list_struct_name(values_list_type));
    snprintf(items_struct_name, sizeof(items_struct_name), "%s", codegen_list_struct_name(items_list_type));
    snprintf(keys_prefix, sizeof(keys_prefix), "%s", codegen_list_runtime_prefix(keys_list_type));
    snprintf(values_prefix, sizeof(values_prefix), "%s", codegen_list_runtime_prefix(values_list_type));
    snprintf(items_prefix, sizeof(items_prefix), "%s", codegen_list_runtime_prefix(items_list_type));
    codegen_build_tuple_base_name(tuple_name, sizeof(tuple_name), tuple_type);

    fprintf(ctx->out, "struct %s {\n", struct_name);
    fprintf(ctx->out, "    int refcount;\n");
    fprintf(ctx->out, "    size_t len;\n");
    fprintf(ctx->out, "    size_t cap;\n");
    fprintf(ctx->out, "    %s *keys;\n", key_c_type);
    fprintf(ctx->out, "    %s *values;\n", value_c_type);
    fprintf(ctx->out, "};\n\n");

    fprintf(ctx->out, "static int %s_find_index(%s *dict, %s key)\n{\n", prefix, struct_name, key_c_type);
    fprintf(ctx->out, "    if (dict == NULL) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: %s is null\\n\");\n", type_name);
        fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    for (size_t i = 0; i < dict->len; i++) {\n");
    fprintf(ctx->out, "        if (");
    emit_dict_key_compare(ctx, key_type, "dict->keys[i]", "key");
    fprintf(ctx->out, ") {\n");
    fprintf(ctx->out, "            return (int)i;\n");
    fprintf(ctx->out, "        }\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    return -1;\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static %s *%s_new(void)\n{\n", struct_name, prefix);
    fprintf(ctx->out, "    %s *dict = calloc(1, sizeof(%s));\n", struct_name, struct_name);
    fprintf(ctx->out, "    if (dict == NULL) {\n");
    fprintf(ctx->out, "        perror(\"calloc\");\n");
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    dict->refcount = 1;\n");
    fprintf(ctx->out, "    return dict;\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static void %s_incref(%s *dict)\n{\n", prefix, struct_name);
    fprintf(ctx->out, "    if (dict != NULL) {\n");
    fprintf(ctx->out, "        dict->refcount++;\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static void %s_decref(%s *dict)\n{\n", prefix, struct_name);
    fprintf(ctx->out, "    if (dict == NULL) {\n");
    fprintf(ctx->out, "        return;\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    dict->refcount--;\n");
    fprintf(ctx->out, "    if (dict->refcount == 0) {\n");
    if (key_type == TYPE_STR) {
        fprintf(ctx->out, "        for (size_t i = 0; i < dict->len; i++) {\n");
        fprintf(ctx->out, "            free((void *)dict->keys[i]);\n");
        fprintf(ctx->out, "        }\n");
    }
    if (semantic_type_needs_management(value_type)) {
        fprintf(ctx->out, "        for (size_t i = 0; i < dict->len; i++) {\n");
        emit_runtime_value_release(ctx, value_type, "dict->values[i]");
        fprintf(ctx->out, "        }\n");
    }
    fprintf(ctx->out, "        free(dict->keys);\n");
    fprintf(ctx->out, "        free(dict->values);\n");
    fprintf(ctx->out, "        free(dict);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static void %s_ensure_capacity(%s *dict, size_t needed)\n{\n", prefix, struct_name);
    fprintf(ctx->out, "    size_t new_cap;\n");
    fprintf(ctx->out, "    %s *new_keys;\n", key_c_type);
    fprintf(ctx->out, "    %s *new_values;\n\n", value_c_type);
    fprintf(ctx->out, "    if (dict == NULL) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: %s is null\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    if (dict->cap >= needed) {\n");
    fprintf(ctx->out, "        return;\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    new_cap = dict->cap == 0 ? 4 : dict->cap * 2;\n");
    fprintf(ctx->out, "    while (new_cap < needed) {\n");
    fprintf(ctx->out, "        new_cap *= 2;\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    new_keys = realloc(dict->keys, sizeof(%s) * new_cap);\n", key_c_type);
    fprintf(ctx->out, "    new_values = realloc(dict->values, sizeof(%s) * new_cap);\n", value_c_type);
    fprintf(ctx->out, "    if (new_keys == NULL || new_values == NULL) {\n");
    fprintf(ctx->out, "        perror(\"realloc\");\n");
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    dict->keys = new_keys;\n");
    fprintf(ctx->out, "    dict->values = new_values;\n");
    fprintf(ctx->out, "    dict->cap = new_cap;\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static void %s_set(%s *dict, %s key, %s value)\n{\n", prefix, struct_name, key_c_type, value_c_type);
    fprintf(ctx->out, "    int index;\n");
    fprintf(ctx->out, "    if (dict == NULL) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: %s is null\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    index = %s_find_index(dict, key);\n", prefix);
    fprintf(ctx->out, "    if (index >= 0) {\n");
    if (semantic_type_needs_management(value_type)) {
        emit_runtime_value_release(ctx, value_type, "dict->values[index]");
    }
    fprintf(ctx->out, "        dict->values[index] = value;\n");
    if (semantic_type_needs_management(value_type)) {
        emit_runtime_value_retain(ctx, value_type, "dict->values[index]");
    }
    fprintf(ctx->out, "        return;\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    %s_ensure_capacity(dict, dict->len + 1);\n", prefix);
    if (key_type == TYPE_STR) {
        fprintf(ctx->out, "    {\n");
        fprintf(ctx->out, "        size_t key_len = strlen(key) + 1;\n");
        fprintf(ctx->out, "        char *key_copy = malloc(key_len);\n");
        fprintf(ctx->out, "        if (key_copy == NULL) {\n");
        fprintf(ctx->out, "            perror(\"malloc\");\n");
        fprintf(ctx->out, "            exit(1);\n");
        fprintf(ctx->out, "        }\n");
        fprintf(ctx->out, "        memcpy(key_copy, key, key_len);\n");
        fprintf(ctx->out, "        dict->keys[dict->len] = key_copy;\n");
        fprintf(ctx->out, "    }\n");
    } else {
        fprintf(ctx->out, "    dict->keys[dict->len] = key;\n");
    }
    fprintf(ctx->out, "    dict->values[dict->len] = value;\n");
    if (semantic_type_needs_management(value_type)) {
        emit_runtime_value_retain(ctx, value_type, "dict->values[dict->len]");
    }
    fprintf(ctx->out, "    dict->len++;\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static %s *%s_from_pairs(size_t count, const %s *keys, const %s *values)\n{\n",
        struct_name, prefix, key_c_type, value_c_type);
    fprintf(ctx->out, "    %s *dict = %s_new();\n", struct_name, prefix);
    fprintf(ctx->out, "    for (size_t i = 0; i < count; i++) {\n");
    fprintf(ctx->out, "        %s_set(dict, keys[i], values[i]);\n", prefix);
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    return dict;\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static %s %s_get(%s *dict, %s key)\n{\n", value_c_type, prefix, struct_name, key_c_type);
    fprintf(ctx->out, "    int index = %s_find_index(dict, key);\n", prefix);
    fprintf(ctx->out, "    if (index < 0) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: key not found in %s\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    if (semantic_type_needs_management(value_type)) {
        fprintf(ctx->out, "    %s value = dict->values[index];\n", value_c_type);
        emit_runtime_value_retain(ctx, value_type, "value");
        fprintf(ctx->out, "    return value;\n");
    } else {
    fprintf(ctx->out, "    return dict->values[index];\n");
    }
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static %s %s_get_or(%s *dict, %s key, %s fallback)\n{\n",
        value_c_type, prefix, struct_name, key_c_type, value_c_type);
    fprintf(ctx->out, "    int index = %s_find_index(dict, key);\n", prefix);
    fprintf(ctx->out, "    if (index < 0) {\n");
    if (semantic_type_needs_management(value_type)) {
        fprintf(ctx->out, "        %s value = fallback;\n", value_c_type);
        emit_runtime_value_retain(ctx, value_type, "value");
        fprintf(ctx->out, "        return value;\n");
    } else {
        fprintf(ctx->out, "        return fallback;\n");
    }
    fprintf(ctx->out, "    }\n");
    if (semantic_type_needs_management(value_type)) {
        fprintf(ctx->out, "    %s value = dict->values[index];\n", value_c_type);
        emit_runtime_value_retain(ctx, value_type, "value");
        fprintf(ctx->out, "    return value;\n");
    } else {
        fprintf(ctx->out, "    return dict->values[index];\n");
    }
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static %s %s_key_at(%s *dict, int index)\n{\n", key_c_type, prefix, struct_name);
    fprintf(ctx->out, "    if (dict == NULL) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: %s is null\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    if (index < 0 || (size_t)index >= dict->len) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: %s key index out of bounds\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    return dict->keys[index];\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static bool %s_contains(%s *dict, %s key)\n{\n", prefix, struct_name, key_c_type);
    fprintf(ctx->out, "    return %s_find_index(dict, key) >= 0;\n", prefix);
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static %s *%s_keys(%s *dict)\n{\n", keys_struct_name, prefix, struct_name);
    fprintf(ctx->out, "    %s *keys;\n", keys_struct_name);
    fprintf(ctx->out, "    if (dict == NULL) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: %s is null\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    keys = %s_new();\n", keys_prefix);
    fprintf(ctx->out, "    for (size_t i = 0; i < dict->len; i++) {\n");
    fprintf(ctx->out, "        %s_append(keys, dict->keys[i]);\n", keys_prefix);
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    return keys;\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static %s *%s_values(%s *dict)\n{\n", values_struct_name, prefix, struct_name);
    fprintf(ctx->out, "    %s *values;\n", values_struct_name);
    fprintf(ctx->out, "    if (dict == NULL) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: %s is null\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    values = %s_new();\n", values_prefix);
    fprintf(ctx->out, "    for (size_t i = 0; i < dict->len; i++) {\n");
    fprintf(ctx->out, "        %s_append(values, dict->values[i]);\n", values_prefix);
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    return values;\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static %s *%s_items(%s *dict)\n{\n", items_struct_name, prefix, struct_name);
    fprintf(ctx->out, "    %s *items;\n", items_struct_name);
    fprintf(ctx->out, "    if (dict == NULL) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: %s is null\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    items = %s_new();\n", items_prefix);
    fprintf(ctx->out, "    for (size_t i = 0; i < dict->len; i++) {\n");
    fprintf(ctx->out, "        %s entry = (%s){dict->keys[i], dict->values[i]};\n", tuple_name, tuple_name);
    fprintf(ctx->out, "        %s_append(items, entry);\n", items_prefix);
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    return items;\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static %s %s_pop(%s *dict, %s key)\n{\n", value_c_type, prefix, struct_name, key_c_type);
    fprintf(ctx->out, "    int index;\n");
    fprintf(ctx->out, "    %s value;\n", value_c_type);
    fprintf(ctx->out, "    if (dict == NULL) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: %s is null\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    index = %s_find_index(dict, key);\n", prefix);
    fprintf(ctx->out, "    if (index < 0) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: key not found in %s\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    value = dict->values[index];\n");
    if (key_type == TYPE_STR) {
        fprintf(ctx->out, "    free((void *)dict->keys[index]);\n");
    }
    fprintf(ctx->out, "    for (size_t i = (size_t)index + 1; i < dict->len; i++) {\n");
    fprintf(ctx->out, "        dict->keys[i - 1] = dict->keys[i];\n");
    fprintf(ctx->out, "        dict->values[i - 1] = dict->values[i];\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    dict->len--;\n");
    fprintf(ctx->out, "    return value;\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static void %s_clear(%s *dict)\n{\n", prefix, struct_name);
    fprintf(ctx->out, "    if (dict == NULL) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: %s is null\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    if (key_type == TYPE_STR) {
        fprintf(ctx->out, "    for (size_t i = 0; i < dict->len; i++) {\n");
        fprintf(ctx->out, "        free((void *)dict->keys[i]);\n");
        fprintf(ctx->out, "    }\n");
    }
    if (semantic_type_needs_management(value_type)) {
        fprintf(ctx->out, "    for (size_t i = 0; i < dict->len; i++) {\n");
        emit_runtime_value_release(ctx, value_type, "dict->values[i]");
        fprintf(ctx->out, "    }\n");
    }
    fprintf(ctx->out, "    dict->len = 0;\n");
    fprintf(ctx->out, "}\n\n");
    fprintf(ctx->out, "static %s *%s_copy(%s *dict)\n{\n", struct_name, prefix, struct_name);
    fprintf(ctx->out, "    %s *copy;\n", struct_name);
    fprintf(ctx->out, "    if (dict == NULL) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: %s is null\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    copy = %s_new();\n", prefix);
    fprintf(ctx->out, "    %s_ensure_capacity(copy, dict->len);\n", prefix);
    fprintf(ctx->out, "    for (size_t i = 0; i < dict->len; i++) {\n");
    if (key_type == TYPE_STR) {
        fprintf(ctx->out, "        size_t key_len = strlen(dict->keys[i]) + 1;\n");
        fprintf(ctx->out, "        char *key_copy = malloc(key_len);\n");
        fprintf(ctx->out, "        if (key_copy == NULL) {\n");
        fprintf(ctx->out, "            perror(\"malloc\");\n");
        fprintf(ctx->out, "            exit(1);\n");
        fprintf(ctx->out, "        }\n");
        fprintf(ctx->out, "        memcpy(key_copy, dict->keys[i], key_len);\n");
        fprintf(ctx->out, "        copy->keys[i] = key_copy;\n");
    } else {
        fprintf(ctx->out, "        copy->keys[i] = dict->keys[i];\n");
    }
    fprintf(ctx->out, "        copy->values[i] = dict->values[i];\n");
    if (semantic_type_needs_management(value_type)) {
        emit_runtime_value_retain(ctx, value_type, "copy->values[i]");
    }
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    copy->len = dict->len;\n");
    fprintf(ctx->out, "    return copy;\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static int %s_len(%s *dict)\n{\n", prefix, struct_name);
    fprintf(ctx->out, "    if (dict == NULL) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: %s is null\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    return (int)dict->len;\n");
    fprintf(ctx->out, "}\n\n");

    free(key_c_type);
    free(value_c_type);
}

void codegen_emit_container_runtime(CodegenContext *ctx)
{
    for (size_t i = 0; i < semantic_list_type_count(); i++) {
        emit_list_runtime(ctx, semantic_list_type_at(i));
    }
    for (size_t i = 0; i < semantic_dict_type_count(); i++) {
        emit_dict_runtime(ctx, semantic_dict_type_at(i));
    }
}
