#include "clause.h"
#include "msg.h"
#include <stdio.h>
#include <stdlib.h>

int literal_compare(const void *a, const void *b) {
    literal_t literal_a = *(literal_t *)a;
    literal_t literal_b = *(literal_t *)b;
    literal_t var_a = literal_a > 0 ? literal_a : -literal_a;
    literal_t var_b = literal_b > 0 ? literal_b : -literal_b;
    if (var_a < var_b)
        return -1;
    else if (var_a > var_b)
        return 1;
    return 0;
}

struct clause *clause_create(uint64_t index, struct literal_stack literals, struct clause_ptr_stack chain, bool is_rat) {
    uint32_t size = SIZE(literals);
    struct clause *clause_ptr = malloc(sizeof(struct clause) + sizeof(literal_t) * size);
    ASSERT_ERROR(clause_ptr, "clause_create: malloc failed");
    clause_ptr->index = index;
    clause_ptr->size = size;
    clause_ptr->chain = chain;
    clause_ptr->pivot = is_rat ? ACCESS(literals, 0) : 0;
    // copying literals and sorting them by absolute value
    // literal_stack literals are not sorted
    // sort algorithm: quicksort
    literal_t *literals_ptr = clause_ptr->literals;
    for (all_literals_in_stack(literal, literals)) {
        *literals_ptr = literal;
        literals_ptr++;
    }
    qsort(clause_ptr->literals, size, sizeof(literal_t), literal_compare);
    return clause_ptr;
}

void clause_release(struct clause *clause_ptr) {
    if (!clause_ptr)
        return;

    if (clause_ptr->chain.begin)
        RELEASE(clause_ptr->chain);

    if (clause_ptr->todos.begin)
        RELEASE(clause_ptr->todos);

    free(clause_ptr);
}

void clause_print(struct clause *clause_ptr) {
    if (!clause_ptr)
        return;

    printf("%llu ", clause_ptr->index);
    literal_t pivot = clause_ptr->pivot;
    if (pivot)
        printf("%d ", clause_ptr->pivot);

    for (all_literals_in_clause(literal, clause_ptr)) {
        if (literal == pivot)
            continue;
        printf("%d ", literal);
    }
    printf("0 ");
    for (all_clause_ptrs_in_stack(chain_clause_ptr, clause_ptr->chain)) {
        if (IS_NEG_CHAIN_HINT(chain_clause_ptr))
            printf("-%llu ", GET_CHAIN_HINT_PTR(chain_clause_ptr)->index);
        else
            printf("%llu ", GET_CHAIN_HINT_PTR(chain_clause_ptr)->index);
    }
    printf("0\n");
}

struct clause_ptr_stack get_neg_chain(struct clause *rat_clause_ptr, struct clause *clause_ptr) {
    struct clause_ptr_stack chain = {0, 0, 0};
    bool found = false;
    for (all_clause_ptrs_in_stack(chain_clause_ptr, rat_clause_ptr->chain)) {
        if (IS_NEG_CHAIN_HINT(chain_clause_ptr) && GET_CHAIN_HINT_PTR(chain_clause_ptr) == clause_ptr) {
            found = true;
            PUSH(chain, clause_ptr);
        } else if (found && IS_NEG_CHAIN_HINT(chain_clause_ptr))
            return chain; // reached end of chain with negative hint
        else if (found)
            PUSH(chain, chain_clause_ptr);
    }
    return chain; // reached end of chain without negative hint
}

void clause_ptr_stack_print(struct clause_ptr_stack stack, index_t *index) {
    for (all_clause_ptrs_in_stack(clause_ptr, stack)) {
        if (!clause_ptr)
            continue;
        clause_ptr = GET_CLAUSE_PTR(clause_ptr);
        if (index)
            clause_ptr->index = (*index)++;
        clause_print(clause_ptr);
    }
}

struct clause *resolve(struct clause *left_clause_ptr, struct clause *right_clause_ptr, literal_t resolvent) {
    uint64_t index = left_clause_ptr->index > right_clause_ptr->index ? left_clause_ptr->index : right_clause_ptr->index;
    struct literal_stack literals;
    INIT(literals);
    literal_t left, right, var_left, var_right,
        *left_ptr = left_clause_ptr->literals,
        *right_ptr = right_clause_ptr->literals,
        *left_end = left_clause_ptr->literals + left_clause_ptr->size,
        *right_end = right_clause_ptr->literals + right_clause_ptr->size;

    while (left_ptr != left_end || right_ptr != right_end) {
        if (left_ptr == left_end) {
            PUSH(literals, *right_ptr);
            right_ptr++;
            continue;
        } else if (right_ptr == right_end) {
            PUSH(literals, *left_ptr);
            left_ptr++;
            continue;
        }
        left = *left_ptr;
        var_left = left > 0 ? left : -left;
        right = *right_ptr;
        var_right = right > 0 ? right : -right;
        if (var_left < var_right) {
            PUSH(literals, left);
            left_ptr++;
        } else if (var_left > var_right) {
            PUSH(literals, right);
            right_ptr++;
        } else {
            if (left == right)
                PUSH(literals, left);
            else if (left != resolvent)
                ERROR("resolve failed %d == %d != %d", left, right, resolvent);

            left_ptr++;
            right_ptr++;
        }
    }

    struct clause_ptr_stack chain = {0, 0, 0};
    PUSH(chain, left_clause_ptr);
    PUSH(chain, right_clause_ptr);
    struct clause *result = clause_create(index, literals, chain, false);
    RELEASE(literals);
    return result;
}

struct literal_stack *clause_get_literals(struct clause *clause_ptr) {
    struct literal_stack *literals_ptr = malloc(sizeof(struct literal_stack));
    ASSERT_ERROR(literals_ptr, "clause_get_literals: malloc failed");
    INIT(*literals_ptr);
    for (all_literals_in_clause(literal, clause_ptr)) {
        PUSH(*literals_ptr, literal);
    }
    return literals_ptr;
}

literal_t clause_get_reverse_resolvent(struct literal_stack *literals, struct clause *other) {
    for (all_literals_in_clause(literal_other, other)) {
        bool found = false;
        for (all_literals_in_stack(literal_own, *literals)) {
            if (literal_own == literal_other) {
                found = true;
                break;
            }
        }
        if (!found) {
            PUSH(*literals, -literal_other);
            return -literal_other;
        }
    }
    return 0;
}

bool literal_in_clause(literal_t literal, struct clause *clause_ptr) {
    literal_t variable = literal > 0 ? literal : -literal;
    // using binary search
    literal_t *begin = clause_ptr->literals;
    literal_t *end = clause_ptr->literals + clause_ptr->size;
    while (begin < end) {
        literal_t *middle = begin + (end - begin) / 2;
        literal_t lit_mid = *middle;
        literal_t var_mid = lit_mid > 0 ? lit_mid : -lit_mid;
        if (var_mid == variable)
            return lit_mid == literal;
        else if (var_mid < variable)
            begin = middle + 1;
        else
            end = middle;
    }
    return false;
}

bool clause_in_chain(struct clause *clause_ptr, struct clause_ptr_stack chain) {
    for (all_clause_ptrs_in_stack(clause_in_chain, chain)) {
        if (clause_in_chain == clause_ptr)
            return true;
    }
    return false;
}

struct clause *parse_dimacs_clause(FILE *file) {
    struct literal_stack literals = {0, 0, 0};
    INIT(literals);
    uint32_t number = 0;
    bool negated = false;
    char next;
    while ((next = fgetc(file)) != EOF) {
        if (next == ' ' && number != 0) {
            PUSH(literals, negated ? -number : number);
            negated = false;
            number = 0;
        } else if (number == 0 && next == '0') {
            ASSERT_ERROR(fgetc(file) == '\n', "expected newline after 0 in dimacs");
            return clause_create(0, literals, (struct clause_ptr_stack){0, 0, 0}, false);
        } else if (next == '-') {
            negated = true;
        } else if (next != '\n') {
            ASSERT_ERROR(next >= '0' && next <= '9', "expected number in dimacs");
            number = number * 10 + next - '0';
        }
    }
    return NULL;
}

struct clause_ptr_stack parse_dimacs(const char *filename) {
    FILE *file = fopen(filename, "r");
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

    struct clause *clause_ptr;

    while ((clause_ptr = parse_dimacs_clause(file))) {
        PUSH(clause_ptr_stack, clause_ptr);
    }

    fclose(file);
    return clause_ptr_stack;
}

void clause_ptr_stack_release(struct clause_ptr_stack stack) {
    for (all_clause_ptrs_in_stack(clause_ptr, stack)) {
        clause_release(clause_ptr);
    }
    RELEASE(stack);
}

void todos_print(struct clause *clause_ptr) {
    if (!clause_ptr)
        return;
    clause_ptr = GET_CLAUSE_PTR(clause_ptr);
    printf("[%llu] Todos: \n", clause_ptr->index);
    for (all_todos_in_stack(todo, clause_ptr->todos)) {
        printf("\t");
        clause_print(todo.result);
    }
    printf("\n");
}
