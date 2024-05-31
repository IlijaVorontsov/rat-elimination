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
CFLAGS = -D_POSIX_C_SOURCE -std=c11

.PHONY: all clean

all: $(EXE)

debug : CFLAGS += -DDEBUG -g -pedantic -Wall 
debug : $(EXE)

$(EXE): $(OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@


$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@


$(BIN_DIR) $(OBJ_DIR) $(TMP_DIR) $(TEST_DIR):
	mkdir -p $@

$(TMP_DIR)/drat-trim.c: $(TMP_DIR)
	if [ ! -f $@ ]; then \
		wget -O $@ https://www.cs.utexas.edu/~marijn/drat-trim/drat-trim.c; \
	fi

$(BIN_DIR)/drat-trim: $(TMP_DIR)/drat-trim.c | $(BIN_DIR) $(TMP_DIR)
	$(CC) -O2 $< -o $@

$(TMP_DIR)/lrat-check.c: $(TMP_DIR)
	if [ ! -f $@ ]; then \
		wget -O $@ https://raw.githubusercontent.com/marijnheule/drat-trim/master/lrat-check.c; \
	fi

$(BIN_DIR)/lrat-check: $(TMP_DIR)/lrat-check.c | $(BIN_DIR)
	$(CC) -O2 $< -o $@

$(TEST_DIR)/%.lrat: $(BIN_DIR)/drat-trim $(TEST_DIR)/%.cnf $(TEST_DIR)/%.drat 
	$^ -L $@
	sed -i '/d/d' $@ 
	sort -n -o $@ $@

$(TEST_DIR)/%.lrup: $(EXE) $(TEST_DIR)/%.cnf $(TEST_DIR)/%.lrat
	@ if $^ > $@; then \
		echo "SUCCESS" ; \
	else \
		echo "FAILED" ; \
	fi

$(TEST_DIR)/%.drup: $(TEST_DIR)/%.lrup
	sed 's/ 0.*/ 0/' $< > $@
	sed -i -E 's/[0-9]+[[:space:]]//' $@

lrups: $(EXE)
	@echo "Converting LRAT's ..."
	@for file in $(CNF) ; do \
		base=$$(echo $$file | sed 's/\.cnf$$//'); \
		echo "Converting $$base.lrat ..."; \
		if [ -f $$base.lrat ] && [ -f $$file ]; then \
			if ! make $$base.lrup > /dev/null; then \
				echo "Failed to convert $$base.lrat to $$base.lrup"; \
			fi \
		fi \
	done

check: $(BIN_DIR)/lrat-check
	make lrups > /dev/null
	@echo "Checking for LRUP files ..."
	@for file in $(CNF) ; do \
		base=$$(echo $$file | sed 's/\.cnf$$//'); \
		if $^ $$file $$base.lrup > /dev/null; then \
			echo "$$base.lrup OK" ; \
		else \
			echo "$$base.lrup FAILED" ; \
		fi ; \
	done

# remove lrup
clean:
	rm -rv $(EXE) $(OBJ_DIR) $(TEST_DIR)/*.lrup || true

