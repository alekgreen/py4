#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen_native_internal.h"

void emit_native_http_runtime(CodegenContext *ctx)
{
    ValueType response_type = semantic_make_tuple_type((ValueType[]){TYPE_INT, TYPE_STR}, 2);
    ValueType request_headers_type = semantic_make_dict_type(TYPE_STR, TYPE_STR);
    ValueType response_header_item_type = semantic_make_tuple_type((ValueType[]){TYPE_STR, TYPE_STR}, 2);
    ValueType response_header_list_type = semantic_make_list_type(response_header_item_type);
    ValueType response_headers_type = semantic_make_tuple_type(
        (ValueType[]){TYPE_INT, TYPE_STR, response_header_list_type},
        3);
    char request_headers_struct_name[MAX_NAME_LEN];
    char response_header_item_name[MAX_NAME_LEN];
    char response_header_list_struct_name[MAX_NAME_LEN];
    char response_header_list_prefix[MAX_NAME_LEN];

    snprintf(request_headers_struct_name, sizeof(request_headers_struct_name), "%s",
        codegen_dict_struct_name(request_headers_type));
    codegen_build_tuple_base_name(
        response_header_item_name,
        sizeof(response_header_item_name),
        response_header_item_type);
    snprintf(response_header_list_struct_name, sizeof(response_header_list_struct_name), "%s",
        codegen_list_struct_name(response_header_list_type));
    snprintf(response_header_list_prefix, sizeof(response_header_list_prefix), "%s",
        codegen_list_runtime_prefix(response_header_list_type));

    fputs("typedef struct Py4HttpBuffer {\n", ctx->out);
    fputs("    char *data;\n", ctx->out);
    fputs("    size_t length;\n", ctx->out);
    fputs("    size_t capacity;\n", ctx->out);
    fputs("    const char *url;\n", ctx->out);
    fputs("} Py4HttpBuffer;\n\n", ctx->out);

    fprintf(ctx->out, "typedef struct Py4HttpHeaderCapture {\n");
    fprintf(ctx->out, "    %s *headers;\n", response_header_list_struct_name);
    fprintf(ctx->out, "    const char *url;\n");
    fprintf(ctx->out, "} Py4HttpHeaderCapture;\n\n");

    fprintf(ctx->out, "typedef struct Py4HttpResult {\n");
    fprintf(ctx->out, "    int status_code;\n");
    fprintf(ctx->out, "    char *body;\n");
    fprintf(ctx->out, "    %s *response_headers;\n", response_header_list_struct_name);
    fprintf(ctx->out, "} Py4HttpResult;\n\n");

    fputs("static int py4_http_initialized = 0;\n\n", ctx->out);

    fputs("static void py4_http_fail(const char *message)\n{\n", ctx->out);
    fputs("    fprintf(stderr, \"Runtime error: %s\\n\", message);\n", ctx->out);
    fputs("    exit(1);\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static void py4_http_fail_with_url(const char *message, const char *url)\n{\n", ctx->out);
    fputs("    if (url != NULL) {\n", ctx->out);
    fputs("        fprintf(stderr, \"Runtime error: %s for %s\\n\", message, url);\n", ctx->out);
    fputs("    } else {\n", ctx->out);
    fputs("        fprintf(stderr, \"Runtime error: %s\\n\", message);\n", ctx->out);
    fputs("    }\n", ctx->out);
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

    fputs("static void py4_http_buffer_init(Py4HttpBuffer *buffer, const char *url)\n{\n", ctx->out);
    fputs("    buffer->data = malloc(1);\n", ctx->out);
    fputs("    if (buffer->data == NULL) {\n", ctx->out);
    fputs("        perror(\"malloc\");\n", ctx->out);
    fputs("        exit(1);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    buffer->data[0] = '\\0';\n", ctx->out);
    fputs("    buffer->length = 0;\n", ctx->out);
    fputs("    buffer->capacity = 1;\n", ctx->out);
    fputs("    buffer->url = url;\n", ctx->out);
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
    fputs("        py4_http_fail_with_url(\"http response body contains NUL byte\", buffer->url);\n", ctx->out);
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

    fputs("static void py4_http_fail_curl_request(const char *method, const char *url, CURLcode result)\n{\n", ctx->out);
    fputs("    if (result == CURLE_URL_MALFORMAT || result == CURLE_UNSUPPORTED_PROTOCOL) {\n", ctx->out);
    fputs("        py4_http_fail_with_url(\"malformed http URL\", url);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    if (result == CURLE_COULDNT_CONNECT || result == CURLE_COULDNT_RESOLVE_HOST || result == CURLE_COULDNT_RESOLVE_PROXY) {\n", ctx->out);
    fputs("        py4_http_fail_with_url(\"http connection failed\", url);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    if (result == CURLE_OPERATION_TIMEDOUT) {\n", ctx->out);
    fputs("        py4_http_fail_with_url(\"http request timed out\", url);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    fprintf(stderr, \"Runtime error: http %s failed for %s: %s\\n\", method, url, curl_easy_strerror(result));\n", ctx->out);
    fputs("    exit(1);\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fprintf(ctx->out, "static char *py4_http_copy_slice(const char *start, size_t length)\n{\n");
    fputs("    char *copy = malloc(length + 1);\n", ctx->out);
    fputs("    if (copy == NULL) {\n", ctx->out);
    fputs("        perror(\"malloc\");\n", ctx->out);
    fputs("        exit(1);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    memcpy(copy, start, length);\n", ctx->out);
    fputs("    copy[length] = '\\0';\n", ctx->out);
    fputs("    return copy;\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fprintf(ctx->out, "static void py4_http_clear_response_headers(%s *headers)\n{\n", response_header_list_struct_name);
    fputs("    if (headers == NULL) {\n", ctx->out);
    fputs("        return;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    for (size_t i = 0; i < headers->len; i++) {\n", ctx->out);
    fputs("        free((void *)headers->items[i].item0);\n", ctx->out);
    fputs("        free((void *)headers->items[i].item1);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    headers->len = 0;\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fprintf(ctx->out, "static void py4_http_dispose_response_headers(%s *headers)\n{\n", response_header_list_struct_name);
    fputs("    if (headers == NULL) {\n", ctx->out);
    fputs("        return;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    py4_http_clear_response_headers(headers);\n", ctx->out);
    fprintf(ctx->out, "    %s_decref(headers);\n", response_header_list_prefix);
    fputs("}\n\n", ctx->out);

    fprintf(ctx->out, "static size_t py4_http_write_headers(char *ptr, size_t size, size_t nmemb, void *userdata)\n{\n");
    fputs("    Py4HttpHeaderCapture *capture = userdata;\n", ctx->out);
    fputs("    size_t total = size * nmemb;\n", ctx->out);
    fputs("    size_t line_len = total;\n", ctx->out);
    fputs("    size_t value_len;\n", ctx->out);
    fputs("    const char *colon;\n", ctx->out);
    fputs("    const char *value_start;\n", ctx->out);
    fprintf(ctx->out, "    %s entry;\n", response_header_item_name);
    fputs("    if (total == 0) {\n", ctx->out);
    fputs("        return 0;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    while (line_len > 0 && (ptr[line_len - 1] == '\\r' || ptr[line_len - 1] == '\\n')) {\n", ctx->out);
    fputs("        line_len--;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    if (line_len == 0) {\n", ctx->out);
    fputs("        return total;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    if (line_len >= 5 && strncmp(ptr, \"HTTP/\", 5) == 0) {\n", ctx->out);
    fputs("        py4_http_clear_response_headers(capture->headers);\n", ctx->out);
    fputs("        return total;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    colon = memchr(ptr, ':', line_len);\n", ctx->out);
    fputs("    if (colon == NULL) {\n", ctx->out);
    fputs("        return total;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    value_start = colon + 1;\n", ctx->out);
    fputs("    while ((size_t)(value_start - ptr) < line_len && (*value_start == ' ' || *value_start == '\\t')) {\n", ctx->out);
    fputs("        value_start++;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    value_len = line_len - (size_t)(value_start - ptr);\n", ctx->out);
    fputs("    while (value_len > 0 && (value_start[value_len - 1] == ' ' || value_start[value_len - 1] == '\\t')) {\n", ctx->out);
    fputs("        value_len--;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    entry.item0 = py4_http_copy_slice(ptr, (size_t)(colon - ptr));\n", ctx->out);
    fputs("    entry.item1 = py4_http_copy_slice(value_start, value_len);\n", ctx->out);
    fprintf(ctx->out, "    %s_append(capture->headers, entry);\n", response_header_list_prefix);
    fputs("    return total;\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fprintf(ctx->out, "static struct curl_slist *py4_http_header_list_from_dict(%s *headers, const char *url)\n{\n", request_headers_struct_name);
    fputs("    struct curl_slist *result = NULL;\n", ctx->out);
    fputs("    if (headers == NULL) {\n", ctx->out);
    fputs("        py4_http_fail_with_url(\"http request headers cannot be null\", url);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    for (size_t i = 0; i < headers->len; i++) {\n", ctx->out);
    fputs("        size_t header_len = strlen(headers->keys[i]) + strlen(headers->values[i]) + 3;\n", ctx->out);
    fputs("        char *header_line = malloc(header_len);\n", ctx->out);
    fputs("        struct curl_slist *next;\n", ctx->out);
    fputs("        if (header_line == NULL) {\n", ctx->out);
    fputs("            curl_slist_free_all(result);\n", ctx->out);
    fputs("            perror(\"malloc\");\n", ctx->out);
    fputs("            exit(1);\n", ctx->out);
    fputs("        }\n", ctx->out);
    fputs("        snprintf(header_line, header_len, \"%s: %s\", headers->keys[i], headers->values[i]);\n", ctx->out);
    fputs("        next = curl_slist_append(result, header_line);\n", ctx->out);
    fputs("        free(header_line);\n", ctx->out);
    fputs("        if (next == NULL) {\n", ctx->out);
    fputs("            curl_slist_free_all(result);\n", ctx->out);
    fputs("            py4_http_fail_with_url(\"failed to build http request headers\", url);\n", ctx->out);
    fputs("        }\n", ctx->out);
    fputs("        result = next;\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    return result;\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fprintf(ctx->out, "static Py4HttpResult py4_http_request_raw(const char *method, const char *url, const char *request_body, %s *request_headers, int timeout_ms, bool capture_response_headers)\n{\n", request_headers_struct_name);
    fputs("    CURL *handle;\n", ctx->out);
    fputs("    CURLcode result;\n", ctx->out);
    fputs("    long status_code = 0;\n", ctx->out);
    fputs("    Py4HttpBuffer buffer;\n", ctx->out);
    fputs("    struct curl_slist *header_list = NULL;\n", ctx->out);
    fputs("    Py4HttpResult request_result = {0, NULL, NULL};\n", ctx->out);
    fputs("    Py4HttpHeaderCapture response_capture = {0};\n", ctx->out);
    fputs("    bool is_post;\n", ctx->out);
    fputs("    if (url == NULL) {\n", ctx->out);
    fputs("        py4_http_fail(\"http request url cannot be null\");\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    if (method == NULL) {\n", ctx->out);
    fputs("        py4_http_fail(\"http request method cannot be null\");\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    if (timeout_ms <= 0) {\n", ctx->out);
    fputs("        py4_http_fail_with_url(\"http timeout must be positive\", url);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    py4_http_global_init();\n", ctx->out);
    fputs("    handle = curl_easy_init();\n", ctx->out);
    fputs("    if (handle == NULL) {\n", ctx->out);
    fputs("        py4_http_fail(\"failed to create http request handle\");\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    py4_http_buffer_init(&buffer, url);\n", ctx->out);
    fputs("    request_result.body = buffer.data;\n", ctx->out);
    fputs("    is_post = strcmp(method, \"POST\") == 0;\n", ctx->out);
    fputs("    curl_easy_setopt(handle, CURLOPT_URL, url);\n", ctx->out);
    fputs("    curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);\n", ctx->out);
    fputs("    curl_easy_setopt(handle, CURLOPT_MAXREDIRS, 10L);\n", ctx->out);
    fputs("    curl_easy_setopt(handle, CURLOPT_CONNECTTIMEOUT_MS, (long)timeout_ms);\n", ctx->out);
    fputs("    curl_easy_setopt(handle, CURLOPT_TIMEOUT_MS, (long)timeout_ms);\n", ctx->out);
    fputs("    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, py4_http_write_body);\n", ctx->out);
    fputs("    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &buffer);\n", ctx->out);
    fputs("    if (request_headers != NULL) {\n", ctx->out);
        fputs("        header_list = py4_http_header_list_from_dict(request_headers, url);\n", ctx->out);
        fputs("        if (header_list != NULL) {\n", ctx->out);
            fputs("            curl_easy_setopt(handle, CURLOPT_HTTPHEADER, header_list);\n", ctx->out);
        fputs("        }\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    if (capture_response_headers) {\n", ctx->out);
    fprintf(ctx->out, "        response_capture.headers = %s_new();\n", response_header_list_prefix);
    fputs("        response_capture.url = url;\n", ctx->out);
    fputs("        curl_easy_setopt(handle, CURLOPT_HEADERFUNCTION, py4_http_write_headers);\n", ctx->out);
    fputs("        curl_easy_setopt(handle, CURLOPT_HEADERDATA, &response_capture);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    if (is_post) {\n", ctx->out);
        fputs("        if (request_body == NULL) {\n", ctx->out);
    fputs("            py4_http_dispose_response_headers(response_capture.headers);\n", ctx->out);
            fputs("            curl_slist_free_all(header_list);\n", ctx->out);
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
        fputs("        py4_http_dispose_response_headers(response_capture.headers);\n", ctx->out);
        fputs("        curl_slist_free_all(header_list);\n", ctx->out);
        fputs("        curl_easy_cleanup(handle);\n", ctx->out);
        fputs("        free(buffer.data);\n", ctx->out);
    fputs("        py4_http_fail_curl_request(method, url, result);\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    result = curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status_code);\n", ctx->out);
    fputs("    curl_slist_free_all(header_list);\n", ctx->out);
    fputs("    curl_easy_cleanup(handle);\n", ctx->out);
    fputs("    if (result != CURLE_OK) {\n", ctx->out);
        fputs("        py4_http_dispose_response_headers(response_capture.headers);\n", ctx->out);
        fputs("        free(buffer.data);\n", ctx->out);
        fputs("        py4_http_fail(\"failed to read http status code\");\n", ctx->out);
    fputs("    }\n", ctx->out);
    fputs("    request_result.status_code = (int)status_code;\n", ctx->out);
    fputs("    request_result.response_headers = response_capture.headers;\n", ctx->out);
    fputs("    return request_result;\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static ", ctx->out);
    codegen_emit_type_name(ctx, response_type);
    fprintf(ctx->out, " py4_http_request_text(const char *method, const char *url, const char *request_body, %s *request_headers, int timeout_ms)\n{\n", request_headers_struct_name);
    fputs("    Py4HttpResult request_result = py4_http_request_raw(method, url, request_body, request_headers, timeout_ms, false);\n", ctx->out);
    fputs("    return (", ctx->out);
    codegen_emit_type_name(ctx, response_type);
    fputs("){request_result.status_code, request_result.body};\n", ctx->out);
    fputs("}\n\n", ctx->out);

    fputs("static ", ctx->out);
    codegen_emit_type_name(ctx, response_headers_type);
    fprintf(ctx->out, " py4_http_request_text_with_response_headers(const char *method, const char *url, const char *request_body, %s *request_headers, int timeout_ms)\n{\n", request_headers_struct_name);
    fputs("    Py4HttpResult request_result = py4_http_request_raw(method, url, request_body, request_headers, timeout_ms, true);\n", ctx->out);
    fputs("    return (", ctx->out);
    codegen_emit_type_name(ctx, response_headers_type);
    fputs("){request_result.status_code, request_result.body, request_result.response_headers};\n", ctx->out);
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
        fprintf(ctx->out, "return py4_http_request_text(\"GET\", %s, NULL, NULL, 30000);\n", url_name);
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
        fprintf(ctx->out, "return py4_http_request_text(\"POST\", %s, %s, NULL, 30000);\n", url_name, body_name);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "http") == 0 &&
        strcmp(name->value, "get_response_headers") == 0 &&
        parameters->child_count == 1 &&
        first_param_type == TYPE_STR) {
        const char *url_name = parameters->children[0]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return py4_http_request_text_with_response_headers(\"GET\", %s, NULL, NULL, 30000);\n", url_name);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "http") == 0 &&
        strcmp(name->value, "get") == 0 &&
        parameters->child_count == 2 &&
        first_param_type == TYPE_STR &&
        semantic_type_of(ctx->semantic, codegen_expect_child(parameters->children[1], 0, NODE_TYPE)) == TYPE_INT) {
        const char *url_name = parameters->children[0]->value;
        const char *timeout_name = parameters->children[1]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return py4_http_request_text(\"GET\", %s, NULL, NULL, %s);\n", url_name, timeout_name);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "http") == 0 &&
        strcmp(name->value, "post_response_headers") == 0 &&
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
        fprintf(ctx->out, "return py4_http_request_text_with_response_headers(\"POST\", %s, %s, NULL, 30000);\n", url_name, body_name);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "http") == 0 &&
        strcmp(name->value, "post") == 0 &&
        parameters->child_count == 3 &&
        first_param_type == TYPE_STR &&
        semantic_type_of(ctx->semantic, codegen_expect_child(parameters->children[1], 0, NODE_TYPE)) == TYPE_STR &&
        semantic_type_of(ctx->semantic, codegen_expect_child(parameters->children[2], 0, NODE_TYPE)) == TYPE_INT) {
        const char *url_name = parameters->children[0]->value;
        const char *body_name = parameters->children[1]->value;
        const char *timeout_name = parameters->children[2]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return py4_http_request_text(\"POST\", %s, %s, NULL, %s);\n", url_name, body_name, timeout_name);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "http") == 0 &&
        strcmp(name->value, "get") == 0 &&
        parameters->child_count == 2 &&
        first_param_type == TYPE_STR &&
        semantic_type_of(ctx->semantic, codegen_expect_child(parameters->children[1], 0, NODE_TYPE)) == semantic_make_dict_type(TYPE_STR, TYPE_STR)) {
        const char *url_name = parameters->children[0]->value;
        const char *headers_name = parameters->children[1]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return py4_http_request_text(\"GET\", %s, NULL, %s, 30000);\n", url_name, headers_name);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    if (module_name != NULL &&
        strcmp(module_name, "http") == 0 &&
        strcmp(name->value, "post") == 0 &&
        parameters->child_count == 3 &&
        first_param_type == TYPE_STR &&
        semantic_type_of(ctx->semantic, codegen_expect_child(parameters->children[1], 0, NODE_TYPE)) == TYPE_STR &&
        semantic_type_of(ctx->semantic, codegen_expect_child(parameters->children[2], 0, NODE_TYPE)) == semantic_make_dict_type(TYPE_STR, TYPE_STR)) {
        const char *url_name = parameters->children[0]->value;
        const char *body_name = parameters->children[1]->value;
        const char *headers_name = parameters->children[2]->value;

        codegen_emit_type_name(ctx, return_type);
        fprintf(ctx->out, " %s(", c_name);
        emit_parameter_list(ctx, parameters, 0);
        fputs(")\n{\n", ctx->out);
        ctx->indent_level++;
        codegen_emit_indent(ctx);
        fprintf(ctx->out, "return py4_http_request_text(\"POST\", %s, %s, %s, 30000);\n", url_name, body_name, headers_name);
        ctx->indent_level--;
        fputs("}\n\n", ctx->out);
        return 1;
    }

    return 0;
}
