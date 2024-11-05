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
 * @brief Check if a clause contains a literal. (binary search)
 */
bool literal_in_clause(literal_t literal, clause_t clause)
{
    literal_t *begin = clause.literals;
    literal_t *end = begin + clause.literal_count;

    while (begin < end)
    {
        literal_t *mid = begin + (end - begin) / 2;
        if (*mid == literal)
            return true;
        else if (*mid < literal)
            begin = mid + 1;
        else
            end = mid;
    }
    return false;
}

signed char var_in_clause(literal_t literal, clause_t clause)
{
    literal_t var = VAR(literal),
              *begin = clause.literals,
              *end = begin + clause.literal_count;

    while (begin < end)
    {
        literal_t *mid_ptr = begin + (end - begin) / 2,
                  mid = *mid_ptr,
                  var_mid = VAR(mid);
        if (var_mid == var)
            return mid == literal ? 1 : -1;
        else if (var_mid < var)
            begin = mid_ptr + 1;
        else
            end = mid_ptr;
    }
    return 0;
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
    if (clause.purity == rat)
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
