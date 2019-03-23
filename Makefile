CC := gcc
CFLAGS := -Wall -Wextra -Wpedantic -std=c11
TARGET := py4
PY4_SRC := transpiler/main.c transpiler/lexer.c transpiler/parse.c transpiler/token.c transpiler/codegen.c

INPUT ?= examples/functions.p4
OUTPUT ?= out.c
RUN_TARGET ?= out

.PHONY: all transpile run clean

all: $(TARGET)

$(TARGET): $(PY4_SRC)
	$(CC) $(CFLAGS) $(PY4_SRC) -o $(TARGET)

transpile: $(TARGET)
	./$(TARGET) $(INPUT) > $(OUTPUT)

run: transpile
	$(CC) -std=c11 $(OUTPUT) -o $(RUN_TARGET)
	$(RUN_TARGET)

clean:
	rm -f $(TARGET) $(OUTPUT) $(RUN_TARGET)
