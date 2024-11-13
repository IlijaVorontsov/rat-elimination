#ifndef __msg_h__
#define __msg_h__

#include <stdarg.h>
#include <stdio.h>

#define DEBUG_MSG(FMT, ...) fprintf(stderr, "[%s:%d] " FMT "\n", __FILE__, __LINE__, ##__VA_ARGS__)

#define ERROR(FMT, ...)                \
    do                                 \
    {                                  \
        DEBUG_MSG(FMT, ##__VA_ARGS__); \
        exit(1);                       \
    } while (0)

#define ASSERT_ERROR(condition, message, ...)             \
    do                                                    \
    {                                                     \
        if (!(condition))                                 \
        {                                                 \
            fprintf(stderr, message "\n", ##__VA_ARGS__); \
            exit(EXIT_FAILURE);                           \
        }                                                 \
    } while (0)

#endif /* __msg_h__ */
