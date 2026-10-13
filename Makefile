CC := gcc
MPICC := mpicc

CFLAGS := -std=c11 -O2 -Wall -Wextra -Wpedantic
PROFILE_FLAGS := -pg

BIN_DIR := bin
SEQUENTIAL_BIN := $(BIN_DIR)/sequencial
MPI_BIN := $(BIN_DIR)/mpi_example
NP ?= 4
N ?= 100

.PHONY: all run run-sequential profile clean

all: $(SEQUENTIAL_BIN)

$(BIN_DIR):
	mkdir -p $@

$(SEQUENTIAL_BIN): src/sequencial.c | $(BIN_DIR)
	$(CC) $(CFLAGS) $< -o $@

$(MPI_BIN): src/mpi_example.c | $(BIN_DIR)
	$(MPICC) $(CFLAGS) $< -o $@

run: $(MPI_BIN)
	mpirun --allow-run-as-root -np $(NP) $(MPI_BIN)

run-sequential: $(SEQUENTIAL_BIN)
	$(SEQUENTIAL_BIN) $(N)

profile: CFLAGS += $(PROFILE_FLAGS)
profile: clean $(SEQUENTIAL_BIN)
	$(SEQUENTIAL_BIN) $(N)
	gprof $(SEQUENTIAL_BIN) gmon.out

clean:
	rm -rf $(BIN_DIR) gmon.out
