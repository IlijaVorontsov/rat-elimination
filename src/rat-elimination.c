#include "proof.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

volatile sig_atomic_t quit = 0;
void handle_signal(int signal);

struct proof proof;
void cleanup(void);

void update_next_rat(struct proof *proof);
void mark_purity(struct proof proof);
void transform_chains_and_do_todos(struct clause_ptr_stack *stack_ptr, struct clause *rat_clause_ptr);
void do_rat_todos(struct clause_ptr_stack *stack_ptr);
void do_remaining_todos(struct proof *proof_ptr);

void D(struct clause_ptr_stack *rho_prime, struct clause *D_ptr, literal_t l,
       struct clause **chain_begin, struct clause **chain_end, struct literal_stack *literals, struct clause *C_ptr);

int main(int argc, char *argv[]) {
    // to save progress on ctrl-c or kill
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    ASSERT_ERROR(argc == 3, "Usage: %s <dimacs-file> <lrat-file>\n", argv[0]);

    atexit(cleanup);
    proof = proof_from_dimacs_lrat(argv[1], argv[2]);
    update_next_rat(&proof);
    // proof_print(proof);
    while (proof.next_rat && !quit) {
        struct clause *next_rat_ptr = *GET_PTR(proof.next_rat->begin);
        proof_update_indices(proof);
        mark_purity(proof);
        for (all_stacks_up_to_next_rat(stack, proof)) {
            transform_chains_and_do_todos(stack_ptr, next_rat_ptr);
        }
        do_rat_todos(proof.next_rat);
        update_next_rat(&proof);
    }
    do_remaining_todos(&proof);

    proof_print(proof);

    // clause_ptr_stack_print((*(proof.end - 1)->begin)->chain, NULL);
    exit(EXIT_SUCCESS);
}

struct clause *E_star(struct clause *E_ptr, struct clause *D_ptr, literal_t l, struct clause *C_ptr) {
    if (literal_in_clause(-l, E_ptr)) {
        struct clause *max = E_ptr->index > D_ptr->index ? E_ptr : D_ptr;
        struct clause *min = D_ptr->index < E_ptr->index ? D_ptr : E_ptr;

        // find todo in max
        for (all_todos_in_stack(todo, max->todos)) {
            if (todo.other == min) {
                return todo.result;
            }
        }
#ifdef DEBUG
        printf("[%llu, %llu] \n", max->index, min->index);
        fflush(stdout);
#endif /* DEBUG */
        struct clause *result = resolve(D_ptr, E_ptr, l);
        if (max == C_ptr) { // rat clause is the other one
            CLEAR(result->chain);
            result->chain = get_neg_chain(C_ptr, min);
        } else if (max->index > C_ptr->index) {
            CLEAR(result->chain);
            struct literal_stack *literals = clause_get_literals(max);
            D(&result->chain, min, max == E_ptr ? l : -l, max->chain.begin, max->chain.end, literals, C_ptr);
            RELEASE(*literals);
        }
        struct todo todo = {min, result}; // -l in E
        PUSH(max->todos, todo);
        return todo.result;
    } else {
        return E_ptr;
    }
}

#define all_clause_ptrs_from_to(clause_ptr, begin, end)                                                             \
    struct clause *clause_ptr, **clause_ptr##_ptr = begin, **clause_ptr##_end = end, **clause_ptr##_last = end - 1; \
    (clause_ptr##_ptr != clause_ptr##_end) && ((clause_ptr = *clause_ptr##_ptr), true);                             \
    ++clause_ptr##_ptr

void D(struct clause_ptr_stack *rho_prime, struct clause *D_ptr, literal_t l,
       struct clause **chain_begin, struct clause **chain_end, struct literal_stack *literals, struct clause *C_ptr) {
    for (all_clause_ptrs_from_to(E_ptr, chain_begin, chain_end)) {
        if (E_ptr_ptr == E_ptr_last) {
            PUSH(*rho_prime, E_star(E_ptr, D_ptr, l, C_ptr));
            return;
        }
        literal_t k = clause_get_reverse_resolvent(literals, E_ptr);

        if (k == -l || literal_in_clause(k, D_ptr)) { // k \in D \cup {-l}
            continue;
        } else if (-k != l && literal_in_clause(-k, D_ptr)) { // -k \in D \\ {-l}
            PUSH(*rho_prime, E_star(E_ptr, D_ptr, l, C_ptr));
            return;
        } else if (!literal_in_clause(k, D_ptr) && !literal_in_clause(-k, D_ptr)) { // k, -k \notin D
            PUSH(*rho_prime, E_star(E_ptr, D_ptr, l, C_ptr));
            continue;
        } else {
            assert(0);
        }
    }
}

void do_rat_todos(struct clause_ptr_stack *rat_stack_ptr) {
    rat_stack_ptr->begin = GET_PTR(rat_stack_ptr->begin);
    struct clause *rat_ptr = POP(*rat_stack_ptr);
    struct todo_stack todos = rat_ptr->todos;
    while (!EMPTY(todos)) {
        PUSH(*rat_stack_ptr, POP(todos).result);
    }
    clause_release(rat_ptr);
}

void do_remaining_todos(struct proof *proof_ptr) {
    if (!proof_ptr->next_rat) {
        struct clause_ptr_stack *stack_ptr = proof_ptr->begin;
        struct clause_ptr_stack results = {0, 0, 0};
        for (all_clause_ptrs_in_stack(clause_ptr, *stack_ptr)) {
            if (clause_ptr == NULL)
                continue;
            struct todo_stack todos = clause_ptr->todos;
            while (!EMPTY(todos)) {
                PUSH(results, POP(todos).result);
            }
            RELEASE(clause_ptr->todos);
        }
        while (!EMPTY(results)) {
            PUSH(*stack_ptr, POP(results));
        }
        RELEASE(results);
    } else {
        for (all_stacks_before_next_rat(stack, *proof_ptr)) {
            for (all_clause_ptrs_in_stack(clause_ptr, stack)) {
                if (clause_ptr == NULL)
                    continue;
                struct todo_stack todos = clause_ptr->todos;
                while (!EMPTY(todos)) {
                    PUSH(*stack_ptr, POP(todos).result);
                }
                RELEASE(clause_ptr->todos);
            }
        }
    }
}

void transform_chains_and_do_todos(struct clause_ptr_stack *stack, struct clause *C) {
    literal_t l = C->literals[0];
    // todo's first
    struct clause_ptr_stack todo_stack = {0, 0, 0};
    for (all_clause_ptrs_in_stack(clause_ptr, *stack)) {
        if (clause_ptr == NULL)
            continue;
        uint8_t purity = GET_PURITY_TAG(clause_ptr);
        clause_ptr = GET_CLAUSE_PTR(clause_ptr);

        struct todo_stack todos = clause_ptr->todos;
        while (!EMPTY(todos)) {
            PUSH(todo_stack, POP(todos).result);
        }
        RELEASE(clause_ptr->todos);

        struct clause_ptr_stack chain = {0, 0, 0};
        struct literal_stack *literals_ptr;
        switch (purity) {
        case TAG_IMPURE:
            clause_release(clause_ptr);
            *clause_ptr_ptr = NULL;
            break;
        case TAG_SEMIPURE:
            literals_ptr = clause_get_literals(clause_ptr);
            for (all_clause_ptrs_in_stack(chain_clause_ptr, clause_ptr->chain)) {
                literal_t k = clause_get_reverse_resolvent(literals_ptr, chain_clause_ptr);
                if (k == l) { // left-semipure
                    D(&chain, chain_clause_ptr, -l, chain_clause_ptr_ptr + 1, chain_clause_ptr_end, literals_ptr, C);
                    break;
                } else if (k == -l) { // right-semipure
                    D(&chain, chain_clause_ptr, l, chain_clause_ptr_ptr + 1, chain_clause_ptr_end, literals_ptr, C);
                    break;
                } else {
                    PUSH(chain, chain_clause_ptr);
                }
            }
            RELEASE(*literals_ptr);
            RELEASE(clause_ptr->chain);
            clause_ptr->chain = chain;
            *clause_ptr_ptr = clause_ptr; // remove purity tag
            break;
        case TAG_PURE:
            break;
        default:
            assert(0);
        }
    }

    for (all_clause_ptrs_in_stack(clause_ptr, todo_stack)) {
        PUSH(*stack, clause_ptr);
    }
    RELEASE(todo_stack);
}

#define all_stacks_after_next_rat(stack, proof)                                                     \
    struct clause_ptr_stack stack, *stack##_ptr = (proof).next_rat + 1, *stack##_end = (proof).end; \
    (stack##_ptr != stack##_end) && ((stack = *stack##_ptr), true);                                 \
    ++stack##_ptr

void mark_purity(struct proof proof) {
    struct clause_ptr_stack impure = {0, 0, 0};
    struct clause *rat_clause_ptr = *GET_PTR(proof.next_rat->begin);
    literal_t rat_literal = rat_clause_ptr->literals[0];
    PUSH(impure, rat_clause_ptr);
    bool found;

    for (all_stacks_after_next_rat(stack, proof)) {
        for (all_clause_ptrs_in_stack(clause_ptr, stack)) {
            if (!clause_ptr)
                continue;
            found = false;
            struct clause_ptr_stack chain = clause_ptr->chain;
            for (all_clause_ptrs_in_stack(chain_clause_ptr, chain)) {
                for (all_clause_ptrs_in_stack(impure_clause_ptr, impure)) {
                    if (impure_clause_ptr == chain_clause_ptr) {
                        if (literal_in_clause(rat_literal, clause_ptr)) {
                            *clause_ptr_ptr = SET_PURITY_TAG(clause_ptr, TAG_IMPURE);
                            PUSH(impure, clause_ptr);
                            found = true;
                            break;
                        } else {
                            *clause_ptr_ptr = SET_PURITY_TAG(clause_ptr, TAG_SEMIPURE);
                            found = true;
                            break;
                        }
                    }
                }
                if (found)
                    break;
            }
        }
    }
    RELEASE(impure);
}

void update_next_rat(struct proof *proof) {
    for (all_stacks_before_next_rat(stack, *proof)) {
        if (IS_RAT(stack.begin)) {
            proof->next_rat = stack_ptr;
            return;
        }
    }
    proof->next_rat = NULL;
}

void handle_signal(int signal) {
    quit = 1;
}

void cleanup(void) {
    proof_release(proof);
}
