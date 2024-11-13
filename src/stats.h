#ifndef __stats_h__
#define __stats_h__

#ifdef BENCH
#include <time.h>
#include <stdio.h>
#include "clause.h"

struct stats
{
  index_t new_todos;
  index_t reused_todos;
  index_t deletions;
  index_t not_impure_with_rat_pivot;
  index_t dimacs_creations;
  index_t lrat_creations;
  double total_time;
  double parse_dimacs_lrat_time;
  double mark_purity_time;
  double elimination_time;
  double finish_todos_time;
  double misc_time;
  double chain_distribution_time;
};

extern struct stats stats;

#define init_stats() struct stats stats = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}

#define increment_stat(stat) ++stats.stat

#define timeit(fun, ...)                                                       \
  do                                                                           \
  {                                                                            \
    clock_t fun##_start_time = clock();                                        \
    fun(__VA_ARGS__);                                                          \
    stats.fun##_time += (double)(clock() - fun##_start_time) / CLOCKS_PER_SEC; \
  } while (0)

#define time_start(stat) clock_t stat##_start_time = clock()
#define time_end(stat) stats.stat##_time += (double)(clock() - stat##_start_time) / CLOCKS_PER_SEC

// prints the stats to be outputted as csv
// dimacs, lrat, additions, reused, deletions, total, marking, distributing, finishing
#define print_stats() fprintf(stdout, "%llu, %llu, %llu, %llu, %llu, %f, %f, %f, %f\n", stats.dimacs_creations, stats.lrat_creations, stats.new_todos, stats.reused_todos, stats.deletions, stats.total_time, stats.mark_purity_time, stats.chain_distribution_time, stats.finish_todos_time)

#define print_header() fprintf(stdout, "dimacs, lrat, additions, reused, deletions, total, marking, distributing, finishing\n")

#else

#define init_stats()
#define increment_stat(stat)
#define time_start(stat)
#define time_end(stat)
#define timeit(fun, ...) fun(__VA_ARGS__)

#define print_stats() \
  do                  \
  {                   \
  } while (0)

#define print_header() \
  do                   \
  {                    \
  } while (0)
#endif

#endif /* __stats_h__ */