#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen_native_internal.h"

int emit_native_chars_function_definition(
    CodegenContext *ctx,
    const char *module_name,
    const ParseNode *name,
    const ParseNode *parameters,
    const char *c_name,
    ValueType return_type,
    ValueType first_param_type)
{
    if (module_name != NULL &&
        strcmp(module_name, "chars") == 0 &&
        strcmp(name->value, "is_digit") == 0 &&
        parameters->child_count == 1 &&
        first_param_type == TYPE_CHAR) {
        const char *value = parameters->children[0]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return py4_char_is_digit(%s);\n", value);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "chars") == 0 &&
        strcmp(name->value, "is_alpha") == 0 &&
        parameters->child_count == 1 &&
        first_param_type == TYPE_CHAR) {
        const char *value = parameters->children[0]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return py4_char_is_alpha(%s);\n", value);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "chars") == 0 &&
        strcmp(name->value, "is_space") == 0 &&
        parameters->child_count == 1 &&
        first_param_type == TYPE_CHAR) {
        const char *value = parameters->children[0]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return py4_char_is_space(%s);\n", value);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    return 0;
}
