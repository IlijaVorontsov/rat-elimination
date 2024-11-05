#ifndef __clause_h__
#define __clause_h__

#include "literal.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#define index_t unsigned long long // 64-bit

struct subsumption_merge_chain
{
    unsigned size; // pivot_count = chain_size - 1
    literal_t *pivots;
    struct clause **clauses;
};

typedef struct clause
{
    enum
    {
        pure,
        semipure,
        impure,
        todo,
        rat
    } purity;
    index_t index;
    struct clause *prev, *next;
    literal_t hint;
    struct clause *hint_clause;
    struct subsumption_merge_chain chain;
    unsigned literal_count;
    literal_t *literals;
} clause_t;

extern bool *bit_vector;

#define is_dimacs_clause(C) ((C).purity != rat && (C).chain.size == 0)
#define is_rat_clause(C) ((C).purity == rat)
#define is_rup_clause(C) ((C).purity != rat && (C).chain.size > 0)

void clause_release(clause_t *clause_ptr);

void clause_fprint(FILE *file, clause_t clause);
void clause_fprint_with_pivots(FILE *file, clause_t clause);

void clause_reconstruct_pivots(clause_t *clause_ptr);
clause_t *resolve(clause_t left, clause_t right, literal_t resolvent, clause_t **chain, literal_t *pivots, unsigned chain_size);

bool literal_in_clause(literal_t literal, clause_t clause);
signed char var_in_clause(literal_t literal, clause_t clause);

struct subsumption_merge_chain get_chain(struct subsumption_merge_chain rat_chain, clause_t *clause_ptr);

#define TAG_CHAIN_HINT_NEG 1
#define SET_NEG_CHAIN_HINT(P) ((struct clause *)((uintptr_t)(P) | TAG_CHAIN_HINT_NEG))
#define IS_NEG_CHAIN_HINT(P) ((uintptr_t)(P) & TAG_CHAIN_HINT_NEG)
#define GET_CHAIN_HINT_PTR(P) ((struct clause *)((uintptr_t)(P) & ~TAG_CHAIN_HINT_NEG))

#define all_literals_in_clause(L, C)                                                     \
    literal_t L, *L##_begin = (C).literals, *L##_end = (C).literals + (C).literal_count; \
    (L##_begin != L##_end) && (L = *L##_begin, true);                                    \
    ++L##_begin

#define all_chain_clause_ptrs_in_clause_chain(CP, CH)                                 \
    clause_t *CP, **CP##_begin = (CH).clauses, **CP##_end = (CH).clauses + (CH).size; \
    (CP##_begin != CP##_end) && (CP = *CP##_begin, true);                             \
    ++CP##_begin

#define bit_vector_init(max_variable)                               \
    do                                                              \
    {                                                               \
        bit_vector = calloc((max_variable << 1) + 1, sizeof(bool)); \
    } while (0)

#define bit_vector_set_clause_literals(C)                \
    do                                                   \
    {                                                    \
        for (unsigned i = 0; i < (C).literal_count; i++) \
            bit_vector[(C).literals[i]] = true;          \
    } while (0)

#define bit_vector_set_clause_pivots(C)               \
    do                                                \
    {                                                 \
        for (unsigned i = 0; i < (C).chain.size; i++) \
            bit_vector[(C).chain.pivots[i]] = true;   \
    } while (0)

#define bit_vector_clear_clause_literals(C)              \
    do                                                   \
    {                                                    \
        for (unsigned i = 0; i < (C).literal_count; i++) \
            bit_vector[(C).literals[i]] = false;         \
    } while (0)

#define bit_vector_clear_clause_pivots(C)                 \
    do                                                    \
    {                                                     \
        for (unsigned i = 0; i < (C).chain.size - 1; i++) \
            bit_vector[(C).chain.pivots[i]] = false;      \
    } while (0)

#endif /* __clause_h__ */
