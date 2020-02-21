#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen_native_internal.h"

void emit_native_io_type_runtime(CodegenContext *ctx)
{
    fputs("typedef struct Py4IoFile {\n", ctx->out);
    fputs("    int refcount;\n", ctx->out);
    fputs("    FILE *handle;\n", ctx->out);
    fputs("    bool closed;\n", ctx->out);
    fputs("} Py4IoFile;\n\n", ctx->out);

    fputs("static Py4IoFile *py4_io_file_new(FILE *handle)\n{\n", ctx->out);
    fputs("    Py4IoFile *file = malloc(sizeof(Py4IoFile));\n", ctx->out);
    fputs("    if (file == NULL) {\n", ctx->out);
    fputs("        perror(\"malloc\");\n", ctx->out);
    fputs("        if (handle != NULL) {\n", ctx->out);
    fputs("            fclose(handle);\n", ctx->out);
    fputs("        }\n", ctx->out);
    fputs("        exit(1);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    file->refcount = 1;\n", ctx->out);
    fputs("    file->handle = handle;\n", ctx->out);
    fputs("    file->closed = false;\n", ctx->out);
    fputs("    return file;\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static void py4_io_file_incref(Py4IoFile *file)\n{\n", ctx->out);
    fputs("    if (file != NULL) {\n", ctx->out);
    fputs("        file->refcount++;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static void py4_io_file_decref(Py4IoFile *file)\n{\n", ctx->out);
    fputs("    if (file == NULL) {\n", ctx->out);
    fputs("        return;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    file->refcount--;\n", ctx->out);
    fputs("    if (file->refcount <= 0) {\n", ctx->out);
    fputs("        if (!file->closed && file->handle != NULL) {\n", ctx->out);
    fputs("            fclose(file->handle);\n", ctx->out);
    fputs("        }\n", ctx->out);
    fputs("        free(file);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("}\n\n", ctx->out);
}

int emit_native_io_function_definition(
    CodegenContext *ctx,
    const char *module_name,
    const ParseNode *name,
    const ParseNode *parameters,
    const char *c_name,
    ValueType return_type,
    ValueType first_param_type)
{
    if (module_name != NULL &&
        strcmp(module_name, "io") == 0 &&
        strcmp(name->value, "read_text") == 0 &&
        parameters->child_count == 1 &&
        first_param_type == TYPE_STR) {
        const char *path = parameters->children[0]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("FILE *file = fopen(", ctx->out);
        fputs(path, ctx->out);
        fputs(", \"rb\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("long size;\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("char *buffer;\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("size_t read_size;\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("if (file == NULL) {\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("perror(\"fopen\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("if (fseek(file, 0, SEEK_END) != 0) {\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("perror(\"fseek\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("fclose(file);\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("size = ftell(file);\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("if (size < 0) {\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("perror(\"ftell\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("fclose(file);\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("if (fseek(file, 0, SEEK_SET) != 0) {\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("perror(\"fseek\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("fclose(file);\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("buffer = malloc((size_t)size + 1);\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("if (buffer == NULL) {\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("perror(\"malloc\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("fclose(file);\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("read_size = fread(buffer, 1, (size_t)size, file);\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("if (read_size != (size_t)size) {\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("fprintf(stderr, \"Runtime error: failed to read full file\\n\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("free(buffer);\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("fclose(file);\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("buffer[size] = '\\0';\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("fclose(file);\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("return buffer;\n", ctx->out);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "io") == 0 &&
        strcmp(name->value, "open") == 0 &&
        parameters->child_count == 2 &&
        first_param_type == TYPE_STR) {
        const char *path = parameters->children[0]->value;
        const char *mode = parameters->children[1]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "FILE *handle = fopen(%s, %s);\n", path, mode);
        codegen_emit_indent(ctx);
        fputs("if (handle == NULL) {\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("perror(\"fopen\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("return py4_io_file_new(handle);\n", ctx->out);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "io") == 0 &&
        strcmp(name->value, "write_text") == 0 &&
        parameters->child_count == 2 &&
        first_param_type == TYPE_STR) {
        const char *path = parameters->children[0]->value;
        const char *data = parameters->children[1]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "FILE *file = fopen(%s, \"w\");\n", path);
        codegen_emit_indent(ctx);
        fputs("if (file == NULL) {\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("perror(\"fopen\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (fputs(%s, file) == EOF) {\n", data);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("perror(\"fputs\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("fclose(file);\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("fclose(file);\n", ctx->out);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "io") == 0 &&
        strcmp(name->value, "write") == 0 &&
        parameters->child_count == 2 &&
        first_param_type != TYPE_STR) {
        const char *file_name = parameters->children[0]->value;
        const char *data = parameters->children[1]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (%s == NULL || %s->closed || %s->handle == NULL) {\n",
            file_name, file_name, file_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("fprintf(stderr, \"Runtime error: cannot write to a closed file\\n\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (fputs(%s, %s->handle) == EOF) {\n", data, file_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("perror(\"fputs\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "io") == 0 &&
        strcmp(name->value, "read") == 0 &&
        parameters->child_count == 1 &&
        semantic_type_is_native(first_param_type)) {
        const char *file_name = parameters->children[0]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "long size;\n");
        codegen_emit_indent(ctx);
        fputs("char *buffer;\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("size_t read_size;\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (%s == NULL || %s->closed || %s->handle == NULL) {\n",
            file_name, file_name, file_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("fprintf(stderr, \"Runtime error: cannot read from a closed file\\n\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (fseek(%s->handle, 0, SEEK_END) != 0) {\n", file_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("perror(\"fseek\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "size = ftell(%s->handle);\n", file_name);
        codegen_emit_indent(ctx);
        fputs("if (size < 0) {\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("perror(\"ftell\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (fseek(%s->handle, 0, SEEK_SET) != 0) {\n", file_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("perror(\"fseek\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("buffer = malloc((size_t)size + 1);\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("if (buffer == NULL) {\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("perror(\"malloc\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "read_size = fread(buffer, 1, (size_t)size, %s->handle);\n", file_name);
        codegen_emit_indent(ctx);
        fputs("if (read_size != (size_t)size) {\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("fprintf(stderr, \"Runtime error: failed to read full file\\n\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("free(buffer);\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("buffer[size] = '\\0';\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("return buffer;\n", ctx->out);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "io") == 0 &&
        strcmp(name->value, "read_line") == 0 &&
        parameters->child_count == 1 &&
        semantic_type_is_native(first_param_type)) {
        const char *file_name = parameters->children[0]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("size_t cap = 64;\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("size_t len = 0;\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("char *buffer;\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("int ch;\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (%s == NULL || %s->closed || %s->handle == NULL) {\n",
            file_name, file_name, file_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("fprintf(stderr, \"Runtime error: cannot read from a closed file\\n\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("buffer = malloc(cap);\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("if (buffer == NULL) {\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("perror(\"malloc\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "while ((ch = fgetc(%s->handle)) != EOF) {\n", file_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("if (len + 1 >= cap) {\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("char *next;\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("cap *= 2;\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("next = realloc(buffer, cap);\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("if (next == NULL) {\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("perror(\"realloc\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("free(buffer);\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("buffer = next;\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("if (ch == '\\n') {\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("break;\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("buffer[len++] = (char)ch;\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("buffer[len] = '\\0';\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("return buffer;\n", ctx->out);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "io") == 0 &&
        strcmp(name->value, "append_text") == 0 &&
        parameters->child_count == 2 &&
        first_param_type == TYPE_STR) {
        const char *path = parameters->children[0]->value;
        const char *data = parameters->children[1]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "FILE *file = fopen(%s, \"a\");\n", path);
        codegen_emit_indent(ctx);
        fputs("if (file == NULL) {\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("perror(\"fopen\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (fputs(%s, file) == EOF) {\n", data);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("perror(\"fputs\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("fclose(file);\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("fclose(file);\n", ctx->out);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "io") == 0 &&
        strcmp(name->value, "flush") == 0 &&
        parameters->child_count == 1 &&
        semantic_type_is_native(first_param_type)) {
        const char *file_name = parameters->children[0]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (%s == NULL || %s->closed || %s->handle == NULL) {\n",
            file_name, file_name, file_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("fprintf(stderr, \"Runtime error: cannot flush a closed file\\n\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (fflush(%s->handle) != 0) {\n", file_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("perror(\"fflush\");\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("exit(1);\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "io") == 0 &&
        strcmp(name->value, "close") == 0 &&
        parameters->child_count == 1 &&
        semantic_type_is_native(first_param_type)) {
        const char *file_name = parameters->children[0]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (%s == NULL || %s->closed) {\n", file_name, file_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("return;\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "if (%s->handle != NULL) {\n", file_name);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "fclose(%s->handle);\n", file_name);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "%s->handle = NULL;\n", file_name);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "%s->closed = true;\n", file_name);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "io") == 0 &&
        strcmp(name->value, "exists") == 0 &&
        parameters->child_count == 1 &&
        first_param_type == TYPE_STR) {
        const char *path = parameters->children[0]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "FILE *file = fopen(%s, \"r\");\n", path);
        codegen_emit_indent(ctx);
        fputs("if (file == NULL) {\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fputs("return false;\n", ctx->out);
        ctx->indent_level--;
        codegen_emit_indent(ctx);
        fputs("}\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("fclose(file);\n", ctx->out);
        codegen_emit_indent(ctx);
        fputs("return true;\n", ctx->out);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    return 0;
}
