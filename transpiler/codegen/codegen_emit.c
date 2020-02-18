#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen_internal.h"
#include "semantic_internal.h"

static void emit_auto_main(CodegenContext *ctx)
{
    if (ctx->has_user_main) {
        return;
    }

    fputs("int main(void)\n{\n", ctx->out);
    ctx->indent_level++;
    if (ctx->has_top_level_executable_statements) {
        codegen_emit_indent(ctx);
        fputs("py4_module_init();\n", ctx->out);
    }
    codegen_emit_indent(ctx);
    fputs("return 0;\n", ctx->out);
    ctx->indent_level--;
    fputs("}\n", ctx->out);
}

void emit_c_program(FILE *out, const LoadedProgram *program, const SemanticInfo *info)
{
    CodegenContext ctx = {0};
    const ParseNode *root;

    if (program == NULL) {
        codegen_error("expected loaded program");
    }
    root = program->emission_root;
    if (root == NULL || root->kind != NODE_S) {
        codegen_error("expected program root");
    }

    ctx.out = out;
    ctx.root = root;
    ctx.semantic = info;
    codegen_collect_program_state(&ctx, root);
    for (size_t i = 0; i < semantic_dict_type_count(); i++) {
        ValueType dict_type = semantic_dict_type_at(i);
        (void)semantic_make_list_type(semantic_dict_key_type(dict_type));
        ValueType value_type = semantic_dict_value_type(dict_type);
        ValueType dict_item_types[2] = {
            semantic_dict_key_type(dict_type),
            value_type
        };

        (void)semantic_make_list_type(value_type);
        (void)semantic_make_list_type(semantic_make_tuple_type(dict_item_types, 2));
    }

    fputs("#include <stdbool.h>\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n\n", out);
    fputs("static int py4_ord(char value)\n{\n", out);
    fputs("    return (int)(unsigned char)value;\n", out);
    fputs("}\n\n", out);
    fputs("static char py4_chr(int value)\n{\n", out);
    fputs("    if (value < 0 || value > 255) {\n", out);
    fputs("        fprintf(stderr, \"Runtime error: chr code out of bounds\\n\");\n", out);
    fputs("        exit(1);\n", out);
    fputs("    }\n", out);
    fputs("    return (char)(unsigned char)value;\n", out);
    fputs("}\n\n", out);
    fputs("static bool py4_char_is_digit(char value)\n{\n", out);
    fputs("    return value >= '0' && value <= '9';\n", out);
    fputs("}\n\n", out);
    fputs("static bool py4_char_is_alpha(char value)\n{\n", out);
    fputs("    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');\n", out);
    fputs("}\n\n", out);
    fputs("static bool py4_char_is_space(char value)\n{\n", out);
    fputs("    return value == ' ' || value == '\\t' || value == '\\n' || value == '\\r' || value == '\\f' || value == '\\v';\n", out);
    fputs("}\n\n", out);
    fputs("static char *py4_str_dup(const char *value)\n{\n", out);
    fputs("    size_t len;\n", out);
    fputs("    char *result;\n", out);
    fputs("    if (value == NULL) {\n", out);
    fputs("        fprintf(stderr, \"Runtime error: str dup received null\\n\");\n", out);
    fputs("        exit(1);\n", out);
    fputs("    }\n", out);
    fputs("    len = strlen(value);\n", out);
    fputs("    result = malloc(len + 1);\n", out);
    fputs("    if (result == NULL) {\n", out);
    fputs("        perror(\"malloc\");\n", out);
    fputs("        exit(1);\n", out);
    fputs("    }\n", out);
    fputs("    memcpy(result, value, len + 1);\n", out);
    fputs("    return result;\n", out);
    fputs("}\n\n", out);
    fputs("static char *py4_str_concat(const char *lhs, const char *rhs)\n{\n", out);
    fputs("    size_t lhs_len;\n", out);
    fputs("    size_t rhs_len;\n", out);
    fputs("    char *result;\n", out);
    fputs("    if (lhs == NULL || rhs == NULL) {\n", out);
    fputs("        fprintf(stderr, \"Runtime error: str concat received null\\n\");\n", out);
    fputs("        exit(1);\n", out);
    fputs("    }\n", out);
    fputs("    lhs_len = strlen(lhs);\n", out);
    fputs("    rhs_len = strlen(rhs);\n", out);
    fputs("    result = malloc(lhs_len + rhs_len + 1);\n", out);
    fputs("    if (result == NULL) {\n", out);
    fputs("        perror(\"malloc\");\n", out);
    fputs("        exit(1);\n", out);
    fputs("    }\n", out);
    fputs("    memcpy(result, lhs, lhs_len);\n", out);
    fputs("    memcpy(result + lhs_len, rhs, rhs_len + 1);\n", out);
    fputs("    return result;\n", out);
    fputs("}\n\n", out);
    fputs("static bool py4_str_starts_with(const char *value, const char *prefix)\n{\n", out);
    fputs("    size_t value_len;\n", out);
    fputs("    size_t prefix_len;\n", out);
    fputs("    if (value == NULL || prefix == NULL) {\n", out);
    fputs("        fprintf(stderr, \"Runtime error: str starts_with received null\\n\");\n", out);
    fputs("        exit(1);\n", out);
    fputs("    }\n", out);
    fputs("    value_len = strlen(value);\n", out);
    fputs("    prefix_len = strlen(prefix);\n", out);
    fputs("    if (prefix_len > value_len) {\n", out);
    fputs("        return false;\n", out);
    fputs("    }\n", out);
    fputs("    return strncmp(value, prefix, prefix_len) == 0;\n", out);
    fputs("}\n\n", out);
    fputs("static bool py4_str_ends_with(const char *value, const char *suffix)\n{\n", out);
    fputs("    size_t value_len;\n", out);
    fputs("    size_t suffix_len;\n", out);
    fputs("    if (value == NULL || suffix == NULL) {\n", out);
    fputs("        fprintf(stderr, \"Runtime error: str ends_with received null\\n\");\n", out);
    fputs("        exit(1);\n", out);
    fputs("    }\n", out);
    fputs("    value_len = strlen(value);\n", out);
    fputs("    suffix_len = strlen(suffix);\n", out);
    fputs("    if (suffix_len > value_len) {\n", out);
    fputs("        return false;\n", out);
    fputs("    }\n", out);
    fputs("    return strcmp(value + (value_len - suffix_len), suffix) == 0;\n", out);
    fputs("}\n\n", out);
    fputs("static int py4_str_find(const char *value, const char *needle)\n{\n", out);
    fputs("    const char *match;\n", out);
    fputs("    if (value == NULL || needle == NULL) {\n", out);
    fputs("        fprintf(stderr, \"Runtime error: str find received null\\n\");\n", out);
    fputs("        exit(1);\n", out);
    fputs("    }\n", out);
    fputs("    match = strstr(value, needle);\n", out);
    fputs("    if (match == NULL) {\n", out);
    fputs("        return -1;\n", out);
    fputs("    }\n", out);
    fputs("    return (int)(match - value);\n", out);
    fputs("}\n\n", out);
    fputs("static char *py4_str_from_int(int value)\n{\n", out);
    fputs("    int len;\n", out);
    fputs("    char *result;\n", out);
    fputs("    len = snprintf(NULL, 0, \"%d\", value);\n", out);
    fputs("    result = malloc((size_t)len + 1);\n", out);
    fputs("    if (result == NULL) {\n", out);
    fputs("        perror(\"malloc\");\n", out);
    fputs("        exit(1);\n", out);
    fputs("    }\n", out);
    fputs("    snprintf(result, (size_t)len + 1, \"%d\", value);\n", out);
    fputs("    return result;\n", out);
    fputs("}\n\n", out);
    fputs("static char py4_str_get(const char *value, int index)\n{\n", out);
    fputs("    size_t len;\n", out);
    fputs("    if (value == NULL) {\n", out);
    fputs("        fprintf(stderr, \"Runtime error: str index on null\\n\");\n", out);
    fputs("        exit(1);\n", out);
    fputs("    }\n", out);
    fputs("    len = strlen(value);\n", out);
    fputs("    if (index < 0 || (size_t)index >= len) {\n", out);
    fputs("        fprintf(stderr, \"Runtime error: str index out of bounds\\n\");\n", out);
    fputs("        exit(1);\n", out);
    fputs("    }\n", out);
    fputs("    return value[index];\n", out);
    fputs("}\n\n", out);
    fputs("static char *py4_str_slice(const char *value, int start, int end)\n{\n", out);
    fputs("    size_t len;\n", out);
    fputs("    char *result;\n", out);
    fputs("    if (value == NULL) {\n", out);
    fputs("        fprintf(stderr, \"Runtime error: str slice on null\\n\");\n", out);
    fputs("        exit(1);\n", out);
    fputs("    }\n", out);
    fputs("    len = strlen(value);\n", out);
    fputs("    if (start < 0 || end < start || (size_t)end > len) {\n", out);
    fputs("        fprintf(stderr, \"Runtime error: str slice out of bounds\\n\");\n", out);
    fputs("        exit(1);\n", out);
    fputs("    }\n", out);
    fputs("    result = malloc((size_t)(end - start) + 1);\n", out);
    fputs("    if (result == NULL) {\n", out);
    fputs("        perror(\"malloc\");\n", out);
    fputs("        exit(1);\n", out);
    fputs("    }\n", out);
    fputs("    memcpy(result, value + start, (size_t)(end - start));\n", out);
    fputs("    result[end - start] = '\\0';\n", out);
    fputs("    return result;\n", out);
    fputs("}\n\n", out);
    codegen_emit_struct_declarations(&ctx);
    emit_native_type_runtime(&ctx);
    codegen_emit_container_runtime(&ctx);
    fputs("static Py4List_str *py4_str_split(const char *value, const char *sep)\n{\n", out);
    fputs("    Py4List_str *result;\n", out);
    fputs("    const char *start;\n", out);
    fputs("    const char *match;\n", out);
    fputs("    size_t sep_len;\n", out);
    fputs("    if (value == NULL || sep == NULL) {\n", out);
    fputs("        fprintf(stderr, \"Runtime error: str split received null\\n\");\n", out);
    fputs("        exit(1);\n", out);
    fputs("    }\n", out);
    fputs("    sep_len = strlen(sep);\n", out);
    fputs("    if (sep_len == 0) {\n", out);
    fputs("        fprintf(stderr, \"Runtime error: str split separator cannot be empty\\n\");\n", out);
    fputs("        exit(1);\n", out);
    fputs("    }\n", out);
    fputs("    result = py4_list_str_new();\n", out);
    fputs("    start = value;\n", out);
    fputs("    while ((match = strstr(start, sep)) != NULL) {\n", out);
    fputs("        size_t part_len = (size_t)(match - start);\n", out);
    fputs("        char *part = malloc(part_len + 1);\n", out);
    fputs("        if (part == NULL) {\n", out);
    fputs("            perror(\"malloc\");\n", out);
    fputs("            exit(1);\n", out);
    fputs("        }\n", out);
    fputs("        memcpy(part, start, part_len);\n", out);
    fputs("        part[part_len] = '\\0';\n", out);
    fputs("        py4_list_str_append(result, part);\n", out);
    fputs("        start = match + sep_len;\n", out);
    fputs("    }\n", out);
    fputs("    py4_list_str_append(result, py4_str_dup(start));\n", out);
    fputs("    return result;\n", out);
    fputs("}\n\n", out);
    fputs("static char *py4_str_replace(const char *value, const char *old_value, const char *new_value)\n{\n", out);
    fputs("    size_t value_len;\n", out);
    fputs("    size_t old_len;\n", out);
    fputs("    size_t new_len;\n", out);
    fputs("    size_t count = 0;\n", out);
    fputs("    size_t result_len;\n", out);
    fputs("    char *result;\n", out);
    fputs("    char *write_ptr;\n", out);
    fputs("    const char *read_ptr;\n", out);
    fputs("    const char *match;\n", out);
    fputs("    if (value == NULL || old_value == NULL || new_value == NULL) {\n", out);
    fputs("        fprintf(stderr, \"Runtime error: str replace received null\\n\");\n", out);
    fputs("        exit(1);\n", out);
    fputs("    }\n", out);
    fputs("    old_len = strlen(old_value);\n", out);
    fputs("    if (old_len == 0) {\n", out);
    fputs("        fprintf(stderr, \"Runtime error: str replace old value cannot be empty\\n\");\n", out);
    fputs("        exit(1);\n", out);
    fputs("    }\n", out);
    fputs("    value_len = strlen(value);\n", out);
    fputs("    new_len = strlen(new_value);\n", out);
    fputs("    read_ptr = value;\n", out);
    fputs("    while ((match = strstr(read_ptr, old_value)) != NULL) {\n", out);
    fputs("        count++;\n", out);
    fputs("        read_ptr = match + old_len;\n", out);
    fputs("    }\n", out);
    fputs("    if (new_len >= old_len) {\n", out);
    fputs("        result_len = value_len + count * (new_len - old_len);\n", out);
    fputs("    } else {\n", out);
    fputs("        result_len = value_len - count * (old_len - new_len);\n", out);
    fputs("    }\n", out);
    fputs("    result = malloc(result_len + 1);\n", out);
    fputs("    if (result == NULL) {\n", out);
    fputs("        perror(\"malloc\");\n", out);
    fputs("        exit(1);\n", out);
    fputs("    }\n", out);
    fputs("    read_ptr = value;\n", out);
    fputs("    write_ptr = result;\n", out);
    fputs("    while ((match = strstr(read_ptr, old_value)) != NULL) {\n", out);
    fputs("        size_t part_len = (size_t)(match - read_ptr);\n", out);
    fputs("        memcpy(write_ptr, read_ptr, part_len);\n", out);
    fputs("        write_ptr += part_len;\n", out);
    fputs("        memcpy(write_ptr, new_value, new_len);\n", out);
    fputs("        write_ptr += new_len;\n", out);
    fputs("        read_ptr = match + old_len;\n", out);
    fputs("    }\n", out);
    fputs("    strcpy(write_ptr, read_ptr);\n", out);
    fputs("    return result;\n", out);
    fputs("}\n\n", out);
    emit_native_function_runtime(&ctx, root);
    codegen_emit_struct_types(&ctx);
    codegen_emit_union_runtime(&ctx);
    emit_global_declarations(&ctx, root);
    emit_function_prototypes(&ctx, root);
    emit_module_init(&ctx, root);
    emit_top_level_functions(&ctx, root);
    emit_auto_main(&ctx);
}
