#ifndef LITERAL_H
#define LITERAL_H

/**
 * @brief Type of literals.
 * Correspondance
 * |   dimacs    | literal_t  |
 * |-------------|------------|
 * | 1           | 0          |
 * | -1          | 1          |
 * | ...         | ...        |
 * | 2147483647  | 4294967292 |
 * | -2147483647 | 4294967293 |
 *
 * 4294967295 is reserved for undefined literals
 */
#define literal_t unsigned
#define literal_undefined 4294967295

#define NEG(literal) ((literal) ^ 1)
#define IS_SIGNED(literal) ((literal) & 1)
#define VAR(literal) ((literal) >> 1)
#define LITERAL(sign, var) (((var) << 1) | (sign))
#define SIGN(sign, literal) ((literal) | (sign))
#define TO_DIMACS(literal) (((literal) & 1) ? -((int)((literal) >> 1) + 1) : (int)(((literal) >> 1) + 1))

#endif // LITERAL_H
