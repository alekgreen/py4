CC := gcc
CFLAGS := -Wall -Wextra -Wpedantic -std=c11 \
	-Itranspiler \
	-Itranspiler/frontend \
	-Itranspiler/semantic \
	-Itranspiler/codegen \
	-Itranspiler/codegen/native
TARGET := py4
FRONTEND_SRC := \
	transpiler/frontend/token.c \
	transpiler/frontend/lexer.c \
	transpiler/frontend/parse.c \
	transpiler/frontend/parse_expr.c \
	transpiler/frontend/module_loader.c

SEMANTIC_SRC := \
	transpiler/semantic/semantic_types.c \
	transpiler/semantic/semantic_symbols.c \
	transpiler/semantic/semantic_resolve.c \
	transpiler/semantic/semantic_expr.c \
	transpiler/semantic/semantic_analysis.c

CODEGEN_SRC := \
	transpiler/codegen/codegen_list.c \
	transpiler/codegen/codegen_types.c \
	transpiler/codegen/codegen_expr.c \
	transpiler/codegen/codegen_runtime.c \
	transpiler/codegen/codegen_emit.c \
	transpiler/codegen/codegen_functions.c \
	transpiler/codegen/codegen_statements.c \
	transpiler/codegen/native/codegen_native.c \
	transpiler/codegen/native/codegen_native_math.c \
	transpiler/codegen/native/codegen_native_strings.c \
	transpiler/codegen/native/codegen_native_chars.c \
	transpiler/codegen/native/codegen_native_io.c \
	transpiler/codegen/native/codegen_native_json.c

PY4_SRC := transpiler/main.c $(FRONTEND_SRC) $(SEMANTIC_SRC) $(CODEGEN_SRC)

INPUT ?= examples/functions.p4
OUTPUT ?= out.c
RUN_TARGET ?= out
GENERATED_RUNTIME_SRC := runtime/vendor/cjson/cJSON.c

.PHONY: all transpile run test clean

all: $(TARGET)

$(TARGET): $(PY4_SRC)
	$(CC) $(CFLAGS) $(PY4_SRC) -o $(TARGET)

transpile: $(TARGET)
	./$(TARGET) $(INPUT) > $(OUTPUT)

run: transpile
	$(CC) -std=c11 $(OUTPUT) $(GENERATED_RUNTIME_SRC) -o $(RUN_TARGET)
	$(if $(filter /%,$(RUN_TARGET)),$(RUN_TARGET),./$(RUN_TARGET))

test: $(TARGET)
	bash tests/run_tests.sh

clean:
	rm -f $(TARGET) $(OUTPUT) $(RUN_TARGET)
