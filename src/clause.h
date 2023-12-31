#ifndef __clause_h__
#define __clause_h__

#include "stack.h"
#include <stdbool.h>
#include <stdint.h>

#define literal_t int32_t
#define index_t uint64_t

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

struct clause {
    index_t index;
    struct clause_ptr_stack chain;
    struct todo_stack todos;
    uint32_t size;
    literal_t pivot;
    literal_t literals[];
};

struct clause *clause_create(uint64_t index, struct literal_stack literals, struct clause_ptr_stack chain, bool is_rat);
void clause_release(struct clause *clause_ptr);
void clause_print(struct clause *clause_ptr);

struct clause *resolve(struct clause *left_clause_ptr, struct clause *right, literal_t resolvent);

struct literal_stack *clause_get_literals(struct clause *clause_ptr);
literal_t clause_get_reverse_resolvent(struct literal_stack *literals, struct clause *other);
struct clause_ptr_stack get_neg_chain(struct clause *rat_clause_ptr, struct clause *clause_ptr);

bool literal_in_clause(literal_t literal, struct clause *clause_ptr);
bool clause_in_chain(struct clause *clause_ptr, struct clause_ptr_stack chain);

struct clause_ptr_stack parse_dimacs(const char *filename);
void clause_ptr_stack_print(struct clause_ptr_stack stack, index_t *index);
void clause_ptr_stack_release(struct clause_ptr_stack stack);
void todos_print(struct clause *clause_ptr);

#define all_literals_in_stack(L, S) \
    all_elements_on_stack(literal_t, L, S)

#define all_literals_in_clause(L, C)                                             \
    literal_t L, *L##_ptr = (C)->literals, *L##_end = (C)->literals + (C)->size; \
    (L##_ptr != L##_end) && ((L = *L##_ptr), 1);                                 \
    ++L##_ptr

#define all_clause_ptrs_in_stack(C, S) \
    all_pointers_on_stack(struct clause, C, S)

#define all_todos_in_stack(T, S) \
    all_elements_on_stack(struct todo, T, S)

#define TAG_CHAIN_HINT_NEG 1
#define SET_NEG_CHAIN_HINT(P) ((struct clause *)((uintptr_t)(P) | TAG_CHAIN_HINT_NEG))
#define IS_NEG_CHAIN_HINT(P) ((uintptr_t)(P)&TAG_CHAIN_HINT_NEG)
#define GET_CHAIN_HINT_PTR(P) ((struct clause *)((uintptr_t)(P) & ~TAG_CHAIN_HINT_NEG))

#define MASK_PURITY 3
#define TAG_PURE 0
#define TAG_SEMIPURE 1
#define TAG_IMPURE 2
#define SET_PURITY_TAG(P, T) ((struct clause *)((uintptr_t)(P) | (T)))
#define GET_PURITY_TAG(P) ((uintptr_t)(P)&MASK_PURITY)
#define GET_CLAUSE_PTR(P) ((struct clause *)((uintptr_t)(P) & ~MASK_PURITY))

#endif /* __clause_h__ */
