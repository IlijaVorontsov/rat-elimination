#include "proof.h"

#include "stack.h"
#include "msg.h"

/**
 * @brief Releases the memory allocated for the proof structure.
 *
 * This function iterates through each clause in the proof and releases the memory for each clause.
 * Finally, it frees the memory allocated for the rat_clauses array.
 *
 * @param proof The proof structure to release memory for.
 */
void proof_release(struct proof proof)
{
    clause_t *current = proof.begin,
             *next;
    while (current)
    {
        next = current->next;
        clause_release(current);
        current = next;
    }
}

/**
 * @brief Prints the proof excluding the
 *
 * @param stream The stream to print to.
 * @param proof The proof to print.
 */
void proof_fprint(FILE *stream, struct proof proof)
{
    index_t i = 1;
    struct clause *current = proof.begin;
    while (current)
    {
        current->index = i++;
        if (!is_dimacs_clause(*current))
            clause_fprint(stream, *current);
        current = current->next;
    }
}

/**
 * @brief Prints the proof including the dimacs clauses with pivots
 *        used for debugging.
 *
 * @param stream
 * @param proof
 */
void proof_fprint_all(FILE *stream, struct proof proof)
{
    struct clause *current = proof.begin;
    while (current)
    {
        clause_fprint_with_pivots(stream, *current);
        current = current->next;
    }
}

/**
 * @brief Prints the non-dimacs proof (as lrat) with deletions and unifies subsumptions that are equal.
 *
 * @param stream The stream to print to.
 * @param proof The proof to print.
 * @param print_pivots If true, the pivots are printed.
 */
void proof_fprint_final(FILE *stream, struct proof proof, bool print_pivots)
{

    struct clause *current_ptr = proof.begin;

    index_t index = 0;
    // skipping dimacs clauses
    for (clause_t current = *current_ptr; is_dimacs_clause(current); current_ptr = current.next, current = *current_ptr)
        current_ptr->index = ++index;

    index_t last_dimacs_index = index;

    while (current_ptr) // traversing the proof once, to determine the occurences for future deletions
    {
        clause_t current = *current_ptr;

        // checking if clause is equal to the one in its chain
        if (current.chain.size == 1)
        {
            current_ptr->purity = impure;
            current_ptr->index = current.chain.clauses[0]->index;
            current_ptr = current.next;
            continue;
        }

        for (unsigned i = 0; i < current.chain.size; i++)
        {
            clause_t *chain_ptr = GET_CHAIN_HINT_PTR(current.chain.clauses[i]);
            while (chain_ptr->purity == impure)
                chain_ptr = chain_ptr->chain.clauses[0];

            chain_ptr->hint_clause = (clause_t *)(((index_t)chain_ptr->hint_clause) + 1);
        }
        current_ptr->index = ++index;
        current_ptr->hint_clause = NULL;
        current_ptr = current.next;
    }

    // printing deletion of unused dimacs clauses
    fprintf(stream, "%llu d ", last_dimacs_index);
    for (clause_t current = *proof.begin; is_dimacs_clause(current); current_ptr = current.next, current = *current_ptr)
        if (current.hint_clause == NULL)
            fprintf(stream, "%llu ", current.index);
    fprintf(stream, "0\n");

    struct
    {
        index_t *begin, *end, *allocated;
    } deletions = {NULL, NULL, NULL};

    while (current_ptr)
    {
        clause_t current = *current_ptr;
        if (!(current.purity == impure))
        {
            if (!print_pivots)
                clause_fprint(stream, current);
            else
                clause_fprint_with_pivots(stream, current);

            // removing occurences of chain clauses
            for (all_chain_clause_ptrs_in_clause_chain(chain_clause, current.chain))
            {
                clause_t *chain_ptr = GET_CHAIN_HINT_PTR(chain_clause);

                // finding the original clause for equal clauses
                while (chain_ptr->purity == impure)
                    chain_ptr = chain_ptr->chain.clauses[0];

                index_t index = chain_ptr->index;
                index_t occurences = ((index_t)chain_ptr->hint_clause) - 1;
                chain_ptr->hint_clause = (clause_t *)(occurences);

                if (occurences == 0)
                    PUSH(deletions, index);
            }

            if (!EMPTY(deletions) && current.next) // not the last clause
            {
                fprintf(stream, "%llu d ", current.index);
                for (all_elements_on_stack(index_t, index, deletions))
                    fprintf(stream, "%llu ", index);
                fprintf(stream, "0\n");
            }
            CLEAR(deletions);
        }
        current_ptr = current.next;
    }
    RELEASE(deletions);
}

/**
 * @brief Unlinks (by linking prev and next) and frees clause_ptr.
 * @param clause_ptr The clause to unlink and free.
 * @return clause_t* Pointer to the next clause.
 */
clause_t *proof_unlink_free(clause_t *clause_ptr)
{
    clause_t clause = *clause_ptr,
             *next_ptr = clause.next;
    clause.prev->next = clause.next;
    clause.next->prev = clause.prev;
    clause_release(clause_ptr);
    return next_ptr;
}
