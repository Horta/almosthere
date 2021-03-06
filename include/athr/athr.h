#ifndef ATHR_ATHR_H
#define ATHR_ATHR_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "athr/export.h"

    struct athr;

    enum ATHR_OPTS
    {
        ATHR_BAR = 1,
        ATHR_ETA = 2,
        ATHR_PERC = 4
    };

    typedef struct
    {
        long           volume;
        char const*    desc;
        enum ATHR_OPTS opts;
    } athr_create_args;

    ATHR_API struct athr* athr_create_var(athr_create_args in);

#define athr_create(...) athr_create_var((athr_create_args){__VA_ARGS__});

    ATHR_API void athr_consume(struct athr* at, long consume);
    ATHR_API void athr_finish(struct athr* at);
    ATHR_API void athr_sleep(long milliseconds);

#ifdef __cplusplus
}
#endif

#endif
