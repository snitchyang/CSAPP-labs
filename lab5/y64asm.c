#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "y64asm.h"

line_t *line_head = NULL;
line_t *line_tail = NULL;
int lineno = 0;
int max_byte = 0;

#define err_print(_s, _a ...) do { \
  if (lineno < 0) \
    fprintf(stderr, "[--]: "_s"\n", ## _a); \
  else \
    fprintf(stderr, "[L%d]: "_s"\n", lineno, ## _a); \
} while (0);

void set_n_val(byte_t *dest, long val, int bytes)
{
	for (int i = 0; i < bytes; i++) {
		dest[i] = val & 0xFF;
		val >>= 8;
	}	
}

int64_t vmaddr = 0;    /* vm addr */

/* register table */
const reg_t reg_table[REG_NONE] = {
    {"%rax", REG_RAX, 4},
    {"%rcx", REG_RCX, 4},
    {"%rdx", REG_RDX, 4},
    {"%rbx", REG_RBX, 4},
    {"%rsp", REG_RSP, 4},
    {"%rbp", REG_RBP, 4},
    {"%rsi", REG_RSI, 4},
    {"%rdi", REG_RDI, 4},
    {"%r8",  REG_R8,  3},
    {"%r9",  REG_R9,  3},
    {"%r10", REG_R10, 4},
    {"%r11", REG_R11, 4},
    {"%r12", REG_R12, 4},
    {"%r13", REG_R13, 4},
    {"%r14", REG_R14, 4}
};
const reg_t* find_register(char *name)
{
    int i;
    for (i = 0; i < REG_NONE; i++)
        if (!strncmp(name, reg_table[i].name, reg_table[i].namelen))
            return &reg_table[i];
    return NULL;
}


/* instruction set */
instr_t instr_set[] = {
    {"nop", 3,   HPACK(I_NOP, F_NONE), 1 },
    {"halt", 4,  HPACK(I_HALT, F_NONE), 1 },
    {"rrmovq", 6,HPACK(I_RRMOVQ, F_NONE), 2 },
    {"cmovle", 6,HPACK(I_RRMOVQ, C_LE), 2 },
    {"cmovl", 5, HPACK(I_RRMOVQ, C_L), 2 },
    {"cmove", 5, HPACK(I_RRMOVQ, C_E), 2 },
    {"cmovne", 6,HPACK(I_RRMOVQ, C_NE), 2 },
    {"cmovge", 6,HPACK(I_RRMOVQ, C_GE), 2 },
    {"cmovg", 5, HPACK(I_RRMOVQ, C_G), 2 },
    {"irmovq", 6,HPACK(I_IRMOVQ, F_NONE), 10 },
    {"rmmovq", 6,HPACK(I_RMMOVQ, F_NONE), 10 },
    {"mrmovq", 6,HPACK(I_MRMOVQ, F_NONE), 10 },
    {"addq", 4,  HPACK(I_ALU, A_ADD), 2 },
    {"subq", 4,  HPACK(I_ALU, A_SUB), 2 },
    {"andq", 4,  HPACK(I_ALU, A_AND), 2 },
    {"xorq", 4,  HPACK(I_ALU, A_XOR), 2 },
    {"jmp", 3,   HPACK(I_JMP, C_YES), 9 },
    {"jle", 3,   HPACK(I_JMP, C_LE), 9 },
    {"jl", 2,    HPACK(I_JMP, C_L), 9 },
    {"je", 2,    HPACK(I_JMP, C_E), 9 },
    {"jne", 3,   HPACK(I_JMP, C_NE), 9 },
    {"jge", 3,   HPACK(I_JMP, C_GE), 9 },
    {"jg", 2,    HPACK(I_JMP, C_G), 9 },
    {"call", 4,  HPACK(I_CALL, F_NONE), 9 },
    {"ret", 3,   HPACK(I_RET, F_NONE), 1 },
    {"pushq", 5, HPACK(I_PUSHQ, F_NONE), 2 },
    {"popq", 4,  HPACK(I_POPQ, F_NONE),  2 },
    {".byte", 5, HPACK(I_DIRECTIVE, D_DATA), 1 },
    {".word", 5, HPACK(I_DIRECTIVE, D_DATA), 2 },
    {".long", 5, HPACK(I_DIRECTIVE, D_DATA), 4 },
    {".quad", 5, HPACK(I_DIRECTIVE, D_DATA), 8 },
    {".pos", 4,  HPACK(I_DIRECTIVE, D_POS), 0 },
    {".align", 6,HPACK(I_DIRECTIVE, D_ALIGN), 0 },
    {NULL, 1,    0   , 0 } //end
};

instr_t *find_instr(char *name)
{
    int i;
    for (i = 0; instr_set[i].name; i++)
	if (strncmp(instr_set[i].name, name, instr_set[i].len) == 0)
	    return &instr_set[i];
    return NULL;
}

/* symbol table (don't forget to init and finit it) */
symbol_t *symtab = NULL;

/*
 * find_symbol: scan table to find the symbol
 * args
 *     name: the name of symbol
 *
 * return
 *     symbol_t: the 'name' symbol
 *     NULL: not exist
 */
symbol_t *find_symbol(char *name)
{
	symbol_t *p = symtab->next;
	while (p) {
		if (strcmp(p->name, name) == 0) {
			break;
		}
		p = p->next;	
	}
    return p;
}

/*
 * add_symbol: add a new symbol to the symbol table
 * args
 *     name: the name of symbol
 *
 * return
 *     0: success
 *     -1: error, the symbol has exist
 */
int add_symbol(char *name)
{
    /* check duplicate */
	if (find_symbol(name)) {
		return -1;	
	}
    /* create new symbol_t (don't forget to free it)*/
	symbol_t *newSym = (symbol_t *)malloc(sizeof(symbol_t));
    /* add the new symbol_t to symbol table */
	newSym->addr = vmaddr;
	newSym->name = name;
	newSym->next = symtab->next;
	symtab->next = newSym;
    return 0;
}

/* relocation table (don't forget to init and finit it) */
reloc_t *reltab = NULL;

/*
 * add_reloc: add a new relocation to the relocation table
 * args
 *     name: the name of symbol
 *
 * return
 *     0: success
 *     -1: error, the symbol has exist
 */
void add_reloc(char *name, bin_t *bin)
{

    /* create new reloc_t (don't forget to free it)*/
	reloc_t *newRelocation = (reloc_t *)malloc(sizeof(reloc_t)); 

    /* add the new reloc_t to relocation table */
	newRelocation->name = name;
	newRelocation->y64bin = bin;
	newRelocation->next = reltab->next;
	reltab->next = newRelocation;
}


/* macro for parsing y64 assembly code */
#define IS_DIGIT(s) ((*(s)>='0' && *(s)<='9') || *(s)=='-' || *(s)=='+')
#define IS_LETTER(s) ((*(s)>='a' && *(s)<='z') || (*(s)>='A' && *(s)<='Z'))
#define IS_COMMENT(s) (*(s)=='#')
#define IS_REG(s) (*(s)=='%')
#define IS_IMM(s) (*(s)=='$')

#define IS_BLANK(s) (*(s)==' ' || *(s)=='\t')
#define IS_END(s) (*(s)=='\0')

#define SKIP_BLANK(s) do {  \
  while(!IS_END(s) && IS_BLANK(s))  \
    (s)++;    \
} while(0);

/* return value from different parse_xxx function */
typedef enum { PARSE_ERR=-1, PARSE_REG, PARSE_DIGIT, PARSE_SYMBOL, 
    PARSE_MEM, PARSE_DELIM, PARSE_INSTR, PARSE_LABEL} parse_t;

/*
 * parse_instr: parse an expected data token (e.g., 'rrmovq')
 * args
 *     ptr: point to the start of string
 *     inst: point to the inst_t within instr_set
 *
 * return
 *     PARSE_INSTR: success, move 'ptr' to the first char after token,
 *                            and store the pointer of the instruction to 'inst'
 *     PARSE_ERR: error, the value of 'ptr' and 'inst' are undefined
 */
parse_t parse_instr(char **ptr, instr_t **inst)
{
    /* skip the blank */
	char *p = *ptr;
	SKIP_BLANK(p);

    /* find_instr and check end */
	instr_t* temp;
	if(IS_END(p) || !(temp = find_instr(p))){
		return PARSE_ERR;
	}

    /* set 'ptr' and 'inst' */
	p += temp->len;
	*inst = temp;
	*ptr = p;
    return PARSE_INSTR;
}

/*
 * parse_delim: parse an expected delimiter token (e.g., ',')
 * args
 *     ptr: point to the start of string
 *
 * return
 *     PARSE_DELIM: success, move 'ptr' to the first char after token
 *     PARSE_ERR: error, the value of 'ptr' and 'delim' are undefined
 */
parse_t parse_delim(char **ptr, char delim)
{

    /* skip the blank and check */
	SKIP_BLANK(*ptr);
	if(**ptr==delim && !IS_END(*ptr)){
		(*ptr)++;
		return PARSE_DELIM;
	}
	err_print("Invalid \'%c\'", delim);
    return PARSE_ERR;
}

/*
 * parse_reg: parse an expected register token (e.g., '%rax')
 * args
 *     ptr: point to the start of string
 *     regid: point to the regid of register
 *
 * return
 *     PARSE_REG: success, move 'ptr' to the first char after token, 
 *                         and store the regid to 'regid'
 *     PARSE_ERR: error, the val of 'ptr' and 'regid' are undefined
 */
parse_t parse_reg(char **ptr, regid_t *regid)
{
    /* skip the blank and check */
	SKIP_BLANK(*ptr);
	char *p = *ptr;
	if (IS_END(p)) {
		return PARSE_ERR;
	}

    /* find register */
	const reg_t *temp = find_register(p);
	if (!temp) {
		err_print("Invalid REG")
		return PARSE_ERR;
	}

    /* set 'ptr' and 'regid' */
	p += temp->namelen;
	*regid = temp->id;
	*ptr = p;
    return PARSE_REG;
}

/*
 * parse_symbol: parse an expected symbol token (e.g., 'Main')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *
 * return
 *     PARSE_SYMBOL: success, move 'ptr' to the first char after token,
 *                               and allocate and store name to 'name'
 *     PARSE_ERR: error, the value of 'ptr' and 'name' are undefined
 */
parse_t parse_symbol(char **ptr, char **name)
{
    /* skip the blank and check */
	SKIP_BLANK(*ptr);
	char *p = *ptr;
	if (IS_END(p)) {
		return PARSE_ERR;
	}

    /* allocate name and copy to it */
	char *symbol = (char *)malloc(100 * sizeof(char));
	int len = 0;
	while(!IS_BLANK(p) && (IS_LETTER(p) || (len != 0) && IS_DIGIT(p))){
		symbol[len] = *p;
		len++;
		p++;
	}
	symbol[len] = '\0';

    /* set 'ptr' and 'name' */
	*name = symbol;
	*ptr = p;
	if (strlen(symbol) == 0) {
		return PARSE_ERR;
	}
    return PARSE_SYMBOL;
}

/*
 * parse_digit: parse an expected digit token (e.g., '0x100')
 * args
 *     ptr: point to the start of string
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, move 'ptr' to the first char after token
 *                            and store the value of digit to 'value'
 *     PARSE_ERR: error, the value of 'ptr' and 'value' are undefined
 */
parse_t parse_digit(char **ptr, long *val)
{
    /* skip the blank and check */
	char *p = *ptr;
	SKIP_BLANK(p);
	if(IS_END(p) || !IS_DIGIT(p))
        return PARSE_ERR;

    /* calculate the digit, (NOTE: see strtoull()) */
	int sign;
	if (*p == '-') {
		p++;
		sign = -1;
	}else{
		sign = 1;
	}
	int base;
	if (*(p+1) == 'x') { 
		base = 16;
	} else {
		base = 10;
	}
	long temp = sign * strtoull(p, &p, base);

    /* set 'ptr' and 'val' */
	*ptr =p;
	*val = temp;
    return PARSE_DIGIT;
}

/*
 * parse_imm: parse an expected immediate token (e.g., '$0x100' or 'STACK')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, the immediate token is a digit,
 *                            move 'ptr' to the first char after token,
 *                            and store the value of digit to 'value'
 *     PARSE_SYMBOL: success, the immediate token is a symbol,
 *                            move 'ptr' to the first char after token,
 *                            and allocate and store name to 'name' 
 *     PARSE_ERR: error, the value of 'ptr', 'name' and 'value' are undefined
 */
parse_t parse_imm(char **ptr, char **name, long *val)
{
    /* skip the blank and check */
	char *p = *ptr;
	SKIP_BLANK(p);
	if (IS_END(p)) {
		return PARSE_ERR;
	}

    /* if IS_IMM, then parse the digit */
	if (IS_IMM(p)) {
		*ptr = ++p;
		if (parse_digit(ptr, val) == PARSE_ERR) {
			err_print("Invalid Immediate");
			return PARSE_ERR;
		}
		else {
			return PARSE_DIGIT;
		}
	}

    /* if IS_LETTER, then parse the symbol */
	if (IS_LETTER(p)) {
		if (parse_symbol(ptr, name) == PARSE_ERR) {
			err_print("Invalid immediate");
			return PARSE_ERR;
		}
		else {
			return PARSE_SYMBOL;
		}
	} 
    /* set 'ptr' and 'name' or 'val' */

    return PARSE_ERR;
}

/*
 * parse_mem: parse an expected memory token (e.g., '8(%rbp)')
 * args
 *     ptr: point to the start of string
 *     value: point to the value of digit
 *     regid: point to the regid of register
 *
 * return
 *     PARSE_MEM: success, move 'ptr' to the first char after token,
 *                          and store the value of digit to 'value',
 *                          and store the regid to 'regid'
 *     PARSE_ERR: error, the value of 'ptr', 'value' and 'regid' are undefined
 */
parse_t parse_mem(char **ptr, long *val, regid_t *regid)
{

    /* skip the blank and check */
	char *p = *ptr;
	SKIP_BLANK(p);
	if (IS_END(p)) {
		return PARSE_ERR;
	}

    /* calculate the digit and register, (ex: (%rbp) or 8(%rbp)) */
	if (IS_DIGIT(p)) {
		if (parse_digit(ptr, val) == PARSE_ERR) {
			err_print("Invalid MEM");
			return PARSE_ERR;
		}
	} else {
		*ptr = p;
		*val = 0;
	}

	if (**ptr == '(') {
		(*ptr)++;
		if (parse_reg(ptr, regid) == PARSE_ERR || **ptr != ')') {
			err_print("Invalid MEM");
			return PARSE_ERR;
		}
		(*ptr)++;
		return PARSE_MEM;
	}else{
		err_print("Invalid MEM");
		return PARSE_ERR;
	}
}

/*
 * parse_data: parse an expected data token (e.g., '0x100' or 'array')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *     value: point to the value of digit
 *
 * return
 *     PARSE_DIGIT: success, data token is a digit,
 *                            and move 'ptr' to the first char after token,
 *                            and store the value of digit to 'value'
 *     PARSE_SYMBOL: success, data token is a symbol,
 *                            and move 'ptr' to the first char after token,
 *                            and allocate and store name to 'name' 
 *     PARSE_ERR: error, the value of 'ptr', 'name' and 'value' are undefined
 */
parse_t parse_data(char **ptr, char **name, long *val)
{
    /* skip the blank and check */
	char *p = *ptr;
	SKIP_BLANK(p);
	if (IS_END(p)) {
		return PARSE_ERR;
	}

    /* if IS_DIGIT, then parse the digit */
	if (IS_DIGIT(p)) {
		return parse_digit(ptr, val);
	}
    /* if IS_LETTER, then parse the symbol */
	if (IS_LETTER(p)) {
		return parse_symbol(ptr, name);
	}
    /* set 'ptr', 'name' and 'val' */

    return PARSE_ERR;
}

/*
 * parse_label: parse an expected label token (e.g., 'Loop:')
 * args
 *     ptr: point to the start of string
 *     name: point to the name of symbol (should be allocated in this function)
 *
 * return
 *     PARSE_LABEL: success, move 'ptr' to the first char after token
 *                            and allocate and store name to 'name'
 *     PARSE_ERR: error, the val of 'ptr' is undefined
 */
parse_t parse_label(char **ptr, char **name)
{
    /* skip the blank and check */
	char *pos, *check;
	SKIP_BLANK(*ptr);
	pos = strchr(*ptr, ':');
	if(pos != NULL){
		check = strchr(*ptr, '#');
		if(check < pos && check != NULL)
			return PARSE_ERR;
		/* allocate name and copy to it */
		int len = pos - *ptr;
		*name = (char*)malloc(len + 1);
		    /* set 'ptr' and 'name' */
		strncpy(*name, *ptr, len);
		*ptr = pos + 1;
		(*name)[len] = '\0';
		return PARSE_LABEL;
	}
	return PARSE_ERR;
}

/*
 * parse_line: parse a line of y64 code (e.g., 'Loop: mrmovq (%rcx), %rsi')
 * (you could combine above parse_xxx functions to do it)
 * args
 *     line: point to a line_t data with a line of y64 assembly code
 *
 * return
 *     PARSE_XXX: success, fill line_t with assembled y64 code
 *     PARSE_ERR: error, try to print err information (e.g., instr type and line number)
 */
type_t parse_line(line_t *line)
{

/* when finish parse an instruction or lable, we still need to continue check 
* e.g., 
*  Loop: mrmovl (%rbp), %rcx
*           call SUM  #invoke SUM function */
	char *p = line->y64asm;
	char *name = NULL;
	regid_t regA;
	regid_t regB;
	instr_t *inst = NULL;
	bin_t bin;
	long val;
	int ret;

    /* skip blank and check IS_END */
	SKIP_BLANK(p);
	if (IS_END(p) || IS_COMMENT(p)) {
		return line->type;
	}
	if (IS_LETTER(p) || *p == '.') {
		if (parse_instr(&p, &inst) == PARSE_ERR) {
            //p is a symbol
			if (parse_label(&p, &name) == PARSE_ERR) {
				line->type = TYPE_ERR;
				return line->type;
			}
			if (add_symbol(name) < 0) {
				err_print("Dup symbol:%s", name);
				line->type = TYPE_ERR;
				return line->type;
			}
			SKIP_BLANK(p);
			if (IS_END(p) || IS_COMMENT(p)) {
				line->y64bin.bytes = 0;
				line->y64bin.addr = vmaddr;
				line->type = TYPE_INS;
				return line->type;
			}
            //finish parsing symbol and parse the following instruction
			if (parse_instr(&p, &inst) == PARSE_ERR) {
				line->type = TYPE_ERR;
				return line->type;
			}
		}	
		switch (HIGH(inst->code)) {
			
			case I_RRMOVQ:
			case I_ALU:
				ret = parse_reg(&p, &regA)>=0 && parse_delim(&p, ',')>=0 && parse_reg(&p, &regB)>=0;
				if (!ret) {
					line->type = TYPE_ERR;
					return line->type;
				}
				bin.codes[1] = HPACK(regA, regB);
				break;
			case I_RMMOVQ:
				if (parse_reg(&p, &regA) == PARSE_ERR ||
						parse_delim(&p, ',') == PARSE_ERR ||
						parse_mem(&p, &val, &regB) == PARSE_ERR) {
					line->type = TYPE_ERR;
					return line->type;
				}
				set_n_val(&bin.codes[2], val, 8);
				bin.codes[1] = HPACK(regA, regB);
				break;
			case I_IRMOVQ:
				{
					parse_t par = parse_imm(&p, &name, &val);
					ret = par>=0 && parse_delim(&p, ',')>=0 && parse_reg(&p, &regB) >= 0;
					if (!ret) {
						line->type = TYPE_ERR;
						return line->type;
					}
					bin.codes[1] = HPACK(REG_NONE, regB);
					if (par == PARSE_DIGIT) {
						set_n_val(&bin.codes[2], val, 8);
					} else if (par == PARSE_SYMBOL) {
						add_reloc(name, &line->y64bin);
					}
					break;
				}
			
			case I_MRMOVQ:
				if (parse_mem(&p, &val, &regB) == PARSE_ERR ||
						parse_delim(&p, ',') == PARSE_ERR ||
						parse_reg(&p, &regA)) {
					line->type = TYPE_ERR;
					return line->type;
				}
				bin.codes[1] = HPACK(regA, regB);
				set_n_val(&bin.codes[2], val, 8);
				break;
			case I_CALL:
			case I_JMP:
				if (parse_symbol(&p, &name) == PARSE_ERR) {
					line->type = TYPE_ERR;
					err_print("Invalid DEST");
					return line->type;
				}
				add_reloc(name, &(line->y64bin));
				break;
			case I_PUSHQ:
			case I_POPQ:
				if (parse_reg(&p, &regA) == PARSE_ERR) {
					line->type = TYPE_ERR;
					return line->type;
				}
				bin.codes[1] = HPACK(regA, REG_NONE);
				break;
			case I_HALT:
			case I_RET:
			case I_NOP:
			case I_DIRECTIVE:
			default:
				break;
		}
		if (HIGH(inst->code) != I_DIRECTIVE) {
			//set binary code
			bin.bytes = inst->bytes;
			bin.addr = vmaddr;
			bin.codes[0] = inst->code;
			line->y64bin = bin;
			line->type = TYPE_INS;
			vmaddr += inst->bytes;
		} else {
			switch (LOW(inst->code)) {
				case D_POS:
					if (parse_digit(&p, &val) == PARSE_ERR) {
						line->type = TYPE_ERR;
						return line->type;
					}
					vmaddr = val;
					line->y64bin.bytes = 0;
					line->y64bin.addr = vmaddr;
					line->type = TYPE_INS;
					break;				
				case D_ALIGN:
					if (parse_digit(&p, &val) == PARSE_ERR) {
						line->type = TYPE_ERR;
						return line->type;
					}
                    /* set type and y64bin */
					vmaddr = (vmaddr + val - 1) / val * val;
					line->y64bin.bytes = 0;
					line->y64bin.addr = vmaddr;
					line->type = TYPE_INS;
					break;
				case D_DATA: 
					{
						int bytes = inst->bytes;
						parse_t pt = parse_data(&p, &name, &val);
						if (pt == PARSE_DIGIT) {
							set_n_val(line->y64bin.codes, val, bytes);
						} else if (pt == PARSE_SYMBOL) {
							add_reloc(name, &(line->y64bin));
						} else if (pt == PARSE_ERR) {
							line->type = TYPE_ERR;
							return line->type;
						}
						line->y64bin.bytes = bytes;
						line->y64bin.addr = vmaddr;
						line->type = TYPE_INS;
						vmaddr += bytes;
						break;
					}
				default:
					break;
			}		
		}
	}	
    return line->type;
}

/*
 * assemble: assemble an y64 file (e.g., 'asum.ys')
 * args
 *     in: point to input file (an y64 assembly file)
 *
 * return
 *     0: success, assmble the y64 file to a list of line_t
 *     -1: error, try to print err information (e.g., instr type and line number)
 */
int assemble(FILE *in)
{
    static char asm_buf[MAX_INSLEN]; /* the current line of asm code */
    line_t *line;
    int slen;
    char *y64asm;

    /* read y64 code line-by-line, and parse them to generate raw y64 binary code list */
    while (fgets(asm_buf, MAX_INSLEN, in) != NULL) {
        slen  = strlen(asm_buf);
        while ((asm_buf[slen-1] == '\n') || (asm_buf[slen-1] == '\r')) { 
            asm_buf[--slen] = '\0'; /* replace terminator */
        }

        /* store y64 assembly code */
        y64asm = (char *)malloc(sizeof(char) * (slen + 1)); // free in finit
        strcpy(y64asm, asm_buf);

        line = (line_t *)malloc(sizeof(line_t)); // free in finit
        memset(line, '\0', sizeof(line_t));

        line->type = TYPE_COMM;
        line->y64asm = y64asm;
        line->next = NULL;

        line_tail->next = line;
        line_tail = line;
        lineno ++;

        if (parse_line(line) == TYPE_ERR) {
            return -1;
        }
		if (line->type == TYPE_INS) {
			bin_t bin = line->y64bin;
			if (bin.bytes > 0) {
				int byte = bin.addr + bin.bytes;
				if (byte > max_byte) {
					max_byte = byte;
				}
			}
		}
    }

	lineno = -1;
    return 0;
}

/*
 * relocate: relocate the raw y64 binary code with symbol address
 *
 * return
 *     0: success
 *     -1: error, try to print err information (e.g., addr and symbol)
 */
int relocate(void)
{
    reloc_t *rtmp = NULL;
    
    rtmp = reltab->next;
    while (rtmp) {
        /* find symbol */
		symbol_t *stmp = find_symbol(rtmp->name);
		if (!stmp) {
			err_print("Unknown symbol:\'%s\'", rtmp->name);
			return -1;
		}
        /* relocate y64bin according itype */
		byte_t codefun = rtmp->y64bin->codes[0];
		switch (HIGH(codefun)) {
			case I_IRMOVQ:
				set_n_val(rtmp->y64bin->codes + 2, stmp->addr, 8);
				break;
			case I_JMP:
			case I_CALL:
				set_n_val(rtmp->y64bin->codes + 1, stmp->addr, 8);
				break;
			default:
				set_n_val(rtmp->y64bin->codes, stmp->addr, 8);
				break;
		}
        /* next */
        rtmp = rtmp->next;
    }
    return 0;
}

/*
 * binfile: generate the y64 binary file
 * args
 *     out: point to output file (an y64 binary file)
 *
 * return
 *     0: success
 *     -1: error
 */
int binfile(FILE *out)
{
    /* prepare image with y64 binary code */
	line_t *tmp = line_head->next;
	byte_t *buf = (byte_t *)calloc(max_byte, sizeof(byte_t));
	while (tmp) {
		bin_t bin = tmp->y64bin;
		if (tmp->type == TYPE_INS) {
			memcpy(buf+bin.addr, bin.codes, bin.bytes);
		}
		tmp = tmp->next;
	} 

    /* binary write y64 code to output file (NOTE: see fwrite()) */
	fwrite(buf, sizeof(byte_t), max_byte, out);
	free(buf); 
    return 0;
}


/* whether print the readable output to screen or not ? */
bool_t screen = FALSE; 

static void hexstuff(char *dest, int val, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        char c;
        int h = (val >> 4*i) & 0xF;
        c = h < 10 ? h + '0' : h - 10 + 'a';
        dest[len-i-1] = c;
    }
}

void print_line(line_t *line)
{
    char buf[64];

    /* line format: 0xHHH: cccccccccccc | <line> */
    if (line->type == TYPE_INS) {
        bin_t *y64bin = &line->y64bin;
        int i;
        
        strcpy(buf, "  0x000:                      | ");
        
        hexstuff(buf+4, y64bin->addr, 3);
        if (y64bin->bytes > 0)
            for (i = 0; i < y64bin->bytes; i++)
                hexstuff(buf+9+2*i, y64bin->codes[i]&0xFF, 2);
    } else {
        strcpy(buf, "                              | ");
    }

    printf("%s%s\n", buf, line->y64asm);
}

/* 
 * print_screen: dump readable binary and assembly code to screen
 * (e.g., Figure 4.8 in ICS book)
 */
void print_screen(void)
{
    line_t *tmp = line_head->next;
    while (tmp != NULL) {
        print_line(tmp);
        tmp = tmp->next;
    }
}

/* init and finit */
void init(void)
{
    reltab = (reloc_t *)malloc(sizeof(reloc_t)); // free in finit
    memset(reltab, 0, sizeof(reloc_t));

    symtab = (symbol_t *)malloc(sizeof(symbol_t)); // free in finit
    memset(symtab, 0, sizeof(symbol_t));

    line_head = (line_t *)malloc(sizeof(line_t)); // free in finit
    memset(line_head, 0, sizeof(line_t));
    line_tail = line_head;
    lineno = 0;
}

void finit(void)
{
    reloc_t *rtmp = NULL;
    do {
        rtmp = reltab->next;
        if (reltab->name) 
            free(reltab->name);
        free(reltab);
        reltab = rtmp;
    } while (reltab);
    
    symbol_t *stmp = NULL;
    do {
        stmp = symtab->next;
        if (symtab->name) 
            free(symtab->name);
        free(symtab);
        symtab = stmp;
    } while (symtab);

    line_t *ltmp = NULL;
    do {
        ltmp = line_head->next;
        if (line_head->y64asm) 
            free(line_head->y64asm);
        free(line_head);
        line_head = ltmp;
    } while (line_head);
}

static void usage(char *pname)
{
    printf("Usage: %s [-v] file.ys\n", pname);
    printf("   -v print the readable output to screen\n");
    exit(0);
}

int main(int argc, char *argv[])
{
    int rootlen;
    char infname[512];
    char outfname[512];
    int nextarg = 1;
    FILE *in = NULL, *out = NULL;
    
    if (argc < 2)
        usage(argv[0]);
    
    if (argv[nextarg][0] == '-') {
        char flag = argv[nextarg][1];
        switch (flag) {
          case 'v':
            screen = TRUE;
            nextarg++;
            break;
          default:
            usage(argv[0]);
        }
    }

    /* parse input file name */
    rootlen = strlen(argv[nextarg])-3;
    /* only support the .ys file */
    if (strcmp(argv[nextarg]+rootlen, ".ys"))
        usage(argv[0]);
    
    if (rootlen > 500) {
        err_print("File name too long");
        exit(1);
    }
 

    /* init */
    init();

    
    /* assemble .ys file */
    strncpy(infname, argv[nextarg], rootlen);
    strcpy(infname+rootlen, ".ys");
    in = fopen(infname, "r");
    if (!in) {
        err_print("Can't open input file '%s'", infname);
        exit(1);
    }
    
    if (assemble(in) < 0) {
        err_print("Assemble y64 code error");
        fclose(in);
        exit(1);
    }
    fclose(in);


    /* relocate binary code */
    if (relocate() < 0) {
        err_print("Relocate binary code error");
        exit(1);
    }


    /* generate .bin file */
    strncpy(outfname, argv[nextarg], rootlen);
    strcpy(outfname+rootlen, ".bin");
    out = fopen(outfname, "wb");
    if (!out) {
        err_print("Can't open output file '%s'", outfname);
        exit(1);
    }

    if (binfile(out) < 0) {
        err_print("Generate binary file error");
        fclose(out);
        exit(1);
    }
    fclose(out);
    
    /* print to screen (.yo file) */
    if (screen)
       print_screen(); 

    /* finit */
    finit();
    return 0;
}


