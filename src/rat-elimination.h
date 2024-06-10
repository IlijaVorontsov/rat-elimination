#ifndef __rat_elimination_h__
#define __rat_elimination_h__

#include "clause.h"
#include "literal.h"
#include <stdbool.h>
#include <stdio.h>
#include "proof.h"
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include "msg.h"
#include "stack.h"
#include <time.h>
#include <unistd.h>
#include "stats.h"
#include <unistd.h>
#include <getopt.h>
#include "parser.h"

// Struct to hold the parsed arguments
typedef struct
{
  bool verbose;
  bool print_pivots;
  char *dimacs_filename;
  char *lrat_filename;
  char *lrat_out;
} Arguments;

void usage(const char *prog_name);
Arguments parse_arguments(int argc, char *argv[]);

void sigint_handler(int signum);

void mark_purity(clause_t *current_ptr);
void finish_todos(clause_t *rat_clause_ptr);
void chain_distribution(clause_t *distributing_clause_ptr, literal_t distributing_literal, clause_t *chain_clause_ptr, unsigned index);

#define fprintf_verbose(...) \
  if (args.verbose)          \
  fprintf(__VA_ARGS__)

#endif /* __rat_elimination_h__ */
