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

const char *codegen_list_struct_name(ValueType type)
{
    switch (type) {
        case TYPE_LIST_INT:
            return "Py4ListInt";
        case TYPE_LIST_FLOAT:
            return "Py4ListFloat";
        case TYPE_LIST_BOOL:
            return "Py4ListBool";
        case TYPE_LIST_CHAR:
            return "Py4ListChar";
        case TYPE_LIST_STR:
            return "Py4ListStr";
        default:
            codegen_error("%s is not a supported list type", semantic_type_name(type));
            return "";
    }
}

const char *codegen_list_runtime_prefix(ValueType type)
{
    switch (type) {
        case TYPE_LIST_INT:
            return "py4_list_int";
        case TYPE_LIST_FLOAT:
            return "py4_list_float";
        case TYPE_LIST_BOOL:
            return "py4_list_bool";
        case TYPE_LIST_CHAR:
            return "py4_list_char";
        case TYPE_LIST_STR:
            return "py4_list_str";
        default:
            codegen_error("%s is not a supported list type", semantic_type_name(type));
            return "";
    }
}

const char *codegen_list_element_c_type(ValueType type)
{
    switch (semantic_list_element_type(type)) {
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
            codegen_error("%s does not have a supported list element C type", semantic_type_name(type));
            return "";
    }
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
