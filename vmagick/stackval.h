#ifndef STACKVAL_H
#define STACKVAL_H

#include <stdint.h>

typedef struct {
    uint64_t val;
    char type;
} StackVal;

#endif
