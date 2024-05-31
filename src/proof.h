#ifndef __proof_h__
#define __proof_h__

#include <stdint.h>

#include "clause.h"

struct proof
{
    unsigned rat_count;
    index_t current_rat_clause_index;
    literal_t current_rat_pivot;
    clause_t **rat_clauses;
    clause_t *begin, *end, *current_rat_clause, *last_dimacs;
};

struct proof proof_from_dimacs_lrat(const char *dimacs_path,
                                    const char *lrat_path);

/**
 * @brief Frees all clauses and the rat_clauses stack.
 */
void proof_release(struct proof proof);

/**
 * @brief Prints the proof in lrat format to stdout.
 */
void proof_fprint(FILE *stream, struct proof proof);

/**
 * @brief Prints the proof including the dimacs clauses to stdout.
 */
void proof_fprint_all(FILE *stream, struct proof proof);

#endif /* __proof_h__ */
