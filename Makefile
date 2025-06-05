# Generic C Server Framework Makefile
# Original source: https://www.throwtheswitch.org/build/make
# Modified by: Taylor Marrion

ifeq ($(OS),Windows_NT)
  ifeq ($(shell uname -s),) # not in a bash-like shell
	CLEANUP = del /F /Q
	MKDIR = mkdir
  else # in a bash-like shell, like msys
	CLEANUP = rm -rf
	MKDIR = mkdir -p
  endif
	TARGET_EXTENSION=exe
else
	CLEANUP = rm -rf
	MKDIR = mkdir -p
	TARGET_EXTENSION=out
endif

### Configuration ###
SERVER_THREADED = server_threaded
SERVER_SELECT   = server_select
CLIENT          = client

# Define directories
BIN_DIR = bin
BUILD_DIR = build
TEST_BUILD_DIR = $(BUILD_DIR)/test
INC_DIR = include
SRC_DIR = src
TEST_DIR = test/c_unity
UNITY_DIR = unity/src

# Define paths
DATA_DIR = ./data
#USER_DB = $(DATA_DIR)/users.db

# Compiler
CC = gcc

# Base Flags
CFLAGS = -g -std=c99 -Wall -Wextra -Wpedantic -Waggregate-return -Wwrite-strings -Wvla -Wfloat-equal
CFLAGS += -I$(INC_DIR) -I$(UNITY_DIR)
LDFLAGS = -pthread
ASANFLAGS = -fsanitize=address -fno-omit-frame-pointer


# Main programs
SERVER_THREADED_SRC := $(SRC_DIR)/$(SERVER_THREADED).c
SERVER_SELECT_SRC   := $(SRC_DIR)/$(SERVER_SELECT).c
CLIENT_SRC          := $(SRC_DIR)/$(CLIENT).c

# All .c files in src/ excluding any server/client main programs
UTIL_SOURCES = $(filter-out $(SERVER_THREADED_SRC) $(SERVER_SELECT_SRC) $(CLIENT_SRC), $(wildcard $(SRC_DIR)/*.c))
UTIL_OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(UTIL_SOURCES))

# Server and client object rules (explicit)
SERVER_THREADED_OBJ := $(BUILD_DIR)/$(SERVER_THREADED).o
SERVER_SELECT_OBJ   := $(BUILD_DIR)/$(SERVER_SELECT).o
CLIENT_OBJ          := $(BUILD_DIR)/$(CLIENT).o

SERVER_THREADED_BIN := $(BIN_DIR)/$(SERVER_THREADED)
SERVER_SELECT_BIN   := $(BIN_DIR)/$(SERVER_SELECT)
CLIENT_BIN          := $(BIN_DIR)/$(CLIENT)

### Test Files
TEST_SOURCES = $(filter-out $(TEST_DIR)/test_runner.c, $(wildcard $(TEST_DIR)/test_*.c))
TEST_SOURCES += $(TEST_DIR)/test_runner.c
TEST_OBJECTS = $(patsubst $(TEST_DIR)/%.c, $(TEST_BUILD_DIR)/%.o, $(TEST_SOURCES))
UNITY_OBJECT = $(TEST_BUILD_DIR)/unity.o
TEST_EXECUTABLE = $(BIN_DIR)/test_generic_framework

# Define targets
.PHONY: all clean server_threaded server_select client run memcheck docs

# Target: all
all: server_threaded client
	@echo "Build complete."

# Server Threaded
server_threaded: $(SERVER_THREADED_BIN)
	@echo "Built threaded server."

$(SERVER_THREADED_BIN): $(SERVER_THREADED_OBJ) $(UTIL_OBJECTS) | $(BIN_DIR)
	@echo "Linking $@..."
	$(CC) $^ -o $@ $(LDFLAGS)

# Server Select
server_select: $(SERVER_SELECT_BIN)
	@echo "Built select-based server."

$(SERVER_SELECT_BIN): $(SERVER_SELECT_OBJ) $(UTIL_OBJECTS) | $(BIN_DIR)
	@echo "Linking $@..."
	$(CC) $^ -o $@ $(LDFLAGS)

# Client
client: $(CLIENT_BIN)
	@echo "Built client."

$(CLIENT_BIN): $(CLIENT_OBJ) $(UTIL_OBJECTS) | $(BIN_DIR)
	@echo "Linking $@..."
	$(CC) $^ -o $@ $(LDFLAGS)

# Object compilation rule
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Directory creation
$(BUILD_DIR):
	@$(MKDIR) $(BUILD_DIR)

$(BIN_DIR):
	@$(MKDIR) $(BIN_DIR)

# Run (default = server_threaded + client)
run: all
	@echo "Running server_threaded..."
	@./$(SERVER_THREADED_BIN)

# Target: reset-db
#reset-db:
#	@echo "Reinitializing $(USER_DB)..."
#	@mkdir -p $(DATA_DIR)
#	echo "username,password,role" > $(USER_DB)
#	echo "admin,password,1" >> $(USER_DB)
#	echo "guest,password,0" >> $(USER_DB)

# Target: clean
clean:
	@echo "Cleaning up build artifacts..."
	$(CLEANUP) $(BUILD_DIR)/*
	$(CLEANUP) $(TEST_BUILD_DIR)/*
	$(CLEANUP) $(BIN_DIR)/*
#	$(MAKE) reset-db


# Target: Run Tests
test: clean $(TEST_EXECUTABLE)
	@echo "Running unit tests..."
	./$(TEST_EXECUTABLE)

$(TEST_EXECUTABLE): $(TEST_OBJECTS) $(UNITY_OBJECT) $(UTIL_SOURCES)
	@echo "Linking test executable..."
	@$(MKDIR) $(BIN_DIR)
	$(CC) $(TEST_OBJECTS) $(UNITY_OBJECT) $(UTIL_OBJECTS) -o $@ $(LDFLAGS)

$(TEST_BUILD_DIR)/%.o: $(TEST_DIR)/%.c | $(TEST_BUILD_DIR)
	@echo "Compiling test $<..."
	@$(MKDIR) $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -DTEST -c $< -o $@

# Compile Unity
$(UNITY_OBJECT): $(UNITY_DIR)/unity.c | $(TEST_BUILD_DIR)
	@echo "Compiling Unity framework..."
	$(CC) $(CFLAGS) -c $< -o $@

# Ensure Test Build Directory Exists
$(TEST_BUILD_DIR):
	@$(MKDIR) $(TEST_BUILD_DIR)


memcheck: clean $(UTIL_OBJECTS)
	@echo "Building test executable with AddressSanitizer..."
	@$(MKDIR) $(BIN_DIR)
	@$(MKDIR) $(TEST_BUILD_DIR)
	$(CC) $(ASANFLAGS) $(CFLAGS) \
		$(TEST_OBJECTS) \
		$(UNITY_OBJECT) \
		$(UTIL_OBJECTS) -o $(BIN_DIR)/memcheck.out $(LDFLAGS)
	@echo "Running memcheck.out..."
	@./$(BIN_DIR)/memcheck.out
	@echo "Memory check passed"


# Target: docs
docs: Doxyfile
	@if ! command -v doxygen >/dev/null 2>&1; then \
	    echo "Warning: Doxygen is not installed. Skipping documentation generation."; \
	    exit 0; \
	fi
	@echo "Generating documentation..."
	doxygen Doxyfile > /dev/null 2>&1
	@echo "Generating PDF documentation..."
	@cd docs/latex && pdflatex -interaction=nonstopmode refman.tex > /dev/null 2>&1
	@echo "Copying and renaming PDF..."
	@cp docs/latex/refman.pdf docs/project_docs.pdf
	@echo "PDF documentation available at: docs/project_docs.pdf"

# Generate Doxyfile if it doesn't exist
Doxyfile:
	@if ! command -v doxygen >/dev/null 2>&1; then \
	    echo "Error: Doxygen is not installed. Cannot generate Doxyfile."; \
	    exit 1; \
	fi
	@echo "Generating Doxyfile..."
	doxygen -g
	@sed -i 's|^PROJECT_NAME.*|PROJECT_NAME = "Generic C Server Framework"|' Doxyfile
	@sed -i 's|^OUTPUT_DIRECTORY.*|OUTPUT_DIRECTORY = docs|' Doxyfile
	@sed -i 's|^INPUT.*|INPUT = ./src ./include|' Doxyfile
	@sed -i 's|^FILE_PATTERNS.*|FILE_PATTERNS = *.c *.h|' Doxyfile
	@sed -i 's|^GENERATE_HTML.*|GENERATE_HTML = YES|' Doxyfile
	@sed -i 's|^GENERATE_LATEX.*|GENERATE_LATEX = YES|' Doxyfile
	@sed -i 's|^HAVE_DOT.*|HAVE_DOT = YES|' Doxyfile
	@sed -i 's|^DOT_GRAPH_MAX_NODES.*|DOT_GRAPH_MAX_NODES = 50|' Doxyfile
	@echo "Doxyfile created and configured."
