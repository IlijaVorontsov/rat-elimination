#include "proof.h"

#include <stdio.h>
#include <stdlib.h>

#include "msg.h"

void proof_update_indices(struct proof proof) {
    index_t i = 1;
    for (all_stacks_in_proof(stack, proof)) {
        stack.begin = GET_PTR(stack.begin);
        for (all_clause_ptrs_in_stack(clause_ptr, stack)) {
            if (!clause_ptr)
                continue;
            GET_CLAUSE_PTR(clause_ptr)->index = i++;
        }
    }
}

struct proof proof_from_dimacs_lrat(const char *dimacs_path,
                                    const char *lrat_path) {
    struct proof proof = {0, 0, 0, 0};
    FILE *file = fopen(dimacs_path, "r");
    ASSERT_ERROR(file, "parse_dimacs: fopen failed");

    // skip over lines with comments
    char next;
    while ((next = fgetc(file)) == 'c' || next == 'p') {
        while (fgetc(file) != '\n')
            ;
        if (next == 'p')
            break;
    }

    ASSERT_ERROR(next == 'p', "missig header after comments in dimacs");

    struct clause_ptr_stack clause_ptr_stack = {0, 0, 0};

    struct literal_stack literals = {0, 0, 0};
    literal_t variable = 0;
    literal_t max_variable = 0;
    bool negated = false;

    while ((next = fgetc(file)) != EOF) {
        if (next == ' ' && variable != 0) {
            variable = variable - 1; // converting to internal representation
            max_variable = max_variable > variable ? max_variable : variable;
            PUSH(literals, LITERAL(negated, variable));
            negated = false;
            variable = 0;
        } else if (variable == 0 && next == '0' && negated == false) {
            ASSERT_ERROR(fgetc(file) == '\n', "expected newline after 0 in dimacs");
            PUSH(clause_ptr_stack, clause_create(0, literals, (struct clause_ptr_stack){0, 0, 0}, false));
            CLEAR(literals);
        } else if (next == '-') {
            negated = true;
        } else if (next != '\n') {
            ASSERT_ERROR(next >= '0' && next <= '9', "expected number in dimacs");
            variable = variable * 10 + next - '0';
        }
    }
    ASSERT_ERROR(variable == 0 && negated == false && EMPTY(literals), "expected 0 at the end of file in dimacs");
    struct clause_ptr_stack dimacs = clause_ptr_stack;
    PUSH(proof, clause_ptr_stack);
    uint32_t size_dimacs = SIZE(clause_ptr_stack);

    FILE *lrat_file = fopen(lrat_path, "r");
    ASSERT_ERROR(lrat_file, "proof_from_dimacs_lrat: fopen failed");

    bool rat = false;
    uint64_t number = 0;

    enum { READ_INDEX,
           READ_LITERALS,
           READ_CHAIN } state = READ_INDEX;

    index_t index;
    struct {
        index_t *begin, *end, *allocated;
    } lookup_table = {0, 0, 0};
    PUSH(lookup_table, 0); // shifting by one

    struct clause_ptr_stack chain;
    INIT(chain);
    while ((next = fgetc(lrat_file)) != EOF) {
        switch (state) {
        case READ_INDEX:
            if (next == ' ') {
                state = READ_LITERALS;
                index = number;
                number = 0;
            } else {
                ASSERT_ERROR(next >= '0' && next <= '9', "expected number in [%s:%lu]", lrat_path, SIZE(proof));
                number = (number * 10) + (next - '0');
            }
            break;
        case READ_LITERALS:
            if (number == 0 && next == 'd') {
                state = READ_INDEX;
                index = 0;
                while ((next = fgetc(lrat_file)) != '\n')
                    ;
            } else if (number == 0 && next == '0') {
                state = READ_CHAIN;
                assert(fgetc(lrat_file) == ' '); // skipping space
            } else if (number == 0 && next == '-') {
                negated = true;
            } else if (next == ' ') {
                number = number - 1; // converting to internal representation
                max_variable = max_variable > number ? max_variable : number;
                PUSH(literals, LITERAL(negated, number));
                negated = false;
                number = 0;
            } else {
                ASSERT_ERROR(next >= '0' && next <= '9', "expected number in [%s:%lu]", lrat_path, SIZE(proof));
                number = (number * 10) + (next - '0');
            }
            break;
        case READ_CHAIN:
            if (number == 0 && next == '0') {
                struct clause_ptr_stack stack = {0, 0, 0};
                if (EMPTY(chain))
                    rat = true;
                PUSH(stack, clause_create(SIZE(proof), literals, chain, rat));
                stack.begin = rat ? SET_RAT(stack.begin) : stack.begin;
                PUSH(proof, stack);
                PUSH(lookup_table, index);
                ASSERT_ERROR(fgetc(lrat_file) == '\n', "expected newline in [%s:%lu]", lrat_path, SIZE(proof));
                CLEAR(literals);
                INIT(chain);
                rat = false;
                state = READ_INDEX;
            } else if (number == 0 && next == '-') {
                negated = rat = true;
            } else if (next == ' ') {
                struct clause *clause_ptr;
                if (number <= size_dimacs) {
                    clause_ptr = ACCESS(dimacs, number - 1);
                } else {
                    // reverse lookup in lookup table using binary search
                    index_t *begin = lookup_table.begin;
                    index_t *end = lookup_table.end;
                    while (begin < end) {
                        index_t *mid = begin + (end - begin) / 2;
                        if (*mid == number) {
                            struct clause_ptr_stack stack = ACCESS(proof, mid - lookup_table.begin);
                            stack.begin = GET_PTR(stack.begin);
                            clause_ptr = ACCESS(stack, 0);
                            break;
                        } else if (*mid < number) {
                            begin = mid + 1;
                        } else {
                            end = mid;
                        }
                    }
                }
                PUSH(chain, negated ? SET_NEG_CHAIN_HINT(clause_ptr) : clause_ptr);
                negated = false;
                number = 0;
            } else {
                ASSERT_ERROR(next >= '0' && next <= '9', "expected number in  [%s:%lu]", lrat_path, SIZE(proof));
                number = (number * 10) + (next - '0');
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
    proof.next_rat = proof.end;
    proof.max_variable = max_variable;
    return proof;
}

void proof_release(struct proof proof) {
    for (all_stacks_in_proof(stack, proof)) {
        clause_ptr_stack_release(stack);
    }
    RELEASE(proof);
}

void proof_print(struct proof proof) {
    index_t i = 1;
    for (all_stacks_in_proof(stack, proof)) {
        if (stack_ptr == proof.begin) {
            for (all_clause_ptrs_in_stack(clause_ptr, stack)) {
                clause_ptr->index = i++;
                if (!EMPTY(clause_ptr->chain)) {
                    clause_print(clause_ptr);
                }
            }
        } else {
            stack.begin = GET_PTR(stack.begin);
            clause_ptr_stack_print(stack, &i);
        }
    }
}

void proof_print_all(struct proof proof) {
    index_t i = 1;
    for (all_stacks_in_proof(stack, proof)) {
        clause_ptr_stack_print(stack, &i);
    }
}
