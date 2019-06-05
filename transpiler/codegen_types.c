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
    TYPE_NONE,
    TYPE_LIST_INT,
    TYPE_LIST_FLOAT,
    TYPE_LIST_BOOL,
    TYPE_LIST_CHAR
};

const size_t CODEGEN_ORDERED_TYPE_COUNT =
    sizeof(CODEGEN_ORDERED_TYPES) / sizeof(CODEGEN_ORDERED_TYPES[0]);

void codegen_build_tuple_base_name(char *buffer, size_t size, ValueType type)
{
    size_t count;

    if (!semantic_type_is_tuple(type)) {
        codegen_error("%s is not a tuple type", semantic_type_name(type));
    }

    snprintf(buffer, size, "py4_tuple");
    count = semantic_tuple_element_count(type);
    for (size_t i = 0; i < count; i++) {
        const char *suffix = codegen_type_suffix(semantic_tuple_element_type(type, i));

        if (strlen(buffer) + strlen(suffix) + 2 >= size) {
            codegen_error("generated tuple identifier is too long");
        }
        strcat(buffer, "_");
        strcat(buffer, suffix);
    }
}

void codegen_build_tuple_print_name(char *buffer, size_t size, ValueType type)
{
    char base_name[MAX_NAME_LEN];
    const char *trimmed;

    codegen_build_tuple_base_name(base_name, sizeof(base_name), type);
    trimmed = strncmp(base_name, "py4_", 4) == 0 ? base_name + 4 : base_name;
    snprintf(buffer, size, "py4_print_%s", trimmed);
}

void codegen_build_tuple_retain_name(char *buffer, size_t size, ValueType type)
{
    char base_name[MAX_NAME_LEN];
    const char *trimmed;

    codegen_build_tuple_base_name(base_name, sizeof(base_name), type);
    trimmed = strncmp(base_name, "py4_", 4) == 0 ? base_name + 4 : base_name;
    snprintf(buffer, size, "py4_retain_%s", trimmed);
}

void codegen_build_tuple_release_name(char *buffer, size_t size, ValueType type)
{
    char base_name[MAX_NAME_LEN];
    const char *trimmed;

    codegen_build_tuple_base_name(base_name, sizeof(base_name), type);
    trimmed = strncmp(base_name, "py4_", 4) == 0 ? base_name + 4 : base_name;
    snprintf(buffer, size, "py4_release_%s", trimmed);
}

void codegen_build_class_print_name(char *buffer, size_t size, ValueType type)
{
    snprintf(buffer, size, "py4_print_%s", semantic_class_name(type));
}

void codegen_build_class_retain_name(char *buffer, size_t size, ValueType type)
{
    snprintf(buffer, size, "py4_retain_%s", semantic_class_name(type));
}

void codegen_build_class_release_name(char *buffer, size_t size, ValueType type)
{
    snprintf(buffer, size, "py4_release_%s", semantic_class_name(type));
}

void codegen_build_list_print_name(char *buffer, size_t size, ValueType type)
{
    snprintf(buffer, size, "py4_print_%s", codegen_type_suffix(type));
}

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

const ParseNode *codegen_simple_statement_target(const ParseNode *simple_stmt)
{
    if (simple_stmt == NULL || simple_stmt->child_count == 0) {
        codegen_error("malformed simple statement");
    }
    return simple_stmt->children[0];
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
    static char tuple_name[MAX_NAME_LEN];

    if (semantic_type_is_tuple(type)) {
        codegen_build_tuple_base_name(tuple_name, sizeof(tuple_name), type);
        return tuple_name;
    }
    if (semantic_type_is_class(type)) {
        return semantic_class_name(type);
    }

    switch (type) {
        case TYPE_INT: return "int";
        case TYPE_FLOAT: return "float";
        case TYPE_BOOL: return "bool";
        case TYPE_CHAR: return "char";
        case TYPE_STR: return "str";
        case TYPE_NONE: return "none";
        case TYPE_LIST_INT: return "list_int";
        case TYPE_LIST_FLOAT: return "list_float";
        case TYPE_LIST_BOOL: return "list_bool";
        case TYPE_LIST_CHAR: return "list_char";
        default: return "unknown";
    }
}

const char *codegen_type_field(ValueType type)
{
    if (semantic_type_is_tuple(type)) {
        codegen_error("tuple types are not valid union fields");
    }
    if (semantic_type_is_class(type)) {
        codegen_error("class types are not valid union fields");
    }

    switch (type) {
        case TYPE_INT: return "as_int";
        case TYPE_FLOAT: return "as_float";
        case TYPE_BOOL: return "as_bool";
        case TYPE_CHAR: return "as_char";
        case TYPE_STR: return "as_str";
        case TYPE_LIST_INT: return "as_list_int";
        case TYPE_LIST_FLOAT: return "as_list_float";
        case TYPE_LIST_BOOL: return "as_list_bool";
        case TYPE_LIST_CHAR: return "as_list_char";
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

static int is_codegen_builtin_name(const char *name)
{
    return strcmp(name, "len") == 0 ||
        strcmp(name, "range") == 0 ||
        strcmp(name, "list_int") == 0 ||
        strcmp(name, "list_float") == 0 ||
        strcmp(name, "list_bool") == 0 ||
        strcmp(name, "list_char") == 0 ||
        strcmp(name, "list_append") == 0 ||
        strcmp(name, "list_get") == 0 ||
        strcmp(name, "list_len") == 0 ||
        strcmp(name, "list_set") == 0;
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
    char tuple_name[MAX_NAME_LEN];

    if (semantic_type_is_tuple(type)) {
        codegen_build_tuple_base_name(tuple_name, sizeof(tuple_name), type);
        fputs(tuple_name, out);
        return;
    }
    if (semantic_type_is_class(type)) {
        fputs(semantic_class_name(type), out);
        return;
    }

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
        case TYPE_LIST_INT:
        case TYPE_LIST_FLOAT:
        case TYPE_LIST_BOOL:
        case TYPE_LIST_CHAR:
            fprintf(out, "%s *", codegen_list_struct_name(type));
            return;
        default:
            codegen_error("unsupported scalar type %s", semantic_type_name(type));
    }
}

char *codegen_type_to_c_string(ValueType type)
{
    char buffer[MAX_NAME_LEN];

    if (!semantic_type_is_union(type)) {
        switch (type) {
            case TYPE_INT: return codegen_dup_printf("int");
            case TYPE_FLOAT: return codegen_dup_printf("double");
            case TYPE_BOOL: return codegen_dup_printf("bool");
            case TYPE_CHAR: return codegen_dup_printf("char");
            case TYPE_STR: return codegen_dup_printf("const char *");
            case TYPE_NONE: return codegen_dup_printf("void");
            case TYPE_LIST_INT:
            case TYPE_LIST_FLOAT:
            case TYPE_LIST_BOOL:
            case TYPE_LIST_CHAR:
                return codegen_dup_printf("%s *", codegen_list_struct_name(type));
        }
        if (semantic_type_is_tuple(type)) {
            char tuple_name[MAX_NAME_LEN];

            codegen_build_tuple_base_name(tuple_name, sizeof(tuple_name), type);
            return codegen_dup_printf("%s", tuple_name);
        }
        if (semantic_type_is_class(type)) {
            return codegen_dup_printf("%s", semantic_class_name(type));
        }
    }

    codegen_build_union_base_name(buffer, sizeof(buffer), type);
    return codegen_dup_printf("%s", buffer);
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
        const ParseNode *target = codegen_simple_statement_target(node);
        const ParseNode *statement_tail = codegen_simple_statement_tail(node);
        const ParseNode *expr = codegen_statement_tail_expression(statement_tail);
        ValueType expr_type = semantic_type_of(ctx->semantic, expr);
        ValueType target_type;

        if (target->kind == NODE_INDEX) {
            target_type = TYPE_INT;
        } else {
            target_type = codegen_is_type_assignment(statement_tail)
                ? semantic_type_of(ctx->semantic, codegen_statement_tail_type_node(statement_tail))
                : semantic_type_of(ctx->semantic, target);
        }

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
        ValueType class_type;

        if (strcmp(callee->value, "print") == 0) {
            if (arguments->child_count == 1 && !codegen_is_epsilon_node(arguments->children[0])) {
                codegen_add_printable_union_type(ctx, semantic_type_of(ctx->semantic, arguments->children[0]));
            }
        } else if (is_codegen_builtin_name(callee->value)) {
            /* Builtins lower directly to runtime helpers and do not need union conversions. */
        } else {
            class_type = semantic_find_class_type(callee->value);
            if (class_type != 0) {
                return;
            }

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

    if (node->kind == NODE_METHOD_CALL) {
        const ParseNode *receiver = node->children[0];
        const ParseNode *method = codegen_expect_child(node, 1, NODE_PRIMARY);
        const ParseNode *arguments = codegen_expect_child(node, 2, NODE_ARGUMENTS);
        ValueType receiver_type = semantic_type_of(ctx->semantic, receiver);

        if (!semantic_type_is_class(receiver_type)) {
            return;
        }

        for (size_t i = 0; i < arguments->child_count; i++) {
            ValueType expr_type;
            ValueType target_type;

            if (codegen_is_epsilon_node(arguments->children[i])) {
                continue;
            }

            expr_type = semantic_type_of(ctx->semantic, arguments->children[i]);
            target_type = semantic_method_parameter_type(ctx->semantic, receiver_type, method->value, i);
            if (semantic_type_is_union(target_type) &&
                semantic_type_is_union(expr_type) &&
                target_type != expr_type) {
                codegen_add_union_conversion(ctx, expr_type, target_type);
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

static void emit_managed_field_retain(FILE *out, ValueType type, const char *value_expr);
static void emit_managed_field_release(FILE *out, ValueType type, const char *value_expr);

static void emit_tuple_value_print(FILE *out, ValueType type, const char *value_expr)
{
    char helper_name[MAX_NAME_LEN];

    if (semantic_type_is_tuple(type)) {
        codegen_build_tuple_print_name(helper_name, sizeof(helper_name), type);
        fprintf(out, "            %s(%s);\n", helper_name, value_expr);
        return;
    }
    if (semantic_type_is_class(type)) {
        codegen_build_class_print_name(helper_name, sizeof(helper_name), type);
        fprintf(out, "            %s(%s);\n", helper_name, value_expr);
        return;
    }
    if (semantic_type_is_list(type)) {
        codegen_build_list_print_name(helper_name, sizeof(helper_name), type);
        fprintf(out, "            %s(%s);\n", helper_name, value_expr);
        return;
    }

    switch (type) {
        case TYPE_INT:
            fprintf(out, "            printf(\"%%d\", %s);\n", value_expr);
            return;
        case TYPE_FLOAT:
            fprintf(out, "            printf(\"%%g\", %s);\n", value_expr);
            return;
        case TYPE_BOOL:
            fprintf(out, "            printf(\"%%s\", %s ? \"True\" : \"False\");\n", value_expr);
            return;
        case TYPE_CHAR:
            fprintf(out, "            printf(\"%%c\", %s);\n", value_expr);
            return;
        case TYPE_STR:
            fprintf(out, "            printf(\"%%s\", %s);\n", value_expr);
            return;
        default:
            codegen_error("unsupported tuple print type %s", semantic_type_name(type));
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

void codegen_emit_tuple_runtime(CodegenContext *ctx)
{
    char helper_name[MAX_NAME_LEN];

    for (size_t i = 0; i < semantic_tuple_type_count(); i++) {
        ValueType tuple_type = semantic_tuple_type_at(i);
        char tuple_name[MAX_NAME_LEN];
        size_t element_count = semantic_tuple_element_count(tuple_type);

        codegen_build_tuple_base_name(tuple_name, sizeof(tuple_name), tuple_type);
        fprintf(ctx->out, "typedef struct {\n");
        for (size_t j = 0; j < element_count; j++) {
            codegen_emit_scalar_c_type(ctx->out, semantic_tuple_element_type(tuple_type, j));
            fprintf(ctx->out, " item%zu;\n", j);
        }
        fprintf(ctx->out, "} %s;\n\n", tuple_name);
    }

    for (size_t i = 0; i < semantic_tuple_type_count(); i++) {
        ValueType tuple_type = semantic_tuple_type_at(i);
        char tuple_name[MAX_NAME_LEN];

        codegen_build_tuple_base_name(tuple_name, sizeof(tuple_name), tuple_type);
        codegen_build_tuple_retain_name(helper_name, sizeof(helper_name), tuple_type);
        fprintf(ctx->out, "static void %s(%s *value);\n", helper_name, tuple_name);
        codegen_build_tuple_release_name(helper_name, sizeof(helper_name), tuple_type);
        fprintf(ctx->out, "static void %s(%s *value);\n", helper_name, tuple_name);
        codegen_build_tuple_print_name(helper_name, sizeof(helper_name), tuple_type);
        fprintf(ctx->out, "static void %s(%s value);\n", helper_name, tuple_name);
    }

    if (semantic_tuple_type_count() > 0) {
        fputc('\n', ctx->out);
    }

    for (size_t i = 0; i < semantic_tuple_type_count(); i++) {
        ValueType tuple_type = semantic_tuple_type_at(i);
        char tuple_name[MAX_NAME_LEN];
        size_t element_count = semantic_tuple_element_count(tuple_type);

        codegen_build_tuple_base_name(tuple_name, sizeof(tuple_name), tuple_type);
        codegen_build_tuple_retain_name(helper_name, sizeof(helper_name), tuple_type);
        fprintf(ctx->out, "static void %s(%s *value)\n{\n", helper_name, tuple_name);
        for (size_t j = 0; j < element_count; j++) {
            char value_expr[MAX_NAME_LEN];

            snprintf(value_expr, sizeof(value_expr), "value->item%zu", j);
            emit_managed_field_retain(ctx->out, semantic_tuple_element_type(tuple_type, j), value_expr);
        }
        fputs("}\n\n", ctx->out);

        codegen_build_tuple_release_name(helper_name, sizeof(helper_name), tuple_type);
        fprintf(ctx->out, "static void %s(%s *value)\n{\n", helper_name, tuple_name);
        for (size_t j = element_count; j > 0; j--) {
            char value_expr[MAX_NAME_LEN];

            snprintf(value_expr, sizeof(value_expr), "value->item%zu", j - 1);
            emit_managed_field_release(ctx->out, semantic_tuple_element_type(tuple_type, j - 1), value_expr);
        }
        fputs("}\n\n", ctx->out);

        codegen_build_tuple_print_name(helper_name, sizeof(helper_name), tuple_type);
        fprintf(ctx->out, "static void %s(%s value)\n{\n", helper_name, tuple_name);
        fputs("    printf(\"(\");\n", ctx->out);
        for (size_t j = 0; j < element_count; j++) {
            char value_expr[MAX_NAME_LEN];

            if (j > 0) {
                fputs("    printf(\", \");\n", ctx->out);
            }
            snprintf(value_expr, sizeof(value_expr), "value.item%zu", j);
            emit_tuple_value_print(ctx->out, semantic_tuple_element_type(tuple_type, j), value_expr);
        }
        fputs("    printf(\")\");\n", ctx->out);
        fputs("}\n\n", ctx->out);
    }
}

void codegen_emit_class_types(CodegenContext *ctx)
{
    char helper_name[MAX_NAME_LEN];

    for (size_t i = 0; i < semantic_class_type_count(); i++) {
        ValueType class_type = semantic_class_type_at(i);

        fprintf(ctx->out, "typedef struct {\n");
        for (size_t j = 0; j < semantic_class_field_count(class_type); j++) {
            codegen_emit_scalar_c_type(ctx->out, semantic_class_field_type(class_type, j));
            fprintf(ctx->out, " %s;\n", semantic_class_field_name(class_type, j));
        }
        fprintf(ctx->out, "} %s;\n\n", semantic_class_name(class_type));
    }

    for (size_t i = 0; i < semantic_class_type_count(); i++) {
        ValueType class_type = semantic_class_type_at(i);

        codegen_build_class_print_name(helper_name, sizeof(helper_name), class_type);
        fprintf(ctx->out, "static void %s(%s value);\n",
            helper_name,
            semantic_class_name(class_type));
    }

    if (semantic_class_type_count() > 0) {
        fputc('\n', ctx->out);
    }

    for (size_t i = 0; i < semantic_class_type_count(); i++) {
        ValueType class_type = semantic_class_type_at(i);

        codegen_build_class_print_name(helper_name, sizeof(helper_name), class_type);
        fprintf(ctx->out, "static void %s(%s value)\n{\n",
            helper_name,
            semantic_class_name(class_type));
        fputs("    printf(\"", ctx->out);
        fputs(semantic_class_name(class_type), ctx->out);
        fputs("(\");\n", ctx->out);
        for (size_t j = 0; j < semantic_class_field_count(class_type); j++) {
            char value_expr[MAX_NAME_LEN];

            if (j > 0) {
                fputs("    printf(\", \");\n", ctx->out);
            }
            fprintf(ctx->out, "    printf(\"%s=\");\n", semantic_class_field_name(class_type, j));
            snprintf(value_expr, sizeof(value_expr), "value.%s", semantic_class_field_name(class_type, j));
            emit_tuple_value_print(ctx->out, semantic_class_field_type(class_type, j), value_expr);
        }
        fputs("    printf(\")\");\n", ctx->out);
        fputs("}\n\n", ctx->out);
    }
}

static size_t tuple_type_index(ValueType type)
{
    for (size_t i = 0; i < semantic_tuple_type_count(); i++) {
        if (semantic_tuple_type_at(i) == type) {
            return i;
        }
    }
    codegen_error("unknown tuple type index for %s", semantic_type_name(type));
    return 0;
}

static size_t class_type_index(ValueType type)
{
    for (size_t i = 0; i < semantic_class_type_count(); i++) {
        if (semantic_class_type_at(i) == type) {
            return i;
        }
    }
    codegen_error("unknown class type index for %s", semantic_type_name(type));
    return 0;
}

static void emit_managed_field_retain(FILE *out, ValueType type, const char *value_expr)
{
    char helper_name[MAX_NAME_LEN];

    if (!semantic_type_needs_management(type)) {
        return;
    }

    if (semantic_type_is_ref(type)) {
        fprintf(out, "    %s_incref(%s);\n", codegen_list_runtime_prefix(type), value_expr);
        return;
    }

    if (semantic_type_is_tuple(type)) {
        codegen_build_tuple_retain_name(helper_name, sizeof(helper_name), type);
        fprintf(out, "    %s(&%s);\n", helper_name, value_expr);
        return;
    }

    if (semantic_type_is_class(type)) {
        codegen_build_class_retain_name(helper_name, sizeof(helper_name), type);
        fprintf(out, "    %s(&%s);\n", helper_name, value_expr);
        return;
    }

    codegen_error("unsupported managed type %s", semantic_type_name(type));
}

static void emit_managed_field_release(FILE *out, ValueType type, const char *value_expr)
{
    char helper_name[MAX_NAME_LEN];

    if (!semantic_type_needs_management(type)) {
        return;
    }

    if (semantic_type_is_ref(type)) {
        fprintf(out, "    %s_decref(%s);\n", codegen_list_runtime_prefix(type), value_expr);
        return;
    }

    if (semantic_type_is_tuple(type)) {
        codegen_build_tuple_release_name(helper_name, sizeof(helper_name), type);
        fprintf(out, "    %s(&%s);\n", helper_name, value_expr);
        return;
    }

    if (semantic_type_is_class(type)) {
        codegen_build_class_release_name(helper_name, sizeof(helper_name), type);
        fprintf(out, "    %s(&%s);\n", helper_name, value_expr);
        return;
    }

    codegen_error("unsupported managed type %s", semantic_type_name(type));
}

static void emit_list_print_definitions(CodegenContext *ctx)
{
    for (size_t i = 0; i < CODEGEN_ORDERED_TYPE_COUNT; i++) {
        ValueType type = CODEGEN_ORDERED_TYPES[i];
        char helper_name[MAX_NAME_LEN];

        if (!semantic_type_is_list(type)) {
            continue;
        }

        codegen_build_list_print_name(helper_name, sizeof(helper_name), type);
        fprintf(ctx->out, "static void %s(%s *value)\n{\n", helper_name, codegen_list_struct_name(type));
        fputs("    printf(\"[\");\n", ctx->out);
        fputs("    if (value != NULL) {\n", ctx->out);
        fputs("        for (size_t i = 0; i < value->len; i++) {\n", ctx->out);
        fputs("            if (i > 0) {\n", ctx->out);
        fputs("                printf(\", \");\n", ctx->out);
        fputs("            }\n", ctx->out);
        switch (type) {
            case TYPE_LIST_INT:
                fputs("            printf(\"%d\", value->items[i]);\n", ctx->out);
                break;
            case TYPE_LIST_FLOAT:
                fputs("            printf(\"%g\", value->items[i]);\n", ctx->out);
                break;
            case TYPE_LIST_BOOL:
                fputs("            printf(\"%s\", value->items[i] ? \"True\" : \"False\");\n", ctx->out);
                break;
            case TYPE_LIST_CHAR:
                fputs("            printf(\"%c\", value->items[i]);\n", ctx->out);
                break;
            default:
                codegen_error("unsupported list print type %s", semantic_type_name(type));
        }
        fputs("        }\n", ctx->out);
        fputs("    }\n", ctx->out);
        fputs("    printf(\"]\");\n", ctx->out);
        fputs("}\n\n", ctx->out);
    }
}

static void emit_struct_definition_recursive(
    CodegenContext *ctx,
    ValueType type,
    unsigned char *emitted_tuples,
    unsigned char *visiting_tuples,
    unsigned char *emitted_classes,
    unsigned char *visiting_classes);

static void emit_tuple_definition_recursive(
    CodegenContext *ctx,
    ValueType tuple_type,
    unsigned char *emitted_tuples,
    unsigned char *visiting_tuples,
    unsigned char *emitted_classes,
    unsigned char *visiting_classes)
{
    size_t index = tuple_type_index(tuple_type);
    char tuple_name[MAX_NAME_LEN];
    size_t element_count = semantic_tuple_element_count(tuple_type);

    if (emitted_tuples[index]) {
        return;
    }
    if (visiting_tuples[index]) {
        codegen_error("recursive tuple type dependency is not supported for %s", semantic_type_name(tuple_type));
    }
    visiting_tuples[index] = 1;

    for (size_t i = 0; i < element_count; i++) {
        emit_struct_definition_recursive(
            ctx,
            semantic_tuple_element_type(tuple_type, i),
            emitted_tuples,
            visiting_tuples,
            emitted_classes,
            visiting_classes);
    }

    codegen_build_tuple_base_name(tuple_name, sizeof(tuple_name), tuple_type);
    fprintf(ctx->out, "typedef struct {\n");
    for (size_t i = 0; i < element_count; i++) {
        codegen_emit_scalar_c_type(ctx->out, semantic_tuple_element_type(tuple_type, i));
        fprintf(ctx->out, " item%zu;\n", i);
    }
    fprintf(ctx->out, "} %s;\n\n", tuple_name);

    visiting_tuples[index] = 0;
    emitted_tuples[index] = 1;
}

static void emit_class_definition_recursive(
    CodegenContext *ctx,
    ValueType class_type,
    unsigned char *emitted_tuples,
    unsigned char *visiting_tuples,
    unsigned char *emitted_classes,
    unsigned char *visiting_classes)
{
    size_t index = class_type_index(class_type);

    if (emitted_classes[index]) {
        return;
    }
    if (visiting_classes[index]) {
        codegen_error("recursive class field dependency is not supported for %s", semantic_type_name(class_type));
    }
    visiting_classes[index] = 1;

    for (size_t i = 0; i < semantic_class_field_count(class_type); i++) {
        emit_struct_definition_recursive(
            ctx,
            semantic_class_field_type(class_type, i),
            emitted_tuples,
            visiting_tuples,
            emitted_classes,
            visiting_classes);
    }

    fprintf(ctx->out, "typedef struct {\n");
    for (size_t i = 0; i < semantic_class_field_count(class_type); i++) {
        codegen_emit_scalar_c_type(ctx->out, semantic_class_field_type(class_type, i));
        fprintf(ctx->out, " %s;\n", semantic_class_field_name(class_type, i));
    }
    fprintf(ctx->out, "} %s;\n\n", semantic_class_name(class_type));

    visiting_classes[index] = 0;
    emitted_classes[index] = 1;
}

static void emit_struct_definition_recursive(
    CodegenContext *ctx,
    ValueType type,
    unsigned char *emitted_tuples,
    unsigned char *visiting_tuples,
    unsigned char *emitted_classes,
    unsigned char *visiting_classes)
{
    if (semantic_type_is_tuple(type)) {
        emit_tuple_definition_recursive(
            ctx,
            type,
            emitted_tuples,
            visiting_tuples,
            emitted_classes,
            visiting_classes);
    } else if (semantic_type_is_class(type)) {
        emit_class_definition_recursive(
            ctx,
            type,
            emitted_tuples,
            visiting_tuples,
            emitted_classes,
            visiting_classes);
    }
}

void codegen_emit_struct_types(CodegenContext *ctx)
{
    unsigned char emitted_tuples[MAX_TUPLE_TYPES] = {0};
    unsigned char visiting_tuples[MAX_TUPLE_TYPES] = {0};
    unsigned char emitted_classes[MAX_CLASS_TYPES] = {0};
    unsigned char visiting_classes[MAX_CLASS_TYPES] = {0};
    char helper_name[MAX_NAME_LEN];

    for (size_t i = 0; i < semantic_class_type_count(); i++) {
        emit_class_definition_recursive(
            ctx,
            semantic_class_type_at(i),
            emitted_tuples,
            visiting_tuples,
            emitted_classes,
            visiting_classes);
    }
    for (size_t i = 0; i < semantic_tuple_type_count(); i++) {
        emit_tuple_definition_recursive(
            ctx,
            semantic_tuple_type_at(i),
            emitted_tuples,
            visiting_tuples,
            emitted_classes,
            visiting_classes);
    }

    for (size_t i = 0; i < semantic_class_type_count(); i++) {
        ValueType class_type = semantic_class_type_at(i);

        codegen_build_class_retain_name(helper_name, sizeof(helper_name), class_type);
        fprintf(ctx->out, "static void %s(%s *value);\n",
            helper_name,
            semantic_class_name(class_type));
        codegen_build_class_release_name(helper_name, sizeof(helper_name), class_type);
        fprintf(ctx->out, "static void %s(%s *value);\n",
            helper_name,
            semantic_class_name(class_type));
        codegen_build_class_print_name(helper_name, sizeof(helper_name), class_type);
        fprintf(ctx->out, "static void %s(%s value);\n",
            helper_name,
            semantic_class_name(class_type));
    }
    for (size_t i = 0; i < semantic_tuple_type_count(); i++) {
        ValueType tuple_type = semantic_tuple_type_at(i);
        char tuple_name[MAX_NAME_LEN];

        codegen_build_tuple_base_name(tuple_name, sizeof(tuple_name), tuple_type);
        codegen_build_tuple_retain_name(helper_name, sizeof(helper_name), tuple_type);
        fprintf(ctx->out, "static void %s(%s *value);\n", helper_name, tuple_name);
        codegen_build_tuple_release_name(helper_name, sizeof(helper_name), tuple_type);
        fprintf(ctx->out, "static void %s(%s *value);\n", helper_name, tuple_name);
        codegen_build_tuple_print_name(helper_name, sizeof(helper_name), tuple_type);
        fprintf(ctx->out, "static void %s(%s value);\n", helper_name, tuple_name);
    }

    if (semantic_class_type_count() > 0 || semantic_tuple_type_count() > 0) {
        fputc('\n', ctx->out);
    }

    emit_list_print_definitions(ctx);

    for (size_t i = 0; i < semantic_class_type_count(); i++) {
        ValueType class_type = semantic_class_type_at(i);

        codegen_build_class_retain_name(helper_name, sizeof(helper_name), class_type);
        fprintf(ctx->out, "static void %s(%s *value)\n{\n",
            helper_name,
            semantic_class_name(class_type));
        for (size_t j = 0; j < semantic_class_field_count(class_type); j++) {
            char value_expr[MAX_NAME_LEN];

            snprintf(value_expr, sizeof(value_expr), "value->%s", semantic_class_field_name(class_type, j));
            emit_managed_field_retain(ctx->out, semantic_class_field_type(class_type, j), value_expr);
        }
        fputs("}\n\n", ctx->out);

        codegen_build_class_release_name(helper_name, sizeof(helper_name), class_type);
        fprintf(ctx->out, "static void %s(%s *value)\n{\n",
            helper_name,
            semantic_class_name(class_type));
        for (size_t j = semantic_class_field_count(class_type); j > 0; j--) {
            char value_expr[MAX_NAME_LEN];

            snprintf(value_expr, sizeof(value_expr), "value->%s", semantic_class_field_name(class_type, j - 1));
            emit_managed_field_release(ctx->out, semantic_class_field_type(class_type, j - 1), value_expr);
        }
        fputs("}\n\n", ctx->out);

        codegen_build_class_print_name(helper_name, sizeof(helper_name), class_type);
        fprintf(ctx->out, "static void %s(%s value)\n{\n",
            helper_name,
            semantic_class_name(class_type));
        fputs("    printf(\"", ctx->out);
        fputs(semantic_class_name(class_type), ctx->out);
        fputs("(\");\n", ctx->out);
        for (size_t j = 0; j < semantic_class_field_count(class_type); j++) {
            char value_expr[MAX_NAME_LEN];

            if (j > 0) {
                fputs("    printf(\", \");\n", ctx->out);
            }
            fprintf(ctx->out, "    printf(\"%s=\");\n", semantic_class_field_name(class_type, j));
            snprintf(value_expr, sizeof(value_expr), "value.%s", semantic_class_field_name(class_type, j));
            emit_tuple_value_print(ctx->out, semantic_class_field_type(class_type, j), value_expr);
        }
        fputs("    printf(\")\");\n", ctx->out);
        fputs("}\n\n", ctx->out);
    }

    for (size_t i = 0; i < semantic_tuple_type_count(); i++) {
        ValueType tuple_type = semantic_tuple_type_at(i);
        char tuple_name[MAX_NAME_LEN];
        size_t element_count = semantic_tuple_element_count(tuple_type);

        codegen_build_tuple_base_name(tuple_name, sizeof(tuple_name), tuple_type);
        codegen_build_tuple_retain_name(helper_name, sizeof(helper_name), tuple_type);
        fprintf(ctx->out, "static void %s(%s *value)\n{\n", helper_name, tuple_name);
        for (size_t j = 0; j < element_count; j++) {
            char value_expr[MAX_NAME_LEN];

            snprintf(value_expr, sizeof(value_expr), "value->item%zu", j);
            emit_managed_field_retain(ctx->out, semantic_tuple_element_type(tuple_type, j), value_expr);
        }
        fputs("}\n\n", ctx->out);

        codegen_build_tuple_release_name(helper_name, sizeof(helper_name), tuple_type);
        fprintf(ctx->out, "static void %s(%s *value)\n{\n", helper_name, tuple_name);
        for (size_t j = element_count; j > 0; j--) {
            char value_expr[MAX_NAME_LEN];

            snprintf(value_expr, sizeof(value_expr), "value->item%zu", j - 1);
            emit_managed_field_release(ctx->out, semantic_tuple_element_type(tuple_type, j - 1), value_expr);
        }
        fputs("}\n\n", ctx->out);

        codegen_build_tuple_print_name(helper_name, sizeof(helper_name), tuple_type);
        fprintf(ctx->out, "static void %s(%s value)\n{\n", helper_name, tuple_name);
        fputs("    printf(\"(\");\n", ctx->out);
        for (size_t j = 0; j < element_count; j++) {
            char value_expr[MAX_NAME_LEN];

            if (j > 0) {
                fputs("    printf(\", \");\n", ctx->out);
            }
            snprintf(value_expr, sizeof(value_expr), "value.item%zu", j);
            emit_tuple_value_print(ctx->out, semantic_tuple_element_type(tuple_type, j), value_expr);
        }
        fputs("    printf(\")\");\n", ctx->out);
        fputs("}\n\n", ctx->out);
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
        } else if (payload->kind != NODE_CLASS_DEF) {
            ctx->has_top_level_executable_statements = 1;
        }
    }

    codegen_collect_union_types_from_node(ctx, root);
    codegen_collect_required_conversions(ctx, root);
}
