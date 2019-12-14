#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runtime/vendor/cjson/cJSON.h"

static void fail(const char *message)
{
    fprintf(stderr, "%s\n", message);
    exit(1);
}

static char *read_file(const char *path)
{
    FILE *handle = fopen(path, "rb");
    long size;
    char *buffer;

    if (handle == NULL) {
        fail("failed to open benchmark input");
    }

    if (fseek(handle, 0, SEEK_END) != 0) {
        fail("failed to seek benchmark input");
    }

    size = ftell(handle);
    if (size < 0) {
        fail("failed to size benchmark input");
    }

    if (fseek(handle, 0, SEEK_SET) != 0) {
        fail("failed to rewind benchmark input");
    }

    buffer = (char *)malloc((size_t)size + 1);
    if (buffer == NULL) {
        fail("failed to allocate benchmark input");
    }

    if (fread(buffer, 1, (size_t)size, handle) != (size_t)size) {
        fail("failed to read benchmark input");
    }

    fclose(handle);
    buffer[size] = '\0';
    return buffer;
}

static cJSON *require_object_item(cJSON *object, const char *name)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
    if (item == NULL) {
        fail("missing object item");
    }
    return item;
}

int main(void)
{
    char *text = read_file("benchmarks/json_roundtrip_input.json");
    int total = 0;

    for (int i = 0; i < 2000; i++) {
        cJSON *root = cJSON_Parse(text);
        cJSON *owner;
        cJSON *metrics;
        cJSON *metric0;
        cJSON *team;
        cJSON *revision;
        char *encoded;

        if (root == NULL) {
            fail("failed to parse benchmark json");
        }

        owner = require_object_item(root, "owner");
        metrics = require_object_item(root, "metrics");
        metric0 = cJSON_GetArrayItem(metrics, 0);
        team = require_object_item(owner, "team");
        revision = require_object_item(root, "revision");

        total += require_object_item(root, "id")->valueint;
        total += revision->valueint;
        total += (int)strlen(team->valuestring);
        total += cJSON_GetArraySize(require_object_item(root, "flags"));
        total += cJSON_GetArraySize(metrics);
        total += (int)strlen(require_object_item(metric0, "name")->valuestring);
        total += cJSON_GetArraySize(require_object_item(metric0, "tags"));

        revision->valueint += 1;
        revision->valuedouble = (double)revision->valueint;

        encoded = cJSON_PrintUnformatted(root);
        if (encoded == NULL) {
            fail("failed to encode benchmark json");
        }

        total += (int)strlen(encoded);
        free(encoded);
        cJSON_Delete(root);
    }

    free(text);
    printf("%d\n", total);
    return 0;
}
