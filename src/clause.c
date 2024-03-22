#include "clause.h"
#include "msg.h"
#include <stdio.h>
#include <stdlib.h>

extern bool *literal_array;
extern struct literal_stack set_literals;

void load_literal(literal_t literal) {
    if (!literal_array[literal]) {
        literal_array[literal] = true;
        PUSH(set_literals, literal);
    }
}

void load_clause(struct clause *clause_ptr) {
    for (all_literals_in_clause(literal, clause_ptr)) {
        load_literal(literal);
    }
}

void clear_literal_array(void) {
    for (all_literals_in_stack(literal, set_literals)) {
        literal_array[literal] = false;
    }
    CLEAR(set_literals);
}

int literal_compare(const void *a, const void *b) {
    return (*(literal_t *)a > *(literal_t *)b);
}

void literal_fprint(FILE *file, literal_t literal) {
    if (IS_SIGNED(literal))
        fprintf(file, "-");
    fprintf(file, "%d ", VAR(literal) + 1);
}

struct clause *clause_create(uint64_t index, struct literal_stack literals, struct clause_ptr_stack chain, bool is_rat) {
    uint32_t size = SIZE(literals);
    struct clause *clause_ptr = malloc(sizeof(struct clause) + sizeof(literal_t) * size);
    ASSERT_ERROR(clause_ptr, "clause_create: malloc failed");
    clause_ptr->purity = pure;
    clause_ptr->index = index;
    clause_ptr->size = size;
    clause_ptr->chain = chain;
    clause_ptr->pivot = is_rat ? ACCESS(literals, 0) : literal_undefined;
    clause_ptr->todos = (struct todo_stack){0, 0, 0};
    clause_ptr->prev = NULL;
    clause_ptr->next = NULL;
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
    clause_fprint(stdout, clause_ptr);
}

void clause_fprint(FILE *file, struct clause *clause_ptr) {
    if (!clause_ptr)
        return;

    fprintf(file, "%u ", clause_ptr->index);
    literal_t pivot = clause_ptr->pivot;
    if (pivot != literal_undefined) {
        literal_fprint(file, clause_ptr->pivot);
    }

    for (all_literals_in_clause(literal, clause_ptr)) {
        if (literal == pivot)
            continue;
        literal_fprint(file, literal);
    }
    fprintf(file, "0 ");
    for (all_clause_ptrs_in_stack(chain_clause_ptr, clause_ptr->chain)) {
        if (IS_NEG_CHAIN_HINT(chain_clause_ptr))
            fprintf(file, "-%u ", GET_CHAIN_HINT_PTR(chain_clause_ptr)->index);
        else
            fprintf(file, "%u ", GET_CHAIN_HINT_PTR(chain_clause_ptr)->index);
    }
    fprintf(file, "0\n");
}

struct clause_ptr_stack get_neg_chain(struct clause *rat_clause_ptr, struct clause *clause_ptr) {
    struct clause_ptr_stack chain = {0, 0, 0};
    PUSH(chain, clause_ptr);
    bool pre_chain_copied = false;
    bool found = false;
    for (all_clause_ptrs_in_stack(chain_clause_ptr, rat_clause_ptr->chain)) {
        if (!pre_chain_copied) {
            if (IS_NEG_CHAIN_HINT(chain_clause_ptr))
                pre_chain_copied = true;
            else {
                PUSH(chain, chain_clause_ptr);
                continue;
            }
        }

        if (IS_NEG_CHAIN_HINT(chain_clause_ptr) && GET_CHAIN_HINT_PTR(chain_clause_ptr) == clause_ptr) {
            found = true;
            // PUSH(chain, clause_ptr);
        } else if (found && IS_NEG_CHAIN_HINT(chain_clause_ptr))
            return chain; // reached end of chain with negative hint
        else if (found)
            PUSH(chain, chain_clause_ptr);
    }
    if (!found) {
        fprintf(stderr, "get_neg_chain: clause not found in chain\n");
        clause_fprint(stderr, clause_ptr);
        clause_fprint(stderr, rat_clause_ptr);
    }
    assert(found && SIZE(chain) > 0);
    return chain; // reached end of chain without negative hint
}

struct clause *resolve(struct clause *left_clause_ptr, struct clause *right_clause_ptr, literal_t resolvent) {
    struct clause *result_ptr = malloc(sizeof(struct clause) + sizeof(literal_t) * (left_clause_ptr->size + right_clause_ptr->size));
    ASSERT_ERROR(result_ptr, "resolve: malloc failed");
    result_ptr->index = left_clause_ptr->index > right_clause_ptr->index ? left_clause_ptr->index : right_clause_ptr->index;
    result_ptr->pivot = literal_undefined;
    result_ptr->todos = (struct todo_stack){0, 0, 0};
    // unrolled stack initialization
    result_ptr->chain.begin = malloc(sizeof(struct clause *) * 2);
    ASSERT_ERROR(result_ptr->chain.begin, "resolve: malloc failed");
    result_ptr->chain.begin[0] = left_clause_ptr;
    result_ptr->chain.begin[1] = right_clause_ptr;
    result_ptr->chain.end = result_ptr->chain.begin + 2;
    result_ptr->chain.allocated = result_ptr->chain.end;
    result_ptr->next = NULL;
    result_ptr->prev = NULL;
    result_ptr->purity = pure;

    literal_t neg_resolvent = NEG(resolvent);
    literal_t left, right;
    literal_t *left_ptr = left_clause_ptr->literals;
    literal_t *left_end = left_ptr + left_clause_ptr->size;
    literal_t *right_ptr = right_clause_ptr->literals;
    literal_t *right_end = right_ptr + right_clause_ptr->size;
    literal_t *result_literal_ptr = result_ptr->literals;

    while (true) {
        if (left_ptr == left_end && right_ptr == right_end)
            break;
        if (left_ptr == left_end) {
            right = *right_ptr++;
            if (right == neg_resolvent || right == resolvent)
                continue;
            *result_literal_ptr++ = right;
        } else if (right_ptr == right_end) {
            left = *left_ptr++;
            if (left == resolvent || left == neg_resolvent)
                continue;
            *result_literal_ptr++ = left;
        } else {
            left = *left_ptr;
            right = *right_ptr;
            if (left == right) {
                left_ptr++;
                right_ptr++;
                *result_literal_ptr++ = left;
            } else if (left < right) {
                left_ptr++;
                if (left == resolvent || left == neg_resolvent)
                    continue;
                *result_literal_ptr++ = left;
            } else {
                right_ptr++;
                if (right == neg_resolvent || right == resolvent)
                    continue;
                *result_literal_ptr++ = right;
            }
        }
    }
    result_ptr->size = result_literal_ptr - result_ptr->literals;
    result_ptr = realloc(result_ptr, sizeof(struct clause) + sizeof(literal_t) * result_ptr->size);
    ASSERT_ERROR(result_ptr, "resolve: realloc failed");
    return result_ptr;
}

literal_t clause_get_reverse_resolvent(struct clause *other) {
    for (all_literals_in_clause(literal_other, other)) {
        if (!literal_array[literal_other]) {
            literal_other = NEG(literal_other);
            load_literal(literal_other);
            return literal_other;
        }
    }
    return literal_undefined;
}

bool literal_in_clause(literal_t literal, struct clause *clause_ptr) {
    // using binary search
    literal_t *begin = clause_ptr->literals;
    literal_t *end = begin + clause_ptr->size;

    while (begin < end) {
        uint32_t *mid = begin + (end - begin) / 2;
        if (*mid == literal)
            return true;
        else if (*mid < literal)
            begin = mid + 1;
        else
            end = mid;
    }

    return false;
}
