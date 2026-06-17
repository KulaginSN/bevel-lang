CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11 -g -O0
INCLUDES = -Isrc

COMMON_SRCS = src/common/string.c src/common/arena.c src/common/utils.c src/common/error.c src/common/file.c
LEXER_SRCS = src/lexer/lexer.c src/lexer/keywords.c
PARSER_SRCS = src/parser/ast.c src/parser/parser.c src/parser/printer.c
SEMANTIC_SRCS = src/semantic/types.c src/semantic/symbols.c src/semantic/analyzer.c src/semantic/generics.c
IR_SRCS = src/ir/ir.c src/ir/builder.c src/ir/printer.c
OPTIMIZER_SRCS = src/optimizer/optimizer.c src/optimizer/constfold.c src/optimizer/dce.c src/optimizer/inline.c
CODEGEN_SRCS = src/codegen/codegen.c src/codegen/c_backend.c src/codegen/llvm_backend.c src/codegen/x86_backend.c

ALL_SRCS = $(COMMON_SRCS) $(LEXER_SRCS) $(PARSER_SRCS) $(SEMANTIC_SRCS) $(IR_SRCS) $(OPTIMIZER_SRCS) $(CODEGEN_SRCS)
ALL_OBJS = $(ALL_SRCS:.c=.o)

TARGET = bevel_test

all: $(TARGET)

$(TARGET): src/main.c $(ALL_OBJS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ src/main.c $(ALL_OBJS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(ALL_OBJS) $(TARGET)
	rm -f src/*.o src/common/*.o src/lexer/*.o src/parser/*.o src/semantic/*.o src/ir/*.o src/optimizer/*.o src/codegen/*.o
	rm -f output.c output.ll output.s program

test: $(TARGET)
	./$(TARGET) --target=c
	@echo ""
	@echo "=== Compiling generated C code ==="
	gcc output.c -o program && ./program && echo "✅ Program executed successfully with exit code $$?"

llvm: $(TARGET)
	./$(TARGET) --target=llvm
	@echo "✅ Generated LLVM IR in output.ll"

.PHONY: all clean test llvm