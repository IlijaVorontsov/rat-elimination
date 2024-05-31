#include "proof.h"

#include <stdio.h>
#include <stdlib.h>
#include "stack.h"
#include "msg.h"

struct proof proof_from_dimacs_lrat(const char *dimacs_path,
                                    const char *lrat_path)
{
    struct proof proof = {0, 0, 0, NULL, NULL, NULL, NULL, NULL};
    FILE *file = fopen(dimacs_path, "r");
    ASSERT_ERROR(file, "parse_dimacs: fopen failed");

    // skip over lines with comments
    char character;
    while ((character = fgetc(file)) == 'c' || character == 'p')
    {
        while (fgetc(file) != '\n')
            ;
        if (character == 'p')
            break;
    }

    ASSERT_ERROR(character == 'p', "missig header after comments in dimacs");

    struct clause_stack lookup_stack = {0, 0, 0};
    PUSH(lookup_stack, NULL); // shifting by one (index 0 is not used in dimacs format)
    struct
    {
        index_t *begin, *end, *allocated;
    } lookup_table = {0, 0, 0};
    PUSH(lookup_table, 0); // shifting by one

    struct literal_stack literals = {0, 0, 0};
    literal_t variable = 0;
    literal_t max_variable = 0;
    index_t index = 1;
    bool negated = false;
    struct clause *previous = NULL, *current = NULL;

    while ((character = fgetc(file)) != EOF)
    {
        if (character == ' ' && variable != 0)
        {
            variable = variable - 1; // converting to internal representation
            max_variable = max_variable > variable ? max_variable : variable;
            PUSH(literals, LITERAL(negated, variable));
            negated = false;
            variable = 0;
        }
        else if (variable == 0 && character == '0' && negated == false)
        {
            ASSERT_ERROR(fgetc(file) == '\n', "expected newline after 0 in dimacs");
            current = clause_create(SIZE(literals), literals.begin, 0, NULL, false);
            current->index = index;
            current->prev = previous;
            if (previous)
            {
                previous->next = current;
            }
            else
            {
                proof.begin = current;
            }
            previous = current;
            PUSH(lookup_stack, current);
            PUSH(lookup_table, index);
            index++;
            CLEAR(literals);
        }
        else if (character == '-')
        {
            negated = true;
        }
        else if (character != '\n')
        {
            ASSERT_ERROR(character >= '0' && character <= '9', "expected number in dimacs");
            variable = variable * 10 + character - '0';
        }
    }
    ASSERT_ERROR(variable == 0 && negated == false && EMPTY(literals), "expected 0 at the end of file in dimacs");
    proof.last_dimacs = current;
    fclose(file);
    FILE *lrat_file = fopen(lrat_path, "r");
    ASSERT_ERROR(lrat_file, "proof_from_dimacs_lrat: fopen failed");

    bool is_rat = false;
    uint64_t number = 0;

    enum
    {
        READ_INDEX,
        READ_LITERALS,
        READ_CHAIN
    } state = READ_INDEX;

    index_t clause_index;

    struct clause_stack chain = {NULL, NULL, NULL};
    struct clause_stack rat_clauses = {NULL, NULL, NULL};

    while ((character = fgetc(lrat_file)) != EOF)
    {
        switch (state)
        {
        case READ_INDEX:
            if (character == ' ')
            {
                state = READ_LITERALS;
                clause_index = number;
                number = 0;
            }
            else
            {
                ASSERT_ERROR(character >= '0' && character <= '9', "expected number in [%s:%llu]", lrat_path, index);
                number = (number * 10) + (character - '0');
            }
            break;
        case READ_LITERALS:
            if (number == 0 && character == 'd')
            {
                state = READ_INDEX;
                clause_index = 0;
                while ((character = fgetc(lrat_file)) != '\n')
                    ;
            }
            else if (number == 0 && character == '0')
            {
                state = READ_CHAIN;
                assert(fgetc(lrat_file) == ' '); // skipping space
            }
            else if (number == 0 && character == '-')
            {
                negated = true;
            }
            else if (character == ' ')
            {
                number = number - 1; // converting to internal representation
                max_variable = max_variable > number ? max_variable : number;
                PUSH(literals, LITERAL(negated, number));
                negated = false;
                number = 0;
            }
            else
            {
                ASSERT_ERROR(character >= '0' && character <= '9', "expected number in [%s:%llu]", lrat_path, index);
                number = (number * 10) + (character - '0');
            }
            break;
        case READ_CHAIN:
            if (number == 0 && character == '0')
            {
                is_rat = is_rat || EMPTY(chain); // empty chain for bdd solvers
                current = clause_create(SIZE(literals), literals.begin, SIZE(chain), chain.begin, is_rat);
                current->index = index++;
                current->prev = previous;
                previous = current;
                if (is_rat)
                    PUSH(rat_clauses, current);
                PUSH(lookup_table, clause_index);
                PUSH(lookup_stack, current);
                ASSERT_ERROR(fgetc(lrat_file) == '\n', "expected newline in [%s:%llu]", lrat_path, index);
                CLEAR(literals);
                CLEAR(chain);
                is_rat = false;
                state = READ_INDEX;
            }
            else if (number == 0 && character == '-')
            {
                negated = is_rat = true;
            }
            else if (character == ' ')
            {
                struct clause *clause_ptr;
                // reverse lookup in lookup table using binary search
                index_t *begin = lookup_table.begin;
                index_t *end = lookup_table.end;
                while (begin < end)
                {
                    index_t *mid = begin + (end - begin) / 2;
                    if (*mid == number)
                    {
                        clause_ptr = ACCESS(lookup_stack, mid - lookup_table.begin);
                        break;
                    }
                    else if (*mid < number)
                    {
                        begin = mid + 1;
                    }
                    else
                    {
                        end = mid;
                    }
                }

                PUSH(chain, negated ? SET_NEG_CHAIN_HINT(clause_ptr) : clause_ptr);
                negated = false;
                number = 0;
            }
            else
            {
                ASSERT_ERROR(character >= '0' && character <= '9', "expected number in  [%s:%llu]", lrat_path, index);
                number = (number * 10) + (character - '0');
            }
            break;
        default:
            assert(0);
            break;
        }
    }
    fclose(lrat_file);
    RELEASE(literals);
    RELEASE(lookup_table);
    proof.end = previous;
    proof.current_rat_clause = NULL;
    proof.rat_clauses = rat_clauses.begin;
    proof.rat_count = SIZE(rat_clauses);
    // proof.max_variable = max_variable;
    struct clause *next = proof.end;
    current = next->prev;
    while (current)
    {
        current->next = next;
        next = current;
        current = current->prev;
    }
    return proof;
}
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

void proof_fprint_all(FILE *stream, struct proof proof)
{
    struct clause *current = proof.begin;
    while (current)
    {
        clause_fprint_with_pivots(stream, *current);
        current = current->next;
    }
}
