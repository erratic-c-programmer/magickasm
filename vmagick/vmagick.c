#include "dynstr.h"
#include "instruction_opcodes.h"
#include "stackval.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

	// Instruction table
	size_t     instr_tbl_sz = *(load_bptr++);
	uint64_t  *instr_tbl = load_bptr;
	load_bptr += instr_tbl_sz << 2;  // 8 qwords per instruction

	// Set stack up.
	const size_t stack_sz = (1 << 16) - 1;
	StackVal    *stack = calloc(stack_sz, sizeof(*stack));

	/* FETCH-DECODE-EXECUTE */
	uint64_t instr_idx = 1; // one-indexed
	while (instr_idx - 1 < instr_tbl_sz) {
		// Fetch the instruction.
		const uint64_t instr_raw = instr_tbl[(instr_idx - 1) << 2];
		// Decode it.
		const uint64_t instr = (instr_raw << 16) >> 16;
		char           type_flags = (instr_raw >> 48) & 0b1111UL;
		char           reg_flags = (instr_raw >> 52) & 0b1111UL;

		// Load the argument into temp arg registers - raw for now.
		uint64_t args[4];
		uint64_t arg_types[4];
		for (int i = 0; i < 3; i++) {
			uint64_t arg_raw = instr_tbl[((instr_idx - 1) << 2) + 1 + i];
			if (reg_flags & (1 << i)) {
				// Register argument, have to fetch.
				args[i] = stack[arg_raw].val;
				arg_types[i] = stack[arg_raw].type;
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
#define acc_int_cast()                                  \
	do {                                                \
		if (stack[0].type == 1) {                       \
			dynstr_decref((uint64_t **)stack[0].val);   \
			stack[0].val = **(uint64_t **)stack[0].val; \
			stack[0].type = 0;                          \
		}                                               \
	} while (0)
#define arg_int_cast(i)                          \
	do {                                         \
		if (arg_types[i] == 1) {                 \
			dynstr_decref((uint64_t **)args[i]); \
			args[i] = **(uint64_t **)args[i];    \
			arg_types[i] = 0;                    \
		}                                        \
	} while (0)
#define acc_str_cast()                                                           \
	do {                                                                         \
		if (stack[0].type == 0) {                                                \
			stack[0].type = 1;                                                   \
			size_t     len = snprintf(NULL, 0, "%lld", (long long)stack[0].val); \
			uint64_t **s = dynstr_make(len);                                     \
			snprintf(                                                            \
				(char *)(*s + 1),                                                \
				len + 1,                                                         \
				"%lld",                                                          \
				(long long)stack[0].val                                          \
			);                                                                   \
			dynstr_incref(s);                                                    \
			stack[0].val = (uint64_t)s;                                          \
		}                                                                        \
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
#define stack_incref(i)                               \
	do {                                              \
		if (stack[i].type) {                          \
			dynstr_incref((uint64_t **)stack[i].val); \
		}                                             \
	} while (0)
#define stack_decref(i)                               \
	do {                                              \
		if (stack[i].type) {                          \
			dynstr_decref((uint64_t **)stack[i].val); \
		}                                             \
	} while (0)
#define stack_assign(i, x, t)                    \
	do {                                         \
		StackVal old = stack[i];                 \
		stack[i].val = x;                        \
		stack[i].type = t;                       \
		if (old.type) {                          \
			dynstr_decref((uint64_t **)old.val); \
		}                                        \
		stack_incref(i);                         \
	} while (0);

		// Execute the corresponding instruction.
		switch (instr) {

		// General memory primitives.
		case INSTR_PUT: {
			stack_assign(0, args[0], arg_types[0]);
			break;
		}

		case INSTR_CLS: {
			if (stack[0].type == 1 && (uint64_t **)stack[0].val == strlit_tbl) {
				stack_assign(0, args[0], arg_types[0]);
			}
			break;
		}

		case INSTR_ST: {
			arg_int_cast(0);
			arg_int_cast(1);
			uint64_t off = args[0] + args[1];
			stack_assign(off, stack[0].val, stack[0].type);
			break;
		}

		case INSTR_MST: {
			arg_int_cast(0);
			arg_int_cast(1);
			uint64_t off = args[0] + args[1];
			stack_assign(off, stack[0].val, stack[0].type);
			stack_assign(0, (uint64_t)strlit_tbl, 1);
			break;
		}

		case INSTR_LD: {
			arg_int_cast(0);
			arg_int_cast(1);
			uint64_t off = args[0] + args[1];
			stack_assign(0, stack[off].val, stack[off].type);
			break;
		}

		case INSTR_MLD: {
			arg_int_cast(0);
			arg_int_cast(1);
			uint64_t off = args[0] + args[1];
			stack_assign(0, stack[off].val, stack[off].type);
			stack_assign(off, (uint64_t)strlit_tbl, 1);
			break;
		}

		case INSTR_SWP: {
			arg_int_cast(0);
			arg_int_cast(1);
			uint64_t off = args[0] + args[1];
			StackVal t = stack[0];
			stack_assign(0, stack[off].val, stack[off].type);
			stack_assign(off, t.val, t.type);
			break;
		}

		// General numeric primitives.
		case INSTR_ADD: {
			arg_int_cast(0);
			acc_int_cast();
			stack[0].val = (uint64_t)((int64_t)stack[0].val + (int64_t)args[0]);
			break;
		}

		case INSTR_SUB: {
			arg_int_cast(0);
			acc_int_cast();
			stack[0].val = (uint64_t)((int64_t)stack[0].val - (int64_t)args[0]);
			break;
		}

		case INSTR_NEG: {
			acc_int_cast();
			stack[0].val = (uint64_t)(-(int64_t)stack[0].val);
			break;
		}

		case INSTR_MUL: {
			arg_int_cast(0);
			acc_int_cast();
			stack[0].val = (uint64_t)((int64_t)stack[0].val * (int64_t)args[0]);
			break;
		}

		case INSTR_DIV: {
			arg_int_cast(0);
			acc_int_cast();
			stack[0].val = (uint64_t)((int64_t)stack[0].val / (int64_t)args[0]);
			break;
		}

		case INSTR_MOD: {
			arg_int_cast(0);
			acc_int_cast();
			stack[0].val = (uint64_t)((int64_t)stack[0].val % (int64_t)args[0]);
			break;
		}

		case INSTR_SHL: {
			arg_int_cast(0);
			acc_int_cast();
			stack[0].val <<= args[0];
			break;
		}

		case INSTR_SHR: {
			arg_int_cast(0);
			acc_int_cast();
			stack[0].val >>= args[0];
			break;
		}

		// General string primitives.
		case INSTR_LEN: {
			acc_str_cast();
			stack_assign(0, **(uint64_t **)stack[0].val, 0);
			break;
		}

		case INSTR_INS: {
			acc_str_cast();
			arg_int_cast(0);
			arg_str_cast(1);
			if ((int64_t)args[0] < 0 ||
			    args[0] >= **(uint64_t **)stack[0].val) {
				stack_assign(0, (uint64_t)strlit_tbl, 1);
			} else {
				size_t arg1_len = **(uint64_t **)args[1];
				size_t accum_len = **(uint64_t **)stack[0].val;

				uint64_t **s = dynstr_make(accum_len + arg1_len);
				memcpy(*s + 1, *(uint64_t **)stack[0].val + 1, args[0]);
				memcpy(
					(char *)(*s + 1) + args[0],
					*(uint64_t **)(args[1]) + 1,
					arg1_len
				);
				memcpy(
					(char *)(*s + 1) + args[0] + arg1_len,
					(char *)(*(uint64_t **)stack[0].val + 1) + args[0],
					accum_len - args[0]
				);
				stack_assign(0, (uint64_t)s, 1);
			}
			break;
		}

		case INSTR_CAT: {
			acc_str_cast();
			arg_str_cast(0);

			size_t arg_len = **(uint64_t **)args[0];
			size_t accum_len = **(uint64_t **)stack[0].val;

			uint64_t **s = dynstr_make(accum_len + arg_len);
			memcpy(*s + 1, *(uint64_t **)stack[0].val + 1, accum_len);
			memcpy(
				(char *)(*s + 1) + accum_len,
				*(uint64_t **)args[0] + 1,
				arg_len
			);
			stack_assign(0, (uint64_t)s, 1);

			break;
		}

		case INSTR_SBS: {
			acc_str_cast();
			arg_int_cast(0);
			arg_int_cast(1);

			size_t accum_len = **(uint64_t **)stack[0].val;
			args[1] =
				args[1] < accum_len - args[0] ? args[1] : accum_len - args[0];
			uint64_t **s = dynstr_make(args[1]);
			memcpy(
				*s + 1,
				(char *)(*(uint64_t **)stack[0].val + 1) + args[0],
				args[1]
			);
			stack_assign(0, (uint64_t)s, 1);

			break;
		}

		case INSTR_CMP: {
			acc_str_cast();
			arg_str_cast(0);

			size_t accum_len = **(uint64_t **)stack[0].val;
			size_t arg_len = **(uint64_t **)args[0];

			int cmp = memcmp(
				*(uint64_t **)stack[0].val + 1,
				*(uint64_t **)args[0] + 1,
				arg_len < accum_len ? arg_len : accum_len
			);
			if (cmp || accum_len == arg_len) {
				stack_assign(0, cmp, 0);
			} else {
				stack_assign(0, accum_len < arg_len ? -1 : 1, 0);
			}

			break;
		}

		case INSTR_CASI: {
			acc_str_cast();
			arg_int_cast(0);

			size_t accum_len = **(uint64_t **)stack[0].val;
			if (args[0] < 0 || args[0] >= accum_len) {
				stack_assign(0, (uint64_t)strlit_tbl, 1);
			} else {
				stack_assign(
					0,
					((char *)(*(uint64_t **)stack[0].val + 1))[args[0]],
					0
				);
			}

			break;
		}

		case INSTR_IASC: {
			acc_int_cast();

			char **s = (char **)dynstr_make(1);
			(*s)[sizeof(uint64_t)] = stack[0].val % 128;
			stack_assign(0, (uint64_t)s, 1);

			break;
		}

		case INSTR_STOI: {
			acc_str_cast();
			arg_int_cast(0);

			if (args[0] < 2 || args[0] > 36) {
				stack_assign(0, (uint64_t)strlit_tbl, 1);
				break;
			}

			char    digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
			size_t  acc_len = **(uint64_t **)stack[0].val;
			char   *acc_str = (char *)(*(uint64_t **)stack[0].val + 1);
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
				stack_assign(0, (uint64_t)strlit_tbl, 1);
			} else {
				stack_assign(0, num / (int64_t)args[0], 0);
			}

			break;
		}

		case INSTR_ITOS: {
			acc_int_cast();
			arg_int_cast(0);

			if (args[0] < 2 || args[0] > 36) {
				stack_assign(0, (uint64_t)strlit_tbl, 1);
				break;
			}

			uint64_t **s = dynstr_make(65
			); // absolute maximum possible - will adjust later
			char      *s_s = (char *)(*(uint64_t **)s + 1);

			size_t nconv = 0;
			char   neg = 0;
			if ((int64_t)stack[0].val < 0) {
				stack[0].val = -stack[0].val;
				neg = 1;
			}
			while (stack[0].val > 0) {
				int64_t x = stack[0].val % args[0];
				stack[0].val /= args[0];
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
			stack_assign(0, (uint64_t)s, 1);

			break;
		}

		// Branching instructions.
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
			stack_incref(0);
			args[1] = stack[0].val;
			arg_int_cast(1);
			if (args[1] < 0) {
				instr_idx = args[0] - 1;
			}
			break;
		}

		case INSTR_JE: {
			arg_int_cast(1);
			stack_incref(0);
			args[1] = stack[0].val;
			arg_int_cast(1);
			if (args[1] == 0) {
				instr_idx = args[0] - 1;
			}
			break;
		}

		case INSTR_JA: {
			arg_int_cast(1);
			stack_incref(0);
			args[1] = stack[0].val;
			arg_int_cast(1);
			if (args[1] > 0) {
				instr_idx = args[0] - 1;
			}
			break;
		}

		// Misc.
		case INSTR_NOP: {
			break;
		}

		case INSTR_TYPE: {
			if (arg_types[0] && (uint64_t **)args[0] == strlit_tbl) {
				stack_assign(0, -1, 0);
			} else {
				stack_assign(0, arg_types[0], 0);
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
	if (stack[0].type) {
		printf("%s\n", (char *)((*(uint64_t **)stack[0].val) + 1));
	} else {
		printf("%ld\n", (int64_t)stack[0].val);
	}

	/* CLEANUP */
	free(stack);
	free(strlit_tbl);
	free(code_raw);
}
