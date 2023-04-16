// Opcodes are 6 bytes long for alignment.
// Format: #define INSTR_<NAME> NUMBER // NARGS
// Must be strictly be followed because the assembler parses this.
#define INSTR_PUT	0	// 1
#define INSTR_CLS	1	// 1
#define INSTR_ST	2	// 2
#define INSTR_MST	3	// 2
#define INSTR_LD	4	// 2
#define INSTR_MLD	5	// 2
#define INSTR_SWP	6	// 2
#define INSTR_ADD	7	// 1
#define INSTR_SUB	8	// 1
#define INSTR_NEG	9	// 0
#define INSTR_MUL	10	// 1
#define INSTR_DIV	11	// 1
#define INSTR_MOD	12	// 1
#define INSTR_LEN	13	// 0
#define INSTR_INS	14	// 2
#define INSTR_CAT	15	// 1
#define INSTR_SBS	16	// 2
#define INSTR_CMP	17	// 1
#define INSTR_CASI	18	// 1
#define INSTR_IASC	19	// 0
#define INSTR_STOI	20	// 1
#define INSTR_ITOS	21	// 1
#define INSTR_JMP	22	// 1
#define INSTR_JB	23	// 1
#define INSTR_JE	24	// 1
#define INSTR_JA	25	// 1
#define INSTR_NOP	26	// 0
#define INSTR_TYPE	27	// 0
#define INSTR_RUN	28	// 1
