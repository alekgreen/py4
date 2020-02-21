#ifndef CODEGEN_NATIVE_INTERNAL_H
#define CODEGEN_NATIVE_INTERNAL_H

#include "codegen_internal.h"

void emit_native_type_runtime(CodegenContext *ctx);
void emit_native_io_type_runtime(CodegenContext *ctx);
void emit_native_json_type_runtime(CodegenContext *ctx);
void emit_native_function_runtime(CodegenContext *ctx, const ParseNode *root);
int emit_native_math_function_definition(
    CodegenContext *ctx,
    const char *module_name,
    const ParseNode *name,
    const ParseNode *parameters,
    const char *c_name,
    ValueType return_type,
    ValueType first_param_type);
int emit_native_strings_function_definition(
    CodegenContext *ctx,
    const char *module_name,
    const ParseNode *name,
    const ParseNode *parameters,
    const char *c_name,
    ValueType return_type,
    ValueType first_param_type);
int emit_native_chars_function_definition(
    CodegenContext *ctx,
    const char *module_name,
    const ParseNode *name,
    const ParseNode *parameters,
    const char *c_name,
    ValueType return_type,
    ValueType first_param_type);
int emit_native_io_function_definition(
    CodegenContext *ctx,
    const char *module_name,
    const ParseNode *name,
    const ParseNode *parameters,
    const char *c_name,
    ValueType return_type,
    ValueType first_param_type);
int emit_native_json_function_definition(
    CodegenContext *ctx,
    const char *module_name,
    const ParseNode *name,
    const ParseNode *parameters,
    const char *c_name,
    ValueType return_type,
    ValueType first_param_type);

#endif
