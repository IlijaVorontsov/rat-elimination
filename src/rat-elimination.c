#include "proof.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

volatile sig_atomic_t quit = 0;
void handle_signal(int signal);

struct proof proof;
bool *literal_array;
struct literal_stack set_literals;

void cleanup(void);

void update_next_rat(struct proof *proof);
void mark_purity(struct proof proof);
void transform_chain_and_do_todos(struct clause *clause_ptr, literal_t l);
void do_remaining_todos(struct proof proof);

void D(struct clause_ptr_stack *rho_prime, struct clause *D_ptr, literal_t l,
       struct clause **chain_begin, struct clause **chain_end);

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
    literal_array = calloc(UINT32_MAX, sizeof(bool)); // calloc(proof.max_variable + 1) << 1, sizeof(bool));
    INIT(set_literals);
    ASSERT_ERROR(literal_array, "main: calloc failed");
    while (!EMPTY(proof.rat_clauses) && !quit) {
        proof.next_rat_ptr = POP(proof.rat_clauses);
        proof_update_indices(proof);
        fprintf(stderr, "%lu RAT clauses remaining\n", SIZE(proof.rat_clauses));
        fprintf(stderr, "Next rat: %u\n", proof.next_rat_ptr->index);
        literal_t l = proof.next_rat_ptr->pivot;
        mark_purity(proof);
        struct clause *current = proof.end;
        struct clause *next;
        while (current != proof.next_rat_ptr) {
            next = current->prev;
            transform_chain_and_do_todos(current, l);
            current = next;
        }
        do_remaining_todos(proof);
    }

    proof_print(proof);

    exit(EXIT_SUCCESS);
}

struct clause *E_star(struct clause *E_ptr, struct clause *D_ptr, literal_t l) {
    if (literal_in_clause(NEG(l), E_ptr)) {
        index_t D_index = D_ptr->index;
        index_t E_index = E_ptr->index;
        struct clause *min = D_index < E_index ? D_ptr : E_ptr;
        struct clause *max = D_index < E_index ? E_ptr : D_ptr;

        // looking for todo in maximal clause
        for (all_todos_in_stack(todo, max->todos)) {
            if (todo.other == min) {
                return todo.result;
            }
        }

        // creating resulting clause
        struct clause *result = resolve(D_ptr, E_ptr, l);
        result->pivot = l;
        struct todo todo = {min, result};
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
       struct clause **chain_begin, struct clause **chain_end) {
    for (all_clause_ptrs_from_to(E_ptr, chain_begin, chain_end)) {
        if (E_ptr_ptr == E_ptr_last) {
            PUSH(*rho_prime, E_star(E_ptr, D_ptr, l));
            return;
        }
        literal_t k = clause_get_reverse_resolvent(E_ptr);
        if (k == NEG(l) || literal_in_clause(k, D_ptr)) { // k \in D \cup {-l}
            continue;
        } else if (NEG(k) != l && literal_in_clause(NEG(k), D_ptr)) { // -k \in D \\ {-l}
            PUSH(*rho_prime, E_star(E_ptr, D_ptr, l));
            return;
        } else if (!literal_in_clause(k, D_ptr) && !literal_in_clause(NEG(k), D_ptr)) { // k, -k \notin D
            PUSH(*rho_prime, E_star(E_ptr, D_ptr, l));
            continue;
        } else {
            assert(0);
        }
    }
}

/**
 * @brief Finishing todo's by copying the resulting chain.
 *        adds them to the proof and releases the rat clause.
 */
void do_remaining_todos(struct proof proof) {
    struct clause *to_insert_begin = NULL;
    struct clause *to_insert_last = NULL;
    struct clause *current = proof.begin;
    struct todo_stack todos;
    struct todo todo;

    while (current != proof.next_rat_ptr) {
        todos = current->todos;
        while (!EMPTY(todos)) {
            todo = POP(todos);
            if (todo.result->index == 21845) {
                fprintf(stderr, "21845\n");
            }
            todo.result->pivot = literal_undefined;
            if (!to_insert_begin) {
                to_insert_begin = to_insert_last = todo.result;
            } else {
                to_insert_last->next = todo.result;
                todo.result->prev = to_insert_last;
                to_insert_last = todo.result;
            }
        }
        CLEAR(current->todos);
        current = current->next;
    }

    assert(current == proof.next_rat_ptr);
    todos = current->todos;
    while (!EMPTY(todos)) {
        todo = POP(todos);
        if (todo.result->index == 21845) {
            fprintf(stderr, "21845\n");
        }
        todo.result->chain = get_neg_chain(proof.next_rat_ptr, todo.other);
        todo.result->pivot = literal_undefined;
        if (!to_insert_begin) {
            to_insert_begin = to_insert_last = todo.result;
        } else {
            to_insert_last->next = todo.result;
            todo.result->prev = to_insert_last;
            to_insert_last = todo.result;
        }
    }

    if (to_insert_begin) {

        to_insert_begin->prev = proof.next_rat_ptr->prev;
        proof.next_rat_ptr->prev->next = to_insert_begin;

        to_insert_last->next = proof.next_rat_ptr->next;
        proof.next_rat_ptr->next->prev = to_insert_last;
    } else {
        proof.next_rat_ptr->prev->next = proof.next_rat_ptr->next;
        proof.next_rat_ptr->next->prev = proof.next_rat_ptr->prev;
    }
    clause_release(proof.next_rat_ptr);
}

void transform_chain_and_do_todos(struct clause *clause_ptr, literal_t l) {
    if (clause_ptr->index == 21845) {
        fprintf(stderr, "21845\n");
    }
    while (!EMPTY(clause_ptr->todos)) {
        struct todo todo = POP(clause_ptr->todos);
        load_clause(clause_ptr);
        CLEAR(todo.result->chain);
        literal_t pivot = todo.result->pivot;
        D(&(todo.result->chain), todo.other, literal_in_clause(pivot, clause_ptr) ? NEG(pivot) : pivot, clause_ptr->chain.begin, clause_ptr->chain.end);
        clear_literal_array();
        todo.result->pivot = literal_undefined;

        clause_ptr->next->prev = todo.result;
        todo.result->next = clause_ptr->next;

        todo.result->prev = clause_ptr;
        clause_ptr->next = todo.result;
    }
    RELEASE(clause_ptr->todos);

    struct clause_ptr_stack chain = {0, 0, 0};
    switch (clause_ptr->purity) {
    case impure:
        clause_ptr->prev->next = clause_ptr->next;
        clause_ptr->next->prev = clause_ptr->prev;
        clause_release(clause_ptr);
        break;
    case semipure:
        load_clause(clause_ptr);
        for (all_clause_ptrs_in_stack(chain_clause_ptr, clause_ptr->chain)) {
            literal_t k = clause_get_reverse_resolvent(chain_clause_ptr);

            if (VAR(k) != VAR(l)) {
                PUSH(chain, chain_clause_ptr);
            } else {
                D(&chain, chain_clause_ptr, NEG(k), chain_clause_ptr_ptr + 1, chain_clause_ptr_end);
                break;
            }
        }
        clear_literal_array();
        RELEASE(clause_ptr->chain);
        clause_ptr->chain = chain;
        clause_ptr->purity = pure;
        break;
    case pure:
        break;
    default:
        assert(0);
    }
}

void mark_purity(struct proof proof) {
    literal_t rat_literal = proof.next_rat_ptr->pivot;
    load_literal(proof.next_rat_ptr->index);
    struct clause *current = proof.next_rat_ptr->next;
    while (current) {
        struct clause_ptr_stack chain = current->chain;
        for (all_clause_ptrs_in_stack(chain_clause_ptr, chain)) {
            if (literal_array[chain_clause_ptr->index]) {
                if (literal_in_clause(rat_literal, current)) {
                    current->purity = impure;
                    load_literal(current->index);
                    break;
                } else {
                    current->purity = semipure;
                    break;
                }
            }
        }
        current = current->next;
    }

    clear_literal_array();
}

void handle_signal(int signal) {
    quit = 1;
}

void cleanup(void) {
    proof_release(proof);
    free(literal_array);
    RELEASE(set_literals);
}
