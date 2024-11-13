#include "rat-elimination.h"

bool *bit_vector;
struct clause_stack todo_clauses;
index_t current_rat_index;
volatile sig_atomic_t quit;
init_stats();

int main(int argc, char *argv[])
{
    time_start(total);
    // Catches SIGINT <CTRL-C> to save progress
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigaction(SIGINT, &sa, NULL);

    // Parsing command-line arguments and opening output file
    Arguments args = parse_arguments(argc, argv);
    FILE *output = args.lrat_out ? fopen(args.lrat_out, "w") : stdout;
    ASSERT_ERROR(output, "main: fopen failed for %s", args.lrat_out);

    // Parsing the proof
    struct proof proof = parse_dimacs_lrat(args.dimacs_filename, args.lrat_filename);
    if (args.verbose)
        print_header();

    while (!EMPTY(proof.rat_clauses) && !quit)
    {
        clause_t *current_rat_clause_ptr = POP(proof.rat_clauses);
        current_rat_index = current_rat_clause_ptr->index;

        timeit(mark_purity, current_rat_clause_ptr);
        time_start(elimination);
        // traverses proof from end to the clause after the current rat clause (before insertions of todo's)
        // since todo's after the current rat clause are not yet finished (finish todo's is called after this loop)
        for (clause_t *clause_ptr = proof.end, clause = *clause_ptr, *last_ptr = current_rat_clause_ptr->next;
             ; clause_ptr = clause_ptr->prev, clause = *clause_ptr)
        {
            bool last = clause_ptr == last_ptr;
            switch (clause.purity)
            {
            case impure:
            {
                clause_ptr = proof_unlink_free(clause_ptr);
                break;
            }
            case semipure:
            {
                timeit(chain_distribution, clause.chain.clauses[clause.hint], NEG(clause.chain.pivots[clause.hint]), clause_ptr, clause.hint);
                clause_ptr->purity = pure;
                break;
            }
            case pure:
                break;
            case todo:
            {
                timeit(chain_distribution, clause.hint_clause, clause.hint, clause_ptr, 0);
                clause_ptr->purity = pure;
                break;
            }
            default:
                ASSERT_ERROR(0, "transform_chain: unknown purity %d", clause.purity);
            }
            if (last)
                break;
        }
        time_end(elimination);
        timeit(finish_todos, current_rat_clause_ptr);
        proof_unlink_free(current_rat_clause_ptr);
        if (args.verbose)
            print_stats();
    }
    proof_fprint_final(output, proof, args.print_pivots && EMPTY(proof.rat_clauses));
    time_end(total);
    exit(0);
}

/**
 * @brief Reconstructs the chain and pivots for todo's after the current rat clause.
 *        Links all todo's from clauses above the current rat clause bellow the rat clause.
 *
 *
 * @param rat_clause_ptr Pointer to the current rat clause
 */
void finish_todos(clause_t *rat_clause_ptr)
{
    clause_t rat_clause = *rat_clause_ptr;
    clause_t *current_ptr = rat_clause.next;

    // all todo's after the current rat clause
    for (clause_t current = *current_ptr; current.purity == todo; current_ptr = current_ptr->next, current = *current_ptr)
    {
        // get the chain from the rat chain (using the negative hints -index and prechain)
        current_ptr->chain = get_chain(rat_clause.chain, current.hint_clause);
        current_ptr->hint_clause = NULL;
        clause_reconstruct_pivots(current_ptr);
        current_ptr->purity = pure;
    }

    clause_t *next_ptr = rat_clause_ptr->next; // required since linking chains not elements

    for (all_pointers_on_stack(clause_t, clause_ptr, todo_clauses))
    {
        // start of the todo chain
        clause_t *todo_start_ptr = clause_ptr->next;
        current_ptr = todo_start_ptr;
        while (current_ptr->purity == todo)
        {
            current_ptr->purity = pure;
            current_ptr = current_ptr->next;
        } // current_ptr->prev is chain end

        // linking end
        next_ptr->prev = current_ptr->prev;
        current_ptr->prev->next = next_ptr;
        // linking start
        rat_clause_ptr->next = todo_start_ptr;
        todo_start_ptr->prev = rat_clause_ptr;
        // for next insertion (since next_ptr is the next after current rat)
        next_ptr = todo_start_ptr;

        // linking where the todo chain was cut
        clause_ptr->next = current_ptr;
        current_ptr->prev = clause_ptr;
    }
    CLEAR(todo_clauses);
}

/**
 * @brief Marks the purity based on existence of rat pivot and purity of chain clauses.
 *        Additionally, sets the indices, since new clauses where created.
 *
 * @param current_ptr Pointer to the current rat clause (reused as the running pointer)
 */
void mark_purity(clause_t *current_ptr)
{
    const literal_t rat_pivot = current_ptr->hint;
    const literal_t neg_rat_pivot = NEG(rat_pivot);
    current_ptr->purity = impure;
    index_t current_index = current_ptr->index;

    while (current_ptr->next != NULL) // end not reached
    {
        current_ptr = current_ptr->next;
        current_ptr->purity = pure;
        current_ptr->index = ++current_index;

        signed char sign = var_in_clause(rat_pivot, current_ptr);

        if (sign < 0) // NEG(rat_pivot) in clause
        {
            continue;
        }
        else if (sign > 0) // rat_pivot in clause
        {
            for (unsigned i = 0; i < current_ptr->chain.size; i++)
            {
                if (current_ptr->chain.clauses[i]->purity == impure)
                {
                    current_ptr->purity = impure;
                    break;
                }
            }
            increment_stat(not_impure_with_rat_pivot);
        }
        else
        {
            // rat_pivot not in clause -> check pivots to see if some clause might have rat_literal in it,
            // but the rat_literal is resolved away
            for (unsigned i = 0, end = current_ptr->chain.size - 1; i < end; i++)
            {
                literal_t chain_pivot = current_ptr->chain.pivots[i];
                if (chain_pivot == neg_rat_pivot)
                // k_i = \lnot rat_pivot -> only C_i has to be checked,
                // since rat_pivot \in C_i and there exist no later C_j j>i s.t. rat_pivot \in C_j
                // proof there exist no later C_j j>i s.t. rat_pivot \in C_j:
                // contradiction: assume there exist a later j>i s.t. rat_pivot \in C_j
                // \lnot rat_pivot is introduced in the subsumption step
                // since rat_pivot in C_j the only possible pivot would be rat_pivot,
                // but then step i would not be a merge step. (Contradiction)
                {
                    if (current_ptr->chain.clauses[i]->purity == impure)
                        current_ptr->purity = semipure;
                    current_ptr->hint = i;
                    break;
                }
                else if (chain_pivot == rat_pivot)
                // k_i = rat_pivot -> \lnot rat_pivot \in C_i (thus it doesn't have to be checked)
                // rat_pivot must be introduced in the subsumption step, and is required for some
                // C_j j>(i+1) to be a merge step. -> check all j>i+1 for purity
                {
                    for (int j = i + 1; j < current_ptr->chain.size; j++)
                    {
                        if (current_ptr->chain.clauses[j]->purity == impure)
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

clause_t *E_star(clause_t *chain_clause_ptr, clause_t *distributing_clause_ptr, literal_t distributing_literal)
{
    if (literal_in_clause(NEG(distributing_literal), chain_clause_ptr))
    {
        // order of the clauses determined using the minimal number of dereferences
        clause_t *higher_index_clause_ptr, *lower_index_clause_ptr;

        if (distributing_clause_ptr->index > chain_clause_ptr->index)
        {
            lower_index_clause_ptr = chain_clause_ptr;
            higher_index_clause_ptr = distributing_clause_ptr;
        }
        else
        {
            lower_index_clause_ptr = distributing_clause_ptr;
            higher_index_clause_ptr = chain_clause_ptr;
        }

        clause_t *current_ptr = higher_index_clause_ptr->next;

        // in case that the higher (and the lower) index clause are above the current rat clause
        // keep a reference in order to link the todo's at the point of the current rat clause
        if (current_ptr && current_ptr->purity != todo &&
            higher_index_clause_ptr->index < current_rat_index)
        {
            PUSH(todo_clauses, higher_index_clause_ptr);
        }

        // looking for the same todo
        while (current_ptr && current_ptr->purity == todo)
        {
            if (current_ptr->index == lower_index_clause_ptr->index)
            {
                increment_stat(reused_todos);
                return current_ptr;
            }
            current_ptr = current_ptr->next;
        }

        increment_stat(new_todos);
        clause_t *result;

        // Three cases for todo's to be created
        if (higher_index_clause_ptr->index > current_rat_index)
        {
            clause_t **chain = malloc(sizeof(clause_t *) * higher_index_clause_ptr->chain.size);
            memcpy(chain, higher_index_clause_ptr->chain.clauses,
                   sizeof(clause_t *) * higher_index_clause_ptr->chain.size);
            literal_t *pivots = malloc(sizeof(literal_t) * (higher_index_clause_ptr->chain.size - 1));
            memcpy(pivots, higher_index_clause_ptr->chain.pivots,
                   sizeof(literal_t) * (higher_index_clause_ptr->chain.size - 1));
            result = resolve(distributing_clause_ptr, chain_clause_ptr, distributing_literal,
                             chain, pivots, higher_index_clause_ptr->chain.size);
            result->hint = lower_index_clause_ptr == distributing_clause_ptr ? distributing_literal : NEG(distributing_literal);
            result->hint_clause = lower_index_clause_ptr;
        }
        else if (higher_index_clause_ptr->index == current_rat_index)
        {
            result = resolve(distributing_clause_ptr, chain_clause_ptr, distributing_literal,
                             NULL, NULL, 0);
            result->hint_clause = lower_index_clause_ptr;
        }
        else
        {
            clause_t **chain = malloc(sizeof(clause_t *) * 2);
            chain[0] = chain_clause_ptr;
            chain[1] = distributing_clause_ptr;
            literal_t *pivots = malloc(sizeof(literal_t));
            pivots[0] = distributing_literal;
            result = resolve(chain_clause_ptr, distributing_clause_ptr, distributing_literal,
                             chain, pivots, 2);
        }

        // linking the new todo clause
        if (higher_index_clause_ptr->next)
        {
            higher_index_clause_ptr->next->prev = result;
            result->next = higher_index_clause_ptr->next;
        }
        result->prev = higher_index_clause_ptr;
        higher_index_clause_ptr->next = result;
        return result;
    }

    return chain_clause_ptr;
}

void chain_distribution(clause_t *distributing_clause_ptr, literal_t distributing_literal,
                        clause_t *chain_clause_ptr, unsigned index)
{
    bit_vector_set_clause_literals(distributing_clause_ptr);

    struct subsumption_merge_chain chain = chain_clause_ptr->chain;
    unsigned write_index = index;

    // special case if index points to distributing clause
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
    bit_vector_clear_clause_literals(distributing_clause_ptr);
}

// atexit

void sigint_handler(int signum)
{
    quit = 1;
}

// Function to display usage information
void usage(const char *prog_name)
{
    fprintf(stderr, "Usage: %s [-v] [-p] dimacs_filename lrat_filename [lrat-out]\n", prog_name);
    exit(EXIT_FAILURE);
}

// Function to parse command-line arguments
Arguments parse_arguments(int argc, char *argv[])
{
    Arguments args = {false, false, NULL, NULL, NULL}; // Initialize the struct
    int opt;

    // Parse options
    while ((opt = getopt(argc, argv, "vp")) != -1)
    {
        switch (opt)
        {
        case 'v':
            args.verbose = true;
            break;
        case 'p':
            args.print_pivots = true;
            break;
        default:
            usage(argv[0]);
        }
    }

    // Check for the required positional arguments
    if (optind + 1 >= argc)
    {
        usage(argv[0]);
    }

    // Retrieve the positional arguments
    args.dimacs_filename = argv[optind];
    args.lrat_filename = argv[optind + 1];

    if (optind + 2 < argc)
    {
        args.lrat_out = argv[optind + 2];
    }

    return args;
}
