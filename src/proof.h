#ifndef __proof_h__
#define __proof_h__

#include <stdint.h>

#include "clause.h"
#include "stack.h"

struct proof {
    struct clause_ptr_stack rat_clauses;
    struct clause *begin, *end, *next_rat_ptr, *last_dimacs;
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
void proof_print(struct proof proof);

/**
 * @brief Prints the proof including the dimacs clauses to stdout.
 */
void proof_print_all(struct proof proof);

/**
 * @brief Updates the indices of the clauses in the proof.
 */
void proof_update_indices(struct proof proof);

#endif /* __proof_h__ */
