#ifndef __clause_h__
#define __clause_h__

#include "stack.h"
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Type of literals.
 * Correspondance
 * |   dimacs    | literal_t  |
 * |-------------|------------|
 * | 1           | 0          |
 * | -1          | 1          |
 * | ...         | ...        |
 * | 2147483647  | 4294967292 |
 * | -2147483647 | 4294967293 |
 *
 * 4294967295 is reserved for undefined literals
 */
#define literal_t uint32_t
#define literal_undefined UINT32_MAX

#define NEG(literal) ((literal) ^ 1)
#define IS_SIGNED(literal) ((literal) & 1)
#define VAR(literal) ((literal) >> 1)
#define LITERAL(sign, var) (((var) << 1) | (sign))
#define SIGN(sign, literal) ((literal) | (sign))

#define index_t uint32_t
void literal_print(literal_t literal);

struct literal_stack {
    literal_t *begin;
    literal_t *end;
    literal_t *allocated;
};

struct clause_ptr_stack {
    struct clause **begin;
    struct clause **end;
    struct clause **allocated;
};

struct todo {
    struct clause *other;
    struct clause *result;
};

struct todo_stack {
    struct todo *begin;
    struct todo *end;
    struct todo *allocated;
};

enum purity {
    pure = 0,
    semipure = 1,
    impure = 2
};

struct clause {
    enum purity purity;
    index_t index;
    struct clause *next, *prev;
    struct clause_ptr_stack chain;
    struct todo_stack todos;
    uint32_t size;
    literal_t pivot;
    literal_t literals[];
};

struct clause *clause_create(uint64_t index, struct literal_stack literals, struct clause_ptr_stack chain, bool is_rat);
void clause_release(struct clause *clause_ptr);
void clause_print(struct clause *clause_ptr);
void clause_fprint(FILE *file, struct clause *clause_ptr);

void load_literal(literal_t literal);
void load_clause(struct clause *clause_ptr);
void clear_literal_array(void);

struct clause *resolve(struct clause *left_clause_ptr, struct clause *right, literal_t resolvent);

literal_t clause_get_reverse_resolvent(struct clause *other);
struct clause_ptr_stack get_neg_chain(struct clause *rat_clause_ptr, struct clause *clause_ptr);

bool literal_in_clause(literal_t literal, struct clause *clause_ptr);

#define all_literals_in_stack(L, S) \
    all_elements_on_stack(literal_t, L, S)

#define all_literals_in_clause(L, C)                                             \
    literal_t L, *L##_ptr = (C)->literals, *L##_end = (C)->literals + (C)->size; \
    (L##_ptr != L##_end) && ((L = *L##_ptr), 1);                                 \
    ++L##_ptr

#define all_clause_ptrs_in_stack(C, S) \
    all_pointers_on_stack(struct clause, C, S)

#define all_clause_ptrs_in_stack_reversed(E, S)                                 \
    struct clause *E, **E##_ptr = (S).end - 1, **const E##_end = (S).begin - 1; \
    (E##_ptr != E##_end) && (E = *E##_ptr, 1);                                  \
    --E##_ptr

#define all_todos_in_stack(T, S) \
    all_elements_on_stack(struct todo, T, S)

#define TAG_CHAIN_HINT_NEG 1
#define SET_NEG_CHAIN_HINT(P) ((struct clause *)((uintptr_t)(P) | TAG_CHAIN_HINT_NEG))
#define IS_NEG_CHAIN_HINT(P) ((uintptr_t)(P) & TAG_CHAIN_HINT_NEG)
#define GET_CHAIN_HINT_PTR(P) ((struct clause *)((uintptr_t)(P) & ~TAG_CHAIN_HINT_NEG))

#define MASK_PURITY 3
#define TAG_PURE 0
#define TAG_SEMIPURE 1
#define TAG_IMPURE 2
#define SET_PURITY_TAG(P, T) ((struct clause *)((uintptr_t)(P) | (T)))
#define GET_PURITY_TAG(P) ((uintptr_t)(P) & MASK_PURITY)
#define GET_CLAUSE_PTR(P) ((struct clause *)((uintptr_t)(P) & ~MASK_PURITY))

#endif /* __clause_h__ */
