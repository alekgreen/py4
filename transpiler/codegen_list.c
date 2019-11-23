#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "codegen_internal.h"

char *codegen_dup_printf(const char *fmt, ...)
{
    va_list args;
    va_list copy;
    int needed;
    char *buffer;

    va_start(args, fmt);
    va_copy(copy, args);
    needed = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);
    if (needed < 0) {
        va_end(args);
        codegen_error("failed to format generated C");
    }

    buffer = malloc((size_t)needed + 1);
    if (buffer == NULL) {
        va_end(args);
        perror("malloc");
        exit(1);
    }

    vsnprintf(buffer, (size_t)needed + 1, fmt, args);
    va_end(args);
    return buffer;
}

const char *codegen_ref_runtime_prefix(ValueType type)
{
    if (semantic_type_is_list(type)) {
        return codegen_list_runtime_prefix(type);
    }
    if (semantic_type_is_dict(type)) {
        return codegen_dict_runtime_prefix(type);
    }
    if (semantic_type_is_native(type)) {
        return semantic_native_runtime_prefix(type);
    }
    codegen_error("%s is not a supported ref type", semantic_type_name(type));
    return "";
}

const char *codegen_list_struct_name(ValueType type)
{
    static char name[MAX_NAME_LEN];

    if (!semantic_type_is_list(type)) {
        codegen_error("%s is not a supported list type", semantic_type_name(type));
    }
    snprintf(name, sizeof(name), "Py4List_%s", codegen_type_suffix(semantic_list_element_type(type)));
    return name;
}

const char *codegen_list_runtime_prefix(ValueType type)
{
    static char name[MAX_NAME_LEN];

    if (!semantic_type_is_list(type)) {
        codegen_error("%s is not a supported list type", semantic_type_name(type));
    }
    snprintf(name, sizeof(name), "py4_list_%s", codegen_type_suffix(semantic_list_element_type(type)));
    return name;
}

const char *codegen_list_element_c_type(ValueType type)
{
    ValueType element_type = semantic_list_element_type(type);
    static char tuple_name[MAX_NAME_LEN];
    static char dict_name[MAX_NAME_LEN];
    static char list_name[MAX_NAME_LEN];

    switch (element_type) {
        case TYPE_INT:
            return "int";
        case TYPE_FLOAT:
            return "double";
        case TYPE_BOOL:
            return "bool";
        case TYPE_CHAR:
            return "char";
        case TYPE_STR:
            return "const char *";
        default:
            if (semantic_type_is_optional(element_type)) {
                return codegen_type_to_c_string(element_type);
            }
            if (semantic_type_is_tuple(element_type)) {
                codegen_build_tuple_base_name(tuple_name, sizeof(tuple_name), element_type);
                return tuple_name;
            }
            if (semantic_type_is_list(element_type)) {
                snprintf(list_name, sizeof(list_name), "%s *", codegen_list_struct_name(element_type));
                return list_name;
            }
            if (semantic_type_is_dict(element_type)) {
                snprintf(dict_name, sizeof(dict_name), "%s *", codegen_dict_struct_name(element_type));
                return dict_name;
            }
            if (semantic_type_is_class(element_type)) {
                return semantic_class_name(element_type);
            }
            if (semantic_type_is_native(element_type)) {
                snprintf(list_name, sizeof(list_name), "%s *", semantic_native_c_type(element_type));
                return list_name;
            }
            codegen_error("%s does not have a supported list element C type", semantic_type_name(type));
            return "";
    }
}

const char *codegen_dict_struct_name(ValueType type)
{
    static char name[MAX_NAME_LEN];
    char key_suffix[MAX_NAME_LEN];
    char value_suffix[MAX_NAME_LEN];

    if (!semantic_type_is_dict(type)) {
        codegen_error("%s is not a supported dict type", semantic_type_name(type));
    }
    snprintf(key_suffix, sizeof(key_suffix), "%s",
        codegen_type_suffix(semantic_dict_key_type(type)));
    snprintf(value_suffix, sizeof(value_suffix), "%s",
        codegen_type_suffix(semantic_dict_value_type(type)));
    snprintf(name, sizeof(name), "Py4Dict_%s_%s", key_suffix, value_suffix);
    return name;
}

const char *codegen_dict_runtime_prefix(ValueType type)
{
    static char name[MAX_NAME_LEN];
    char key_suffix[MAX_NAME_LEN];
    char value_suffix[MAX_NAME_LEN];

    if (!semantic_type_is_dict(type)) {
        codegen_error("%s is not a supported dict type", semantic_type_name(type));
    }
    snprintf(key_suffix, sizeof(key_suffix), "%s",
        codegen_type_suffix(semantic_dict_key_type(type)));
    snprintf(value_suffix, sizeof(value_suffix), "%s",
        codegen_type_suffix(semantic_dict_value_type(type)));
    snprintf(name, sizeof(name), "py4_dict_%s_%s", key_suffix, value_suffix);
    return name;
}

char *codegen_list_new_call(ValueType type)
{
    return codegen_dup_printf("%s_new()", codegen_list_runtime_prefix(type));
}

char *codegen_list_unary_call(ValueType type, const char *suffix, const char *arg)
{
    return codegen_dup_printf("%s_%s(%s)", codegen_list_runtime_prefix(type), suffix, arg);
}

char *codegen_list_binary_call(ValueType type, const char *suffix, const char *arg0, const char *arg1)
{
    return codegen_dup_printf("%s_%s(%s, %s)", codegen_list_runtime_prefix(type), suffix, arg0, arg1);
}

char *codegen_list_ternary_call(
    ValueType type,
    const char *suffix,
    const char *arg0,
    const char *arg1,
    const char *arg2)
{
    return codegen_dup_printf("%s_%s(%s, %s, %s)",
        codegen_list_runtime_prefix(type), suffix, arg0, arg1, arg2);
}

char *codegen_dict_new_call(ValueType type)
{
    return codegen_dup_printf("%s_new()", codegen_dict_runtime_prefix(type));
}

char *codegen_dict_unary_call(ValueType type, const char *suffix, const char *arg)
{
    return codegen_dup_printf("%s_%s(%s)", codegen_dict_runtime_prefix(type), suffix, arg);
}

char *codegen_dict_binary_call(ValueType type, const char *suffix, const char *arg0, const char *arg1)
{
    return codegen_dup_printf("%s_%s(%s, %s)", codegen_dict_runtime_prefix(type), suffix, arg0, arg1);
}

char *codegen_dict_ternary_call(
    ValueType type,
    const char *suffix,
    const char *arg0,
    const char *arg1,
    const char *arg2)
{
    return codegen_dup_printf("%s_%s(%s, %s, %s)",
        codegen_dict_runtime_prefix(type), suffix, arg0, arg1, arg2);
}
