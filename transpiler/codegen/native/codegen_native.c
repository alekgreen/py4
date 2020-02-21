#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen_native_internal.h"

static int native_type_runtime_emitted(ValueType type)
{
    if (!semantic_type_is_native(type)) {
        return 0;
    }

    return (strcmp(semantic_native_type_module(type), "io") == 0 &&
            strcmp(semantic_native_type_name(type), "File") == 0) ||
        (strcmp(semantic_native_type_module(type), "json") == 0 &&
            strcmp(semantic_native_type_name(type), "Value") == 0);
}

void emit_native_type_runtime(CodegenContext *ctx)
{
    int emitted_io_file = 0;
    int emitted_json_value = 0;

    for (size_t i = 0; i < semantic_native_type_count(); i++) {
        ValueType type = semantic_native_type_at(i);

        if (!native_type_runtime_emitted(type)) {
            continue;
        }

        if (strcmp(semantic_native_type_module(type), "io") == 0 &&
            strcmp(semantic_native_type_name(type), "File") == 0) {
            if (emitted_io_file) {
                continue;
            }

            emit_native_io_type_runtime(ctx);
            emitted_io_file = 1;
            continue;
        }

        if (strcmp(semantic_native_type_module(type), "json") == 0 &&
            strcmp(semantic_native_type_name(type), "Value") == 0) {
            if (emitted_json_value) {
                continue;
            }

            emit_native_json_type_runtime(ctx);
            emitted_json_value = 1;
            continue;
        }
    }
}

static void emit_native_function_definition(CodegenContext *ctx, const ParseNode *function_def)
{
    const ParseNode *name = codegen_expect_child(function_def, 0, NODE_PRIMARY);
    const ParseNode *parameters = codegen_function_parameters(function_def);
    const char *c_name = codegen_function_c_name(ctx, function_def);
    const char *module_name = semantic_module_name_for_path(ctx->semantic, function_def->source_path);
    ValueType return_type = codegen_function_return_type(ctx, function_def);
    ValueType first_param_type = parameters->child_count > 0
        ? semantic_type_of(ctx->semantic, codegen_expect_child(parameters->children[0], 0, NODE_TYPE))
        : TYPE_NONE;

    if (emit_native_math_function_definition(
            ctx,
            module_name,
            name,
            parameters,
            c_name,
            return_type,
            first_param_type) ||
        emit_native_strings_function_definition(
            ctx,
            module_name,
            name,
            parameters,
            c_name,
            return_type,
            first_param_type) ||
        emit_native_chars_function_definition(
            ctx,
            module_name,
            name,
            parameters,
            c_name,
            return_type,
            first_param_type) ||
        emit_native_io_function_definition(
            ctx,
            module_name,
            name,
            parameters,
            c_name,
            return_type,
            first_param_type) ||
        emit_native_json_function_definition(
            ctx,
            module_name,
            name,
            parameters,
            c_name,
            return_type,
            first_param_type)) {
        return;
    }

    codegen_error("unsupported native stdlib function %s", name->value);
}

void emit_native_function_runtime(CodegenContext *ctx, const ParseNode *root)
{
    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = codegen_statement_payload(root->children[i]);

        if (payload->kind != NODE_NATIVE_FUNCTION_DEF) {
            continue;
        }

        emit_native_function_definition(ctx, payload);
    }
}
