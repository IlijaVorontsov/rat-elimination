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
    free(clause.chain.pivots);
    free(clause.chain.clauses);
    free(clause_ptr);
    increment_stat(deletions);
}

void clause_reconstruct_pivots(clause_t *clause_ptr)
{
    if (!is_rup_clause(clause_ptr))
        return;

    clause_ptr->chain.pivots = malloc(sizeof(literal_t) * (clause_ptr->chain.size - 1));
    ASSERT_ERROR(clause_ptr->chain.pivots, "reconstruct_pivots: malloc failed");

    bit_vector_set_clause_literals(clause_ptr);
    unsigned read = 0, write = 0;
    for (const unsigned end = clause_ptr->chain.size - 1; read < end; read++, write++)
    {
        clause_t *chain_clause_ptr = clause_ptr->chain.clauses[read];

        if (read != write)
            clause_ptr->chain.clauses[write] = clause_ptr->chain.clauses[read];

        bool found = false;
        for (unsigned i = 0; i < chain_clause_ptr->literal_count; i++)
        {
            if (!bit_vector[chain_clause_ptr->literals[i]])
            {
                if (found)
                {
                    fprintf(stderr, "reconstruct_pivots: multiple pivots in chain clause\n");
                    clause_fprint_with_pivots(stderr, chain_clause_ptr);
                    clause_fprint_with_pivots(stderr, clause_ptr);
                }
                literal_t neg = NEG(chain_clause_ptr->literals[i]);
                if (bit_vector[neg])
                {
                    write--;
                }
                else
                {
                    bit_vector[neg] = true;
                    clause_ptr->chain.pivots[write] = neg;
                }
                found = true;
            }
        }
        if (!found)
            break;
    }
    clause_ptr->chain.size = write + 1;

    if (read != write)
    {
        clause_ptr->chain.clauses[write] = clause_ptr->chain.clauses[read];
        clause_ptr->chain.pivots = realloc(clause_ptr->chain.pivots, sizeof(literal_t) * write);
        clause_ptr->chain.clauses = realloc(clause_ptr->chain.clauses, sizeof(clause_t *) * (clause_ptr->chain.size));
    }

    bit_vector_clear_clause_literals(clause_ptr);
    bit_vector_clear_clause_pivots(clause_ptr);
}

clause_t *resolve(clause_t *left_ptr, clause_t *right_ptr, literal_t resolvent,
                  clause_t **chain, literal_t *pivots, unsigned chain_size)
{
    literal_t neg_resolvent = NEG(resolvent);
    literal_t left_lit, right_lit,
        *left_lit_ptr = left_ptr->literals,
        *left_lit_end = left_lit_ptr + left_ptr->literal_count,
        *right_lit_ptr = right_ptr->literals,
        *right_lit_end = right_lit_ptr + right_ptr->literal_count;

    // Allocate maximum possible size for temporary storage
    literal_t *temp_literals = malloc(sizeof(literal_t) * (left_ptr->literal_count + right_ptr->literal_count));
    ASSERT_ERROR(temp_literals, "resolve: malloc failed %s", "");
    literal_t *result_lit_ptr = temp_literals;

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

    unsigned result_lit_count = result_lit_ptr - temp_literals;

    // Allocate the final clause with exact size needed for literals
    clause_t *clause_ptr = malloc(sizeof(clause_t) + sizeof(literal_t) * result_lit_count);
    ASSERT_ERROR(clause_ptr, "resolve: malloc failed %s", "");

    // Copy literals to flexible array member
    memcpy(clause_ptr->literals, temp_literals, sizeof(literal_t) * result_lit_count);
    free(temp_literals);

    // Initialize the rest of the clause
    *clause_ptr = (struct clause){
        .purity = todo,
        .is_rat = false,
        .index = left_ptr->index < right_ptr->index ? left_ptr->index : right_ptr->index,
        .next = NULL,
        .prev = NULL,
        .literal_count = result_lit_count,
        .hint = literal_undefined,
        .hint_clause = NULL,
        .chain = (struct subsumption_merge_chain){
            .size = chain_size,
            .pivots = pivots,
            .clauses = chain,
        }};

    return clause_ptr;
}

/**
 * @brief Check if a clause contains a literal. (binary search)
 */
bool literal_in_clause(literal_t literal, clause_t *clause_ptr)
{
    literal_t *begin = clause_ptr->literals;
    literal_t *end = begin + clause_ptr->literal_count;

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

signed char var_in_clause(literal_t literal, clause_t *clause_ptr)
{
    literal_t var = VAR(literal),
              *begin = clause_ptr->literals,
              *end = begin + clause_ptr->literal_count;

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

void fprint_rat_literals(FILE *file, clause_t *clause)
{
    fprintf(file, "%d ", TO_DIMACS(clause->hint));
    for (all_literals_in_clause(literal, clause))
        if (literal != clause->hint)
            fprintf(file, "%d ", TO_DIMACS(literal));
    fprintf(file, "0 ");
}

void fprint_chain(FILE *file, struct subsumption_merge_chain chain)
{
    for (all_chain_clause_ptrs_in_clause_chain(chain_clause_ptr, chain))
    {
        ASSERT_ERROR(chain_clause_ptr, "fprint_chain: chain_clause_ptr is NULL %s", "");
        fprintf(file, "%llu ", chain_clause_ptr->index);
    }
    fprintf(file, "0\n");
}

void fprint_chain_with_pivots(FILE *file, struct subsumption_merge_chain chain)
{
    ASSERT_ERROR(chain.pivots, "fprint_chain_with_pivots: chain.pivots is NULL %s", "");
    if (chain.size == 0)
    {
        fprintf(file, "0\n");
        return;
    }
    for (unsigned i = 0; i < chain.size - 1; i++)
    {
        clause_t *chain_clause_ptr = chain.clauses[i];
        ASSERT_ERROR(chain_clause_ptr, "fprint_chain_with_pivots: chain_clause_ptr is NULL %s", "");
        fprintf(file, "%llu [%d] ", chain_clause_ptr->index, TO_DIMACS(chain.pivots[i]));
    }
    fprintf(file, "%llu 0\n", chain.clauses[chain.size - 1]->index);
}

void fprint_clause_literals(FILE *file, clause_t *clause)
{
    for (all_literals_in_clause(literal, clause))
        fprintf(file, "%d ", TO_DIMACS(literal));
    fprintf(file, "0 ");
}

void clause_fprint(FILE *file, clause_t *clause)
{
    fprintf(file, "%llu ", clause->index);
    if (clause->is_rat)
    {
        fprint_rat_literals(file, clause);
        fprint_rat_chain(file, clause->chain);
    }
    else
    {
        fprint_clause_literals(file, clause);
        fprint_chain(file, clause->chain);
    }
}

void clause_fprint_with_pivots(FILE *file, clause_t *clause)
{
    if (!is_rup_clause(clause))
        clause_fprint(file, clause);
    else
    {
        fprintf(file, "%llu ", clause->index);
        fprint_clause_literals(file, clause);
        fprint_chain_with_pivots(file, clause->chain);
    }
}

void fprint_rat_chain(FILE *file, struct subsumption_merge_chain chain)
{
    for (all_chain_clause_ptrs_in_clause_chain(chain_clause_ptr, chain))
    {
        ASSERT_ERROR(chain_clause_ptr, "fprint_rat_chain: chain_clause_ptr is NULL %s", "");
        if (IS_NEG_CHAIN_HINT(chain_clause_ptr))
            fprintf(file, "-%llu ", GET_CHAIN_HINT_PTR(chain_clause_ptr)->index);
        else
            fprintf(file, "%llu ", chain_clause_ptr->index);
    }
    fprintf(file, "0\n");
}