#include "clause.h"
#include "msg.h"
#include "stack.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stats.h"

void clause_release(clause_t *clause_ptr)
{
    ASSERT_ERROR(clause_ptr, "clause_release: clause_ptr is NULL");
    struct clause clause = *clause_ptr;
    free(clause.literals);
    free(clause.chain.pivots);
    free(clause.chain.clauses);
    free(clause_ptr);
    increment_stat(deletions);
}

void clause_reconstruct_pivots(clause_t *clause_ptr)
{
    clause_t clause = *clause_ptr;
    if (!is_rup_clause(clause))
        return;

    clause.chain.pivots = malloc(sizeof(literal_t) * clause.chain.size - 1);
    ASSERT_ERROR(clause.chain.pivots, "reconstruct_pivots: malloc failed");

    bit_vector_set_clause_literals(clause);
    unsigned read = 0, write = 0;
    for (const unsigned end = clause.chain.size - 1; read < end; read++, write++)
    {
        clause_t chain_clause = *(clause.chain.clauses[read]);

        if (read != write)
            clause.chain.clauses[write] = clause.chain.clauses[read];

        bool found = false;
        for (int i = 0; i < chain_clause.literal_count; i++)
        {

            if (!bit_vector[chain_clause.literals[i]])
            {
                if (found)
                {
                    fprintf(stderr, "reconstruct_pivots: multiple pivots in chain clause\n");
                    clause_fprint_with_pivots(stderr, chain_clause);
                    clause_fprint_with_pivots(stderr, clause);
                }
                // ASSERT_ERROR(!found, "reconstruct_pivots: multiple pivots in chain clause");
                literal_t neg = NEG(chain_clause.literals[i]);
                if (bit_vector[neg]) // tautology
                {
                    // DEBUG_MSG("reconstruct_pivots: tautology detected");
                    write--;
                }
                else
                {
                    bit_vector[neg] = true;
                    clause.chain.pivots[write] = neg;
                }
                found = true;
            }
        }
        if (!found) // chain clause is a subset of the clause + pivots up to it -> cut the chain off
            break;
    }
    clause.chain.size = write + 1;

    if (read != write)
    {
        clause_ptr->chain.clauses[write] = clause.chain.clauses[read];
        clause.chain.pivots = realloc(clause.chain.pivots, sizeof(literal_t) * write);
        clause.chain.clauses = realloc(clause_ptr->chain.clauses, sizeof(clause_t *) * (clause.chain.size));
    }

    bit_vector_clear_clause_literals(clause);
    bit_vector_clear_clause_pivots(clause);
    clause_ptr->chain.pivots = clause.chain.pivots;
    clause_ptr->chain.size = clause.chain.size;
}

clause_t *resolve(clause_t left, clause_t right, literal_t resolvent, clause_t **chain, literal_t *pivots, unsigned chain_size)
{
    literal_t neg_resolvent = NEG(resolvent);
    literal_t left_lit, right_lit,
        *left_lit_ptr = left.literals,
        *left_lit_end = left_lit_ptr + left.literal_count,
        *right_lit_ptr = right.literals,
        *right_lit_end = right_lit_ptr + right.literal_count,
        *result_lit_ptr = malloc(sizeof(literal_t) * (left.literal_count + right.literal_count)),
        *result_lit_start = result_lit_ptr;
    ASSERT_ERROR(result_lit_ptr, "resolve: malloc failed");

    while (true)
    {
        if (left_lit_ptr == left_lit_end && right_lit_ptr == right_lit_end)
            break;
        if (left_lit_ptr == left_lit_end)
        {
            right_lit = *right_lit_ptr++;
            if (right_lit == neg_resolvent || right_lit == resolvent)
                continue;
            *result_lit_ptr++ = right_lit;
        }
        else if (right_lit_ptr == right_lit_end)
        {
            left_lit = *left_lit_ptr++;
            if (left_lit == resolvent || left_lit == neg_resolvent)
                continue;
            *result_lit_ptr++ = left_lit;
        }
        else
        {
            left_lit = *left_lit_ptr;
            right_lit = *right_lit_ptr;
            if (left_lit == right_lit)
            {
                left_lit_ptr++;
                right_lit_ptr++;
                *result_lit_ptr++ = left_lit;
            }
            else if (left_lit < right_lit)
            {
                left_lit_ptr++;
                if (left_lit == resolvent || left_lit == neg_resolvent)
                    continue;
                *result_lit_ptr++ = left_lit;
            }
            else
            {
                right_lit_ptr++;
                if (right_lit == neg_resolvent || right_lit == resolvent)
                    continue;
                *result_lit_ptr++ = right_lit;
            }
        }
    }
    unsigned result_lit_count = result_lit_ptr - result_lit_start;
    result_lit_start = realloc(result_lit_start, sizeof(literal_t) * (result_lit_count));
    ASSERT_ERROR(result_lit_start, "resolve: realloc failed");

    clause_t *clause_ptr = malloc(sizeof(clause_t));
    ASSERT_ERROR(clause_ptr, "resolve: malloc failed");
    *clause_ptr = (struct clause){
        .purity = todo,
        .is_rat = false,
        .index = left.index < right.index ? left.index : right.index,
        .next = NULL,
        .prev = NULL,
        .literal_count = result_lit_count,
        .hint = literal_undefined,
        .hint_clause = NULL,
        .literals = result_lit_start,
        .chain = (struct subsumption_merge_chain){
            .size = chain_size,
            .pivots = pivots,
            .clauses = chain,
        },
    };
    return clause_ptr;
}

/**
 * @brief Searches for a literal in a sorted clause using an optimized hybrid approach.
 *
 * Uses different strategies based on clause size:
 * - Small clauses (<=4): Direct comparison
 * - Medium clauses (5-32): Unrolled linear search
 * - Large clauses (>32): Binary search
 *
 * Benchmark results show:
 * - Small sizes:  1.1x-1.6x faster than alternatives
 * - Medium sizes: 1.0x-1.2x faster
 * - Large sizes:  Binary search consistently faster
 *
 * @param literal The literal to search for
 * @param clause The clause to search in (must be sorted by variable index)
 * @return true if literal found, false otherwise
 */
bool literal_in_clause(literal_t literal, clause_t clause)
{
    const size_t size = clause.literal_count;
    const literal_t *literals = clause.literals;

    // Handle small clauses (<=4) with direct comparison
    if (size <= 4)
    {
        switch (size)
        {
        case 0:
            return false;
        case 1:
            return literals[0] == literal;
        case 2:
            return literals[0] == literal ||
                   literals[1] == literal;
        case 3:
            return literals[0] == literal ||
                   literals[1] == literal ||
                   literals[2] == literal;
        case 4:
            return literals[0] == literal ||
                   literals[1] == literal ||
                   literals[2] == literal ||
                   literals[3] == literal;
        }
    }

    // Handle medium-sized clauses (5-32) with unrolled linear search
    if (size <= 32)
    {
        const literal_t *p = literals;
        const literal_t *end = p + size;

        // Calculate the end point for 8-element chunks
        // size & ~7 rounds down to the nearest multiple of 8
        // Example: for size 30, ~7 is ...11111000
        //         so 30 & ~7 = 24, giving us 3 complete chunks
        const literal_t *aligned_end = p + (size & ~7);
        while (p < aligned_end)
        {
            // Unrolled comparison of 8 elements
            if (p[0] == literal || p[1] == literal ||
                p[2] == literal || p[3] == literal ||
                p[4] == literal || p[5] == literal ||
                p[6] == literal || p[7] == literal)
            {
                return true;
            }
            p += 8;
        }

        // Handle remaining elements
        while (p < end)
        {
            if (*p == literal)
                return true;
            p++;
        }
        return false;
    }

    // Handle large clauses (>32) with binary search
    {
        const literal_t lit_var = VAR(literal);
        literal_t *begin = clause.literals;
        literal_t *end = begin + size;

        while (begin < end)
        {
            literal_t *mid = begin + (end - begin) / 2;
            literal_t mid_val = *mid;
            literal_t mid_var = VAR(mid_val);

            if (mid_var == lit_var)
            {
                // Since a clause cannot contain both a literal and its negation,
                // if we found the same variable, we can just check equality
                return mid_val == literal;
            }
            else if (mid_var < lit_var)
            {
                begin = mid + 1;
            }
            else
            {
                end = mid;
            }
        }
        return false;
    }
}

/**
 * @brief Searches for a variable in a clause and returns sign information
 *
 * @param literal The literal to search for
 * @param clause The clause to search in (must be sorted by variable index)
 * @return signed char:
 *         1  if literal found exactly as is
 *         -1 if variable found but with opposite sign
 *         0  if variable not found
 */
signed char var_in_clause(literal_t literal, clause_t clause)
{
    const literal_t var = VAR(literal);

    // Early exit for empty clause
    if (clause.literal_count == 0)
        return 0;

    // Early range check for larger clauses
    if (clause.literal_count > 4)
    {
        if (var < VAR(clause.literals[0]) ||
            var > VAR(clause.literals[clause.literal_count - 1]))
        {
            return 0;
        }
    }

    // Special case for tiny clauses (≤2) since benchmark shows they're sensitive
    if (clause.literal_count <= 2)
    {
        literal_t lit0 = clause.literals[0];
        if (VAR(lit0) == var)
            return lit0 == literal ? 1 : -1;
        if (clause.literal_count == 2)
        {
            literal_t lit1 = clause.literals[1];
            if (VAR(lit1) == var)
                return lit1 == literal ? 1 : -1;
        }
        return 0;
    }

    // Small clauses (3-4)
    if (clause.literal_count <= 4)
    {
        literal_t lit0 = clause.literals[0];
        literal_t lit1 = clause.literals[1];
        literal_t lit2 = clause.literals[2];

        if (VAR(lit0) == var)
            return lit0 == literal ? 1 : -1;
        if (VAR(lit1) == var)
            return lit1 == literal ? 1 : -1;
        if (VAR(lit2) == var)
            return lit2 == literal ? 1 : -1;

        if (clause.literal_count == 4)
        {
            literal_t lit3 = clause.literals[3];
            if (VAR(lit3) == var)
                return lit3 == literal ? 1 : -1;
        }
        return 0;
    }

    // For medium-sized clauses (5-32), use unrolled linear search
    if (clause.literal_count <= 32)
    {
        const literal_t *p = clause.literals;
        const literal_t *const end = p + clause.literal_count;

        // Round down to nearest multiple of 8 for unrolled processing
        const literal_t *const aligned_end = p + (clause.literal_count & ~7);

        // Process 8 elements at a time
        while (p < aligned_end)
        {
            // Load 8 literals first to help compiler optimize
            literal_t l0 = p[0], l1 = p[1], l2 = p[2], l3 = p[3];
            literal_t l4 = p[4], l5 = p[5], l6 = p[6], l7 = p[7];

            // Check variables
            if (VAR(l0) == var)
                return l0 == literal ? 1 : -1;
            if (VAR(l1) == var)
                return l1 == literal ? 1 : -1;
            if (VAR(l2) == var)
                return l2 == literal ? 1 : -1;
            if (VAR(l3) == var)
                return l3 == literal ? 1 : -1;
            if (VAR(l4) == var)
                return l4 == literal ? 1 : -1;
            if (VAR(l5) == var)
                return l5 == literal ? 1 : -1;
            if (VAR(l6) == var)
                return l6 == literal ? 1 : -1;
            if (VAR(l7) == var)
                return l7 == literal ? 1 : -1;

            p += 8;
        }

        // Handle remaining elements
        while (p < end)
        {
            literal_t lit = *p++;
            if (VAR(lit) == var)
                return lit == literal ? 1 : -1;
        }
        return 0;
    }

    // For large clauses, use binary search
    {
        literal_t *begin = clause.literals;
        literal_t *end = begin + clause.literal_count;

        while (begin < end)
        {
            literal_t *mid = begin + (end - begin) / 2;
            literal_t mid_lit = *mid;
            literal_t mid_var = VAR(mid_lit);

            if (mid_var == var)
            {
                // No need for extra comparisons - a variable can only appear
                // once in a clause (either positive or negative)
                return mid_lit == literal ? 1 : -1;
            }

            if (mid_var < var)
            {
                begin = mid + 1;
            }
            else
            {
                end = mid;
            }
        }
        return 0;
    }
}

struct subsumption_merge_chain get_chain(struct subsumption_merge_chain rat_chain, clause_t *clause_ptr)
{
    struct clause_stack chain = {0, 0, 0};
    PUSH(chain, clause_ptr);
    bool pre_chain_copied = false;
    bool found = false;
    for (all_chain_clause_ptrs_in_clause_chain(chain_clause_ptr, rat_chain))
    {
        if (!pre_chain_copied)
        {
            if (IS_NEG_CHAIN_HINT(chain_clause_ptr))
                pre_chain_copied = true;
            else
            {
                PUSH(chain, chain_clause_ptr);
                continue;
            }
        }

        if (IS_NEG_CHAIN_HINT(chain_clause_ptr) && GET_CHAIN_HINT_PTR(chain_clause_ptr) == clause_ptr)
            found = true;
        else if (found && IS_NEG_CHAIN_HINT(chain_clause_ptr))
            return (struct subsumption_merge_chain){
                .size = SIZE(chain),
                .pivots = NULL,
                .clauses = chain.begin,
            }; // found post chain end
        else if (found)
            PUSH(chain, chain_clause_ptr);
    }

    ASSERT_ERROR(found, "get_chain: clause not found in chain");
    return (struct subsumption_merge_chain){
        .size = SIZE(chain),
        .pivots = NULL,
        .clauses = chain.begin,
    };
}

static inline void fprint_rat_literals(FILE *file, clause_t clause)
{
    fprintf(file, "%d ", TO_DIMACS(clause.hint));
    for (all_literals_in_clause(literal, clause))
        if (literal != clause.hint)
            fprintf(file, "%d ", TO_DIMACS(literal));
    fprintf(file, "0 ");
}

static inline void fprint_chain(FILE *file, struct subsumption_merge_chain chain)
{
    for (all_chain_clause_ptrs_in_clause_chain(chain_clause_ptr, chain))
    {
        ASSERT_ERROR(chain_clause_ptr, "fprint_chain: chain_clause_ptr is NULL");
        fprintf(file, "%llu ", chain_clause_ptr->index);
    }
    fprintf(file, "0\n");
}

static inline void fprint_chain_with_pivots(FILE *file, struct subsumption_merge_chain chain)
{
    ASSERT_ERROR(chain.pivots, "fprint_chain_with_pivots: chain.pivots is NULL");
    if (chain.size == 0)
    {
        fprintf(file, "0\n");
        return;
    }
    for (unsigned i = 0; i < chain.size - 1; i++)
    {
        clause_t *chain_clause_ptr = chain.clauses[i];
        ASSERT_ERROR(chain_clause_ptr, "fprint_chain_with_pivots: chain_clause_ptr is NULL");
        fprintf(file, "%llu [%d] ", chain_clause_ptr->index, TO_DIMACS(chain.pivots[i]));
    }
    fprintf(file, "%llu 0\n", chain.clauses[chain.size - 1]->index);
}

static inline void fprint_rat_chain(FILE *file, struct subsumption_merge_chain chain)
{
    for (all_chain_clause_ptrs_in_clause_chain(chain_clause_ptr, chain))
    {
        ASSERT_ERROR(chain_clause_ptr, "fprint_rat_chain: chain_clause_ptr is NULL");
        if (IS_NEG_CHAIN_HINT(chain_clause_ptr))
            fprintf(file, "-%llu ", GET_CHAIN_HINT_PTR(chain_clause_ptr)->index);
        else
            fprintf(file, "%llu ", GET_CHAIN_HINT_PTR(chain_clause_ptr)->index);
    }
    fprintf(file, "0\n");
}

static inline void fprint_clause_literals(FILE *file, clause_t clause)
{
    for (all_literals_in_clause(literal, clause))
        fprintf(file, "%d ", TO_DIMACS(literal));
    fprintf(file, "0 ");
}

void clause_fprint(FILE *file, struct clause clause)
{
    fprintf(file, "%llu ", clause.index);
    if (clause.is_rat)
    {
        fprint_rat_literals(file, clause);
        fprint_rat_chain(file, clause.chain);
    }
    else
    {
        fprint_clause_literals(file, clause);
        fprint_chain(file, clause.chain);
    }
}

void clause_fprint_with_pivots(FILE *file, clause_t clause)
{
    if (!is_rup_clause(clause))
        clause_fprint(file, clause);
    else
    {
        fprintf(file, "%llu ", clause.index);
        fprint_clause_literals(file, clause);
        fprint_chain_with_pivots(file, clause.chain);
    }
}
