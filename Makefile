CC := gcc
CPPFLAGS := \
	-Itranspiler \
	-Itranspiler/frontend \
	-Itranspiler/semantic \
	-Itranspiler/codegen \
	-Itranspiler/codegen/native
CFLAGS := -Wall -Wextra -Wpedantic -std=c11
TARGET := py4
MAIN_SRC := transpiler/main.c
FRONTEND_SRC := $(sort $(wildcard transpiler/frontend/*.c))
SEMANTIC_SRC := $(sort $(wildcard transpiler/semantic/*.c))
CODEGEN_SRC := $(sort $(wildcard transpiler/codegen/*.c))
CODEGEN_NATIVE_SRC := $(sort $(wildcard transpiler/codegen/native/*.c))
PY4_SRC := $(MAIN_SRC) $(FRONTEND_SRC) $(SEMANTIC_SRC) $(CODEGEN_SRC) $(CODEGEN_NATIVE_SRC)
BUILD_DIR := build
OBJ_DIR := $(BUILD_DIR)/obj
PY4_OBJ := $(patsubst %.c,$(OBJ_DIR)/%.o,$(PY4_SRC))
PY4_DEP := $(PY4_OBJ:.o=.d)

INPUT ?= examples/functions.p4
OUTPUT ?= out.c
RUN_TARGET ?= out
GENERATED_RUNTIME_SRC := runtime/vendor/cjson/cJSON.c

.PHONY: all transpile run test clean ensure-libcurl print-libcurl-cflags print-libcurl-libs

all: $(TARGET)

$(TARGET): $(PY4_OBJ)
	$(CC) $(PY4_OBJ) -o $(TARGET)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

transpile: $(TARGET)
	./$(TARGET) --emit-c $(INPUT) > $(OUTPUT)

run: $(TARGET)
	./$(TARGET) -o $(RUN_TARGET) $(INPUT)
	$(if $(filter /%,$(RUN_TARGET)),$(RUN_TARGET),./$(RUN_TARGET))

test: $(TARGET)
	bash tests/run_tests.sh

ensure-libcurl:
	bash scripts/ensure_libcurl.sh

print-libcurl-cflags:
	bash scripts/ensure_libcurl.sh --print-cflags

print-libcurl-libs:
	bash scripts/ensure_libcurl.sh --print-libs

clean:
	rm -rf $(TARGET) $(BUILD_DIR) $(OUTPUT) $(RUN_TARGET)

-include $(PY4_DEP)
