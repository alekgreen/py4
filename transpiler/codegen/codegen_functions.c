#include <string.h>

#include "codegen_internal.h"
#include "semantic_codegen_internal.h"

static size_t class_member_start_index(const ParseNode *class_def)
{
    return (class_def->child_count > 1 && class_def->children[1]->kind == NODE_TYPE) ? 3 : 2;
}

void emit_parameter_list(CodegenContext *ctx, const ParseNode *parameters, int is_c_main)
{
    if (is_c_main) {
        fputs("void", ctx->out);
        return;
    }

    if (parameters->child_count == 1 && codegen_is_epsilon_node(parameters->children[0])) {
        fputs("void", ctx->out);
        return;
    }

    for (size_t i = 0; i < parameters->child_count; i++) {
        const ParseNode *param = parameters->children[i];
        const ParseNode *type_node = codegen_expect_child(param, 0, NODE_TYPE);

        if (i > 0) {
            fputs(", ", ctx->out);
        }
        codegen_emit_type_name(ctx, semantic_type_of(ctx->semantic, type_node));
        if (ctx->current_function_is_init && i == 0) {
            fprintf(ctx->out, " *%s", param->value);
        } else {
            fprintf(ctx->out, " %s", param->value);
        }
    }
}

static const ParseNode *find_class_init_definition(const ParseNode *class_def)
{
    for (size_t i = class_member_start_index(class_def); i < class_def->child_count; i++) {
        const ParseNode *member = class_def->children[i];

        if (member->kind == NODE_FUNCTION_DEF &&
            strcmp(codegen_expect_child(member, 0, NODE_PRIMARY)->value, "__init__") == 0) {
            return member;
        }
    }

    return NULL;
}

static void emit_constructor_parameter_list(CodegenContext *ctx, const ParseNode *init_def)
{
    const ParseNode *parameters = codegen_function_parameters(init_def);

    if (parameters->child_count <= 1) {
        fputs("void", ctx->out);
        return;
    }

    for (size_t i = 1; i < parameters->child_count; i++) {
        const ParseNode *param = parameters->children[i];
        const ParseNode *type_node = codegen_expect_child(param, 0, NODE_TYPE);

        if (i > 1) {
            fputs(", ", ctx->out);
        }
        codegen_emit_type_name(ctx, semantic_type_of(ctx->semantic, type_node));
        fprintf(ctx->out, " %s", param->value);
    }
}

static void emit_constructor_helper_signature(
    CodegenContext *ctx,
    ValueType owner_type,
    const ParseNode *init_def,
    int prototype_only)
{
    char ctor_name[MAX_NAME_LEN];

    codegen_build_class_ctor_name(ctor_name, sizeof(ctor_name), owner_type);
    codegen_emit_type_name(ctx, owner_type);
    fprintf(ctx->out, " %s(", ctor_name);
    emit_constructor_parameter_list(ctx, init_def);
    fputc(')', ctx->out);
    if (prototype_only) {
        fputs(";\n", ctx->out);
    }
}

static ValueType codegen_method_owner_type(CodegenContext *ctx, const ParseNode *function_def)
{
    for (size_t i = 0; i < ctx->root->child_count; i++) {
        const ParseNode *payload = codegen_statement_payload(ctx->root->children[i]);

        if (payload->kind != NODE_CLASS_DEF) {
            continue;
        }
        for (size_t j = class_member_start_index(payload); j < payload->child_count; j++) {
            if (payload->children[j] == function_def) {
                return semantic_find_class_type(codegen_expect_child(payload, 0, NODE_PRIMARY)->value);
            }
        }
    }

    return 0;
}

static void emit_function_signature(CodegenContext *ctx, const ParseNode *function_def, int prototype_only)
{
    const ParseNode *name = codegen_expect_child(function_def, 0, NODE_PRIMARY);
    const ParseNode *parameters = codegen_function_parameters(function_def);
    ValueType owner_type = codegen_method_owner_type(ctx, function_def);
    int is_c_main = owner_type == 0 && codegen_is_main_function(function_def);
    int prev_is_init = ctx->current_function_is_init;
    int is_init_method = owner_type != 0 && strcmp(name->value, "__init__") == 0;
    ValueType return_type = codegen_function_return_type(ctx, function_def);

    ctx->current_function_is_init = is_init_method;
    if (is_c_main) {
        fputs("int main(", ctx->out);
    } else {
        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", owner_type != 0
            ? semantic_method_c_name(ctx->semantic, owner_type, name->value)
            : semantic_function_c_name(ctx->semantic, function_def));
    }
    emit_parameter_list(ctx, parameters, is_c_main);
    fputc(')', ctx->out);
    if (prototype_only) {
        fputs(";\n", ctx->out);
    }
    ctx->current_function_is_init = prev_is_init;
}

void emit_function_definition(CodegenContext *ctx, const ParseNode *function_def)
{
    ValueType owner_type = codegen_method_owner_type(ctx, function_def);
    int is_c_main = owner_type == 0 && codegen_is_main_function(function_def);
    int is_init_method = owner_type != 0 &&
        strcmp(codegen_expect_child(function_def, 0, NODE_PRIMARY)->value, "__init__") == 0;
    ValueType return_type = codegen_function_return_type(ctx, function_def);
    const ParseNode *parameters = codegen_function_parameters(function_def);
    int prev_is_main = ctx->current_function_is_main;
    int prev_is_init = ctx->current_function_is_init;
    ValueType prev_method_owner_type = ctx->current_method_owner_type;
    ValueType prev_return_type = ctx->current_function_return_type;

    ctx->current_function_is_init = is_init_method;
    emit_function_signature(ctx, function_def, 0);
    fputs("\n{\n", ctx->out);
    ctx->indent_level++;
    ctx->current_function_is_main = is_c_main;
    ctx->current_method_owner_type = owner_type;
    ctx->current_function_return_type = return_type;
    codegen_push_cleanup_scope(ctx);

    if (!(parameters->child_count == 1 && codegen_is_epsilon_node(parameters->children[0]))) {
        for (size_t i = 0; i < parameters->child_count; i++) {
            const ParseNode *param = parameters->children[i];
            ValueType param_type = semantic_type_of(ctx->semantic, codegen_expect_child(param, 0, NODE_TYPE));

            if (is_init_method && i == 0) {
                continue;
            }
            if (!semantic_type_needs_management(param_type)) {
                continue;
            }

            codegen_emit_value_retain(ctx, param_type, param->value);
            codegen_register_ref_local(ctx, param->value, param_type);
        }
    }

    if (is_c_main && ctx->has_top_level_executable_statements) {
        codegen_emit_indent(ctx);
        fputs("py4_module_init();\n", ctx->out);
    }

    codegen_emit_suite(ctx, codegen_function_suite(function_def));
    codegen_pop_cleanup_scope(ctx);

    if (is_c_main) {
        codegen_emit_indent(ctx);
        fputs("return 0;\n", ctx->out);
    }

    ctx->current_function_is_main = prev_is_main;
    ctx->current_function_is_init = prev_is_init;
    ctx->current_method_owner_type = prev_method_owner_type;
    ctx->current_function_return_type = prev_return_type;
    ctx->indent_level--;
    fputs("}\n", ctx->out);
}

static void emit_synthetic_method_signature(CodegenContext *ctx, const MethodInfo *method, int prototype_only)
{
    codegen_emit_type_name(ctx, method->return_type);
    fprintf(ctx->out, " %s(", method->c_name);
    for (size_t i = 0; i < method->param_count; i++) {
        if (i > 0) {
            fputs(", ", ctx->out);
        }
        codegen_emit_type_name(ctx, method->param_types[i]);
        if (i == 0) {
            fputs(" self", ctx->out);
        } else {
            fprintf(ctx->out, " arg%zu", i);
        }
    }
    if (method->param_count == 0) {
        fputs("void", ctx->out);
    }
    fputc(')', ctx->out);
    if (prototype_only) {
        fputs(";\n", ctx->out);
    }
}

static void emit_synthetic_method_definition(CodegenContext *ctx, const MethodInfo *method)
{
    MethodInfo *source_method = semantic_find_method(
        semantic_methods(ctx->semantic),
        method->source_owner_type,
        method->name);
    size_t field_count = semantic_class_field_count(method->source_owner_type);

    if (source_method == NULL) {
        codegen_error("missing inherited source method '%s' on %s",
            method->name,
            semantic_type_name(method->source_owner_type));
    }

    emit_synthetic_method_signature(ctx, method, 0);
    fputs("\n{\n", ctx->out);
    ctx->indent_level++;
    codegen_emit_indent(ctx);
    codegen_emit_type_name(ctx, method->source_owner_type);
    fputs(" base_self = ", ctx->out);
    if (field_count == 0) {
        fputs("{0};\n", ctx->out);
    } else {
        fputc('{', ctx->out);
        for (size_t i = 0; i < field_count; i++) {
            if (i > 0) {
                fputs(", ", ctx->out);
            }
            fprintf(ctx->out, "self.%s", semantic_class_field_name(method->owner_type, i));
        }
        fputs("};\n", ctx->out);
    }

    codegen_emit_indent(ctx);
    if (method->return_type != TYPE_NONE) {
        fputs("return ", ctx->out);
    }
    fprintf(ctx->out, "%s(base_self", source_method->c_name);
    for (size_t i = 1; i < method->param_count; i++) {
        fprintf(ctx->out, ", arg%zu", i);
    }
    fputs(");\n", ctx->out);
    ctx->indent_level--;
    fputs("}\n", ctx->out);
}

static void emit_constructor_helper_definition(
    CodegenContext *ctx,
    ValueType owner_type,
    const ParseNode *init_def)
{
    char ctor_name[MAX_NAME_LEN];
    const ParseNode *parameters = codegen_function_parameters(init_def);
    const char *init_c_name = semantic_method_c_name(ctx->semantic, owner_type, "__init__");

    codegen_build_class_ctor_name(ctor_name, sizeof(ctor_name), owner_type);
    emit_constructor_helper_signature(ctx, owner_type, init_def, 0);
    fputs("\n{\n", ctx->out);
    ctx->indent_level++;
    codegen_emit_indent(ctx);
    codegen_emit_type_name(ctx, owner_type);
    fprintf(ctx->out, " tmp = {0};\n");
    codegen_emit_indent(ctx);
    fprintf(ctx->out, "%s(&tmp", init_c_name);
    if (parameters->child_count > 1) {
        for (size_t i = 1; i < parameters->child_count; i++) {
            fprintf(ctx->out, ", %s", parameters->children[i]->value);
        }
    }
    fputs(");\n", ctx->out);
    codegen_emit_indent(ctx);
    fputs("return tmp;\n", ctx->out);
    ctx->indent_level--;
    fputs("}\n", ctx->out);
}

void emit_function_prototypes(CodegenContext *ctx, const ParseNode *root)
{
    int wrote_any = 0;

    if (ctx->has_top_level_executable_statements) {
        fputs("static void py4_module_init(void);\n", ctx->out);
        wrote_any = 1;
    }

    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = codegen_statement_payload(root->children[i]);

        if (payload->kind == NODE_IMPORT_STATEMENT) {
            codegen_error("imports should be resolved before C code generation");
        } else if (payload->kind == NODE_ENUM_DEF) {
            continue;
        } else if (payload->kind == NODE_NATIVE_TYPE_DEF) {
            continue;
        } else if (payload->kind == NODE_NATIVE_FUNCTION_DEF) {
            continue;
        } else if (payload->kind == NODE_CLASS_DEF) {
            ValueType class_type = semantic_find_class_type(codegen_expect_child(payload, 0, NODE_PRIMARY)->value);

            for (size_t j = class_member_start_index(payload); j < payload->child_count; j++) {
                if (payload->children[j]->kind == NODE_FUNCTION_DEF) {
                    emit_function_signature(ctx, payload->children[j], 1);
                    wrote_any = 1;
                }
            }
            for (MethodInfo *method = semantic_methods(ctx->semantic); method != NULL; method = method->next) {
                if (method->owner_type == class_type && method->node == NULL) {
                    emit_synthetic_method_signature(ctx, method, 1);
                    wrote_any = 1;
                }
            }
            {
                const ParseNode *init_def = find_class_init_definition(payload);

                if (init_def != NULL) {
                    emit_constructor_helper_signature(ctx, class_type, init_def, 1);
                    wrote_any = 1;
                }
            }
            continue;
        } else if (payload->kind == NODE_FUNCTION_DEF) {
            emit_function_signature(ctx, payload, 1);
            wrote_any = 1;
        }
    }

    if (!ctx->has_user_main && ctx->has_top_level_executable_statements) {
        fputs("int main(void);\n", ctx->out);
        wrote_any = 1;
    }

    if (wrote_any) {
        fputc('\n', ctx->out);
    }
}

void emit_top_level_functions(CodegenContext *ctx, const ParseNode *root)
{
    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = codegen_statement_payload(root->children[i]);

        if (payload->kind == NODE_IMPORT_STATEMENT) {
            codegen_error("imports should be resolved before C code generation");
        } else if (payload->kind == NODE_ENUM_DEF) {
            continue;
        } else if (payload->kind == NODE_NATIVE_TYPE_DEF) {
            continue;
        } else if (payload->kind == NODE_NATIVE_FUNCTION_DEF) {
            continue;
        } else if (payload->kind == NODE_CLASS_DEF) {
            const ParseNode *init_def = find_class_init_definition(payload);
            ValueType class_type = semantic_find_class_type(codegen_expect_child(payload, 0, NODE_PRIMARY)->value);

            for (size_t j = class_member_start_index(payload); j < payload->child_count; j++) {
                if (payload->children[j]->kind == NODE_FUNCTION_DEF) {
                    emit_function_definition(ctx, payload->children[j]);
                    fputc('\n', ctx->out);
                }
            }
            for (MethodInfo *method = semantic_methods(ctx->semantic); method != NULL; method = method->next) {
                if (method->owner_type == class_type && method->node == NULL) {
                    emit_synthetic_method_definition(ctx, method);
                    fputc('\n', ctx->out);
                }
            }
            if (init_def != NULL) {
                emit_constructor_helper_definition(ctx, class_type, init_def);
                fputc('\n', ctx->out);
            }
            continue;
        } else if (payload->kind == NODE_FUNCTION_DEF) {
            emit_function_definition(ctx, payload);
            fputc('\n', ctx->out);
        }
    }
}
