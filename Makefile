CC := gcc
CFLAGS := -Wall -Wextra -Wpedantic -std=c11
TARGET := py4
PY4_SRC := transpiler/main.c transpiler/lexer.c transpiler/parse.c transpiler/parse_expr.c transpiler/token.c transpiler/module_loader.c transpiler/codegen_list.c transpiler/codegen_types.c transpiler/codegen_expr.c transpiler/codegen_runtime.c transpiler/codegen_emit.c transpiler/codegen_functions.c transpiler/semantic_types.c transpiler/semantic_symbols.c transpiler/semantic_resolve.c transpiler/semantic_expr.c transpiler/semantic_analysis.c

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
