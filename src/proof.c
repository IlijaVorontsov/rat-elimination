#include "proof.h"

#include <stdio.h>
#include <stdlib.h>

#include "msg.h"

void proof_update_indices(struct proof proof) {
    index_t i = 1;
    struct clause *clause_ptr = proof.begin;
    while (clause_ptr) {
        clause_ptr->index = i++;
        clause_ptr = clause_ptr->next;
    }
    fprintf(stderr, "New size: %u\n", i - 1);
}

struct proof proof_from_dimacs_lrat(const char *dimacs_path,
                                    const char *lrat_path) {
    struct proof proof = {(struct clause_ptr_stack){0, 0, 0}, 0, 0, 0};
    FILE *file = fopen(dimacs_path, "r");
    ASSERT_ERROR(file, "parse_dimacs: fopen failed");

    // skip over lines with comments
    char character;
    while ((character = fgetc(file)) == 'c' || character == 'p') {
        while (fgetc(file) != '\n')
            ;
        if (character == 'p')
            break;
    }

    ASSERT_ERROR(character == 'p', "missig header after comments in dimacs");

    struct clause_ptr_stack lookup_stack = {0, 0, 0};
    PUSH(lookup_stack, NULL); // shifting by one (index 0 is not used in dimacs format)
    struct {
        index_t *begin, *end, *allocated;
    } lookup_table = {0, 0, 0};
    PUSH(lookup_table, 0); // shifting by one

    struct literal_stack literals = {0, 0, 0};
    literal_t variable = 0;
    literal_t max_variable = 0;
    index_t index = 1;
    bool negated = false;
    struct clause *previous = NULL, *current = NULL;

    while ((character = fgetc(file)) != EOF) {
        if (character == ' ' && variable != 0) {
            variable = variable - 1; // converting to internal representation
            max_variable = max_variable > variable ? max_variable : variable;
            PUSH(literals, LITERAL(negated, variable));
            negated = false;
            variable = 0;
        } else if (variable == 0 && character == '0' && negated == false) {
            ASSERT_ERROR(fgetc(file) == '\n', "expected newline after 0 in dimacs");
            current = clause_create(index, literals, (struct clause_ptr_stack){0, 0, 0}, false);
            current->prev = previous;
            if (previous == NULL) {
                proof.begin = current;
            }
            previous = current;
            PUSH(lookup_stack, current);
            PUSH(lookup_table, index);
            index++;
            CLEAR(literals);
        } else if (character == '-') {
            negated = true;
        } else if (character != '\n') {
            ASSERT_ERROR(character >= '0' && character <= '9', "expected number in dimacs");
            variable = variable * 10 + character - '0';
        }
    }
    ASSERT_ERROR(variable == 0 && negated == false && EMPTY(literals), "expected 0 at the end of file in dimacs");
    proof.last_dimacs = current;
    fclose(file);
    FILE *lrat_file = fopen(lrat_path, "r");
    ASSERT_ERROR(lrat_file, "proof_from_dimacs_lrat: fopen failed");

    bool rat = false;
    uint64_t number = 0;

    enum { READ_INDEX,
           READ_LITERALS,
           READ_CHAIN } state = READ_INDEX;

    index_t clause_index;

    struct clause_ptr_stack chain;
    INIT(chain);
    while ((character = fgetc(lrat_file)) != EOF) {
        switch (state) {
        case READ_INDEX:
            if (character == ' ') {
                state = READ_LITERALS;
                clause_index = number;
                number = 0;
            } else {
                ASSERT_ERROR(character >= '0' && character <= '9', "expected number in [%s:%u]", lrat_path, index);
                number = (number * 10) + (character - '0');
            }
            break;
        case READ_LITERALS:
            if (number == 0 && character == 'd') {
                state = READ_INDEX;
                index = 0;
                while ((character = fgetc(lrat_file)) != '\n')
                    ;
            } else if (number == 0 && character == '0') {
                state = READ_CHAIN;
                assert(fgetc(lrat_file) == ' '); // skipping space
            } else if (number == 0 && character == '-') {
                negated = true;
            } else if (character == ' ') {
                number = number - 1; // converting to internal representation
                max_variable = max_variable > number ? max_variable : number;
                PUSH(literals, LITERAL(negated, number));
                negated = false;
                number = 0;
            } else {
                ASSERT_ERROR(character >= '0' && character <= '9', "expected number in [%s:%u]", lrat_path, index);
                number = (number * 10) + (character - '0');
            }
            break;
        case READ_CHAIN:
            if (number == 0 && character == '0') {
                rat = rat || EMPTY(chain);
                current = clause_create(index++, literals, chain, rat);
                current->prev = previous;
                previous = current;
                if (rat)
                    PUSH(proof.rat_clauses, current);
                PUSH(lookup_table, clause_index);
                PUSH(lookup_stack, current);
                ASSERT_ERROR(fgetc(lrat_file) == '\n', "expected newline in [%s:%lu]", lrat_path, SIZE(proof));
                CLEAR(literals);
                INIT(chain);
                rat = false;
                state = READ_INDEX;
            } else if (number == 0 && character == '-') {
                negated = rat = true;
            } else if (character == ' ') {
                struct clause *clause_ptr;
                // reverse lookup in lookup table using binary search
                index_t *begin = lookup_table.begin;
                index_t *end = lookup_table.end;
                while (begin < end) {
                    index_t *mid = begin + (end - begin) / 2;
                    if (*mid == number) {
                        clause_ptr = ACCESS(lookup_stack, mid - lookup_table.begin);
                        break;
                    } else if (*mid < number) {
                        begin = mid + 1;
                    } else {
                        end = mid;
                    }
                }

                PUSH(chain, negated ? SET_NEG_CHAIN_HINT(clause_ptr) : clause_ptr);
                negated = false;
                number = 0;
            } else {
                ASSERT_ERROR(character >= '0' && character <= '9', "expected number in  [%s:%lu]", lrat_path, SIZE(proof));
                number = (number * 10) + (character - '0');
            }
            break;
        default:
            assert(0);
            break;
        }
    }
    fclose(lrat_file);
    RELEASE(literals);
    RELEASE(lookup_table);
    proof.end = previous;
    proof.next_rat_ptr = NULL;
    // proof.max_variable = max_variable;
    struct clause *next = proof.end;
    current = next->prev;
    while (current) {
        current->next = next;
        next = current;
        current = current->prev;
    }
    return proof;
}

void proof_release(struct proof proof) {
    struct clause *clause_ptr = proof.begin;
    while (clause_ptr) {
        struct clause *next = clause_ptr->prev;
        clause_release(clause_ptr);
        clause_ptr = next;
    }
    RELEASE(proof.rat_clauses);
}

void proof_print(struct proof proof) {
    index_t i = 1;
    struct clause *clause_ptr = proof.begin;
    while (clause_ptr) {
        clause_ptr->index = i++;
        if (!(EMPTY(clause_ptr->chain) || clause_ptr->pivot != literal_undefined))
            clause_print(clause_ptr);
        clause_ptr = clause_ptr->next;
    }
}

void proof_print_all(struct proof proof) {
    index_t i = 1;
    struct clause *clause_ptr = proof.begin;
    while (clause_ptr) {
        clause_ptr->index = i++;
        clause_print(clause_ptr);
        clause_ptr = clause_ptr->next;
    }
}
