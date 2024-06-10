#ifndef __proof_h__
#define __proof_h__

#include "clause.h"
#include "stack.h"

struct proof
{
    struct clause_stack rat_clauses;
    clause_t *begin, *end, *last_dimacs;
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

void proof_fprint_final(FILE *stream, struct proof proof, bool print_pivots);

/**
 * @brief Unlinks (by linking prev and next) and frees clause_ptr.
 * @return pointer to the next clause.
 */
clause_t *proof_unlink_free(clause_t *clause_ptr);

#endif /* __proof_h__ */
