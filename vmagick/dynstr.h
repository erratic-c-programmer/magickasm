#ifndef DYNSTR_H
#define DYNSTR_H

// Implements refcounted dynamic strings. Their format is
// as follows:
//   - 8 bytes for unsigned length
//   - however long the string is, padded with nonzero amount
//     of nulls such that its size is a multiple of 8 bytes
//   - 8 bytes for signed refcount

#include <stdint.h>
#include <stddef.h>

uint64_t **dynstr_make(size_t len);
void dynstr_incref(uint64_t **s_ptr);
void dynstr_decref(uint64_t **s_ptr);

#endif
