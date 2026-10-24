CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c99 -Iinclude -O2 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lm

BIN_DIR = bin
OBJ_DIR = obj
SRC_DIR = src
TEST_DIR = tests

SRCS = $(SRC_DIR)/common.c \
       $(SRC_DIR)/slot.c \
       $(SRC_DIR)/allocation.c \
       $(SRC_DIR)/manager.c \
       $(SRC_DIR)/persistence.c \
       $(SRC_DIR)/simulation.c

OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

MAIN_SRC = $(SRC_DIR)/main.c
MAIN_OBJ = $(OBJ_DIR)/main.o
TARGET = $(BIN_DIR)/smartpark

TEST_INTERVALS_SRC = $(TEST_DIR)/test_intervals.c
TEST_INTERVALS_BIN = $(BIN_DIR)/test_intervals

TEST_ALLOC_SRC = $(TEST_DIR)/test_allocation.c
TEST_ALLOC_BIN = $(BIN_DIR)/test_allocation

TEST_EXP_SRC = $(TEST_DIR)/run_experiments.c
TEST_EXP_BIN = $(BIN_DIR)/run_experiments

.PHONY: all clean test run directories experiments

all: directories $(TARGET)

directories:
	@mkdir -p $(BIN_DIR) $(OBJ_DIR) data

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS) $(MAIN_OBJ)
	$(CC) $(OBJS) $(MAIN_OBJ) -o $@ $(LDFLAGS)
	@echo "Build successful: $(TARGET)"

$(TEST_INTERVALS_BIN): $(OBJS) $(TEST_INTERVALS_SRC)
	$(CC) $(CFLAGS) $(OBJS) $(TEST_INTERVALS_SRC) -o $@ $(LDFLAGS)

$(TEST_ALLOC_BIN): $(OBJS) $(TEST_ALLOC_SRC)
	$(CC) $(CFLAGS) $(OBJS) $(TEST_ALLOC_SRC) -o $@ $(LDFLAGS)

$(TEST_EXP_BIN): $(OBJS) $(TEST_EXP_SRC)
	$(CC) $(CFLAGS) $(OBJS) $(TEST_EXP_SRC) -o $@ $(LDFLAGS)

test: directories $(TEST_INTERVALS_BIN) $(TEST_ALLOC_BIN)
	@echo "\n=== Running Test Suites ==="
	@./$(TEST_INTERVALS_BIN)
	@./$(TEST_ALLOC_BIN)
	@echo "=== All Tests Passed! ===\n"

experiments: directories $(TEST_EXP_BIN)
	@./$(TEST_EXP_BIN)

run: all
	@./$(TARGET)

clean:
	rm -rf $(BIN_DIR) $(OBJ_DIR)
	@echo "Clean completed."
