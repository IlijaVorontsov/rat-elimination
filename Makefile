SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin
TMP_DIR := tmp
TEST_DIR := tests

EXE := $(BIN_DIR)/rat-elimination
SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DRUP := $(wildcard $(TEST_DIR)/*.drup)
CNF := $(wildcard $(TEST_DIR)/*.cnf)

CC = clang
CFLAGS = -std=c99 -O1

.PHONY: all clean

all: $(EXE)

debug : CFLAGS += -DDEBUG -g -pedantic -Wno-gnu-zero-variadic-macro-arguments -Wall
debug : $(EXE)

$(EXE): $(OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@


$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@


$(BIN_DIR) $(OBJ_DIR) $(TMP_DIR) $(TEST_DIR):
	mkdir -p $@

$(TMP_DIR)/drat-trim.c:
	if [ ! -f $@ ]; then \
		wget -O $@ https://www.cs.utexas.edu/~marijn/drat-trim/drat-trim.c; \
	fi

$(BIN_DIR)/drat-trim: $(TMP_DIR)/drat-trim.c | $(BIN_DIR) $(TMP_DIR)
	$(CC) -O2 $< -o $@

$(TEST_DIR)/%.lrat: $(BIN_DIR)/drat-trim $(TEST_DIR)/%.cnf $(TEST_DIR)/%.drat 
	$^ -L $@
	sed -i '' '/d/d' $@ 
	sort -n -o $@ $@

$(TEST_DIR)/%.lrup: $(EXE) $(TEST_DIR)/%.cnf $(TEST_DIR)/%.lrat
	$^ > $@

$(TEST_DIR)/%.drup: $(TEST_DIR)/%.lrup
	sed 's/ 0.*/ 0/' $< > $@
	sed -i '' -E 's/[0-9]+[[:space:]]//' $@

lrups: $(EXE)
	@echo "Converting LRAT's ..."
	@for file in $(CNF) ; do \
		base=$$(echo $$file | sed 's/\.cnf$$//'); \
		echo "Converting $$base.lrat ..."; \
		if [ -f $$base.lrat ] && [ -f $$file ]; then \
			make $$base.lrup > /dev/null; \
		fi \
	done

drups: $(EXE)
	@echo "Converting LRAT's ..."
	@for file in $(CNF) ; do \
		base=$$(echo $$file | sed 's/\.cnf$$//'); \
		echo "Converting $$base.lrat ..."; \
		if [ -f $$base.lrat ] && [ -f $$file ]; then \
			make $$base.drup > /dev/null; \
		fi \
	done

verify: $(BIN_DIR)/drat-trim
	@echo "Verifying DRUP files ..."
	@for file in $(DRUP) ; do \
		if $(BIN_DIR)/drat-trim `echo $$file | sed 's/\.drup$$//'`.cnf $$file > /dev/null 2>&1; then \
			echo "$$file SUCCESS" ; \
		else \
			echo "$$file FAILED" ; \
		fi ; \
	done

# remove lrup and drup files
clean:
	rm -rv $(EXE) $(OBJ_DIR) $(TEST_DIR)/*.lrup $(TEST_DIR)/*.drup || true

