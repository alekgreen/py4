#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen_internal.h"
#include "semantic_internal.h"

void emit_native_json_type_runtime(CodegenContext *ctx)
{
    fputs("struct cJSON {\n", ctx->out);
    fputs("    struct cJSON *next;\n", ctx->out);
    fputs("    struct cJSON *prev;\n", ctx->out);
    fputs("    struct cJSON *child;\n", ctx->out);
    fputs("    int type;\n", ctx->out);
    fputs("    char *valuestring;\n", ctx->out);
    fputs("    int valueint;\n", ctx->out);
    fputs("    double valuedouble;\n", ctx->out);
    fputs("    char *string;\n", ctx->out);
    fputs("};\n\n", ctx->out);

    fputs("extern cJSON *cJSON_Parse(const char *value);\n", ctx->out);
    fputs("extern void cJSON_Delete(cJSON *item);\n", ctx->out);
    fputs("extern char *cJSON_PrintUnformatted(const cJSON *item);\n", ctx->out);
    fputs("extern cJSON *cJSON_CreateObject(void);\n", ctx->out);
    fputs("extern cJSON *cJSON_CreateArray(void);\n", ctx->out);
    fputs("extern cJSON *cJSON_CreateString(const char *string);\n", ctx->out);
    fputs("extern cJSON *cJSON_CreateNumber(double num);\n", ctx->out);
    fputs("extern cJSON *cJSON_CreateBool(int boolean);\n", ctx->out);
    fputs("extern cJSON *cJSON_CreateNull(void);\n", ctx->out);
    fputs("extern int cJSON_AddItemToObject(cJSON *object, const char *string, cJSON *item);\n", ctx->out);
    fputs("extern int cJSON_AddItemToArray(cJSON *array, cJSON *item);\n", ctx->out);
    fputs("extern cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON *object, const char *string);\n", ctx->out);
    fputs("extern int cJSON_GetArraySize(const cJSON *array);\n", ctx->out);
    fputs("extern cJSON *cJSON_GetArrayItem(const cJSON *array, int index);\n", ctx->out);
    fputs("extern const char *cJSON_GetErrorPtr(void);\n", ctx->out);
    fputs("extern int cJSON_IsInvalid(const cJSON *item);\n", ctx->out);
    fputs("extern int cJSON_IsBool(const cJSON *item);\n", ctx->out);
    fputs("extern int cJSON_IsTrue(const cJSON *item);\n", ctx->out);
    fputs("extern int cJSON_IsNumber(const cJSON *item);\n", ctx->out);
    fputs("extern int cJSON_IsString(const cJSON *item);\n", ctx->out);
    fputs("extern int cJSON_IsArray(const cJSON *item);\n", ctx->out);
    fputs("extern int cJSON_IsObject(const cJSON *item);\n", ctx->out);
    fputs("extern int cJSON_IsNull(const cJSON *item);\n\n", ctx->out);

    fputs("typedef struct Py4JsonOwner {\n", ctx->out);
    fputs("    int refcount;\n", ctx->out);
    fputs("    cJSON *root;\n", ctx->out);
    fputs("} Py4JsonOwner;\n\n", ctx->out);

    fputs("typedef struct Py4JsonValue {\n", ctx->out);
    fputs("    int refcount;\n", ctx->out);
    fputs("    Py4JsonOwner *owner;\n", ctx->out);
    fputs("    cJSON *node;\n", ctx->out);
    fputs("} Py4JsonValue;\n\n", ctx->out);

    fputs("static char *py4_json_strdup(const char *value)\n{\n", ctx->out);
    fputs("    size_t len;\n", ctx->out);
    fputs("    char *copy;\n", ctx->out);
    fputs("    if (value == NULL) {\n", ctx->out);
    fputs("        value = \"\";\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    len = strlen(value);\n", ctx->out);
    fputs("    copy = malloc(len + 1);\n", ctx->out);
    fputs("    if (copy == NULL) {\n", ctx->out);
    fputs("        perror(\"malloc\");\n", ctx->out);
    fputs("        exit(1);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    memcpy(copy, value, len + 1);\n", ctx->out);
    fputs("    return copy;\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static void py4_json_fail(const char *message)\n{\n", ctx->out);
    fputs("    fprintf(stderr, \"Runtime error: %s\\n\", message);\n", ctx->out);
    fputs("    exit(1);\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static Py4JsonOwner *py4_json_owner_new(cJSON *root)\n{\n", ctx->out);
    fputs("    Py4JsonOwner *owner = malloc(sizeof(Py4JsonOwner));\n", ctx->out);
    fputs("    if (owner == NULL) {\n", ctx->out);
    fputs("        perror(\"malloc\");\n", ctx->out);
    fputs("        if (root != NULL) {\n", ctx->out);
    fputs("            cJSON_Delete(root);\n", ctx->out);
    fputs("        }\n", ctx->out);
    fputs("        exit(1);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    owner->refcount = 1;\n", ctx->out);
    fputs("    owner->root = root;\n", ctx->out);
    fputs("    return owner;\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static void py4_json_owner_incref(Py4JsonOwner *owner)\n{\n", ctx->out);
    fputs("    if (owner != NULL) {\n", ctx->out);
    fputs("        owner->refcount++;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static void py4_json_owner_decref(Py4JsonOwner *owner)\n{\n", ctx->out);
    fputs("    if (owner == NULL) {\n", ctx->out);
    fputs("        return;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    owner->refcount--;\n", ctx->out);
    fputs("    if (owner->refcount <= 0) {\n", ctx->out);
    fputs("        if (owner->root != NULL) {\n", ctx->out);
    fputs("            cJSON_Delete(owner->root);\n", ctx->out);
    fputs("        }\n", ctx->out);
    fputs("        free(owner);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static Py4JsonValue *py4_json_value_new(Py4JsonOwner *owner, cJSON *node)\n{\n", ctx->out);
    fputs("    Py4JsonValue *value = malloc(sizeof(Py4JsonValue));\n", ctx->out);
    fputs("    if (value == NULL) {\n", ctx->out);
    fputs("        perror(\"malloc\");\n", ctx->out);
    fputs("        py4_json_owner_decref(owner);\n", ctx->out);
    fputs("        exit(1);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    value->refcount = 1;\n", ctx->out);
    fputs("    value->owner = owner;\n", ctx->out);
    fputs("    value->node = node;\n", ctx->out);
    fputs("    py4_json_owner_incref(owner);\n", ctx->out);
    fputs("    return value;\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static void py4_json_value_incref(Py4JsonValue *value)\n{\n", ctx->out);
    fputs("    if (value != NULL) {\n", ctx->out);
    fputs("        value->refcount++;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static void py4_json_value_decref(Py4JsonValue *value)\n{\n", ctx->out);
    fputs("    if (value == NULL) {\n", ctx->out);
    fputs("        return;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    value->refcount--;\n", ctx->out);
    fputs("    if (value->refcount <= 0) {\n", ctx->out);
    fputs("        py4_json_owner_decref(value->owner);\n", ctx->out);
    fputs("        free(value);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static char *py4_json_stringify_handle(Py4JsonValue *value)\n{\n", ctx->out);
    fputs("    char *text;\n", ctx->out);
    fputs("    if (value == NULL || value->node == NULL) {\n", ctx->out);
    fputs("        py4_json_fail(\"cannot stringify null json value handle\");\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    text = cJSON_PrintUnformatted(value->node);\n", ctx->out);
    fputs("    if (text == NULL) {\n", ctx->out);
    fputs("        py4_json_fail(\"failed to stringify json value\");\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    return text;\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static Py4JsonValue *py4_json_parse_text(const char *text)\n{\n", ctx->out);
    fputs("    cJSON *root = cJSON_Parse(text);\n", ctx->out);
    fputs("    Py4JsonOwner *owner;\n", ctx->out);
    fputs("    Py4JsonValue *value;\n", ctx->out);
    fputs("    if (root == NULL) {\n", ctx->out);
    fputs("        const char *error_ptr = cJSON_GetErrorPtr();\n", ctx->out);
    fputs("        fprintf(stderr, \"Runtime error: failed to parse json\");\n", ctx->out);
    fputs("        if (error_ptr != NULL) {\n", ctx->out);
    fputs("            fprintf(stderr, \" near: %s\", error_ptr);\n", ctx->out);
    fputs("        }\n", ctx->out);
    fputs("        fprintf(stderr, \"\\n\");\n", ctx->out);
    fputs("        exit(1);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    owner = py4_json_owner_new(root);\n", ctx->out);
    fputs("    value = py4_json_value_new(owner, root);\n", ctx->out);
    fputs("    py4_json_owner_decref(owner);\n", ctx->out);
    fputs("    return value;\n", ctx->out);
    fputs("}\n\n", ctx->out);
}

int emit_native_json_function_definition(
    CodegenContext *ctx,
    const char *module_name,
    const ParseNode *name,
    const ParseNode *parameters,
    const char *c_name,
    ValueType return_type,
    ValueType first_param_type)
{
    if (module_name != NULL &&
        strcmp(module_name, "json") == 0 &&
        strcmp(name->value, "parse") == 0 &&
        parameters->child_count == 1 &&
        first_param_type == TYPE_STR) {
        const char *text = parameters->children[0]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return py4_json_parse_text(%s);\n", text);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "json") == 0 &&
        strcmp(name->value, "stringify") == 0 &&
        parameters->child_count == 1 &&
        semantic_type_is_native(first_param_type)) {
        const char *value_name = parameters->children[0]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return py4_json_stringify_handle(%s);\n", value_name);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "json") == 0 &&
        strcmp(name->value, "to_string") == 0 &&
        parameters->child_count == 1 &&
        semantic_type_is_native(first_param_type)) {
        const char *value_name = parameters->children[0]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return py4_json_stringify_handle(%s);\n", value_name);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "json") == 0 &&
        strcmp(name->value, "len") == 0 &&
        parameters->child_count == 1 &&
        semantic_type_is_native(first_param_type)) {
        const char *value_name = parameters->children[0]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (%s == NULL || %s->node == NULL) {\n", value_name, value_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("py4_json_fail(\"cannot get length of null json value handle\");\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (!cJSON_IsArray(%s->node) && !cJSON_IsObject(%s->node)) {\n", value_name, value_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("py4_json_fail(\"json.len expects an array or object value\");\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("int count = 0;\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "for (cJSON *child = %s->node->child; child != NULL; child = child->next) {\n", value_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("count++;\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("return count;\n", ctx->out);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "json") == 0 &&
        strcmp(name->value, "has") == 0 &&
        parameters->child_count == 2 &&
        semantic_type_is_native(first_param_type)) {
        const char *value_name = parameters->children[0]->value;
        const char *key_name = parameters->children[1]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (%s == NULL || %s->node == NULL) {\n", value_name, value_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("py4_json_fail(\"cannot query a null json value handle\");\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (!cJSON_IsObject(%s->node)) {\n", value_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("py4_json_fail(\"json.has expects an object value\");\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return cJSON_GetObjectItemCaseSensitive(%s->node, %s) != NULL;\n", value_name, key_name);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "json") == 0 &&
        strcmp(name->value, "keys") == 0 &&
        parameters->child_count == 1 &&
        semantic_type_is_native(first_param_type)) {
        const char *value_name = parameters->children[0]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (%s == NULL || %s->node == NULL) {\n", value_name, value_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("py4_json_fail(\"cannot get keys from null json value handle\");\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (!cJSON_IsObject(%s->node)) {\n", value_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("py4_json_fail(\"json.keys expects an object value\");\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("Py4List_str *keys = py4_list_str_new();\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "for (cJSON *child = %s->node->child; child != NULL; child = child->next) {\n", value_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("py4_list_str_append(keys, child->string != NULL ? child->string : \"\");\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("return keys;\n", ctx->out);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "json") == 0 &&
        strcmp(name->value, "values") == 0 &&
        parameters->child_count == 1 &&
        semantic_type_is_native(first_param_type)) {
        const char *value_name = parameters->children[0]->value;
        const char *list_struct = codegen_list_struct_name(return_type);
        const char *list_prefix = codegen_list_runtime_prefix(return_type);

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (%s == NULL || %s->node == NULL) {\n", value_name, value_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("py4_json_fail(\"cannot get values from null json value handle\");\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (!cJSON_IsObject(%s->node)) {\n", value_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("py4_json_fail(\"json.values expects an object value\");\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "%s *values = %s_new();\n", list_struct, list_prefix);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "for (cJSON *child = %s->node->child; child != NULL; child = child->next) {\n", value_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "Py4JsonValue *item = py4_json_value_new(%s->owner, child);\n", value_name);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "%s_append(values, item);\n", list_prefix);
        codegen_emit_indent(ctx);
        fputs("py4_json_value_decref(item);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("return values;\n", ctx->out);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "json") == 0 &&
        strcmp(name->value, "items") == 0 &&
        parameters->child_count == 1 &&
        semantic_type_is_native(first_param_type)) {
        const char *value_name = parameters->children[0]->value;
        ValueType tuple_type = semantic_list_element_type(return_type);
        char tuple_name[MAX_NAME_LEN];
        char tuple_release_name[MAX_NAME_LEN];
        const char *list_struct = codegen_list_struct_name(return_type);
        const char *list_prefix = codegen_list_runtime_prefix(return_type);

        codegen_build_tuple_base_name(tuple_name, sizeof(tuple_name), tuple_type);
        codegen_build_tuple_release_name(tuple_release_name, sizeof(tuple_release_name), tuple_type);
        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (%s == NULL || %s->node == NULL) {\n", value_name, value_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("py4_json_fail(\"cannot get items from null json value handle\");\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (!cJSON_IsObject(%s->node)) {\n", value_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("py4_json_fail(\"json.items expects an object value\");\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "%s *items = %s_new();\n", list_struct, list_prefix);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "for (cJSON *child = %s->node->child; child != NULL; child = child->next) {\n", value_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "%s entry;\n", tuple_name);
        codegen_emit_indent(ctx);
        fputs("entry.item0 = child->string != NULL ? child->string : \"\";\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "entry.item1 = py4_json_value_new(%s->owner, child);\n", value_name);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "%s_append(items, entry);\n", list_prefix);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "%s(&entry);\n", tuple_release_name);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("return items;\n", ctx->out);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "json") == 0 &&
        strcmp(name->value, "get_index") == 0 &&
        parameters->child_count == 2 &&
        semantic_type_is_native(first_param_type)) {
        const char *value_name = parameters->children[0]->value;
        const char *index_name = parameters->children[1]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (%s == NULL || %s->node == NULL) {\n", value_name, value_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("py4_json_fail(\"cannot index a null json value handle\");\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (!cJSON_IsArray(%s->node)) {\n", value_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("py4_json_fail(\"json.get_index expects an array value\");\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (%s < 0) {\n", index_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("py4_json_fail(\"json array index out of bounds\");\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("int current = 0;\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "for (cJSON *child = %s->node->child; child != NULL; child = child->next, current++) {\n", value_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (current == %s) {\n", index_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return py4_json_value_new(%s->owner, child);\n", value_name);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("py4_json_fail(\"json array index out of bounds\");\n", ctx->out);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "json") == 0 &&
        strcmp(name->value, "get_or") == 0 &&
        parameters->child_count == 3 &&
        semantic_type_is_native(first_param_type)) {
        const char *value_name = parameters->children[0]->value;
        const char *key_name = parameters->children[1]->value;
        const char *fallback_name = parameters->children[2]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (%s == NULL || %s->node == NULL) {\n", value_name, value_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("py4_json_fail(\"cannot get from null json value handle\");\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (!cJSON_IsObject(%s->node)) {\n", value_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("py4_json_fail(\"json.get_or expects an object value\");\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "cJSON *child = cJSON_GetObjectItemCaseSensitive(%s->node, %s);\n", value_name, key_name);
        codegen_emit_indent(ctx);
        fputs("if (child == NULL) {\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "py4_json_value_incref(%s);\n", fallback_name);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return %s;\n", fallback_name);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return py4_json_value_new(%s->owner, child);\n", value_name);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "json") == 0 &&
        strcmp(name->value, "get") == 0 &&
        parameters->child_count == 2 &&
        semantic_type_is_native(first_param_type)) {
        const char *value_name = parameters->children[0]->value;
        const char *key_name = parameters->children[1]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (%s == NULL || %s->node == NULL) {\n", value_name, value_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("py4_json_fail(\"cannot get from null json value handle\");\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (!cJSON_IsObject(%s->node)) {\n", value_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("py4_json_fail(\"json.get expects an object value\");\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "cJSON *child = cJSON_GetObjectItemCaseSensitive(%s->node, %s);\n", value_name, key_name);
        codegen_emit_indent(ctx);
        fputs("if (child == NULL) {\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("static cJSON py4_json_missing_null = {0};\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("py4_json_missing_null.type = 4;\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("child = &py4_json_missing_null;\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return py4_json_value_new(%s->owner, child);\n", value_name);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "json") == 0 &&
        parameters->child_count == 1 &&
        semantic_type_is_native(first_param_type)) {
        const char *value_name = parameters->children[0]->value;
        const char *predicate = NULL;
        int handled_bool_conversion = 0;
        int handled_float_conversion = 0;
        int handled_string_conversion = 0;

        if (strcmp(name->value, "is_null") == 0) {
            predicate = "cJSON_IsNull";
        } else if (strcmp(name->value, "is_bool") == 0) {
            predicate = "cJSON_IsBool";
        } else if (strcmp(name->value, "is_number") == 0) {
            predicate = "cJSON_IsNumber";
        } else if (strcmp(name->value, "is_string") == 0) {
            predicate = "cJSON_IsString";
        } else if (strcmp(name->value, "is_array") == 0) {
            predicate = "cJSON_IsArray";
        } else if (strcmp(name->value, "is_object") == 0) {
            predicate = "cJSON_IsObject";
        } else if (strcmp(name->value, "as_bool") == 0) {
            handled_bool_conversion = 1;
        } else if (strcmp(name->value, "as_float") == 0) {
            handled_float_conversion = 1;
        } else if (strcmp(name->value, "as_string") == 0) {
            handled_string_conversion = 1;
        }

        if (predicate != NULL || handled_bool_conversion || handled_float_conversion || handled_string_conversion) {
            codegen_emit_type_name(ctx, return_type);
            fprintf(ctx->out, " %s(", c_name);
            emit_parameter_list(ctx, parameters, 0);
            fputs(")\n{\n", ctx->out);
            ctx->indent_level++;
            codegen_emit_indent(ctx);
            fprintf(ctx->out, "if (%s == NULL || %s->node == NULL) {\n", value_name, value_name);
            ctx->indent_level++;
            codegen_emit_indent(ctx);
            fputs("py4_json_fail(\"cannot use a null json value handle\");\n", ctx->out);
            ctx->indent_level--;
            codegen_emit_indent(ctx);
            fputs("}\n", ctx->out);

            if (predicate != NULL) {
                codegen_emit_indent(ctx);
                fprintf(ctx->out, "return %s(%s->node) ? true : false;\n", predicate, value_name);
            } else if (handled_bool_conversion) {
                codegen_emit_indent(ctx);
                fprintf(ctx->out, "if (!cJSON_IsBool(%s->node)) {\n", value_name);
                ctx->indent_level++;
                codegen_emit_indent(ctx);
                fputs("py4_json_fail(\"json value is not a bool\");\n", ctx->out);
                ctx->indent_level--;
                codegen_emit_indent(ctx);
                fputs("}\n", ctx->out);
                codegen_emit_indent(ctx);
                fprintf(ctx->out, "return cJSON_IsTrue(%s->node) ? true : false;\n", value_name);
            } else if (handled_float_conversion) {
                codegen_emit_indent(ctx);
                fprintf(ctx->out, "if (!cJSON_IsNumber(%s->node)) {\n", value_name);
                ctx->indent_level++;
                codegen_emit_indent(ctx);
                fputs("py4_json_fail(\"json value is not a number\");\n", ctx->out);
                ctx->indent_level--;
                codegen_emit_indent(ctx);
                fputs("}\n", ctx->out);
                codegen_emit_indent(ctx);
                fprintf(ctx->out, "return %s->node->valuedouble;\n", value_name);
            } else if (handled_string_conversion) {
                codegen_emit_indent(ctx);
                fprintf(ctx->out, "if (!cJSON_IsString(%s->node) || %s->node->valuestring == NULL) {\n", value_name, value_name);
                ctx->indent_level++;
                codegen_emit_indent(ctx);
                fputs("py4_json_fail(\"json value is not a string\");\n", ctx->out);
                ctx->indent_level--;
                codegen_emit_indent(ctx);
                fputs("}\n", ctx->out);
                codegen_emit_indent(ctx);
                fprintf(ctx->out, "return py4_json_strdup(%s->node->valuestring);\n", value_name);
            }

            ctx->indent_level--;
            fputs("}\n\n", ctx->out);
            return 1;
        }
    }

    return 0;
}
