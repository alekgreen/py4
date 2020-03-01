#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen_native_internal.h"

void emit_native_http_runtime(CodegenContext *ctx)
{
    ValueType response_type = semantic_make_tuple_type((ValueType[]){TYPE_INT, TYPE_STR}, 2);

    fputs("typedef struct Py4HttpBuffer {\n", ctx->out);
    fputs("    char *data;\n", ctx->out);
    fputs("    size_t length;\n", ctx->out);
    fputs("    size_t capacity;\n", ctx->out);
    fputs("} Py4HttpBuffer;\n\n", ctx->out);

    fputs("static int py4_http_initialized = 0;\n\n", ctx->out);

    fputs("static void py4_http_fail(const char *message)\n{\n", ctx->out);
    fputs("    fprintf(stderr, \"Runtime error: %s\\n\", message);\n", ctx->out);
    fputs("    exit(1);\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static void py4_http_global_cleanup(void)\n{\n", ctx->out);
    fputs("    if (py4_http_initialized) {\n", ctx->out);
    fputs("        curl_global_cleanup();\n", ctx->out);
    fputs("        py4_http_initialized = 0;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static void py4_http_global_init(void)\n{\n", ctx->out);
    fputs("    CURLcode result;\n", ctx->out);
    fputs("    if (py4_http_initialized) {\n", ctx->out);
    fputs("        return;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    result = curl_global_init(CURL_GLOBAL_DEFAULT);\n", ctx->out);
    fputs("    if (result != CURLE_OK) {\n", ctx->out);
    fputs("        py4_http_fail(\"failed to initialize libcurl\");\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    py4_http_initialized = 1;\n", ctx->out);
    fputs("    atexit(py4_http_global_cleanup);\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static void py4_http_buffer_init(Py4HttpBuffer *buffer)\n{\n", ctx->out);
    fputs("    buffer->data = malloc(1);\n", ctx->out);
    fputs("    if (buffer->data == NULL) {\n", ctx->out);
    fputs("        perror(\"malloc\");\n", ctx->out);
    fputs("        exit(1);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    buffer->data[0] = '\\0';\n", ctx->out);
    fputs("    buffer->length = 0;\n", ctx->out);
    fputs("    buffer->capacity = 1;\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static size_t py4_http_write_body(char *ptr, size_t size, size_t nmemb, void *userdata)\n{\n", ctx->out);
    fputs("    Py4HttpBuffer *buffer = userdata;\n", ctx->out);
    fputs("    size_t total = size * nmemb;\n", ctx->out);
    fputs("    size_t needed;\n", ctx->out);
    fputs("    char *grown;\n", ctx->out);
    fputs("    if (total == 0) {\n", ctx->out);
    fputs("        return 0;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    if (memchr(ptr, '\\0', total) != NULL) {\n", ctx->out);
    fputs("        py4_http_fail(\"http response body contains NUL byte\");\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    needed = buffer->length + total + 1;\n", ctx->out);
    fputs("    if (needed > buffer->capacity) {\n", ctx->out);
    fputs("        size_t new_capacity = buffer->capacity;\n", ctx->out);
    fputs("        while (new_capacity < needed) {\n", ctx->out);
    fputs("            new_capacity *= 2;\n", ctx->out);
    fputs("        }\n", ctx->out);
    fputs("        grown = realloc(buffer->data, new_capacity);\n", ctx->out);
    fputs("        if (grown == NULL) {\n", ctx->out);
    fputs("            perror(\"realloc\");\n", ctx->out);
    fputs("            exit(1);\n", ctx->out);
    fputs("        }\n", ctx->out);
    fputs("        buffer->data = grown;\n", ctx->out);
    fputs("        buffer->capacity = new_capacity;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    memcpy(buffer->data + buffer->length, ptr, total);\n", ctx->out);
    fputs("    buffer->length += total;\n", ctx->out);
    fputs("    buffer->data[buffer->length] = '\\0';\n", ctx->out);
    fputs("    return total;\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static void py4_http_fail_curl_request(const char *method, CURLcode result)\n{\n", ctx->out);
    fputs("    if (result == CURLE_URL_MALFORMAT || result == CURLE_UNSUPPORTED_PROTOCOL) {\n", ctx->out);
    fputs("        py4_http_fail(\"malformed http URL\");\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    if (result == CURLE_COULDNT_CONNECT || result == CURLE_COULDNT_RESOLVE_HOST || result == CURLE_COULDNT_RESOLVE_PROXY) {\n", ctx->out);
    fputs("        py4_http_fail(\"http connection failed\");\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    fprintf(stderr, \"Runtime error: http %s failed: %s\\n\", method, curl_easy_strerror(result));\n", ctx->out);
    fputs("    exit(1);\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static ", ctx->out);
    codegen_emit_type_name(ctx, response_type);
    fputs(" py4_http_request_text(const char *method, const char *url, const char *request_body)\n{\n", ctx->out);
    fputs("    CURL *handle;\n", ctx->out);
    fputs("    CURLcode result;\n", ctx->out);
    fputs("    long status_code = 0;\n", ctx->out);
    fputs("    Py4HttpBuffer buffer;\n", ctx->out);
    fputs("    bool is_post;\n", ctx->out);
    fputs("    if (url == NULL) {\n", ctx->out);
    fputs("        py4_http_fail(\"http request url cannot be null\");\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    if (method == NULL) {\n", ctx->out);
    fputs("        py4_http_fail(\"http request method cannot be null\");\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    py4_http_global_init();\n", ctx->out);
    fputs("    handle = curl_easy_init();\n", ctx->out);
    fputs("    if (handle == NULL) {\n", ctx->out);
    fputs("        py4_http_fail(\"failed to create http request handle\");\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    py4_http_buffer_init(&buffer);\n", ctx->out);
    fputs("    is_post = strcmp(method, \"POST\") == 0;\n", ctx->out);
    fputs("    curl_easy_setopt(handle, CURLOPT_URL, url);\n", ctx->out);
    fputs("    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);\n", ctx->out);
    fputs("    curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 10L);\n", ctx->out);
    fputs("    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT, 10L);\n", ctx->out);
    fputs("    curl_easy_setopt(handle, CURLOPT_TIMEOUT, 30L);\n", ctx->out);
    fputs("    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, py4_http_write_body);\n", ctx->out);
    fputs("    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &buffer);\n", ctx->out);
    fputs("    if (is_post) {\n", ctx->out);
    fputs("        if (request_body == NULL) {\n", ctx->out);
    fputs("            curl_easy_cleanup(handle);\n", ctx->out);
    fputs("            free(buffer.data);\n", ctx->out);
    fputs("            py4_http_fail(\"http POST body cannot be null\");\n", ctx->out);
    fputs("        }\n", ctx->out);
    fputs("        curl_easy_setopt(handle, CURLOPT_POST, 1L);\n", ctx->out);
    fputs("        curl_easy_setopt(handle, CURLOPT_POSTFIELDS, request_body);\n", ctx->out);
    fputs("        curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, (long)strlen(request_body));\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    result = curl_easy_perform(handle);\n", ctx->out);
    fputs("    if (result != CURLE_OK) {\n", ctx->out);
    fputs("        curl_easy_cleanup(handle);\n", ctx->out);
    fputs("        free(buffer.data);\n", ctx->out);
    fputs("        py4_http_fail_curl_request(method, result);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    result = curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status_code);\n", ctx->out);
    fputs("    curl_easy_cleanup(handle);\n", ctx->out);
    fputs("    if (result != CURLE_OK) {\n", ctx->out);
    fputs("        free(buffer.data);\n", ctx->out);
    fputs("        py4_http_fail(\"failed to read http status code\");\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    return (", ctx->out);
    codegen_emit_type_name(ctx, response_type);
    fputs("){(int)status_code, buffer.data};\n", ctx->out);
    fputs("}\n\n", ctx->out);
}

int emit_native_http_function_definition(
    CodegenContext *ctx,
    const char *module_name,
    const ParseNode *name,
    const ParseNode *parameters,
    const char *c_name,
    ValueType return_type,
    ValueType first_param_type)
{
    if (module_name != NULL &&
        strcmp(module_name, "http") == 0 &&
        strcmp(name->value, "get") == 0 &&
        parameters->child_count == 1 &&
        first_param_type == TYPE_STR) {
        const char *url_name = parameters->children[0]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return py4_http_request_text(\"GET\", %s, NULL);\n", url_name);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "http") == 0 &&
        strcmp(name->value, "post") == 0 &&
        parameters->child_count == 2 &&
        first_param_type == TYPE_STR &&
        semantic_type_of(ctx->semantic, codegen_expect_child(parameters->children[1], 0, NODE_TYPE)) == TYPE_STR) {
        const char *url_name = parameters->children[0]->value;
        const char *body_name = parameters->children[1]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return py4_http_request_text(\"POST\", %s, %s);\n", url_name, body_name);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    return 0;
}
