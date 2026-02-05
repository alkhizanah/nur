ifeq ($(OS),Windows_NT)
EXE = .exe
RM = rm -rf
MKDIR = mkdir -p
CP = cp -af
LN = ln -sf
INSTALL = install
else
EXE =
RM = rm -rf
MKDIR = mkdir -p
CP = cp -af
LN = ln -sf
INSTALL = install
endif

CFLAGS = -Wall -Wextra -fPIC
TARGET = nur$(EXE)
LIBS = -lm

SRCS = src/main.c \
	   src/ast.c \
	   src/compiler.c \
	   src/lexer.c \
	   src/main.c \
	   src/fs.c \
	   src/parser.c \
	   src/source_location.c \
	   src/vm.c \
	   src/vm_gc.c \
	   src/vm_builtins.c \
	   src/vm_map.c \
	   src/vm_util.c

OBJ_DIR = build
OBJS = $(patsubst %.c, $(OBJ_DIR)/%.o, $(SRCS))

# Installation paths
PREFIX ?= /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/share/man
SHAREDIR = $(PREFIX)/share/zenc
INCLUDEDIR = $(PREFIX)/include/zenc

# Default target
all: $(TARGET)

# Link
$(TARGET): $(OBJS)
	@$(MKDIR) $(dir $@)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)
	@echo "=> Build complete: $(TARGET)"

# Compile
$(OBJ_DIR)/%.o: %.c
	@$(MKDIR) $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Run
run: $(TARGET)
	@$(TARGET) $(ARGS)

# Clean
clean:
	$(RM) $(OBJ_DIR) $(TARGET)
	@echo "=> Clean complete!"

.PHONY: all run clean
