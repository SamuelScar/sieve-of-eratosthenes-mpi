CC := gcc
MPICC := mpicc

CFLAGS := -std=c11 -O2 -Wall -Wextra -Wpedantic
PROFILE_FLAGS := -pg

BIN_DIR := bin
SEQUENTIAL_BIN := $(BIN_DIR)/sequencial
MPI_BIN := $(BIN_DIR)/paralelo
NP ?= 4
N ?= 100

.PHONY: all run run-sequential profile clean

all: $(SEQUENTIAL_BIN) $(MPI_BIN)

$(BIN_DIR):
	mkdir -p $@

$(SEQUENTIAL_BIN): src/sequencial.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(MPI_BIN): src/paralelo.c | $(BIN_DIR)
	$(MPICC) $(CFLAGS) $< -o $@ -lm

run: $(MPI_BIN)
	mpirun --allow-run-as-root --oversubscribe -np $(NP) $(MPI_BIN) $(N)

run-sequential: $(SEQUENTIAL_BIN)
	$(SEQUENTIAL_BIN) $(N)

profile: CFLAGS += $(PROFILE_FLAGS)
profile: clean $(SEQUENTIAL_BIN)
	$(SEQUENTIAL_BIN) $(N)
	gprof $(SEQUENTIAL_BIN) gmon.out

clean:
	rm -rf $(BIN_DIR) gmon.out
