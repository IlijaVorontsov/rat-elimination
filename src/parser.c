#include "parser.h"
#include "stats.h"

struct index_stack lookup_table;
struct clause_stack lookup_stack;
struct clause_stack rat_clauses;
literal_t max_variable;

// compares two literals for `qsort`
int literal_compare(const void *a, const void *b)
{
  return (*(long long int *)a - *(long long int *)b);
}

// extends number by digit
static inline unsigned long long parse_extend_number(char digit, unsigned long long number)
{
  ASSERT_ERROR(digit >= '0' && digit <= '9', "expected digit :%c", digit);
  return number * 10 + digit - '0';
}

/**
 * @brief Creates a clause.
 *
 * @param index The index of the clause.
 * @param prev The previous clause.
 * @param literals The literals of the clause.
 * @param chain The chain of the clause.
 * @param is_rat Whether the clause is a RAT clause.
 * @return `clause_t*`
 * @note This function sorts the literals of the clause.
 */
static inline clause_t *clause_create(index_t index, clause_t *prev, struct literal_stack literals, struct clause_stack chain, bool is_rat)
{
  literal_t hint = is_rat ? ACCESS(literals, 0) : 0;
  unsigned literal_count = SIZE(literals);
  unsigned chain_size = SIZE(chain);
  chain.begin = realloc(chain.begin, SIZE(chain) * sizeof(clause_t *));

  literals.begin = realloc(literals.begin, literal_count * sizeof(literal_t));
  qsort(literals.begin, literal_count, sizeof(literal_t), literal_compare);

  clause_t *clause = malloc(sizeof(clause_t));
  *clause = (clause_t){
      .purity = pure,
      .is_rat = is_rat,
      .index = index,
      .prev = prev,
      .next = NULL,
      .hint = hint,
      .hint_clause = NULL,
      .chain = (struct subsumption_merge_chain){
          .size = chain_size,
          .pivots = NULL,
          .clauses = chain.begin},
      .literal_count = literal_count,
      .literals = literals.begin};

  return clause;
}

/**
 * @brief Parses a clause from a DIMACS file.
 *
 * @param dimacs_fp The file pointer to the DIMACS file.
 * @param index The index of the clause.
 * @param prev The previous clause.
 * @return `clause_t*` the pointer to the clause or `NULL` if the end of the file is reached.
 * @pre `dimacs_fp` is at the beginning of a line
 * @post `dimacs_fp` is at the beginning of the next line
 */
static inline clause_t *parse_dimacs_clause(FILE *dimacs_fp, index_t index, clause_t *prev)
{
  literal_t number = 0;
  bool negated = false;
  struct literal_stack literals = {0, 0, 0};
  char c = fgetc(dimacs_fp);
  if (c == EOF)
    return NULL;

  for (; c != '\n'; c = fgetc(dimacs_fp))
  {
    if (c == ' ')
    {
      ASSERT_ERROR(number != 0, "unexpected space in clause %llu", index);
      max_variable = max_variable > number ? max_variable : number;
      PUSH(literals, LITERAL(negated, number - 1));
      number = 0;
      negated = false;
    }
    else if (c == '-')
      negated = true;
    else
      number = parse_extend_number(c, number);
  }
  ASSERT_ERROR(number == 0, "Unexpected endinig of clause %llu in dimacs", index);
  clause_t *clause_ptr = clause_create(index, prev, literals, (struct clause_stack){0, NULL, NULL}, false);
  PUSH(lookup_table, index);
  PUSH(lookup_stack, clause_ptr);
  return clause_ptr;
}

/**
 * @brief Looks up a clause by its index.
 *
 * @param index The index of the clause.
 * @return `struct clause*`
 * @note This function uses a binary search.
 */
static inline struct clause *lookup_clause(index_t index)
{
  index_t *begin_ptr = lookup_table.begin,
          *end_ptr = lookup_table.end;

  while (begin_ptr < end_ptr)
  {
    index_t *mid_ptr = begin_ptr + (end_ptr - begin_ptr) / 2,
            mid = *mid_ptr;
    if (mid == index)
      return ACCESS(lookup_stack, mid_ptr - lookup_table.begin);
    else if (mid < index)
      begin_ptr = mid_ptr + 1;
    else
      end_ptr = mid_ptr;
  }
  return NULL;
}

/**
 * @brief Parses a clause from an LRAT file.
 *
 * @param lrat_fp The file pointer to the LRAT file.
 * @param index The index of the clause.
 * @param prev The previous clause.
 * @return `clause_t*`
 */
static inline clause_t *parse_lrat_clause(FILE *lrat_fp, index_t index, clause_t *prev)
{
  index_t parsed_index = 0;
  index_t number = 0;
  char c;
  bool negated = false,
       is_rat = false;

  struct literal_stack literals = {0, 0, 0};
  struct clause_stack chain = {0, 0, 0};

  enum
  {
    READ_INDEX,
    READ_LITERALS,
    READ_CHAIN
  } state = READ_INDEX;

  while ((c = fgetc(lrat_fp)) != EOF)
    switch (state)
    {
    case READ_INDEX:
      if (c == ' ') // end of index
      {
        state = READ_LITERALS;
        parsed_index = number;
        number = 0;
      }
      else
      {
        number = parse_extend_number(c, number);
      }
      break;
    case READ_LITERALS:
      if (number == 0 && c == 'd') // skipping over deletions
      {
        state = READ_INDEX;
        parsed_index = 0;
        while ((c = fgetc(lrat_fp)) != '\n')
          ;
      }
      else if (number == 0 && c == '0') // end of literals
      {
        state = READ_CHAIN;
        ASSERT_ERROR(fgetc(lrat_fp) == ' ', "expected space after literals in lrat clause %llu", parsed_index);
      }
      else if (number == 0 && c == '-') // negative literal
      {
        negated = true;
      }
      else if (c == ' ') // literal separator -> push literal
      {
        number = number - 1; // converting to internal representation
        max_variable = max_variable > number ? max_variable : number;
        PUSH(literals, LITERAL(negated, number));
        negated = false;
        number = 0;
      }
      else
      {
        number = parse_extend_number(c, number);
      }
      break;
    case READ_CHAIN:
      if (number == 0 && c == '0')
      {
        ASSERT_ERROR(fgetc(lrat_fp) == '\n', "expected newline clause index %llu", index);
        is_rat = is_rat || EMPTY(chain); // empty chain for bdd solvers
        clause_t *clause_ptr = clause_create(index, prev, literals, chain, is_rat);
        if (is_rat)
          PUSH(rat_clauses, clause_ptr);
        PUSH(lookup_table, parsed_index);
        PUSH(lookup_stack, clause_ptr);
        return clause_ptr;
      }
      else if (number == 0 && c == '-')
      {
        negated = is_rat = true;
      }
      else if (c == ' ')
      {
        clause_t *clause_ptr = lookup_clause(number);
        PUSH(chain, negated ? SET_NEG_CHAIN_HINT(clause_ptr) : clause_ptr);
        negated = false;
        number = 0;
      }
      else
      {
        number = parse_extend_number(c, number);
      }
      break;
    default:
      assert(0);
    }
  ASSERT_ERROR(number == 0, "unexpected end of file at clause index [%llu]", parsed_index);
  return NULL;
}

/**
 * @brief Constructs a proof from a DIMACS and LRAT file.
 *
 * @param dimacs_path The path to the DIMACS file.
 * @param lrat_path The path to the LRAT file.
 * @return `struct proof`
 */
struct proof
parse_dimacs_lrat(const char *dimacs_path, const char *lrat_path)
{
  time_start(parse_dimacs_lrat);
  struct proof proof = {(struct clause_stack){0, 0, 0}, NULL, NULL, NULL};
  index_t index = 1;

  // shifting by 1 to directly use the index as an array index
  PUSH(lookup_stack, NULL);
  PUSH(lookup_table, 0);

  /* ----------------- Parsing DIMACS ----------------- */
  FILE *file_ptr = fopen(dimacs_path, "r");
  ASSERT_ERROR(file_ptr, "parse_dimacs: fopen of %s failed", dimacs_path);

  // skip over lines with comments
  for (char character = fgetc(file_ptr); character == 'c' || character == 'p'; character = fgetc(file_ptr))
  {
    while (fgetc(file_ptr) != '\n')
      ;
    if (character == 'p')
      break;
  }

  // parsing dimacs clauses
  for (clause_t *clause_ptr = proof.begin = parse_dimacs_clause(file_ptr, index++, NULL);
       clause_ptr != NULL;
       clause_ptr = parse_dimacs_clause(file_ptr, index++, clause_ptr))
    increment_stat(dimacs_creations);
  ;
  index--; // last clause is NULL
  fclose(file_ptr);

  /* ----------------- Parsing LRAT ----------------- */
  file_ptr = fopen(lrat_path, "r");
  ASSERT_ERROR(file_ptr, "proof_from_dimacs_lrat: fopen of %s failed", lrat_path);

  // parsing lrat clauses
  for (clause_t *clause_ptr = parse_lrat_clause(file_ptr, index++, TOP(lookup_stack));
       clause_ptr != NULL;
       clause_ptr = parse_lrat_clause(file_ptr, index++, clause_ptr))
    increment_stat(lrat_creations);
  fclose(file_ptr);

  proof.end = TOP(lookup_stack);
  proof.rat_clauses = rat_clauses;

  // parsing cleanup
  RELEASE(lookup_table);
  RELEASE(lookup_stack);

  // linking in backward direction and reconstructing pivots
  struct clause *next = NULL, *curr = proof.end;
  bit_vector_init(max_variable);

  while (curr)
  {
    curr->next = next;
    clause_reconstruct_pivots(curr);
    next = curr;
    curr = curr->prev;
  }
  time_end(parse_dimacs_lrat);
  return proof;
}
