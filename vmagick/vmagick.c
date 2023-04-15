#include "instruction_opcodes.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline uint64_t **
dynstr_make(size_t len)
{
	uint64_t **s = malloc(sizeof(*s));
	*s = calloc(1 + len / 8 + 1 + 1, sizeof(**s));
	*s[0] = len;
	printf("Creating new string at %p\n", s);
	return s;
}

static inline void
dynstr_incref(uint64_t **s_ptr)
{
	uint64_t *s = *s_ptr;
	if ((int64_t)s[1 + s[0] / 8 + 1] == -1) {
		return;
	}
	s[1 + s[0] / 8 + 1]++;
}

static inline void
dynstr_decref(uint64_t **s_ptr)
{
	// NOTE: will delete the string if refcount drops to zero.
	uint64_t *s = *s_ptr;
	if ((int64_t)s[1 + s[0] / 8 + 1] == -1) {
		return;
	}
	if (--s[1 + s[0] / 8 + 1] == 0) {
		printf("Freeing string at %p\n", s_ptr);
		free(s);
		free(s_ptr);
	}
}

int
main(int argc, char **argv)
{
	/* SETUP */

	// Read raw code into memory.
	FILE *codefile = fopen(argv[1], "rb");
	fseek(codefile, 0, SEEK_END);
	size_t codefile_sz = ftell(codefile);
	fseek(codefile, 0, SEEK_SET);
	uint64_t *code_raw = calloc(codefile_sz + 1, 1);
	fread(code_raw, 1, codefile_sz, codefile);
	fclose(codefile);

	uint64_t *load_bptr = code_raw;

	// Load all structures into arrays.

	// String literal table.
	size_t strlit_tbl_sz = load_bptr[0];
	load_bptr++;
	uint64_t **strlit_tbl = malloc(strlit_tbl_sz * sizeof(*strlit_tbl));
	for (size_t i = 0; i < strlit_tbl_sz; i++) {
		strlit_tbl[i] = load_bptr;
		load_bptr += 1 + *strlit_tbl[i] / 8 + 1 + 1;
	}

	// Instruction table; compute offsets as well.
	size_t     instr_tbl_sz = *(load_bptr++);
	uint64_t **instr_tbl = malloc(instr_tbl_sz * sizeof(*instr_tbl));
	// Stores instruction offsets in quadwords from the start of instr_tbl -
	// last entry is dummy.
	uint64_t *offset_tbl = malloc((instr_tbl_sz + 1) * sizeof(*offset_tbl));
	offset_tbl[0] = 0;
	for (size_t i = 0; i < instr_tbl_sz; i++) {
		instr_tbl[i] = load_bptr;
		size_t nargs = ((*instr_tbl[i]) >> 56) & 0xffUL;
		offset_tbl[i + 1] = offset_tbl[i] + 1 + nargs;
		load_bptr += 1 + nargs;
	}

	// Set stacks up: one value stack and one tag stack.
	const size_t stack_sz = (1 << 16) - 1;
	uint64_t    *stack = calloc(stack_sz, sizeof(*stack));
	char        *stack_types = calloc(stack_sz, sizeof(*stack_types));
	// THE ALL-HOLY ACCUMULATOR... is actually just stack[0].
#define accum      stack[0]
#define accum_type stack_types[0]

	/* FETCH-DECODE-EXECUTE */
	uint64_t instr_idx = 1; // one-indexed
	while (instr_idx - 1 < instr_tbl_sz) {
		// Fetch the instruction.
		const uint64_t instr_raw = *instr_tbl[instr_idx - 1];
		// Decode it.
		const uint64_t instr = (instr_raw << 16) >> 16;
		char           type_flags = (instr_raw >> 48) & 0b1111UL;
		char           reg_flags = (instr_raw >> 52) & 0b1111UL;
		size_t         nargs = (instr_raw >> 56) & 0xffUL;

		// Load the argument into temp arg registers - raw for now.
		uint64_t args[4];
		uint64_t arg_types[4];
		for (int i = 0; i < nargs; i++) {
			uint64_t arg_raw = instr_tbl[instr_idx - 1][i + 1];
			if (reg_flags & (1 << i)) {
				// Register argument, have to fetch.
				args[i] = stack[arg_raw];
				arg_types[i] = stack_types[arg_raw];
			} else if (type_flags & (1 << i)) {
				// String/empty literal argument.
				args[i] = (uint64_t)(strlit_tbl + arg_raw);
				arg_types[i] = 1;
			} else {
				// Integer literal argument.
				args[i] = (int64_t)arg_raw;
				arg_types[i] = 0;
			}
		}

		// Some macros for argument implicit conversion.
#define acc_int_cast()                         \
	do {                                       \
		if (accum_type == 1) {                 \
			dynstr_decref((uint64_t **)accum); \
			accum = **(uint64_t **)accum;      \
			accum_type = 0;                    \
		}                                      \
	} while (0)
#define arg_int_cast(i)                          \
	do {                                         \
		if (arg_types[i] == 1) {                 \
			dynstr_decref((uint64_t **)args[i]); \
			args[i] = **(uint64_t **)args[i];    \
			arg_types[i] = 0;                    \
		}                                        \
	} while (0)
#define acc_str_cast()                                                     \
	do {                                                                   \
		if (accum_type == 0) {                                             \
			accum_type = 1;                                                \
			size_t     len = snprintf(NULL, 0, "%lld", (long long)accum);  \
			uint64_t **s = dynstr_make(len);                               \
			snprintf((char *)(*s + 1), len + 1, "%lld", (long long)accum); \
			dynstr_incref(s);                                              \
			accum = (uint64_t)s;                                           \
		}                                                                  \
	} while (0)
#define arg_str_cast(i)                                                      \
	do {                                                                     \
		if (arg_types[i] == 0) {                                             \
			arg_types[i] = 1;                                                \
			size_t     len = snprintf(NULL, 0, "%lld", (long long)args[i]);  \
			uint64_t **s = dynstr_make(len);                                 \
			snprintf((char *)(*s + 1), len + 1, "%lld", (long long)args[i]); \
			dynstr_incref(s);                                                \
			args[i] = (uint64_t)s;                                           \
		}                                                                    \
	} while (0)

// Some convenience reference-count related macros.
#define accum_incref()                         \
	do {                                       \
		if (accum_type) {                      \
			dynstr_incref((uint64_t **)accum); \
		}                                      \
	} while (0)
#define accum_decref()                         \
	do {                                       \
		if (accum_type) {                      \
			dynstr_decref((uint64_t **)accum); \
		}                                      \
	} while (0)
#define stack_incref(i)                           \
	do {                                          \
		if (stack_types[i]) {                     \
			dynstr_incref((uint64_t **)stack[i]); \
		}                                         \
	} while (0)
#define stack_decref(i)                           \
	do {                                          \
		if (stack_types[i]) {                     \
			dynstr_decref((uint64_t **)stack[i]); \
		}                                         \
	} while (0)
#define accum_assign(x, t)                   \
	do {                                     \
		uint64_t old = accum;                \
		char     old_t = accum_type;         \
		accum = x;                           \
		accum_type = t;                      \
		if (old_t) {                         \
			dynstr_decref((uint64_t **)old); \
		}                                    \
		accum_incref();                      \
	} while (0)
#define stack_assign(i, x, t)                \
	do {                                     \
		uint64_t old = stack[i];             \
		char     old_t = stack_types[i];     \
		stack[i] = x;                        \
		stack_types[i] = t;                  \
		if (old_t) {                         \
			dynstr_decref((uint64_t **)old); \
		}                                    \
		stack_incref(i);                     \
	} while (0);

		// Execute the corresponding instruction.
		switch (instr) {
		case INSTR_PUT: {
			accum_assign(args[0], arg_types[0]);
			break;
		}

		case INSTR_CLS: {
			if (accum_type == 1 && (uint64_t **)accum == strlit_tbl) {
				accum_assign(args[0], arg_types[0]);
			}
			break;
		}

		case INSTR_ST: {
			arg_int_cast(0);
			arg_int_cast(1);
			uint64_t off = args[0] + args[1];
			stack_assign(off, accum, accum_type);
			break;
		}

		case INSTR_MST: {
			arg_int_cast(0);
			arg_int_cast(1);
			uint64_t off = args[0] + args[1];
			stack_assign(off, accum, accum_type);
			accum_assign((uint64_t)strlit_tbl, 1);
			break;
		}

		case INSTR_LD: {
			arg_int_cast(0);
			arg_int_cast(1);
			uint64_t off = args[0] + args[1];
			accum_assign(stack[off], stack_types[off]);
			break;
		}

		case INSTR_MLD: {
			arg_int_cast(0);
			arg_int_cast(1);
			uint64_t off = args[0] + args[1];
			accum_assign(stack[off], stack_types[off]);
			stack_assign(off, (uint64_t)strlit_tbl, 1);
			break;
		}

		case INSTR_SWP: {
			arg_int_cast(0);
			arg_int_cast(1);
			uint64_t off = args[0] + args[1];
			uint64_t t = accum;
			char     t_t = accum_type;
			accum_assign(stack[off], stack_types[off]);
			stack_assign(off, t, t_t);
			break;
		}

		case INSTR_ADD: {
			arg_int_cast(0);
			acc_int_cast();
			accum += args[0];
			break;
		}

		case INSTR_SUB: {
			arg_int_cast(0);
			acc_int_cast();
			accum -= args[0];
			break;
		}

		case INSTR_NEG: {
			acc_int_cast();
			accum = -accum;
			break;
		}

		case INSTR_MUL: {
			arg_int_cast(0);
			acc_int_cast();
			accum *= args[0];
			break;
		}

		case INSTR_DIV: {
			arg_int_cast(0);
			acc_int_cast();
			accum /= args[0];
			break;
		}

		case INSTR_MOD: {
			arg_int_cast(0);
			acc_int_cast();
			accum = (int64_t)accum % (int64_t)args[0];
			break;
		}

		case INSTR_LEN: {
			acc_str_cast();
			accum_assign(**(uint64_t **)accum, 0);
			break;
		}

		case INSTR_INS: {
			acc_str_cast();
			arg_int_cast(0);
			arg_str_cast(1);
			if ((int64_t)args[0] < 0 || args[0] >= **(uint64_t **)accum) {
				accum_assign((uint64_t)strlit_tbl, 1);
			} else {
				size_t arg1_len = **(uint64_t **)args[1];
				size_t accum_len = **(uint64_t **)accum;

				uint64_t **s = dynstr_make(accum_len + arg1_len);
				memcpy(*s + 1, *(uint64_t **)accum + 1, args[0]);
				memcpy(
					(char *)(*s + 1) + args[0],
					*(uint64_t **)(args[1]) + 1,
					arg1_len
				);
				memcpy(
					(char *)(*s + 1) + args[0] + arg1_len,
					(char *)(*(uint64_t **)accum + 1) + args[0],
					accum_len - args[0]
				);
				accum_assign((uint64_t)s, 1);
			}
			break;
		}

		case INSTR_CAT: {
			acc_str_cast();
			arg_str_cast(0);

			size_t arg_len = **(uint64_t **)args[0];
			size_t accum_len = **(uint64_t **)accum;

			uint64_t **s = dynstr_make(accum_len + arg_len);
			memcpy(*s + 1, *(uint64_t **)accum + 1, accum_len);
			memcpy(
				(char *)(*s + 1) + accum_len,
				*(uint64_t **)args[0] + 1,
				arg_len
			);
			accum_assign((uint64_t)s, 1);

			break;
		}

		case INSTR_SBS: {
			acc_str_cast();
			arg_int_cast(0);
			arg_int_cast(1);

			size_t accum_len = **(uint64_t **)accum;
			args[1] =
				args[1] < accum_len - args[0] ? args[1] : accum_len - args[0];
			uint64_t **s = dynstr_make(args[1]);
			memcpy(
				*s + 1,
				(char *)(*(uint64_t **)accum + 1) + args[0],
				args[1]
			);
			accum_assign((uint64_t)s, 1);

			break;
		}

		case INSTR_CMP: {
			acc_str_cast();
			arg_str_cast(0);

			size_t accum_len = **(uint64_t **)accum;
			size_t arg_len = **(uint64_t **)args[0];

			int cmp = memcmp(
				*(uint64_t **)accum + 1,
				*(uint64_t **)args[0] + 1,
				arg_len < accum_len ? arg_len : accum_len
			);
			if (cmp || accum_len == arg_len) {
				accum_assign(cmp, 0);
			} else {
				accum_assign(accum_len < arg_len ? -1 : 1, 0);
			}

			break;
		}

		case INSTR_CASI: {
			acc_str_cast();
			arg_int_cast(0);

			size_t accum_len = **(uint64_t **)accum;
			if (args[0] < 0 || args[0] >= accum_len) {
				accum_assign((uint64_t)strlit_tbl, 1);
			} else {
				accum_assign(((char *)(*(uint64_t **)accum + 1))[args[0]], 0);
			}

			break;
		}

		case INSTR_IASC: {
			acc_int_cast();

			char **s = (char **)dynstr_make(1);
			(*s)[sizeof(uint64_t)] = accum % 128;
			accum_assign((uint64_t)s, 1);

			break;
		}

		case INSTR_STOI: {
			acc_str_cast();
			arg_int_cast(0);

			if (args[0] < 2 || args[0] > 36) {
				accum_assign((uint64_t)strlit_tbl, 1);
				break;
			}

			char    digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
			size_t  acc_len = **(uint64_t **)accum;
			char   *acc_str = (char *)(*(uint64_t **)accum + 1);
			int64_t num = 0;
			size_t  i = 0;

			if (acc_str[0] == '-') {
				i++;
			}
			for (; i < acc_len; i++) {
				if (tolower(acc_str[i]) > digits[args[0] - 1] ||
				    !isalnum(acc_str[i])) {
					break;
				}
				num += isdigit(acc_str[i]) ? acc_str[i] - '0'
				                           : 10 + tolower(acc_str[i]) - 'a';
				num *= args[0];
			}
			if (acc_str[0] == '-') {
				num *= -1;
			}

			if (i == 0) {
				accum_assign((uint64_t)strlit_tbl, 1);
			} else {
				accum_assign(num / (int64_t)args[0], 0);
			}

			break;
		}

		case INSTR_ITOS: {
			acc_int_cast();
			arg_int_cast(0);

			if (args[0] < 2 || args[0] > 36) {
				accum_assign((uint64_t)strlit_tbl, 1);
				break;
			}

			uint64_t **s = dynstr_make(65
			); // absolute maximum possible - will adjust later
			char      *s_s = (char *)(*(uint64_t **)s + 1);

			size_t nconv = 0;
			char   neg = 0;
			if ((int64_t)accum < 0) {
				accum = -accum;
				neg = 1;
			}
			while (accum > 0) {
				int64_t x = accum % args[0];
				accum /= args[0];
				s_s[nconv++] = x >= 0 && x <= 9 ? '0' + x : 'a' + x - 10;
			}
			if (neg) {
				s_s[nconv++] = '-';
			}
			// Reverse the string.
			for (size_t i = 0; i < nconv / 2; i++) {
				char t = s_s[i];
				s_s[i] = s_s[nconv - i - 1];
				s_s[nconv - i - 1] = t;
			}
			// Fix the string length.
			**s = (uint64_t)nconv;
			accum_assign((uint64_t)s, 1);

			break;
		}

		case INSTR_JMP: {
			arg_int_cast(0);
			instr_idx = args[0] - 1;
			break;
		}

		case INSTR_JB: {
			arg_int_cast(0);
			// acc_int_cast modifies the value in the accumulator. This is
			// normally fine as almost all instructions using it end up
			// casting the value anyway, but the j* instructions cannot do that.
			accum_incref();
			args[1] = accum;
			arg_int_cast(1);
			if (args[1] < 0) {
				instr_idx = args[0] - 1;
			}
			break;
		}

		case INSTR_JE: {
			arg_int_cast(1);
			accum_incref();
			args[1] = accum;
			arg_int_cast(1);
			if (args[1] == 0) {
				instr_idx = args[0] - 1;
			}
			break;
		}

		case INSTR_JA: {
			arg_int_cast(1);
			accum_incref();
			args[1] = accum;
			arg_int_cast(1);
			if (args[1] > 0) {
				instr_idx = args[0] - 1;
			}
			break;
		}

		case INSTR_NOP: {
			break;
		}

		case INSTR_TYPE: {
			if (arg_types[0] && (uint64_t **)args[0] == strlit_tbl) {
				accum_assign(-1, 0);
			} else {
				accum_assign(arg_types[0], 0);
			}
		}

		default: {
			// Unimplemented?? Bad instruction???
			break;
		}
		}

		// Step to next instruction.
		instr_idx++;
	}

	// Print accumulator :D
	printf("ACCUM: ");
	if (accum_type) {
		printf("%s\n", (char *)((*(uint64_t **)accum) + 1));
	} else {
		printf("%ld\n", (int64_t)accum);
	}

	/* CLEANUP */
	free(stack_types);
	free(stack);
	free(offset_tbl);
	free(instr_tbl);
	free(strlit_tbl);
	free(code_raw);
}
