SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

EXE := $(BIN_DIR)/rat-elimination
SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

CC = clang
CFLAGS = -arch arm64 -std=c99 -O3 

.PHONY: all clean

all: $(EXE)

debug : CFLAGS += -DDEBUG -g -pedantic -Wno-gnu-zero-variadic-macro-arguments -Wall
debug : $(EXE)

$(EXE): $(OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@


$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@


$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@


clean:
	rm -rv $(BIN_DIR) $(OBJ_DIR) || true

test:
	./bin/rat-elimination tests/c.cnf tests/l.lrat > p.lrup
	python3 utils/lrat-convert.py p.lrup p.drat
	./utils/drat-trim tests/c.cnf p.drat
	rm p.lrup p.drat


run: # takes a dimacs and drat proof
	./utils/dimacs-prepare.py $(dimacs) p.cnf
	./utils/drat-trim $(dimacs) $(drat) -L p.lrat
	./utils/lrat-prepare.py p.lrat p.lrat
	./bin/rat-elimination $(dimacs) p.lrat > p.lrup
	python3 utils/lrat-convert.py p.lrup p.drat
	./utils/drat-trim $(dimacs) p.drat
	rm p.lrat p.drat p.cnf