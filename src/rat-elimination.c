#include "proof.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "msg.h"
#include "stack.h"

bool *bit_vector;
struct proof proof;

void mark_purity(struct proof proof);
void chain_distribution(clause_t *distributing_clause_ptr, literal_t distributing_literal, clause_t *chain_clause_ptr, unsigned index);
void finish_todos(clause_t *current_ptr);

struct clause_stack todo_clauses;

int main(int argc, char *argv[])
{
    ASSERT_ERROR(argc == 3, "Usage: %s <dimacs-file> <lrat-file>\n", argv[0]);

    bit_vector = calloc(UINT32_MAX, sizeof(bool)); // calloc(proof.max_variable + 1) << 1, sizeof(bool));
    ASSERT_ERROR(bit_vector, "main: calloc failed");

    proof = proof_from_dimacs_lrat(argv[1], argv[2]);
    INIT(todo_clauses);
    while (proof.rat_count > 0)
    {
        proof.current_rat_clause = proof.rat_clauses[--proof.rat_count];
        proof.current_rat_pivot = proof.current_rat_clause->hint;
        proof.current_rat_clause_index = proof.current_rat_clause->index;
        fprintf(stderr, "current rat clause index: %llu\n", proof.current_rat_clause_index);
        fprintf(stderr, "remaining rat clauses: %u\n", proof.rat_count);

        mark_purity(proof);
        clause_t *current_ptr = proof.end,
                 current = *current_ptr;
        clause_t *clause_after_rat = proof.current_rat_clause->next;
        while (true) // todo's inserted after the rat_clause have index of a lower clause than the rat_clause
        {

            bool is_last = current_ptr == clause_after_rat;
            switch (current.purity)
            {
            case impure:
            {
                // unlink and free current
                clause_t *tmp = current.next;
                current.prev->next = current.next;
                current.next->prev = current.prev;
                clause_release(current_ptr);
                current_ptr = tmp;
                current = *current_ptr;
                break;
            }
            case semipure:
            {
                chain_distribution(current.chain.clauses[current.hint], NEG(current.chain.pivots[current.hint]), current_ptr, current.hint);
                current_ptr->purity = pure;
                break;
            }
            case pure:
                break;
            case todo:
            {
                chain_distribution(current.hint_clause, current.hint, current_ptr, 0);
                current_ptr->purity = pure;
                break;
            }
            default:
                ASSERT_ERROR(0, "transform_chain: unknown purity");
            }
            current_ptr = current_ptr->prev;
            current = *current_ptr;
            if (is_last)
                break;
        }
        finish_todos(current_ptr);
    }
    proof_fprint(stdout, proof);
    return 0;
}

void finish_todos(clause_t *current_ptr)
{
    for (; current_ptr->purity == todo; current_ptr = current_ptr->prev)
    {
        clause_reconstruct_pivots(current_ptr);
        current_ptr->purity = pure;
    }
    ASSERT_ERROR(current_ptr == proof.current_rat_clause, "finish_todos: current_ptr != proof.current_rat_clause");
    clause_t *next = proof.current_rat_clause->next;

    for (all_pointers_on_stack(clause_t, clause_ptr, todo_clauses))
    {
        clause_t *todo_start = clause_ptr->next;
        current_ptr = todo_start;
        while (current_ptr->purity == todo)
        {
            current_ptr->purity = pure;
            current_ptr = current_ptr->next;
        }
        next->prev = current_ptr->prev;
        current_ptr->prev->next = next;
        proof.current_rat_clause->next = todo_start;
        todo_start->prev = proof.current_rat_clause;
        next = todo_start;

        clause_ptr->next = current_ptr;
        current_ptr->prev = clause_ptr;
    }
    proof.current_rat_clause->prev->next = proof.current_rat_clause->next;
    proof.current_rat_clause->next->prev = proof.current_rat_clause->prev;
    clause_release(proof.current_rat_clause);
    CLEAR(todo_clauses);
}

void mark_purity(struct proof proof)
{
    const literal_t rat_pivot = proof.current_rat_pivot;
    const literal_t neg_rat_pivot = NEG(rat_pivot);
    proof.current_rat_clause->purity = impure;
    clause_t current = *proof.current_rat_clause,
             *current_ptr = proof.current_rat_clause;
    index_t current_index = current.index;
    while (current.next != NULL)
    {
        current_ptr = current.next;
        current = *current_ptr;
        current_ptr->purity = pure;
        current_ptr->index = ++current_index;
        if (literal_in_clause(neg_rat_pivot, current))
        {
            continue;
        }
        else if (literal_in_clause(rat_pivot, current))
        {
            for (unsigned i = 0; i < current.chain.size; i++)
            {
                if (current.chain.clauses[i]->purity == impure)
                {
                    current_ptr->purity = impure;
                    break;
                }
            }
        }
        else
        {
            for (unsigned i = 0, end = current.chain.size - 1; i < end; i++)
            {
                literal_t chain_pivot = current.chain.pivots[i];
                if (chain_pivot == neg_rat_pivot)
                {
                    if (current.chain.clauses[i]->purity == impure)
                        current_ptr->purity = semipure;
                    current_ptr->hint = i;
                    break;
                }
                else if (chain_pivot == rat_pivot)
                {
                    for (int j = i; j < current.chain.size; j++)
                    {
                        if (current.chain.clauses[j]->purity == impure)
                        {
                            current_ptr->purity = semipure;
                            current_ptr->hint = i;
                            break;
                        }
                    }
                    break;
                }
            }
        }
    }
}

struct clause_stack get_neg_chain(clause_t rat_clause, clause_t *clause_ptr)
{
    struct clause_stack chain = {0, 0, 0};
    PUSH(chain, clause_ptr);
    bool pre_chain_copied = false;
    bool found = false;
    for (all_chain_clause_ptrs_in_clause_chain(chain_clause_ptr, rat_clause))
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
            return chain; // reached end of chain with negative hint
        else if (found)
            PUSH(chain, chain_clause_ptr);
    }
    if (!found)
    {
        fprintf(stderr, "get_neg_chain: clause not found in chain\n");
        clause_fprint(stderr, *clause_ptr);
        clause_fprint(stderr, rat_clause);
    }
    assert(found && SIZE(chain) > 0);

    return chain; // reached end of chain without negative hint
}

clause_t *E_star(clause_t *chain_clause_ptr, clause_t *distributing_clause_ptr, literal_t distributing_literal)
{
    clause_t chain_clause = *chain_clause_ptr;
    if (literal_in_clause(NEG(distributing_literal), chain_clause))
    {
        clause_t distributing_clause = *distributing_clause_ptr;
        clause_t higher_index_clause, lower_index_clause,
            *higher_index_clause_ptr, *lower_index_clause_ptr;
        if (distributing_clause.index > chain_clause.index)
        {
            lower_index_clause = chain_clause;
            lower_index_clause_ptr = chain_clause_ptr;
            higher_index_clause = distributing_clause;
            higher_index_clause_ptr = distributing_clause_ptr;
        }
        else
        {
            lower_index_clause = distributing_clause;
            lower_index_clause_ptr = distributing_clause_ptr;
            higher_index_clause = chain_clause;
            higher_index_clause_ptr = chain_clause_ptr;
        }

        clause_t *current_ptr = higher_index_clause.next,
                 current = *current_ptr;

        if (current.purity != todo && higher_index_clause.index < proof.current_rat_clause_index)
        {
            PUSH(todo_clauses, higher_index_clause_ptr);
        }
        // looking for the same todo
        while (current.purity == todo)
        {
            if (current.index == lower_index_clause.index)
                return current_ptr;
            current_ptr = current.next;
            current = *current_ptr;
        }

        clause_t *result;
        // three of todo's
        // 1. todo clauses where the higher of the two indices is higher than the current rat clause index
        // fprintf(stderr, "creating todo [%llu] [%llu]\n", higher_index_clause.index, lower_index_clause.index);
        // fprintf(stderr, "current rat clause index: %llu\n", proof.current_rat_clause_index);
        if (higher_index_clause.index > proof.current_rat_clause_index)
        {
            clause_t **chain = malloc(sizeof(clause_t *) * (higher_index_clause.chain.size));
            memcpy(chain, higher_index_clause.chain.clauses, sizeof(clause_t *) * higher_index_clause.chain.size);
            literal_t *pivots = malloc(sizeof(literal_t) * (higher_index_clause.chain.size - 1));
            memcpy(pivots, higher_index_clause.chain.pivots, sizeof(literal_t) * (higher_index_clause.chain.size - 1));
            result = resolve(distributing_clause, chain_clause, distributing_literal, chain, pivots, higher_index_clause.chain.size);
            result->hint = lower_index_clause_ptr == distributing_clause_ptr ? distributing_literal : NEG(distributing_literal);
            result->hint_clause = lower_index_clause_ptr;
        }
        // 2. todo clauses where the higher index is the rat index
        else if (higher_index_clause.index == proof.current_rat_clause_index)
        {
            struct clause_stack chain = get_neg_chain(*proof.current_rat_clause, lower_index_clause_ptr);
            result = resolve(distributing_clause, chain_clause, distributing_literal, chain.begin, NULL, SIZE(chain));
        }
        // 3. todo clauses where the higher index is lower than the rat index
        else
        {
            clause_t **chain = malloc(sizeof(clause_t *) * 2);
            chain[0] = distributing_clause_ptr;
            chain[1] = chain_clause_ptr;
            literal_t *pivots = malloc(sizeof(literal_t));
            pivots[0] = distributing_literal;
            result = resolve(distributing_clause, chain_clause, distributing_literal, chain, pivots, 2);
        }
        higher_index_clause.next->prev = result;
        result->next = higher_index_clause.next;
        result->prev = higher_index_clause_ptr;
        higher_index_clause_ptr->next = result;

        return result;
    }
    else
    {
        return chain_clause_ptr;
    }
}

void chain_distribution(clause_t *distributing_clause_ptr, literal_t distributing_literal, clause_t *chain_clause_ptr, unsigned index)
{
    clause_t distributing_clause = *distributing_clause_ptr;
    bit_vector_set_clause_literals(distributing_clause);

    struct subsumption_merge_chain chain = chain_clause_ptr->chain;
    unsigned write_index = index;

    // special case if index points to distributing clause (Writes over it later, since write pointer still points to it)
    if (chain.clauses[index] == distributing_clause_ptr)
        index++;

    for (; index < chain.size - 1; index++, write_index++)
    {
        literal_t current_pivot = chain.pivots[index];
        if (current_pivot == NEG(distributing_literal) || bit_vector[current_pivot])
        {
            write_index--;
        }
        else if (current_pivot != NEG(distributing_literal) && bit_vector[NEG(current_pivot)])
        {
            break;
        }
        else if (!bit_vector[NEG(current_pivot)] && !bit_vector[current_pivot])
        {
            chain.clauses[write_index] = E_star(chain.clauses[index], distributing_clause_ptr, distributing_literal);
            chain.pivots[write_index] = chain.pivots[index];
        }
    }
    chain.clauses[write_index] = E_star(chain.clauses[index], distributing_clause_ptr, distributing_literal);
    chain_clause_ptr->chain.size = write_index + 1;
    bit_vector_clear_clause_literals(distributing_clause);
}
