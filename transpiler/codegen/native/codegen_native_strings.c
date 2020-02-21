#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen_native_internal.h"

int emit_native_strings_function_definition(
    CodegenContext *ctx,
    const char *module_name,
    const ParseNode *name,
    const ParseNode *parameters,
    const char *c_name,
    ValueType return_type,
    ValueType first_param_type)
{
    if (module_name != NULL &&
        strcmp(module_name, "strings") == 0 &&
        strcmp(name->value, "starts_with") == 0 &&
        parameters->child_count == 2 &&
        first_param_type == TYPE_STR) {
        const char *value = parameters->children[0]->value;
        const char *prefix = parameters->children[1]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return py4_str_starts_with(%s, %s);\n", value, prefix);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "strings") == 0 &&
        strcmp(name->value, "ends_with") == 0 &&
        parameters->child_count == 2 &&
        first_param_type == TYPE_STR) {
        const char *value = parameters->children[0]->value;
        const char *suffix = parameters->children[1]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return py4_str_ends_with(%s, %s);\n", value, suffix);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "strings") == 0 &&
        strcmp(name->value, "find") == 0 &&
        parameters->child_count == 2 &&
        first_param_type == TYPE_STR) {
        const char *value = parameters->children[0]->value;
        const char *needle = parameters->children[1]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return py4_str_find(%s, %s);\n", value, needle);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "strings") == 0 &&
        strcmp(name->value, "from_int") == 0 &&
        parameters->child_count == 1 &&
        first_param_type == TYPE_INT) {
        const char *value = parameters->children[0]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return py4_str_from_int(%s);\n", value);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "strings") == 0 &&
        strcmp(name->value, "split") == 0 &&
        parameters->child_count == 2 &&
        first_param_type == TYPE_STR) {
        const char *value = parameters->children[0]->value;
        const char *sep = parameters->children[1]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return py4_str_split(%s, %s);\n", value, sep);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "strings") == 0 &&
        strcmp(name->value, "replace") == 0 &&
        parameters->child_count == 3 &&
        first_param_type == TYPE_STR) {
        const char *value = parameters->children[0]->value;
        const char *old_value = parameters->children[1]->value;
        const char *new_value = parameters->children[2]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return py4_str_replace(%s, %s, %s);\n", value, old_value, new_value);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    return 0;
}
