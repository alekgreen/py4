#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen_internal.h"

const ValueType CODEGEN_ORDERED_TYPES[] = {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_STR,
    TYPE_NONE
};

const size_t CODEGEN_ORDERED_TYPE_COUNT =
    sizeof(CODEGEN_ORDERED_TYPES) / sizeof(CODEGEN_ORDERED_TYPES[0]);

void codegen_error(const char *message, ...)
{
    va_list args;

    fprintf(stderr, "Codegen error: ");
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(1);
}

void codegen_emit_indent(CodegenContext *ctx)
{
    for (int i = 0; i < ctx->indent_level; i++) {
        fputs("    ", ctx->out);
    }
}

const ParseNode *codegen_expect_child(const ParseNode *node, size_t index, NodeKind kind)
{
    if (node == NULL || index >= node->child_count) {
        codegen_error("malformed AST");
    }
    if (node->children[index]->kind != kind) {
        codegen_error("unexpected AST node kind");
    }
    return node->children[index];
}

int codegen_is_type_assignment(const ParseNode *statement_tail)
{
    return statement_tail->child_count == 4 &&
        statement_tail->children[0]->kind == NODE_COLON;
}

int codegen_is_epsilon_node(const ParseNode *node)
{
    return node->kind == NODE_EPSILON;
}

int codegen_is_print_call_expr(const ParseNode *expr)
{
    const ParseNode *call;
    const ParseNode *callee;

    if (expr->kind != NODE_EXPRESSION || expr->child_count != 1) {
        return 0;
    }

    call = expr->children[0];
    if (call->kind != NODE_CALL || call->child_count == 0) {
        return 0;
    }

    callee = call->children[0];
    return callee->kind == NODE_PRIMARY &&
        callee->value != NULL &&
        strcmp(callee->value, "print") == 0;
}

const ParseNode *codegen_statement_payload(const ParseNode *statement)
{
    if (statement->kind != NODE_STATEMENT || statement->child_count != 1) {
        codegen_error("malformed statement node");
    }
    return statement->children[0];
}

const ParseNode *codegen_simple_statement_name(const ParseNode *simple_stmt)
{
    return codegen_expect_child(simple_stmt, 0, NODE_PRIMARY);
}

const ParseNode *codegen_simple_statement_tail(const ParseNode *simple_stmt)
{
    return codegen_expect_child(simple_stmt, 1, NODE_STATEMENT_TAIL);
}

const ParseNode *codegen_statement_tail_expression(const ParseNode *statement_tail)
{
    if (codegen_is_type_assignment(statement_tail)) {
        return codegen_expect_child(statement_tail, 3, NODE_EXPRESSION);
    }
    return codegen_expect_child(statement_tail, 1, NODE_EXPRESSION);
}

const ParseNode *codegen_statement_tail_type_node(const ParseNode *statement_tail)
{
    return codegen_expect_child(statement_tail, 1, NODE_TYPE);
}

const ParseNode *codegen_function_parameters(const ParseNode *function_def)
{
    return codegen_expect_child(function_def, 1, NODE_PARAMETERS);
}

const ParseNode *codegen_function_suite(const ParseNode *function_def)
{
    return function_def->children[function_def->child_count - 1];
}

int codegen_is_main_function(const ParseNode *function_def)
{
    const ParseNode *name = codegen_expect_child(function_def, 0, NODE_PRIMARY);

    return strcmp(name->value, "main") == 0;
}

ValueType codegen_function_return_type(CodegenContext *ctx, const ParseNode *function_def)
{
    if (function_def->child_count >= 3 && function_def->children[2]->kind == NODE_RETURN_TYPE) {
        return semantic_type_of(ctx->semantic, codegen_expect_child(function_def->children[2], 0, NODE_TYPE));
    }
    return TYPE_NONE;
}

const ParseNode *codegen_find_function_definition(const ParseNode *root, const char *name)
{
    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = codegen_statement_payload(root->children[i]);
        const ParseNode *function_name;

        if (payload->kind != NODE_FUNCTION_DEF) {
            continue;
        }

        function_name = codegen_expect_child(payload, 0, NODE_PRIMARY);
        if (strcmp(function_name->value, name) == 0) {
            return payload;
        }
    }

    return NULL;
}

const char *codegen_type_suffix(ValueType type)
{
    switch (type) {
        case TYPE_INT: return "int";
        case TYPE_FLOAT: return "float";
        case TYPE_BOOL: return "bool";
        case TYPE_CHAR: return "char";
        case TYPE_STR: return "str";
        case TYPE_NONE: return "none";
        default: return "unknown";
    }
}

const char *codegen_type_field(ValueType type)
{
    switch (type) {
        case TYPE_INT: return "as_int";
        case TYPE_FLOAT: return "as_float";
        case TYPE_BOOL: return "as_bool";
        case TYPE_CHAR: return "as_char";
        case TYPE_STR: return "as_str";
        default: return "";
    }
}

static void append_name(char *buffer, size_t size, const char *suffix)
{
    if (strlen(buffer) + strlen(suffix) + 1 >= size) {
        codegen_error("generated identifier is too long");
    }
    strcat(buffer, suffix);
}

void codegen_build_union_base_name(char *buffer, size_t size, ValueType type)
{
    strcpy(buffer, "py4_union");
    for (size_t i = 0; i < CODEGEN_ORDERED_TYPE_COUNT; i++) {
        if (semantic_type_contains(type, CODEGEN_ORDERED_TYPES[i])) {
            append_name(buffer, size, "_");
            append_name(buffer, size, codegen_type_suffix(CODEGEN_ORDERED_TYPES[i]));
        }
    }
}

void codegen_build_union_tag_name(char *buffer, size_t size, ValueType type)
{
    codegen_build_union_base_name(buffer, size, type);
    append_name(buffer, size, "_tag");
}

void codegen_build_union_enum_value_name(char *buffer, size_t size, ValueType union_type, ValueType member)
{
    codegen_build_union_base_name(buffer, size, union_type);
    append_name(buffer, size, "_tag_");
    append_name(buffer, size, codegen_type_suffix(member));
}

void codegen_build_union_ctor_name(char *buffer, size_t size, ValueType union_type, ValueType member)
{
    codegen_build_union_base_name(buffer, size, union_type);
    append_name(buffer, size, "_from_");
    append_name(buffer, size, codegen_type_suffix(member));
}

void codegen_build_union_print_name(char *buffer, size_t size, ValueType union_type)
{
    char base_name[MAX_NAME_LEN];
    const char *trimmed;

    codegen_build_union_base_name(base_name, sizeof(base_name), union_type);
    trimmed = strncmp(base_name, "py4_", 4) == 0 ? base_name + 4 : base_name;
    snprintf(buffer, size, "py4_print_%s", trimmed);
}

void codegen_build_union_convert_name(char *buffer, size_t size, ValueType from_type, ValueType to_type)
{
    char from_name[MAX_NAME_LEN];
    char to_name[MAX_NAME_LEN];

    codegen_build_union_base_name(from_name, sizeof(from_name), from_type);
    codegen_build_union_base_name(to_name, sizeof(to_name), to_type);
    snprintf(buffer, size, "py4_convert_%s_to_%s", from_name, to_name);
}

void codegen_emit_scalar_c_type(FILE *out, ValueType type)
{
    switch (type) {
        case TYPE_INT:
            fputs("int", out);
            return;
        case TYPE_FLOAT:
            fputs("double", out);
            return;
        case TYPE_BOOL:
            fputs("bool", out);
            return;
        case TYPE_CHAR:
            fputs("char", out);
            return;
        case TYPE_STR:
            fputs("const char *", out);
            return;
        case TYPE_NONE:
            fputs("void", out);
            return;
        default:
            codegen_error("unsupported scalar type %s", semantic_type_name(type));
    }
}

void codegen_emit_type_name(CodegenContext *ctx, ValueType type)
{
    char buffer[MAX_NAME_LEN];

    if (!semantic_type_is_union(type)) {
        codegen_emit_scalar_c_type(ctx->out, type);
        return;
    }

    codegen_build_union_base_name(buffer, sizeof(buffer), type);
    fputs(buffer, ctx->out);
}

void codegen_add_union_type(CodegenContext *ctx, ValueType type)
{
    if (!semantic_type_is_union(type)) {
        return;
    }

    for (size_t i = 0; i < ctx->union_type_count; i++) {
        if (ctx->union_types[i] == type) {
            return;
        }
    }

    if (ctx->union_type_count >= MAX_UNION_TYPES) {
        codegen_error("too many union types for code generation");
    }

    ctx->union_types[ctx->union_type_count++] = type;
}

ValueType codegen_resolve_union_member_type(ValueType union_type, ValueType value_type)
{
    if (semantic_type_contains(union_type, value_type)) {
        return value_type;
    }

    if (value_type == TYPE_INT && semantic_type_contains(union_type, TYPE_FLOAT)) {
        return TYPE_FLOAT;
    }

    codegen_error("cannot store %s in %s",
        semantic_type_name(value_type),
        semantic_type_name(union_type));
    return TYPE_NONE;
}

void codegen_add_union_conversion(CodegenContext *ctx, ValueType from_type, ValueType to_type)
{
    if (!semantic_type_is_union(from_type) || !semantic_type_is_union(to_type) || from_type == to_type) {
        return;
    }

    codegen_add_union_type(ctx, from_type);
    codegen_add_union_type(ctx, to_type);

    for (size_t i = 0; i < ctx->conversion_count; i++) {
        if (ctx->conversions[i].from_type == from_type &&
            ctx->conversions[i].to_type == to_type) {
            return;
        }
    }

    if (ctx->conversion_count >= MAX_UNION_CONVERSIONS) {
        codegen_error("too many union conversions for code generation");
    }

    ctx->conversions[ctx->conversion_count].from_type = from_type;
    ctx->conversions[ctx->conversion_count].to_type = to_type;
    ctx->conversion_count++;
}

void codegen_add_printable_union_type(CodegenContext *ctx, ValueType type)
{
    if (!semantic_type_is_union(type)) {
        return;
    }

    for (size_t i = 0; i < ctx->printable_union_type_count; i++) {
        if (ctx->printable_union_types[i] == type) {
            return;
        }
    }

    if (ctx->printable_union_type_count >= MAX_UNION_TYPES) {
        codegen_error("too many printable union types for code generation");
    }

    codegen_add_union_type(ctx, type);
    ctx->printable_union_types[ctx->printable_union_type_count++] = type;
}

void codegen_collect_union_types_from_node(CodegenContext *ctx, const ParseNode *node)
{
    if (node == NULL) {
        return;
    }

    if (node->kind == NODE_TYPE) {
        codegen_add_union_type(ctx, semantic_type_of(ctx->semantic, node));
    }

    for (size_t i = 0; i < node->child_count; i++) {
        codegen_collect_union_types_from_node(ctx, node->children[i]);
    }
}

void codegen_collect_required_conversions(CodegenContext *ctx, const ParseNode *node)
{
    if (node == NULL) {
        return;
    }

    if (node->kind == NODE_RETURN_STATEMENT &&
        node->child_count == 1 &&
        !codegen_is_epsilon_node(node->children[0])) {
        ValueType from_type = semantic_type_of(ctx->semantic, node->children[0]);
        if (semantic_type_is_union(ctx->current_function_return_type) &&
            semantic_type_is_union(from_type) &&
            from_type != ctx->current_function_return_type) {
            codegen_add_union_conversion(ctx, from_type, ctx->current_function_return_type);
        }
    }

    if (node->kind == NODE_SIMPLE_STATEMENT && node->child_count == 2) {
        const ParseNode *name = codegen_simple_statement_name(node);
        const ParseNode *statement_tail = codegen_simple_statement_tail(node);
        const ParseNode *expr = codegen_statement_tail_expression(statement_tail);
        ValueType expr_type = semantic_type_of(ctx->semantic, expr);
        ValueType target_type = codegen_is_type_assignment(statement_tail)
            ? semantic_type_of(ctx->semantic, codegen_statement_tail_type_node(statement_tail))
            : semantic_type_of(ctx->semantic, name);

        if (semantic_type_is_union(target_type) &&
            semantic_type_is_union(expr_type) &&
            target_type != expr_type) {
            codegen_add_union_conversion(ctx, expr_type, target_type);
        }
    }

    if (node->kind == NODE_CALL) {
        const ParseNode *callee = codegen_expect_child(node, 0, NODE_PRIMARY);
        const ParseNode *arguments = codegen_expect_child(node, 1, NODE_ARGUMENTS);
        const ParseNode *function_def;
        const ParseNode *parameters;

        if (strcmp(callee->value, "print") == 0) {
            if (arguments->child_count == 1 && !codegen_is_epsilon_node(arguments->children[0])) {
                codegen_add_printable_union_type(ctx, semantic_type_of(ctx->semantic, arguments->children[0]));
            }
        } else {
            function_def = codegen_find_function_definition(ctx->root, callee->value);
            if (function_def == NULL) {
                codegen_error("unknown function '%s' during conversion collection", callee->value);
            }

            parameters = codegen_function_parameters(function_def);
            for (size_t i = 0; i < arguments->child_count; i++) {
                ValueType expr_type;
                ValueType target_type;
                const ParseNode *parameter;

                if (codegen_is_epsilon_node(arguments->children[i])) {
                    continue;
                }

                parameter = codegen_expect_child(parameters, i, NODE_PARAMETER);
                expr_type = semantic_type_of(ctx->semantic, arguments->children[i]);
                target_type = semantic_type_of(ctx->semantic, codegen_expect_child(parameter, 0, NODE_TYPE));
                if (semantic_type_is_union(target_type) &&
                    semantic_type_is_union(expr_type) &&
                    target_type != expr_type) {
                    codegen_add_union_conversion(ctx, expr_type, target_type);
                }
            }
        }
    }

    if (node->kind == NODE_FUNCTION_DEF) {
        ValueType previous = ctx->current_function_return_type;
        ctx->current_function_return_type = codegen_function_return_type(ctx, node);
        for (size_t i = 0; i < node->child_count; i++) {
            codegen_collect_required_conversions(ctx, node->children[i]);
        }
        ctx->current_function_return_type = previous;
        return;
    }

    for (size_t i = 0; i < node->child_count; i++) {
        codegen_collect_required_conversions(ctx, node->children[i]);
    }
}

static void emit_scalar_print_case(FILE *out, ValueType type, const char *value_expr)
{
    switch (type) {
        case TYPE_INT:
            fprintf(out, "            printf(\"%%d\\n\", %s);\n", value_expr);
            return;
        case TYPE_FLOAT:
            fprintf(out, "            printf(\"%%g\\n\", %s);\n", value_expr);
            return;
        case TYPE_BOOL:
            fprintf(out, "            printf(\"%%s\\n\", %s ? \"True\" : \"False\");\n", value_expr);
            return;
        case TYPE_CHAR:
            fprintf(out, "            printf(\"%%c\\n\", %s);\n", value_expr);
            return;
        case TYPE_STR:
            fprintf(out, "            printf(\"%%s\\n\", %s);\n", value_expr);
            return;
        case TYPE_NONE:
            fputs("            printf(\"None\\n\");\n", out);
            return;
        default:
            codegen_error("unsupported print type %s", semantic_type_name(type));
    }
}

static void emit_union_definition(CodegenContext *ctx, ValueType type)
{
    char base_name[MAX_NAME_LEN];
    char tag_name[MAX_NAME_LEN];
    char enum_name[MAX_NAME_LEN];
    char ctor_name[MAX_NAME_LEN];

    codegen_build_union_base_name(base_name, sizeof(base_name), type);
    codegen_build_union_tag_name(tag_name, sizeof(tag_name), type);

    fprintf(ctx->out, "typedef enum {\n");
    for (size_t i = 0; i < CODEGEN_ORDERED_TYPE_COUNT; i++) {
        if (!semantic_type_contains(type, CODEGEN_ORDERED_TYPES[i])) {
            continue;
        }
        codegen_build_union_enum_value_name(enum_name, sizeof(enum_name), type, CODEGEN_ORDERED_TYPES[i]);
        fprintf(ctx->out, "    %s%s\n",
            enum_name,
            i + 1 < CODEGEN_ORDERED_TYPE_COUNT ? "," : "");
    }
    fprintf(ctx->out, "} %s;\n\n", tag_name);

    fprintf(ctx->out, "typedef struct {\n");
    fprintf(ctx->out, "    %s tag;\n", tag_name);
    fputs("    union {\n", ctx->out);
    for (size_t i = 0; i < CODEGEN_ORDERED_TYPE_COUNT; i++) {
        if (!semantic_type_contains(type, CODEGEN_ORDERED_TYPES[i]) || CODEGEN_ORDERED_TYPES[i] == TYPE_NONE) {
            continue;
        }
        fputs("        ", ctx->out);
        codegen_emit_scalar_c_type(ctx->out, CODEGEN_ORDERED_TYPES[i]);
        fprintf(ctx->out, " %s;\n", codegen_type_field(CODEGEN_ORDERED_TYPES[i]));
    }
    fputs("    } value;\n", ctx->out);
    fprintf(ctx->out, "} %s;\n\n", base_name);

    for (size_t i = 0; i < CODEGEN_ORDERED_TYPE_COUNT; i++) {
        ValueType member = CODEGEN_ORDERED_TYPES[i];
        if (!semantic_type_contains(type, member)) {
            continue;
        }

        codegen_build_union_ctor_name(ctor_name, sizeof(ctor_name), type, member);
        fprintf(ctx->out, "static %s %s(", base_name, ctor_name);
        if (member != TYPE_NONE) {
            codegen_emit_scalar_c_type(ctx->out, member);
            fputs(" value", ctx->out);
        } else {
            fputs("void", ctx->out);
        }
        fputs(")\n{\n", ctx->out);
        fprintf(ctx->out, "    %s result;\n", base_name);
        codegen_build_union_enum_value_name(enum_name, sizeof(enum_name), type, member);
        fprintf(ctx->out, "    result.tag = %s;\n", enum_name);
        if (member != TYPE_NONE) {
            fprintf(ctx->out, "    result.value.%s = value;\n", codegen_type_field(member));
        }
        fputs("    return result;\n}\n\n", ctx->out);
    }
}

static void emit_union_print_helper(CodegenContext *ctx, ValueType type)
{
    char base_name[MAX_NAME_LEN];
    char helper_name[MAX_NAME_LEN];
    char enum_name[MAX_NAME_LEN];

    codegen_build_union_base_name(base_name, sizeof(base_name), type);
    codegen_build_union_print_name(helper_name, sizeof(helper_name), type);

    fprintf(ctx->out, "static void %s(%s value)\n{\n", helper_name, base_name);
    fputs("    switch (value.tag) {\n", ctx->out);
    for (size_t i = 0; i < CODEGEN_ORDERED_TYPE_COUNT; i++) {
        ValueType member = CODEGEN_ORDERED_TYPES[i];

        if (!semantic_type_contains(type, member)) {
            continue;
        }

        codegen_build_union_enum_value_name(enum_name, sizeof(enum_name), type, member);
        fprintf(ctx->out, "        case %s:\n", enum_name);
        if (member == TYPE_NONE) {
            emit_scalar_print_case(ctx->out, member, NULL);
        } else {
            char value_expr[MAX_NAME_LEN];
            snprintf(value_expr, sizeof(value_expr), "value.value.%s", codegen_type_field(member));
            emit_scalar_print_case(ctx->out, member, value_expr);
        }
        fputs("            return;\n", ctx->out);
    }
    fputs("    }\n}\n\n", ctx->out);
}

static void emit_union_conversion(CodegenContext *ctx, ValueType from_type, ValueType to_type)
{
    char helper_name[MAX_NAME_LEN];
    char from_name[MAX_NAME_LEN];
    char to_name[MAX_NAME_LEN];
    char enum_name[MAX_NAME_LEN];
    char ctor_name[MAX_NAME_LEN];

    codegen_build_union_convert_name(helper_name, sizeof(helper_name), from_type, to_type);
    codegen_build_union_base_name(from_name, sizeof(from_name), from_type);
    codegen_build_union_base_name(to_name, sizeof(to_name), to_type);

    fprintf(ctx->out, "static %s %s(%s value)\n{\n", to_name, helper_name, from_name);
    fputs("    switch (value.tag) {\n", ctx->out);
    for (size_t i = 0; i < CODEGEN_ORDERED_TYPE_COUNT; i++) {
        ValueType member = CODEGEN_ORDERED_TYPES[i];
        ValueType target_member;

        if (!semantic_type_contains(from_type, member)) {
            continue;
        }

        codegen_build_union_enum_value_name(enum_name, sizeof(enum_name), from_type, member);
        fprintf(ctx->out, "        case %s:\n", enum_name);
        target_member = codegen_resolve_union_member_type(to_type, member);
        codegen_build_union_ctor_name(ctor_name, sizeof(ctor_name), to_type, target_member);
        if (member == TYPE_NONE) {
            fprintf(ctx->out, "            return %s();\n", ctor_name);
        } else {
            fprintf(ctx->out, "            return %s(value.value.%s);\n", ctor_name, codegen_type_field(member));
        }
    }
    fputs("    }\n", ctx->out);
    fputs("    return ", ctx->out);
    for (size_t i = 0; i < CODEGEN_ORDERED_TYPE_COUNT; i++) {
        if (!semantic_type_contains(to_type, CODEGEN_ORDERED_TYPES[i])) {
            continue;
        }
        codegen_build_union_ctor_name(ctor_name, sizeof(ctor_name), to_type, CODEGEN_ORDERED_TYPES[i]);
        if (CODEGEN_ORDERED_TYPES[i] == TYPE_NONE) {
            fprintf(ctx->out, "%s();\n}\n\n", ctor_name);
        } else if (CODEGEN_ORDERED_TYPES[i] == TYPE_STR) {
            fprintf(ctx->out, "%s(\"\");\n}\n\n", ctor_name);
        } else {
            fprintf(ctx->out, "%s(0);\n}\n\n", ctor_name);
        }
        return;
    }
    codegen_error("cannot generate conversion fallback for %s", semantic_type_name(to_type));
}

void codegen_emit_union_runtime(CodegenContext *ctx)
{
    for (size_t i = 0; i < ctx->union_type_count; i++) {
        emit_union_definition(ctx, ctx->union_types[i]);
    }

    for (size_t i = 0; i < ctx->printable_union_type_count; i++) {
        emit_union_print_helper(ctx, ctx->printable_union_types[i]);
    }

    for (size_t i = 0; i < ctx->conversion_count; i++) {
        emit_union_conversion(ctx,
            ctx->conversions[i].from_type,
            ctx->conversions[i].to_type);
    }
}

void codegen_collect_program_state(CodegenContext *ctx, const ParseNode *root)
{
    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = codegen_statement_payload(root->children[i]);

        if (payload->kind == NODE_FUNCTION_DEF) {
            if (codegen_is_main_function(payload)) {
                ctx->has_user_main = 1;
            }
        } else {
            ctx->has_top_level_executable_statements = 1;
        }
    }

    codegen_collect_union_types_from_node(ctx, root);
    codegen_collect_required_conversions(ctx, root);
}
