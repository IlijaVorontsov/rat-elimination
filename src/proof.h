#ifndef __proof_h__
#define __proof_h__

#include <stdint.h>

#include "clause.h"
#include "stack.h"

struct proof {
    struct clause_ptr_stack *begin, *end, *allocated,
        *next_rat; // where begin[0] are the dimacs clauses
    literal_t max_variable;
};

struct proof proof_from_dimacs_lrat(const char *dimacs_path,
                                    const char *lrat_path);
void proof_release(struct proof proof);
void proof_print(struct proof proof);
void proof_update_indices(struct proof proof);

#define TAG_RAT 1
#define IS_RAT(P) ((uintptr_t)(P) & TAG_RAT)
#define GET_PTR(P) ((struct clause **)((uintptr_t)(P) & ~TAG_RAT))
#define SET_RAT(P) ((struct clause **)((uintptr_t)(P) | TAG_RAT))

#define all_stacks_in_proof(stack, proof) \
    all_elements_on_stack(struct clause_ptr_stack, stack, proof)

#define all_stacks_up_to_next_rat(stack, proof)                     \
    struct clause_ptr_stack stack, *stack##_ptr = (proof).end - 1,  \
                                   *stack##_end = (proof).next_rat; \
    (stack##_ptr != stack##_end) && ((stack = *stack##_ptr), true); \
    --stack##_ptr

#define all_stacks_before_next_rat(stack, proof)                                                          \
    struct clause_ptr_stack stack, *stack##_ptr = (proof).next_rat - 1, *stack##_end = (proof).begin - 1; \
    (stack##_ptr != stack##_end) && ((stack = *stack##_ptr), true);                                       \
    --stack##_ptr

#endif /* __proof_h__ */
