#include "dynstr.h"
#include "instruction_opcodes.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NOT_INT(x)  (!(x >> 56))
#define GET_INT(x)  ((int32_t)(x & 0xffffffff))
#define MAKE_INT(x) ((int32_t)x | (1UL << 56))

// Error flag - can be global because it's not actually used that much.
char errorflag = 0;

// Some convenience reference-count related macros.
#define stack_incref(i)                           \
	do {                                          \
		if (NOT_INT(stack[i])) {                  \
			dynstr_incref((uint64_t **)stack[i]); \
		}                                         \
	} while (0)
#define stack_decref(i)                           \
	do {                                          \
		if (NOT_INT(stack[i])) {                  \
			dynstr_decref((uint64_t **)stack[i]); \
		}                                         \
	} while (0)
#define stack_assign(i, x)                   \
	do {                                     \
		uint64_t old = stack[i];             \
		stack[i] = x;                        \
		if (NOT_INT(old)) {                  \
			dynstr_decref((uint64_t **)old); \
		}                                    \
		stack_incref(i);                     \
	} while (0);

#define INSTR_IMPL(instr)          \
	uint64_t instr_##instr##_impl( \
		char     arg_types[],      \
		uint64_t args[],           \
		uint64_t accum            \
	)

// Actual instruction implementations.

// General memory primitives.
inline INSTR_IMPL(PUT)
{
	if (arg_types[0]) {
		return args[0];
	} else {
		return MAKE_INT(args[0]);
	}
}

inline INSTR_IMPL(GETE)
{
	return MAKE_INT(errorflag);
}

inline INSTR_IMPL(SETE)
{
	if (NOT_INT(accum) || GET_INT(accum)) {
		errorflag = 1;
	} else {
		errorflag = 0;
	}
	return accum;
}

inline INSTR_IMPL(ADD)
{
	return MAKE_INT((uint64_t)(GET_INT(accum) + args[0]));
}

inline INSTR_IMPL(SUB)
{
	return MAKE_INT((uint64_t)(GET_INT(accum) - args[0]));
}

inline INSTR_IMPL(NEG)
{
	return MAKE_INT((uint64_t)(-GET_INT(accum)));
}

inline INSTR_IMPL(MUL)
{
	return MAKE_INT((uint64_t)(GET_INT(accum) * args[0]));
}

inline INSTR_IMPL(DIV)
{
	return MAKE_INT((uint64_t)(GET_INT(accum) / args[0]));
}

inline INSTR_IMPL(MOD)
{
	return MAKE_INT((uint64_t)(GET_INT(accum) % args[0]));
}

inline INSTR_IMPL(SHL)
{
	return MAKE_INT((uint64_t)(GET_INT(accum) << args[0]));
}

inline INSTR_IMPL(SHR)
{
	return MAKE_INT((uint64_t)(GET_INT(accum) >> args[0]));
}

// General string primitives.

inline INSTR_IMPL(LEN)
{
	return MAKE_INT(**(uint64_t **)accum);
}

INSTR_IMPL(INS)
{
	if (args[0] < 0 || args[0] >= **(uint64_t **)accum) {
		errorflag = 1;
		return accum;
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
		return (uint64_t)s;
	}
}

INSTR_IMPL(CAT)
{
	size_t arg_len = **(uint64_t **)args[0];
	size_t accum_len = **(uint64_t **)accum;

	uint64_t **s = dynstr_make(accum_len + arg_len);
	memcpy(*s + 1, *(uint64_t **)accum + 1, accum_len);
	memcpy(
		(char *)(*s + 1) + accum_len,
		*(uint64_t **)args[0] + 1,
		arg_len
	);
	return (uint64_t)s;
}

INSTR_IMPL(SBS)
{
	size_t accum_len = **(uint64_t **)accum;
	args[1] = args[1] < accum_len - args[0] ? args[1]
											: accum_len - args[0];
	uint64_t **s = dynstr_make(args[1]);
	memcpy(
		*s + 1,
		(char *)(*(uint64_t **)accum + 1) + args[0],
		args[1]
	);
	return (uint64_t)s;
}

INSTR_IMPL(CMP)
{
	size_t accum_len = **(uint64_t **)accum;
	size_t arg_len = **(uint64_t **)args[0];

	int cmp = memcmp(
		*(uint64_t **)accum + 1,
		*(uint64_t **)args[0] + 1,
		arg_len < accum_len ? arg_len : accum_len
	);
	if (cmp || accum_len == arg_len) {
		return MAKE_INT(cmp);
	} else {
		return MAKE_INT(accum_len < arg_len ? -1 : 1);
	}
}

INSTR_IMPL(CASI)
{
	size_t accum_len = **(uint64_t **)accum;
	if (args[0] < 0 || args[0] >= accum_len) {
		errorflag = 1;
		return accum;
	} else {
		return MAKE_INT(
			((char *)(*(uint64_t **)accum + 1))[args[0]]
		);
	}
}

inline INSTR_IMPL(IASC)
{
	char **s = (char **)dynstr_make(1);
	(*s)[sizeof(uint64_t)] = accum % 128;
	return (uint64_t)s;
}

INSTR_IMPL(STOI)
{
	if (args[0] < 2 || args[0] > 36) {
		errorflag = 1;
		return accum;
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
		errorflag = 1;
		return accum;
	} else {
		return MAKE_INT(num / args[0]);
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

	// Set stack up and error flag.
	const size_t stack_sz = (1 << 16) - 1;
	uint64_t    *stack = calloc(stack_sz, sizeof(*stack));
	for (size_t i = 0; i < stack_sz; i++) {
		stack[i] = MAKE_INT(0);
	}

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

		// Load the argument into temp arg registers and do conversions.
		uint64_t args[4];
		char     arg_types[4];
		for (int i = 0; i < nargs; i++) {
			uint64_t arg_raw = instr_tbl[instr_idx - 1][i + 1];
			if (reg_flags & (1 << i)) {
				// Register argument, have to fetch.
				args[i] = stack[arg_raw];
				arg_types[i] = NOT_INT(args[i]);
				if (!arg_types[i]) {
					args[i] = GET_INT(args[i]);
				}
			} else if (type_flags & (1 << i)) {
				// String/empty literal argument.
				args[i] = (uint64_t)(strlit_tbl + arg_raw);
				arg_types[i] = 1;
			} else {
				// Integer literal argument.
				args[i] = GET_INT(arg_raw);
				arg_types[i] = 0;
			}
		}

		// Execute the corresponding instruction.
		switch (instr) {

#define CASE_INSTR(instr)                                                 \
	case INSTR_##instr: {                                                 \
		stack_assign(0, instr_##instr##_impl(arg_types, args, stack[0])); \
		break;                                                            \
	}

			// General memory primitives.
			CASE_INSTR(PUT);

			case INSTR_ST: {
				uint64_t off = args[0] + args[1];
				stack_assign(off, stack[0]);
				break;
			}

			case INSTR_LD: {
				uint64_t off = args[0] + args[1];
				stack_assign(0, stack[off]);
				break;
			}

			case INSTR_SWP: {
				uint64_t off = args[0] + args[1];
				uint64_t t = stack[0];
				stack_assign(0, stack[off]);
				stack_assign(off, t);
				break;
			}

				CASE_INSTR(GETE);

				CASE_INSTR(SETE);

				// General numeric primitives.
				CASE_INSTR(ADD);
				CASE_INSTR(SUB);
				CASE_INSTR(NEG);
				CASE_INSTR(MUL);
				CASE_INSTR(MOD);
				CASE_INSTR(SHL);
				CASE_INSTR(SHR);

				// General string primitives.
				CASE_INSTR(LEN);
				CASE_INSTR(INS);
				CASE_INSTR(CAT);
				CASE_INSTR(SBS);
				CASE_INSTR(CMP);
				CASE_INSTR(CASI);
				CASE_INSTR(IASC);
				CASE_INSTR(STOI);

			case INSTR_ITOS: {
				if (args[0] < 2 || args[0] > 36) {
					errorflag = 1;
					break;
				}

				uint64_t **s = dynstr_make(65
				); // absolute maximum possible - will adjust later
				char      *s_s = (char *)(*(uint64_t **)s + 1);

				size_t nconv = 0;
				stack[0] = GET_INT(stack[0]);
				char neg = 0;
				if (stack[0] < 0) {
					stack[0] = -stack[0];
					neg = 1;
				}
				while (stack[0] > 0) {
					int32_t x = stack[0] % args[0];
					stack[0] /= args[0];
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
				stack_assign(0, (uint64_t)s);

				break;
			}

			// Branching instructions.
			case INSTR_JMP: {
				instr_idx = args[0] - 1;
				break;
			}

			case INSTR_JB: {
				if (GET_INT(stack[0]) < 0) {
					instr_idx = args[0] - 1;
				}
				break;
			}

			case INSTR_JE: {
				if (GET_INT(args[1]) == 0) {
					instr_idx = args[0] - 1;
				}
				break;
			}

			case INSTR_JA: {
				if (GET_INT(stack[0]) > 0) {
					instr_idx = args[0] - 1;
				}
				break;
			}

			// Misc.
			case INSTR_NOP: {
				break;
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
	if (NOT_INT(stack[0])) {
		printf("%s\n", (char *)((*(uint64_t **)stack[0]) + 1));
	} else {
		printf("%d\n", GET_INT(stack[0]));
	}

	/* CLEANUP */
	free(stack);
	free(offset_tbl);
	free(instr_tbl);
	free(strlit_tbl);
	free(code_raw);
}
