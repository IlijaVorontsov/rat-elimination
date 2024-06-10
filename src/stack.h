/*
 * Stack Implementation
 * Copyright (c) 2021 Armin Biere, Johannes Kepler University Linz, Austria
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * This implementation is based on the work of Armin Biere and Johannes Kepler University Linz.
 * Original Source: https://github.com/arminbiere/satch/tree/rel-0.2.5
 */

#ifndef _stack_h_INCLUDED
#define _stack_h_INCLUDED

// Generic stack implementation similar to 'std::vector' API in C++.
// In order to use it you need to provide 'fatal_error' below which could
// also be a local macro in the user compilation unit since this
// implementation here is header only and also only uses macros (beside type
// issue this part would also not be that easy with inline functions).

#include "msg.h"
#include <assert.h>
#include <stdlib.h> // For 'size_t', 'realloc', 'free'.

/*------------------------------------------------------------------------*/

// Predicates.

#define EMPTY(S) ((S).end == (S).begin)
#define FULL(S) ((S).end == (S).allocated)

/*------------------------------------------------------------------------*/

#define SIZE(S) ((size_t)((S).end - (S).begin))
#define CAPACITY(S) ((size_t)((S).allocated - (S).begin))

/*------------------------------------------------------------------------*/

#define INIT(S)                                  \
    do                                           \
    {                                            \
        (S).end = (S).begin = (S).allocated = 0; \
    } while (0)

#define RELEASE(S)                               \
    do                                           \
    {                                            \
        free((S).begin);                         \
        (S).begin = (S).end = (S).allocated = 0; \
    } while (0)

/*------------------------------------------------------------------------*/

// Duplicate size of stack.

#define ENLARGE(S)                                                          \
    do                                                                      \
    {                                                                       \
        const size_t old_size = SIZE(S);                                    \
        const size_t old_capacity = CAPACITY(S);                            \
        const size_t new_capacity = old_capacity ? 2 * old_capacity : 1;    \
        const size_t new_bytes = new_capacity * sizeof *(S).begin;          \
        (S).begin = realloc((S).begin, new_bytes);                          \
        ASSERT_ERROR((S).begin, "out-of-memory reallocating stack %s", #S); \
        (S).end = (S).begin + old_size;                                     \
        (S).allocated = (S).begin + new_capacity;                           \
    } while (0)

#define PUSH(S, E)        \
    do                    \
    {                     \
        if (FULL(S))      \
            ENLARGE(S);   \
        *(S).end++ = (E); \
    } while (0)

/*------------------------------------------------------------------------*/

// Flush all elements.

#define CLEAR(S)             \
    do                       \
    {                        \
        (S).end = (S).begin; \
    } while (0)

/*------------------------------------------------------------------------*/

// Access element at a certain position (also works as 'lvalue').

#define ACCESS(S, I) \
    ((S).begin[assert((size_t)(I) < SIZE(S)), (I)])

/*------------------------------------------------------------------------*/

// Access least recently added 'last' element of stack.

#define TOP(S) \
    (assert(!EMPTY(S)), (S).end[-1])

#define POP(S) \
    (assert(!EMPTY(S)), *--(S).end)

/*------------------------------------------------------------------------*/

// Common types of stacks.

typedef struct unsigned_stack // Generic stack with 'unsigned' elements.
{
    unsigned *begin, *end, *allocated;
} unsigned_stack_t;

typedef struct int_stack // Generic stack with 'int' elements.
{
    int *begin, *end, *allocated;
} int_stack_t;

/*------------------------------------------------------------------------*/

// Explicitly typed iterator over non-pointer stack elements, e.g.,
//
//   struct int_stack stack;
//   INIT (stack);
//   for (int i = 0; i < 10; i++)
//     PUSH (stack, i);
//   for (all_elements_on_stack (int, i, stack))
//     printf ("%d\n", i);
//   RELEASE (stack);
//
// pushes the integers 0,...,9 onto a stack and then prints its elements.

#define all_elements_on_stack(TYPE, E, S)                   \
    TYPE E, *E##_ptr = (S).begin, *const E##_end = (S).end; \
    (E##_ptr != E##_end) && (E = *E##_ptr, 1);              \
    ++E##_ptr

// For pointer elements we need additional '*'s in the declaration and
// the 'TYPE' argument is the base type of the pointer.  To iterate a stack
// of 'struct clause *' use 'for (all_pointers_on_stack (clause, c, S))'.

#define all_pointers_on_stack(TYPE, E, S)                      \
    TYPE *E, **E##_ptr = (S).begin, **const E##_end = (S).end; \
    (E##_ptr != E##_end) && (E = *E##_ptr, 1);                 \
    ++E##_ptr

/*------------------------------------------------------------------------*/

/*------------------------------------------------------------------------*/
// RAT Elimination Specific Implementation
struct clause_stack
{
    clause_t **begin, **end, **allocated;
};

struct literal_stack
{
    literal_t *begin, *end, *allocated;
};

struct index_stack
{
    index_t *begin, *end, *allocated;
};

#endif
