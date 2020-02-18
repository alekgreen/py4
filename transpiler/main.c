#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

#include "codegen.h"
#include "module_loader.h"
#include "parse.h"
#include "semantic.h"

static char *dup_string(const char *text)
{
    size_t len = strlen(text);
    char *copy = malloc(len + 1);

    if (copy == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }

    memcpy(copy, text, len + 1);
    return copy;
}

static int ends_with(const char *text, const char *suffix)
{
    size_t text_len = strlen(text);
    size_t suffix_len = strlen(suffix);

    if (suffix_len > text_len) {
        return 0;
    }

    return strcmp(text + text_len - suffix_len, suffix) == 0;
}

static int path_is_directory(const char *path)
{
    struct stat st;

    if (stat(path, &st) != 0) {
        return 0;
    }

    return S_ISDIR(st.st_mode);
}

static void append_test_path(char ***paths, int *count, int *capacity, const char *path)
{
    if (*count == *capacity) {
        int new_capacity = *capacity == 0 ? 8 : *capacity * 2;
        char **new_paths = realloc(*paths, sizeof(char *) * new_capacity);

        if (new_paths == NULL) {
            fprintf(stderr, "out of memory\n");
            exit(1);
        }

        *paths = new_paths;
        *capacity = new_capacity;
    }

    (*paths)[*count] = dup_string(path);
    *count = *count + 1;
}

static void collect_test_paths(const char *path, char ***paths, int *count, int *capacity)
{
    DIR *dir;
    struct dirent *entry;

    if (!path_is_directory(path)) {
        if (ends_with(path, ".p4")) {
            append_test_path(paths, count, capacity, path);
        }
        return;
    }

    dir = opendir(path);
    if (dir == NULL) {
        fprintf(stderr, "could not open test directory: %s\n", path);
        exit(1);
    }

    while ((entry = readdir(dir)) != NULL) {
        char child[1024];

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(child, sizeof(child), "%s/%s", path, entry->d_name);

        if (path_is_directory(child)) {
            collect_test_paths(child, paths, count, capacity);
        } else if (ends_with(entry->d_name, "_test.p4")) {
            append_test_path(paths, count, capacity, child);
        }
    }

    closedir(dir);
}

static int compare_paths(const void *left, const void *right)
{
    const char *left_path = *(const char *const *)left;
    const char *right_path = *(const char *const *)right;
    return strcmp(left_path, right_path);
}

static int run_command(const char *command)
{
    int status = system(command);

    if (status == -1) {
        return 1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return 1;
}

static int run_py4_test_file(const char *exe_path, const char *test_path, const char *tmp_dir)
{
    char generated_c[1024];
    char generated_bin[1024];
    char stdout_path[1024];
    char stderr_path[1024];
    char command[4096];
    const char *base_name = strrchr(test_path, '/');
    int status;

    base_name = base_name == NULL ? test_path : base_name + 1;

    snprintf(generated_c, sizeof(generated_c), "%s/%s.c", tmp_dir, base_name);
    snprintf(generated_bin, sizeof(generated_bin), "%s/%s.bin", tmp_dir, base_name);
    snprintf(stdout_path, sizeof(stdout_path), "%s/%s.stdout", tmp_dir, base_name);
    snprintf(stderr_path, sizeof(stderr_path), "%s/%s.stderr", tmp_dir, base_name);

    snprintf(command,
        sizeof(command),
        "\"%s\" \"%s\" > \"%s\"",
        exe_path,
        test_path,
        generated_c);
    status = run_command(command);
    if (status != 0) {
        printf("FAIL py4/%s: transpiler exited with failure\n", base_name);
        return 0;
    }

    snprintf(command,
        sizeof(command),
        "gcc -std=c11 \"%s\" runtime/vendor/cjson/cJSON.c -o \"%s\" > \"%s\" 2>&1",
        generated_c,
        generated_bin,
        stderr_path);
    status = run_command(command);
    if (status != 0) {
        FILE *log = fopen(stderr_path, "r");
        char line[512];

        printf("FAIL py4/%s: generated C did not compile\n", base_name);
        if (log != NULL) {
            while (fgets(line, sizeof(line), log) != NULL) {
                fputs(line, stdout);
            }
            fclose(log);
        }
        return 0;
    }

    snprintf(command,
        sizeof(command),
        "\"%s\" > \"%s\" 2> \"%s\"",
        generated_bin,
        stdout_path,
        stderr_path);
    status = run_command(command);
    if (status != 0) {
        FILE *log = fopen(stderr_path, "r");
        char line[512];

        printf("FAIL py4/%s: runtime failed\n", base_name);
        if (log != NULL) {
            while (fgets(line, sizeof(line), log) != NULL) {
                fputs(line, stdout);
            }
            fclose(log);
        }
        return 0;
    }

    printf("PASS py4/%s\n", base_name);
    return 1;
}

static int run_py4_tests(const char *exe_path, const char *path)
{
    char **paths = NULL;
    int count = 0;
    int capacity = 0;
    int passed = 0;
    int failed = 0;
    char tmp_dir[256];
    long seed;

    collect_test_paths(path, &paths, &count, &capacity);
    qsort(paths, (size_t)count, sizeof(char *), compare_paths);

    seed = (long)time(NULL);
    snprintf(tmp_dir, sizeof(tmp_dir), "/tmp/py4-tests-%ld-%d", seed, (int)getpid());
    if (mkdir(tmp_dir, 0700) != 0) {
        fprintf(stderr, "could not create temporary directory\n");
        return 1;
    }

    for (int i = 0; i < count; i++) {
        if (run_py4_test_file(exe_path, paths[i], tmp_dir)) {
            passed++;
        } else {
            failed++;
        }
        free(paths[i]);
    }
    free(paths);

    printf("\n%d passed, %d failed\n", passed, failed);
    return failed == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    const char *input_path = "examples/transpiler_example0.p4";
    int show_tokens = 0;
    int show_tree = 0;
    SemanticInfo *semantic;
    LoadedProgram *program;

    if (argc >= 2 && strcmp(argv[1], "test") == 0) {
        const char *test_path = argc >= 3 ? argv[2] : "tests/py4";
        return run_py4_tests(argv[0], test_path);
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0) {
            show_tokens = 1;
        } else if (strcmp(argv[i], "--tree") == 0) {
            show_tree = 1;
        } else {
            input_path = argv[i];
        }
    }

    program = load_program_from_entry(input_path, show_tokens);
    if (show_tree) {
        print_tree(program->emission_root, 0);
    }

    semantic = analyze_program(program);
    emit_c_program(stdout, program, semantic);

    free_semantic_info(semantic);
    free_loaded_program(program);
    return 0;
}
