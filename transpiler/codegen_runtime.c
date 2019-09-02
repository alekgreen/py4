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
    const char *struct_name = codegen_list_struct_name(list_type);
    const char *prefix = codegen_list_runtime_prefix(list_type);
    const char *type_name = semantic_type_name(list_type);
    const char *item_c_type = codegen_list_element_c_type(list_type);
    ValueType element_type = semantic_list_element_type(list_type);

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

    fprintf(ctx->out, "static %s *%s_from_values(size_t count, const %s *values)\n{\n",
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

static void emit_dict_runtime(
    CodegenContext *ctx,
    const char *struct_name,
    const char *prefix,
    const char *type_name)
{
    fprintf(ctx->out, "struct %s {\n", struct_name);
    fprintf(ctx->out, "    int refcount;\n");
    fprintf(ctx->out, "    size_t len;\n");
    fprintf(ctx->out, "    size_t cap;\n");
    fprintf(ctx->out, "    const char **keys;\n");
    fprintf(ctx->out, "    const char **values;\n");
    fprintf(ctx->out, "};\n\n");

    fprintf(ctx->out, "static int %s_find_index(%s *dict, const char *key)\n{\n", prefix, struct_name);
    fprintf(ctx->out, "    if (dict == NULL) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: %s is null\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    for (size_t i = 0; i < dict->len; i++) {\n");
    fprintf(ctx->out, "        if (strcmp(dict->keys[i], key) == 0) {\n");
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
    fprintf(ctx->out, "        free(dict->keys);\n");
    fprintf(ctx->out, "        free(dict->values);\n");
    fprintf(ctx->out, "        free(dict);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static void %s_ensure_capacity(%s *dict, size_t needed)\n{\n", prefix, struct_name);
    fprintf(ctx->out, "    size_t new_cap;\n");
    fprintf(ctx->out, "    const char **new_keys;\n");
    fprintf(ctx->out, "    const char **new_values;\n\n");
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
    fprintf(ctx->out, "    new_keys = realloc(dict->keys, sizeof(const char *) * new_cap);\n");
    fprintf(ctx->out, "    new_values = realloc(dict->values, sizeof(const char *) * new_cap);\n");
    fprintf(ctx->out, "    if (new_keys == NULL || new_values == NULL) {\n");
    fprintf(ctx->out, "        perror(\"realloc\");\n");
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    dict->keys = new_keys;\n");
    fprintf(ctx->out, "    dict->values = new_values;\n");
    fprintf(ctx->out, "    dict->cap = new_cap;\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static void %s_set(%s *dict, const char *key, const char *value)\n{\n", prefix, struct_name);
    fprintf(ctx->out, "    int index;\n");
    fprintf(ctx->out, "    if (dict == NULL) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: %s is null\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    index = %s_find_index(dict, key);\n", prefix);
    fprintf(ctx->out, "    if (index >= 0) {\n");
    fprintf(ctx->out, "        dict->values[index] = value;\n");
    fprintf(ctx->out, "        return;\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    %s_ensure_capacity(dict, dict->len + 1);\n", prefix);
    fprintf(ctx->out, "    dict->keys[dict->len] = key;\n");
    fprintf(ctx->out, "    dict->values[dict->len] = value;\n");
    fprintf(ctx->out, "    dict->len++;\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static %s *%s_from_pairs(size_t count, const char **keys, const char **values)\n{\n",
        struct_name, prefix);
    fprintf(ctx->out, "    %s *dict = %s_new();\n", struct_name, prefix);
    fprintf(ctx->out, "    for (size_t i = 0; i < count; i++) {\n");
    fprintf(ctx->out, "        %s_set(dict, keys[i], values[i]);\n", prefix);
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    return dict;\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static const char *%s_get(%s *dict, const char *key)\n{\n", prefix, struct_name);
    fprintf(ctx->out, "    int index = %s_find_index(dict, key);\n", prefix);
    fprintf(ctx->out, "    if (index < 0) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: key not found in %s\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    return dict->values[index];\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static const char *%s_key_at(%s *dict, int index)\n{\n", prefix, struct_name);
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

    fprintf(ctx->out, "static bool %s_contains(%s *dict, const char *key)\n{\n", prefix, struct_name);
    fprintf(ctx->out, "    return %s_find_index(dict, key) >= 0;\n", prefix);
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static Py4List_str *%s_keys(%s *dict)\n{\n", prefix, struct_name);
    fprintf(ctx->out, "    Py4List_str *keys;\n");
    fprintf(ctx->out, "    if (dict == NULL) {\n");
    fprintf(ctx->out, "        fprintf(stderr, \"Runtime error: %s is null\\n\");\n", type_name);
    fprintf(ctx->out, "        exit(1);\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    keys = py4_list_str_new();\n");
    fprintf(ctx->out, "    py4_list_str_ensure_capacity(keys, dict->len);\n");
    fprintf(ctx->out, "    for (size_t i = 0; i < dict->len; i++) {\n");
    fprintf(ctx->out, "        keys->items[i] = dict->keys[i];\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    keys->len = dict->len;\n");
    fprintf(ctx->out, "    return keys;\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static const char *%s_pop(%s *dict, const char *key)\n{\n", prefix, struct_name);
    fprintf(ctx->out, "    int index;\n");
    fprintf(ctx->out, "    const char *value;\n");
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
    fprintf(ctx->out, "        copy->keys[i] = dict->keys[i];\n");
    fprintf(ctx->out, "        copy->values[i] = dict->values[i];\n");
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
}

void codegen_emit_container_runtime(CodegenContext *ctx)
{
    for (size_t i = 0; i < semantic_list_type_count(); i++) {
        emit_list_runtime(ctx, semantic_list_type_at(i));
    }
    emit_dict_runtime(ctx, "Py4DictStrStr", "py4_dict_str_str", "dict[str, str]");
}
