#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>

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

static int run_process(char *const argv[])
{
    pid_t pid = fork();
    int status;

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        execvp(argv[0], argv);
        fprintf(stderr, "failed to execute '%s': %s\n", argv[0], strerror(errno));
        _exit(127);
    }

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return 1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return 1;
}

static int file_contains_line(const char *path, const char *needle)
{
    FILE *file = fopen(path, "r");
    char line[1024];

    if (file == NULL) {
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        if (strstr(line, needle) != NULL) {
            fclose(file);
            return 1;
        }
    }

    fclose(file);
    return 0;
}

static char *read_command_output_trimmed_lines(char *const argv[], char ***items_out, size_t *count_out)
{
    FILE *pipe;
    char command[4096];
    char line[2048];
    char **items = NULL;
    size_t count = 0;
    size_t capacity = 0;

    snprintf(command, sizeof(command), "\"%s\" \"%s\" \"%s\"",
        argv[0], argv[1], argv[2]);
    pipe = popen(command, "r");
    if (pipe == NULL) {
        perror("popen");
        return dup_string("failed to launch libcurl helper");
    }

    while (fgets(line, sizeof(line), pipe) != NULL) {
        size_t len = strlen(line);

        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (len == 0) {
            continue;
        }
        if (count == capacity) {
            size_t next_capacity = capacity == 0 ? 8 : capacity * 2;
            char **next_items = realloc(items, sizeof(char *) * next_capacity);

            if (next_items == NULL) {
                perror("realloc");
                pclose(pipe);
                return dup_string("out of memory while collecting libcurl flags");
            }
            items = next_items;
            capacity = next_capacity;
        }
        items[count++] = dup_string(line);
    }

    if (pclose(pipe) != 0) {
        size_t message_len = 64;
        char *message;

        for (size_t i = 0; i < count; i++) {
            message_len += strlen(items[i]) + 1;
        }
        message = malloc(message_len);
        if (message == NULL) {
            perror("malloc");
            return dup_string("failed to resolve libcurl build flags");
        }
        snprintf(message, message_len, "failed to resolve libcurl build flags");
        for (size_t i = 0; i < count; i++) {
            free(items[i]);
        }
        free(items);
        return message;
    }

    *items_out = items;
    *count_out = count;
    return NULL;
}

static char *resolve_libcurl_flags(char ***cflags_out, size_t *cflag_count_out, char ***libs_out, size_t *lib_count_out)
{
    char *cflag_argv[] = {"bash", "scripts/ensure_libcurl.sh", "--print-cflags", NULL};
    char *lib_argv[] = {"bash", "scripts/ensure_libcurl.sh", "--print-libs", NULL};
    char *error;

    error = read_command_output_trimmed_lines(cflag_argv, cflags_out, cflag_count_out);
    if (error != NULL) {
        return error;
    }

    error = read_command_output_trimmed_lines(lib_argv, libs_out, lib_count_out);
    if (error != NULL) {
        for (size_t i = 0; i < *cflag_count_out; i++) {
            free((*cflags_out)[i]);
        }
        free(*cflags_out);
        *cflags_out = NULL;
        *cflag_count_out = 0;
        return error;
    }

    return NULL;
}

static void free_flag_items(char **items, size_t count)
{
    if (items == NULL) {
        return;
    }
    for (size_t i = 0; i < count; i++) {
        free(items[i]);
    }
    free(items);
}

static FILE *open_output_file(const char *path)
{
    FILE *out = fopen(path, "w");

    if (out == NULL) {
        perror(path);
        exit(1);
    }
    return out;
}

static char *default_binary_output_path(const char *input_path)
{
    const char *base_name = strrchr(input_path, '/');
    size_t len;
    char *path;

    base_name = base_name == NULL ? input_path : base_name + 1;
    len = strlen(base_name);
    if (len >= 3 && strcmp(base_name + len - 3, ".p4") == 0) {
        len -= 3;
    }

    path = malloc(len + 1);
    if (path == NULL) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }

    memcpy(path, base_name, len);
    path[len] = '\0';
    return path;
}

static const char *backend_optimization_flag(const char *level)
{
    if (strcmp(level, "0") == 0) {
        return "-O0";
    }
    if (strcmp(level, "1") == 0) {
        return "-O1";
    }
    if (strcmp(level, "2") == 0) {
        return "-O2";
    }
    if (strcmp(level, "3") == 0) {
        return "-O3";
    }
    if (strcmp(level, "s") == 0) {
        return "-Os";
    }
    if (strcmp(level, "z") == 0) {
        return "-Oz";
    }
    return NULL;
}

static int compile_generated_c_file(
    const char *backend,
    const char *backend_optimization,
    const char *c_path,
    const char *output_path)
{
    char *compile_argv[64];
    char **http_cflags = NULL;
    char **http_libs = NULL;
    size_t http_cflag_count = 0;
    size_t http_lib_count = 0;
    size_t argc = 0;
    char *flag_error = NULL;

    if (file_contains_line(c_path, "#include <curl/curl.h>")) {
        flag_error = resolve_libcurl_flags(
            &http_cflags,
            &http_cflag_count,
            &http_libs,
            &http_lib_count);
        if (flag_error != NULL) {
            fprintf(stderr, "%s\n", flag_error);
            free(flag_error);
            return 1;
        }
    }

    compile_argv[argc++] = (char *)backend;
    if (backend_optimization != NULL) {
        compile_argv[argc++] = (char *)backend_optimization;
    }
    compile_argv[argc++] = "-std=c11";
    for (size_t i = 0; i < http_cflag_count; i++) {
        compile_argv[argc++] = http_cflags[i];
    }
    compile_argv[argc++] = "-x";
    compile_argv[argc++] = "c";
    compile_argv[argc++] = (char *)c_path;
    compile_argv[argc++] = "-x";
    compile_argv[argc++] = "none";
    compile_argv[argc++] = "runtime/vendor/cjson/cJSON.c";
    compile_argv[argc++] = "-o";
    compile_argv[argc++] = (char *)output_path;
    for (size_t i = 0; i < http_lib_count; i++) {
        compile_argv[argc++] = http_libs[i];
    }
    compile_argv[argc] = NULL;

    {
        int status = run_process(compile_argv);
        free_flag_items(http_cflags, http_cflag_count);
        free_flag_items(http_libs, http_lib_count);
        return status;
    }
}

static int emit_binary_program(
    const char *backend,
    const char *backend_optimization,
    const char *output_path,
    const LoadedProgram *program,
    const SemanticInfo *semantic)
{
    char temp_template[] = "/tmp/py4-build-XXXXXX";
    FILE *generated_c;
    int fd = mkstemp(temp_template);
    int status;

    if (fd < 0) {
        perror("mkstemp");
        return 1;
    }

    generated_c = fdopen(fd, "w");
    if (generated_c == NULL) {
        perror("fdopen");
        close(fd);
        unlink(temp_template);
        return 1;
    }

    emit_c_program(generated_c, program, semantic);
    if (fclose(generated_c) != 0) {
        perror("fclose");
        unlink(temp_template);
        return 1;
    }

    status = compile_generated_c_file(backend, backend_optimization, temp_template, output_path);
    if (status != 0) {
        fprintf(stderr, "backend compilation failed; kept generated C at %s\n", temp_template);
        return status;
    }

    unlink(temp_template);
    return 0;
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
        "\"%s\" --emit-c \"%s\" > \"%s\"",
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
    const char *output_path = NULL;
    const char *backend = "gcc";
    const char *backend_optimization = NULL;
    const char *requested_opt_level = NULL;
    int emit_c_output = 0;
    int release_mode = 0;
    int show_tokens = 0;
    int show_tree = 0;
    SemanticInfo *semantic;
    LoadedProgram *program;
    char *owned_output_path = NULL;
    int exit_code = 0;

    if (argc >= 2 && strcmp(argv[1], "test") == 0) {
        const char *test_path = argc >= 3 ? argv[2] : "tests/py4";
        return run_py4_tests(argv[0], test_path);
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--tokens") == 0) {
            show_tokens = 1;
        } else if (strcmp(argv[i], "--tree") == 0) {
            show_tree = 1;
        } else if (strcmp(argv[i], "--emit-c") == 0) {
            emit_c_output = 1;
        } else if (strcmp(argv[i], "--release") == 0) {
            release_mode = 1;
        } else if (strcmp(argv[i], "--opt-level") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--opt-level requires one of: 0, 1, 2, 3, s, z\n");
                return 1;
            }
            requested_opt_level = argv[++i];
        } else if (strcmp(argv[i], "--compile") == 0) {
            /* Compile is now the default mode; keep this as an explicit alias. */
        } else if (strcmp(argv[i], "--backend") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "--backend requires a compiler name\n");
                return 1;
            }
            backend = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s requires a path\n", argv[i]);
                return 1;
            }
            output_path = argv[++i];
        } else {
            input_path = argv[i];
        }
    }

    if (backend[0] == '\0') {
        fprintf(stderr, "backend name cannot be empty\n");
        return 1;
    }
    if (requested_opt_level != NULL) {
        backend_optimization = backend_optimization_flag(requested_opt_level);
        if (backend_optimization == NULL) {
            fprintf(stderr, "unsupported --opt-level '%s'; expected one of: 0, 1, 2, 3, s, z\n",
                requested_opt_level);
            return 1;
        }
    } else if (release_mode) {
        backend_optimization = "-O2";
    }

    program = load_program_from_entry(input_path, show_tokens);
    if (show_tree) {
        print_tree(program->emission_root, 0);
    }

    semantic = analyze_program(program);
    if (emit_c_output) {
        if (output_path != NULL) {
            FILE *out = open_output_file(output_path);
            emit_c_program(out, program, semantic);
            if (fclose(out) != 0) {
                perror("fclose");
                exit_code = 1;
            }
        } else {
            emit_c_program(stdout, program, semantic);
        }
    } else {
        if (output_path == NULL) {
            owned_output_path = default_binary_output_path(input_path);
            output_path = owned_output_path;
        }
        exit_code = emit_binary_program(backend, backend_optimization, output_path, program, semantic);
    }

    free_semantic_info(semantic);
    free_loaded_program(program);
    free(owned_output_path);
    return exit_code;
}
