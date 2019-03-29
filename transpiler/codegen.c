#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen.h"

#define MAX_UNION_TYPES 64
#define MAX_UNION_CONVERSIONS 128
#define MAX_NAME_LEN 128

typedef struct {
    ValueType from_type;
    ValueType to_type;
} UnionConversion;

typedef struct {
    FILE *out;
    const ParseNode *root;
    const SemanticInfo *semantic;
    int indent_level;
    int has_user_main;
    int has_top_level_executable_statements;
    int current_function_is_main;
    ValueType current_function_return_type;
    ValueType union_types[MAX_UNION_TYPES];
    size_t union_type_count;
    ValueType printable_union_types[MAX_UNION_TYPES];
    size_t printable_union_type_count;
    UnionConversion conversions[MAX_UNION_CONVERSIONS];
    size_t conversion_count;
} CodegenContext;

static const ValueType ORDERED_TYPES[] = {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_STR,
    TYPE_NONE
};

static void codegen_error(const char *message, ...)
{
    va_list args;

    fprintf(stderr, "Codegen error: ");
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(1);
}

static void emit_indent(CodegenContext *ctx)
{
    for (int i = 0; i < ctx->indent_level; i++) {
        fputs("    ", ctx->out);
    }
}

static const ParseNode *expect_child(const ParseNode *node, size_t index, NodeKind kind)
{
    if (node == NULL || index >= node->child_count) {
        codegen_error("malformed AST");
    }
    if (node->children[index]->kind != kind) {
        codegen_error("unexpected AST node kind");
    }
    return node->children[index];
}

static int is_type_assignment(const ParseNode *statement_tail)
{
    return statement_tail->child_count == 4 &&
        statement_tail->children[0]->kind == NODE_COLON;
}

static int is_epsilon_node(const ParseNode *node)
{
    return node->kind == NODE_EPSILON;
}

static int is_print_call_expr(const ParseNode *expr)
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

static const ParseNode *statement_payload(const ParseNode *statement)
{
    if (statement->kind != NODE_STATEMENT || statement->child_count != 1) {
        codegen_error("malformed statement node");
    }
    return statement->children[0];
}

static const ParseNode *simple_statement_name(const ParseNode *simple_stmt)
{
    return expect_child(simple_stmt, 0, NODE_PRIMARY);
}

static const ParseNode *simple_statement_tail(const ParseNode *simple_stmt)
{
    return expect_child(simple_stmt, 1, NODE_STATEMENT_TAIL);
}

static const ParseNode *statement_tail_expression(const ParseNode *statement_tail)
{
    if (is_type_assignment(statement_tail)) {
        return expect_child(statement_tail, 3, NODE_EXPRESSION);
    }
    return expect_child(statement_tail, 1, NODE_EXPRESSION);
}

static const ParseNode *statement_tail_type_node(const ParseNode *statement_tail)
{
    return expect_child(statement_tail, 1, NODE_TYPE);
}

static const ParseNode *function_parameters(const ParseNode *function_def)
{
    return expect_child(function_def, 1, NODE_PARAMETERS);
}

static const ParseNode *function_suite(const ParseNode *function_def)
{
    return function_def->children[function_def->child_count - 1];
}

static int is_main_function(const ParseNode *function_def)
{
    const ParseNode *name = expect_child(function_def, 0, NODE_PRIMARY);

    return strcmp(name->value, "main") == 0;
}

static ValueType function_return_type(CodegenContext *ctx, const ParseNode *function_def)
{
    if (function_def->child_count >= 3 && function_def->children[2]->kind == NODE_RETURN_TYPE) {
        return semantic_type_of(ctx->semantic, expect_child(function_def->children[2], 0, NODE_TYPE));
    }
    return TYPE_NONE;
}

static const ParseNode *find_function_definition(const ParseNode *root, const char *name)
{
    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = statement_payload(root->children[i]);
        const ParseNode *function_name;

        if (payload->kind != NODE_FUNCTION_DEF) {
            continue;
        }

        function_name = expect_child(payload, 0, NODE_PRIMARY);
        if (strcmp(function_name->value, name) == 0) {
            return payload;
        }
    }

    return NULL;
}

static const char *type_suffix(ValueType type)
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

static const char *type_field(ValueType type)
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

static void build_union_base_name(char *buffer, size_t size, ValueType type)
{
    strcpy(buffer, "py4_union");
    for (size_t i = 0; i < sizeof(ORDERED_TYPES) / sizeof(ORDERED_TYPES[0]); i++) {
        if (semantic_type_contains(type, ORDERED_TYPES[i])) {
            append_name(buffer, size, "_");
            append_name(buffer, size, type_suffix(ORDERED_TYPES[i]));
        }
    }
}

static void build_union_tag_name(char *buffer, size_t size, ValueType type)
{
    build_union_base_name(buffer, size, type);
    append_name(buffer, size, "_tag");
}

static void build_union_enum_value_name(char *buffer, size_t size, ValueType union_type, ValueType member)
{
    build_union_base_name(buffer, size, union_type);
    append_name(buffer, size, "_tag_");
    append_name(buffer, size, type_suffix(member));
}

static void build_union_ctor_name(char *buffer, size_t size, ValueType union_type, ValueType member)
{
    build_union_base_name(buffer, size, union_type);
    append_name(buffer, size, "_from_");
    append_name(buffer, size, type_suffix(member));
}

static void build_union_print_name(char *buffer, size_t size, ValueType union_type)
{
    char base_name[MAX_NAME_LEN];
    const char *trimmed;

    build_union_base_name(base_name, sizeof(base_name), union_type);
    trimmed = strncmp(base_name, "py4_", 4) == 0 ? base_name + 4 : base_name;
    snprintf(buffer, size, "py4_print_%s", trimmed);
}

static void build_union_convert_name(char *buffer, size_t size, ValueType from_type, ValueType to_type)
{
    char from_name[MAX_NAME_LEN];
    char to_name[MAX_NAME_LEN];

    build_union_base_name(from_name, sizeof(from_name), from_type);
    build_union_base_name(to_name, sizeof(to_name), to_type);
    snprintf(buffer, size, "py4_convert_%s_to_%s", from_name, to_name);
}

static void emit_scalar_c_type(FILE *out, ValueType type)
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

static void emit_type_name(CodegenContext *ctx, ValueType type)
{
    char buffer[MAX_NAME_LEN];

    if (!semantic_type_is_union(type)) {
        emit_scalar_c_type(ctx->out, type);
        return;
    }

    build_union_base_name(buffer, sizeof(buffer), type);
    fputs(buffer, ctx->out);
}

static void add_union_type(CodegenContext *ctx, ValueType type)
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

static ValueType resolve_union_member_type(ValueType union_type, ValueType value_type)
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

static void add_union_conversion(CodegenContext *ctx, ValueType from_type, ValueType to_type)
{
    if (!semantic_type_is_union(from_type) || !semantic_type_is_union(to_type) || from_type == to_type) {
        return;
    }

    add_union_type(ctx, from_type);
    add_union_type(ctx, to_type);

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

static void add_printable_union_type(CodegenContext *ctx, ValueType type)
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

    add_union_type(ctx, type);
    ctx->printable_union_types[ctx->printable_union_type_count++] = type;
}

static void collect_union_types_from_node(CodegenContext *ctx, const ParseNode *node)
{
    if (node == NULL) {
        return;
    }

    if (node->kind == NODE_TYPE) {
        add_union_type(ctx, semantic_type_of(ctx->semantic, node));
    }

    for (size_t i = 0; i < node->child_count; i++) {
        collect_union_types_from_node(ctx, node->children[i]);
    }
}

static void collect_required_conversions(CodegenContext *ctx, const ParseNode *node)
{
    if (node == NULL) {
        return;
    }

    if (node->kind == NODE_RETURN_STATEMENT &&
        node->child_count == 1 &&
        !is_epsilon_node(node->children[0])) {
        ValueType from_type = semantic_type_of(ctx->semantic, node->children[0]);
        if (semantic_type_is_union(ctx->current_function_return_type) &&
            semantic_type_is_union(from_type) &&
            from_type != ctx->current_function_return_type) {
            add_union_conversion(ctx, from_type, ctx->current_function_return_type);
        }
    }

    if (node->kind == NODE_SIMPLE_STATEMENT && node->child_count == 2) {
        const ParseNode *name = simple_statement_name(node);
        const ParseNode *statement_tail = simple_statement_tail(node);
        const ParseNode *expr = statement_tail_expression(statement_tail);
        ValueType expr_type = semantic_type_of(ctx->semantic, expr);
        ValueType target_type = is_type_assignment(statement_tail)
            ? semantic_type_of(ctx->semantic, statement_tail_type_node(statement_tail))
            : semantic_type_of(ctx->semantic, name);

        if (semantic_type_is_union(target_type) &&
            semantic_type_is_union(expr_type) &&
            target_type != expr_type) {
            add_union_conversion(ctx, expr_type, target_type);
        }
    }

    if (node->kind == NODE_CALL) {
        const ParseNode *callee = expect_child(node, 0, NODE_PRIMARY);
        const ParseNode *arguments = expect_child(node, 1, NODE_ARGUMENTS);
        const ParseNode *function_def;
        const ParseNode *parameters;

        if (strcmp(callee->value, "print") == 0) {
            if (arguments->child_count == 1 && !is_epsilon_node(arguments->children[0])) {
                add_printable_union_type(ctx, semantic_type_of(ctx->semantic, arguments->children[0]));
            }
        } else {
            function_def = find_function_definition(ctx->root, callee->value);
            if (function_def == NULL) {
                codegen_error("unknown function '%s' during conversion collection", callee->value);
            }

            parameters = function_parameters(function_def);
            for (size_t i = 0; i < arguments->child_count; i++) {
                ValueType expr_type;
                ValueType target_type;
                const ParseNode *parameter;

                if (is_epsilon_node(arguments->children[i])) {
                    continue;
                }

                parameter = expect_child(parameters, i, NODE_PARAMETER);
                expr_type = semantic_type_of(ctx->semantic, arguments->children[i]);
                target_type = semantic_type_of(ctx->semantic, expect_child(parameter, 0, NODE_TYPE));
                if (semantic_type_is_union(target_type) &&
                    semantic_type_is_union(expr_type) &&
                    target_type != expr_type) {
                    add_union_conversion(ctx, expr_type, target_type);
                }
            }
        }
    }

    if (node->kind == NODE_FUNCTION_DEF) {
        ValueType previous = ctx->current_function_return_type;
        ctx->current_function_return_type = function_return_type(ctx, node);
        for (size_t i = 0; i < node->child_count; i++) {
            collect_required_conversions(ctx, node->children[i]);
        }
        ctx->current_function_return_type = previous;
        return;
    }

    for (size_t i = 0; i < node->child_count; i++) {
        collect_required_conversions(ctx, node->children[i]);
    }
}

static void emit_union_constructor_call(CodegenContext *ctx, ValueType union_type, ValueType stored_type)
{
    char ctor_name[MAX_NAME_LEN];

    build_union_ctor_name(ctor_name, sizeof(ctor_name), union_type, stored_type);
    fputs(ctor_name, ctx->out);
}

static void emit_expression(CodegenContext *ctx, const ParseNode *expr);

static void emit_wrapped_expression(CodegenContext *ctx, const ParseNode *expr, ValueType target_type)
{
    ValueType expr_type = semantic_type_of(ctx->semantic, expr);
    char helper_name[MAX_NAME_LEN];

    if (semantic_type_is_union(target_type)) {
        if (semantic_type_is_union(expr_type)) {
            if (expr_type == target_type) {
                emit_expression(ctx, expr);
                return;
            }

            build_union_convert_name(helper_name, sizeof(helper_name), expr_type, target_type);
            fputs(helper_name, ctx->out);
            fputc('(', ctx->out);
            emit_expression(ctx, expr);
            fputc(')', ctx->out);
            return;
        }

        emit_union_constructor_call(ctx, target_type, resolve_union_member_type(target_type, expr_type));
        if (expr_type == TYPE_NONE) {
            fputs("()", ctx->out);
            return;
        }

        fputc('(', ctx->out);
        emit_expression(ctx, expr);
        fputc(')', ctx->out);
        return;
    }

    emit_expression(ctx, expr);
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

    build_union_base_name(base_name, sizeof(base_name), type);
    build_union_tag_name(tag_name, sizeof(tag_name), type);

    fprintf(ctx->out, "typedef enum {\n");
    for (size_t i = 0; i < sizeof(ORDERED_TYPES) / sizeof(ORDERED_TYPES[0]); i++) {
        if (!semantic_type_contains(type, ORDERED_TYPES[i])) {
            continue;
        }
        build_union_enum_value_name(enum_name, sizeof(enum_name), type, ORDERED_TYPES[i]);
        fprintf(ctx->out, "    %s%s\n",
            enum_name,
            i + 1 < sizeof(ORDERED_TYPES) / sizeof(ORDERED_TYPES[0]) ? "," : "");
    }
    fprintf(ctx->out, "} %s;\n\n", tag_name);

    fprintf(ctx->out, "typedef struct {\n");
    fprintf(ctx->out, "    %s tag;\n", tag_name);
    fputs("    union {\n", ctx->out);
    for (size_t i = 0; i < sizeof(ORDERED_TYPES) / sizeof(ORDERED_TYPES[0]); i++) {
        if (!semantic_type_contains(type, ORDERED_TYPES[i]) || ORDERED_TYPES[i] == TYPE_NONE) {
            continue;
        }
        fputs("        ", ctx->out);
        emit_scalar_c_type(ctx->out, ORDERED_TYPES[i]);
        fprintf(ctx->out, " %s;\n", type_field(ORDERED_TYPES[i]));
    }
    fputs("    } value;\n", ctx->out);
    fprintf(ctx->out, "} %s;\n\n", base_name);

    for (size_t i = 0; i < sizeof(ORDERED_TYPES) / sizeof(ORDERED_TYPES[0]); i++) {
        ValueType member = ORDERED_TYPES[i];
        if (!semantic_type_contains(type, member)) {
            continue;
        }

        build_union_ctor_name(ctor_name, sizeof(ctor_name), type, member);
        fprintf(ctx->out, "static %s %s(", base_name, ctor_name);
        if (member != TYPE_NONE) {
            emit_scalar_c_type(ctx->out, member);
            fputs(" value", ctx->out);
        } else {
            fputs("void", ctx->out);
        }
        fputs(")\n{\n", ctx->out);
        fprintf(ctx->out, "    %s result;\n", base_name);
        build_union_enum_value_name(enum_name, sizeof(enum_name), type, member);
        fprintf(ctx->out, "    result.tag = %s;\n", enum_name);
        if (member != TYPE_NONE) {
            fprintf(ctx->out, "    result.value.%s = value;\n", type_field(member));
        }
        fputs("    return result;\n}\n\n", ctx->out);
    }

}

static void emit_union_print_helper(CodegenContext *ctx, ValueType type)
{
    char base_name[MAX_NAME_LEN];
    char helper_name[MAX_NAME_LEN];
    char enum_name[MAX_NAME_LEN];

    build_union_base_name(base_name, sizeof(base_name), type);
    build_union_print_name(helper_name, sizeof(helper_name), type);

    fprintf(ctx->out, "static void %s(%s value)\n{\n", helper_name, base_name);
    fputs("    switch (value.tag) {\n", ctx->out);
    for (size_t i = 0; i < sizeof(ORDERED_TYPES) / sizeof(ORDERED_TYPES[0]); i++) {
        ValueType member = ORDERED_TYPES[i];

        if (!semantic_type_contains(type, member)) {
            continue;
        }

        build_union_enum_value_name(enum_name, sizeof(enum_name), type, member);
        fprintf(ctx->out, "        case %s:\n", enum_name);
        if (member == TYPE_NONE) {
            emit_scalar_print_case(ctx->out, member, NULL);
        } else {
            char value_expr[MAX_NAME_LEN];
            snprintf(value_expr, sizeof(value_expr), "value.value.%s", type_field(member));
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

    build_union_convert_name(helper_name, sizeof(helper_name), from_type, to_type);
    build_union_base_name(from_name, sizeof(from_name), from_type);
    build_union_base_name(to_name, sizeof(to_name), to_type);

    fprintf(ctx->out, "static %s %s(%s value)\n{\n", to_name, helper_name, from_name);
    fputs("    switch (value.tag) {\n", ctx->out);
    for (size_t i = 0; i < sizeof(ORDERED_TYPES) / sizeof(ORDERED_TYPES[0]); i++) {
        ValueType member = ORDERED_TYPES[i];
        ValueType target_member;

        if (!semantic_type_contains(from_type, member)) {
            continue;
        }

        build_union_enum_value_name(enum_name, sizeof(enum_name), from_type, member);
        fprintf(ctx->out, "        case %s:\n", enum_name);
        target_member = resolve_union_member_type(to_type, member);
        build_union_ctor_name(ctor_name, sizeof(ctor_name), to_type, target_member);
        if (member == TYPE_NONE) {
            fprintf(ctx->out, "            return %s();\n", ctor_name);
        } else {
            fprintf(ctx->out, "            return %s(value.value.%s);\n", ctor_name, type_field(member));
        }
    }
    fputs("    }\n", ctx->out);
    fputs("    return ", ctx->out);
    for (size_t i = 0; i < sizeof(ORDERED_TYPES) / sizeof(ORDERED_TYPES[0]); i++) {
        if (!semantic_type_contains(to_type, ORDERED_TYPES[i])) {
            continue;
        }
        build_union_ctor_name(ctor_name, sizeof(ctor_name), to_type, ORDERED_TYPES[i]);
        if (ORDERED_TYPES[i] == TYPE_NONE) {
            fprintf(ctx->out, "%s();\n}\n\n", ctor_name);
        } else if (ORDERED_TYPES[i] == TYPE_STR) {
            fprintf(ctx->out, "%s(\"\");\n}\n\n", ctor_name);
        } else {
            fprintf(ctx->out, "%s(0);\n}\n\n", ctor_name);
        }
        return;
    }
    codegen_error("cannot generate conversion fallback for %s", semantic_type_name(to_type));
}

static void emit_union_runtime(CodegenContext *ctx)
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

static void emit_arguments(CodegenContext *ctx, const ParseNode *call, const ParseNode *arguments)
{
    int first = 1;
    const ParseNode *callee = expect_child(call, 0, NODE_PRIMARY);
    const ParseNode *function_def = find_function_definition(ctx->root, callee->value);
    const ParseNode *parameters;

    if (arguments->child_count == 1 && is_epsilon_node(arguments->children[0])) {
        return;
    }

    if (function_def == NULL) {
        codegen_error("unknown function '%s' during code generation", callee->value);
    }

    parameters = function_parameters(function_def);
    for (size_t i = 0; i < arguments->child_count; i++) {
        const ParseNode *parameter = expect_child(parameters, i, NODE_PARAMETER);
        const ParseNode *type_node = expect_child(parameter, 0, NODE_TYPE);
        ValueType target_type = semantic_type_of(ctx->semantic, type_node);

        if (!first) {
            fputs(", ", ctx->out);
        }
        emit_wrapped_expression(ctx, arguments->children[i], target_type);
        first = 0;
    }
}

static void emit_call(CodegenContext *ctx, const ParseNode *call)
{
    const ParseNode *callee = expect_child(call, 0, NODE_PRIMARY);
    const ParseNode *arguments = expect_child(call, 1, NODE_ARGUMENTS);

    fputs(callee->value, ctx->out);
    fputc('(', ctx->out);
    emit_arguments(ctx, call, arguments);
    fputc(')', ctx->out);
}

static void emit_primary(CodegenContext *ctx, const ParseNode *primary)
{
    if (primary->kind == NODE_CALL) {
        emit_call(ctx, primary);
        return;
    }

    if (primary->kind == NODE_EXPRESSION) {
        emit_expression(ctx, primary);
        return;
    }

    if (primary->kind != NODE_PRIMARY || primary->value == NULL) {
        codegen_error("unsupported primary node");
    }

    if (primary->token_type == TOKEN_KEYWORD) {
        if (strcmp(primary->value, "True") == 0) {
            fputs("true", ctx->out);
            return;
        }
        if (strcmp(primary->value, "False") == 0) {
            fputs("false", ctx->out);
            return;
        }
    }

    fputs(primary->value, ctx->out);
}

static void emit_expression(CodegenContext *ctx, const ParseNode *expr)
{
    if (expr->kind != NODE_EXPRESSION || expr->child_count == 0) {
        codegen_error("malformed expression node");
    }

    if (expr->child_count == 1) {
        emit_primary(ctx, expr->children[0]);
        return;
    }

    fputc('(', ctx->out);
    for (size_t i = 0; i < expr->child_count; i++) {
        const ParseNode *child = expr->children[i];

        if (child->kind == NODE_OPERATOR) {
            fprintf(ctx->out, " %s ", child->value);
        } else {
            emit_primary(ctx, child);
        }
    }
    fputc(')', ctx->out);
}

static void emit_print_statement(CodegenContext *ctx, const ParseNode *expr)
{
    const ParseNode *call = expr->children[0];
    const ParseNode *arguments = expect_child(call, 1, NODE_ARGUMENTS);
    ValueType arg_type;
    char helper_name[MAX_NAME_LEN];

    emit_indent(ctx);
    if (arguments->child_count == 1 && is_epsilon_node(arguments->children[0])) {
        fputs("printf(\"\\n\");\n", ctx->out);
        return;
    }

    if (arguments->child_count != 1) {
        codegen_error("print currently supports exactly one argument");
    }

    arg_type = semantic_type_of(ctx->semantic, arguments->children[0]);
    if (semantic_type_is_union(arg_type)) {
        build_union_print_name(helper_name, sizeof(helper_name), arg_type);
        fprintf(ctx->out, "%s(", helper_name);
        emit_expression(ctx, arguments->children[0]);
        fputs(");\n", ctx->out);
        return;
    }

    switch (arg_type) {
        case TYPE_INT:
            fputs("printf(\"%d\\n\", ", ctx->out);
            emit_expression(ctx, arguments->children[0]);
            fputs(");\n", ctx->out);
            return;
        case TYPE_FLOAT:
            fputs("printf(\"%g\\n\", (double)(", ctx->out);
            emit_expression(ctx, arguments->children[0]);
            fputc(')', ctx->out);
            fputs(");\n", ctx->out);
            return;
        case TYPE_BOOL:
            fputs("printf(\"%s\\n\", (", ctx->out);
            emit_expression(ctx, arguments->children[0]);
            fputs(") ? \"True\" : \"False\");\n", ctx->out);
            return;
        case TYPE_CHAR:
            fputs("printf(\"%c\\n\", ", ctx->out);
            emit_expression(ctx, arguments->children[0]);
            fputs(");\n", ctx->out);
            return;
        case TYPE_STR:
            fputs("printf(\"%s\\n\", ", ctx->out);
            emit_expression(ctx, arguments->children[0]);
            fputs(");\n", ctx->out);
            return;
        case TYPE_NONE:
            codegen_error("cannot print None");
    }
}

static void emit_expression_statement(CodegenContext *ctx, const ParseNode *expr_stmt)
{
    const ParseNode *expr = expect_child(expr_stmt, 0, NODE_EXPRESSION);

    if (is_print_call_expr(expr)) {
        emit_print_statement(ctx, expr);
        return;
    }

    emit_indent(ctx);
    emit_expression(ctx, expr);
    fputs(";\n", ctx->out);
}

static void emit_return_statement(CodegenContext *ctx, const ParseNode *return_stmt)
{
    emit_indent(ctx);

    if (return_stmt->child_count != 1) {
        codegen_error("malformed return statement");
    }

    if (is_epsilon_node(return_stmt->children[0])) {
        if (ctx->current_function_is_main) {
            fputs("return 0;\n", ctx->out);
        } else if (semantic_type_is_union(ctx->current_function_return_type)) {
            fputs("return ", ctx->out);
            emit_union_constructor_call(ctx, ctx->current_function_return_type, TYPE_NONE);
            fputs("();\n", ctx->out);
        } else {
            fputs("return;\n", ctx->out);
        }
        return;
    }

    fputs("return ", ctx->out);
    emit_wrapped_expression(ctx, return_stmt->children[0], ctx->current_function_return_type);
    fputs(";\n", ctx->out);
}

static void emit_local_simple_statement(CodegenContext *ctx, const ParseNode *simple_stmt)
{
    const ParseNode *first_child = simple_stmt->children[0];

    if (first_child->kind == NODE_RETURN_STATEMENT) {
        emit_return_statement(ctx, first_child);
        return;
    }

    if (first_child->kind == NODE_EXPRESSION_STATEMENT) {
        emit_expression_statement(ctx, first_child);
        return;
    }

    if (simple_stmt->child_count != 2) {
        codegen_error("malformed simple statement");
    }

    const ParseNode *name = simple_statement_name(simple_stmt);
    const ParseNode *statement_tail = simple_statement_tail(simple_stmt);
    const ParseNode *expr = statement_tail_expression(statement_tail);
    ValueType target_type;

    emit_indent(ctx);
    if (is_type_assignment(statement_tail)) {
        target_type = semantic_type_of(ctx->semantic, statement_tail_type_node(statement_tail));
        emit_type_name(ctx, target_type);
        fprintf(ctx->out, " %s = ", name->value);
    } else {
        target_type = semantic_type_of(ctx->semantic, name);
        fprintf(ctx->out, "%s = ", name->value);
    }

    emit_wrapped_expression(ctx, expr, target_type);
    fputs(";\n", ctx->out);
}

static void emit_statement(CodegenContext *ctx, const ParseNode *statement, int allow_function_defs);
static void emit_suite(CodegenContext *ctx, const ParseNode *suite);

static void emit_if_statement(CodegenContext *ctx, const ParseNode *if_stmt)
{
    emit_indent(ctx);
    fputs("if (", ctx->out);
    emit_expression(ctx, if_stmt->children[0]);
    fputs(")\n", ctx->out);
    emit_indent(ctx);
    fputs("{\n", ctx->out);
    ctx->indent_level++;
    emit_suite(ctx, if_stmt->children[2]);
    ctx->indent_level--;
    emit_indent(ctx);
    fputs("}", ctx->out);

    for (size_t i = 3; i < if_stmt->child_count; i++) {
        const ParseNode *branch = if_stmt->children[i];

        if (branch->kind == NODE_ELIF_CLAUSE) {
            fputs(" else if (", ctx->out);
            emit_expression(ctx, branch->children[0]);
            fputs(")\n", ctx->out);
            emit_indent(ctx);
            fputs("{\n", ctx->out);
            ctx->indent_level++;
            emit_suite(ctx, branch->children[2]);
            ctx->indent_level--;
            emit_indent(ctx);
            fputs("}", ctx->out);
            continue;
        }

        if (branch->kind == NODE_ELSE_CLAUSE) {
            fputs(" else\n", ctx->out);
            emit_indent(ctx);
            fputs("{\n", ctx->out);
            ctx->indent_level++;
            emit_suite(ctx, branch->children[1]);
            ctx->indent_level--;
            emit_indent(ctx);
            fputs("}", ctx->out);
            continue;
        }

        codegen_error("malformed if statement");
    }

    fputc('\n', ctx->out);
}

static void emit_suite(CodegenContext *ctx, const ParseNode *suite)
{
    if (suite->child_count == 1 && is_epsilon_node(suite->children[0])) {
        emit_indent(ctx);
        fputs("/* empty */\n", ctx->out);
        return;
    }

    for (size_t i = 0; i < suite->child_count; i++) {
        emit_statement(ctx, suite->children[i], 0);
    }
}

static void emit_parameter_list(CodegenContext *ctx, const ParseNode *parameters, int is_c_main)
{
    if (is_c_main) {
        fputs("void", ctx->out);
        return;
    }

    if (parameters->child_count == 1 && is_epsilon_node(parameters->children[0])) {
        fputs("void", ctx->out);
        return;
    }

    for (size_t i = 0; i < parameters->child_count; i++) {
        const ParseNode *param = parameters->children[i];
        const ParseNode *type_node = expect_child(param, 0, NODE_TYPE);

        if (i > 0) {
            fputs(", ", ctx->out);
        }
        emit_type_name(ctx, semantic_type_of(ctx->semantic, type_node));
        fprintf(ctx->out, " %s", param->value);
    }
}

static void emit_function_signature(CodegenContext *ctx, const ParseNode *function_def, int prototype_only)
{
    const ParseNode *name = expect_child(function_def, 0, NODE_PRIMARY);
    const ParseNode *parameters = function_parameters(function_def);
    int is_c_main = is_main_function(function_def);
    ValueType return_type = function_return_type(ctx, function_def);

    if (is_c_main) {
        fputs("int main(", ctx->out);
    } else {
        emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", name->value);
    }
    emit_parameter_list(ctx, parameters, is_c_main);
    fputc(')', ctx->out);
    if (prototype_only) {
        fputs(";\n", ctx->out);
    }
}

static void emit_function_definition(CodegenContext *ctx, const ParseNode *function_def)
{
    int is_c_main = is_main_function(function_def);
    ValueType return_type = function_return_type(ctx, function_def);
    int prev_is_main = ctx->current_function_is_main;
    ValueType prev_return_type = ctx->current_function_return_type;

    emit_function_signature(ctx, function_def, 0);
    fputs("\n{\n", ctx->out);
    ctx->indent_level++;
    ctx->current_function_is_main = is_c_main;
    ctx->current_function_return_type = return_type;

    if (is_c_main && ctx->has_top_level_executable_statements) {
        emit_indent(ctx);
        fputs("py4_module_init();\n", ctx->out);
    }

    emit_suite(ctx, function_suite(function_def));

    if (is_c_main) {
        emit_indent(ctx);
        fputs("return 0;\n", ctx->out);
    }

    ctx->current_function_is_main = prev_is_main;
    ctx->current_function_return_type = prev_return_type;
    ctx->indent_level--;
    fputs("}\n", ctx->out);
}

static void emit_statement(CodegenContext *ctx, const ParseNode *statement, int allow_function_defs)
{
    const ParseNode *payload = statement_payload(statement);

    if (payload->kind == NODE_FUNCTION_DEF) {
        if (!allow_function_defs) {
            codegen_error("nested function definitions are not supported in C output");
        }
        emit_function_definition(ctx, payload);
        return;
    }

    if (payload->kind == NODE_IF_STATEMENT) {
        emit_if_statement(ctx, payload);
        return;
    }

    if (payload->kind != NODE_SIMPLE_STATEMENT) {
        codegen_error("unsupported statement node");
    }

    emit_local_simple_statement(ctx, payload);
}

static void collect_program_state(CodegenContext *ctx, const ParseNode *root)
{
    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = statement_payload(root->children[i]);

        if (payload->kind == NODE_FUNCTION_DEF) {
            if (is_main_function(payload)) {
                ctx->has_user_main = 1;
            }
        } else {
            ctx->has_top_level_executable_statements = 1;
        }
    }

    collect_union_types_from_node(ctx, root);
    collect_required_conversions(ctx, root);
}

static void emit_global_declaration(CodegenContext *ctx, const ParseNode *simple_stmt)
{
    const ParseNode *statement_tail = simple_statement_tail(simple_stmt);
    ValueType type;

    if (!is_type_assignment(statement_tail)) {
        return;
    }

    const ParseNode *name = simple_statement_name(simple_stmt);
    type = semantic_type_of(ctx->semantic, statement_tail_type_node(statement_tail));
    emit_type_name(ctx, type);
    fprintf(ctx->out, " %s;\n", name->value);
}

static void emit_global_declarations(CodegenContext *ctx, const ParseNode *root)
{
    int wrote_any = 0;

    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = statement_payload(root->children[i]);

        if (payload->kind == NODE_SIMPLE_STATEMENT) {
            if (payload->child_count == 2 &&
                payload->children[0]->kind == NODE_PRIMARY &&
                is_type_assignment(payload->children[1])) {
                emit_global_declaration(ctx, payload);
                wrote_any = 1;
            }
        }
    }

    if (wrote_any) {
        fputc('\n', ctx->out);
    }
}

static void emit_function_prototypes(CodegenContext *ctx, const ParseNode *root)
{
    int wrote_any = 0;

    if (ctx->has_top_level_executable_statements) {
        fputs("static void py4_module_init(void);\n", ctx->out);
        wrote_any = 1;
    }

    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = statement_payload(root->children[i]);

        if (payload->kind == NODE_FUNCTION_DEF) {
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

static void emit_module_init(CodegenContext *ctx, const ParseNode *root)
{
    if (!ctx->has_top_level_executable_statements) {
        return;
    }

    fputs("static void py4_module_init(void)\n{\n", ctx->out);
    ctx->indent_level++;
    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = statement_payload(root->children[i]);

        if (payload->kind == NODE_SIMPLE_STATEMENT) {
            const ParseNode *name;
            const ParseNode *statement_tail;
            const ParseNode *expr;
            ValueType target_type;

            if (payload->children[0]->kind == NODE_EXPRESSION_STATEMENT) {
                emit_expression_statement(ctx, payload->children[0]);
                continue;
            }

            if (payload->children[0]->kind == NODE_RETURN_STATEMENT) {
                codegen_error("return is not valid at module scope");
            }

            name = simple_statement_name(payload);
            statement_tail = simple_statement_tail(payload);
            expr = statement_tail_expression(statement_tail);
            target_type = semantic_type_of(ctx->semantic, name);

            emit_indent(ctx);
            fprintf(ctx->out, "%s = ", name->value);
            emit_wrapped_expression(ctx, expr, target_type);
            fputs(";\n", ctx->out);
            continue;
        }

        if (payload->kind == NODE_IF_STATEMENT) {
            emit_if_statement(ctx, payload);
        }
    }
    ctx->indent_level--;
    fputs("}\n\n", ctx->out);
}

static void emit_top_level_functions(CodegenContext *ctx, const ParseNode *root)
{
    for (size_t i = 0; i < root->child_count; i++) {
        const ParseNode *payload = statement_payload(root->children[i]);

        if (payload->kind == NODE_FUNCTION_DEF) {
            emit_function_definition(ctx, payload);
            fputc('\n', ctx->out);
        }
    }
}

static void emit_auto_main(CodegenContext *ctx)
{
    if (ctx->has_user_main) {
        return;
    }

    fputs("int main(void)\n{\n", ctx->out);
    ctx->indent_level++;
    if (ctx->has_top_level_executable_statements) {
        emit_indent(ctx);
        fputs("py4_module_init();\n", ctx->out);
    }
    emit_indent(ctx);
    fputs("return 0;\n", ctx->out);
    ctx->indent_level--;
    fputs("}\n", ctx->out);
}

void emit_c_program(FILE *out, const ParseNode *root, const SemanticInfo *info)
{
    CodegenContext ctx = {0};

    if (root == NULL || root->kind != NODE_S) {
        codegen_error("expected program root");
    }

    ctx.out = out;
    ctx.root = root;
    ctx.semantic = info;
    collect_program_state(&ctx, root);

    fputs("#include <stdbool.h>\n#include <stdio.h>\n\n", out);
    emit_union_runtime(&ctx);
    emit_global_declarations(&ctx, root);
    emit_function_prototypes(&ctx, root);
    emit_module_init(&ctx, root);
    emit_top_level_functions(&ctx, root);
    emit_auto_main(&ctx);
}
