#include "dynstr.h"
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

uint64_t **
dynstr_make(size_t len)
{
	uint64_t **s = malloc(sizeof(*s));
	*s = calloc(1 + len / 8 + 1 + 1, sizeof(**s));
	*s[0] = len;
	return s;
}

void
dynstr_incref(uint64_t **s_ptr)
{
	uint64_t *s = *s_ptr;
	if ((int64_t)s[1 + s[0] / 8 + 1] == -1) {
		return;
	}
	s[1 + s[0] / 8 + 1]++;
}

void
dynstr_decref(uint64_t **s_ptr)
{
	// NOTE: will delete the string if refcount drops to zero.
	uint64_t *s = *s_ptr;
	if ((int64_t)s[1 + s[0] / 8 + 1] == -1) {
		return;
	}
	if (--s[1 + s[0] / 8 + 1] == 0) {
		free(s);
		free(s_ptr);
	}
}
