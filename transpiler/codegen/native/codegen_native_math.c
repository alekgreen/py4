#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen_internal.h"
#include "semantic_internal.h"

int emit_native_math_function_definition(
    CodegenContext *ctx,
    const char *module_name,
    const ParseNode *name,
    const ParseNode *parameters,
    const char *c_name,
    ValueType return_type,
    ValueType first_param_type)
{
    if (module_name != NULL &&
        strcmp(module_name, "math") == 0 &&
        strcmp(name->value, "abs") == 0 &&
        parameters->child_count == 1 &&
        first_param_type == TYPE_INT) {
        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return %s < 0 ? -%s : %s;\n",
            parameters->children[0]->value,
            parameters->children[0]->value,
            parameters->children[0]->value);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "math") == 0 &&
        strcmp(name->value, "abs") == 0 &&
        parameters->child_count == 1 &&
        first_param_type == TYPE_FLOAT) {
        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return %s < 0 ? -%s : %s;\n",
            parameters->children[0]->value,
            parameters->children[0]->value,
            parameters->children[0]->value);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "math") == 0 &&
        strcmp(name->value, "max") == 0 &&
        parameters->child_count == 2 &&
        first_param_type == TYPE_INT) {
        const char *a = parameters->children[0]->value;
        const char *b = parameters->children[1]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return %s > %s ? %s : %s;\n", a, b, a, b);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "math") == 0 &&
        strcmp(name->value, "max") == 0 &&
        parameters->child_count == 2 &&
        first_param_type == TYPE_FLOAT) {
        const char *a = parameters->children[0]->value;
        const char *b = parameters->children[1]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return %s > %s ? %s : %s;\n", a, b, a, b);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "math") == 0 &&
        strcmp(name->value, "min") == 0 &&
        parameters->child_count == 2 &&
        first_param_type == TYPE_INT) {
        const char *a = parameters->children[0]->value;
        const char *b = parameters->children[1]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return %s < %s ? %s : %s;\n", a, b, a, b);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "math") == 0 &&
        strcmp(name->value, "min") == 0 &&
        parameters->child_count == 2 &&
        first_param_type == TYPE_FLOAT) {
        const char *a = parameters->children[0]->value;
        const char *b = parameters->children[1]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return %s < %s ? %s : %s;\n", a, b, a, b);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    return 0;
}
