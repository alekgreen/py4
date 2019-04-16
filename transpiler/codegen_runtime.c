#include <stdio.h>
#include <stdlib.h>

#include "codegen_internal.h"

static void emit_list_runtime(
    CodegenContext *ctx,
    const char *struct_name,
    const char *prefix,
    const char *type_name,
    const char *item_c_type)
{
    fprintf(ctx->out, "typedef struct {\n");
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
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static %s *%s_from_values(size_t count, const %s *values)\n{\n",
        struct_name, prefix, item_c_type);
    fprintf(ctx->out, "    %s *list = %s_new();\n", struct_name, prefix);
    fprintf(ctx->out, "    %s_ensure_capacity(list, count);\n", prefix);
    fprintf(ctx->out, "    for (size_t i = 0; i < count; i++) {\n");
    fprintf(ctx->out, "        list->items[i] = values[i];\n");
    fprintf(ctx->out, "    }\n");
    fprintf(ctx->out, "    list->len = count;\n");
    fprintf(ctx->out, "    return list;\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static %s %s_get(%s *list, int index)\n{\n", item_c_type, prefix, struct_name);
    fprintf(ctx->out, "    %s_bounds_check(list, index);\n", prefix);
    fprintf(ctx->out, "    return list->items[index];\n");
    fprintf(ctx->out, "}\n\n");

    fprintf(ctx->out, "static void %s_set(%s *list, int index, %s value)\n{\n",
        prefix, struct_name, item_c_type);
    fprintf(ctx->out, "    %s_bounds_check(list, index);\n", prefix);
    fprintf(ctx->out, "    list->items[index] = value;\n");
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
}

void codegen_emit_container_runtime(CodegenContext *ctx)
{
    emit_list_runtime(ctx, "Py4ListInt", "py4_list_int", "list[int]", "int");
    emit_list_runtime(ctx, "Py4ListFloat", "py4_list_float", "list[float]", "double");
    emit_list_runtime(ctx, "Py4ListBool", "py4_list_bool", "list[bool]", "bool");
}
