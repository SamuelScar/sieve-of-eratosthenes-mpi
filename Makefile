CC := gcc
MPICC := mpicc

CFLAGS := -std=c11 -O2 -Wall -Wextra -Wpedantic

BIN_DIR := bin
BIN_SEQUENCIAL := $(BIN_DIR)/sequencial
BIN_PARALELO := $(BIN_DIR)/paralelo
NP ?= 4
N ?= 100

.PHONY: all run run-sequential profile clean

all: $(BIN_SEQUENCIAL) $(BIN_PARALELO)

$(BIN_DIR):
	mkdir -p $@

$(BIN_SEQUENCIAL): src/sequencial.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(BIN_PARALELO): src/paralelo.c | $(BIN_DIR)
	$(MPICC) $(CFLAGS) $< -o $@ -lm

run: $(BIN_PARALELO)
	mpirun --allow-run-as-root --oversubscribe -np $(NP) $(BIN_PARALELO) $(N)

run-sequential: $(BIN_SEQUENCIAL)
	$(BIN_SEQUENCIAL) $(N)

profile: CFLAGS += -pg
profile: clean $(BIN_SEQUENCIAL)
	$(BIN_SEQUENCIAL) $(N)
	gprof $(BIN_SEQUENCIAL) gmon.out

clean:
	rm -rf $(BIN_DIR) gmon.out
