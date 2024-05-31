#include "clause.h"
#include "msg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern bool *bit_vector;

void clause_reconstruct_pivots(clause_t *clause_ptr)
{
    clause_t clause = *clause_ptr;
    if (clause.is_rat || clause.chain.size <= 1)
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
                    DEBUG_MSG("reconstruct_pivots: tautology detected");
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

int literal_compare(const void *a, const void *b)
{
    return (*(long long int *)a - *(long long int *)b);
}

clause_t *clause_create(unsigned literal_count, literal_t *literals, unsigned chain_size, clause_t **chain, bool is_rat)
{
    clause_t *clause_ptr = malloc(sizeof(struct clause));
    ASSERT_ERROR(clause_ptr, "clause_create: malloc failed");

    literal_t *literals_copy = malloc(sizeof(literal_t) * literal_count);
    ASSERT_ERROR(literals_copy, "clause_create: malloc failed");
    memcpy(literals_copy, literals, sizeof(literal_t) * literal_count);
    qsort(literals_copy, literal_count, sizeof(literal_t), literal_compare);

    clause_t **chain_copy = malloc(sizeof(clause_t *) * chain_size);
    ASSERT_ERROR(chain_copy, "clause_create: malloc failed");
    memcpy(chain_copy, chain, sizeof(clause_t *) * chain_size);

    *clause_ptr = (struct clause){
        .purity = pure,
        .is_rat = is_rat,
        .index = 0,
        .next = NULL,
        .prev = NULL,
        .chain = (struct subsumption_merge_chain){
            .size = chain_size,
            .pivots = NULL,
            .clauses = chain_copy,
        },
        .literal_count = literal_count,
        .hint = is_rat ? literals[0] : literal_undefined,
        .literals = literals_copy,
    };
    clause_reconstruct_pivots(clause_ptr);
    return clause_ptr;
}

void clause_release(clause_t *clause_ptr)
{
    if (clause_ptr == NULL)
        return;
    struct clause clause = *clause_ptr;

    free(clause.literals);
    free(clause.chain.pivots);
    free(clause.chain.clauses);
    free(clause_ptr);
    return;
}

void clause_fprint(FILE *file, struct clause clause)
{
    fprintf(file, "%llu ", clause.index);

    literal_t pivot = literal_undefined;
    if (clause.is_rat && clause.hint != literal_undefined)
    {
        pivot = clause.hint;
        fprintf(file, "%d ", TO_DIMACS(pivot));
    }

    for (all_literals_in_clause(literal, clause))
        if (literal != pivot)
            fprintf(file, "%d ", TO_DIMACS(literal));

    fprintf(file, "0 ");
    for (all_chain_clause_ptrs_in_clause_chain(chain_clause, clause))
    {
        if (chain_clause == NULL)
            continue;
        if (IS_NEG_CHAIN_HINT(chain_clause))
            fprintf(file, "-%llu ", GET_CHAIN_HINT_PTR(chain_clause)->index);
        else
            fprintf(file, "%llu ", GET_CHAIN_HINT_PTR(chain_clause)->index);
    }
    fprintf(file, "0\n");
}

void clause_fprint_with_pivots(FILE *file, clause_t clause)
{
    if (clause.is_rat || clause.chain.size == 0)
    {
        clause_fprint(file, clause);
        return;
    }

    fprintf(file, "%llu ", clause.index);

    for (all_literals_in_clause(literal, clause))
        fprintf(file, "%d ", TO_DIMACS(literal));

    fprintf(file, "0 ");

    unsigned pivot_count = clause.chain.size - 1;
    for (unsigned i = 0; i < pivot_count; i++)
    {
        clause_t *chain_clause_ptr = clause.chain.clauses[i];
        fprintf(file, "%llu [%d] ", chain_clause_ptr->index, clause.chain.pivots != NULL ? TO_DIMACS(clause.chain.pivots[i]) : 0);
    }
    fprintf(file, "%llu 0\n", clause.chain.clauses[pivot_count]->index);
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
