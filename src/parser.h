#ifndef __parser_h__
#define __parser_h__

#include "clause.h"
#include "stack.h"
#include "proof.h"

struct proof parse_dimacs_lrat(const char *dimacs_path, const char *lrat_path);

#endif /* __parser_h__ */